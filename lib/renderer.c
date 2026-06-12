/* renderer.c — Waze Tile Renderer (raylib-based GUI)
 *
 * Renders .wzt / .wzdf tiles with full detail:
 *   polygons, roads, street labels, roundabouts, points, overlays.
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
//  Colour tables (matching draw_wzt.py)
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

// CFCC draw priority (lower = drawn first = behind)
static int cfcc_priority(uint8_t cfcc) {
    switch (cfcc) {
        case 16: return 0;   // city base (back)
        case 12: return 1;   // land
        case 20: return 2;   // water
        case 15: return 99;  // buildings (front)
        default: return 50;
    }
}

// ============================================================================
//  Draw: polygons
// ============================================================================

static void draw_polygons(const WazeTile *tile, const float ox, const float oy, const float zoom) {
    if (!tile->polygons || tile->polygon_count == 0) return;

    // Sort by CFCC priority
    uint32_t *order = malloc(tile->polygon_count * sizeof(uint32_t));
    if (!order) return;
    for (uint32_t i = 0; i < tile->polygon_count; i++) order[i] = i;

    // yes this is slow. do i care? no
    for (uint32_t i = 0; i < tile->polygon_count; i++) {
        for (uint32_t j = i + 1; j < tile->polygon_count; j++) {
            const int pi = cfcc_priority(tile->polygons[order[i]].cfcc);
            const int pj = cfcc_priority(tile->polygons[order[j]].cfcc);
            if (pi > pj) { const uint32_t t = order[i]; order[i] = order[j]; order[j] = t; }
        }
    }

    // draw the polygons themselves
    for (uint32_t pi = 0; pi < tile->polygon_count; pi++) {
        const WazePolygon *poly = &tile->polygons[order[pi]];
        const uint16_t count = poly->point_count;
        if (count < 3) continue;
        const uint16_t pp_start = poly->first_point_idx;
        if (pp_start + count > tile->polygon_point_count) continue;

        Vector2 points[count];
        for (uint16_t i = 0; i < count; i++) {
            const uint16_t pt_idx = tile->polygon_points[pp_start + i] & 0x7FFF;
            if (pt_idx >= tile->point_count) continue;
            points[i] = world_to_screen(tile->points[pt_idx].x, tile->points[pt_idx].y, ox, oy, zoom);
        }
        makeCCW(points, count);
        int triCount = 0;
        Vector2 *tris = triangulatePolygon(points, count, &triCount);
        if (!tris) {
            fprintf(stderr, "No tris!!!");
            continue;
        }

        const Color fill = poly->cfcc <= CFCC_MAX && CFCC_COLORS[poly->cfcc].r != 0 ? CFCC_COLORS[poly->cfcc] : DEFAULT_POLYGON_RGB;
        const Color outline = (Color){fill.r, fill.g, fill.b, 165};

        float cx = 0, cy = 0, total_area = 0;

        // fill
        for (int i = 0; i < triCount; i++) {
            const Vector2 *t = &tris[i*3];
            DrawTriangle(t[0], t[1], t[2], fill);
            float ta = fabsf(cross2d(&t[0], &t[1], &t[2])) * 0.5f;
            cx += (t[0].x + t[1].x + t[2].x) / 3.0f * ta;
            cy += (t[0].y + t[1].y + t[2].y) / 3.0f * ta;
            total_area += ta;
        }
        if (total_area > 0) { cx /= total_area; cy /= total_area; }
        // outline
        for (uint16_t i = 1; i < count; i++) {
            DrawLineV(points[i-1], points[i], outline);
        }
        DrawLineV(points[count-1], points[0], outline);

        // Venue name label
        if (poly->cfcc != 16 && poly->name && *poly->name && total_area > 400.0f) {
            int lbl_fs = (int)(14 * zoom);
            if (lbl_fs < 8) lbl_fs = 8;
            if (lbl_fs > 14) lbl_fs = 14;
            Vector2 ts = MeasureTextEx(GetFontDefault(), poly->name, (float)lbl_fs, 1);
            DrawTextPro(GetFontDefault(), poly->name,
                        (Vector2){ cx, cy },
                        (Vector2){ ts.x / 2, ts.y / 2 },
                        0, (float)lbl_fs, 1, Fade(BLACK, 0.55f));
        }

        // free ts
        free(tris);
    }
    free(order);
}

// ============================================================================
//  Draw: roads
// ============================================================================

static void draw_roads(const WazeTile *tile, float ox, float oy, float zoom, bool show_names) {
    if (!tile->lines || tile->line_count == 0) return;

    for (uint32_t i = 0; i < tile->line_count; i++) {
        const WazeLine *line = &tile->lines[i];
        if (line->first_point_idx >= tile->point_count ||
            line->last_point_idx  >= tile->point_count) continue;

        Color color = line->road_type <= ROAD_TYPE_MAX ? ROAD_TYPE_COLORS[line->road_type] : DEFAULT_LINE_RGB;
        int width = line->road_type <= ROAD_TYPE_MAX ? ROAD_WIDTHS[line->road_type] : 2;
        if (width < 1) width = 1;

        // Build screen-space point list
        Vector2 scr_pts[line->shape_count + 2];
        int pt_count = 0;

        if (line->shapes && line->shape_count > 0 && line->shape_count < 500) {
            // Accumulate world-space positions via delta decoding
            float wx, wy;
            if (line->first_point_idx < tile->point_count) {
                wx = tile->points[line->first_point_idx].x;
                wy = tile->points[line->first_point_idx].y;
                scr_pts[pt_count++] = world_to_screen(wx, wy, ox, oy, zoom);
            }
            for (uint16_t s = 0; s < line->shape_count && pt_count < 510; s++) {
                wx += line->shapes[s].dx;
                wy += line->shapes[s].dy;
                scr_pts[pt_count++] = world_to_screen(wx, wy, ox, oy, zoom);
            }
            // Add end point
            const WazePoint *ep = &tile->points[line->last_point_idx];
            scr_pts[pt_count++] = world_to_screen(ep->x, ep->y, ox, oy, zoom);
        } else {
            const WazePoint *p1 = &tile->points[line->first_point_idx];
            const WazePoint *p2 = &tile->points[line->last_point_idx];
            scr_pts[0] = world_to_screen(p1->x, p1->y, ox, oy, zoom);
            scr_pts[1] = world_to_screen(p2->x, p2->y, ox, oy, zoom);
            pt_count = 2;
        }
        if (pt_count < 2) continue;

        // Draw polyline
        for (int i = 0; i < pt_count - 1; i++) {
            DrawLineEx(scr_pts[i], scr_pts[i + 1], (float)width, color);
        }

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

                for (int i = start; i != end; i += step) {
                    float seg_dx = scr_pts[i + step].x - scr_pts[i].x;
                    float seg_dy = scr_pts[i + step].y - scr_pts[i].y;
                    float seg_len = hypotf(seg_dx, seg_dy);
                    if (seg_len < 0.5f) { acc += seg_len; continue; }

                    float dir_x = seg_dx / seg_len;
                    float dir_y = seg_dy / seg_len;
                    float perp_x = -dir_y;
                    float perp_y = dir_x;

                    while (next_arrow <= acc + seg_len) {
                        float t = (next_arrow - acc) / seg_len;
                        float cx = scr_pts[i].x + t * seg_dx;
                        float cy = scr_pts[i].y + t * seg_dy;

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
        if (show_names && line->street && line->street->full_name) {
            // Calculate total segment length and midpoint
            float total_len = 0;
            for (int j = 0; j < pt_count - 1; j++) {
                total_len += hypotf(scr_pts[j + 1].x - scr_pts[j].x, scr_pts[j + 1].y - scr_pts[j].y);
            }
            if (total_len < 30) continue;

            float half = total_len / 2;
            float acc = 0;
            float mx = 0, my = 0, seg_dx = 0, seg_dy = 0;
            for (int i = 0; i < pt_count - 1; i++) {
                float d = hypotf(scr_pts[i + 1].x - scr_pts[i].x, scr_pts[i + 1].y - scr_pts[i].y);
                seg_dx = scr_pts[i + 1].x - scr_pts[i].x;
                seg_dy = scr_pts[i + 1].y - scr_pts[i].y;
                if (acc + d >= half) {
                    float t = d > 0 ? (half - acc) / d : 0;
                    mx = scr_pts[i].x + t * seg_dx;
                    my = scr_pts[i].y + t * seg_dy;
                    break;
                }
                acc += d;
            }

            const char *label = line->street->full_name;
            if (!label || !*label) {
                // label = (line->road_type <= ROAD_TYPE_MAX && ROAD_LABELS[line->road_type])
                //          ? ROAD_LABELS[line->road_type] : NULL;
            }
            if (!label) continue;

            float angle = atan2f(seg_dy, seg_dx) * RAD2DEG;
            if (angle < -90) angle += 180;
            else if (angle > 90) angle -= 180;

            int font_size = (int)(18 * zoom);
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

// ============================================================================
//  Draw: broken point markers (fake tile-border points)
// ============================================================================

static void draw_broken_markers(const WazeTile *tile, float ox, float oy, float zoom) {
    bool *seen = calloc(tile->point_count, sizeof(bool));
    if (!seen) return;

    for (uint32_t i = 0; i < tile->line_count; i++) {
        const WazeLine *line = &tile->lines[i];
        if (line->first_point_fake) {
            seen[line->first_point_idx] = true;
            const WazePoint *p = &tile->points[line->first_point_idx];
            Vector2 sp = world_to_screen(p->x, p->y, ox, oy, zoom);
            DrawCircleLinesV(sp, 6, (Color){200, 80, 200, 255});
        }
        if (line->last_point_fake) {
            seen[line->last_point_idx] = true;
            const WazePoint *p = &tile->points[line->last_point_idx];
            Vector2 sp = world_to_screen(p->x, p->y, ox, oy, zoom);
            DrawCircleLinesV(sp, 6, (Color){200, 80, 200, 255});
        }
    }
    free(seen);
}

// ============================================================================
//  Hover: find closest line/poly & draw popups
// ============================================================================

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

static int32_t find_hovered_line(const WazeTile *tile, float ox, float oy,
                                  float zoom, Vector2 mouse_screen) {
    if (!tile->lines || tile->line_count == 0) return -1;

    float best_dist_sq = (10 * 10);
    int32_t best_line = -1;

    for (uint32_t i = 0; i < tile->line_count; i++) {
        const WazeLine *line = &tile->lines[i];
        if (line->first_point_idx >= tile->point_count ||
            line->last_point_idx  >= tile->point_count) continue;

        if (line->shapes && line->shape_count > 0 && line->shape_count < 500) {
            float wx, wy;
            wx = tile->points[line->first_point_idx].x;
            wy = tile->points[line->first_point_idx].y;
            Vector2 prev = world_to_screen(wx, wy, ox, oy, zoom);
            for (uint16_t s = 0; s < line->shape_count; s++) {
                wx += line->shapes[s].dx;
                wy += line->shapes[s].dy;
                Vector2 cur = world_to_screen(wx, wy, ox, oy, zoom);
                float d = point_to_segment_dist_sq(mouse_screen, prev, cur);
                if (d < best_dist_sq) {
                    best_dist_sq = d;
                    best_line = (int32_t)i;
                }
                prev = cur;
            }
            const WazePoint *ep = &tile->points[line->last_point_idx];
            Vector2 end = world_to_screen(ep->x, ep->y, ox, oy, zoom);
            float d = point_to_segment_dist_sq(mouse_screen, prev, end);
            if (d < best_dist_sq) { best_dist_sq = d; best_line = (int32_t)i; }
        } else {
            const WazePoint *p1 = &tile->points[line->first_point_idx];
            const WazePoint *p2 = &tile->points[line->last_point_idx];
            Vector2 a = world_to_screen(p1->x, p1->y, ox, oy, zoom);
            Vector2 b = world_to_screen(p2->x, p2->y, ox, oy, zoom);
            float d = point_to_segment_dist_sq(mouse_screen, a, b);
            if (d < best_dist_sq) { best_dist_sq = d; best_line = (int32_t)i; }
        }
    }
    return best_line;
}

static int32_t find_hovered_polygon(const WazeTile *tile, float ox, float oy,
                                     float zoom, Vector2 mouse_screen) {
    if (!tile->polygons || tile->polygon_count == 0) return -1;

    for (uint32_t pi = 0; pi < tile->polygon_count; pi++) {
        const WazePolygon *poly = &tile->polygons[pi];
        if (poly->point_count < 3) continue;
        if (poly->first_point_idx + poly->point_count > tile->polygon_point_count) continue;

        Vector2 points[poly->point_count];
        int valid = 0;
        for (uint16_t i = 0; i < poly->point_count; i++) {
            uint16_t pt_idx = tile->polygon_points[poly->first_point_idx + i] & 0x7FFF;
            if (pt_idx >= tile->point_count) continue;
            points[valid++] = world_to_screen(tile->points[pt_idx].x, tile->points[pt_idx].y, ox, oy, zoom);
        }
        if (valid < 3) continue;

        if (poly->cfcc != 16 && point_in_polygon_screen(mouse_screen, points, valid))
            return (int32_t)pi;
    }
    return -1;
}

static void draw_line_popup(const WazeTile *tile, int32_t line_idx, Vector2 mouse) {
    if (line_idx < 0 || line_idx >= (int32_t)tile->line_count) return;
    const WazeLine *line = &tile->lines[line_idx];

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

    int rows = 8;
    int popup_w = 260, popup_h = rows * line_h + 12;
    if (px + popup_w > scr_w) px = (int)mouse.x - popup_w - 8;
    if (py + popup_h > scr_h) py = (int)mouse.y - popup_h - 8;

    DrawRectangle(px, py, popup_w, popup_h, Fade(BLACK, 0.85f));
    DrawRectangleLines(px, py, popup_w, popup_h, (Color){180, 180, 180, 255});

    int tx = px + 8, ty = py + 6;

    snprintf(buf, sizeof(buf), "Road: %s (%d)", type_label, line->road_type);
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

    snprintf(buf, sizeof(buf), "Broken: %s, Roundabout: %s", line->is_broken ? "Yes" : "No", line->is_roundabout ? "Yes" : "No");
    DrawText(buf, tx, ty + row * line_h, font_size, WHITE);
}

static void draw_polygon_popup(const WazeTile *tile, int32_t poly_idx, Vector2 mouse) {
    if (poly_idx < 0 || poly_idx >= (int32_t)tile->polygon_count) return;
    const WazePolygon *poly = &tile->polygons[poly_idx];

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

    int rows = 5;
    int popup_w = 250, popup_h = rows * line_h + 12;
    if (px + popup_w > scr_w) px = (int)mouse.x - popup_w - 8;
    if (py + popup_h > scr_h) py = (int)mouse.y - popup_h - 8;

    DrawRectangle(px, py, popup_w, popup_h, Fade(BLACK, 0.85f));
    DrawRectangleLines(px, py, popup_w, popup_h, (Color){180, 180, 180, 255});

    int tx = px + 8, ty = py + 6;

    snprintf(buf, sizeof(buf), "CFCC: %s (%d)", cfcc_label, poly->cfcc);
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
}

// ============================================================================
//  Draw: point nodes (debug)
// ============================================================================

static void draw_points(const WazeTile *tile, float ox, float oy, float zoom) {
    bool *fake = calloc(tile->point_count, sizeof(bool));
    if (!fake) return;

    for (uint32_t i = 0; i < tile->line_count; i++) {
        const WazeLine *line = &tile->lines[i];
        fake[line->first_point_idx] = line->first_point_fake;
        fake[line->last_point_idx] = line->last_point_fake;
    }

    for (uint32_t i = 0; i < tile->point_count; i++) {
        const WazePoint *p = &tile->points[i];
        const Vector2 sp = world_to_screen(p->x, p->y, ox, oy, zoom);
        if (fake[i]) {
            DrawCircleV(sp, 3, (Color){220, 50, 50, 255});
        } else {
            DrawCircleV(sp, 3, (Color){50, 50, 220, 255});
        }
    }

    free(fake);
}

// ============================================================================
//  Draw: info panel overlay (top-left)
// ============================================================================

static void draw_info_panel(const WazeTile *tile, float zoom) {
    char buf[256];
    int y = 8;
    int font_size = 12;

    DrawRectangle(8, 8, 275, 65, Fade((Color){25, 25, 25, 255}, 0.82f));

    snprintf(buf, sizeof(buf), "Tile %d  scale=%d  (%.4f, %.4f)",
             tile->square.tile_id, tile->square.scale,
             tile->square.latitude, tile->square.longitude);
    DrawText(buf, 8, y, font_size, WHITE); y += 15;

    snprintf(buf, sizeof(buf), "entries=%u  lines=%u  pts=%u",
             tile->entry_count, tile->line_count, tile->point_count);
    DrawText(buf, 8, y, font_size, WHITE); y += 15;

    snprintf(buf, sizeof(buf), "streets=%u  polygons=%u",
             tile->street_count, tile->polygon_count);
    DrawText(buf, 8, y, font_size, WHITE); y += 15;

    snprintf(buf, sizeof(buf), "zoom=%.2fx", zoom);
    DrawText(buf, 8, y, font_size, WHITE);
}

// ============================================================================
//  Draw: road type + polygon legend (bottom-left)
// ============================================================================

static void draw_legend(const WazeTile *tile, int screen_h) {
    // Determine which road types are used
    bool used[ROAD_TYPE_MAX + 1] = {0};
    for (uint32_t i = 0; i < tile->line_count; i++) {
        uint8_t t = tile->lines[i].road_type;
        if (t <= ROAD_TYPE_MAX) used[t] = true;
    }

    int count = 0;
    for (int i = 0; i <= ROAD_TYPE_MAX; i++) if (used[i]) count++;
    if (count == 0) return;

    int font_size = 10;
    int box_w = 180, box_h = count * 16 + 12;
    int x = 8, y = screen_h - box_h - 8;

    // Background
    DrawRectangle(x, y, box_w, box_h, Fade((Color){240, 240, 240, 255}, 0.82f));

    int row = 0;
    for (int rt = 0; rt <= ROAD_TYPE_MAX; rt++) {
        if (!used[rt]) continue;
        int ly = y + 8 + row * 16;
        Color color = ROAD_TYPE_COLORS[rt];
        int width = (ROAD_WIDTHS[rt] > 0) ? ROAD_WIDTHS[rt] : 2;
        DrawLine(x + 6, ly + 7, x + 36, ly + 7, color);
        // Thicker lines
        for (int w = 1; w < width; w++) {
            DrawLine(x + 6, ly + 7 + w, x + 36, ly + 7 + w, color);
            DrawLine(x + 6, ly + 7 - w, x + 36, ly + 7 - w, color);
        }
        const char *label = ROAD_LABELS[rt] ? ROAD_LABELS[rt] : "";
        DrawText(label, x + 44, ly + 2, font_size, BLACK);
        row++;
    }

    // Polygon legend (to the right of road legend)
    bool poly_used[CFCC_MAX + 1] = {0};
    for (uint32_t i = 0; i < tile->polygon_count; i++) {
        uint8_t cfcc = tile->polygons[i].cfcc;
        if (cfcc != 16) poly_used[cfcc] = true;
    }
    int pcount = 0;
    for (int i = 0; i <= CFCC_MAX; i++) if (poly_used[i]) pcount++;
    if (pcount > 0) {
        int px = x + box_w + 8;
        int py = screen_h - pcount * 16 - 12;
        DrawRectangle(px, py, 140, pcount * 16 + 12,
                      Fade((Color){240, 240, 240, 255}, 0.82f));
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
//  Main
// ============================================================================

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.wzt|file.wzdf>\n", argv[0]);
        return 1;
    }

    // ---- Load and parse ----
    uint8_t *file_data = NULL;
    size_t file_size = 0;
    if (read_file(argv[1], &file_data, &file_size) != 0) return 1;

    uint8_t *decomp_data = NULL;
    size_t decomp_size = 0;
    uint8_t *tile_data = file_data;
    size_t tile_size = file_size;

    int type = detect_type(file_data, file_size);
    if (type == 1) {  // WZDF
        printf("Decompressing WZDF...\n");
        if (wzdf_decompress(file_data, file_size, &decomp_data, &decomp_size) != 0) {
            fprintf(stderr, "Decompression failed\n");
            free(file_data);
            return 1;
        }
        tile_data = decomp_data;
        tile_size = decomp_size;
        printf("Decompressed: %zu → %zu bytes\n", file_size, decomp_size);
    } else if (type == 2) {  // WZM
        // TODO: support multiple tiles
        fprintf(stderr, "WZM files not yet supported in renderer. "
                "Extract tiles first with waze-parser.\n");
        free(file_data);
        return 1;
    }

    WazeTile *tile = wzt_parse(tile_data, tile_size);
    if (!tile) {
        fprintf(stderr, "Failed to parse tile\n");
        free(decomp_data);
        free(file_data);
        return 1;
    }

    printf("Tile %d: %u lines, %u pts, %u streets, %u polygons\n",
           tile->square.tile_id, tile->line_count, tile->point_count,
           tile->street_count, tile->polygon_count);

    // ---- Compute world bounds ----
    float min_x = 0, max_x = 100, min_y = 0, max_y = 100;
    if (tile->point_count > 0) {
        min_x = max_x = tile->points[0].x;
        min_y = max_y = tile->points[0].y;
        for (uint32_t i = 1; i < tile->point_count; i++) {
            if (tile->points[i].x < min_x) min_x = tile->points[i].x;
            if (tile->points[i].x > max_x) max_x = tile->points[i].x;
            if (tile->points[i].y < min_y) min_y = tile->points[i].y;
            if (tile->points[i].y > max_y) max_y = tile->points[i].y;
        }
    }
    float ww = max_x - min_x; if (ww < 100) ww = 100;
    float wh = max_y - min_y; if (wh < 100) wh = 100;
    float pad_x = ww * 0.15f, pad_y = wh * 0.15f;
    min_x -= pad_x; max_x += pad_x;
    min_y -= pad_y; max_y += pad_y;

    // ---- Window ----
    int W = 1500, H = 950;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(W, H, TextFormat("Waze Tile %d  (%.4f, %.4f)",
                 tile->square.tile_id, tile->square.latitude, tile->square.longitude));
    SetTargetFPS(60);

    // ---- View state ----
    float zoom = fminf(W / (max_x - min_x), H / (max_y - min_y)) * 0.92f;
    float ox = (min_x + max_x) / 2 - W / 2 / zoom;
    float oy = (min_y + max_y) / 2 + H / 2 / zoom;

    bool show_names   = true;
    bool show_points  = false;
    bool show_legend  = true;
    bool show_info    = true;
    bool show_broken  = true;

    bool dragging = false;
    Vector2 drag_start = {0, 0};
    float drag_ox = 0, drag_oy = 0;

    // ---- Main loop ----
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt < 0.001f) dt = 0.001f;

        // ---- Input ----
        // Mouse wheel zoom
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            Vector2 mouse = GetMousePosition();
            // Convert mouse screen pos to world before zoom change
            float wx = mouse.x / zoom + ox;
            float wy = oy - mouse.y / zoom;
            zoom *= (wheel > 0) ? 1.2f : (1.0f / 1.2f);
            if (zoom < 0.001f) zoom = 0.001f;
            ox = wx - mouse.x / zoom;
            oy = wy + mouse.y / zoom;
        }

        // Keyboard toggles
        if (IsKeyPressed(KEY_R)) {
            zoom = fminf(W / (max_x - min_x), H / (max_y - min_y)) * 0.92f;
            ox = (min_x + max_x) / 2 - W / 2 / zoom;
            oy = (min_y + max_y) / 2 + H / 2 / zoom;
        }
        if (IsKeyPressed(KEY_N)) show_names  = !show_names;
        if (IsKeyPressed(KEY_P)) show_points = !show_points;
        if (IsKeyPressed(KEY_L)) show_legend = !show_legend;
        if (IsKeyPressed(KEY_I)) show_info   = !show_info;
        if (IsKeyPressed(KEY_B)) show_broken = !show_broken;

        // Mouse drag
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            dragging = true;
            drag_start = GetMousePosition();
            drag_ox = ox;
            drag_oy = oy;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            dragging = false;
        }
        if (dragging) {
            Vector2 mouse = GetMousePosition();
            float dx = mouse.x - drag_start.x;
            float dy = mouse.y - drag_start.y;
            ox = drag_ox - dx / zoom;
            oy = drag_oy + dy / zoom;
        }

        // Keyboard pan
        float speed = 350.0f / zoom * dt;
        if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) ox -= speed;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) ox += speed;
        if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) oy += speed;
        if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) oy -= speed;

        // Window resize
        if (IsWindowResized()) {
            W = GetScreenWidth();
            H = GetScreenHeight();
        }

        // ---- Draw ----
        BeginDrawing();
        ClearBackground((Color){252, 252, 248, 255});

        // Layer 1: polygons
        draw_polygons(tile, ox, oy, zoom);

        // Layer 2: roads
        draw_roads(tile, ox, oy, zoom, show_names);

        // Roundabout markers
        // draw_roundabouts(tile, ox, oy, zoom);

        // Broken/fake point markers
        if (show_broken)
            draw_broken_markers(tile, ox, oy, zoom);

        // Point nodes (debug)
        if (show_points)
            draw_points(tile, ox, oy, zoom);

        // Hover popups
        {
            Vector2 mouse = GetMousePosition();
            int32_t hl = find_hovered_line(tile, ox, oy, zoom, mouse);
            if (hl >= 0) {
                draw_line_popup(tile, hl, mouse);
            } else {
                int32_t hp = find_hovered_polygon(tile, ox, oy, zoom, mouse);
                if (hp >= 0) draw_polygon_popup(tile, hp, mouse);
            }
        }

        // Overlays
        if (show_info)
            draw_info_panel(tile, zoom);
        if (show_legend)
            draw_legend(tile, H);

        EndDrawing();
    }

    // ---- Cleanup ----
    CloseWindow();
    wzt_free(tile);
    free(decomp_data);
    free(file_data);
    return 0;
}
