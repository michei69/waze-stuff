import zlib

def compress_raw(data: bytes):
    uncompressed_size = len(data)
    compressed = zlib.compress(data, wbits=15)
    compressed_size = len(compressed)

    raw_data = bytearray()
    raw_data.extend(b"WZDF")
    raw_data.extend(b"\x01\x00\x00\x00")
    raw_data.extend(b"\x00\x00\x03\x00")
    raw_data.extend(compressed_size.to_bytes(4, "little"))
    raw_data.extend(uncompressed_size.to_bytes(4, "little"))
    raw_data.extend(compressed)

    return raw_data

def compress(file_name: str):
    with open(file_name, "rb") as f:
        data = f.read()
    
    return compress_raw(data)

if __name__ == "__main__":
    file_name = input("File: ")

    raw_data = compress(file_name)

    with open(file_name.replace(".wzt", ".wzdf"), "wb") as f:
        f.write(raw_data)

    print("Done")