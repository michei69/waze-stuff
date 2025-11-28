import { exit } from "process"

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
        type: "LineString" | "Point" | "Polygon",
        coordinates: Array<Array<number>> // lon, lat
    }
}

async function main() {
    console.log("reading")
    const data: { features: Feature[] } = await Bun.file("C:/Users/miki/Desktop/New Folder/generated/ets2.geojson").json()

    const scale = 1/19

    console.log("scaling")
    for (let feature of data.features) {
        try {
            if (feature.geometry.type == "Point") {
                //@ts-ignore icba to actually type
                feature.geometry.coordinates = [feature.geometry.coordinates[0] * scale, feature.geometry.coordinates[1] * scale]
            } else if (feature.geometry.type == "Polygon") {
                //@ts-ignore icba to actually type
                feature.geometry.coordinates = feature.geometry.coordinates.map(coords => coords.map(([lon, lat]) => [lon * scale, lat * scale]))
            } else {
                feature.geometry.coordinates = feature.geometry.coordinates.map(([lon, lat]) => [lon * scale, lat * scale])
            }
        } catch (e) {
            console.error(e, feature)
            exit(1)
        }
    }

    console.log("writing")
    await Bun.write("./ets2-scaled.geojson", JSON.stringify(data))
    console.log("done")
}
main()