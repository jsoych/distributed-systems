import os
import sys
import time

# Create output file
fd = os.open('output', os.O_WRONLY | os.O_CREAT)

# Close stdout and write its output to the output file
os.close(sys.stdout.fileno())
os.dup(fd)

# Close stderr and write its output to the output file
os.close(sys.stderr.fileno())
os.dup(fd)

os.close(fd)

time.sleep(5)
with open('file', 'w') as f:
    # Try to read from a read only file
    f.read()