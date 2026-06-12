#ifndef WZDF_H
#define WZDF_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Decompress a WZDF buffer. On success returns decompressed data in *out
// (caller must free) and sets *out_size. Returns 0 on success, -1 on error.
int wzdf_decompress(const uint8_t *data, size_t data_size,
                    uint8_t **out, size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif /* WZDF_H */
