import express from "express"

const app = express()
const sockets: Bun.Socket[] = []
Bun.listen({
    hostname: "0.0.0.0",
    port: 44113,
    socket: {
        open(socket) {
            sockets.push(socket)
            console.log("connection from " + socket.remoteAddress)
            socket.write("0,0,0,0")
            socket.flush()
        },
        close(socket) {
            sockets.splice(sockets.indexOf(socket), 1)
            console.log("connection closed from " + socket.remoteAddress)
        },
        data(socket, data) {
            console.log(data.toString())
        }
    }
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
        for (let sock of sockets) {
            sock.write(`${lon},${lat},${speed},${bearing}`)
            sock.flush()
        }
        // sock.stdin.write(`location ${lat} ${lon} 10 0 ${speed} ${bearing}\r\n`)
        // await sock.stdin.flush()
        flushed = true
        console.log(`flushed! - ${lat} ${lon}`)
    }

    res.send(200)
})

app.listen(44114)