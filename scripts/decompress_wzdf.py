import zlib

def decompress_raw(data: bytes):
    if len(data) < 20:
        raise ValueError("Not a WazeDataFile. File size too small")

    if data[:4] != b"WZDF":
        raise ValueError("Not a WazeDataFile. Did you pass a network response instead?")

    if data[4:8] != b"\x01\x00\x00\x00":
        raise ValueError("Invalid endianess. Expected 0x01, got 0x%02x" % int.from_bytes(data[4:8], "big"))

    if data[8:12] != b"\x00\x00\x03\x00":
        raise ValueError("Invalid version. Expected 0x000003, got 0x%02x" % int.from_bytes(data[8:12], "big"))

    compressed_size = int.from_bytes(data[12:16], "little")
    uncompressed_size = int.from_bytes(data[16:20], "little")

    compressed = data[20:]
    decompressed = zlib.decompress(compressed, wbits=15)

    if len(compressed) != compressed_size:
        raise ValueError("Invalid compressed size. Expected %d, got %d" % (compressed_size, len(compressed)))

    if len(decompressed) != uncompressed_size:
        raise ValueError("Invalid uncompressed size. Expected %d, got %d" % (uncompressed_size, len(decompressed)))

    return decompressed

def decompress(file_name: str):
    with open(file_name, "rb") as f:
        data = f.read()

    return decompress_raw(data)

if __name__ == "__main__":
    file_name = input("File: ")

    decompressed = decompress(file_name)

    with open(file_name.replace(".wzdf", ".wzt"), "wb") as f:
        f.write(decompressed)
    
    print("Done")