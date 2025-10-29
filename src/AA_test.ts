import express from "express"

const app = express()
const proc = Bun.spawn(["C:/Users/miki/AppData/Local/Android/Sdk/extras/google/auto/desktop-head-unit.exe", "-c", "C:/Users/miki/AppData/Local/Android/Sdk/extras/google/auto/config/waze-test.ini"], {
    stdin: "pipe"
})

var flushed = true
var lastTime = 0

app.use(express.json())
app.post("/update", async (req, res) => {
    const data = req.body
    console.log(data)
    if (!data) {
        res.send(400)
        return
    }
    if (!flushed || new Date().getTime() - lastTime < 1000) {
        res.send(200)
        return
    }

    if (data.id == -1) {
        const lon = data.result.data.position[0] / 19
        const lat = data.result.data.position[1] / 19
        const speed = data.result.data.speedMph * 0.44704 / 1.25
        const bearing = data.result.data.bearing

        flushed = false
        proc.stdin.write(`location ${lat} ${lon} 10 0 ${speed} ${bearing}\r\n`)
        await proc.stdin.flush()
        flushed = true
        console.log(`flushed! - ${lat} ${lon}`)
    }

    res.send(200)
})

app.listen(44114)