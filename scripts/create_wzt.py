def create_wzt(tile_id: str):
    STR0 = b"\x00"
    STR1 = b"\x00\x00\x00\x00Street\x00"
    STR2 = b"\x00"
    STR3 = b"\x00\x00\x00\x00"
    STR4 = b"\x00\x00\x00\x00"
    STR5 = b"\x00\x00\x00\x00City\x00"
    STR6 = b"\x00\x00\x00\x00\x56\x65\x72\x73\x69\x6F\x6E\x00\x44\x61\x74\x65\x00\x46\x72\x69\x20\x4A\x75\x6E\x20\x32\x37\x20\x30\x38\x3A\x32\x39\x3A\x30\x30\x20\x32\x30\x32\x35\x0A\x00\x55\x6E\x69\x78\x54\x69\x6D\x65\x00\x31\x37\x35\x31\x30\x31\x32\x39\x34\x30\x00\x42\x75\x69\x6C\x64\x65\x72\x00\x31\x2E\x31\x38\x2E\x32\x35\x00"
    STR7 = b"\x00" + tile_id.encode("utf-8")# + b"\x00"

    SHAPE = b"\x00\x00\x00\x00"
    LINEDATA = b"\x00\x00\x01\x00\xff\xff\x00\x00\x01\x00\x02\x00\xff\xff\x00\x00"
    LINESUMMARY = b"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    BROKEN = b"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    LINEROUNDABOUT = b"\x00\x00"
    POINTDATA = b"\x00\x00\x00\x00\x8d\x20\xDE\x14\x10\x27\x10\x27\x00\x00\x10\x27\x10\x27\x00\x00"
    POINTID = b"\xfe\xff\xff\xff\xfe\xff\xff\xff\xfe\xff\xff\xff"
    LINEROUTE = b"\x01\x01\x01\x01\x01\x01\x01\x01"
    STREETNAME = b"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    STREETCITY = b"\x00\x00\x00\x00"

    POLYGONHEAD = b"\x00\x00\x04\x00\x01\x00\x10\x00\x10\x27\x00\x00\x10\x27\x00\x00"
    POLYGONPOINT = b"\x02\x00\x04\x00\x00\x00\x03\x00"
    LINEREF = b""
    LINESPEEDAVG = b"\x00\x00\x00\x00"
    LINESPEEDID = b""
    LINESPEEDIDSMALL = b""

    RANGE = b"\x00\x00\x00\x00\x00\x00"
    ALERTDATA = b""
    SQUAREDATA = int(tile_id).to_bytes(4, "little") + b"\x00\x00\x00\x00\x3F\x32\x5E\x68"
    METADATAATTRIBUTE = b"\x01\x00\x09\x00\x0E\x00\x00\x00\x01\x00\x28\x00\x31\x00\x00\x00\x01\x00\x3C\x00\x44\x00\x00\x00"
    VENUEHEAD = b""
    VENUEID = b""

    LINEEXTTYPE = b"\x05\x07"
    LINEEXTID = b"\x00\x00\x00\x00\x00\x00\x00\x00"
    LINEEXTLEVELBYLINE = b"\x00\x00"
    LINEEXTLEVEL = b"\x00"
    LINESPEEDMAX = b"\x00\x00\x00\x32"

    POLYGONEX = b"\x00\x00\x00\x00"
    LINEATTRIBUTE = b"\x20\x00\x00\x00\x20\x00\x00\x00"
    STREETID = b"\x20\x4A\x96\x02"

    BEACONPOS = b""
    BEACONID = b""
    BEACONEXPOS = b""
    BEACONEXID = b""
    BEACONEXMASK = b""

    LANETYPE = b"\x00\x00"
    LINENEWTYPE = b""
    EXTPROTOBUFDATA = b""

    def check(raw: bytes, size: int):
        assert len(raw) % size == 0

    check(LINEDATA, 8)
    check(LINESUMMARY, 62)
    check(BROKEN, 2)
    check(LINEROUNDABOUT, 2)
    check(POINTDATA, 4)
    check(POINTID, 4)
    check(SHAPE, 4)
    check(LINEROUTE, 4)
    check(STREETNAME, 10)
    check(STREETCITY, 4)
    check(STREETID, 4)
    check(POLYGONHEAD, 16)
    check(POLYGONPOINT, 2)
    check(LINESPEEDAVG, 2)
    check(LINEREF, 4)
    check(RANGE, 6)
    # check(ALERTDATA, 16)
    check(SQUAREDATA, 12)
    check(METADATAATTRIBUTE, 8)
    check(VENUEHEAD, 16)
    check(VENUEID, 2)
    check(LINEEXTTYPE, 1)
    check(LINEEXTID, 4)
    check(LINEEXTLEVELBYLINE, 1)
    check(LINEEXTLEVEL, 1)
    check(LINESPEEDMAX, 2)
    check(LINEATTRIBUTE, 4)
    check(LANETYPE, 1)
    check(LINENEWTYPE, 1)
    check(POLYGONEX, 4)
    check(BEACONPOS, 4)
    check(BEACONID, 16)
    check(BEACONEXPOS, 4)
    check(BEACONEXID, 16)
    check(BEACONEXMASK, 1)
    check(EXTPROTOBUFDATA, 1)
    check(LINESPEEDID, 4)
    check(LINESPEEDIDSMALL, 2)
    check(LINEROUNDABOUT, 2)

    with open(f"temp/{tile_id}.wzt", "wb") as f:
        f.write(b"\x2e\x00\x00\x00")
        f.write(b"\x00\x00\x00\x00")
        data = b""
        ln = 0
        count = 0

        def add(thing: bytes):
            nonlocal data, ln, count
            data += thing
            ln += len(thing)
            f.write(ln.to_bytes(4, "little"))
            count += 1
            # print(f"{count}/46")

        add(STR0)
        add(STR1)
        add(STR2)
        add(STR3)
        add(STR4)
        add(STR5)
        add(STR6)
        add(STR7)
        
        add(SHAPE)
        add(LINEDATA)
        add(LINESUMMARY)
        add(BROKEN)
        add(LINEROUNDABOUT)
        add(POINTDATA)
        add(POINTID)
        add(LINEROUTE)
        add(STREETNAME)
        add(STREETCITY)

        add(POLYGONHEAD)
        add(POLYGONPOINT)
        add(LINEREF)
        add(LINESPEEDAVG)
        add(LINESPEEDID)
        add(LINESPEEDIDSMALL)

        add(RANGE)
        add(ALERTDATA)
        add(SQUAREDATA)
        add(METADATAATTRIBUTE)
        add(VENUEHEAD)
        add(VENUEID)

        add(LINEEXTTYPE)
        add(LINEEXTID)
        add(LINEEXTLEVELBYLINE)
        add(LINEEXTLEVEL)
        add(LINESPEEDMAX)

        add(POLYGONEX)
        add(LINEATTRIBUTE)
        add(STREETID)

        add(BEACONPOS)
        add(BEACONID)
        add(BEACONEXPOS)
        add(BEACONEXID)
        add(BEACONEXMASK)

        add(LANETYPE)
        add(LINENEWTYPE)
        add(EXTPROTOBUFDATA)

        f.write(data)