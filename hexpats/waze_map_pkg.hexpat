#pragma magic [ 0x2e 0x2e 0x2e 0x2e ] @ 0x0

char magic[4] @ 0x0;

struct Tile {
    be u32 tile_id;
    padding[4];
    be u32 raw_tile_size;
    char tile_magic[4];
    u32 endianess;
    u32 tile_version;
    u32 compressed_size;
    u32 uncompressed_size;
    char compressed_data[compressed_size];
};

struct MapPkg {
    be u32 number_of_tiles;
    Tile tiles[number_of_tiles];
};

MapPkg pkg @ 0x4;