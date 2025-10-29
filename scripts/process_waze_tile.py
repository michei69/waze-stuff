from waze_tile_classes import *
import base64
import json

def wzt_to_json(data: bytes):
    tile = WazeTile(data)

    # tile_data = {
    #     "entry_count": tile.entry_count,
    #     "mask_bits": tile.mask_bits,
    #     "offsets": tile.offsets,
    #     "line_data": base64.b64encode(tile.raw_line_data).decode('utf-8'),
    #     "line_summary": base64.b64encode(tile.raw_line_summary).decode('utf-8'),
    #     "broken": base64.b64encode(tile.raw_broken).decode('utf-8'),
    #     "point_data": base64.b64encode(tile.raw_point_data).decode('utf-8'),
    #     "point_ids": base64.b64encode(tile.raw_point_ids).decode('utf-8'),
    #     "shape": base64.b64encode(tile.raw_shapes).decode('utf-8'),
    #     "str0": tile.str0,
    #     "str1": tile.str1,
    #     "str2": tile.str2,
    #     "str3": tile.str3,
    #     "str4": tile.str4,
    #     "str5": tile.str5,
    #     "str6": tile.str6,
    #     "str7": tile.str7,
    #     "line_routes": base64.b64encode(tile.raw_line_routes).decode('utf-8'),
    #     "street_names": base64.b64encode(tile.raw_street_names).decode('utf-8'),
    #     "street_cities": base64.b64encode(tile.raw_street_cities).decode('utf-8'),
    #     "street_ids": base64.b64encode(tile.raw_street_ids).decode('utf-8'),
    #     "polygon_heads": base64.b64encode(tile.raw_polygon_heads).decode('utf-8'),
    #     "polygon_points": base64.b64encode(tile.raw_polygon_points).decode('utf-8'),
    #     "line_speed_average": base64.b64encode(tile.raw_line_speed_average).decode('utf-8'),
    #     "line_ref": base64.b64encode(tile.raw_line_ref).decode('utf-8'),
    #     "range": base64.b64encode(tile.raw_ranges).decode('utf-8'),
    #     "alert_data": base64.b64encode(tile.raw_alert_data).decode('utf-8'),
    #     "square_data": base64.b64encode(tile.raw_square_data).decode('utf-8'),
    #     "metadata_attributes": base64.b64encode(tile.raw_metadata_attributes).decode('utf-8'),
    #     "venue_heads": base64.b64encode(tile.raw_venue_heads).decode('utf-8'),
    #     "venue_ids": base64.b64encode(tile.raw_venue_ids).decode('utf-8'),
    #     "line_ext_types": base64.b64encode(tile.raw_line_ext_types).decode('utf-8'),
    #     "line_ext_ids": base64.b64encode(tile.raw_line_ext_ids).decode('utf-8'),
    #     "line_ext_level_by_line": base64.b64encode(tile.raw_line_ext_level_by_line).decode('utf-8'),
    #     "line_ext_levels": base64.b64encode(tile.raw_line_ext_levels).decode('utf-8'),
    #     "line_speed_max": base64.b64encode(tile.raw_line_speed_max).decode('utf-8'),
    #     "line_attributes": base64.b64encode(tile.raw_line_attributes).decode('utf-8'),
    #     "lane_types": base64.b64encode(tile.raw_lane_types).decode('utf-8'),
    #     "line_new_types": base64.b64encode(tile.raw_line_new_types).decode('utf-8'),
    #     "polygon_ex": base64.b64encode(tile.raw_polygon_ex).decode('utf-8'),
    #     "beacon_pos": base64.b64encode(tile.raw_beacon_pos).decode('utf-8'),
    #     "beacon_ids": base64.b64encode(tile.raw_beacon_ids).decode('utf-8'),
    #     "beacon_ex_pos": base64.b64encode(tile.raw_beacon_ex_pos).decode('utf-8'),
    #     "beacon_ex_ids": base64.b64encode(tile.raw_beacon_ex_ids).decode('utf-8'),
    #     "beacon_ex_masks": base64.b64encode(tile.raw_beacon_ex_masks).decode('utf-8'),
    #     "extension_protobuf_data": base64.b64encode(tile.raw_extension_protobuf_data).decode('utf-8'),
    #     "line_speed_index": base64.b64encode(tile.raw_line_speed_index).decode('utf-8'),
    #     "line_speed_index_small": base64.b64encode(tile.raw_line_speed_index_small).decode('utf-8'),
    #     "line_roundabout": base64.b64encode(tile.raw_line_roundabout).decode('utf-8'),
    #     # "parsed_lines": parsed_lines
    # }

    # print(tile)
    # print(tile.polygons[6])
    for line in tile.lines:
        if line.street.name == "A2205 - Bermondsey St":
            print(line)

    return json.dumps({}, indent=4)

if __name__ == "__main__":
    file_name = input("File: ")

    with open(file_name, "rb") as f:
        data = f.read()

    with open("test.json", "w") as f:
        json.dump(wzt_to_json(data), f, indent=4)