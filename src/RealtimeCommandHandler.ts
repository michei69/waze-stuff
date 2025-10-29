import { randomUUID } from "crypto"
import Fuse, { FuseResult } from "fuse.js"
import path from "path"
import protobuf from "protobufjs"

const root = new protobuf.Root()
root.resolvePath = (origin, target) => {
    return path.resolve(".", "protos", target)
}

const Batch: protobuf.Type = root.loadSync("linqmap/proto/rt/Container.proto").lookupType("linqmap.proto.rt.Batch")
const SearchResult: protobuf.Type = root.loadSync("SearchV2.proto").lookupType("linqmap.proto.search.v2.SearchResult")
const SearchResponse: protobuf.Type = root.loadSync("SearchV2.proto").lookupType("linqmap.proto.search.v2.SearchResponse")

var SearchGeoJSON: any = null
var fuse: Fuse<SearchFeature> = null

export async function initializeRTHandler() {
    SearchGeoJSON = await Bun.file("./ets2-search-scaled.geojson").json()
    fuse = new Fuse(SearchGeoJSON.features, {
        keys: ["properties.label", "properties.city.name", "properties.stateName"],
        includeMatches: true,
        includeScore: true,
        useExtendedSearch: true,
        threshold: 0.3
    })
}

export async function handleCommand(command: string) {
    if (command.startsWith("ProtoBase64,")) {
        const parsed: any = Batch.decode(Buffer.from(command.split(",")[1], "base64"))
        
        for (let el of parsed.element) {
            if (el.getRequest) {
                if (!("venueId" in el.getRequest)) {
                    console.log(el.getRequest)
                    continue
                }
                const idx = parseInt(el.getRequest.venueId.split(".")[1])
                const searchedThing: SearchFeature = SearchGeoJSON.features[idx]

                const data = Batch.encode({
                    element: [{
                        searchResponse: {
                            id: randomUUID(),
                            displayGroup: [{
                                id: "getByID",
                                result: [{
                                    id: randomUUID(),
                                    infoUrl: "",
                                    provider: "myAss",
                                    routingContext: "",
                                    venue: {
                                        name: searchedThing.properties.label,
                                        location: {
                                            x: searchedThing.geometry.coordinates[0],
                                            y: searchedThing.geometry.coordinates[1]
                                        },
                                        city: searchedThing.properties.city ? searchedThing.properties.city.name : searchedThing.properties.label,
                                        state: searchedThing.properties.city ? searchedThing.properties.city.name : searchedThing.properties.label,
                                        country: searchedThing.properties.stateName,
                                        url: "",
                                        segmentId: "-1",
                                        venueId: "googlePlaces." + idx,
                                        nativeCategories: []
                                    }
                                }]
                            }]
                        }
                    }]
                })
                return data.finish()
            }
        }

    } else if (command.startsWith("KeepAlive")) {
        return null
    } else {
        console.log(command)
        return null
    }
    return null
}


type SearchFeature = {
    type: "Feature",
    properties: {
        dlcGuard: number,
        stateName: string,
        stateCode: string,
        city: {
            name: string,
            stateCode: string,
            distance: number,
        },
        type: 'company' | 'landmark' | 'viewpoint' | 'ferry' | 'train' | 'dealer' | 'city' | 'scenery',
        label: string,
        sprite: string,
        tags: string[]
    },
    geometry: {
        type: "Point",
        coordinates: [number, number]
    }
}
export async function searchAutocomplete(query: string) {
    const results: FuseResult<SearchFeature>[] = fuse.search(query, {
        limit: 10,
    })
    const data = results.map(result => [result.item.properties.label, 0, [], {
        a: `${result.item.properties.city ? result.item.properties.city?.name + ", " : ""}${result.item.properties.stateName}`,
        v: "googlePlaces." + result.refIndex,
        g: [],
        x: result.item.geometry.coordinates[0],
        y: result.item.geometry.coordinates[1],
        o: {
            r: {
                c: [0,0],
                l: [0,0],
                s: [0,0]
            },
            d: `${result.item.properties.label}, ${result.item.properties.city ? result.item.properties.city?.name + ", " : ""}${result.item.properties.stateName}`
        }
    }])
    return [query, data, {q: ""}]
}