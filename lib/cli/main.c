/* main.c — Waze tile file parser
 *
 * Handles argument parsing and dispatch ONLY.
 * All format logic lives in wzdf.c, wzm.c, wzt.c.
 *
 * Usage:
 *   waze-parser <file.wzt>                 Parse and summarize a WZT tile
 *   waze-parser <file.wzdf>                Decompress WZDF → summarize WZT
 *   waze-parser <file.wzm>                 Split WZM → decompress → summarize each tile
 *   waze-parser -o <out> <file.wzdf>       Decompress and write raw WZT to file
 *   waze-parser -o <dir>  <file.wzm>       Extract all tiles to directory
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "waze_types.h"
#include "wzdf.h"
#include "wzm.h"
#include "wzt.h"

typedef enum {
    ACTION_AUTO,       /* detect type, print summary */
    ACTION_DECOMPRESS, /* write decompressed WZT to output */
    ACTION_SPLIT       /* split WZM, decompress each tile */
} Action;

/* Read entire file into memory. Caller must free *out. */
static int read_file(const char *path, uint8_t **out, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz < 0) {
        fprintf(stderr, "ftell failed\n");
        fclose(f);
        return -1;
    }

    *out = (uint8_t *)malloc((size_t)sz);
    if (!*out) {
        fprintf(stderr, "malloc(%ld) failed\n", sz);
        fclose(f);
        return -1;
    }

    size_t rd = fread(*out, 1, (size_t)sz, f);
    fclose(f);

    if (rd != (size_t)sz) {
        fprintf(stderr, "short read (%zu/%ld)\n", rd, sz);
        free(*out);
        return -1;
    }

    *out_size = (size_t)sz;
    return 0;
}

/* Detect file type from magic bytes. Returns:
 *   0 = WZT (decompressed tile data)
 *   1 = WZDF
 *   2 = WZM
 *  -1 = unknown
 */
static int detect_type(const uint8_t *data, size_t size)
{
    if (size < 4) return -1;

    /* Check WZDF/WZM magic */
    uint32_t m;
    memcpy(&m, data, 4);
    if (m == WZDF_MAGIC) return 1;
    if (m == WZM_MAGIC) return 2;

    /* If not WZDF or WZM, check if it looks like WZT:
     * First 4 bytes: entry_count (reasonable: 1-100)
     * Next 4 bytes: mask_bits (reasonable: 0-4)
     */
    uint32_t entry_count, mask_bits;
    memcpy(&entry_count, data, 4);
    memcpy(&mask_bits, data + 4, 4);

    if (entry_count > 0 && entry_count <= 100 &&
        mask_bits <= 4) {
        return 0;  /* looks like WZT */
    }

    return -1;
}

static const char *ROAD_TYPE_NAMES[] = {
    "unknown", "freeway", "major_hwy", "boulevard", "ramp",
    "primary", "passage", "street", "pedestrian", "dirt",
    "path", "stairway", "railway", "runway", "ferry",
    "private", "parking"
};

void wzt_print_summary(const WazeTile *tile)
{
    if (!tile) return;

    printf("\n══════════════════════════════════════════\n");
    printf("           Waze Tile Summary\n");
    printf("══════════════════════════════════════════\n\n");

    printf("  Square: tile_id=%d  scale=%d  (%.4f, %.4f)  ver=%u\n",
           tile->square.tile_id, tile->square.scale,
           tile->square.latitude, tile->square.longitude,
           tile->square.timestamp);

    printf("  Header: entry_count=%u  mask_bits=%u\n\n",
           tile->entry_count, tile->mask_bits);

    printf("  Sections:\n");
    printf("    str0 (street type):     %u strings\n", wzt_count_strings(tile->sec_ptr[0], tile->sec_size[0]));
    printf("    str1 (street name):     %u strings\n", wzt_count_strings(tile->sec_ptr[1], tile->sec_size[1]));
    printf("    str2:                   %u strings\n", wzt_count_strings(tile->sec_ptr[2], tile->sec_size[2]));
    printf("    str3 (street prefix):   %u strings\n", wzt_count_strings(tile->sec_ptr[3], tile->sec_size[3]));
    printf("    str4 (street suffix):   %u strings\n", wzt_count_strings(tile->sec_ptr[4], tile->sec_size[4]));
    printf("    str5 (city):            %u strings\n", wzt_count_strings(tile->sec_ptr[5], tile->sec_size[5]));
    printf("    str6 (metadata):        %u strings\n", wzt_count_strings(tile->sec_ptr[6], tile->sec_size[6]));
    printf("    str7 (venue):           %u strings\n", wzt_count_strings(tile->sec_ptr[7], tile->sec_size[7]));
    printf("    shapes:                 %u\n", tile->shape_count);
    printf("    lines:                  %u\n", tile->line_count);
    printf("    broken:                 %u\n", tile->broken_count);
    printf("    roundabout:             %u\n", tile->roundabout_count);
    printf("    points:                 %u\n", tile->point_count);
    printf("    line routes:            %u\n", tile->line_route_count);
    printf("    streets:                %u\n", tile->street_count);
    printf("    street cities:          %u\n", tile->street_city_count);
    printf("    polygons:               %u\n", tile->polygon_count);
    printf("    speed ref:              %u\n", tile->line_speed_ref_count);
    printf("    speed avg:              %u\n", tile->line_speed_avg_count);
    printf("    speed idx:              %u\n", tile->line_speed_idx_count);
    printf("    speed idx small:        %u\n", tile->line_speed_idx_sm_count);
    printf("    ranges:                 %u\n", tile->range_count);
    printf("    alerts:                 %u\n", tile->alert_count);
    printf("    metadata attributes:    %u\n", tile->metadata_attr_count);
    printf("    venue heads:            %u\n", tile->venue_head_count);
    printf("    venue ids:              %u\n", tile->venue_id_count);
    printf("    line ext ids:           %u\n", tile->line_ext_id_count);
    printf("    line ext levels (map):  %u\n", tile->line_ext_level_by_line_count);
    printf("    line ext levels:        %u\n", tile->line_ext_level_count);
    printf("    beacon pos:             %u\n", tile->beacon_pos_count);
    printf("    beacon ids:             %u\n", tile->beacon_id_count);
    printf("    ext protobuf size:      %u bytes\n", tile->ext_protobuf_size);

    /* Print line summary */
    if (tile->line_summary && tile->line_summary_count > 0) {
        const WazeLineSummary *ls = &tile->line_summary[0];
        printf("\n  Line Summary (section 10):\n");
        printf("    First line index per road type:\n");
        for (int i = 0; i <= 16; i++) {
            if (ls->next_lines[i] != 0xFFFF)
                printf("      %-12s: %u\n", ROAD_TYPE_NAMES[i], ls->next_lines[i]);
        }
        for (int i = 17; i < 21; i++) {
            if (ls->next_lines[i] != 0xFFFF)
                printf("      type %-2d     : %u\n", i, ls->next_lines[i]);
        }
        printf("    Roundabout count: %u\n", ls->roundabout_count);
        printf("    Total broken:     %u\n", ls->total_broken_count);
        if (ls->total_broken_count > 0) {
            printf("    First broken idx per dir:\n");
            const char *dir_names[] = {
                "A→B", "B→A", "A→B_fake", "B→A_fake",
                "B→A_alt", "A→B_alt", "B→A_fake_alt", "A→B_fake_alt"
            };
            for (int i = 0; i < 8; i++) {
                if (ls->first_brokens[i] != 0xFFFF)
                    printf("      %-16s: %u\n", dir_names[i], ls->first_brokens[i]);
            }
        }
    }

    /* Print some street names */
    if (tile->street_count > 0) {
        printf("\n  Sample streets (first 10):\n");
        for (uint32_t i = 0; i < tile->street_count && i < 10; i++) {
            printf("    [%u] %s\n", i, tile->streets[i].full_name);
        }
    }

    /* Print road type distribution */
    if (tile->line_count > 0) {
        uint32_t type_counts[17] = {0};
        for (uint32_t i = 0; i < tile->line_count; i++) {
            uint8_t t = tile->lines[i].road_type;
            if (t <= 16) type_counts[t]++;
        }
        printf("\n  Road types (from parsed lines):\n");
        for (int i = 0; i <= 16; i++) {
            if (type_counts[i] > 0) {
                printf("    %-12s: %u\n", ROAD_TYPE_NAMES[i], type_counts[i]);
            }
        }
    }

    /* Print polygon distribution */
    if (tile->polygon_count > 0) {
        printf("\n  Polygon CFCC distribution:\n");
        /* Simple count table */
        uint32_t cfcc_counts[256] = {0};
        for (uint32_t i = 0; i < tile->polygon_count; i++) {
            cfcc_counts[tile->polygons[i].cfcc]++;
        }
        for (int i = 0; i < 256; i++) {
            if (cfcc_counts[i] > 0) {
                printf("    CFCC %3d: %u\n", i, cfcc_counts[i]);
            }
        }
    }

    printf("\n══════════════════════════════════════════\n");
}

/* Process an uncompressed WZT buffer: parse and print summary */
static int process_wzt(const uint8_t *data, size_t size)
{
    WazeTile *tile = wzt_parse(data, size);
    if (!tile) {
        fprintf(stderr, "Failed to parse WZT data\n");
        return -1;
    }
    wzt_print_summary(tile);
    wzt_free(tile);
    return 0;
}

/* Process a WZDF buffer: decompress, then parse WZT */
static int process_wzdf(const uint8_t *data, size_t size,
                         const char *out_path)
{
    uint8_t *decompressed = NULL;
    size_t decomp_size = 0;

    if (wzdf_decompress(data, size, &decompressed, &decomp_size) != 0) {
        return -1;
    }

    printf("Decompressed: %zu → %zu bytes\n", size, decomp_size);

    int ret = 0;
    if (out_path) {
        FILE *f = fopen(out_path, "wb");
        if (!f) {
            perror("fopen output");
            ret = -1;
        } else {
            size_t wr = fwrite(decompressed, 1, decomp_size, f);
            fclose(f);
            if (wr != decomp_size) {
                fprintf(stderr, "short write (%zu/%zu)\n", wr, decomp_size);
                ret = -1;
            } else {
                printf("Wrote decompressed WZT → %s\n", out_path);
            }
        }
    } else {
        ret = process_wzt(decompressed, decomp_size);
    }

    free(decompressed);
    return ret;
}

/* Process a WZM buffer: split, decompress each tile, print summaries */
static int process_wzm(const uint8_t *data, size_t size,
                        const char *out_dir)
{
    uint32_t tile_count = 0;
    WzmTile *tiles = wzm_split(data, size, &tile_count);
    if (!tiles) return -1;

    int ret = 0;
    for (uint32_t i = 0; i < tile_count; i++) {
        printf("\n──────────────────────────────────────────\n");
        printf("  Tile %u/%u  (id=%u, %zu bytes)\n",
               i + 1, tile_count, tiles[i].tile_id, tiles[i].data_size);

        uint8_t *decompressed = NULL;
        size_t decomp_size = 0;

        if (wzdf_decompress(tiles[i].data, tiles[i].data_size,
                            &decompressed, &decomp_size) != 0) {
            fprintf(stderr, "Failed to decompress tile %u\n", tiles[i].tile_id);
            ret = -1;
            continue;
        }

        printf("  Decompressed: %zu → %zu bytes\n",
               tiles[i].data_size, decomp_size);

        if (out_dir) {
            char out_path[1024];
            snprintf(out_path, sizeof(out_path), "%s/%u.wzt",
                     out_dir, tiles[i].tile_id);
            FILE *f = fopen(out_path, "wb");
            if (!f) {
                perror("fopen");
                ret = -1;
            } else {
                fwrite(decompressed, 1, decomp_size, f);
                fclose(f);
                printf("  Wrote → %s\n", out_path);
            }
        }

        if (!out_dir) {
            process_wzt(decompressed, decomp_size);
        }

        free(decompressed);
    }

    wzm_free_tiles(tiles, tile_count);
    return ret;
}

static void print_usage(const char *prog)
{
    printf("Usage: %s [options] <file>\n\n", prog);
    printf("Options:\n");
    printf("  -o <path>   Output path (file for WZT/WZDF, dir for WZM)\n");
    printf("  -h          Show this help\n\n");
    printf("File types handled:\n");
    printf("  .wzt   Decompressed Waze tile data\n");
    printf("  .wzdf  Compressed Waze data file (auto-decompresses)\n");
    printf("  .wzm   Waze map package (splits, decompresses each tile)\n");
}

int main(int argc, char **argv)
{
    const char *input_path = NULL;
    const char *output_path = NULL;
    Action action = ACTION_AUTO;

    /* --- Parse arguments --- */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_path = argv[++i];
                action = ACTION_DECOMPRESS;
            } else {
                fprintf(stderr, "-o requires an argument\n");
                return 1;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            input_path = argv[i];
        }
    }

    if (!input_path) {
        fprintf(stderr, "No input file specified.\n");
        print_usage(argv[0]);
        return 1;
    }

    /* --- Read input file --- */
    uint8_t *data = NULL;
    size_t data_size = 0;

    if (read_file(input_path, &data, &data_size) != 0) {
        return 1;
    }

    printf("File: %s (%zu bytes)\n", input_path, data_size);

    /* --- Detect type and dispatch --- */
    int type = detect_type(data, data_size);

    int ret = 0;
    switch (type) {
    case 0: /* WZT */
        printf("Detected: WZT (Waze Tile)\n");
        ret = process_wzt(data, data_size);
        break;

    case 1: /* WZDF */
        printf("Detected: WZDF (Waze Data File)\n");
        ret = process_wzdf(data, data_size, output_path);
        break;

    case 2: /* WZM */
        printf("Detected: WZM (Waze Map Package)\n");
        if (action == ACTION_AUTO)
            action = ACTION_SPLIT;
        ret = process_wzm(data, data_size, output_path);
        break;

    default:
        fprintf(stderr, "Unknown file format (no valid magic detected)\n");
        ret = 1;
        break;
    }

    free(data);
    return ret;
}
