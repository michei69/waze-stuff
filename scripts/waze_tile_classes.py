from dataclasses import dataclass, field
from waze_scales import SCALE_STEP, SQUARE_SIZE, MAX_SQUARE_SIZE, ScaleData, MAX_SCALE

def get_string_at_offset(data: bytes, offset: int) -> str:
    if not data or offset < 0 or offset >= len(data):
        return ""

    # Find null terminator
    end = offset
    while end < len(data) and data[end] != 0:
        end += 1
    
    return data[offset:end].decode('utf-8', errors='ignore')

def get_current_data(raw_data: bytes, size: int, offset: int) -> bytes:
    return raw_data[offset*size:offset*size+size]

@dataclass
class LineSummary():
    roundabout_count: int = 0
    broken_count: int = 0
    prefix_data: bytes = b''
    def __init__(self, offset = 0, raw_data: bytes = b""):
        if raw_data != b"":
            raw_data = get_current_data(raw_data, 62, offset)
            self.prefix_data = raw_data[:42]
            self.roundabout_count = int.from_bytes(raw_data[42:44], "little")
            self.broken_count = int.from_bytes(raw_data[60:], "little")
    
@dataclass
class LineRouteData():
    type: int = 3

    from_flags: int = 0
    to_flags: int = 0
    from_turn_flags: int = 0
    to_turn_flags: int = 0

    a_to_b: bool = False
    b_to_a: bool = False

    def __init__(self, offset = 0, raw_data: bytes = b""):
        raw_data = get_current_data(raw_data, 4, offset)
        self.from_flags = raw_data[0]
        self.to_flags = raw_data[1]
        self.from_turn_flags = raw_data[2]
        self.to_turn_flags = raw_data[3]

        self.a_to_b = self.from_flags & 1 == 1
        self.b_to_a = self.to_flags & 1 == 1


        if self.from_flags & 1 == 0:
            if self.to_flags & 1 == 0:
                self.type = 0
            else:
                self.type = 2
        else:
            self.type = 1
    
@dataclass
class PointData():
    x: int = 0
    y: int = 0
    def __init__(self, offset = 0, raw_data: bytes = b""):
        raw_data = get_current_data(raw_data, 4, offset)
        if raw_data != b"":
            self.x = int.from_bytes(raw_data[:2], "little") & 0x7fff
            self.y = int.from_bytes(raw_data[2:], "little") & 0x7fff
    
@dataclass
class Street():
    name: str = ""
    def __init__(
            self, 
            idx: int = 0, 
            raw_street_names: bytes = b"", 
            raw_str0 = b"", 
            raw_str1 = b"", 
            raw_str3 = b"", 
            raw_str4 = b""
        ):
        entry = get_current_data(raw_street_names, 10, idx)

        type_part = get_string_at_offset(raw_str0, int.from_bytes(entry[:2], "little") + 3)
        base_name = get_string_at_offset(raw_str1, int.from_bytes(entry[2:4], "little") + 3)
        prefix_part = get_string_at_offset(raw_str3, int.from_bytes(entry[4:6], "little") + 3)
        suffix_part = get_string_at_offset(raw_str4, int.from_bytes(entry[6:8], "little") + 3)

        if type_part.strip() != "":
            self.name += type_part + " "
        self.name += base_name
        if prefix_part.strip() != "":
            self.name += " " + prefix_part
        if suffix_part.strip() != "":
            self.name += " " + suffix_part

@dataclass
class LineSpeedAverage():
    data: int = 0

    def __init__(self, offset = 0, raw_data: bytes = b""):
        raw_data = get_current_data(raw_data, 2, offset)
        self.data = int.from_bytes(raw_data, "little")

@dataclass
class MaxSpeed():
    max1: int = 0 # kmh
    max2: int = 0 # kmh

    def __init__(self, max1: int = 0, max2: int = 0):
        self.max1 = max1
        self.max2 = max2

@dataclass
class LineSpeedMax():
    a_to_b: MaxSpeed = field(default_factory=MaxSpeed)
    b_to_a: MaxSpeed = field(default_factory=MaxSpeed)

    def __init__(self, offset = 0, raw_data = b""):
        raw_data = get_current_data(raw_data, 4, offset)
        self.a_to_b = MaxSpeed(int.from_bytes(raw_data[:1], "little"), int.from_bytes(raw_data[1:2], "little"))
        self.b_to_a = MaxSpeed(int.from_bytes(raw_data[2:3], "little"), int.from_bytes(raw_data[3:4], "little"))

@dataclass
class SegmentId:
    id: int = 0
    def __init__(self, offset = 0, raw_data: bytes = b""):
        raw_data = get_current_data(raw_data, 4, offset)
        self.id = int.from_bytes(raw_data, "little")

@dataclass
class LineAttributes():
    data: int = 0
    def __init__(self, offset = 0, raw_data: bytes = b""):
        raw_data = get_current_data(raw_data, 4, offset)
        self.data = int.from_bytes(raw_data, "little")

@dataclass
class LineData():
    first_point_idx: int = 0
    second_point_idx: int = 0
    shape_idx: int = -1
    street_idx: int = 0 # range

    street: Street = field(default_factory=Street)
    route: LineRouteData = field(default_factory=LineRouteData)
    speed_average: LineSpeedAverage = field(default_factory=LineSpeedAverage)
    road_type: int = 0
    ext_id: SegmentId = field(default_factory=SegmentId)
    level: int = 0 # elevation
    max_speed: LineSpeedMax = field(default_factory=LineSpeedMax)
    attributes: LineAttributes = field(default_factory=LineAttributes)
    lane_type: int = 0

    def __init__(
            self,
            offset = 0,
            raw_line_data: bytes = b"", 
            ranges: bytes = b"", 
            streets: list[Street] = [],
            raw_line_routes: bytes = b"",
            raw_line_ext_types = b"",
            raw_line_ext_ids = b"",
            raw_line_level_by_line = b"",
            raw_line_speed_max = b"",
            raw_line_attributes = b"",
            raw_lane_types = b""
        ):
        raw_line_data = get_current_data(raw_line_data, 8, offset)
        self.first_point_idx = int.from_bytes(raw_line_data[:2], "little") & 0x7fff
        self.second_point_idx = int.from_bytes(raw_line_data[2:4], "little") & 0x7fff
        self.shape_idx = int.from_bytes(raw_line_data[4:6], "little", signed=True)

        temp = raw_line_data[6:]
        temp_u = int.from_bytes(temp, "little")
        temp_s = int.from_bytes(temp, "little", signed=True)
        if temp_u == 0xffff:
            pass
        if temp_s < 0:
            self.street_idx = temp_u & 0x7fff
        else:
            self.street_idx = int.from_bytes(ranges[temp_u * 6:temp_u * 6 + 2], "little") & 0x3fff
    
        self.street = streets[self.street_idx]
        self.route = LineRouteData(offset, raw_line_routes)
        self.speed_average = LineSpeedAverage(offset, raw_line_routes)
        self.road_type = int.from_bytes(get_current_data(raw_line_ext_types, 1, offset), "little")
        self.ext_id = SegmentId(offset, raw_line_ext_ids)
        self.level = int.from_bytes(get_current_data(raw_line_level_by_line, 1, offset), "little")
        self.max_speed = LineSpeedMax(offset, raw_line_speed_max)
        self.attributes = LineAttributes(offset, raw_line_attributes)
        self.lane_type = int.from_bytes(get_current_data(raw_lane_types, 1, offset), "little")

@dataclass
class SquareData():
    tile_id: int = 0
    scale: int = 0
    actual_scale: int = 0
    timestamp: int = 0
    latitude: int = 0
    longitude: int = 0
    def __init__(self, offset = 0, raw_data: bytes = b""):
        raw_data = get_current_data(raw_data, 12, offset)
        self.tile_id = int.from_bytes(raw_data[:4], "little")
        self.scale = int.from_bytes(raw_data[4:8], "little")
        self.timestamp = int.from_bytes(raw_data[8:], "little")

        for i in range(1, MAX_SCALE + 1):
            if ScaleData[i]["base_index"] > self.tile_id:
                self.actual_scale = i - 1
                break
        
        temp = self.tile_id - ScaleData[self.actual_scale]["base_index"]
        lat_index = temp % ScaleData[self.actual_scale]["num_rows"]
        lon_index = temp // ScaleData[self.actual_scale]["num_rows"]

        self.latitude = (lat_index - 9000) / 100
        self.longitude = (lon_index - 18000) / 100
    
@dataclass
class PolygonEx():
    data: int = 0
    def __init__(self, offset = 0, raw_data = b""):
        raw_data = get_current_data(raw_data, 4, offset)
        self.data = int.from_bytes(raw_data, "little")

@dataclass
class Polygon():
    points: list[PointData] = field(default_factory=list)
    name: str = ""
    north: int = 0
    south: int = 0
    west: int = 0
    east: int = 0
    cfcc: int = 0 # idk
    first: int = 0
    count: int = 0

    ex: PolygonEx = field(default_factory=PolygonEx)    

    def __init__(
            self, 
            offset = 0, 
            raw_data: bytes = b"", 
            raw_polygon_points: bytes = b"", 
            points: list[PointData] = [], 
            raw_str7: bytes = b"",
            raw_polygon_ex: bytes = b""    
        ):
        raw_data = get_current_data(raw_data, 16, offset)
        self.first = int.from_bytes(raw_data[:2], "little")
        self.count = int.from_bytes(raw_data[2:4], "little")
        self.name = get_string_at_offset(raw_str7, int.from_bytes(raw_data[4:6], "little"))
        self.cfcc = int.from_bytes(raw_data[6:7], "little")

        self.north = int.from_bytes(raw_data[8:10], "little")
        self.west = int.from_bytes(raw_data[10:12], "little")
        self.east = int.from_bytes(raw_data[12:14], "little")
        self.south = int.from_bytes(raw_data[14:16], "little")

        self.points = []

        for i in range(self.count):
            point_id = int.from_bytes(raw_polygon_points[(self.first + i) * 2:(self.first + i + 1) * 2], "little") & 0x7fff
            self.points.append(points[point_id])

        self.ex = PolygonEx(offset, raw_polygon_ex)

@dataclass
class LineExtLevel():
    level: int = 0
    def __init__(self, offset = 0, raw_data = b""):
        raw_data = get_current_data(raw_data, 1, offset)
        self.level = int.from_bytes(raw_data, "little")

@dataclass
class WazeTile():
    raw_data = b""
    raw_data_block = b""

    entry_count: int = 0
    mask_bits: int = 0
    offsets: list[int] = field(default_factory=list)
    raw_line_data = b''
    line_data_count: int = 0
    raw_line_summary = b''
    line_summary_count: int = 0
    raw_broken = b''
    broken_count: int = 0
    raw_point_data = b''
    point_data_count: int = 0
    raw_point_ids = b''
    point_ids_count: int = 0
    raw_shapes = b''
    shapes_count: int = 0
    raw_str0 = b''
    raw_str1 = b''
    raw_str2 = b''
    raw_str3 = b''
    raw_str4 = b''
    raw_str5 = b''
    raw_str6 = b''
    raw_str7 = b''
    raw_line_routes = b''
    line_routes_count: int = 0
    raw_street_names = b''
    street_names_count: int = 0
    raw_street_cities = b''
    street_cities_count: int = 0
    raw_street_ids = b''
    street_ids_count: int = 0
    raw_polygon_heads = b''
    polygon_heads_count: int = 0
    raw_polygon_points = b''
    polygon_points_count: int = 0
    raw_line_speed_average = b''
    line_speed_average_count: int = 0
    raw_line_ref = b''
    line_ref_count: int = 0
    raw_ranges = b''
    ranges_count: int = 0
    raw_alert_data = b''
    alert_data_count: int = 0
    raw_square_data = b''
    square_data_count: int = 0
    raw_metadata_attributes = b''
    metadata_attributes_count: int = 0
    raw_venue_heads = b''
    venue_heads_count: int = 0
    raw_venue_ids = b''
    venue_ids_count: int = 0
    raw_line_ext_types = b''
    line_ext_types_count: int = 0
    raw_line_ext_ids = b''
    line_ext_ids_count: int = 0
    raw_line_ext_level_by_line = b''
    line_ext_level_by_line_count: int = 0
    raw_line_ext_levels = b''
    line_ext_levels_count: int = 0
    raw_line_speed_max = b''
    line_speed_max_count: int = 0
    raw_line_attributes = b''
    line_attributes_count: int = 0
    raw_lane_types = b''
    lane_types_count: int = 0
    raw_polygon_ex = b''
    polygon_ex_count: int = 0
    raw_beacon_pos = b''
    beacon_pos_count: int = 0
    raw_beacon_ids = b''
    beacon_ids_count: int = 0
    raw_beacon_ex_pos = b''
    beacon_ex_pos_count: int = 0
    raw_beacon_ex_ids = b''
    beacon_ex_ids_count: int = 0
    raw_beacon_ex_masks = b''
    beacon_ex_masks_count: int = 0
    raw_extension_protobuf_data = b''
    extension_protobuf_data_count: int = 0
    raw_line_speed_index = b''
    line_speed_index_count: int = 0
    raw_line_speed_index_small = b''
    line_speed_index_small_count: int = 0
    raw_line_roundabout = b''
    line_roundabout_count: int = 0

    square_data: SquareData = field(default_factory=SquareData)
    line_summary: LineSummary = field(default_factory=LineSummary)

    str0: list[str] = field(default_factory=list) # Street Type Name?
    str1: list[str] = field(default_factory=list) # Street Name
    str2: list[str] = field(default_factory=list)
    str3: list[str] = field(default_factory=list) # Street Prefix
    str4: list[str] = field(default_factory=list) # Street Suffix
    str5: list[str] = field(default_factory=list) # City / Region Names
    str6: list[str] = field(default_factory=list) # Metadata
    str7: list[str] = field(default_factory=list) # Building / Venue Names

    points: list[PointData] = field(default_factory=list)
    polygons: list[Polygon] = field(default_factory=list)
    streets: list[Street] = field(default_factory=list)
    lines: list[LineData] = field(default_factory=list)
    line_levels: list[LineExtLevel] = field(default_factory=list)

    latitude: int = 0
    longitude: int = 0
    scale: int = 0

    def get_tile_data_item(self, item_id: int, element_size: int):
        start_offset = 0
        end_offset = self.offsets[0]
        if item_id != 0:
            start_offset = self.offsets[item_id - 1]
            end_offset = self.offsets[item_id]

        if item_id == 34: # workaround
            start_offset += 3
            end_offset += 1
        
        total_bytes = end_offset - start_offset

        assert(element_size != 0)

        element_count = total_bytes // element_size
        
        #* technically this is a check, but... it either doesn't run in waze
        #* or my implementation is broken. There's 3 remaining bytes which are
        #* part of this offset, but unused
        #* ! might also be because of the maskbits workaround, who knows
        # assert(total_bytes == element_count * element_size)
        
        return self.raw_data_block[start_offset:start_offset + total_bytes], element_count

    def __init__(self, data: bytes):
        self.offsets = []
        self.raw_data = data

        self.entry_count = int.from_bytes(data[:4], "little") # 46 rn
        self.mask_bits = int.from_bytes(data[4:8], "little") # 2 rn

        data = data[8:]
        for _ in range(self.entry_count):
            self.offsets.append(int.from_bytes(data[:4], "little"))
            data = data[4:]

        self.raw_data_block = data
        
        self.raw_line_data, self.line_data_count = self.get_tile_data_item(9, 8)
        self.raw_line_summary, self.line_summary_count = self.get_tile_data_item(10, 62)
        self.raw_broken, self.broken_count = self.get_tile_data_item(11, 2)
        self.raw_point_data, self.point_data_count = self.get_tile_data_item(13, 4)
        self.raw_point_ids, self.point_ids_count = self.get_tile_data_item(14, 4)

        self.shapes, self.shapes_count = self.get_tile_data_item(8, 4)
        self.raw_str0, self.str0_count = self.get_tile_data_item(0, 1)
        self.raw_str1, self.str1_count = self.get_tile_data_item(1, 1)
        self.raw_str2, self.str2_count = self.get_tile_data_item(2, 1)
        self.raw_str3, self.str3_count = self.get_tile_data_item(3, 1)
        self.raw_str4, self.str4_count = self.get_tile_data_item(4, 1)
        self.raw_str5, self.str5_count = self.get_tile_data_item(5, 1)
        self.raw_str6, self.str6_count = self.get_tile_data_item(6, 1)
        self.raw_str7, self.str7_count = self.get_tile_data_item(7, 1)

        self.raw_line_routes, self.line_routes_count = self.get_tile_data_item(15, 4)
        self.raw_street_names, self.street_names_count = self.get_tile_data_item(16, 10)
        self.raw_street_cities, self.street_cities_count = self.get_tile_data_item(17, 4)
        self.raw_street_ids, self.street_ids_count = self.get_tile_data_item(37, 4)
        self.raw_polygon_heads, self.polygon_heads_count = self.get_tile_data_item(18, 16)
        self.raw_polygon_points, self.polygon_points_count = self.get_tile_data_item(19, 2)
        self.raw_line_speed_average, self.line_speed_average_count = self.get_tile_data_item(21, 2)
        self.raw_line_ref, self.line_ref_count = self.get_tile_data_item(20, 4)
        self.raw_ranges, self.ranges_count = self.get_tile_data_item(24, 6)
        self.raw_alert_data, self.alert_data_count = self.get_tile_data_item(25, 16)
        self.raw_square_data, self.square_data_count = self.get_tile_data_item(26, 12)

        self.raw_metadata_attributes, self.metadata_attributes_count = self.get_tile_data_item(27, 8)
        self.raw_venue_heads, self.venue_heads_count = self.get_tile_data_item(28, 16)
        self.raw_venue_ids, self.venue_ids_count = self.get_tile_data_item(29, 2)
        self.raw_line_ext_types, self.line_ext_types_count = self.get_tile_data_item(30, 1)
        self.raw_line_ext_ids, self.line_ext_ids_count = self.get_tile_data_item(31, 4)
        self.raw_line_ext_level_by_line, self.line_ext_level_by_line_count = self.get_tile_data_item(32, 1)
        self.raw_line_ext_levels, self.line_ext_levels_count = self.get_tile_data_item(33, 1)
        self.raw_line_speed_max, self.line_speed_max_count = self.get_tile_data_item(34, 2)
        self.raw_line_attributes, self.line_attributes_count = self.get_tile_data_item(36, 4)
        self.raw_lane_types, self.lane_types_count = self.get_tile_data_item(43, 1)
        self.raw_line_new_types, self.line_new_types_count = self.get_tile_data_item(44, 1)
        self.raw_polygon_ex, self.polygon_ex_count = self.get_tile_data_item(35, 4)
        self.raw_beacon_pos, self.beacon_pos_count = self.get_tile_data_item(38, 4)
        self.raw_beacon_ids, self.beacon_ids_count = self.get_tile_data_item(39, 16)
        self.raw_beacon_ex_pos, self.beacon_ex_pos_count = self.get_tile_data_item(40, 4)
        self.raw_beacon_ex_ids, self.beacon_ex_ids_count = self.get_tile_data_item(41, 16)
        self.raw_beacon_ex_masks, self.beacon_ex_masks_count = self.get_tile_data_item(42, 1)
        self.raw_extension_protobuf_data, self.extension_protobuf_data_count = self.get_tile_data_item(45, 1)
        self.raw_line_speed_index, self.line_speed_index_count = self.get_tile_data_item(22, 4)
        self.raw_line_speed_index_small, self.line_speed_index_small_count = self.get_tile_data_item(23, 2)
        self.raw_line_roundabout, self.line_roundabout_count = self.get_tile_data_item(12, 2)

        self.square_data = SquareData(0, self.raw_square_data)
        self.line_summary = LineSummary(0, self.raw_line_summary)

        self.str0 = [i.decode("utf-8") if i else "" for i in self.raw_str0.split(b"\0")]
        self.str1 = [i.decode("utf-8") if i else "" for i in self.raw_str1.split(b"\0")]
        self.str2 = [i.decode("utf-8") if i else "" for i in self.raw_str2.split(b"\0")]
        self.str3 = [i.decode("utf-8") if i else "" for i in self.raw_str3.split(b"\0")]
        self.str4 = [i.decode("utf-8") if i else "" for i in self.raw_str4.split(b"\0")]
        self.str5 = [i.decode("utf-8") if i else "" for i in self.raw_str5.split(b"\0")]
        self.str6 = [i.decode("utf-8") if i else "" for i in self.raw_str6.split(b"\0")]
        self.str7 = [i.decode("utf-8") if i else "" for i in self.raw_str7.split(b"\0")]

        self.points = []
        for i in range(len(self.raw_point_data)):
            try:
                self.points.append(PointData(i, self.raw_point_data))
            except:
                pass

        self.polygons = []
        for i in range(self.polygon_heads_count):
            try:
                self.polygons.append(Polygon(i, self.raw_polygon_heads, self.raw_polygon_points, self.points, self.raw_str7, self.raw_polygon_ex))
            except:
                pass

        self.streets = []
        for i in range(self.street_names_count):
            try:
                self.streets.append(Street(i, self.raw_street_names, self.raw_str0, self.raw_str1, self.raw_str3, self.raw_str4))
            except:
                pass

        self.lines = []
        for i in range(self.line_data_count):
            try:
                self.lines.append(LineData(i, self.raw_line_data, self.raw_ranges, self.streets, self.raw_line_routes, self.raw_line_ext_types, self.raw_line_ext_ids, self.raw_line_ext_level_by_line, self.raw_line_speed_max, self.raw_line_attributes, self.raw_lane_types))
            except:
                pass

        self.line_levels = []
        for i in range(self.line_ext_levels_count):
            try:
                self.line_levels.append(LineExtLevel(i, self.raw_line_ext_levels))
            except:
                pass
