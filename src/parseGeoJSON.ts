import fs from "fs"
import { randomUUID } from "crypto"
import { SingleBar } from "cli-progress"
import { clipPolyline } from "lineclip"

interface Point {
    x: number;
    y: number;
}

interface TileLine {
    points: Point[];
    lines: { [key: string]: { points: number[], type: number } };
    polygons: { [key: string]: { points: number[], type: number, north: number, west: number, south: number, east: number } };
}

var ScaleData: Array<{ scale_factor: number, tile_size: number, base_index: number, num_rows: number, num_cols: number }> = []
var MAX_SCALE = 0;

function getTileId(lat: number, lon: number, scale: number = 0) {
    lat *= 100
    lon *= 100
    lat = Math.floor((Math.floor(lat) + 9000) / ScaleData[scale].scale_factor)
    lon = Math.floor((Math.floor(lon) + 18000) / ScaleData[scale].scale_factor)
    return lon * ScaleData[scale].num_rows + lat + ScaleData[scale].base_index
}

function getTileScale(tileId: number) {
    let scale = 0
    for (let i = 0; i <= MAX_SCALE; i++) {
        if (tileId < ScaleData[i].base_index) {
            scale = i - 1
            break
        }
    }
    return scale
}

function getTileCenter(tileId: number): [number, number] {
    const scale = getTileScale(tileId)
    tileId = tileId - ScaleData[scale].base_index

    const lat = ((tileId % ScaleData[scale].num_rows) * ScaleData[scale].scale_factor - 9000) / 100
    const lon = (Math.floor(tileId / ScaleData[scale].num_rows) * ScaleData[scale].scale_factor - 18000) / 100
    return [lon, lat]
}

function toTileCoordinatesRaw(lon: number, lat: number, tileCenter: [number, number], scale: number = 0): Point {
    return {
        x: Math.floor((lon - tileCenter[0]) * 1_000_000 / ScaleData[scale].scale_factor),
        y: Math.floor((lat - tileCenter[1]) * 1_000_000 / ScaleData[scale].scale_factor)
    }
}

function getTileBounds(tileId: number): { minLon: number, minLat: number, maxLon: number, maxLat: number } {
    const scale = getTileScale(tileId)
    const [centerLon, centerLat] = getTileCenter(tileId)
    return {
        minLon: centerLon,
        minLat: centerLat,
        maxLon: centerLon + 0.01 * ScaleData[scale].scale_factor,
        maxLat: centerLat + 0.01 * ScaleData[scale].scale_factor
    }
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
        endNodeUid: string,
        color?: number
    },
    geometry: {
        type: "LineString"|"Polygon",
        coordinates: Array<Array<number>> // lon, lat
    }
}

function generateScales() {
    const SCALE_STEP = 4;
    const SQUARE_SIZE = 10_000;
    const MAX_SQUARE_SIZE = 30_000_000;
    let _size = SQUARE_SIZE;
    let _scale = 0;
    while (_size <= MAX_SQUARE_SIZE) {
        _scale += 1;
        _size *= SCALE_STEP;
    }
    MAX_SCALE = _scale - 1;
    ScaleData = [];
    let _base = 0;
    let _factor = 1;

    for (let _scale = 0; _scale <= MAX_SCALE; _scale++) {
        ScaleData[_scale] = {
            scale_factor: _factor,
            tile_size: _factor * SQUARE_SIZE,
            base_index: _base,
            num_rows: Math.floor(179999999 / (_factor * SQUARE_SIZE)) + 1,
            num_cols: Math.floor(359999999 / (_factor * SQUARE_SIZE)) + 1
        };
        _base += ScaleData[_scale].num_rows * ScaleData[_scale].num_cols;
        _factor *= SCALE_STEP;
    }
}
generateScales()

async function main() {
    console.log("Loading geojson...")
    const { features }: { features: Feature[] } = JSON.parse(fs.readFileSync("./ets2-scaled.geojson", "utf-8"))

    const tileData: { [key: number]: TileLine } = {}

    console.log("Parsing...")
    for (let scaleId = 0; scaleId < 2; scaleId++) {
        console.log("GC...")
        Bun.gc(true)
        console.log(`Parsing for scale ${scaleId}...`)
        const progress = new SingleBar({
            etaBuffer: 1000,
            fps: 1
        })
        progress.start(features.length, 0)
        let i = 0
        for (let feature of features) {
            i++
            progress.increment()
            // if (i % 50 == 0) {
            //     console.log(i, "/", features.length)
            // }
            if (feature.type != "Feature") {
                console.warn("Unexpected feature type:", feature)
                continue
            }

            if (feature.geometry.type == "Polygon") {
                const polygonName = randomUUID()
                const coordinates = feature.geometry.coordinates[0]
                const polygonType = feature.properties.color // TODO
                
                // Process each segment of the Polygon
                for (let j = 0; j < coordinates.length - 1; j++) {
                    const startCoord = coordinates[j]
                    const endCoord = coordinates[j + 1]
                    
                    const startLon = startCoord[0], startLat = startCoord[1]
                    const endLon = endCoord[0], endLat = endCoord[1]
                    
                    // Get all tiles this segment might intersect with
                    const startTileId = getTileId(startLat, startLon, scaleId)
                    const endTileId = getTileId(endLat, endLon, scaleId)
                    
                    const tilesToCheck = new Set<number>([startTileId, endTileId])
                    
                    // Simple approach: check all tiles along the bounding box
                    const minLat = Math.min(startLat, endLat)
                    const maxLat = Math.max(startLat, endLat)
                    const minLon = Math.min(startLon, endLon)
                    const maxLon = Math.max(startLon, endLon)
                    const minTileLat = Math.floor((Math.floor(minLat * 100) + 9000) / ScaleData[scaleId].scale_factor)
                    const maxTileLat = Math.floor((Math.floor(maxLat * 100) + 9000) / ScaleData[scaleId].scale_factor)
                    const minTileLon = Math.floor((Math.floor(minLon * 100) + 18000) / ScaleData[scaleId].scale_factor)
                    const maxTileLon = Math.floor((Math.floor(maxLon * 100) + 18000) / ScaleData[scaleId].scale_factor)
                    
                    for (let lonIdx = minTileLon; lonIdx <= maxTileLon; lonIdx++) {
                        for (let latIdx = minTileLat; latIdx <= maxTileLat; latIdx++) {
                            const tileId = lonIdx * ScaleData[scaleId].num_rows + latIdx + ScaleData[scaleId].base_index
                            if (tileId != Math.floor(tileId)) {
                                console.log(tileId, scaleId, lonIdx, latIdx)
                            }
                            tilesToCheck.add(tileId)
                        }
                    }
                    
                    // Process each potential tile
                    for (const tileId of tilesToCheck) {
                        const tileBounds = getTileBounds(tileId)
                        const clippedSegment = clipPolyline([[startLon, startLat], [endLon, endLat]], [tileBounds.minLon, tileBounds.minLat, tileBounds.maxLon, tileBounds.maxLat])[0]
                        
                        var north = 0, west = 0, south = 10000, east = 10000

                        if (clippedSegment) {
                            if (!tileData[tileId]) {
                                tileData[tileId] = {
                                    points: [],
                                    lines: {},
                                    polygons: {}
                                }
                            }
                            
                            if (!tileData[tileId].polygons[polygonName]) {
                                tileData[tileId].polygons[polygonName] = {
                                    points: [],
                                    type: polygonType,
                                    north: 0,
                                    west: 0,
                                    south: 10000,
                                    east: 10000
                                }
                            }
                            
                            const tileCenter = getTileCenter(tileId)
                            const pointIndices: Set<number> = new Set()
                            
                            // Add all points of the clipped segment
                            for (const coord of clippedSegment) {
                                const point = toTileCoordinatesRaw(coord[0], coord[1], tileCenter, scaleId)
                                if (point.x < 0 || point.y < 0) console.log(point)

                                if (point.x < east) east = point.x
                                if (point.x > west) west = point.x
                                if (point.y < south) south = point.y
                                if (point.y > north) north = point.y

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
                                    pointIndices.add(tileData[tileId].points.length - 1)
                                } else {
                                    pointIndices.add(existingIndex)
                                }
                            }
                            
                            tileData[tileId].polygons[polygonName].points.push(...pointIndices)
                            tileData[tileId].polygons[polygonName].north = north
                            tileData[tileId].polygons[polygonName].west = west
                            tileData[tileId].polygons[polygonName].south = south
                            tileData[tileId].polygons[polygonName].east = east
                        }
                    }
                    // }
                }
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
                
                // for (let scaleId = 0; scaleId < MAX_SCALE; scaleId++) {
                // Get all tiles this segment might intersect with
                const startTileId = getTileId(startLat, startLon, scaleId)
                const endTileId = getTileId(endLat, endLon, scaleId)
                
                const tilesToCheck = new Set<number>([startTileId, endTileId])
                
                // Simple approach: check all tiles along the bounding box
                const minLat = Math.min(startLat, endLat)
                const maxLat = Math.max(startLat, endLat)
                const minLon = Math.min(startLon, endLon)
                const maxLon = Math.max(startLon, endLon)
                const minTileLat = Math.floor((Math.floor(minLat * 100) + 9000) / ScaleData[scaleId].scale_factor)
                const maxTileLat = Math.floor((Math.floor(maxLat * 100) + 9000) / ScaleData[scaleId].scale_factor)
                const minTileLon = Math.floor((Math.floor(minLon * 100) + 18000) / ScaleData[scaleId].scale_factor)
                const maxTileLon = Math.floor((Math.floor(maxLon * 100) + 18000) / ScaleData[scaleId].scale_factor)
                
                for (let lonIdx = minTileLon; lonIdx <= maxTileLon; lonIdx++) {
                    for (let latIdx = minTileLat; latIdx <= maxTileLat; latIdx++) {
                        const tileId = lonIdx * ScaleData[scaleId].num_rows + latIdx + ScaleData[scaleId].base_index
                        if (tileId != Math.floor(tileId)) {
                            console.log(tileId, scaleId, lonIdx, latIdx)
                        }
                        tilesToCheck.add(tileId)
                    }
                }
                
                // Process each potential tile
                for (const tileId of tilesToCheck) {
                    const tileBounds = getTileBounds(tileId)
                    const clippedSegment = clipPolyline([[startLon, startLat], [endLon, endLat]], [tileBounds.minLon, tileBounds.minLat, tileBounds.maxLon, tileBounds.maxLat])[0]
                    // clipLineSegment(
                    //     [startLon, startLat],
                    //     [endLon, endLat],
                    //     tileBounds
                    // )
                    
                    if (clippedSegment) {
                        if (!tileData[tileId]) {
                            tileData[tileId] = {
                                points: [],
                                lines: {},
                                polygons: {}
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
                            const point = toTileCoordinatesRaw(coord[0], coord[1], tileCenter, scaleId)
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
                // }
            }
        }
        progress.stop()
    }

    console.log("Saving...")
    fs.writeFileSync("./test/test.json", JSON.stringify(tileData))
    console.log(`Processed ${Object.keys(tileData).length} tiles`)
}

main()