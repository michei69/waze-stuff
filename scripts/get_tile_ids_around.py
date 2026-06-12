from waze_util import id_to_latlon, latlon_to_id

while True:
    tile_id = int(input("tile id: "))
    lat, lon = id_to_latlon(tile_id)
    print(latlon_to_id(lat-0.01, lon))
    print(latlon_to_id(lat, lon-0.01))
    print(latlon_to_id(lat+0.01, lon))
    print(latlon_to_id(lat, lon+0.01))