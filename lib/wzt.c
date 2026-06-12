#include "wzt.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
//                              Internal helpers
// ============================================================================

/* Calculate aligned offset. add = (1 << mask_bits) - 1, mask = ~add */
static inline uint32_t align_offset(uint32_t raw, uint32_t add, uint32_t mask)
{
    return (raw + add) & mask;
}

/* Count null-terminated strings in a byte buffer */
uint32_t wzt_count_strings(const uint8_t *data, uint32_t size)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < size; i++) {
        if (data[i] == 0) n++;
    }
    return n;
}

/* Assemble street name from its parts */
static char *assemble_street_name(
    const char *type_part,
    const char *base_name,
    const char *prefix_part,
    const char *suffix_part
) {
    size_t len = 0;
    if (type_part && *type_part) len += strlen(type_part) + 1;
    if (base_name) len += strlen(base_name);
    if (prefix_part && *prefix_part) len += strlen(prefix_part) + 1;
    if (suffix_part && *suffix_part) len += strlen(suffix_part) + 1;
    len++; /* null terminator */

    char *name = (char *)malloc(len);
    if (!name) return strdup("");

    name[0] = '\0';
    if (type_part && *type_part) {
        strcat(name, type_part);
        strcat(name, " ");
    }
    if (base_name) strcat(name, base_name);
    if (prefix_part && *prefix_part) {
        strcat(name, " ");
        strcat(name, prefix_part);
    }
    if (suffix_part && *suffix_part) {
        strcat(name, " ");
        strcat(name, suffix_part);
    }
    return name;
}

// Look up a null-terminated string from raw byte array at given byte offset.
// Returns pointer into the raw data (no allocation).
static const char *lookup_string(const uint8_t *raw, uint32_t raw_size, uint32_t offset)
{
    if (!raw || offset >= raw_size) return "";
    return (const char *)(raw + offset);
}

// ============================================================================
//                         Tile coordinate utilities
// ============================================================================

#define WZT_SCALE_STEP      4
#define WZT_SQUARE_SIZE     10000
#define WZT_COORD_RANGE_LAT 179999999
#define WZT_COORD_RANGE_LON 359999999

// Convert tile_id to lat/lon in degrees.
// Single-pass: finds the scale where tile_id lives, then computes coords.
// Matches waze_tile_classes.py SquareData / waze_scales.py ScaleData.
void wzt_tile_id_to_lat_lon(int32_t tile_id, double *lat, double *lon)
{
    int32_t sf = 1, base = 0, num_rows = 0;

    for (int s = 0; s < 10; s++) {
        num_rows = WZT_COORD_RANGE_LAT / (sf * WZT_SQUARE_SIZE) + 1;
        int32_t num_cols = WZT_COORD_RANGE_LON / (sf * WZT_SQUARE_SIZE) + 1;
        int32_t next_base = base + num_rows * num_cols;
        if (tile_id < next_base) break;
        base = next_base;
        sf *= WZT_SCALE_STEP;
    }

    int32_t temp      = tile_id - base;
    int32_t lat_index = temp % num_rows;
    int32_t lon_index = temp / num_rows;

    if (lat) *lat = (lat_index * sf - 9000) / 100.0;
    if (lon) *lon = (lon_index * sf - 18000) / 100.0;
}

// Convert lat/lon in degrees to tile_id at the given scale (0 = finest).
// Matches waze_util.py latlon_to_id.
int32_t wzt_lat_lon_to_tile_id(double lat, double lon, int32_t scale)
{
    /* Compute scale data up to the requested scale */
    int32_t sf = 1, base = 0, num_rows = 0;
    for (int s = 0; s < scale; s++) {
        num_rows = WZT_COORD_RANGE_LAT / (sf * WZT_SQUARE_SIZE) + 1;
        int32_t num_cols = WZT_COORD_RANGE_LON / (sf * WZT_SQUARE_SIZE) + 1;
        base += num_rows * num_cols;
        sf *= WZT_SCALE_STEP;
    }
    num_rows = WZT_COORD_RANGE_LAT / (sf * WZT_SQUARE_SIZE) + 1;

    int32_t lat_index = (int32_t)(lat * 100.0 + 9000.0) / sf;
    int32_t lon_index = (int32_t)(lon * 100.0 + 18000.0) / sf;

    return base + lon_index * num_rows + lat_index;
}

// ============================================================================
//                                 Public API
// ============================================================================

WazeTile *wzt_parse(const uint8_t *data, size_t data_size)
{
    if (!data || data_size < 12) {
        fprintf(stderr, "wzt: data too small (%zu bytes)\n", data_size);
        return NULL;
    }

    WazeTile *tile = (WazeTile *)calloc(1, sizeof(WazeTile));
    if (!tile) return NULL;

    tile->raw_data = (uint8_t *)data;  /* non-owning pointer */
    tile->raw_size = (uint32_t)data_size;

    /* Header */
    tile->entry_count = *(const uint32_t *)(data);
    tile->mask_bits   = *(const uint32_t *)(data + 4);

    if (tile->entry_count > WZT_SECTION_COUNT) {
        fprintf(stderr, "wzt: entry_count %u > max %u\n",
                tile->entry_count, WZT_SECTION_COUNT);
        free(tile);
        return NULL;
    }

    /* Read offsets */
    const uint32_t *off_ptr = (const uint32_t *)(data + 8);
    for (uint32_t i = 0; i < tile->entry_count; i++) {
        tile->offsets[i] = off_ptr[i];
    }

    uint32_t header_size = 8 + tile->entry_count * 4;
    uint32_t add = (1u << tile->mask_bits) - 1;
    uint32_t mask = ~add;
    const uint8_t *data_block = data + header_size;

    /* ---- Precompute all section pointers ---- */
    for (uint32_t s = 0; s < tile->entry_count; s++) {
        uint32_t raw_start = (s == 0) ? 0 : tile->offsets[s - 1];
        uint32_t raw_end   = tile->offsets[s];
        /* Section 34: LineSpeedMax - +3/+1 offset adjustment */
        if (s == 34) { raw_start += 3; raw_end += 1; }
        tile->sec_ptr[s]  = data_block + align_offset(raw_start, add, mask);
        tile->sec_size[s] = raw_end - raw_start;
    }

    // Access a section precomputed above. _pN = typed pointer, _sN = byte size, _cN = element count.
    #define WZT_SEC(id, type) \
        const uint32_t _s##id = tile->sec_size[(id)]; \
        const uint32_t _c##id = _s##id / sizeof(type); \
        const type *_p##id = (const type *)(tile->sec_ptr[(id)])

    /* Count-only: set tile->field to element count of section */
    #define WZT_CNT(id, type, field) \
        tile->field = tile->sec_size[(id)] / sizeof(type)

    /* ---- Shapes (section 8) ---- */
    WZT_SEC(8, ShapeRaw);
    tile->shape_count = _c8;
    tile->shapes = (WazeShape *)calloc(_c8, sizeof(WazeShape));
    if (tile->shapes)
        for (uint32_t i = 0; i < _c8; i++) {
            tile->shapes[i].dx = _p8[i].delta_longitude;
            tile->shapes[i].dy = _p8[i].delta_latitude;
        }

    /* ---- Point data (section 13) ---- */
    WZT_SEC(13, PointDataRaw);
    tile->point_count = _c13;
    tile->points = (WazePoint *)calloc(_c13, sizeof(WazePoint));
    if (tile->points)
        for (uint32_t i = 0; i < _c13; i++) {
            tile->points[i].x = _p13[i].longitude;
            tile->points[i].y = _p13[i].latitude;
        }

    /* ---- Streets (section 16) ---- */
    WZT_SEC(16, StreetNameRaw);
    tile->street_count = _c16;
    tile->streets = (WazeStreet *)calloc(_c16, sizeof(WazeStreet));
    if (tile->streets)
        for (uint32_t i = 0; i < _c16; i++) {
            WazeStreet *st = &tile->streets[i];
            st->type_part   = lookup_string(tile->sec_ptr[0], tile->sec_size[0], _p16[i].type_part);
            st->name_part   = lookup_string(tile->sec_ptr[1], tile->sec_size[1], _p16[i].name_part);
            st->prefix_part = lookup_string(tile->sec_ptr[3], tile->sec_size[3], _p16[i].prefix_part);
            st->suffix_part = lookup_string(tile->sec_ptr[4], tile->sec_size[4], _p16[i].suffix_part);
            st->full_name   = assemble_street_name(st->type_part, st->name_part, st->prefix_part, st->suffix_part);
        }

    /* ---- Lines (section 9) ---- */
    WZT_SEC(9, LineDataRaw);
    tile->line_count = _c9;
    tile->lines = (WazeLine *)calloc(_c9, sizeof(WazeLine));
    if (tile->lines) {
        /* Parallel arrays from other sections */
        WZT_SEC(15, LineRouteRaw);        tile->line_route_count = _c15;
        WZT_SEC(21, LineSpeedAverageRaw); tile->line_speed_avg_count = _c21;
        WZT_SEC(24, RangeRaw);            tile->range_count = _c24;
        WZT_SEC(31, LineExtIdRaw);        tile->line_ext_id_count = _c31;
        WZT_SEC(36, LineAttributesRaw);   tile->line_attributes_count = _c36;

        /* Special cases */
        WZT_SEC(30, uint8_t);           /* LineExtType - section has +2 padding */
        WZT_SEC(32, uint8_t);           tile->line_ext_level_by_line_count = _c32;
        WZT_SEC(34, LineSpeedMaxRaw);   /* precomputed with +3/+1 adjustment */
        WZT_SEC(43, uint8_t);           /* LaneType */

        WZT_SEC(11, uint16_t); tile->broken_count = _c11;
        WZT_SEC(12, uint16_t); tile->roundabout_count = _c12;

        for (uint32_t i = 0; i < _c9; i++) {
            WazeLine *line = &tile->lines[i];
            line->first_point_idx  = _p9[i].first_point_idx & POINT_INDEX_MASK;
            line->first_point_fake = _p9[i].first_point_idx & POINT_FAKE_FLAG;
            line->last_point_idx   = _p9[i].last_point_idx & POINT_INDEX_MASK;
            line->last_point_fake = _p9[i].last_point_idx & POINT_FAKE_FLAG;
            line->shape_idx        = _p9[i].shape_idx;
            line->is_broken        = false;
            line->is_roundabout    = false;
            for (uint32_t j = 0; j < _c11; j++) {
                if (_p11[j] == i) {
                    line->is_broken = true;
                    break;
                }
            }
            for (uint32_t j = 0; j < _c12; j++) {
                if (_p12[j] == i) {
                    line->is_roundabout = true;
                    break;
                }
            }

            /* Street index resolution */
            uint16_t raw_street = _p9[i].street_idx;
            if (raw_street == 0xFFFF) {
                line->street_idx = 0xFFFF;
            } else if ((int16_t)raw_street < 0) {
                line->street_idx = raw_street & 0x7FFF;
            } else {
                line->street_idx = (raw_street < _c24)
                    ? _p24[raw_street].street & 0x3FFF : raw_street;
            }

            if (line->street_idx < _c16)
                line->street = &tile->streets[line->street_idx];

            /* Route */
            if (i < _c15) {
                line->a_to_b = _p15[i].a_to_b & ROUTE_CAR_ALLOWED;
                line->b_to_a = _p15[i].b_to_a & ROUTE_CAR_ALLOWED;
            }
            /* Speed average / max */
            if (i < _c21) {
                line->a_to_b_avg_speed = _p21[i].a_to_b_avg_speed;
                line->b_to_a_avg_speed = _p21[i].b_to_a_avg_speed;
            }
            if (i < _c34) {
                line->a_to_b_max_speed = _p34[i].a_to_b;
                line->b_to_a_max_speed = _p34[i].b_to_a;
            }
            if (i < _s32) line->level       = _p32[i];
            if (i < _c36) line->attributes  = *(const uint32_t *)_p36[i].data;
            if (i < _c31) line->ext_id      = _p31[i].data;
            if (i < _c30)  line->road_type  = _p30[i];
            if (i < _c43)  line->lane_type  = _p43[i];

            /* Shape chain */
            line->shape_count = 0;
            if (line->shape_idx >= 0 && (uint32_t)line->shape_idx < _c8) {
                WazeShape *m = &tile->shapes[line->shape_idx];
                if (m->dx == 0 && m->dy > 0 &&
                    (uint32_t)(line->shape_idx + m->dy) < _c8) {
                    line->shapes = &tile->shapes[line->shape_idx + 1];
                    line->shape_count = m->dy;
                }
            }
        }
    }

    /* ---- Line summary (section 10) ---- */
    WZT_SEC(10, LineSummaryRaw);
    tile->line_summary_count = _c10;
    tile->line_summary = (WazeLineSummary *)calloc(_c10, sizeof(WazeLineSummary));
    if (tile->line_summary && _c10 > 0) {
        for (uint32_t i = 0; i < _c10; i++) {
            WazeLineSummary *ls = &tile->line_summary[i];
            memcpy(ls->next_lines,     _p10[i].next,          sizeof(ls->next_lines));
            ls->roundabout_count  = _p10[i].roundabout_count;
            memcpy(ls->first_brokens,  _p10[i].first_broken,  sizeof(ls->first_brokens));
            ls->total_broken_count = _p10[i].total_broken_count;
        }
    }

    /* ---- Polygons (sections 18, 19, 35) ---- */
    WZT_SEC(18, PolygonHeadRaw);
    tile->polygon_count = _c18;
    {
        WZT_SEC(19, uint16_t); tile->polygon_point_count = _c19; tile->polygon_points = _p19;
        WZT_SEC(35, PolygonExRaw);

        tile->polygons = (WazePolygon *)calloc(_c18, sizeof(WazePolygon));
        if (tile->polygons)
            for (uint32_t i = 0; i < _c18; i++) {
                WazePolygon *poly = &tile->polygons[i];
                poly->first_point_idx = _p18[i].first_polygon_point_idx;
                poly->point_count     = _p18[i].polygon_point_count;
                poly->cfcc            = _p18[i].cfcc;
                poly->north           = _p18[i].north;
                poly->west            = _p18[i].west;
                poly->east            = _p18[i].east;
                poly->south           = _p18[i].south;
                poly->name = lookup_string(tile->sec_ptr[7], tile->sec_size[7], _p18[i].name);
                if (i < _c35) poly->ex_data = _p35[i].data;
            }
    }

    /* ---- Square data (section 26) ---- */
    WZT_SEC(26, SquareDataRaw);
    if (_c26 > 0) {
        tile->square.tile_id   = _p26[0].tile_id;
        tile->square.scale     = _p26[0].scale;
        tile->square.timestamp = _p26[0].tile_version;

        // Compute lat/lon from tile_id
        wzt_tile_id_to_lat_lon(tile->square.tile_id,
                                &tile->square.latitude,
                                &tile->square.longitude);
    }

    /* ---- Remaining section counts ---- */
    WZT_CNT(WZT_SEC_BROKEN,          BrokenRaw,          broken_count);
    WZT_CNT(WZT_SEC_STREET_CITIES,   StreetCityRaw,      street_city_count);
    WZT_CNT(WZT_SEC_LINE_SPEED_REF,  LineSpeedLineRefRaw,line_speed_ref_count);
    WZT_CNT(WZT_SEC_LINE_SPEED_IDX,  LineSpeedIndexRaw,  line_speed_idx_count);
    WZT_CNT(WZT_SEC_LINE_SPEED_IDX_SM,LineSpeedIndexSmallRaw,line_speed_idx_sm_count);
    WZT_CNT(WZT_SEC_ALERT_DATA,      AlertDataRaw,       alert_count);
    WZT_CNT(WZT_SEC_METADATA_ATTRIB, MetadataAttributeRaw,metadata_attr_count);
    WZT_CNT(WZT_SEC_VENUE_HEADS,     VenueHeadRaw,       venue_head_count);
    WZT_CNT(WZT_SEC_VENUE_IDS,       VenueIdRaw,         venue_id_count);
    WZT_CNT(WZT_SEC_LINE_EXT_LEVELS, LineExtLevelRaw,    line_ext_level_count);
    WZT_CNT(WZT_SEC_BEACON_POS,      BeaconPosRaw,       beacon_pos_count);
    WZT_CNT(WZT_SEC_BEACON_IDS,      BeaconIdRaw,        beacon_id_count);
    WZT_CNT(WZT_SEC_BEACON_EX_POS,   BeaconExPosRaw,     beacon_ex_pos_count);
    WZT_CNT(WZT_SEC_BEACON_EX_IDS,   BeaconExIdRaw,      beacon_ex_id_count);
    WZT_CNT(WZT_SEC_BEACON_EX_MASKS, BeaconExMaskRaw,    beacon_ex_mask_count);
    tile->ext_protobuf_size = tile->offsets[45] - tile->offsets[44];

    return tile;
}

void wzt_free(WazeTile *tile)
{
    if (!tile) return;

    /* Free street full_name (assembled) */
    if (tile->streets) {
        for (uint32_t i = 0; i < tile->street_count; i++)
            free(tile->streets[i].full_name);
        free(tile->streets);
    }

    if (tile->polygons) free(tile->polygons);

    free(tile->points);
    free(tile->shapes);
    free(tile->lines);
    free(tile->line_summary);
    free(tile);
}