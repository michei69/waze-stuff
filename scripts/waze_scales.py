# https://github.com/mkoloberdin/waze/blob/e199da023302b6475c4660e6a6e4eb386c511572/roadmap_tile.c
SCALE_STEP = 4
SQUARE_SIZE = 10_000
MAX_SQUARE_SIZE = 30_000_000
_size = SQUARE_SIZE
_scale = 0
while _size <= MAX_SQUARE_SIZE:
    _scale += 1
    _size *= SCALE_STEP
MAX_SCALE = _scale - 1
ScaleData: list = [None] * (MAX_SCALE + 1)

_base = 0
_factor = 1

for _scale in range(MAX_SCALE + 1):
    ScaleData[_scale] = {
        "scale_factor": _factor,
        "tile_size": _factor * SQUARE_SIZE,
        "base_index": _base,
        "num_rows": 179999999 // (_factor * SQUARE_SIZE) + 1,
        "num_cols": 359999999 // (_factor * SQUARE_SIZE) + 1
    }
    _base += ScaleData[_scale]["num_rows"] * ScaleData[_scale]["num_cols"]
    _factor *= SCALE_STEP
    # print(_scale, ScaleData[_scale])