import json
import base64

# unused
def json_to_wzt(data):
    data_block = bytearray()

    offsets = []

    data_block.extend(b'\0'.join([i.encode() for i in data["str0"] if i]) + b'\0')
    offsets.append(len(data_block))

    data_block.extend(b'\0'.join([i.encode() for i in data["str1"] if i]) + b'\0')
    offsets.append(len(data_block))

    data_block.extend(b'\0'.join([i.encode() for i in data["str2"] if i]) + b'\0')
    offsets.append(len(data_block))

    data_block.extend(b'\0'.join([i.encode() for i in data["str3"] if i]) + b'\0')
    offsets.append(len(data_block))

    data_block.extend(b'\0'.join([i.encode() for i in data["str4"] if i]) + b'\0')
    offsets.append(len(data_block))

    data_block.extend(b'\0'.join([i.encode() for i in data["str5"] if i]) + b'\0')
    offsets.append(len(data_block))

    data_block.extend(b'\0'.join([i.encode() for i in data["str6"] if i]) + b'\0')
    offsets.append(len(data_block))

    data_block.extend(b'\0'.join([i.encode() for i in data["str7"] if i]) + b'\0')
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["shape"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["line_data"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["line_summary"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["broken"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["line_roundabout"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["point_data"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["point_ids"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["line_routes"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["street_names"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["street_cities"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["polygon_heads"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["polygon_points"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["line_ref"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["line_speed_average"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["line_speed_index"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["line_speed_index_small"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["range"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["alert_data"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["square_data"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["metadata_attributes"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["venue_heads"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["venue_ids"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["line_ext_types"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["line_ext_ids"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["line_ext_level_by_line"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["line_ext_levels"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["line_speed_max"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["polygon_ex"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["line_attributes"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["street_ids"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["beacon_pos"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["beacon_ids"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["beacon_ex_pos"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["beacon_ex_ids"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["beacon_ex_masks"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["lane_types"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["line_new_types"]))
    offsets.append(len(data_block))

    data_block.extend(base64.b64decode(data["extension_protobuf_data"]))
    offsets.append(len(data_block))

    raw_data = bytearray()
    raw_data.extend(len(offsets).to_bytes(4, "little"))
    raw_data.extend(0x2.to_bytes(4, "little"))
    for offset in offsets:
        raw_data.extend(offset.to_bytes(4, "little"))
    raw_data.extend(data_block)
    return bytes(raw_data)

if __name__ == "__main__":
    file_name = input("File: ")
    with open(file_name, "r") as f:
        data = json.load(f)

    raw_data = json_to_wzt(data)

    with open(file_name.replace(".json", ".wzt"), "wb") as f:
        f.write(raw_data)