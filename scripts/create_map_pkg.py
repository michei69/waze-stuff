from pathlib import Path

def create_map_pkg_raw(files: list[bytes], tile_ids: list[str]):
    data = bytearray()
    data.extend(b"\x2e\x2e\x2e\x2e")
    data.extend(len(files).to_bytes(4, "big"))
    for i in range(len(files)):
        data.extend(int(tile_ids[i]).to_bytes(4, "big"))
        data.extend(b"\x00\x00\x00\x00")
        data.extend(len(files[i]).to_bytes(4, "big"))
        data.extend(files[i])
    return bytes(data)

def create_map_pkg(files: list[str]):
    data = [Path(file_name).read_bytes() for file_name in files]

    return create_map_pkg_raw(data, [Path(file_name).name.split(".")[0] for file_name in files])

if __name__ == "__main__":
    files = []
    while True:
        file_name = input("File: ")
        if file_name.strip() == "":
            break
        files.append(file_name)

    print("Packaging the following files:\n- " + "\n- ".join(files))

    data = create_map_pkg(files)

    with open("data.map_pkg", "wb") as f:
        f.write(data)
    
    print("Done")