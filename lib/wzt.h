#ifndef WZT_H
#define WZT_H

#include "waze_types.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Parse a raw WZT buffer into a WazeTile. Returns NULL on error.
// The returned tile must be freed with wzt_free().
WazeTile *wzt_parse(const uint8_t *data, size_t data_size);

/* Free a WazeTile and all its allocated members. */
void wzt_free(WazeTile *tile);

/* Count null-terminated strings in a raw string section buffer. */
uint32_t wzt_count_strings(const uint8_t *data, uint32_t size);

/* Convert tile_id to lat/lon in degrees.  Matches waze_tile_classes.py. */
void wzt_tile_id_to_lat_lon(int32_t tile_id, double *lat, double *lon);

// Convert lat/lon in degrees to tile_id at given scale.
int32_t wzt_lat_lon_to_tile_id(double lat, double lon, int32_t scale);

#ifdef __cplusplus
}
#endif

#endif /* WZT_H */
