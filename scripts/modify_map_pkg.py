from split_map_pkg import split_package_raw
from decompress_wzdf import decompress_raw
from process_waze_tile import wzt_to_json
from json_to_wzt import json_to_wzt
from compress_wzt import compress_raw
from create_map_pkg import create_map_pkg_raw
from create_dummy_wzt import create_dummy_wzt, create_dummy_wzt_with_text
from waze_scales import ScaleData, MAX_SCALE
from generate_tiles import generate
from waze_tile_classes import WazeTile
from waze_util import id_to_latlon
import sys
import os
# from uuid import uuid4

file_name = sys.argv[1]

with open(file_name, "rb") as f:
    data = f.read()

tiles = split_package_raw(data)

# d = []

for tile_id in tiles:
    scale = 0

    exists = os.path.exists(f"temp/{tile_id}.wzt")
    if not exists:
        exists = generate(tile_id)
    if exists:
        print(f"Serving {tile_id} from generated")
        with open(f"temp/{tile_id}.wzt", "rb") as f:
            tiles[tile_id] = compress_raw(f.read())
    else:
        tiles[tile_id] = compress_raw(create_dummy_wzt_with_text(int(tile_id)))

    # if decompress_raw(tiles[tile_id]).rfind(b"Thomas") != -1:
    #     print(f"Thomas in {tile_id}")
    # d.append(WazeTile(decompress_raw(tiles[tile_id])))

    latitude, longitude = id_to_latlon(tile_id)

    # print(f"tile(id = {tile_id}, lat = {latitude}, lon = {longitude}, scale = {scale})")

data = create_map_pkg_raw([i for i in tiles.values()], [i for i in tiles.keys()])

with open(file_name, "wb") as f:
    f.write(data)


# with open(f"./test/{uuid4()}.txt", "w") as f:
#     f.write(str(d))