// https://github.com/mkoloberdin/waze/blob/e199da023302b6475c4660e6a6e4eb386c511572/roadmap_types.h#L45-L65
struct RoadMapSize {
    u32 width;
    u32 height;
};
struct RoadMapPosition {
    s32 longitude;
    s32 latitude;
};
struct RoadMapSortedList {
    u32 first;
    u32 count;
};
struct RoadMapArea {
    u32 east;
    u32 north;
    u32 west;
    u32 south;
};

// idk how to do those in hexpat lul
struct TerminatedString {
    char content[];
};
struct StringStruct {
    char character;
};



struct LineData {
    u16 first_point_idx; // bit 15 == FAKE POINT (aka, broken off)
    u16 last_point_idx; // bit 15 == FAKE POINT (aka, broken off)
    s16 shape_idx;
    u16 street_idx;
};
struct LineSummary {
    u16 next[0x15]; // Next line of road type
    u16 roundabout_count;
    // broken = line touches edge and connects with another from a different tile
    // first_broken quite literally means the first broken entry on each side
    // first_broken[direction * 2 + side] where:
    //   direction: EAST = 0, NORTH = 1, WEST = 2, SOUTH = 3
    //   side: a_to_b = 0, b_to_a = 1
    u16 first_broken[0x8];
    u16 total_broken_count;
};
struct Broken { // lists all lines that are broken by tile edges
    u16 line_id;
};
struct LineRoundAbout { // lists all lines that are part of a roundabout
    u16 line_id;
};
struct PointData {
    u16 longitude;
    u16 latitude;
};
struct PointId {
    s32 data; // probably internal ref
};
struct Shape {
    u16 delta_longitude; // or null
    u16 delta_latitude; // or shape count
};
bitfield LineTurnRestriction {
    line0: 1;
    line1: 1;
    line2: 1;
    line3: 1;
    line4: 1;
    line5: 1;
    line6: 1;
    line7: 1;
};
bitfield LineFlags {
    ROUTE_CAR_ALLOWED: 1;
    ROUTE_LOW_WEIGHT: 1;
    padding: 6;
};
struct LineRoute {
    LineFlags a_to_b;
    LineFlags b_to_a;
    LineTurnRestriction b_to_a_turn;
    LineTurnRestriction a_to_b_turn;
};
struct StreetName {
    u16 typePart; // fedirp
    u16 namePart; // fename
    u16 prefixPart; // fetype
    u16 suffixPart; // fedirs
    u16 text2speechPart; // t2s
};
struct StreetCity {
    u16 city;
    u16 first_street;
};
struct StreetId {
    s32 data; // probably internal ref
};
enum PolygonType : u8 {
    WaterStreamOrRiver = 10,
    River = 11,
    LandOrGround = 12,
    ReservoirOrCanal = 13,
    OceanOrSea = 14,
    BuildingOrVenue = 15,
    City = 16,
    Water = 20,
    Forest = 21,
    Golf = 22,
    Cemetery = 23,
    GenericBuilding = 30,
    Commercial = 31,
    BuiltUpArea = 40,
    Unknown = 41..49,
    Airport = 50,
};
struct PolygonHead {
    u16 first_polygon_point_idx;
    u16 polygon_point_count;
    u16 name;
    PolygonType cfcc; // category
    padding[1];
    u16 north;
    u16 west;
    u16 east;
    u16 south;
};
struct PolygonPoint {
    u16 point_idx;
};
struct LineSpeedAverage {
    // 0xff == no data
    u8 a_to_b_avg_speed; // km/h
    u8 b_to_a_avg_speed; // km/h
};
struct LineSpeedLineRef {
    u8 a_to_b_speed_ref;
    u8 b_to_a_speed_ref;
};
struct Range {
    u16 street;
    u16 from_address; // e.g. from house number
    u16 to_address; // e.g. to house number
};
enum AlertCategory : u8 {
    RailroadCrossing = 0,
    DefaultTile = 1,
    Police = 2,
    RedLightCamera = 3,
    Hazard = 4,
    SpeedCamera = 5
};
enum PoliceAlertSubType : u8 {
    POLICE_VISIBLE = 1,
    POLICE_HIDING = 2,
    POLICE_WITH_MOBILE_CAMERA = 41
};
enum HazardSubType : u8 {
    HAZARD_ON_ROAD = 9,
    HAZARD_ON_SHOULDER = 10,
    HAZARD_WEATHER = 11, 
    HAZARD_ON_ROAD_OBJECT = 12,
    HAZARD_ON_ROAD_POT_HOLE = 13,
    HAZARD_ON_ROAD_ROAD_KILL = 14,
    HAZARD_ON_SHOULDER_CAR_STOPPED = 15,
    HAZARD_ON_SHOULDER_ANIMALS = 16,
    HAZARD_ON_SHOULDER_MISSING_SIGN = 17,
    HAZARD_WEATHER_FOG = 18,
    HAZARD_WEATHER_HAIL = 19,
    HAZARD_WEATHER_HEAVY_RAIN = 20,
    HAZARD_WEATHER_HEAVY_SNOW = 21,
    HAZARD_WEATHER_FLOOD = 22,
    HAZARD_WEATHER_MONSOON = 23, 
    HAZARD_WEATHER_TORNADO = 24,
    HAZARD_WEATHER_HEAT_WAVE = 25,
    HAZARD_WEATHER_HURRICANE = 26,
    HAZARD_WEATHER_FREEZING_RAIN = 27,
    HAZARD_ON_ROAD_LANE_CLOSED = 28,     
    HAZARD_ON_ROAD_OIL = 29,
    HAZARD_ON_ROAD_ICE = 30,   
    HAZARD_ON_ROAD_CONSTRUCTION = 31,
    HAZARD_ON_ROAD_CAR_STOPPED = 32,
    HAZARD_ON_ROAD_TRAFFIC_LIGHT_FAULT = 33,
    HAZARD_ON_ROAD_EMERGENCY_VEHICLE = 39,
};
struct AlertData {
    RoadMapPosition pos;
    u16 speed_or_dist; // speed camera
    AlertCategory category;
    try {
        match (category) {
            (AlertCategory::Police): PoliceAlertSubType sub_type;
            (AlertCategory::Hazard): HazardSubType sub_type;
            (_): u8 sub_type;
        }
    } catch {
        u8 sub_type;
    }
    
    u16 steering; // road direction at alert point
    padding[2];
};
struct SquareData {
    s32 tile_id;
    s32 scale;
    u32 tile_version; // timestamp
};
struct MetadataAttributes {
    u16 category;
    u16 name;
    u16 value;
    padding[2];
};
bitfield VenueFlags {
    visible: 1;
    padding: 7; // todo
};
struct VenueHead {
    u16 start_point_idx;
    u16 point_count;
    u16 name_string_idx;
    u8 category;
    VenueFlags flags;
    u16 top;
    u16 left;
    u16 right;
    u16 bottom;
};
struct VenueId {
    u8 data[2];
};
enum LineExtType : u8 {
    Unknown = 0,
    Freeway = 1,
    MajorHighway = 2,
    Boulevard = 3,
    Ramp = 4,
    Primary = 5,
    Passage = 6,
    Street = 7,
    Pedestrian = 8,
    Dirt = 9,
    Path = 10,
    Stairway = 11,
    Railway = 12,
    Runway = 13,
    Ferry = 14,
    Private = 15,
    Parking = 16
};
struct LineExtId {
    u32 data; // editor ref id
};
struct LineExtLevelByLine {
    u8 data; // elevation id for line
};
struct LineExtLevels {
    u8 data; // actual elevation
};
struct LineSpeedMax {
    // 0 == no data
    u8 a_to_b;
    u8 b_to_a;
};
bitfield LineFlagsThing {
    Tunnel: 1;
    padding: 3;
    Unpaved: 1;
    HeadlightRequired: 1;
    Beacons: 1; // TODO
    AdjacentToHOVOrBusLane: 1;
};
struct LineAttributes {
    LineFlagsThing flags;
    u8 data[3];
};
enum LaneType : u8 {
    Regular = 0,
    HOV = 1, // high occupancy vehicle / carpool
    HOT = 2, // high occupancy toll
    Express = 3,
    RegularNoTTS = 4, // doesnt activate tts ?
    Fast = 5
};
struct LineNewType {
    u8 data;
};
struct PolygonEx {
    u32 data; // possibly editor ref id
};

// bluetooth beacons
struct BeaconPos {
    u16 longitude;
    u16 latitude;
};
struct BeaconId {
    u8 uuid[16];
};
struct BeaconExPos {
    u16 longitude;
    u16 latitude;
};
struct BeaconExId {
    u8 uuid[16];
};
struct BeaconExMask {
    u8 flags;
};


struct ExtensionProtobufData {
    u8 data;
};
struct LineSpeedIndex {
   u16 from_speed_ref;
   u16 to_speed_ref;
};
struct LineSpeedIndexSmall {
    u8 speed;
    u8 time_slot;
};
struct LineRoundabout {
    u16 line_id;
};


// mask_bits: byte alignment for section data (roadmap_data_format.h)
//   add = (1 << mask_bits) - 1,  mask = ~add
//   aligned_offset = (raw_offset + add) & mask
//   mask_bits=2 → 4-byte alignment, mask_bits=0 → no alignment

fn align(u32 off, u32 add, u32 mask) { return (off + add) & mask; };

struct File {
    u32 entry_count;
    u32 mask_bits;
    u32 offsets[entry_count];

    u32 temp = entry_count * 4 + 8;              // start of data block
    u32 add  = (1 << mask_bits) - 1;             // alignment addend
    u32 mask = ~add;                              // alignment mask

    u32 a0  = align(0, add, mask);
    u32 a1  = align(offsets[0],  add, mask);
    u32 a2  = align(offsets[1],  add, mask);
    u32 a3  = align(offsets[2],  add, mask);
    u32 a4  = align(offsets[3],  add, mask);
    u32 a5  = align(offsets[4],  add, mask);
    u32 a6  = align(offsets[5],  add, mask);
    u32 a7  = align(offsets[6],  add, mask);
    u32 a8  = align(offsets[7],  add, mask);
    u32 a9  = align(offsets[8],  add, mask);
    u32 a10 = align(offsets[9],  add, mask);
    u32 a11 = align(offsets[10], add, mask);
    u32 a12 = align(offsets[11], add, mask);
    u32 a13 = align(offsets[12], add, mask);
    u32 a14 = align(offsets[13], add, mask);
    u32 a15 = align(offsets[14], add, mask);
    u32 a16 = align(offsets[15], add, mask);
    u32 a17 = align(offsets[16], add, mask);
    u32 a18 = align(offsets[17], add, mask);
    u32 a19 = align(offsets[18], add, mask);
    u32 a20 = align(offsets[19], add, mask);
    u32 a21 = align(offsets[20], add, mask);
    u32 a22 = align(offsets[21], add, mask);
    u32 a23 = align(offsets[22], add, mask);
    u32 a24 = align(offsets[23], add, mask);
    u32 a25 = align(offsets[24], add, mask);
    u32 a26 = align(offsets[25], add, mask);
    u32 a27 = align(offsets[26], add, mask);
    u32 a28 = align(offsets[27], add, mask);
    u32 a29 = align(offsets[28], add, mask);
    u32 a30 = align(offsets[29], add, mask);
    u32 a31 = align(offsets[30], add, mask);
    u32 a32 = align(offsets[31], add, mask);
    u32 a33 = align(offsets[32], add, mask);
    u32 a34 = align(offsets[33], add, mask);
    u32 a35 = align(offsets[34], add, mask);
    u32 a36 = align(offsets[35], add, mask);
    u32 a37 = align(offsets[36], add, mask);
    u32 a38 = align(offsets[37], add, mask);
    u32 a39 = align(offsets[38], add, mask);
    u32 a40 = align(offsets[39], add, mask);
    u32 a41 = align(offsets[40], add, mask);
    u32 a42 = align(offsets[41], add, mask);
    u32 a43 = align(offsets[42], add, mask);
    u32 a44 = align(offsets[43], add, mask);
    u32 a45 = align(offsets[44], add, mask);

    StringStruct strStruct0[(offsets[0] - 0) / sizeof(StringStruct)] @ a0 + temp;
    StringStruct strStruct1[(offsets[1] - offsets[0]) / sizeof(StringStruct)] @ a1 + temp;
    StringStruct strStruct2[(offsets[2] - offsets[1]) / sizeof(StringStruct)] @ a2 + temp;
    StringStruct strStruct3[(offsets[3] - offsets[2]) / sizeof(StringStruct)] @ a3 + temp;
    StringStruct strStruct4[(offsets[4] - offsets[3]) / sizeof(StringStruct)] @ a4 + temp;
    StringStruct strStruct5[(offsets[5] - offsets[4]) / sizeof(StringStruct)] @ a5 + temp;
    StringStruct strStruct6[(offsets[6] - offsets[5]) / sizeof(StringStruct)] @ a6 + temp;
    StringStruct strStruct7[(offsets[7] - offsets[6]) / sizeof(StringStruct)] @ a7 + temp;

    Shape shape[(offsets[8] - offsets[7]) / sizeof(Shape)] @ a8 + temp;
    LineData lineData[(offsets[9] - offsets[8]) / sizeof(LineData)] @ a9 + temp;
    LineSummary lineSummaries[(offsets[10] - offsets[9]) / sizeof(LineSummary)] @ a10 + temp;
    Broken broken[(offsets[11] - offsets[10]) / sizeof(Broken)] @ a11 + temp;
    LineRoundabout lineRoundabout[(offsets[12] - offsets[11]) / sizeof(LineRoundabout)] @ a12 + temp;
    PointData pointData[(offsets[13] - offsets[12]) / sizeof(PointData)] @ a13 + temp;
    PointId pointIds[(offsets[14] - offsets[13]) / sizeof(PointId)] @ a14 + temp;
    LineRoute lineRoutes[(offsets[15] - offsets[14]) / sizeof(LineRoute)] @ a15 + temp;
    StreetName streetNames[(offsets[16] - offsets[15]) / sizeof(StreetName)] @ a16 + temp;
    StreetCity streetCity[(offsets[17] - offsets[16]) / sizeof(StreetCity)] @ a17 + temp;
    PolygonHead polygonHeads[(offsets[18] - offsets[17]) / sizeof(PolygonHead)] @ a18 + temp;
    PolygonPoint polygonPoints[(offsets[19] - offsets[18]) / sizeof(PolygonPoint)] @ a19 + temp;
    LineSpeedLineRef lineSpeedLineRef[(offsets[20] - offsets[19]) / sizeof(LineSpeedLineRef)] @ a20 + temp;
    LineSpeedAverage lineSpeedAvg[(offsets[21] - offsets[20]) / sizeof(LineSpeedAverage)] @ a21 + temp;
    LineSpeedIndex lineSpeedIdx[(offsets[22] - offsets[21]) / sizeof(LineSpeedIndex)] @ a22 + temp;
    LineSpeedIndexSmall lineSpeedIdxSm[(offsets[23] - offsets[22]) / sizeof(LineSpeedIndexSmall)] @ a23 + temp;
    Range range[(offsets[24] - offsets[23]) / sizeof(Range)] @ a24 + temp;
    AlertData alertData[(offsets[25] - offsets[24]) / sizeof(AlertData)] @ a25 + temp;
    SquareData squareData[(offsets[26] - offsets[25]) / sizeof(SquareData)] @ a26 + temp;
    MetadataAttributes metadataAttrib[(offsets[27] - offsets[26]) / sizeof(MetadataAttributes)] @ a27 + temp;
    VenueHead venueHeads[(offsets[28] - offsets[27]) / sizeof(VenueHead)] @ a28 + temp;
    VenueId venueIds[(offsets[29] - offsets[28]) / sizeof(VenueId)] @ a29 + temp;
    LineExtType lineExtTypes[(offsets[30] - offsets[29]) / sizeof(LineExtType)] @ a30 + temp;
    LineExtId lineExtIds[(offsets[31] - offsets[30]) / sizeof(LineExtId)] @ a31 + temp;
    LineExtLevelByLine lineExtLevelByLines[(offsets[32] - offsets[31]) / sizeof(LineExtLevelByLine)] @ a32 + temp;
    LineExtLevels lineExtLevels[(offsets[33] - offsets[32]) / sizeof(LineExtLevels)] @ a33 + temp;
    LineSpeedMax lineSpeedMaxes[(offsets[34] - offsets[33]) / sizeof(LineSpeedMax)] @ a34 + temp;
    PolygonEx polygonEx[(offsets[35] - offsets[34]) / sizeof(PolygonEx)] @ a35 + temp;
    LineAttributes lineAttributes[(offsets[36] - offsets[35]) / sizeof(LineAttributes)] @ a36 + temp;
    StreetId streetIds[(offsets[37] - offsets[36]) / sizeof(StreetId)] @ a37 + temp;
    BeaconPos beaconPos[(offsets[38] - offsets[37]) / sizeof(BeaconPos)] @ a38 + temp;
    BeaconId beaconIds[(offsets[39] - offsets[38]) / sizeof(BeaconId)] @ a39 + temp;
    BeaconExPos beaconExPos[(offsets[40] - offsets[39]) / sizeof(BeaconExPos)] @ a40 + temp;
    BeaconExId beaconExIds[(offsets[41] - offsets[40]) / sizeof(BeaconExId)] @ a41 + temp;
    BeaconExMask beaconExMasks[(offsets[42] - offsets[41]) / sizeof(BeaconExMask)] @ a42 + temp;
    LaneType laneTypes[(offsets[43] - offsets[42]) / sizeof(LaneType)] @ a43 + temp;
    LineNewType lineNewTypes[(offsets[44] - offsets[43]) / sizeof(LineNewType)] @ a44 + temp - 1;
    ExtensionProtobufData extProtoData[(offsets[45] - offsets[44]) / sizeof(ExtensionProtobufData)] @ a45 + temp;
};
File file @ 0x0;