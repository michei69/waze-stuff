#ifndef WAZE_TYPES_H
#define WAZE_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
//                       WZDF — Waze Data File (compressed)
// ============================================================================

#define WZDF_MAGIC     0x46445A57  /* "WZDF" little-endian */
#define WZDF_ENDIANESS 0x01
#define WZDF_VERSION   0x00030000  /* 3.0 in little-endian */

typedef struct __attribute__((packed)) {
    uint32_t magic;            /* "WZDF" */
    uint32_t endianess;        /* 0x01 */
    uint32_t version;          /* 0x00030000 */
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    /* uint8_t data[compressed_size] */
} WzdfHeader;

// ============================================================================
//                   WZM — Waze Map Package (container of tiles)
// ============================================================================

#define WZM_MAGIC 0x2E2E2E2E

typedef struct __attribute__((packed)) {
    uint32_t tile_id;          /* big-endian */
    uint32_t padding;
    uint32_t raw_tile_size;    /* big-endian */

    // WZDF tile header
    char     tile_magic[4];
    uint32_t endianess;
    uint32_t version;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    /* uint8_t compressed_data[compressed_size] */
} WzmTileHeader;


// TODO: clean up cuz its a mess (basically copy pasted from the hexpats)

// ============================================================================
//                               WZT — Waze Tile
// ============================================================================

/* --- Basic geometry types (from roadmap_types.h) --- */

typedef struct __attribute__((packed)) {
    uint32_t width;
    uint32_t height;
} RoadMapSize;

typedef struct __attribute__((packed)) {
    int32_t longitude;
    int32_t latitude;
} RoadMapPosition;

typedef struct __attribute__((packed)) {
    uint32_t first;
    uint32_t count;
} RoadMapSortedList;

typedef struct __attribute__((packed)) {
    uint32_t east;
    uint32_t north;
    uint32_t west;
    uint32_t south;
} RoadMapArea;

/* --- Line data --- */

typedef struct __attribute__((packed)) {
    uint16_t first_point_idx;  /* bit 15 = FAKE POINT */
    uint16_t last_point_idx;   /* bit 15 = FAKE POINT */
    int16_t  shape_idx;
    uint16_t street_idx;
} LineDataRaw;

#define POINT_FAKE_FLAG 0x8000
#define POINT_INDEX_MASK 0x7FFF

/* --- Line summary (62 bytes) --- */

typedef struct __attribute__((packed)) {
    uint16_t next[21];              /* 0x15 next lines of road type */
    uint16_t roundabout_count;
    uint16_t first_broken[8];       /* dir*2 + side */
    uint16_t total_broken_count;
} LineSummaryRaw;

typedef struct __attribute__((packed)) {
    uint16_t line_id;
} BrokenRaw;

typedef struct __attribute__((packed)) {
    uint16_t line_id;
} LineRoundaboutRaw;

/* --- Point data --- */

typedef struct __attribute__((packed)) {
    uint16_t longitude;
    uint16_t latitude;
} PointDataRaw;

typedef struct __attribute__((packed)) {
    int32_t data;
} PointIdRaw;

/* --- Shape (delta-encoded) --- */

typedef struct __attribute__((packed)) {
    int16_t delta_longitude;
    int16_t delta_latitude;
} ShapeRaw;

/* --- Line route / turn restrictions --- */

#define ROUTE_CAR_ALLOWED 0x01
#define ROUTE_LOW_WEIGHT  0x02

typedef struct __attribute__((packed)) {
    uint8_t a_to_b;       /* LineFlags */
    uint8_t b_to_a;       /* LineFlags */
    uint8_t b_to_a_turn;  /* LineTurnRestriction bitfield */
    uint8_t a_to_b_turn;  /* LineTurnRestriction bitfield */
} LineRouteRaw;

/* --- Street name (10 bytes) --- */

typedef struct __attribute__((packed)) {
    uint16_t type_part;       /* fedirp -> str0 */
    uint16_t name_part;       /* fename -> str1 */
    uint16_t prefix_part;     /* fetype -> str3 */
    uint16_t suffix_part;     /* fedirs -> str4 */
    uint16_t text2speech;     /* t2s */
} StreetNameRaw;

/* --- Street city (4 bytes) --- */

typedef struct __attribute__((packed)) {
    uint16_t city;
    uint16_t first_street;
} StreetCityRaw;

/* --- Street ID --- */

typedef struct __attribute__((packed)) {
    int32_t data;
} StreetIdRaw;

/* --- Polygon types --- */

typedef enum : uint8_t {
    POLY_WATER_STREAM_RIVER = 10,
    POLY_RIVER              = 11,
    POLY_LAND_GROUND        = 12,
    POLY_RESERVOIR_CANAL    = 13,
    POLY_OCEAN_SEA          = 14,
    POLY_BUILDING_VENUE     = 15,
    POLY_CITY               = 16,
    POLY_WATER              = 20,
    POLY_FOREST             = 21,
    POLY_GOLF               = 22,
    POLY_CEMETERY           = 23,
    POLY_GENERIC_BUILDING   = 30,
    POLY_COMMERCIAL         = 31,
    POLY_BUILT_UP_AREA      = 40,
    POLY_AIRPORT            = 50
} PolygonType;

/* --- Polygon head (16 bytes) --- */

typedef struct __attribute__((packed)) {
    uint16_t first_polygon_point_idx;
    uint16_t polygon_point_count;
    uint16_t name; /* string offset into str7 */
    uint8_t  cfcc; /* PolygonType */
    uint8_t  _padding;
    uint16_t north;
    uint16_t west;
    uint16_t east;
    uint16_t south;
} PolygonHeadRaw;

/* --- Polygon point (2 bytes) --- */

typedef struct __attribute__((packed)) {
    uint16_t point_idx;
} PolygonPointRaw;

/* --- Line speed --- */

typedef struct __attribute__((packed)) {
    uint8_t a_to_b_avg_speed;  /* 0xFF = no data */
    uint8_t b_to_a_avg_speed;
} LineSpeedAverageRaw;

typedef struct __attribute__((packed)) {
    uint8_t a_to_b_speed_ref;
    uint8_t b_to_a_speed_ref;
} LineSpeedLineRefRaw;

typedef struct __attribute__((packed)) {
    uint16_t from_speed_ref;
    uint16_t to_speed_ref;
} LineSpeedIndexRaw;

typedef struct __attribute__((packed)) {
    uint8_t speed;
    uint8_t time_slot;
} LineSpeedIndexSmallRaw;

typedef struct __attribute__((packed)) {
    uint8_t a_to_b;  /* 0 = no data */
    uint8_t b_to_a;
} LineSpeedMaxRaw;

/* --- Range (6 bytes) --- */

typedef struct __attribute__((packed)) {
    uint16_t street;
    uint16_t from_address;
    uint16_t to_address;
} RangeRaw;

/* --- Alert data (16 bytes) --- */

typedef enum : uint8_t {
    ALERT_RAILROAD_CROSSING = 0,
    ALERT_DEFAULT_TILE      = 1,
    ALERT_POLICE            = 2,
    ALERT_RED_LIGHT_CAMERA  = 3,
    ALERT_HAZARD            = 4,
    ALERT_SPEED_CAMERA      = 5
} AlertCategory;

typedef struct __attribute__((packed)) {
    RoadMapPosition pos;
    uint16_t        speed_or_dist;
    uint8_t         category;    /* AlertCategory */
    uint8_t         sub_type;
    uint16_t        steering;
    uint8_t         _padding[2];
} AlertDataRaw;

/* --- Square data (12 bytes) --- */

typedef struct __attribute__((packed)) {
    int32_t  tile_id;
    int32_t  scale;
    uint32_t tile_version;  /* timestamp */
} SquareDataRaw;

/* --- Metadata attributes (8 bytes) --- */

typedef struct __attribute__((packed)) {
    uint16_t category;
    uint16_t name;
    uint16_t value;
    uint16_t _padding;
} MetadataAttributeRaw;

/* --- Venue (16 bytes head, 2 bytes id) --- */

typedef struct __attribute__((packed)) {
    uint16_t start_point_idx;
    uint16_t point_count;
    uint16_t name_string_idx;
    uint8_t  category;
    uint8_t  flags;
    uint16_t top;
    uint16_t left;
    uint16_t right;
    uint16_t bottom;
} VenueHeadRaw;

typedef struct __attribute__((packed)) {
    uint8_t data[2];
} VenueIdRaw;

/* --- Line extension types --- */

typedef enum : uint8_t {
    LINE_EXT_UNKNOWN   = 0,
    LINE_EXT_FREEWAY   = 1,
    LINE_EXT_MAJOR_HWY = 2,
    LINE_EXT_BOULEVARD = 3,
    LINE_EXT_RAMP      = 4,
    LINE_EXT_PRIMARY   = 5,
    LINE_EXT_PASSAGE   = 6,
    LINE_EXT_STREET    = 7,
    LINE_EXT_PEDESTRIAN= 8,
    LINE_EXT_DIRT      = 9,
    LINE_EXT_PATH      = 10,
    LINE_EXT_STAIRWAY  = 11,
    LINE_EXT_RAILWAY   = 12,
    LINE_EXT_RUNWAY    = 13,
    LINE_EXT_FERRY     = 14,
    LINE_EXT_PRIVATE   = 15,
    LINE_EXT_PARKING   = 16
} LineExtType;

typedef struct __attribute__((packed)) {
    uint32_t data;  /* editor ref id */
} LineExtIdRaw;

typedef struct __attribute__((packed)) {
    uint8_t data;  /* elevation id for line */
} LineExtLevelByLineRaw;

typedef struct __attribute__((packed)) {
    uint8_t data;  /* actual elevation */
} LineExtLevelRaw;

/* --- Line attributes --- */

typedef struct __attribute__((packed)) {
    uint8_t data[4];
} LineAttributesRaw;

/* --- Lane type --- */

typedef enum : uint8_t {
    LANE_REGULAR        = 0,
    LANE_HOV            = 1,  /* carpool */
    LANE_HOT            = 2,  /* high occupancy toll */
    LANE_EXPRESS        = 3,
    LANE_REGULAR_NO_TTS = 4,
    LANE_FAST           = 5
} LaneType;

typedef struct __attribute__((packed)) {
    uint8_t data;
} LineNewTypeRaw;

/* --- Polygon extension --- */

typedef struct __attribute__((packed)) {
    uint32_t data;  /* possibly editor ref id */
} PolygonExRaw;

/* --- Bluetooth beacons --- */

typedef struct __attribute__((packed)) {
    uint16_t longitude;
    uint16_t latitude;
} BeaconPosRaw;

typedef struct __attribute__((packed)) {
    uint8_t uuid[16];
} BeaconIdRaw;

typedef struct __attribute__((packed)) {
    uint16_t longitude;
    uint16_t latitude;
} BeaconExPosRaw;

typedef struct __attribute__((packed)) {
    uint8_t uuid[16];
} BeaconExIdRaw;

typedef struct __attribute__((packed)) {
    uint8_t flags;
} BeaconExMaskRaw;

// ============================================================================
//             Section indices into the offset array (0-based)
// ============================================================================

enum {
    WZT_SEC_STR0               = 0,   /* street type name */
    WZT_SEC_STR1               = 1,   /* street base name */
    WZT_SEC_STR2               = 2,   /* (unused/unknown) */
    WZT_SEC_STR3               = 3,   /* street prefix */
    WZT_SEC_STR4               = 4,   /* street suffix */
    WZT_SEC_STR5               = 5,   /* city names */
    WZT_SEC_STR6               = 6,   /* metadata */
    WZT_SEC_STR7               = 7,   /* venue names */
    WZT_SEC_SHAPES             = 8,
    WZT_SEC_LINE_DATA          = 9,
    WZT_SEC_LINE_SUMMARY       = 10,
    WZT_SEC_BROKEN             = 11,
    WZT_SEC_LINE_ROUNDABOUT    = 12,
    WZT_SEC_POINT_DATA         = 13,
    WZT_SEC_POINT_IDS          = 14,
    WZT_SEC_LINE_ROUTES        = 15,
    WZT_SEC_STREET_NAMES       = 16,
    WZT_SEC_STREET_CITIES      = 17,
    WZT_SEC_POLYGON_HEADS      = 18,
    WZT_SEC_POLYGON_POINTS     = 19,
    WZT_SEC_LINE_SPEED_REF     = 20,
    WZT_SEC_LINE_SPEED_AVG     = 21,
    WZT_SEC_LINE_SPEED_IDX     = 22,
    WZT_SEC_LINE_SPEED_IDX_SM  = 23,
    WZT_SEC_RANGES             = 24,
    WZT_SEC_ALERT_DATA         = 25,
    WZT_SEC_SQUARE_DATA        = 26,
    WZT_SEC_METADATA_ATTRIB    = 27,
    WZT_SEC_VENUE_HEADS        = 28,
    WZT_SEC_VENUE_IDS          = 29,
    WZT_SEC_LINE_EXT_TYPES     = 30,
    WZT_SEC_LINE_EXT_IDS       = 31,
    WZT_SEC_LINE_EXT_LVL_BY_LN = 32,
    WZT_SEC_LINE_EXT_LEVELS    = 33,
    WZT_SEC_LINE_SPEED_MAX     = 34,
    WZT_SEC_POLYGON_EX         = 35,
    WZT_SEC_LINE_ATTRIBUTES    = 36,
    WZT_SEC_STREET_IDS         = 37,
    WZT_SEC_BEACON_POS         = 38,
    WZT_SEC_BEACON_IDS         = 39,
    WZT_SEC_BEACON_EX_POS      = 40,
    WZT_SEC_BEACON_EX_IDS      = 41,
    WZT_SEC_BEACON_EX_MASKS    = 42,
    WZT_SEC_LANE_TYPES         = 43,
    WZT_SEC_LINE_NEW_TYPES     = 44,
    WZT_SEC_EXT_PROTOBUF       = 45,
    WZT_SECTION_COUNT          = 46
};

// ============================================================================
//                             Parsed WZT tile
// ============================================================================

/* Forward declarations */
typedef struct WazeTile        WazeTile;
typedef struct WazePoint       WazePoint;
typedef struct WazeShape       WazeShape;
typedef struct WazeLine        WazeLine;
typedef struct WazeStreet      WazeStreet;
typedef struct WazePolygon     WazePolygon;
typedef struct WazeSquare      WazeSquare;
typedef struct WazeLineSummary WazeLineSummary;

struct WazePoint {
    uint16_t x;
    uint16_t y;
};

struct WazeShape {
    int16_t dx;
    int16_t dy;
};

struct WazeStreet {
    const char *type_part;   /* fedirp -> raw_str[0] */
    const char *name_part;   /* fename -> raw_str[1] */
    const char *prefix_part; /* fetype -> raw_str[3] */
    const char *suffix_part; /* fedirs -> raw_str[4] */
    char       *full_name;   /* assembled name (malloc'd) */
};

struct WazeSquare {
    int32_t  tile_id;
    int32_t  scale;
    uint32_t timestamp;
    double   latitude;
    double   longitude;
};

struct WazeLineSummary {
    uint16_t next_lines[21];       /* line count per road type (0-20) */
    uint16_t roundabout_count;
    uint16_t first_brokens[8];     /* first broken line per dir*2+side */
    uint16_t total_broken_count;
};

struct WazeLine {
    uint16_t     first_point_idx;
    bool         first_point_fake;
    uint16_t     last_point_idx;
    bool         last_point_fake;
    bool         is_roundabout;
    bool         is_broken;
    int16_t      shape_idx;
    uint16_t     street_idx;
    uint8_t      road_type;
    uint8_t      lane_type;
    uint8_t      level;
    uint8_t      a_to_b;
    uint8_t      b_to_a;
    uint8_t      a_to_b_avg_speed;
    uint8_t      b_to_a_avg_speed;
    uint8_t      a_to_b_max_speed;
    uint8_t      b_to_a_max_speed;
    uint32_t     ext_id;
    uint32_t     attributes;
    WazeStreet   *street;
    WazeShape    *shapes;
    uint16_t     shape_count;  /* number of delta points */
};

struct WazePolygon {
    uint16_t     first_point_idx;
    uint16_t     point_count;
    uint8_t      cfcc;
    uint16_t     north;
    uint16_t     west;
    uint16_t     east;
    uint16_t     south;
    const char   *name;
    uint32_t     ex_data;
};

struct WazeTile {
    /* Raw data */
    uint8_t  *raw_data;
    uint32_t  raw_size;

    /* Header */
    uint32_t  entry_count;
    uint32_t  mask_bits;
    uint32_t  offsets[WZT_SECTION_COUNT];

    /* All section data pointers (aligned) and byte sizes */
    const uint8_t *sec_ptr[WZT_SECTION_COUNT];
    uint32_t       sec_size[WZT_SECTION_COUNT];

    /* Parsed arrays */
    WazePoint *points;
    uint32_t point_count;
    WazeShape *shapes;
    uint32_t shape_count;
    WazeStreet *streets;
    uint32_t street_count;
    WazeLine *lines;
    uint32_t line_count;
    WazePolygon *polygons;
    uint32_t polygon_count;
    WazeSquare square;
    WazeLineSummary *line_summary; 
    uint32_t line_summary_count;
    uint16_t *polygon_points;
    uint32_t polygon_point_count;

    /* Counts for remaining sections */
    uint32_t broken_count;
    uint32_t roundabout_count;
    uint32_t line_route_count;
    uint32_t street_city_count;
    uint32_t line_speed_avg_count;
    uint32_t line_speed_ref_count;
    uint32_t line_speed_idx_count;
    uint32_t line_speed_idx_sm_count;
    uint32_t range_count;
    uint32_t alert_count;
    uint32_t metadata_attr_count;
    uint32_t venue_head_count;
    uint32_t venue_id_count;
    uint32_t line_ext_id_count;
    uint32_t line_ext_level_by_line_count;
    uint32_t line_ext_level_count;
    uint32_t line_attributes_count;
    uint32_t beacon_pos_count;
    uint32_t beacon_id_count;
    uint32_t beacon_ex_pos_count;
    uint32_t beacon_ex_id_count;
    uint32_t beacon_ex_mask_count;
    uint32_t ext_protobuf_size;
};

#endif /* WAZE_TYPES_H */
