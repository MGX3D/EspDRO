#
# Sample Python Reader for #EspDRO
#
import json
import asyncio
import websockets

async def espdro_client():
        wsc = await websockets.connect("ws://espdro:81")
        while True:
            reading = await wsc.recv()
            if __debug__: print("EspDRO: {}".format(reading))
            if reading.startswith("{"):
                jreading = json.loads(reading)
                print("  x={:,d}um time={:d}".format(jreading['axis0'], jreading['ts']))

# Python 3.7+: asyncio.run(espdro_client())
asyncio.get_event_loop().run_until_complete(espdro_client())
