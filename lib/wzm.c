#include "wzm.h"
#include "waze_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>  /* ntohl */

WzmTile *wzm_split(const uint8_t *data, size_t data_size,
                   uint32_t *tile_count)
{
    if (!data || !tile_count) return NULL;
    *tile_count = 0;

    if (data_size < 20) {
        fprintf(stderr, "wzm: file too small (%zu bytes)\n", data_size);
        return NULL;
    }

    /* Magic: "...." = 0x2E 0x2E 0x2E 0x2E */
    uint32_t magic;
    memcpy(&magic, data, 4);
    if (magic != WZM_MAGIC) {
        fprintf(stderr, "wzm: invalid magic 0x%08X (expected 0x%08X)\n",
                magic, WZM_MAGIC);
        return NULL;
    }

    /* Number of tiles: big-endian u32 */
    uint32_t magic_be;
    memcpy(&magic_be, data + 4, 4);
    uint32_t num_tiles = ntohl(magic_be);

    printf("wzm: %u tile(s) in package\n", num_tiles);

    WzmTile *tiles = (WzmTile *)calloc(num_tiles, sizeof(WzmTile));
    if (!tiles) {
        fprintf(stderr, "wzm: calloc(%u) failed\n", num_tiles);
        return NULL;
    }

    const uint8_t *ptr = data + 8;
    size_t remaining = data_size - 8;

    for (uint32_t i = 0; i < num_tiles; i++) {
        /* Tile entry: tile_id(4) + padding(4) + raw_tile_size(4) + data */
        if (remaining < 12) {
            fprintf(stderr, "wzm: truncated tile %u header (%zu bytes left)\n",
                    i, remaining);
            goto error;
        }

        uint32_t tile_id_be, raw_tile_size_be;
        memcpy(&tile_id_be, ptr, 4);
        memcpy(&raw_tile_size_be, ptr + 8, 4);

        tiles[i].tile_id   = ntohl(tile_id_be);
        tiles[i].data_size = ntohl(raw_tile_size_be);

        ptr += 12;       /* skip tile_id + padding + raw_tile_size */
        remaining -= 12;

        if (remaining < tiles[i].data_size) {
            fprintf(stderr, "wzm: tile %u (id=%u) data truncated "
                    "(need=%zu, have=%zu)\n",
                    i, tiles[i].tile_id, tiles[i].data_size, remaining);
            goto error;
        }

        tiles[i].data = (uint8_t *)malloc(tiles[i].data_size);
        if (!tiles[i].data) {
            fprintf(stderr, "wzm: malloc(%zu) for tile %u failed\n",
                    tiles[i].data_size, i);
            goto error;
        }

        memcpy(tiles[i].data, ptr, tiles[i].data_size);
        ptr += tiles[i].data_size;
        remaining -= tiles[i].data_size;
    }

    *tile_count = num_tiles;
    return tiles;

error:
    wzm_free_tiles(tiles, num_tiles);
    return NULL;
}

void wzm_free_tiles(WzmTile *tiles, uint32_t count)
{
    if (!tiles) return;
    for (uint32_t i = 0; i < count; i++) {
        free(tiles[i].data);
    }
    free(tiles);
}
