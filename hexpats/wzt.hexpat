// https://github.com/mkoloberdin/waze/blob/e199da023302b6475c4660e6a6e4eb386c511572/roadmap_types.h#L45-L65
struct RoadMapSize {
    u32 width;
    u32 height;
};
struct RoadMapPosition {
    u32 longitude;
    u32 latitude;
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
    u16 first_point_idx;
    u16 last_point_idx;
    s16 shape_idx;
    u16 street_idx;
};
struct LineSummary {
    u16 next[0x15]; // ?
    u16 roundabout_count;
    u16 first_broken[0x9]; // ?
};
struct Broken {
    u16 data; // ?
};
struct LineRoundAbout {
    u8 data[2]; // ?
};
struct PointData {
    u16 longitude;
    u16 latitude;
};
struct PointId {
    s32 data; // probably internal ref
};
struct Shape {
    u16 delta_longitude;
    u16 delta_latitude;
};
struct LineRoute {
    u8 from_flags;
    u8 to_flags;
    u8 from_turn_flags;
    u8 to_turn_flags;
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
struct PolygonHead {
    u16 first_polygon_point_idx;
    u16 polygon_point_count;
    u16 name;
    u8 category; // cfcc
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
    u8 from_avg_speed;
    u8 to_avg_speed;
};
struct LineRef {
    u8 data[4];
};
struct Range {
    u16 street;
    u16 from_address;
    u16 to_address;
};
struct AlertData {
    RoadMapPosition pos;
    u16 steering;
    u8 category;
    u8 speed;
    u16 id;
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
struct VenueHead {
    u8 data[16];
};
struct VenueId {
    u8 data[2];
};
struct LineExtType {
    u8 data; // road type
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
    u8 speed;
    u8 idk;
};
struct LineAttributes {
    u8 data[4];
};
struct LaneType {
    u8 data;
};
struct LineNewType {
    u8 data;
};
struct PolygonEx {
    u32 data; // possibly editor ref id
};
struct BeaconPos {
    u8 data[4];
};
struct BeaconId {
    u8 data[16];
};
struct BeaconExPos {
    u8 data[4];
};
struct BeaconExId {
    u8 data[16];
};
struct BeaconExMask {
    u8 data;    
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
    u8 data[2];
};


// TODO: current offset calc stuff thing is broken because
// TODO: it relies on a constant that just so happens to work
// TODO: for most of waze's official tiles (maskbit 2)
// TODO: but i havent (yet) reversed how the fuck it uses the maskbits
// TODO: nor am i gonna do that any time soon

// TODO: this also doesnt handle entry counts < 46
// TODO: (but all modern tiles are entrycount == 46 either way)
struct File {
    u32 entry_count;
    u32 mask_bits;
    u32 offsets[entry_count];
    padding[5];

    u32 temp = entry_count * 4 + 8;
    
    LineData lineData[(offsets[9] - offsets[8]) / sizeof(LineData)] @ offsets[8] + temp;
    LineSummary lineSummaries[(offsets[10] - offsets[9]) / sizeof(LineSummary)] @ offsets[9] + temp;
    Broken broken[(offsets[11] - offsets[10]) / sizeof(Broken)] @ offsets[10] + temp;
    PointData pointData[(offsets[13] - offsets[12]) / sizeof(PointData)] @ offsets[12] + temp;
    PointId pointIds[(offsets[14] - offsets[13]) / sizeof(PointId)] @ offsets[13] + temp;
    Shape shape[(offsets[8] - offsets[7]) / sizeof(Shape)] @ offsets[7] + temp;
    StringStruct strStruct0[(offsets[0]) / sizeof(StringStruct)] @ temp;
    StringStruct strStruct1[(offsets[1] - offsets[0]) / sizeof(StringStruct)] @ offsets[0] + temp;
    StringStruct strStruct2[(offsets[2] - offsets[1]) / sizeof(StringStruct)] @ offsets[1] + temp;
    StringStruct strStruct3[(offsets[3] - offsets[2]) / sizeof(StringStruct)] @ offsets[2] + temp;
    StringStruct strStruct4[(offsets[4] - offsets[3]) / sizeof(StringStruct)] @ offsets[3] + temp;
    StringStruct strStruct5[(offsets[5] - offsets[4]) / sizeof(StringStruct)] @ offsets[4] + temp;
    StringStruct strStruct6[(offsets[6] - offsets[5]) / sizeof(StringStruct)] @ offsets[5] + temp;
    StringStruct strStruct7[(offsets[7] - offsets[6]) / sizeof(StringStruct)] @ offsets[6] + temp;
    LineRoute lineRoutes[(offsets[15] - offsets[14]) / sizeof(LineRoute)] @ offsets[14] + temp;
    StreetName streetNames[(offsets[16] - offsets[15]) / sizeof(StreetName)] @ offsets[15] + temp;
    StreetCity streetCity[(offsets[17] - offsets[16]) / sizeof(StreetCity)] @ offsets[16] + temp;
    StreetId streetIds[(offsets[37] - offsets[36]) / sizeof(StreetId)] @ offsets[36] + temp;
    PolygonHead polygonHeads[(offsets[18] - offsets[17]) / sizeof(PolygonHead)] @ offsets[17] + temp;
    PolygonPoint polygonPoints[(offsets[19] - offsets[18]) / sizeof(PolygonPoint)] @ offsets[18] + temp;
    LineSpeedAverage lineSpeedAvg[(offsets[21] - offsets[20]) / sizeof(LineSpeedAverage)] @ offsets[20] + temp;
    LineRef lineRef[(offsets[20] - offsets[19]) / sizeof(LineRef)] @ offsets[19] + temp;
    Range range[(offsets[24] - offsets[23]) / sizeof(Range)] @ offsets[23] + temp;
    AlertData alertData[(offsets[25] - offsets[24]) / sizeof(AlertData)] @ offsets[24] + temp;
    SquareData squareData[(offsets[26] - offsets[25]) / sizeof(SquareData)] @ offsets[25] + temp;
    MetadataAttributes metadataAttrib[(offsets[27] - offsets[26]) / sizeof(MetadataAttributes)] @ offsets[26] + temp;
    VenueHead venueHeads[(offsets[28] - offsets[27]) / sizeof(VenueHead)] @ offsets[27] + temp;
    VenueId venueIds[(offsets[29] - offsets[28]) / sizeof(VenueId)] @ offsets[28] + temp;
    LineExtType lineExtTypes[(offsets[30] - offsets[29]) / sizeof(LineExtType)] @ offsets[29] + temp;
    LineExtId lineExtIds[(offsets[31] - offsets[30]) / sizeof(LineExtId)] @ offsets[30] + temp;
    LineExtLevelByLine lineExtLevelByLines[(offsets[32] - offsets[31]) / sizeof(LineExtLevelByLine)] @ offsets[31] + temp;
    LineExtLevels lineExtLevels[(offsets[33] - offsets[32]) / sizeof(LineExtLevels)] @ offsets[32] + temp;
    LineSpeedMax lineSpeedMaxes[(offsets[34] - offsets[33] - 2) / sizeof(LineSpeedMax)] @ offsets[33] + temp + 3;
    LineAttributes lineAttributes[(offsets[36] - offsets[35]) / sizeof(LineAttributes)] @ offsets[35] + temp;
    LaneType laneTypes[(offsets[43] - offsets[42]) / sizeof(LaneType)] @ offsets[42] + temp;
    LineNewType lineNewTypes[(offsets[44] - offsets[43]) / sizeof(LineNewType)] @ offsets[43] + temp;
    PolygonEx polygonEx[(offsets[35] - offsets[34]) / sizeof(PolygonEx)] @ offsets[34] + temp;
    BeaconPos beaconPos[(offsets[38] - offsets[37]) / sizeof(BeaconPos)] @ offsets[37] + temp;
    BeaconId beaconIds[(offsets[39] - offsets[38]) / sizeof(BeaconId)] @ offsets[38] + temp;
    BeaconExPos beaconExPos[(offsets[40] - offsets[39]) / sizeof(BeaconExPos)] @ offsets[39] + temp;
    BeaconExId beaconExIds[(offsets[41] - offsets[40]) / sizeof(BeaconExId)] @ offsets[40] + temp;
    BeaconExMask beaconExMasks[(offsets[42] - offsets[41]) / sizeof(BeaconExMask)] @ offsets[41] + temp;
    ExtensionProtobufData extProtoData[(offsets[45] - offsets[44]) / sizeof(ExtensionProtobufData)] @ offsets[44] + temp;
    LineSpeedIndex lineSpeedIdx[(offsets[22] - offsets[21]) / sizeof(LineSpeedIndex)] @ offsets[21] + temp;
    LineSpeedIndexSmall lineSpeedIdxSm[(offsets[23] - offsets[22]) / sizeof(LineSpeedIndexSmall)] @ offsets[22] + temp;
    LineRoundabout lineRoundabout[(offsets[12] - offsets[11]) / sizeof(LineRoundabout)] @ offsets[11] + temp;
};
File file @ 0x0;