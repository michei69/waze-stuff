import fs from "fs"
import { randomUUID } from "crypto"

interface Point {
    x: number;
    y: number;
}

interface TileLine {
    points: Point[];
    lines: { [key: string]: { points: number[], type: number } };
}

function getTileId(lat: number, lon: number) {
    lat *= 100
    lon *= 100
    lat = Math.floor(lat) + 9000
    lon = Math.floor(lon) + 18000
    return lon * 18000 + lat
}

function getTileCenter(tileId: number): [number, number] {
    const lat = ((tileId % 18000) - 9000) / 100
    const lon = (Math.floor(tileId / 18000) - 18000) / 100
    return [lon, lat]
}

function toTileCoordinates(lon: number, lat: number, tileCenter: [number, number]): Point {
    return {
        x: Math.floor(Math.max(Math.min((lon - tileCenter[0]) * 1_000_000, 10000), 0)),
        y: Math.floor(Math.max(Math.min((lat - tileCenter[1]) * 1_000_000, 10000), 0))
    }
}
// doesnt clamp the values
function toTileCoordinatesRaw(lon: number, lat: number, tileCenter: [number, number]): Point {
    return {
        x: Math.floor((lon - tileCenter[0]) * 1_000_000),
        y: Math.floor((lat - tileCenter[1]) * 1_000_000)
    }
}

function getTileBounds(tileId: number): { minLon: number, minLat: number, maxLon: number, maxLat: number } {
    const [centerLon, centerLat] = getTileCenter(tileId)
    return {
        minLon: centerLon,
        minLat: centerLat,
        maxLon: centerLon + 0.01,
        maxLat: centerLat + 0.01
    }
}

// Line clipping using Cohen-Sutherland algorithm
const INSIDE = 0b0000
const LEFT = 0b0001
const RIGHT = 0b0010
const BOTTOM = 0b0100
const TOP = 0b1000

function computeOutCode(x: number, y: number, bounds: { minLon: number, minLat: number, maxLon: number, maxLat: number }): number {
    let code = INSIDE

    if (x < bounds.minLon) code |= LEFT
    else if (x > bounds.maxLon) code |= RIGHT
    
    if (y < bounds.minLat) code |= BOTTOM
    else if (y > bounds.maxLat) code |= TOP

    return code
}

// Fixed line clipping using Cohen-Sutherland algorithm
function clipLineSegment(
    p1: [number, number], 
    p2: [number, number], 
    bounds: { minLon: number, minLat: number, maxLon: number, maxLat: number }
): [number, number][] | null {
    let [x0, y0] = p1;
    let [x1, y1] = p2;
    
    let code0 = computeOutCode(x0, y0, bounds);
    let code1 = computeOutCode(x1, y1, bounds);
    let accept = false;
    
    while (true) {
        if (!(code0 | code1)) {
            // Both points inside the clip window
            accept = true;
            break;
        } else if (code0 & code1) {
            // Both points share an outside zone -> completely outside
            break;
        } else {
            // Some segment of the line may be inside
            let x = 0, y = 0;
            const codeOut = code0 ? code0 : code1;
            const slope = (y1 - y0) / (x1 - x0)

            // this check might be redundant
            // but idgaf it works so im NOT touching it
            if (code0) {
                if (codeOut & TOP) {
                    // Point is above the clip window
                    x = x0 + (bounds.maxLat - y0) / slope;
                    y = bounds.maxLat;
                } else if (codeOut & BOTTOM) {
                    // Point is below the clip window
                    x = x0 + (bounds.minLat - y0) / slope;
                    y = bounds.minLat;
                } else if (codeOut & RIGHT) {
                    // Point is to the right of clip window
                    y = y0 + slope * (bounds.maxLon - x0);
                    x = bounds.maxLon;
                } else if (codeOut & LEFT) {
                    // Point is to the left of clip window
                    y = y0 + slope * (bounds.minLon - x0);
                    x = bounds.minLon;
                }
            } else if (code1) {
                if (codeOut & TOP) {
                    // Point is above the clip window
                    x = x1 + (bounds.maxLat - y1) / slope;
                    y = bounds.maxLat;
                } else if (codeOut & BOTTOM) {
                    // Point is below the clip window
                    x = x1 + (bounds.minLat - y1) / slope;
                    y = bounds.minLat;
                } else if (codeOut & RIGHT) {
                    // Point is to the right of clip window
                    y = y1 + slope * (bounds.maxLon - x1);
                    x = bounds.maxLon;
                } else if (codeOut & LEFT) {
                    // Point is to the left of clip window
                    y = y1 + slope * (bounds.minLon - x1);
                    x = bounds.minLon;
                }
            }
            
            // Replace the outside point with the intersection point
            if (codeOut === code0) {
                x0 = x;
                y0 = y;
                code0 = computeOutCode(x0, y0, bounds);
            } else {
                x1 = x;
                y1 = y;
                code1 = computeOutCode(x1, y1, bounds);
            }
        }
    }
    
    if (accept) {
        // Ensure points are exactly on boundaries when they should be
        if (Math.abs(x0 - bounds.minLon) < 1e-10) x0 = bounds.minLon;
        if (Math.abs(x0 - bounds.maxLon) < 1e-10) x0 = bounds.maxLon;
        if (Math.abs(y0 - bounds.minLat) < 1e-10) y0 = bounds.minLat;
        if (Math.abs(y0 - bounds.maxLat) < 1e-10) y0 = bounds.maxLat;
        
        if (Math.abs(x1 - bounds.minLon) < 1e-10) x1 = bounds.minLon;
        if (Math.abs(x1 - bounds.maxLon) < 1e-10) x1 = bounds.maxLon;
        if (Math.abs(y1 - bounds.minLat) < 1e-10) y1 = bounds.minLat;
        if (Math.abs(y1 - bounds.maxLat) < 1e-10) y1 = bounds.maxLat;
        
        return [[x0, y0], [x1, y1]];
    }
    
    return null;
}


type Feature = {
    type: "Feature",
    id: string,
    properties: {
        type: "road" | "ferry" | "train",
        roadType: string,
        leftLanes: number,
        rightLanes: number,
        shoulderSpaceLeft: number,
        shoulderSpaceRight: number,
        hidden: boolean,
        secret: boolean,
        lookToken: string,
        dlcGuard: number,
        startNodeUid: string,
        endNodeUid: string
    },
    geometry: {
        type: "LineString",
        coordinates: Array<Array<number>> // lon, lat
    }
}

async function main() {
    console.log("Loading geojson...")
    const { features }: { features: Feature[] } = JSON.parse(fs.readFileSync("./ets2-scaled.geojson", "utf-8"))

    const tileData: { [key: number]: TileLine } = {}

    console.log("Parsing...")
    for (let feature of features) {
        if (feature.type != "Feature") {
            console.warn("Unexpected feature type:", feature)
            continue
        }

        if (feature.geometry.type != "LineString" && feature.geometry.type != "Point") {
            // console.log(feature)
        }
        if (feature.geometry.type != "LineString") continue
        // if (feature.properties.type == "ferry" || feature.properties.type == "train") continue

        const lineName = randomUUID()
        const coordinates = feature.geometry.coordinates
        var roadType = 7
        if (feature.properties.type == "ferry") roadType = 14
        else if (feature.properties.type == "train") roadType = 12
        else switch (feature.properties.roadType) {
            case "freeway":
                roadType = 1
                break;
            case "divided":
            case "local":
                roadType = 7
                break;
            case "train":
                roadType = 12
                break;
            case "no_vehicles":
                roadType = 8
                break;
            case "unknown":
            default:
                continue // skip unknown or other types of roads
        }

        // Process each segment of the LineString
        for (let j = 0; j < coordinates.length - 1; j++) {
            const startCoord = coordinates[j]
            const endCoord = coordinates[j + 1]
            
            const startLon = startCoord[0], startLat = startCoord[1]
            const endLon = endCoord[0], endLat = endCoord[1]
            
            // Get all tiles this segment might intersect with
            const startTileId = getTileId(startLat, startLon)
            const endTileId = getTileId(endLat, endLon)
            
            const tilesToCheck = new Set<number>([startTileId, endTileId])
            
            // Simple approach: check all tiles along the bounding box
            const minLat = Math.min(startLat, endLat)
            const maxLat = Math.max(startLat, endLat)
            const minLon = Math.min(startLon, endLon)
            const maxLon = Math.max(startLon, endLon)
            
            const minTileLat = Math.floor(minLat * 100) + 9000
            const maxTileLat = Math.floor(maxLat * 100) + 9000
            const minTileLon = Math.floor(minLon * 100) + 18000
            const maxTileLon = Math.floor(maxLon * 100) + 18000
            
            for (let lonIdx = minTileLon; lonIdx <= maxTileLon; lonIdx++) {
                for (let latIdx = minTileLat; latIdx <= maxTileLat; latIdx++) {
                    const tileId = lonIdx * 18000 + latIdx
                    tilesToCheck.add(tileId)
                }
            }
            
            // Process each potential tile
            for (const tileId of tilesToCheck) {
                const tileBounds = getTileBounds(tileId)
                const clippedSegment = clipLineSegment(
                    [startLon, startLat],
                    [endLon, endLat],
                    tileBounds
                )
                
                if (clippedSegment) {
                    if (!tileData[tileId]) {
                        tileData[tileId] = {
                            points: [],
                            lines: {}
                        }
                    }
                    
                    if (!tileData[tileId].lines[lineName]) {
                        tileData[tileId].lines[lineName] = {
                            points: [],
                            type: roadType
                        }
                    }
                    
                    const tileCenter = getTileCenter(tileId)
                    const pointIndices: number[] = []
                    
                    // Add both points of the clipped segment
                    for (const coord of clippedSegment) {
                        const point = toTileCoordinatesRaw(coord[0], coord[1], tileCenter)
                        if (point.x < 0 || point.y < 0) console.log(point)
                        // Check if point already exists in this tile to avoid duplicates
                        let existingIndex = -1
                        for (let idx = 0; idx < tileData[tileId].points.length; idx++) {
                            const existingPoint = tileData[tileId].points[idx]
                            if (existingPoint.x === point.x && existingPoint.y === point.y) {
                                existingIndex = idx
                                break
                            }
                        }
                        
                        if (existingIndex === -1) {
                            tileData[tileId].points.push(point)
                            pointIndices.push(tileData[tileId].points.length - 1)
                        } else {
                            pointIndices.push(existingIndex)
                        }
                    }
                    
                    // Add the line segment (only if we have two distinct points)
                    if (pointIndices.length === 2 && pointIndices[0] !== pointIndices[1]) {
                        tileData[tileId].lines[lineName].points.push(...pointIndices)
                    }
                }
            }
        }
    }

    console.log("Saving...")
    fs.writeFileSync("./test/test.json", JSON.stringify(tileData))
    console.log(`Processed ${Object.keys(tileData).length} tiles`)
}

main()