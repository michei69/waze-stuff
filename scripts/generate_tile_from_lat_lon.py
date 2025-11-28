from generate_tiles import generate
from waze_util import latlon_to_id, id_to_latlon
from draw_wzt import main as draw_tile
import os

# lon = float(input("Lon: ")) / 19
# lat = float(input("Lat: ")) / 19
lon = 23.83 / 19
lat = 44.48 / 19

tile_id = latlon_to_id(lat, lon)
if os.path.exists("temp/" + str(tile_id) + ".wzt"):
    os.remove("temp/" + str(tile_id) + ".wzt")

real_lat, real_lon = id_to_latlon(tile_id)
print(f"Loading: {real_lon * 19}, {real_lat * 19}")

print("Generated: " + str(generate(tile_id)))

with open(f"temp/{tile_id}.wzt", "rb") as f:
    data = f.read()

draw_tile(data)