import os
import json
import socket
import time

MANAGER = os.getenv('MANAGER_SOCKET', '/Users/leejahsprock/pyoneer/run/manager3.socket')
BUFSIZE = 1024

sock = socket.socket(socket.AF_UNIX)
sock.connect(MANAGER)

cmd = json.dumps({"command": "get_status"})
sock.send(cmd.encode())
print(json.loads(sock.recv(BUFSIZE)))

cmd = json.dumps({"command": "get_project_status"})
sock.send(cmd.encode())
print(json.loads(sock.recv(BUFSIZE)))

cmd = json.dumps({"command": "run_project", "project": {"id": 42, "jobs":[]}})
sock.send(cmd.encode())
print(json.loads(sock.recv(BUFSIZE)))

time.sleep(5)
cmd = json.dumps({"command": "get_project_status"})
sock.send(cmd.encode())
print(json.loads(sock.recv(BUFSIZE)))

cmd = json.dumps(
    {
        "command": "run_project",
        "project": {
            "id": 3, 
            "jobs": [
                {"job": {"id": 1, "tasks":["task.py"]}, "dependencies": []}
            ]
        }
    }
)
sock.send(cmd.encode())
print(json.loads(sock.recv(BUFSIZE)))

sock.close()
