#ifndef WAZE_TOOLS_UTILS_H
#define WAZE_TOOLS_UTILS_H

#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline float cross2d(const Vector2 *a, const Vector2 *b, const Vector2 *c) {
    return (b->x - a->x) * (c->y - a->y) - (b->y - a->y) * (c->x - a->x);
}

// Check if point P is inside triangle ABC (strict interior)
static inline bool pointInTriangle(const Vector2 *p, const Vector2 *a,
                            const Vector2 *b, const Vector2 *c) {
    float d1 = cross2d(a, b, p);
    float d2 = cross2d(b, c, p);
    float d3 = cross2d(c, a, p);
    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);
}

// Compute signed area of a polygon (positive for CCW)
static inline float polygonArea(const Vector2 *verts, int count) {
    float area = 0.0f;
    for (int i = 0; i < count; ++i) {
        int j = (i + 1) % count;
        area += verts[i].x * verts[j].y - verts[j].x * verts[i].y;
    }
    return area * 0.5f;
}

// Ensure polygon vertices are in counter‑clockwise order
static inline void makeCCW(Vector2 *verts, int count) {
    if (polygonArea(verts, count) > 0) {
        // Reverse order if clockwise
        for (int i = 0, j = count - 1; i < j; ++i, --j) {
            Vector2 tmp = verts[i];
            verts[i] = verts[j];
            verts[j] = tmp;
        }
    }
}

// ----------------------------------------------
// Ear‑clipping triangulation
// ----------------------------------------------
// Returns an array of triangles (3 vertices per triangle) and sets *triCount.
// Caller must free() the returned pointer.
static inline Vector2 *triangulatePolygon(const Vector2 *verts, const int vertCount, int *triCount) {
    if (vertCount < 3) {
        *triCount = 0;
        return NULL;
    }

    // Working copy – we’ll remove ears from it
    int n = vertCount;
    Vector2 *work = malloc(n * sizeof(Vector2));
    if (!work) {
        fprintf(stderr, "malloc failed\n");
        return NULL;
    }
    for (int i = 0; i < n; ++i) work[i] = verts[i];

    // Max triangles = n - 2, so max output vertices = (n - 2) * 3
    Vector2 *triangles = malloc((n - 2) * 3 * sizeof(Vector2));
    if (!triangles) {
        fprintf(stderr, "malloc failed\n");
        free(work);
        return NULL;
    }

    int triIdx = 0;

    int *indices = malloc(n * sizeof(int));
    for (int i = 0; i < n; ++i) indices[i] = i;

    while (n > 3) {
        bool earFound = false;

        for (int i = 0; i < n; ++i) {
            const int prev = (i + n - 1) % n;
            const int cur  = i;
            const int next = (i + 1) % n;

            Vector2 a = work[prev];
            Vector2 b = work[cur];
            Vector2 c = work[next];

            // Check if triangle is convex (positive area)
            if (cross2d(&a, &b, &c) > 0) continue;   // reflex vertex, can’t be ear

            // Test if any other vertex lies inside this triangle
            bool inside = false;
            for (int j = 0; j < n; ++j) {
                if (j == prev || j == cur || j == next) continue;
                if (pointInTriangle(&work[j], &a, &b, &c)) {
                    inside = true;
                    break;
                }
            }

            if (!inside) {
                // Ear found – output the triangle
                triangles[triIdx++] = a;
                triangles[triIdx++] = b;
                triangles[triIdx++] = c;

                // Remove vertex ‘cur’ from working polygon
                for (int k = cur; k < n - 1; ++k) work[k] = work[k + 1];
                n--;
                earFound = true;
                break;
            }
        }

        // If no ear was found, the polygon is degenerate – fallback to fan
        if (!earFound) {
            // Simple fan triangulation (works for convex parts)
            for (int i = 2; i < n; ++i) {
                triangles[triIdx++] = work[0];
                triangles[triIdx++] = work[i - 1];
                triangles[triIdx++] = work[i];
            }
            break;
        }
    }

    // Last triangle (n == 3)
    if (n == 3) {
        triangles[triIdx++] = work[0];
        triangles[triIdx++] = work[1];
        triangles[triIdx++] = work[2];
    }

    *triCount = triIdx / 3;
    free(indices);
    free(work);
    return triangles;
}

static inline Vector2 world_to_screen(float wx, float wy, float ox, float oy, float zoom) {
    return (Vector2){ (wx - ox) * zoom, (oy - wy) * zoom };
}

#ifdef __cplusplus
}
#endif

#endif //WAZE_TOOLS_UTILS_H
