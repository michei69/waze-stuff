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
        type: "LineString" | "Point",
        coordinates: Array<Array<number>> // lon, lat
    }
}

async function main() {
    console.log("reading")
    const data: { features: Feature[] } = await Bun.file("./ets2-search.geojson").json()

    const scale = 1/19

    console.log("scaling")
    for (let feature of data.features) {
        try {
            if (feature.geometry.type == "Point") {
                //@ts-ignore icba to actually type
                feature.geometry.coordinates = [feature.geometry.coordinates[0] * scale, feature.geometry.coordinates[1] * scale]
            } else if (feature.geometry.type == "LineString") {
                feature.geometry.coordinates = feature.geometry.coordinates.map(([lon, lat]) => [lon * scale, lat * scale])
            } else {
                //@ts-ignore icba to actually type
                feature.geometry.coordinates = feature.geometry.coordinates.map((array) => array.map(([lon, lat]) => [lon * scale, lat * scale]))
            }
        } catch (e) {
            console.error(e, feature)
            exit(1)
        }
    }

    console.log("writing")
    await Bun.write("./ets2-search-scaled.geojson", JSON.stringify(data))
    console.log("done")
}
main()