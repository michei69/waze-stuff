import express from "express"
import axios from "axios"
import protobuf from "protobufjs"
import fs from "fs"
import https from "https"
import path from "path"
import { exec } from "child_process"
import { randomUUID } from "crypto"
import Fuse, { FuseResult } from "fuse.js"
import { handleCommand, initializeRTHandler, searchAutocomplete } from "./RealtimeCommandHandler"

const promisify = <T>(fn: any): Promise<T> => new Promise((resolve, reject) => {
    try {
        resolve(fn())
    } catch (e) {
        reject(e)
    }
})

const processOldCommand = (str: string) => str
    // .replaceAll("UpdateConfig,preferences,Tiles3,Is priority queue for pending tiles Enabled,yes", "UpdateConfig,preferences,Tiles3,Is priority queue for pending tiles Enabled,no")
    /// .replaceAll("UpdateConfig,preferences,Tiles3,Batch Size,20", "UpdateConfig,preferences,Tiles3,Batch Size,1")
    // .replaceAll("UpdateConfig,preferences,Download,Tiles V3,https://ctilesgcs-row.waze.com/TileServer/multi-get", "UpdateConfig,preferences,Download,Tiles V3,")
    // .replaceAll("UpdateConfig,preferences,Map,Tile Protobuf enabled,yes", "UpdateConfig,preferences,Map,Tile Protobuf enabled,no")

async function main() {
    const app = express()
    app.use(express.text({ type: "*/*" }))
    
    const root = new protobuf.Root()
    root.resolvePath = (origin, target) => {
        return path.resolve(".", "protos", target)
    }

    const Batch: protobuf.Type = root.loadSync("linqmap/proto/rt/Container.proto").lookupType("linqmap.proto.rt.Batch")

    initializeRTHandler()

    app.post("/rtserver/distrib/login", async (req, res) => {
        const { body } = req
        const temp = await axios.post("https://rt.waze.com/rtserver/distrib/login", body, {
            headers: {
                "X-Waze-Network-Version": "3",
                "Sequence-Number": "1",
                "X-Waze-Wait-Timeout": "3500"
            },
            responseType: "arraybuffer"
        })
        const retData = temp.data
        const parsed: any = Batch.decode(retData)
        for (const element of parsed.element) {
            if ("oldCommand" in element) {
                element.oldCommand = processOldCommand(element.oldCommand)
            }
        }

        res.setHeader("Content-Type", "application/x-protobuf")
        res.setHeader("x-rt-name", "4099")
        res.setHeader("via", "1.1 google")
        res.send(Batch.encode(parsed).finish())
    })
    app.post("/rtserver/distrib/command", async (req, res) => {
        var { body }: { body: string } = req

        const temp = await axios.post("https://rt-xlb-row.waze.com/rtserver/distrib/command", body, {
            headers: {
                "X-Waze-Network-Version": "3",
                "Sequence-Number": "1",
                "X-Waze-Wait-Timeout": "3500",
                "UID": req.headers.uid
            },
            responseType: "arraybuffer"
        })
        const data = temp.data

        const resp = await handleCommand(body)

        if (body.startsWith("ProtoBase64,")) {
            body = body.split(",")[1]
            const parsed = Batch.decode(Buffer.from(body, "base64"));
            try {
                const parsedResp = Batch.decode(data)
        
                console.log("====> IN")
                console.dir(parsed.toJSON(), { depth: null })
                console.dir(parsedResp.toJSON(), { depth: null })
                console.log("<==== OUT")
            } catch (_) {
                console.log("====> IN")
                console.dir(parsed.toJSON(), { depth: null })
                console.log("<==== OUT")
            }
        }
        if (resp) {
            res.send(resp)
        } else {
            res.send(data)
        }
    })

    var nextTileId = -1
    app.get("/TileServer/multi-get", async (req, res) => {
        const q = new URLSearchParams(req.query as any)
        console.log(`https://ctilesgcs-row.waze.com/TileServer/multi-get?${q.toString()}`)
        const temp = await axios.get(`https://ctilesgcs-row.waze.com/TileServer/multi-get?${q.toString()}`, {
            responseType: "arraybuffer",
            headers: {
                "User-Agent": "5.8.0.4"
            }
        })
        const data = temp.data
        const uuid = randomUUID()
        fs.writeFileSync(`./temp/${uuid}.map_pkg`, data)

        console.log(await new Promise((resolve, reject) => {
            exec(`python .\\scripts\\modify_map_pkg.py .\\temp\\${uuid}.map_pkg`, (error, stdout, stderr) => {
                if (error) {
                    console.error(`exec error: ${error}`)
                    reject(error)
                    return
                }
                resolve(stdout)
            })
        }))

        const new_data = fs.readFileSync(`./temp/${uuid}.map_pkg`)
        fs.rmSync(`./temp/${uuid}.map_pkg`)
        res.send(new_data)
    })

    app.get("/autocomplete/q", async (req, res) => {
        const { q, lang, sll } = req.query // sll = current location (lat, lon)
        res.json(await searchAutocomplete(q as string))
    })

    https.createServer({
        cert: fs.readFileSync("server.crt"),
        key: fs.readFileSync("server.key"),
    }, app).listen(3001, () => {
        console.log("Listening on port 3001")
    })
}
main()