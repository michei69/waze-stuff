from pathlib import Path

def split_package_raw(data: bytes):
    if len(data) < 20:
        raise ValueError("Not a WazeMapPackage. File size too small")

    if data[:4] != b"\x2e\x2e\x2e\x2e":
        raise ValueError("Not a WazeMapPackage. Invalid signature")

    number_of_tiles = int.from_bytes(data[4:8], "big")
    print(f"Map package contains {number_of_tiles} tiles inside.")


    current_data = data[8:]
    tiles = {}
    for i in range(number_of_tiles):
        tile_id = int.from_bytes(current_data[:4], "big")
        current_data = current_data[8:] # skip padding
        raw_tile_size = int.from_bytes(current_data[:4], "big")
        raw_data = current_data[4:4 + raw_tile_size]
        current_data = current_data[4 + raw_tile_size:]

        tiles[str(tile_id)] = raw_data

    return tiles

def split_package(file_name: str):
    with open(file_name, "rb") as f:
        data = f.read()

    return split_package_raw(data)


if __name__ == "__main__":
    file_name = input("File: ")
    
    same_dir = str(Path(file_name).parent)
    tiles = split_package(file_name)
    for id in tiles:
        with open(Path(same_dir).joinpath(f"{id}.wzdf"), "wb") as f:
            f.write(tiles[id])

    print("Done")