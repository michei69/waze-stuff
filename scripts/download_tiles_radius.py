from waze_util import id_to_latlon, latlon_to_id

start_tile_id = 366871434
lat, lon = id_to_latlon(start_tile_id)
print(lat,lon)

tiles = set([start_tile_id])
R = 4                      # radius in steps of 0.01 degrees
step = 0.01

for dx in range(-R, R+1):
    for dy in range(-R, R+1):
        if dx == 0 and dy == 0:   # skip center (optional)
            continue
        tiles.add(latlon_to_id(lat + dx * step, lon + dy * step))

url = f"https://ctilesgcs-row.waze.com/TileServer/multi-get?reqtype=tileBatch&protocol=2&sessionid=1803914411&cookie=ChAydlVBOGxESGRFeDhZWWpuEIyUq99EGGOeb39aACIgV3b3JsZCirmZbcBjCdv999aoCDogN5tEHKzlFETl2REoRx29bSyRw99BwW843YsZI9aued9au9ag&num={len(tiles)}&variation=PARTIAL_SIMPLIFICATION"
i = 0
for tid in tiles:
    url += f"&t{i}={tid}&v{i}=0&p{i}=42"
    i+=1

print(url)