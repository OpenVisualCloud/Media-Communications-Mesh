from payload import Audio, Video

payload = Video(
    width = 3840,
    height = 2160
)

print(f"""payload info:
width={payload.width}
height={payload.height}
fps={payload.fps}
pixelFormat={payload.pixelFormat}
type={payload.payload_type}
""")

print(f"""Dict:
{payload.toDict()}
""")


pl = Audio(
    channels=4
)

print(f"""pl info:
channels={pl.channels}
type={pl.payload_type}
""")

print(f"""Dict:
{pl.toDict()}
""")
