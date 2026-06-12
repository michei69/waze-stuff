/* renderer.c — Waze Tile Renderer (raylib-based GUI)
 *
 * Renders .wzt / .wzdf / .wzm tiles with full detail:
 *   polygons, roads, street labels, roundabouts, points, overlays.
 *
 * For .wzm map packages all tiles are loaded and rendered simultaneously.
 *
 * Uses shared wzt/wzdf/wzm parsing from lib/.
 *
 * Controls:
 *   Mouse drag       - pan
 *   Mouse wheel      - zoom in/out
 *   Arrow keys / WASD - pan
 *   R                - reset view
 *   N                - toggle street name labels
 *   P                - toggle point nodes
 *   L                - toggle road type legend
 *   B                - toggle broken/fake point markers
 *   I                - toggle tile info overlay
 *   ESC / Q          - quit
 */

#include "raylib.h"
#include "rlgl.h"
#include "wzt.h"
#include "wzdf.h"
#include "wzm.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
//  Global state
// ============================================================================

// ---- Window ----
static int g_W = 1500, g_H = 950;

// ---- View ----
static float g_zoom = 1.0f;
static float g_ox = 0.0f;
static float g_oy = 0.0f;
static float g_min_x = 0, g_max_x = 100, g_min_y = 0, g_max_y = 100;

// ---- UI toggles ----
static bool g_show_names  = true;
static bool g_show_points = false;
static bool g_show_legend = true;
static bool g_show_info   = true;
static bool g_show_broken = true;

// ---- Tiles ----
static WazeTile **g_tiles = NULL;
static uint32_t   g_tile_count = 0;
static double    *g_tile_off_x = NULL;
static double    *g_tile_off_y = NULL;

// ---- Raw buffers for cleanup ----
static uint8_t **g_decomp_bufs = NULL;
static uint32_t  g_decomp_count = 0;
static uint8_t  *g_file_data = NULL;

// ============================================================================
//  Colour tables
// ============================================================================

static const Color CFCC_COLORS[] = {
    [10] = {80,  160, 230, 255}, [11] = { 60, 140, 220, 255},
    [12] = {170, 210, 140, 255}, [13] = { 50, 130, 210, 255},
    [14] = {30,  100, 190, 255}, [15] = {210, 195, 170, 255},
    [16] = {240, 240, 230, 255}, [20] = {140, 200, 240, 255},
    [21] = {140, 200, 100, 255}, [22] = {120, 190,  80, 255},
    [23] = {100, 170,  60, 255}, [30] = {220, 210, 190, 255},
    [31] = {200, 190, 170, 255}, [40] = {230, 225, 210, 255},
    [50] = {180, 170, 160, 255},
};
#define CFCC_MAX 50
#define DEFAULT_POLYGON_RGB ((Color){220, 195, 160, 255})

static const char *CFCC_LABELS[] = {
    [10] = "Stream/River", [11] = "River",
    [12] = "Land",         [13] = "Reservoir/Canal",
    [14] = "Ocean/Sea",    [15] = "Building/Venue",
    [16] = "City",         [20] = "Water",
    [21] = "Forest",       [22] = "Golf",
    [23] = "Cemetery",     [30] = "Generic Building",
    [31] = "Commercial",   [40] = "Built-up Area",
    [50] = "Airport",
};

static const Color ROAD_TYPE_COLORS[] = {
    [0]  = {180, 180, 180, 255}, [1]  = {255,  60,  40, 255},
    [2]  = {255, 140,  40, 255}, [3]  = {255, 200,  50, 255},
    [4]  = {255, 160, 100, 255}, [5]  = {255, 240,  80, 255},
    [6]  = {230, 230, 180, 255}, [7]  = {200, 200, 200, 255},
    [8]  = {140, 200, 140, 255}, [9]  = {180, 150, 110, 255},
    [10] = {160, 210, 160, 255}, [11] = {160, 160, 180, 255},
    [12] = {140, 120, 140, 255}, [13] = {180, 180, 200, 255},
    [14] = {100, 140, 200, 255}, [15] = {220, 160, 120, 255},
    [16] = {180, 180, 190, 255},
};
#define ROAD_TYPE_MAX 16
#define DEFAULT_LINE_RGB ((Color){100, 100, 100, 255})

static const char *LANE_TYPE_LABELS[] = {
    [0] = "REGULAR", [1] = "HOV",        [2] = "HOT",
    [3] = "EXPRESS", [4] = "REG_NO_TTS", [5] = "FAST"
};

static const char *ROAD_LABELS[] = {
    [1] = "FREEWAY",   [2] = "HIGHWAY",   [3] = "BOULEVARD",
    [4] = "RAMP",      [5] = "PRIMARY",   [6] = "PASSAGE",
    [7] = "STREET",    [8] = "PEDESTRIAN",[9] = "DIRT",
    [10]= "PATH",      [11]= "STAIRWAY",  [12]= "RAILWAY",
    [13]= "RUNWAY",    [14]= "FERRY",     [15]= "PRIVATE",
    [16]= "PARKING",
};

static const int ROAD_WIDTHS[] = {
    [0]=1, [1]=8, [2]=6, [3]=5, [4]=4, [5]=3,
    [6]=2, [7]=3, [8]=1, [9]=2, [10]=1, [11]=1,
    [12]=2, [13]=5, [14]=2, [15]=2, [16]=2,
};

static int cfcc_priority(uint8_t cfcc) {
    switch (cfcc) {
        case 16: return 0;
        case 12: return 1;
        case 20: return 2;
        case 15: return 99;
        default: return 50;
    }
}

// ============================================================================
//  Reset view to fit all tiles
// ============================================================================

static void reset_view(void) {
    float Wf = (float)g_W, Hf = (float)g_H;
    g_zoom = fminf(Wf / (g_max_x - g_min_x), Hf / (g_max_y - g_min_y)) * 0.92f;
    g_ox = (g_min_x + g_max_x) / 2.0f - Wf / 2.0f / g_zoom;
    g_oy = (g_min_y + g_max_y) / 2.0f + Hf / 2.0f / g_zoom;
}

// ============================================================================
//  Compute per-tile coordinate offsets from lat/lon
// ============================================================================

static void compute_tile_offsets(void) {
    g_tile_off_x = malloc(g_tile_count * sizeof(double));
    g_tile_off_y = malloc(g_tile_count * sizeof(double));
    if (!g_tile_off_x || !g_tile_off_y) return;

    double ref_lat = g_tiles[0]->square.latitude;
    double ref_lon = g_tiles[0]->square.longitude;

    for (uint32_t i = 0; i < g_tile_count; i++) {
        int s = g_tiles[i]->square.scale;
        double sf = 1.0;
        for (int k = 0; k < s; k++) sf *= 4.0;
        double upd = 1000000.0 / sf;

        g_tile_off_x[i] = (g_tiles[i]->square.longitude - ref_lon) * upd;
        g_tile_off_y[i] = (g_tiles[i]->square.latitude  - ref_lat) * upd;
    }
}

// ============================================================================
//  Compute world bounds across all tiles (with offsets)
// ============================================================================

static void compute_world_bounds(void) {
    bool first = true;
    for (uint32_t t = 0; t < g_tile_count; t++) {
        const WazeTile *tile = g_tiles[t];
        if (tile->point_count == 0) continue;
        double ox = g_tile_off_x ? g_tile_off_x[t] : 0.0;
        double oy = g_tile_off_y ? g_tile_off_y[t] : 0.0;
        for (uint32_t i = 0; i < tile->point_count; i++) {
            float wx = (float)(tile->points[i].x + ox);
            float wy = (float)(tile->points[i].y + oy);
            if (first) {
                g_min_x = g_max_x = wx;
                g_min_y = g_max_y = wy;
                first = false;
            } else {
                if (wx < g_min_x) g_min_x = wx;
                if (wx > g_max_x) g_max_x = wx;
                if (wy < g_min_y) g_min_y = wy;
                if (wy > g_max_y) g_max_y = wy;
            }
        }
    }
    float ww = g_max_x - g_min_x; if (ww < 100) ww = 100;
    float wh = g_max_y - g_min_y; if (wh < 100) wh = 100;
    float pad_x = ww * 0.15f, pad_y = wh * 0.15f;
    g_min_x -= pad_x; g_max_x += pad_x;
    g_min_y -= pad_y; g_max_y += pad_y;
}

// ============================================================================
//  Draw: polygons
// ============================================================================

static void draw_polygons_all(void) {
    for (uint32_t ti = 0; ti < g_tile_count; ti++) {
        const WazeTile *tile = g_tiles[ti];
        if (!tile->polygons || tile->polygon_count == 0) continue;

        const double off_x = g_tile_off_x ? g_tile_off_x[ti] : 0.0;
        const double off_y = g_tile_off_y ? g_tile_off_y[ti] : 0.0;

        uint32_t *order = malloc(tile->polygon_count * sizeof(uint32_t));
        if (!order) continue;
        for (uint32_t i = 0; i < tile->polygon_count; i++) order[i] = i;

        for (uint32_t i = 0; i < tile->polygon_count; i++) {
            for (uint32_t j = i + 1; j < tile->polygon_count; j++) {
                const int pi = cfcc_priority(tile->polygons[order[i]].cfcc);
                const int pj = cfcc_priority(tile->polygons[order[j]].cfcc);
                if (pi > pj) { const uint32_t tmp = order[i]; order[i] = order[j]; order[j] = tmp; }
            }
        }

        for (uint32_t pi = 0; pi < tile->polygon_count; pi++) {
            const WazePolygon *poly = &tile->polygons[order[pi]];
            const uint16_t count = poly->point_count;
            if (count < 3) continue;
            const uint16_t pp_start = poly->first_point_idx;
            if (pp_start + count > tile->polygon_point_count) continue;

            Vector2 points[count];
            uint16_t valid = 0;
            for (uint16_t i = 0; i < count; i++) {
                const uint16_t pt_idx = tile->polygon_points[pp_start + i] & 0x7FFF;
                if (pt_idx >= tile->point_count) continue;
                points[valid++] = world_to_screen(
                    tile->points[pt_idx].x + off_x,
                    tile->points[pt_idx].y + off_y,
                    g_ox, g_oy, g_zoom);
            }
            if (valid < 3) continue;
            makeCCW(points, valid);

            int triCount = 0;
            Vector2 *tris = triangulatePolygon(points, valid, &triCount);
            if (!tris) continue;

            const Color fill = poly->cfcc <= CFCC_MAX && CFCC_COLORS[poly->cfcc].r != 0 ? CFCC_COLORS[poly->cfcc] : DEFAULT_POLYGON_RGB;
            const Color outline = (Color){fill.r, fill.g, fill.b, 165};

            float cx = 0, cy = 0, total_area = 0;

            for (int i = 0; i < triCount; i++) {
                const Vector2 *t = &tris[i*3];
                DrawTriangle(t[0], t[1], t[2], fill);
                float ta = fabsf(cross2d(&t[0], &t[1], &t[2])) * 0.5f;
                cx += (t[0].x + t[1].x + t[2].x) / 3.0f * ta;
                cy += (t[0].y + t[1].y + t[2].y) / 3.0f * ta;
                total_area += ta;
            }
            if (total_area > 0) { cx /= total_area; cy /= total_area; }

            for (uint16_t i = 1; i < valid; i++)
                DrawLineV(points[i-1], points[i], outline);
            DrawLineV(points[valid-1], points[0], outline);

            if (poly->cfcc != 16 && poly->name && *poly->name && total_area > 400.0f) {
                int lbl_fs = (int)(14 * g_zoom);
                if (lbl_fs < 8) lbl_fs = 8;
                if (lbl_fs > 14) lbl_fs = 14;
                Vector2 ts = MeasureTextEx(GetFontDefault(), poly->name, (float)lbl_fs, 1);
                DrawTextPro(GetFontDefault(), poly->name,
                            (Vector2){ cx, cy },
                            (Vector2){ ts.x / 2, ts.y / 2 },
                            0, (float)lbl_fs, 1, Fade(BLACK, 0.55f));
            }

            free(tris);
        }
        free(order);
    }
}

// ============================================================================
//  Draw: roads
// ============================================================================

static void draw_roads_all(void) {
    for (uint32_t ti = 0; ti < g_tile_count; ti++) {
        const WazeTile *tile = g_tiles[ti];
        if (!tile->lines || tile->line_count == 0) continue;

        const double off_x = g_tile_off_x ? g_tile_off_x[ti] : 0.0;
        const double off_y = g_tile_off_y ? g_tile_off_y[ti] : 0.0;

        for (uint32_t i = 0; i < tile->line_count; i++) {
            const WazeLine *line = &tile->lines[i];
            if (line->first_point_idx >= tile->point_count ||
                line->last_point_idx  >= tile->point_count) continue;

            Color color = line->road_type <= ROAD_TYPE_MAX ? ROAD_TYPE_COLORS[line->road_type] : DEFAULT_LINE_RGB;
            int width = line->road_type <= ROAD_TYPE_MAX ? ROAD_WIDTHS[line->road_type] : 2;
            if (width < 1) width = 1;

            Vector2 scr_pts[512];
            int pt_count = 0;

            if (line->shapes && line->shape_count > 0 && line->shape_count < 500) {
                float wx, wy;
                if (line->first_point_idx < tile->point_count) {
                    wx = tile->points[line->first_point_idx].x + off_x;
                    wy = tile->points[line->first_point_idx].y + off_y;
                    scr_pts[pt_count++] = world_to_screen(wx, wy, g_ox, g_oy, g_zoom);
                }
                for (uint16_t s = 0; s < line->shape_count && pt_count < 512; s++) {
                    wx += line->shapes[s].dx;
                    wy += line->shapes[s].dy;
                    scr_pts[pt_count++] = world_to_screen(wx, wy, g_ox, g_oy, g_zoom);
                }
                const WazePoint *ep = &tile->points[line->last_point_idx];
                scr_pts[pt_count++] = world_to_screen(
                    ep->x + off_x, ep->y + off_y, g_ox, g_oy, g_zoom);
            } else {
                const WazePoint *p1 = &tile->points[line->first_point_idx];
                const WazePoint *p2 = &tile->points[line->last_point_idx];
                scr_pts[0] = world_to_screen(p1->x + off_x, p1->y + off_y, g_ox, g_oy, g_zoom);
                scr_pts[1] = world_to_screen(p2->x + off_x, p2->y + off_y, g_ox, g_oy, g_zoom);
                pt_count = 2;
            }
            if (pt_count < 2) continue;

            for (int j = 0; j < pt_count - 1; j++)
                DrawLineEx(scr_pts[j], scr_pts[j + 1], (float)width, color);

            // One-way arrows
            {
                bool a_allowed = line->a_to_b & ROUTE_CAR_ALLOWED;
                bool b_allowed = line->b_to_a & ROUTE_CAR_ALLOWED;
                if (a_allowed != b_allowed) {
                    int start = 0, end = pt_count - 1, step = 1;
                    if (!a_allowed) { start = pt_count - 1; end = 0; step = -1; }

                    float acc = 0;
                    float spacing = 60.0f;
                    float next_arrow = spacing * 0.5f;
                    float arrow_sz = (float)(width + 3) * 1.5f;

                    for (int j = start; j != end; j += step) {
                        float seg_dx = scr_pts[j + step].x - scr_pts[j].x;
                        float seg_dy = scr_pts[j + step].y - scr_pts[j].y;
                        float seg_len = hypotf(seg_dx, seg_dy);
                        if (seg_len < 0.5f) { acc += seg_len; continue; }

                        float dir_x = seg_dx / seg_len;
                        float dir_y = seg_dy / seg_len;
                        float perp_x = -dir_y;
                        float perp_y = dir_x;

                        while (next_arrow <= acc + seg_len) {
                            float t = (next_arrow - acc) / seg_len;
                            float cx = scr_pts[j].x + t * seg_dx;
                            float cy = scr_pts[j].y + t * seg_dy;

                            Vector2 tri[] = {
                                { cx + dir_x * arrow_sz, cy + dir_y * arrow_sz },
                                { cx + perp_x * arrow_sz * 0.5f, cy + perp_y * arrow_sz * 0.5f },
                                { cx - perp_x * arrow_sz * 0.5f, cy - perp_y * arrow_sz * 0.5f }
                            };
                            makeCCW(tri, 3);
                            DrawTriangle(tri[0], tri[1], tri[2], Fade(BLACK, 0.25f));
                            next_arrow += spacing;
                        }
                        acc += seg_len;
                    }
                }
            }

            // Street name label
            if (g_show_names && line->street && line->street->full_name) {
                float total_len = 0;
                for (int j = 0; j < pt_count - 1; j++)
                    total_len += hypotf(scr_pts[j + 1].x - scr_pts[j].x, scr_pts[j + 1].y - scr_pts[j].y);
                if (total_len < 30) continue;

                float half = total_len / 2;
                float acc = 0;
                float mx = 0, my = 0, seg_dx = 0, seg_dy = 0;
                for (int j = 0; j < pt_count - 1; j++) {
                    float d = hypotf(scr_pts[j + 1].x - scr_pts[j].x, scr_pts[j + 1].y - scr_pts[j].y);
                    seg_dx = scr_pts[j + 1].x - scr_pts[j].x;
                    seg_dy = scr_pts[j + 1].y - scr_pts[j].y;
                    if (acc + d >= half) {
                        float t = d > 0 ? (half - acc) / d : 0;
                        mx = scr_pts[j].x + t * seg_dx;
                        my = scr_pts[j].y + t * seg_dy;
                        break;
                    }
                    acc += d;
                }

                const char *label = line->street->full_name;
                if (!label || !*label) continue;

                float angle = atan2f(seg_dy, seg_dx) * RAD2DEG;
                if (angle < -90) angle += 180;
                else if (angle > 90) angle -= 180;

                int font_size = (int)(18 * g_zoom);
                if (font_size < 8) font_size = 8;
                if (font_size > 18) font_size = 18;

                Vector2 text_size = MeasureTextEx(GetFontDefault(), label, (float)font_size, 1);
                DrawTextPro(GetFontDefault(), label,
                            (Vector2){ mx, my },
                            (Vector2){ text_size.x / 2, text_size.y / 2 },
                            angle, (float)font_size, 1, (Color){50, 50, 50, 255});
            }
        }
    }
}

// ============================================================================
//  Draw: broken point markers
// ============================================================================

static void draw_broken_markers_all(void) {
    for (uint32_t ti = 0; ti < g_tile_count; ti++) {
        const WazeTile *tile = g_tiles[ti];
        bool *seen = calloc(tile->point_count, sizeof(bool));
        if (!seen) continue;

        const double off_x = g_tile_off_x ? g_tile_off_x[ti] : 0.0;
        const double off_y = g_tile_off_y ? g_tile_off_y[ti] : 0.0;

        for (uint32_t i = 0; i < tile->line_count; i++) {
            const WazeLine *line = &tile->lines[i];
            if (line->first_point_fake && !seen[line->first_point_idx]) {
                seen[line->first_point_idx] = true;
                const WazePoint *p = &tile->points[line->first_point_idx];
                Vector2 sp = world_to_screen(p->x + off_x, p->y + off_y, g_ox, g_oy, g_zoom);
                DrawCircleLinesV(sp, 6, (Color){200, 80, 200, 255});
            }
            if (line->last_point_fake && !seen[line->last_point_idx]) {
                seen[line->last_point_idx] = true;
                const WazePoint *p = &tile->points[line->last_point_idx];
                Vector2 sp = world_to_screen(p->x + off_x, p->y + off_y, g_ox, g_oy, g_zoom);
                DrawCircleLinesV(sp, 6, (Color){200, 80, 200, 255});
            }
        }
        free(seen);
    }
}

// ============================================================================
//  Draw: point nodes (debug)
// ============================================================================

static void draw_points_all(void) {
    for (uint32_t ti = 0; ti < g_tile_count; ti++) {
        const WazeTile *tile = g_tiles[ti];
        bool *fake = calloc(tile->point_count, sizeof(bool));
        if (!fake) continue;

        const double off_x = g_tile_off_x ? g_tile_off_x[ti] : 0.0;
        const double off_y = g_tile_off_y ? g_tile_off_y[ti] : 0.0;

        for (uint32_t i = 0; i < tile->line_count; i++) {
            const WazeLine *line = &tile->lines[i];
            fake[line->first_point_idx] = line->first_point_fake;
            fake[line->last_point_idx] = line->last_point_fake;
        }

        for (uint32_t i = 0; i < tile->point_count; i++) {
            const WazePoint *p = &tile->points[i];
            const Vector2 sp = world_to_screen(p->x + off_x, p->y + off_y, g_ox, g_oy, g_zoom);
            if (fake[i])
                DrawCircleV(sp, 3, (Color){220, 50, 50, 255});
            else
                DrawCircleV(sp, 3, (Color){50, 50, 220, 255});
        }
        free(fake);
    }
}

// ============================================================================
//  Hover: find closest line/poly & draw popups
// ============================================================================

typedef struct {
    int32_t tile_idx;
    int32_t elem_idx;
} HoverResult;

static float point_to_segment_dist_sq(Vector2 p, Vector2 a, Vector2 b) {
    float dx = b.x - a.x, dy = b.y - a.y;
    float len_sq = dx * dx + dy * dy;
    if (len_sq < 0.0001f) {
        float ddx = p.x - a.x, ddy = p.y - a.y;
        return ddx * ddx + ddy * ddy;
    }
    float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len_sq;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    float proj_x = a.x + t * dx;
    float proj_y = a.y + t * dy;
    float sx = p.x - proj_x, sy = p.y - proj_y;
    return sx * sx + sy * sy;
}

static bool point_in_polygon_screen(Vector2 p, Vector2 *verts, int count) {
    if (count < 3) return false;
    bool inside = false;
    for (int i = 0, j = count - 1; i < count; j = i++) {
        if ((verts[i].y > p.y) != (verts[j].y > p.y)) {
            float xint = (verts[j].x - verts[i].x) * (p.y - verts[i].y)
                       / (verts[j].y - verts[i].y) + verts[i].x;
            if (p.x < xint) inside = !inside;
        }
    }
    return inside;
}

static HoverResult find_hovered_line(Vector2 mouse_screen) {
    HoverResult best = {-1, -1};
    float best_dist_sq = (10 * 10);

    for (uint32_t ti = 0; ti < g_tile_count; ti++) {
        const WazeTile *tile = g_tiles[ti];
        if (!tile->lines || tile->line_count == 0) continue;

        const double off_x = g_tile_off_x ? g_tile_off_x[ti] : 0.0;
        const double off_y = g_tile_off_y ? g_tile_off_y[ti] : 0.0;

        for (uint32_t i = 0; i < tile->line_count; i++) {
            const WazeLine *line = &tile->lines[i];
            if (line->first_point_idx >= tile->point_count ||
                line->last_point_idx  >= tile->point_count) continue;

            if (line->shapes && line->shape_count > 0 && line->shape_count < 500) {
                float wx, wy;
                wx = tile->points[line->first_point_idx].x + off_x;
                wy = tile->points[line->first_point_idx].y + off_y;
                Vector2 prev = world_to_screen(wx, wy, g_ox, g_oy, g_zoom);
                for (uint16_t s = 0; s < line->shape_count; s++) {
                    wx += line->shapes[s].dx;
                    wy += line->shapes[s].dy;
                    Vector2 cur = world_to_screen(wx, wy, g_ox, g_oy, g_zoom);
                    float d = point_to_segment_dist_sq(mouse_screen, prev, cur);
                    if (d < best_dist_sq) {
                        best_dist_sq = d;
                        best.tile_idx = (int32_t)ti;
                        best.elem_idx = (int32_t)i;
                    }
                    prev = cur;
                }
                const WazePoint *ep = &tile->points[line->last_point_idx];
                Vector2 end = world_to_screen(ep->x + off_x, ep->y + off_y, g_ox, g_oy, g_zoom);
                float d = point_to_segment_dist_sq(mouse_screen, prev, end);
                if (d < best_dist_sq) {
                    best_dist_sq = d;
                    best.tile_idx = (int32_t)ti;
                    best.elem_idx = (int32_t)i;
                }
            } else {
                const WazePoint *p1 = &tile->points[line->first_point_idx];
                const WazePoint *p2 = &tile->points[line->last_point_idx];
                Vector2 a = world_to_screen(p1->x + off_x, p1->y + off_y, g_ox, g_oy, g_zoom);
                Vector2 b = world_to_screen(p2->x + off_x, p2->y + off_y, g_ox, g_oy, g_zoom);
                float d = point_to_segment_dist_sq(mouse_screen, a, b);
                if (d < best_dist_sq) {
                    best_dist_sq = d;
                    best.tile_idx = (int32_t)ti;
                    best.elem_idx = (int32_t)i;
                }
            }
        }
    }
    return best;
}

static HoverResult find_hovered_polygon(Vector2 mouse_screen) {
    HoverResult best = {-1, -1};

    for (uint32_t ti = 0; ti < g_tile_count; ti++) {
        const WazeTile *tile = g_tiles[ti];
        if (!tile->polygons || tile->polygon_count == 0) continue;

        const double off_x = g_tile_off_x ? g_tile_off_x[ti] : 0.0;
        const double off_y = g_tile_off_y ? g_tile_off_y[ti] : 0.0;

        for (uint32_t pi = 0; pi < tile->polygon_count; pi++) {
            const WazePolygon *poly = &tile->polygons[pi];
            if (poly->point_count < 3) continue;
            if (poly->first_point_idx + poly->point_count > tile->polygon_point_count) continue;

            Vector2 points[poly->point_count];
            int valid = 0;
            for (uint16_t i = 0; i < poly->point_count; i++) {
                uint16_t pt_idx = tile->polygon_points[poly->first_point_idx + i] & 0x7FFF;
                if (pt_idx >= tile->point_count) continue;
                points[valid++] = world_to_screen(
                    tile->points[pt_idx].x + off_x,
                    tile->points[pt_idx].y + off_y,
                    g_ox, g_oy, g_zoom);
            }
            if (valid < 3) continue;

            if (poly->cfcc != 16 && point_in_polygon_screen(mouse_screen, points, valid)) {
                best.tile_idx = (int32_t)ti;
                best.elem_idx = (int32_t)pi;
                return best;
            }
        }
    }
    return best;
}

static void draw_line_popup(HoverResult hr, Vector2 mouse) {
    if (hr.tile_idx < 0 || (uint32_t)hr.tile_idx >= g_tile_count) return;
    const WazeTile *tile = g_tiles[hr.tile_idx];
    if (hr.elem_idx < 0 || hr.elem_idx >= (int32_t)tile->line_count) return;
    const WazeLine *line = &tile->lines[hr.elem_idx];

    int font_size = 11;
    int line_h = font_size + 4;
    int row = 0;

    char buf[256];

    int px = (int)mouse.x + 16;
    int py = (int)mouse.y + 16;
    int scr_w = GetScreenWidth(), scr_h = GetScreenHeight();

    char ab_avg_s[8], ab_max_s[8], ba_avg_s[8], ba_max_s[8];
#define FMT_SPD(dst, v) \
    ((v) == 0 || (v) == 255 ? snprintf(dst, sizeof(dst), "N/A") : snprintf(dst, sizeof(dst), "%d", (v)))
    FMT_SPD(ab_avg_s, line->a_to_b_avg_speed);
    FMT_SPD(ab_max_s, line->a_to_b_max_speed);
    FMT_SPD(ba_avg_s, line->b_to_a_avg_speed);
    FMT_SPD(ba_max_s, line->b_to_a_max_speed);
#undef FMT_SPD

    const char *type_label = (line->road_type <= ROAD_TYPE_MAX && ROAD_LABELS[line->road_type])
                             ? ROAD_LABELS[line->road_type] : "UNKNOWN";
    const char *street_name = (line->street && line->street->full_name && *line->street->full_name)
                              ? line->street->full_name : "(unnamed)";
    const char *lane_type = line->lane_type <= 5 && LANE_TYPE_LABELS[line->lane_type] ? LANE_TYPE_LABELS[line->lane_type] : "UNKNOWN";

    int rows = 9;
    int popup_w = 260, popup_h = rows * line_h + 12;
    if (px + popup_w > scr_w) px = (int)mouse.x - popup_w - 8;
    if (py + popup_h > scr_h) py = (int)mouse.y - popup_h - 8;

    DrawRectangle(px, py, popup_w, popup_h, Fade(BLACK, 0.85f));
    DrawRectangleLines(px, py, popup_w, popup_h, (Color){180, 180, 180, 255});

    int tx = px + 8, ty = py + 6;

    snprintf(buf, sizeof(buf), "Tile: %d  Road: %s (%d)",
             tile->square.tile_id, type_label, line->road_type);
    DrawText(buf, tx, ty + row * line_h, font_size, (Color){255, 240, 150, 255});
    row++;

    snprintf(buf, sizeof(buf), "Name: %s", street_name);
    DrawText(buf, tx, ty + row * line_h, font_size, WHITE);
    row++;

    snprintf(buf, sizeof(buf), "Level: %d   Lane: %s   Attr: 0x%08X",
             line->level, lane_type, line->attributes);
    DrawText(buf, tx, ty + row * line_h, font_size, WHITE);
    row++;

    snprintf(buf, sizeof(buf), "Speed A->B: %s avg / %s max", ab_avg_s, ab_max_s);
    DrawText(buf, tx, ty + row * line_h, font_size, WHITE);
    row++;

    snprintf(buf, sizeof(buf), "Speed B->A: %s avg / %s max", ba_avg_s, ba_max_s);
    DrawText(buf, tx, ty + row * line_h, font_size, WHITE);
    row++;

    snprintf(buf, sizeof(buf), "Flags A:0x%02X B:0x%02X   ExtID: %u",
             line->a_to_b, line->b_to_a, line->ext_id);
    DrawText(buf, tx, ty + row * line_h, font_size, WHITE);
    row++;

    snprintf(buf, sizeof(buf), "Pt: %u->%u   Shapes: %u   StIdx: %u",
             line->first_point_idx & POINT_INDEX_MASK,
             line->last_point_idx  & POINT_INDEX_MASK,
             line->shape_count, line->street_idx);
    DrawText(buf, tx, ty + row * line_h, font_size, WHITE);
    row++;

    snprintf(buf, sizeof(buf), "Broken: %s  Roundabout: %s",
             line->is_broken ? "Yes" : "No", line->is_roundabout ? "Yes" : "No");
    DrawText(buf, tx, ty + row * line_h, font_size, WHITE);
    row++;

    snprintf(buf, sizeof(buf), "Lat: %.4f  Lon: %.4f",
             tile->square.latitude, tile->square.longitude);
    DrawText(buf, tx, ty + row * line_h, font_size, WHITE);
}

static void draw_polygon_popup(HoverResult hr, Vector2 mouse) {
    if (hr.tile_idx < 0 || (uint32_t)hr.tile_idx >= g_tile_count) return;
    const WazeTile *tile = g_tiles[hr.tile_idx];
    if (hr.elem_idx < 0 || hr.elem_idx >= (int32_t)tile->polygon_count) return;
    const WazePolygon *poly = &tile->polygons[hr.elem_idx];

    int font_size = 11;
    int line_h = font_size + 4;
    int row = 0;

    char buf[256];

    int px = (int)mouse.x + 16;
    int py = (int)mouse.y + 16;
    int scr_w = GetScreenWidth(), scr_h = GetScreenHeight();

    const char *cfcc_label = (poly->cfcc <= CFCC_MAX && CFCC_LABELS[poly->cfcc])
                             ? CFCC_LABELS[poly->cfcc] : "UNKNOWN";
    const char *name = (poly->name && *poly->name) ? poly->name : "(unnamed)";

    int rows = 6;
    int popup_w = 250, popup_h = rows * line_h + 12;
    if (px + popup_w > scr_w) px = (int)mouse.x - popup_w - 8;
    if (py + popup_h > scr_h) py = (int)mouse.y - popup_h - 8;

    DrawRectangle(px, py, popup_w, popup_h, Fade(BLACK, 0.85f));
    DrawRectangleLines(px, py, popup_w, popup_h, (Color){180, 180, 180, 255});

    int tx = px + 8, ty = py + 6;

    snprintf(buf, sizeof(buf), "Tile: %d  CFCC: %s (%d)",
             tile->square.tile_id, cfcc_label, poly->cfcc);
    DrawText(buf, tx, ty + row * line_h, font_size, (Color){160, 255, 160, 255});
    row++;

    snprintf(buf, sizeof(buf), "Name: %s", name);
    DrawText(buf, tx, ty + row * line_h, font_size, WHITE);
    row++;

    snprintf(buf, sizeof(buf), "Points: %u   Ext: 0x%08X",
             poly->point_count, poly->ex_data);
    DrawText(buf, tx, ty + row * line_h, font_size, WHITE);
    row++;

    snprintf(buf, sizeof(buf), "Bounds N:%u W:%u E:%u S:%u",
             poly->north, poly->west, poly->east, poly->south);
    DrawText(buf, tx, ty + row * line_h, font_size, WHITE);
    row++;

    snprintf(buf, sizeof(buf), "Idx: pt[%u]  count:%u",
             poly->first_point_idx, poly->point_count);
    DrawText(buf, tx, ty + row * line_h, font_size, WHITE);
    row++;

    snprintf(buf, sizeof(buf), "Lat: %.4f  Lon: %.4f",
             tile->square.latitude, tile->square.longitude);
    DrawText(buf, tx, ty + row * line_h, font_size, WHITE);
}

// ============================================================================
//  Draw: info panel overlay (top-left)
// ============================================================================

static void draw_info_panel(void) {
    char buf[256];
    int y = 8;
    int font_size = 12;

    uint32_t total_lines = 0, total_pts = 0, total_streets = 0, total_polys = 0;
    uint32_t total_entries = 0;

    for (uint32_t t = 0; t < g_tile_count; t++) {
        const WazeTile *tile = g_tiles[t];
        total_entries += tile->entry_count;
        total_lines   += tile->line_count;
        total_pts     += tile->point_count;
        total_streets += tile->street_count;
        total_polys   += tile->polygon_count;
    }

    int rows = (g_tile_count > 1) ? 5 : 4;
    DrawRectangle(8, 8, 305, rows * 15 + 12, Fade((Color){25, 25, 25, 255}, 0.82f));

    snprintf(buf, sizeof(buf), "Tiles: %u  zoom=%.2fx", g_tile_count, g_zoom);
    DrawText(buf, 8, y, font_size, WHITE); y += 15;

    snprintf(buf, sizeof(buf), "entries=%u  lines=%u  pts=%u",
             total_entries, total_lines, total_pts);
    DrawText(buf, 8, y, font_size, WHITE); y += 15;

    snprintf(buf, sizeof(buf), "streets=%u  polygons=%u",
             total_streets, total_polys);
    DrawText(buf, 8, y, font_size, WHITE); y += 15;

    if (g_tile_count > 1) {
        double lat_min = g_tiles[0]->square.latitude;
        double lat_max = lat_min;
        double lon_min = g_tiles[0]->square.longitude;
        double lon_max = lon_min;
        for (uint32_t t = 1; t < g_tile_count; t++) {
            double lat = g_tiles[t]->square.latitude;
            double lon = g_tiles[t]->square.longitude;
            if (lat < lat_min) lat_min = lat;
            if (lat > lat_max) lat_max = lat;
            if (lon < lon_min) lon_min = lon;
            if (lon > lon_max) lon_max = lon;
        }
        snprintf(buf, sizeof(buf), "Lat: %.4f..%.4f  Lon: %.4f..%.4f",
                 lat_min, lat_max, lon_min, lon_max);
        DrawText(buf, 8, y, font_size, WHITE); y += 15;
    }

    snprintf(buf, sizeof(buf), "ox=%.2f oy=%.2f", g_ox, g_oy);
    DrawText(buf, 8, y, font_size, WHITE);
}

// ============================================================================
//  Draw: road type + polygon legend (bottom-left)
// ============================================================================

static void draw_legend(void) {
    bool road_used[ROAD_TYPE_MAX + 1] = {0};
    for (uint32_t t = 0; t < g_tile_count; t++) {
        const WazeTile *tile = g_tiles[t];
        for (uint32_t i = 0; i < tile->line_count; i++) {
            uint8_t rt = tile->lines[i].road_type;
            if (rt <= ROAD_TYPE_MAX) road_used[rt] = true;
        }
    }

    int rcount = 0;
    for (int i = 0; i <= ROAD_TYPE_MAX; i++) if (road_used[i]) rcount++;

    bool poly_used[CFCC_MAX + 1] = {0};
    for (uint32_t t = 0; t < g_tile_count; t++) {
        const WazeTile *tile = g_tiles[t];
        for (uint32_t i = 0; i < tile->polygon_count; i++) {
            uint8_t cfcc = tile->polygons[i].cfcc;
            if (cfcc != 16) poly_used[cfcc] = true;
        }
    }

    int pcount = 0;
    for (int i = 0; i <= CFCC_MAX; i++) if (poly_used[i]) pcount++;

    if (rcount == 0 && pcount == 0) return;

    int font_size = 10;

    int rbox_h = rcount > 0 ? rcount * 16 + 12 : 0;
    int pbox_h = pcount > 0 ? pcount * 16 + 12 : 0;
    int box_h = rbox_h > pbox_h ? rbox_h : pbox_h;

    int x = 8, y = g_H - box_h - 8;

    if (rcount > 0) {
        int box_w = 180;
        DrawRectangle(x, y, box_w, rbox_h, Fade((Color){240, 240, 240, 255}, 0.82f));
        int row = 0;
        for (int rt = 0; rt <= ROAD_TYPE_MAX; rt++) {
            if (!road_used[rt]) continue;
            int ly = y + 8 + row * 16;
            Color color = ROAD_TYPE_COLORS[rt];
            int width = (ROAD_WIDTHS[rt] > 0) ? ROAD_WIDTHS[rt] : 2;
            DrawLine(x + 6, ly + 7, x + 36, ly + 7, color);
            for (int w = 1; w < width; w++) {
                DrawLine(x + 6, ly + 7 + w, x + 36, ly + 7 + w, color);
                DrawLine(x + 6, ly + 7 - w, x + 36, ly + 7 - w, color);
            }
            const char *label = ROAD_LABELS[rt] ? ROAD_LABELS[rt] : "";
            DrawText(label, x + 44, ly + 2, font_size, BLACK);
            row++;
        }
    }

    if (pcount > 0) {
        int px = x + 188;
        int py = g_H - pbox_h - 8;
        DrawRectangle(px, py, 140, pbox_h, Fade((Color){240, 240, 240, 255}, 0.82f));
        int prow = 0;
        for (int cfcc = 0; cfcc <= CFCC_MAX; cfcc++) {
            if (!poly_used[cfcc]) continue;
            Color color = (CFCC_COLORS[cfcc].r != 0) ? CFCC_COLORS[cfcc] : DEFAULT_POLYGON_RGB;
            Color outline = (Color){color.r, color.g, color.b, 150};
            DrawRectangle(px + 6, py + 8 + prow * 16, 14, 14, color);
            DrawRectangleLines(px + 6, py + 8 + prow * 16, 14, 14, outline);
            const char *label = CFCC_LABELS[cfcc] ? CFCC_LABELS[cfcc] : "";
            DrawText(label, px + 26, py + 8 + prow * 16, font_size, BLACK);
            prow++;
        }
    }
}

// ============================================================================
//  File loading helpers
// ============================================================================

static int read_file(const char *path, uint8_t **out, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return -1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return -1; }
    *out = (uint8_t *)malloc((size_t)sz);
    if (!*out) { fclose(f); return -1; }
    size_t rd = fread(*out, 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) { free(*out); return -1; }
    *out_size = (size_t)sz;
    return 0;
}

static int detect_type(const uint8_t *data, size_t size) {
    if (size < 4) return -1;
    uint32_t m;
    memcpy(&m, data, 4);
    if (m == WZDF_MAGIC) return 1;
    if (m == WZM_MAGIC)  return 2;
    uint32_t entry_count, mask_bits;
    memcpy(&entry_count, data, 4);
    memcpy(&mask_bits, data + 4, 4);
    if (entry_count > 0 && entry_count <= 100 && mask_bits <= 4) return 0;
    return -1;
}

// ============================================================================
//  Cleanup
// ============================================================================

static void cleanup(void) {
    if (g_tiles) {
        for (uint32_t i = 0; i < g_tile_count; i++)
            wzt_free(g_tiles[i]);
        free(g_tiles);
        g_tiles = NULL;
    }
    free(g_tile_off_x);
    free(g_tile_off_y);
    g_tile_off_x = NULL;
    g_tile_off_y = NULL;
    if (g_decomp_bufs) {
        for (uint32_t i = 0; i < g_decomp_count; i++)
            free(g_decomp_bufs[i]);
        free(g_decomp_bufs);
        g_decomp_bufs = NULL;
    }
    free(g_file_data);
    g_file_data = NULL;
    g_tile_count = 0;
    g_decomp_count = 0;
}

// ============================================================================
//  Main
// ============================================================================

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.wzt|file.wzdf|file.wzm>\n", argv[0]);
        return 1;
    }

    // ---- Read file ----
    size_t file_size = 0;
    if (read_file(argv[1], &g_file_data, &file_size) != 0) return 1;

    int type = detect_type(g_file_data, file_size);
    if (type < 0) {
        fprintf(stderr, "Unknown file format\n");
        cleanup();
        return 1;
    }

    if (type == 2) {
        // ---- WZM: split into tiles, decompress each, parse each ----
        uint32_t raw_count = 0;
        WzmTile *raw_tiles = wzm_split(g_file_data, file_size, &raw_count);
        if (!raw_tiles) {
            fprintf(stderr, "Failed to split WZM package\n");
            cleanup();
            return 1;
        }

        g_tiles = malloc(raw_count * sizeof(WazeTile *));
        if (!g_tiles) {
            wzm_free_tiles(raw_tiles, raw_count);
            cleanup();
            return 1;
        }

        g_decomp_bufs = malloc(raw_count * sizeof(uint8_t *));
        if (!g_decomp_bufs) {
            wzm_free_tiles(raw_tiles, raw_count);
            cleanup();
            return 1;
        }

        for (uint32_t i = 0; i < raw_count; i++) {
            printf("Tile %u/%u: id=%u size=%zu\n",
                   i + 1, raw_count, raw_tiles[i].tile_id, raw_tiles[i].data_size);

            uint8_t *decomp = NULL;
            size_t decomp_sz = 0;
            if (wzdf_decompress(raw_tiles[i].data, raw_tiles[i].data_size,
                                &decomp, &decomp_sz) != 0) {
                fprintf(stderr, "Decompression failed for tile %u (id=%u)\n",
                        i, raw_tiles[i].tile_id);
                continue;
            }
            printf("  Decompressed: %zu → %zu bytes\n", raw_tiles[i].data_size, decomp_sz);

            g_decomp_bufs[g_decomp_count] = decomp;
            g_decomp_count++;

            WazeTile *tile = wzt_parse(decomp, decomp_sz);
            if (!tile) {
                fprintf(stderr, "Failed to parse tile %u (id=%u)\n",
                        i, raw_tiles[i].tile_id);
                continue;
            }

            g_tiles[g_tile_count++] = tile;
            printf("  Parsed: %u lines, %u pts, %u streets, %u polygons\n",
                   tile->line_count, tile->point_count,
                   tile->street_count, tile->polygon_count);
        }

        wzm_free_tiles(raw_tiles, raw_count);

        if (g_tile_count == 0) {
            fprintf(stderr, "No valid tiles extracted from WZM\n");
            cleanup();
            return 1;
        }
    } else {
        // ---- Single tile: .wzt or .wzdf ----
        uint8_t *tile_data = g_file_data;
        size_t tile_size = file_size;
        uint8_t *decomp_data = NULL;
        size_t decomp_size = 0;

        if (type == 1) {
            printf("Decompressing WZDF...\n");
            if (wzdf_decompress(g_file_data, file_size, &decomp_data, &decomp_size) != 0) {
                fprintf(stderr, "Decompression failed\n");
                cleanup();
                return 1;
            }
            tile_data = decomp_data;
            tile_size = decomp_size;
            printf("Decompressed: %zu -> %zu bytes\n", file_size, decomp_size);
        }

        WazeTile *tile = wzt_parse(tile_data, tile_size);
        if (!tile) {
            fprintf(stderr, "Failed to parse tile\n");
            free(decomp_data);
            cleanup();
            return 1;
        }

        g_tiles = malloc(sizeof(WazeTile *));
        g_tiles[0] = tile;
        g_tile_count = 1;

        if (decomp_data) {
            g_decomp_bufs = malloc(sizeof(uint8_t *));
            g_decomp_bufs[0] = decomp_data;
            g_decomp_count = 1;
        }

        printf("Tile %d: %u lines, %u pts, %u streets, %u polygons\n",
               tile->square.tile_id, tile->line_count, tile->point_count,
               tile->street_count, tile->polygon_count);
    }

    // ---- Compute per-tile offsets, then world bounds across all tiles ----
    compute_tile_offsets();
    compute_world_bounds();

    // ---- Window ----
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    const char *win_title;
    char title_buf[256];
    if (g_tile_count == 1) {
        win_title = TextFormat("Waze Tile %d  (%.4f, %.4f)",
                               g_tiles[0]->square.tile_id,
                               g_tiles[0]->square.latitude,
                               g_tiles[0]->square.longitude);
    } else {
        win_title = TextFormat("Waze Map — %u tiles", g_tile_count);
    }
    // Copy to avoid dangling pointer from TextFormat's static buffer
    strncpy(title_buf, win_title, sizeof(title_buf) - 1);
    title_buf[sizeof(title_buf) - 1] = '\0';

    InitWindow(g_W, g_H, title_buf);
    SetTargetFPS(60);

    // ---- View state ----
    reset_view();

    bool dragging = false;
    Vector2 drag_start = {0, 0};
    float drag_ox = 0, drag_oy = 0;

    // ---- Main loop ----
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt < 0.001f) dt = 0.001f;

        // Mouse wheel zoom
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            Vector2 mouse = GetMousePosition();
            float wx = mouse.x / g_zoom + g_ox;
            float wy = g_oy - mouse.y / g_zoom;
            g_zoom *= (wheel > 0) ? 1.2f : (1.0f / 1.2f);
            if (g_zoom < 0.001f) g_zoom = 0.001f;
            g_ox = wx - mouse.x / g_zoom;
            g_oy = wy + mouse.y / g_zoom;
        }

        // Keyboard toggles
        if (IsKeyPressed(KEY_R)) reset_view();
        if (IsKeyPressed(KEY_N)) g_show_names  = !g_show_names;
        if (IsKeyPressed(KEY_P)) g_show_points = !g_show_points;
        if (IsKeyPressed(KEY_L)) g_show_legend = !g_show_legend;
        if (IsKeyPressed(KEY_I)) g_show_info   = !g_show_info;
        if (IsKeyPressed(KEY_B)) g_show_broken = !g_show_broken;

        // Mouse drag
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            dragging = true;
            drag_start = GetMousePosition();
            drag_ox = g_ox;
            drag_oy = g_oy;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            dragging = false;
        if (dragging) {
            Vector2 mouse = GetMousePosition();
            float dx = mouse.x - drag_start.x;
            float dy = mouse.y - drag_start.y;
            g_ox = drag_ox - dx / g_zoom;
            g_oy = drag_oy + dy / g_zoom;
        }

        // Keyboard pan
        float speed = 350.0f / g_zoom * dt;
        if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) g_ox -= speed;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) g_ox += speed;
        if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) g_oy += speed;
        if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) g_oy -= speed;

        // Window resize
        if (IsWindowResized()) {
            g_W = GetScreenWidth();
            g_H = GetScreenHeight();
        }

        // ---- Draw ----
        BeginDrawing();
        ClearBackground((Color){252, 252, 248, 255});

        draw_polygons_all();
        draw_roads_all();

        if (g_show_broken)
            draw_broken_markers_all();

        if (g_show_points)
            draw_points_all();

        // Hover popups
        {
            Vector2 mouse = GetMousePosition();
            HoverResult hl = find_hovered_line(mouse);
            if (hl.tile_idx >= 0) {
                draw_line_popup(hl, mouse);
            } else {
                HoverResult hp = find_hovered_polygon(mouse);
                if (hp.tile_idx >= 0)
                    draw_polygon_popup(hp, mouse);
            }
        }

        if (g_show_info)
            draw_info_panel();
        if (g_show_legend)
            draw_legend();

        EndDrawing();
    }

    CloseWindow();
    cleanup();
    return 0;
}
