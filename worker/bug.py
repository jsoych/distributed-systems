import os
import sys
import time

# Create output file
fd = os.open('output', os.O_WRONLY | os.O_CREAT)

# Close stdout and write its output to the output file
os.close(sys.stdout.fileno())
os.dup(fd)

# Close stderr and write its output to the output file
os.close(sys.stdout.fileno())
os.dup(fd)

os.close(fd)

time.sleep(20)
with open('file', 'r') as f:
    # Try to write to a read only file
    f.write()