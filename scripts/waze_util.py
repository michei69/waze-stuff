from waze_scales import ScaleData, MAX_SCALE
from math import floor

def id_to_latlon(tile_id: int) -> tuple[float, float]:
    tile_id = int(tile_id)
    
    scale = 0
    for i in range(1, MAX_SCALE + 1):
        if ScaleData[i]["base_index"] > tile_id:
            scale = i - 1
            break
    
    temp = tile_id - ScaleData[scale]["base_index"]
    lat_index = temp % ScaleData[scale]["num_rows"]
    lon_index = temp // ScaleData[scale]["num_rows"]

    latitude = (lat_index * ScaleData[scale]["scale_factor"] - 9000) / 100
    longitude = (lon_index * ScaleData[scale]["scale_factor"] - 18000) / 100

    return latitude, longitude

def latlon_to_id(lat: float|str, lon: float|str, scale = 0) -> int:
    lat = float(lat)
    lon = float(lon)

    lat_index = floor(lat * 100 + 9000) // ScaleData[scale]["scale_factor"]
    lon_index = floor(lon * 100 + 18000) // ScaleData[scale]["scale_factor"]

    return ScaleData[scale]["base_index"] + lon_index * ScaleData[scale]["num_rows"] + lat_index

def rescale_tile(tile_id: int, scale = 0, new_scale = 0):
    return latlon_to_id(*id_to_latlon(tile_id), new_scale)