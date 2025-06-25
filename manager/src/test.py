import os
import json
import socket

MANAGER = os.getenv('MANAGER_SOCKET', '/Users/leejahsprock/pyoneer/run/manager3.socket')
BUFFSIZE = 1024

sock = socket.socket(socket.AF_UNIX)
sock.connect(MANAGER)

cmd = json.dumps({"command": "get_status"})
sock.send(cmd.encode())
sock.recv(BUFFSIZE)
