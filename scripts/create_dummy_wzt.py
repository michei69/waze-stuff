from pathlib import Path
from create_wzt import create_wzt
def create_dummy_wzt(tile_id: int):
    create_wzt(str(tile_id))
    with open(Path(__file__).parent.joinpath("../temp", f"{tile_id}.wzt"), "rb") as f:
        data = f.read()
    # data = data.replace((366799430).to_bytes(4, "little"), tile_id.to_bytes(4, "little"))
    return data

def create_dummy_wzt_with_text(tile_id: int):
    data = create_dummy_wzt(tile_id)
    return data

if __name__ == "__main__":
    tile_id = int(input("Tile ID: "))

    with open(f"{tile_id}.wzt", "wb") as f:
        f.write(create_dummy_wzt(tile_id))