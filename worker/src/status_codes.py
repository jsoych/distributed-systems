''' Loads worker and job status codes from header files. '''

def get_worker_status_codes():
    ''' Gets worker status codes from worker.h '''
    codes = {}
    with open('worker.h', 'r') as f:
        for line in f.readlines():
            tokens = line.split()
            if (len(tokens) == 0):
                continue

            if (tokens[0] != '#define' or len(tokens) != 3):
                continue
            name = tokens[1].removeprefix('_WORKER_').lower()
            codes[name] = int(tokens[-1])
    
    return codes

def get_job_status_codes():
    ''' Gets job status codes from job.h '''
    codes = {}
    with open('job.h', 'r') as f:
        for line in f.readlines():
            tokens = line.split()
            if (len(tokens) == 0):
                continue

            if (tokens[0] != '#define' or len(tokens) != 3):
                continue
            name = tokens[1].removeprefix('_JOB_').lower()
            codes[name] = int(tokens[-1])
    
    return codes