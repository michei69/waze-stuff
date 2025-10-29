#pragma magic [0x57 0x5a 0x44 0x46] @ 0x0

char magic[4] @ 0x0; // WZDF

struct Header {
    u32 endianess; // 0x01
    u32 version; // 0x03
    u32 compressed_size;
    u32 uncompressed_size;
    u8 data[compressed_size];
}; 

Header header @ 0x4;