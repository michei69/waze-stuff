/* json-to-wzdf — JSON tile data to WZT binary converter
 *
 * Reads JSON of form:
 *   { "tile_id": { "points": [{"x":N,"y":N},...],
 *                  "lines": { "key": {"points":[i0,i1,...], "type":N}, ... },
 *                  "polygons": { "key": {"points":[i0,i1,...],
 *                                 "type":N, "north":N, "west":N, "south":N, "east":N}, ... } }, ... }
 *
 * For each tile_id, writes tile_id.wzt in the output directory.
 * Line "points" arrays are processed in consecutive pairs — each pair (a,b)
 * becomes one WZT line segment.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "cJSON.h"

// ============================================================================
//  WZT binary types (packed, from waze_types.h)
// ============================================================================

#pragma pack(push, 1)

typedef struct {
    uint16_t first_point_idx;
    uint16_t last_point_idx;
    int16_t  shape_idx;
    uint16_t street_idx;
} LineDataRaw;

typedef struct {
    uint16_t next[21];
    uint16_t roundabout_count;
    uint16_t first_broken[8];
    uint16_t total_broken_count;
} LineSummaryRaw;

typedef struct {
    uint16_t longitude;
    uint16_t latitude;
} PointDataRaw;

typedef struct {
    int32_t data;
} PointIdRaw;

typedef struct {
    uint8_t a_to_b;
    uint8_t b_to_a;
    uint8_t b_to_a_turn;
    uint8_t a_to_b_turn;
} LineRouteRaw;

typedef struct {
    uint16_t type_part;
    uint16_t name_part;
    uint16_t prefix_part;
    uint16_t suffix_part;
    uint16_t text2speech;
} StreetNameRaw;

typedef struct {
    uint16_t city;
    uint16_t first_street;
} StreetCityRaw;

typedef struct {
    int32_t data;
} StreetIdRaw;

typedef struct {
    uint16_t first_polygon_point_idx;
    uint16_t polygon_point_count;
    uint16_t name;
    uint8_t  cfcc;
    uint8_t  _pad;
    uint16_t north;
    uint16_t west;
    uint16_t east;
    uint16_t south;
} PolygonHeadRaw;

typedef struct {
    uint8_t a_to_b_avg_speed;
    uint8_t b_to_a_avg_speed;
} LineSpeedAverageRaw;

typedef struct {
    uint8_t a_to_b_speed_ref;
    uint8_t b_to_a_speed_ref;
} LineSpeedLineRefRaw;

typedef struct {
    uint16_t street;
    uint16_t from_address;
    uint16_t to_address;
} RangeRaw;

typedef struct {
    int32_t  tile_id;
    int32_t  scale;
    uint32_t tile_version;
} SquareDataRaw;

typedef struct {
    uint16_t category;
    uint16_t name;
    uint16_t value;
    uint16_t _padding;
} MetadataAttributeRaw;

typedef struct {
    uint32_t data;
} LineExtIdRaw;

typedef struct {
    uint8_t data;
} LineExtLevelByLineRaw;

typedef struct {
    uint8_t data;
} LineExtLevelRaw;

typedef struct {
    uint8_t a_to_b;
    uint8_t b_to_a;
} LineSpeedMaxRaw;

typedef struct {
    uint8_t data[4];
} LineAttributesRaw;

typedef struct {
    uint32_t data;
} PolygonExRaw;

#pragma pack(pop)

// ============================================================================
//  Parsed tile data from JSON
// ============================================================================

typedef struct {
    uint16_t x, y;
} PtEntry;

typedef struct {
    uint16_t first_pt;
    uint16_t last_pt;
    uint8_t  road_type;
} LineSegment;

typedef struct {
    uint16_t *indices;
    uint32_t  count;
    uint8_t   cfcc;
    uint16_t  north, west, south, east;
} PolyData;

typedef struct {
    PtEntry     *points;
    uint32_t     point_count;
    LineSegment *lines;
    uint32_t     line_count;
    PolyData    *polys;
    uint32_t     poly_count;
    uint32_t     total_poly_pts;
} TileData;

static void td_free(TileData *td) {
    free(td->points);
    free(td->lines);
    for (uint32_t i = 0; i < td->poly_count; i++)
        free(td->polys[i].indices);
    free(td->polys);
}

// ============================================================================
//  Dynamic byte buffer for building sections
// ============================================================================

typedef struct {
    uint8_t *data;
    uint32_t size;
    uint32_t cap;
} ByteBuf;

static void bb_init(ByteBuf *b) {
    b->data = NULL; b->size = 0; b->cap = 0;
}

static void bb_write(ByteBuf *b, const void *src, uint32_t len) {
    if (!len) return;
    if (b->size + len > b->cap) {
        b->cap = b->cap ? b->cap * 2 : 256;
        while (b->size + len > b->cap) b->cap *= 2;
        b->data = realloc(b->data, b->cap);
    }
    memcpy(b->data + b->size, src, len);
    b->size += len;
}

static void bb_write_zeros(ByteBuf *b, uint32_t len) {
    if (!len) return;
    if (b->size + len > b->cap) {
        b->cap = b->cap ? b->cap * 2 : 256;
        while (b->size + len > b->cap) b->cap *= 2;
        b->data = realloc(b->data, b->cap);
    }
    memset(b->data + b->size, 0, len);
    b->size += len;
}

static void bb_free(ByteBuf *b) {
    free(b->data);
    b->data = NULL; b->size = 0; b->cap = 0;
}

// ============================================================================
//  WZT file writer
// ============================================================================

#define N_SECTIONS 46

static bool write_wzt(const char *path, const TileData *td, int32_t tile_id)
{
    ByteBuf sections[N_SECTIONS];
    for (int i = 0; i < N_SECTIONS; i++) bb_init(&sections[i]);

    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%d", tile_id);

    // --- Section 0: STR0 (street type names) ---
    bb_write(&sections[0], "\x00", 1);

    // --- Section 1: STR1 (street base names) ---
    bb_write(&sections[1], "\x00\x00\x00\x00Street\x00", 10);

    // --- Section 2: STR2 (unused) ---
    bb_write(&sections[2], "\x00", 1);

    // --- Section 3: STR3 (street prefix) ---
    bb_write_zeros(&sections[3], 4);

    // --- Section 4: STR4 (street suffix) ---
    bb_write_zeros(&sections[4], 4);

    // --- Section 5: STR5 (city names) ---
    bb_write(&sections[5], "\x00\x00\x00\x00City\x00", 9);

    // --- Section 6: STR6 (metadata) ---
    bb_write(&sections[6],
        "\x00\x00\x00\x00"
        "Version\x00"
        "Date\x00"
        "Fri Jun 27 08:29:00 2025\n\x00"
        "UnixTime\x00"
        "1751012940\x00"
        "Builder\x00"
        "1.18.25\x00",
        79);

    // --- Section 7: STR7 (venue/label names) ---
    bb_write(&sections[7], "\x00", 1);
    bb_write(&sections[7], id_str, (uint32_t)strlen(id_str));

    // --- Section 8: SHAPES (empty — no shapes) ---
    // (empty)

    // --- Section 9: LINE_DATA ---
    if (td->line_count > 0) {
        for (uint32_t i = 0; i < td->line_count; i++) {
            LineDataRaw raw;
            raw.first_point_idx = td->lines[i].first_pt;
            raw.last_point_idx  = td->lines[i].last_pt;
            raw.shape_idx       = -1;
            raw.street_idx      = 0;
            bb_write(&sections[9], &raw, sizeof(raw));
        }
    }

    // --- Section 10: LINE_SUMMARY (62 bytes zeros) ---
    bb_write_zeros(&sections[10], sizeof(LineSummaryRaw));

    // --- Section 11: BROKEN (empty) ---

    // --- Section 12: LINE_ROUNDABOUT (empty) ---

    // --- Section 13: POINT_DATA ---
    if (td->point_count > 0) {
        for (uint32_t i = 0; i < td->point_count; i++) {
            PointDataRaw raw;
            raw.longitude = td->points[i].x;
            raw.latitude  = td->points[i].y;
            bb_write(&sections[13], &raw, sizeof(raw));
        }
    }

    // --- Section 14: POINT_IDS (0xFFFFFFFE for each point) ---
    {
        int32_t pid = -2;
        for (uint32_t i = 0; i < td->point_count; i++)
            bb_write(&sections[14], &pid, sizeof(pid));
    }

    // --- Section 15: LINE_ROUTES (a_to_b=1, b_to_a=1 per line) ---
    if (td->line_count > 0) {
        LineRouteRaw route;
        route.a_to_b      = 0x01;
        route.b_to_a      = 0x01;
        route.b_to_a_turn = 0x01;
        route.a_to_b_turn = 0x01;
        for (uint32_t i = 0; i < td->line_count; i++)
            bb_write(&sections[15], &route, sizeof(route));
    }

    // --- Section 16: STREET_NAMES (1 unnamed street) ---
    {
        StreetNameRaw sn;
        memset(&sn, 0, sizeof(sn));
        bb_write(&sections[16], &sn, sizeof(sn));
    }

    // --- Section 17: STREET_CITIES (1 entry) ---
    {
        StreetCityRaw sc;
        memset(&sc, 0, sizeof(sc));
        bb_write(&sections[17], &sc, sizeof(sc));
    }

    // --- Section 18: POLYGON_HEADS ---
    {
        uint16_t pp_idx_offset = 0;
        for (uint32_t i = 0; i < td->poly_count; i++) {
            PolygonHeadRaw ph;
            ph.first_polygon_point_idx = pp_idx_offset;
            ph.polygon_point_count     = td->polys[i].count;
            ph.name                    = 1;
            ph.cfcc                    = td->polys[i].cfcc;
            ph._pad                    = 0;
            ph.north                   = td->polys[i].north;
            ph.west                    = td->polys[i].west;
            ph.east                    = td->polys[i].east;
            ph.south                   = td->polys[i].south;
            bb_write(&sections[18], &ph, sizeof(ph));
            pp_idx_offset += td->polys[i].count;
        }
    }

    // --- Section 19: POLYGON_POINTS ---
    for (uint32_t i = 0; i < td->poly_count; i++) {
        for (uint32_t j = 0; j < td->polys[i].count; j++) {
            uint16_t idx = td->polys[i].indices[j];
            bb_write(&sections[19], &idx, sizeof(idx));
        }
    }

    // --- Section 20: LINE_SPEED_REF (zeros) ---
    if (td->line_count > 0) {
        LineSpeedLineRefRaw ref;
        memset(&ref, 0, sizeof(ref));
        for (uint32_t i = 0; i < td->line_count; i++)
            bb_write(&sections[20], &ref, sizeof(ref));
    }

    // --- Section 21: LINE_SPEED_AVG (0xFF = no data) ---
    if (td->line_count > 0) {
        LineSpeedAverageRaw avg;
        avg.a_to_b_avg_speed = 0xFF;
        avg.b_to_a_avg_speed = 0xFF;
        for (uint32_t i = 0; i < td->line_count; i++)
            bb_write(&sections[21], &avg, sizeof(avg));
    }

    // --- Section 22: LINE_SPEED_IDX (empty) ---

    // --- Section 23: LINE_SPEED_IDX_SM (empty) ---

    // --- Section 24: RANGES (6 bytes zeros) ---
    {
        RangeRaw rng;
        memset(&rng, 0, sizeof(rng));
        bb_write(&sections[24], &rng, sizeof(rng));
    }

    // --- Section 25: ALERT_DATA (empty) ---

    // --- Section 26: SQUARE_DATA ---
    {
        SquareDataRaw sq;
        sq.tile_id      = tile_id;
        sq.scale        = 0;
        sq.tile_version = 0;
        bb_write(&sections[26], &sq, sizeof(sq));
    }

    // --- Section 27: METADATA_ATTRIBUTE (3 entries, 24 bytes) ---
    {
        MetadataAttributeRaw mattr[3];
        memset(mattr, 0, sizeof(mattr));
        mattr[0].category = 1;  mattr[0].name = 9;  mattr[0].value = 14;
        mattr[1].category = 1;  mattr[1].name = 40; mattr[1].value = 49;
        mattr[2].category = 1;  mattr[2].name = 60; mattr[2].value = 68;
        bb_write(&sections[27], mattr, sizeof(mattr));
    }

    // --- Section 28-29: VENUE (empty) ---

    // --- Section 30: LINE_EXT_TYPES (uint8_t[line_count] + 2 pad) ---
    if (td->line_count > 0) {
        for (uint32_t i = 0; i < td->line_count; i++) {
            uint8_t ext = td->lines[i].road_type;
            bb_write(&sections[30], &ext, 1);
        }
        uint8_t pad[2] = {0, 0};
        bb_write(&sections[30], pad, 2);
    } else {
        uint8_t pad[2] = {0, 0};
        bb_write(&sections[30], pad, 2);
    }

    // --- Section 31: LINE_EXT_IDS (zeros) ---
    if (td->line_count > 0) {
        LineExtIdRaw eid;
        memset(&eid, 0, sizeof(eid));
        for (uint32_t i = 0; i < td->line_count; i++)
            bb_write(&sections[31], &eid, sizeof(eid));
    }

    // --- Section 32: LINE_EXT_LEVEL_BY_LINE (zeros) ---
    if (td->line_count > 0) {
        LineExtLevelByLineRaw lel;
        lel.data = 0;
        for (uint32_t i = 0; i < td->line_count; i++)
            bb_write(&sections[32], &lel, sizeof(lel));
    }

    // --- Section 33: LINE_EXT_LEVELS (zeros) ---
    if (td->line_count > 0) {
        LineExtLevelRaw le;
        le.data = 0;
        for (uint32_t i = 0; i < td->line_count; i++)
            bb_write(&sections[33], &le, sizeof(le));
    }

    // --- Section 34: LINE_SPEED_MAX (3 prefix + data + 1 suffix) ---
    if (td->line_count > 0) {
        uint8_t prefix[3] = {0, 0, 0};
        bb_write(&sections[34], prefix, 3);
        LineSpeedMaxRaw spd;
        memset(&spd, 0, sizeof(spd));
        for (uint32_t i = 0; i < td->line_count; i++)
            bb_write(&sections[34], &spd, sizeof(spd));
        uint8_t suffix = 0;
        bb_write(&sections[34], &suffix, 1);
    } else {
        uint8_t prefix[3] = {0, 0, 0};
        bb_write(&sections[34], prefix, 3);
        uint8_t suffix = 0;
        bb_write(&sections[34], &suffix, 1);
    }

    // --- Section 35: POLYGON_EX ---
    if (td->poly_count > 0) {
        PolygonExRaw pex;
        memset(&pex, 0, sizeof(pex));
        for (uint32_t i = 0; i < td->poly_count; i++)
            bb_write(&sections[35], &pex, sizeof(pex));
    }

    // --- Section 36: LINE_ATTRIBUTES ---
    if (td->line_count > 0) {
        LineAttributesRaw attr;
        memset(&attr, 0, sizeof(attr));
        for (uint32_t i = 0; i < td->line_count; i++)
            bb_write(&sections[36], &attr, sizeof(attr));
    }

    // --- Section 37: STREET_IDS (1 entry) ---
    {
        StreetIdRaw sid;
        sid.data = 0;
        bb_write(&sections[37], &sid, sizeof(sid));
    }

    // --- Sections 38-42: BEACONS (empty) ---

    // --- Section 43: LANE_TYPES (LANE_REGULAR=0 per line) ---
    if (td->line_count > 0) {
        uint8_t lane = 0;
        for (uint32_t i = 0; i < td->line_count; i++)
            bb_write(&sections[43], &lane, 1);
    }

    // --- Section 44: LINE_NEW_TYPES (zeros) ---
    if (td->line_count > 0) {
        uint8_t nt = 0;
        for (uint32_t i = 0; i < td->line_count; i++)
            bb_write(&sections[44], &nt, 1);
    }

    // --- Section 45: EXT_PROTOBUF (empty) ---

    // ---- Compute cumulative offsets and total data size ----
    uint32_t offsets[N_SECTIONS];
    uint32_t cum = 0;
    for (int i = 0; i < N_SECTIONS; i++) {
        cum += sections[i].size;
        offsets[i] = cum;
    }

    // ---- Write file ----
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open %s for writing\n", path);
        goto cleanup;
    }

    uint32_t entry_count = N_SECTIONS;
    uint32_t mask_bits   = 0;
    fwrite(&entry_count, 4, 1, f);
    fwrite(&mask_bits, 4, 1, f);
    fwrite(offsets, 4, N_SECTIONS, f);

    for (int i = 0; i < N_SECTIONS; i++) {
        if (sections[i].size > 0)
            fwrite(sections[i].data, 1, sections[i].size, f);
    }

    fclose(f);

cleanup:
    for (int i = 0; i < N_SECTIONS; i++) bb_free(&sections[i]);
    return f != NULL;
}

// ============================================================================
//  JSON parsing
// ============================================================================

static bool parse_tile(cJSON *jt, TileData *td)
{
    memset(td, 0, sizeof(*td));

    // ---- Parse points ----
    cJSON *jpts = cJSON_GetObjectItem(jt, "points");
    if (!jpts || !cJSON_IsArray(jpts)) {
        fprintf(stderr, "Tile missing 'points' array\n");
        return false;
    }
    int np = cJSON_GetArraySize(jpts);
    if (np == 0) {
        fprintf(stderr, "Tile has empty 'points' array\n");
        return false;
    }
    td->points = calloc((size_t)np, sizeof(PtEntry));
    td->point_count = (uint32_t)np;
    {
        cJSON *pnode = jpts->child;
        for (int i = 0; i < np; i++, pnode = pnode->next) {
            cJSON *jx = cJSON_GetObjectItem(pnode, "x");
            cJSON *jy = cJSON_GetObjectItem(pnode, "y");
            td->points[i].x = (uint16_t)(jx ? jx->valueint : 0);
            td->points[i].y = (uint16_t)(jy ? jy->valueint : 0);
        }
    }

    // ---- Parse lines ----
    cJSON *jlines = cJSON_GetObjectItem(jt, "lines");
    if (jlines && cJSON_IsObject(jlines)) {
        // Count total segments
        int total_segs = 0;
        for (cJSON *lk = jlines->child; lk; lk = lk->next) {
            cJSON *jlps = cJSON_GetObjectItem(lk, "points");
            if (!jlps || !cJSON_IsArray(jlps)) continue;
            int nidx = cJSON_GetArraySize(jlps);
            total_segs += nidx / 2;
        }
        td->lines = calloc((size_t)total_segs, sizeof(LineSegment));
        td->line_count = 0;

        for (cJSON *lk = jlines->child; lk; lk = lk->next) {
            cJSON *jlps  = cJSON_GetObjectItem(lk, "points");
            cJSON *jtype = cJSON_GetObjectItem(lk, "type");
            if (!jlps || !cJSON_IsArray(jlps)) continue;
            int nidx = cJSON_GetArraySize(jlps);
            uint8_t rtype = (uint8_t)(jtype ? jtype->valueint : 0);

            cJSON *pnode = jlps->child;
            for (int s = 0; s < nidx / 2; s++) {
                int i0 = pnode->valueint; pnode = pnode->next;
                int i1 = pnode->valueint; pnode = pnode->next;
                if (i0 == i1) continue;
                LineSegment *ls = &td->lines[td->line_count++];
                ls->first_pt  = (uint16_t)i0;
                ls->last_pt   = (uint16_t)i1;
                ls->road_type = rtype;
            }
        }
    }

    // ---- Parse polygons ----
    cJSON *jpolys = cJSON_GetObjectItem(jt, "polygons");
    if (jpolys && cJSON_IsObject(jpolys)) {
        int npoly = cJSON_GetArraySize(jpolys);
        td->polys = calloc((size_t)npoly, sizeof(PolyData));
        td->poly_count = 0;

        for (cJSON *pk = jpolys->child; pk; pk = pk->next) {
            cJSON *jpp  = cJSON_GetObjectItem(pk, "points");
            cJSON *jtype = cJSON_GetObjectItem(pk, "type");
            cJSON *jn   = cJSON_GetObjectItem(pk, "north");
            cJSON *jw   = cJSON_GetObjectItem(pk, "west");
            cJSON *js   = cJSON_GetObjectItem(pk, "south");
            cJSON *je   = cJSON_GetObjectItem(pk, "east");
            if (!jpp || !cJSON_IsArray(jpp)) continue;
            int nidx = cJSON_GetArraySize(jpp);
            if (nidx < 2) continue;

            PolyData *pd = &td->polys[td->poly_count++];
            pd->count  = (uint16_t)nidx;
            pd->cfcc   = (uint8_t)(jtype ? jtype->valueint : 0);
            pd->north  = (uint16_t)(jn ? jn->valueint : 0);
            pd->west   = (uint16_t)(jw ? jw->valueint : 0);
            pd->south  = (uint16_t)(js ? js->valueint : 0);
            pd->east   = (uint16_t)(je ? je->valueint : 0);
            pd->indices = calloc((size_t)nidx, sizeof(uint16_t));

            cJSON *pnode = jpp->child;
            for (int i = 0; i < nidx; i++, pnode = pnode->next)
                pd->indices[i] = (uint16_t)pnode->valueint;

            td->total_poly_pts += (uint32_t)nidx;
        }
    }

    return true;
}

// ============================================================================
//  Main
// ============================================================================

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.json> <output_dir>\n", argv[0]);
        return 1;
    }

    const char *in_path  = argv[1];
    const char *out_dir  = argv[2];

    // Create output directory
    mkdir(out_dir, 0755);

    // Read input file
    FILE *f = fopen(in_path, "rb");
    if (!f) { perror("fopen input"); return 1; }
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)fsz + 1);
    if (!buf) { fprintf(stderr, "OOM\n"); fclose(f); return 1; }
    if (fread(buf, 1, (size_t)fsz, f) != (size_t)fsz) {
        fprintf(stderr, "Short read\n"); free(buf); fclose(f); return 1;
    }
    buf[fsz] = '\0';
    fclose(f);

    // Parse JSON
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        fprintf(stderr, "JSON parse error: %s\n",
                cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "unknown");
        return 1;
    }

    if (!cJSON_IsObject(root)) {
        fprintf(stderr, "Root JSON element must be an object\n");
        cJSON_Delete(root);
        return 1;
    }

    // Count tiles
    int total = 0;
    for (cJSON *tk = root->child; tk; tk = tk->next) {
        if (tk->string) total++;
    }

    int count = 0, progress = 0;
    for (cJSON *tk = root->child; tk; tk = tk->next) {
        const char *tile_id_str = tk->string;
        if (!tile_id_str) continue;

        if ((++progress & 63) == 0)
            fprintf(stderr, "\r  %d / %d", count, total);

        int32_t tile_id = (int32_t)atol(tile_id_str);

        TileData td;
        if (!parse_tile(tk, &td)) {
            fprintf(stderr, "\nFailed to parse tile %s\n", tile_id_str);
            continue;
        }

        char out_path[1024];
        snprintf(out_path, sizeof(out_path), "%s/%s.wzt", out_dir, tile_id_str);

        if (write_wzt(out_path, &td, tile_id))
            count++;

        td_free(&td);
    }

    cJSON_Delete(root);
    fprintf(stderr, "\r  %d / %d\nWrote %d tiles to %s/\n", count, total, count, out_dir);
    return 0;
}
