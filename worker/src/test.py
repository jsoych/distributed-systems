import os
import json
import socket
import unittest

from status_codes import *

# Define global constants
WORKER = os.getenv('WORKER_SOCKET', '/var/run/pyoneer/worker3.socket')
BUFFSIZE = 1024

# Get worker and job status codes
worker_codes = get_worker_status_codes()
job_codes = get_job_status_codes()

class SmallTest(unittest.TestCase):
    ''' Tests the run_job command. '''

    def setUp(self):
        # Connect to the worker
        self.sock = socket.socket(socket.AF_UNIX)
        self.sock.connect(WORKER)

    def tearDown(self):
        self.sock.close()

    def send_commnad(self, command, buffsize=BUFFSIZE):
        ''' Sends the command to the worker and returns its response. '''
        obj = json.dumps(command)
        self.sock.send(obj.encode())
        data = self.sock.recv(buffsize)
        return json.loads(data)

    def test_run_job(self):
        # Send command
        com = {
            'command': 'run_job',
            'job': {
                'id': 42,
                'tasks': ['task.py']
            }
        }
        res = self.send_commnad(com)

        # Check res status codes
        self.assertIn(res['status'],worker_codes.values())
        self.assertIn(res['job_status'],job_codes.values())
    
    def test_start(self):
        self.send_commnad({'command': 'start'})
    
    def test_stop(self):
        self.send_commnad({'command': 'stop'})

    def test_get_status(self):
        res = self.send_commnad({'command': 'get_status'})
        self.assertIn(res['status'],worker_codes.values())

    def test_get_job_status(self):
        res = self.send_commnad({'command': 'get_job_status'})
        self.assertIn(res['job_status'],job_codes.values(),'accept None')

if __name__ == '__main__':
    unittest.main()