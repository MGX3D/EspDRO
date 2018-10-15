import websocket
 
ws = websocket.WebSocket()
ws.connect("ws://192.168.1.8:81/")
  
while 1:
    result = ws.recv()
    print(result)
 
ws.close()
