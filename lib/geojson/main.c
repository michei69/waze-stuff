#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include "cJSON.h"

// ============================================================================
// Scale data
// ============================================================================
#define SCALE_STEP      4
#define SQUARE_SIZE     10000
#define MAX_SQUARE_SIZE 30000000
#define LAT_RANGE       179999999
#define LON_RANGE       359999999
#define MAX_SCALES      16

typedef struct {
    int    scale_factor;
    int    num_rows;
    int    num_cols;
    int    base_index;
    double step;
    double recip;
} Scale;

static Scale g_scale[MAX_SCALES];
static int   g_max_scale;

static void scales_init(void)
{
    int size = SQUARE_SIZE;
    int s    = 0;
    while (size <= MAX_SQUARE_SIZE) { s++; size *= SCALE_STEP; }
    g_max_scale = s - 1;

    int    base   = 0;
    int    factor = 1;
    for (int i = 0; i <= g_max_scale; i++) {
        g_scale[i].scale_factor = factor;
        g_scale[i].base_index   = base;
        g_scale[i].num_rows     = LAT_RANGE / (factor * SQUARE_SIZE) + 1;
        g_scale[i].num_cols     = LON_RANGE / (factor * SQUARE_SIZE) + 1;
        g_scale[i].step         = 0.01 * factor;
        g_scale[i].recip        = 1000000.0 / factor;
        base += g_scale[i].num_rows * g_scale[i].num_cols;
        factor *= SCALE_STEP;
    }
}

// ============================================================================
// Dynamic int vector
// ============================================================================
typedef struct { int *data; int count; int cap; } IntVec;

static inline void iv_push(IntVec *v, int val)
{
    if (v->count >= v->cap) {
        v->cap = v->cap ? v->cap * 2 : 64;
        v->data = realloc(v->data, (size_t)v->cap * sizeof(int));
    }
    v->data[v->count++] = val;
}

// ============================================================================
// Per-tile point hash set — O(1) dedup, power-of-two open addressing
// ============================================================================
#define PT_HASH_MIN_CAP 256u

typedef struct {
    uint32_t key;
    int32_t  idx;
} PtHashSlot;

static inline uint32_t pt_hash_mix(uint32_t k)
{
    k ^= k >> 16;
    k *= 0x85ebca6bU;
    k ^= k >> 13;
    k *= 0xc2b2ae35U;
    k ^= k >> 16;
    return k;
}

// ============================================================================
// Generic lookup hash for line/poly entries (O(1) per-tile lookup)
// ============================================================================
typedef struct { int key; int idx; } LookupSlot;

#define LK_MIN_CAP 16u

static inline int lk_find(LookupSlot *tbl, uint32_t mask, int key)
{
    if (!tbl) return -1;
    uint32_t idx = ((uint32_t)key * 2654435761u) & mask;
    for (;;) {
        if (tbl[idx].idx < 0) return -1;
        if (tbl[idx].key == key) return tbl[idx].idx;
        idx = (idx + 1) & mask;
    }
}

static void lk_grow(LookupSlot **tbl, uint32_t *mask, int old_cap)
{
    int new_cap = old_cap ? old_cap * 2 : (int)LK_MIN_CAP;
    LookupSlot *new_tbl = calloc((size_t)new_cap, sizeof(LookupSlot));
    for (int i = 0; i < new_cap; i++) new_tbl[i].idx = -1;

    uint32_t new_mask = (uint32_t)(new_cap - 1);
    if (*tbl) {
        for (int i = 0; i < old_cap; i++) {
            if ((*tbl)[i].idx < 0) continue;
            uint32_t hidx = ((uint32_t)(*tbl)[i].key * 2654435761u) & new_mask;
            while (new_tbl[hidx].idx >= 0)
                hidx = (hidx + 1) & new_mask;
            new_tbl[hidx] = (*tbl)[i];
        }
        free(*tbl);
    }
    *tbl  = new_tbl;
    *mask = new_mask;
}

static inline void lk_insert(LookupSlot **tbl, uint32_t *mask, int *fill, int key, int idx)
{
    if (!*tbl || *fill * 10 >= (int)(*mask + 1) * 7)
        lk_grow(tbl, mask, *tbl ? (int)(*mask + 1) : 0);

    uint32_t hidx = ((uint32_t)key * 2654435761u) & *mask;
    while ((*tbl)[hidx].idx >= 0)
        hidx = (hidx + 1) & *mask;
    (*tbl)[hidx].key = key;
    (*tbl)[hidx].idx = idx;
    (*fill)++;
}

// ============================================================================
// Per-tile data
// ============================================================================
typedef struct { int x, y; } Pt2i;

typedef struct { int key; IntVec pts; int type; } LineEntry;

typedef struct {
    int    key;
    IntVec pts;
    int    type;
    int    north, west, south, east;
} PolyEntry;

typedef struct {
    Pt2i       *pts;
    int         pt_count, pt_cap;
    PtHashSlot *pt_hash;
    uint32_t    pt_hash_mask;
    int         pt_hash_fill;

    LineEntry  *lines;
    int         line_count, line_cap;
    LookupSlot *line_hash;
    uint32_t    line_hash_mask;
    int         line_hash_fill;

    PolyEntry  *polys;
    int         poly_count, poly_cap;
    LookupSlot *poly_hash;
    uint32_t    poly_hash_mask;
    int         poly_hash_fill;
} TileData;

static int td_pt_hash_find(TileData *td, uint32_t key)
{
    if (!td->pt_hash) return -1;
    uint32_t mask = td->pt_hash_mask;
    uint32_t idx  = pt_hash_mix(key) & mask;
    int cap = (int)mask + 1;
    for (int p = 0; p < cap; p++) {
        if (td->pt_hash[idx].idx < 0) return -1;
        if (td->pt_hash[idx].key == key) return td->pt_hash[idx].idx;
        idx = (idx + 1) & mask;
    }
    return -1;
}

static void td_pt_hash_grow(TileData *td)
{
    int old_cap = td->pt_hash ? (int)td->pt_hash_mask + 1 : 0;
    int new_cap = old_cap ? old_cap * 2 : (int)PT_HASH_MIN_CAP;
    PtHashSlot *old_tbl = td->pt_hash;
    PtHashSlot *new_tbl = calloc((size_t)new_cap, sizeof(PtHashSlot));
    for (int i = 0; i < new_cap; i++) new_tbl[i].idx = -1;

    uint32_t new_mask = (uint32_t)(new_cap - 1);
    for (int i = 0; i < old_cap; i++) {
        if (old_tbl[i].idx < 0) continue;
        uint32_t hidx = pt_hash_mix(old_tbl[i].key) & new_mask;
        while (new_tbl[hidx].idx >= 0)
            hidx = (hidx + 1) & new_mask;
        new_tbl[hidx] = old_tbl[i];
    }
    free(old_tbl);
    td->pt_hash = new_tbl;
    td->pt_hash_mask = new_mask;
}

static int td_add_point(TileData *td, int x, int y)
{
    uint32_t key = ((uint32_t)x << 16) | (uint32_t)y;
    int found = td_pt_hash_find(td, key);
    if (found >= 0) return found;

    if (td->pt_count >= td->pt_cap) {
        td->pt_cap = td->pt_cap ? td->pt_cap * 2 : 256;
        td->pts = realloc(td->pts, (size_t)td->pt_cap * sizeof(Pt2i));
    }
    int new_idx = td->pt_count;
    td->pts[new_idx].x = x;
    td->pts[new_idx].y = y;
    td->pt_count++;

    if (!td->pt_hash || td->pt_hash_fill * 10 >= (int)(td->pt_hash_mask + 1) * 7)
        td_pt_hash_grow(td);

    uint32_t mask = td->pt_hash_mask;
    uint32_t hidx = pt_hash_mix(key) & mask;
    while (td->pt_hash[hidx].idx >= 0)
        hidx = (hidx + 1) & mask;
    td->pt_hash[hidx].key = key;
    td->pt_hash[hidx].idx = new_idx;
    td->pt_hash_fill++;
    return new_idx;
}

static LineEntry *td_get_or_create_line(TileData *td, int key, int type)
{
    int found = lk_find(td->line_hash, td->line_hash_mask, key);
    if (found >= 0) return &td->lines[found];

    if (td->line_count >= td->line_cap) {
        td->line_cap = td->line_cap ? td->line_cap * 2 : 8;
        td->lines = realloc(td->lines, (size_t)td->line_cap * sizeof(LineEntry));
    }
    int idx = td->line_count;
    LineEntry *le = &td->lines[idx];
    memset(le, 0, sizeof(*le));
    le->key  = key;
    le->type = type;
    td->line_count++;

    lk_insert(&td->line_hash, &td->line_hash_mask, &td->line_hash_fill, key, idx);
    return le;
}

static PolyEntry *td_get_or_create_poly(TileData *td, int key, int type)
{
    int found = lk_find(td->poly_hash, td->poly_hash_mask, key);
    if (found >= 0) return &td->polys[found];

    if (td->poly_count >= td->poly_cap) {
        td->poly_cap = td->poly_cap ? td->poly_cap * 2 : 8;
        td->polys = realloc(td->polys, (size_t)td->poly_cap * sizeof(PolyEntry));
    }
    int idx = td->poly_count;
    PolyEntry *pe = &td->polys[idx];
    memset(pe, 0, sizeof(*pe));
    pe->key   = key;
    pe->type  = type;
    pe->south = 10000;
    pe->east  = 10000;
    td->poly_count++;

    lk_insert(&td->poly_hash, &td->poly_hash_mask, &td->poly_hash_fill, key, idx);
    return pe;
}

static void td_free(TileData *td)
{
    free(td->pts);
    free(td->pt_hash);
    free(td->line_hash);
    free(td->poly_hash);
    for (int i = 0; i < td->line_count; i++) free(td->lines[i].pts.data);
    free(td->lines);
    for (int i = 0; i < td->poly_count; i++) free(td->polys[i].pts.data);
    free(td->polys);
}

// ============================================================================
// Lock-free open-addressing hash table — tile_id → TileData
// Grows dynamically when full — starts at 512K slots, doubles on overflow.
// Old tables are kept alive during processing, freed at cleanup.
// ============================================================================
#define HASH_BITS_INIT 19
#define HASH_CAP_INIT  (1u << HASH_BITS_INIT)

typedef struct {
    _Atomic int  tile_id;    // -1 = free, -2 = being initialized, >= 0 = live
    TileData    *data;       // heap-allocated, NULL until tile_id >= 0
} HashSlot;

static _Atomic(HashSlot *)  g_tiles;
static _Atomic uint32_t     g_hash_mask;
static _Atomic uint32_t     g_hash_cap;

static HashSlot **g_old_tables;
static int        g_old_count, g_old_cap;

static pthread_mutex_t g_resize_lock = PTHREAD_MUTEX_INITIALIZER;

static inline uint32_t hash_i32(int key)
{
    uint32_t h = 2166136261u;
    h ^= (uint32_t)key;
    h *= 16777619u;
    return h;
}

static void hash_grow(void)
{
    HashSlot *old   = atomic_load(&g_tiles);
    uint32_t  cap   = atomic_load(&g_hash_cap);
    uint32_t  new_cap = cap * 2;
    uint32_t  new_mask = new_cap - 1;

    HashSlot *new_tbl = calloc(new_cap, sizeof(HashSlot));
    if (!new_tbl) { fprintf(stderr, "OOM: hash resize\n"); exit(1); }
    for (uint32_t i = 0; i < new_cap; i++)
        atomic_init(&new_tbl[i].tile_id, -1);

    // Rehash all live entries
    for (uint32_t i = 0; i < cap; i++) {
        int tid = atomic_load(&old[i].tile_id);
        if (tid < 0) continue;
        uint32_t idx = (hash_i32(tid)) & new_mask;
        while (atomic_load(&new_tbl[idx].tile_id) >= 0)
            idx = (idx + 1) & new_mask;
        new_tbl[idx].data = old[i].data;
        atomic_store(&new_tbl[idx].tile_id, tid);
    }

    // Save old table — freed during cleanup
    if (g_old_count >= g_old_cap) {
        g_old_cap = g_old_cap ? g_old_cap * 2 : 4;
        g_old_tables = realloc(g_old_tables, (size_t)g_old_cap * sizeof(HashSlot*));
    }
    g_old_tables[g_old_count++] = old;

    atomic_store(&g_tiles, new_tbl);
    atomic_store(&g_hash_cap, new_cap);
    atomic_store(&g_hash_mask, new_mask);
}

// --- sharded data locks for per-tile operations ---
#define NUM_DATA_LOCKS 256
#define DATA_LOCK_MASK (NUM_DATA_LOCKS - 1)

static pthread_mutex_t g_data_locks[NUM_DATA_LOCKS];
static bool            g_use_locks;

#define LOCK_DATA(x)   do { if (g_use_locks) pthread_mutex_lock(&g_data_locks[(uint32_t)(x) & DATA_LOCK_MASK]); } while(0)
#define UNLOCK_DATA(x) do { if (g_use_locks) pthread_mutex_unlock(&g_data_locks[(uint32_t)(x) & DATA_LOCK_MASK]); } while(0)

static TileData *tiles_get(int tile_id)
{
    for (;;) {
        HashSlot *tbl  = atomic_load(&g_tiles);
        uint32_t  mask = atomic_load(&g_hash_mask);
        uint32_t  start = hash_i32(tile_id) & mask;
        uint32_t  idx = start;

        do {
            int cur = atomic_load(&tbl[idx].tile_id);
            if (cur == -1) {
                int expected = -1;
                if (atomic_compare_exchange_strong(&tbl[idx].tile_id, &expected, -2)) {
                    TileData *td = calloc(1, sizeof(TileData));
                    tbl[idx].data = td;
                    atomic_store(&tbl[idx].tile_id, tile_id);
                    return td;
                }
                continue;
            }
            if (cur == -2) continue;
            if (cur == tile_id)
                return tbl[idx].data;
            idx = (idx + 1) & mask;
        } while (idx != start);

        // Table full — grow under lock, then retry
        pthread_mutex_lock(&g_resize_lock);
        if (atomic_load(&g_hash_mask) == mask)  // double-check: no one else grew it
            hash_grow();
        pthread_mutex_unlock(&g_resize_lock);
    }
}

// ============================================================================
// Cohen-Sutherland line clipping
// ============================================================================
enum { CS_INSIDE = 0, CS_LEFT = 1, CS_RIGHT = 2, CS_BOTTOM = 4, CS_TOP = 8 };

static inline int cs_oc(double x, double y,
                         double xmin, double ymin,
                         double xmax, double ymax)
{
    int c = CS_INSIDE;
    if (x < xmin)      c |= CS_LEFT;
    else if (x > xmax) c |= CS_RIGHT;
    if (y < ymin)      c |= CS_BOTTOM;
    else if (y > ymax) c |= CS_TOP;
    return c;
}

static bool clip_segment(double *x0, double *y0, double *x1, double *y1,
                          double xmin, double ymin, double xmax, double ymax)
{
    int oc0 = cs_oc(*x0, *y0, xmin, ymin, xmax, ymax);
    int oc1 = cs_oc(*x1, *y1, xmin, ymin, xmax, ymax);
    for (;;) {
        if ((oc0 | oc1) == 0) return true;
        if (oc0 & oc1)        return false;
        int    oc = oc0 ? oc0 : oc1;
        double x, y;
        double dx = *x1 - *x0, dy = *y1 - *y0;
        if (oc & CS_TOP) {
            x = *x0 + dx * (ymax - *y0) / dy; y = ymax;
        } else if (oc & CS_BOTTOM) {
            x = *x0 + dx * (ymin - *y0) / dy; y = ymin;
        } else if (oc & CS_RIGHT) {
            y = *y0 + dy * (xmax - *x0) / dx; x = xmax;
        } else {
            y = *y0 + dy * (xmin - *x0) / dx; x = xmin;
        }
        if (oc == oc0) { *x0 = x; *y0 = y; oc0 = cs_oc(*x0, *y0, xmin, ymin, xmax, ymax); }
        else           { *x1 = x; *y1 = y; oc1 = cs_oc(*x1, *y1, xmin, ymin, xmax, ymax); }
    }
}

// ============================================================================
// Road type mapping
// ============================================================================
static int road_type_from_props(cJSON *props)
{
    cJSON *j  = cJSON_GetObjectItem(props, "type");
    if (j && cJSON_IsString(j)) {
        const char *s = j->valuestring;
        if (*s == 'f' && strcmp(s, "ferry") == 0) return 14;
        if (*s == 't' && strcmp(s, "train") == 0) return 12;
    }
    j = cJSON_GetObjectItem(props, "roadType");
    if (!j || !cJSON_IsString(j)) return -1;
    switch (j->valuestring[0]) {
    case 'f': if (strcmp(j->valuestring, "freeway") == 0) return 1;  break;
    case 'd': if (strcmp(j->valuestring, "divided") == 0) return 7;  break;
    case 'l': if (strcmp(j->valuestring, "local")   == 0) return 7;  break;
    case 't': if (strcmp(j->valuestring, "train")   == 0) return 12; break;
    case 'n': if (strcmp(j->valuestring, "no_vehicles") == 0) return 8; break;
    }
    return -1;
}

// ============================================================================
// Segment processing — core hot loop
// ============================================================================
static void process_segment(
    double lon0, double lat0, double lon1, double lat1,
    int feat_idx, int road_type, bool is_line, int poly_color)
{
    for (int scale = 0; scale < 2; scale++) {
        const Scale *sc = &g_scale[scale];
        double step  = sc->step;
        double recip = sc->recip;
        int    sf    = sc->scale_factor;
        int    nrows = sc->num_rows;
        int    base  = sc->base_index;

        double min_lat = lat0 < lat1 ? lat0 : lat1;
        double max_lat = lat0 > lat1 ? lat0 : lat1;
        double min_lon = lon0 < lon1 ? lon0 : lon1;
        double max_lon = lon0 > lon1 ? lon0 : lon1;

        int min_tla = (int)floor((floor(min_lat * 100.0) +  9000.0) / sf);
        int max_tla = (int)floor((floor(max_lat * 100.0) +  9000.0) / sf);
        int min_tlo = (int)floor((floor(min_lon * 100.0) + 18000.0) / sf);
        int max_tlo = (int)floor((floor(max_lon * 100.0) + 18000.0) / sf);

        for (int li = min_tlo; li <= max_tlo; li++) {
            double clon = (li * sf - 18000.0) / 100.0;
            double tmin_lon = clon;
            double tmax_lon = clon + step;

            for (int lj = min_tla; lj <= max_tla; lj++) {
                int tid = li * nrows + lj + base;

                double clat = (lj * sf - 9000.0) / 100.0;
                double tmin_lat = clat;
                double tmax_lat = clat + step;

                double cx0 = lon0, cy0 = lat0, cx1 = lon1, cy1 = lat1;
                if (!clip_segment(&cx0, &cy0, &cx1, &cy1,
                                   tmin_lon, tmin_lat, tmax_lon, tmax_lat))
                    continue;

                int px0 = (int)((cx0 - clon) * recip);
                int py0 = (int)((cy0 - clat) * recip);
                int px1 = (int)((cx1 - clon) * recip);
                int py1 = (int)((cy1 - clat) * recip);

                TileData *td = tiles_get(tid);

                LOCK_DATA(tid);

                int i0 = td_add_point(td, px0, py0);
                int i1 = td_add_point(td, px1, py1);

                if (is_line) {
                    if (i0 != i1) {
                        LineEntry *le = td_get_or_create_line(td, feat_idx, road_type);
                        iv_push(&le->pts, i0);
                        iv_push(&le->pts, i1);
                    }
                } else {
                    PolyEntry *pe = td_get_or_create_poly(td, feat_idx, poly_color);
                    iv_push(&pe->pts, i0);
                    if (i0 != i1) iv_push(&pe->pts, i1);

                    if (px0 < pe->east)  pe->east  = px0;
                    if (px0 > pe->west)  pe->west  = px0;
                    if (py0 < pe->south) pe->south = py0;
                    if (py0 > pe->north) pe->north = py0;
                    if (px1 < pe->east)  pe->east  = px1;
                    if (px1 > pe->west)  pe->west  = px1;
                    if (py1 < pe->south) pe->south = py1;
                    if (py1 > pe->north) pe->north = py1;
                }

                UNLOCK_DATA(tid);
            }
        }
    }
}

// ============================================================================
// Process a single feature — walks cJSON linked lists directly, O(N)
// ============================================================================
#define COORD_STACK 2048

static void process_feature(cJSON *feat, int feat_idx)
{
    cJSON *geom  = cJSON_GetObjectItem(feat, "geometry");
    cJSON *props = cJSON_GetObjectItem(feat, "properties");
    if (!geom || !props) return;

    cJSON *gt = cJSON_GetObjectItem(geom, "type");
    if (!gt || !cJSON_IsString(gt)) return;

    cJSON *coords = cJSON_GetObjectItem(geom, "coordinates");
    if (!coords || !cJSON_IsArray(coords)) return;

    const char *gtype = gt->valuestring;

    if (strcmp(gtype, "Polygon") == 0) {
        cJSON *ring = coords->child;  // coordinates[0] — first ring
        if (!ring) return;
        int n = cJSON_GetArraySize(ring);
        if (n < 2) return;

        int poly_color = 0;
        cJSON *col = cJSON_GetObjectItem(props, "color");
        if (col && cJSON_IsNumber(col)) poly_color = col->valueint;

        double  stack_lon[COORD_STACK], stack_lat[COORD_STACK];
        double *lons = stack_lon, *lats = stack_lat;
        if (n > COORD_STACK) {
            lons = malloc((size_t)n * sizeof(double));
            lats = malloc((size_t)n * sizeof(double));
        }

        // Walk the ring's linked list directly — avoids O(N²) GetArrayItem
        cJSON *pt = ring->child;
        for (int j = 0; j < n; j++, pt = pt->next) {
            cJSON *coord_arr = pt->child;
            lons[j] = coord_arr->valuedouble;
            lats[j] = coord_arr->next->valuedouble;
        }

        for (int j = 0; j < n - 1; j++)
            process_segment(lons[j], lats[j], lons[j+1], lats[j+1],
                            feat_idx, 0, false, poly_color);

        if (lons != stack_lon) { free(lons); free(lats); }

    } else if (strcmp(gtype, "LineString") == 0) {
        int road_type = road_type_from_props(props);
        if (road_type < 0) return;

        int n = cJSON_GetArraySize(coords);

        double  stack_lon[COORD_STACK], stack_lat[COORD_STACK];
        double *lons = stack_lon, *lats = stack_lat;
        if (n > COORD_STACK) {
            lons = malloc((size_t)n * sizeof(double));
            lats = malloc((size_t)n * sizeof(double));
        }

        cJSON *pt = coords->child;
        for (int j = 0; j < n; j++, pt = pt->next) {
            cJSON *coord_arr = pt->child;
            lons[j] = coord_arr->valuedouble;
            lats[j] = coord_arr->next->valuedouble;
        }

        for (int j = 0; j < n - 1; j++)
            process_segment(lons[j], lats[j], lons[j+1], lats[j+1],
                            feat_idx, road_type, true, 0);

        if (lons != stack_lon) { free(lons); free(lats); }
    }
}

#undef COORD_STACK

// ============================================================================
// Worker thread entry point
// ============================================================================
typedef struct {
    int          start, stride;
    cJSON      **feat_ptrs;
    int          feat_count;
    atomic_int  *progress;
} ThreadArgs;

static void *worker(void *arg)
{
    ThreadArgs *a = (ThreadArgs *)arg;
    int local_progress = 0;
    for (int fi = a->start; fi < a->feat_count; fi += a->stride) {
        process_feature(a->feat_ptrs[fi], fi);
        local_progress++;
        if (local_progress == 64) {
            atomic_fetch_add(a->progress, 64);
            local_progress = 0;
        }
    }
    if (local_progress)
        atomic_fetch_add(a->progress, local_progress);
    return NULL;
}

// ============================================================================
// JSON output
// ============================================================================
static cJSON *tiles_to_json(void)
{
    cJSON *root = cJSON_CreateObject();
    char   tkey[32];
    char   ikey[32];
    HashSlot *tbl = atomic_load(&g_tiles);
    uint32_t  cap = atomic_load(&g_hash_cap);

    for (uint32_t i = 0; i < cap; i++) {
        int tid = atomic_load(&tbl[i].tile_id);
        if (tid < 0) continue;
        TileData *td = tbl[i].data;

        snprintf(tkey, sizeof(tkey), "%d", tid);
        cJSON *jt = cJSON_CreateObject();

        cJSON *jpts = cJSON_CreateArray();
        for (int p = 0; p < td->pt_count; p++) {
            cJSON *jp = cJSON_CreateObject();
            cJSON_AddNumberToObject(jp, "x", td->pts[p].x);
            cJSON_AddNumberToObject(jp, "y", td->pts[p].y);
            cJSON_AddItemToArray(jpts, jp);
        }
        cJSON_AddItemToObject(jt, "points", jpts);

        cJSON *jlines = cJSON_CreateObject();
        for (int l = 0; l < td->line_count; l++) {
            snprintf(ikey, sizeof(ikey), "%d", td->lines[l].key);
            cJSON *jl  = cJSON_CreateObject();
            cJSON *jlp = cJSON_CreateArray();
            for (int p = 0; p < td->lines[l].pts.count; p++)
                cJSON_AddItemToArray(jlp, cJSON_CreateNumber(td->lines[l].pts.data[p]));
            cJSON_AddItemToObject(jl, "points", jlp);
            cJSON_AddNumberToObject(jl, "type", td->lines[l].type);
            cJSON_AddItemToObject(jlines, ikey, jl);
        }
        cJSON_AddItemToObject(jt, "lines", jlines);

        cJSON *jpolys = cJSON_CreateObject();
        for (int p = 0; p < td->poly_count; p++) {
            snprintf(ikey, sizeof(ikey), "%d", td->polys[p].key);
            cJSON *jp  = cJSON_CreateObject();
            cJSON *jpp = cJSON_CreateArray();
            for (int pp = 0; pp < td->polys[p].pts.count; pp++)
                cJSON_AddItemToArray(jpp, cJSON_CreateNumber(td->polys[p].pts.data[pp]));
            cJSON_AddItemToObject(jp, "points", jpp);
            cJSON_AddNumberToObject(jp, "type",  td->polys[p].type);
            cJSON_AddNumberToObject(jp, "north", td->polys[p].north);
            cJSON_AddNumberToObject(jp, "west",  td->polys[p].west);
            cJSON_AddNumberToObject(jp, "south", td->polys[p].south);
            cJSON_AddNumberToObject(jp, "east",  td->polys[p].east);
            cJSON_AddItemToObject(jpolys, ikey, jp);
        }
        cJSON_AddItemToObject(jt, "polygons", jpolys);

        cJSON_AddItemToObject(root, tkey, jt);
    }
    return root;
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char **argv)
{
    const char *in_path  = NULL;
    const char *out_path = NULL;
    int num_threads      = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-N") == 0 && i + 1 < argc) {
            num_threads = atoi(argv[++i]);
            if (num_threads < 1) num_threads = 1;
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            fprintf(stderr, "Usage: %s [-N threads] <input.geojson> [output.json]\n", argv[0]);
            return 1;
        } else if (!in_path) {
            in_path = argv[i];
        } else if (!out_path) {
            out_path = argv[i];
        } else {
            fprintf(stderr, "Too many arguments\n");
            return 1;
        }
    }

    if (!in_path) {
        fprintf(stderr, "Usage: %s [-N threads] <input.geojson> [output.json]\n", argv[0]);
        return 1;
    }
    if (!out_path) out_path = "test/test.json";

    scales_init();

    HashSlot *init_tbl = calloc(HASH_CAP_INIT, sizeof(HashSlot));
    if (!init_tbl) { fprintf(stderr, "OOM: hash table\n"); return 1; }
    for (uint32_t i = 0; i < HASH_CAP_INIT; i++)
        atomic_init(&init_tbl[i].tile_id, -1);
    atomic_init(&g_tiles, init_tbl);
    atomic_init(&g_hash_cap, HASH_CAP_INIT);
    atomic_init(&g_hash_mask, HASH_CAP_INIT - 1);

    g_use_locks = (num_threads > 1);
    if (g_use_locks) {
        for (int i = 0; i < NUM_DATA_LOCKS; i++)
            pthread_mutex_init(&g_data_locks[i], NULL);
    }

    // ---------- read input ----------
    FILE *f = fopen(in_path, "rb");
    if (!f) { perror("fopen input"); return 1; }
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)fsz + 1);
    if (!buf) { fprintf(stderr, "OOM: file buffer\n"); fclose(f); return 1; }
    if (fread(buf, 1, (size_t)fsz, f) != (size_t)fsz) {
        fprintf(stderr, "Short read\n"); free(buf); fclose(f); return 1;
    }
    buf[fsz] = '\0';
    fclose(f);

    // ---------- parse JSON ----------
    fprintf(stderr, "Parsing JSON (%ld bytes)...\n", fsz);
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        fprintf(stderr, "JSON parse error: %s\n",
                cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "unknown");
        return 1;
    }

    cJSON *features = cJSON_GetObjectItem(root, "features");
    if (!features || !cJSON_IsArray(features)) {
        fprintf(stderr, "No 'features' array\n");
        cJSON_Delete(root);
        return 1;
    }

    int feat_count = cJSON_GetArraySize(features);
    fprintf(stderr, "Processing %d features on %d thread(s)...\n",
            feat_count, num_threads);

    // ---------- build feature pointer array (O(N), avoids O(N²) GetArrayItem) ----------
    cJSON **feat_ptrs = malloc((size_t)feat_count * sizeof(cJSON*));
    if (!feat_ptrs) { fprintf(stderr, "OOM: feat_ptrs\n"); cJSON_Delete(root); return 1; }
    {
        cJSON *fnode = features->child;
        for (int i = 0; i < feat_count; i++, fnode = fnode->next)
            feat_ptrs[i] = fnode;
    }

    // ---------- process features ----------
    if (num_threads == 1) {
        int progress = 0;
        for (int fi = 0; fi < feat_count; fi++) {
            if ((++progress & 1023) == 0)
                fprintf(stderr, "\r  %d / %d", fi, feat_count);
            process_feature(feat_ptrs[fi], fi);
        }
        fprintf(stderr, "\r  %d / %d\n", feat_count, feat_count);
    } else {
        pthread_t  *threads = malloc((size_t)num_threads * sizeof(pthread_t));
        ThreadArgs *args    = malloc((size_t)num_threads * sizeof(ThreadArgs));
        atomic_int  progress = 0;

        for (int t = 0; t < num_threads; t++) {
            args[t].start      = t;
            args[t].stride     = num_threads;
            args[t].feat_ptrs  = feat_ptrs;
            args[t].feat_count = feat_count;
            args[t].progress   = &progress;
            pthread_create(&threads[t], NULL, worker, &args[t]);
        }

        while (atomic_load(&progress) < feat_count) {
            fprintf(stderr, "\r  %d / %d", atomic_load(&progress), feat_count);
            fflush(stderr);
            struct timespec ts = { .tv_sec = 0, .tv_nsec = 50000000 };
            nanosleep(&ts, NULL);
        }

        for (int t = 0; t < num_threads; t++)
            pthread_join(threads[t], NULL);

        fprintf(stderr, "\r  %d / %d\n", feat_count, feat_count);
        free(threads);
        free(args);
    }

    free(feat_ptrs);
    cJSON_Delete(root);

    // ---------- output ----------
    int tile_count = 0;
    {
        HashSlot *tbl = atomic_load(&g_tiles);
        uint32_t  cap = atomic_load(&g_hash_cap);
        for (uint32_t i = 0; i < cap; i++)
            if (atomic_load(&tbl[i].tile_id) >= 0) tile_count++;
    }
    fprintf(stderr, "Built %d tiles. Generating JSON...\n", tile_count);

    cJSON *out = tiles_to_json();
    char  *json_str = cJSON_PrintUnformatted(out);
    cJSON_Delete(out);

    char *slash = strrchr(out_path, '/');
    if (slash) {
        char dir[4096];
        size_t dlen = (size_t)(slash - out_path);
        if (dlen >= sizeof(dir)) dlen = sizeof(dir) - 1;
        memcpy(dir, out_path, dlen);
        dir[dlen] = '\0';
        mkdir(dir, 0755);
    }

    FILE *of = fopen(out_path, "w");
    if (!of) { perror("fopen output"); free(json_str); return 1; }
    fputs(json_str, of);
    fclose(of);
    fprintf(stderr, "Wrote %s (%zu bytes)\n", out_path, strlen(json_str));
    free(json_str);

    // ---------- cleanup ----------
    {
        HashSlot *tbl = atomic_load(&g_tiles);
        uint32_t  cap = atomic_load(&g_hash_cap);
        for (uint32_t i = 0; i < cap; i++)
            if (atomic_load(&tbl[i].tile_id) >= 0) {
                td_free(tbl[i].data);
                free(tbl[i].data);
            }
        free(tbl);
    }
    for (int i = 0; i < g_old_count; i++)
        free(g_old_tables[i]);
    free(g_old_tables);
    return 0;
}
