#include "wzdf.h"
#include "waze_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

int wzdf_decompress(const uint8_t *data, size_t data_size,
                    uint8_t **out, size_t *out_size)
{
    if (!data || !out || !out_size) return -1;
    *out = NULL;
    *out_size = 0;

    if (data_size < 20) {
        fprintf(stderr, "wzdf: file too small (%zu bytes, need >= 20)\n", data_size);
        return -1;
    }

    const WzdfHeader *hdr = (const WzdfHeader *)data;
    if (hdr->magic != WZDF_MAGIC) {
        fprintf(stderr, "wzdf: invalid magic 0x%08X (expected 0x%08X)\n", hdr->magic, WZDF_MAGIC);
        return -1;
    }
    if (hdr->endianess != WZDF_ENDIANESS) {
        fprintf(stderr, "wzdf: invalid endianess 0x%08X (expected 0x%08X)\n", hdr->endianess, WZDF_ENDIANESS);
        return -1;
    }
    if (hdr->version != WZDF_VERSION) {
        fprintf(stderr, "wzdf: unknown version 0x%08X (expected 0x%08X)\n", hdr->version, WZDF_VERSION);
        return -1;
    }

    size_t compressed_size = hdr->compressed_size;
    size_t uncompressed_size = hdr->uncompressed_size;

    if (compressed_size + 20 > data_size) {
        fprintf(stderr, "wzdf: truncated data (compressed_size=%zu, available=%zu)\n", compressed_size, data_size - 20);
        return -1;
    }

    const uint8_t *compressed = data + 20;

    /* Validate compressed size */
    size_t actual_compressed = data_size - 20;
    if (actual_compressed < compressed_size) {
        fprintf(stderr, "wzdf: warning: compressed_size=%zu but only %zu bytes available\n", compressed_size, actual_compressed);
        compressed_size = actual_compressed;
    }

    uint8_t *decompressed = (uint8_t *)malloc(uncompressed_size);
    if (!decompressed) {
        fprintf(stderr, "wzdf: malloc(%zu) failed\n", uncompressed_size);
        return -1;
    }

    z_stream strm = {0};
    strm.next_in = (Bytef *)compressed;
    strm.avail_in = compressed_size;
    strm.next_out = decompressed;
    strm.avail_out = uncompressed_size;

    /* wbits=15 → raw deflate, 15|32 → auto-detect zlib/gzip */
    int ret = inflateInit2(&strm, 15);
    if (ret != Z_OK) {
        fprintf(stderr, "wzdf: inflateInit2 failed: %d\n", ret);
        free(decompressed);
        return -1;
    }

    ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);

    if (ret != Z_STREAM_END) {
        fprintf(stderr, "wzdf: decompression failed (ret=%d, msg=%s)\n", ret, strm.msg ? strm.msg : "unknown");
        free(decompressed);
        return -1;
    }

    size_t actual_uncompressed = strm.total_out;
    if (actual_uncompressed != uncompressed_size) {
        fprintf(stderr, "wzdf: uncompressed size mismatch (expected=%zu, got=%zu)\n", uncompressed_size, actual_uncompressed);
        /* Continue anyway, but warn */
    }

    *out = decompressed;
    *out_size = actual_uncompressed;
    return 0;
}
