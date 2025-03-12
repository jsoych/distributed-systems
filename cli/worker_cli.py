import os
import cmd
import json
import socket

class PyoneerShell(cmd.Cmd):
    intro = 'Welcome to the worker shell. Type help or ? to list commands.\n'
    prompt = '(worker) '
    file = None

    def preloop(self):
        if (os.uname().sysname == 'Darwin'):
            self.workers_dir = '/Users/leejahsprock/run/pyoneer'
        elif (os.uname().sysname == 'Linux'):
            self.workers_dir = '/run/pyoneer'
        self.sock = None

    def postloop(self):
        if (self.sock):
            self.sock.close()

    def precmd(self,line):
        if (line in ['help','?','quit']):
            return line

        tokens = line.split()
        if (tokens[0] in ['get_status','run_job','start','stop']):
            # Connect to worker
            self.sock = socket.socket(socket.AF_UNIX)
            try:
                self.sock.connect(
                    os.path.join(
                        self.workers_dir, 
                        f'worker{tokens[1]}.socket' # Get id
                    )
                )
            except FileNotFoundError as e:
                print(f'{e.strerror}: {e.filename}')
                return 'help'
            
        return ' '.join(tokens)
    
    def postcmd(self,stop,line):
        if (line == 'quit'):
            return True
        
        if (self.sock):
            self.sock.close()
        return False

    def close(self):
        ''' Closes any socket connections. '''
        if (self.sock):
            self.sock.close()

    # Worker commands
    def do_get_status(self,line):
        ''' Gets the worker status. '''
        obj = json.dumps({'command': 'get_status'})
        self.sock.send(obj.encode())
        res = self.sock.recv(1024)
        print(res.decode())

    def do_run_job(self,line):
        ''' Runs the job on the worker. '''
        obj = {'command': 'run_job'}
        tokens = line.split()
        obj['job'] = {
            'id': tokens[1],
            'tasks': [task for task in tokens[2:]]
        }
        obj = json.dumps(obj)
        self.sock.send(obj.encode())
        res = self.sock.recv(1024)
        print(res.decode())

    def do_start(self,id):
        ''' Starts the job. '''
        obj = json.dumps({'command': 'start'})
        self.sock.send(obj.encode())
        res = self.sock.recv(1024)
        print(res.decode())

    def do_stop(self,id):
        ''' Stop the job. '''
        obj = json.dumps({'command': 'stop'})
        self.sock.send(obj.encode())
        res = self.sock.recv(1024)
        print(res.decode())

    def do_quit(self,line):
        ''' Quits the commandline interface. '''
        print('Quitting Time')
        self.close()
        return True

if __name__ == '__main__':
    PyoneerShell().cmdloop()