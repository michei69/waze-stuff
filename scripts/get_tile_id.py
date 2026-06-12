from waze_util import latlon_to_id

lat = input("lat: ")
lon = input("lon: ")
print(latlon_to_id(float(lat), float(lon)))