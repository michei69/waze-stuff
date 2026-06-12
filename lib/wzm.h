#ifndef WZM_H
#define WZM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A single tile extracted from a map package. */
typedef struct {
    uint32_t tile_id;
    uint8_t *data;       /* raw WZDF tile data (caller frees) */
    size_t   data_size;
} WzmTile;

// Split a WZM buffer into individual tiles. Returns array of WzmTile.
// *tile_count is set to number of tiles. Caller must free each tile's .data
// and the returned array. Returns NULL on error. */
WzmTile *wzm_split(const uint8_t *data, size_t data_size, uint32_t *tile_count);

/* Free tiles array. */
void wzm_free_tiles(WzmTile *tiles, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* WZM_H */
