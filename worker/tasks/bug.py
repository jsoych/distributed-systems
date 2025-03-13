import os
import sys
import time

# Create output file
fd = os.open(os.devnull, os.O_WRONLY)

# Close stdout and write its output to the output file
os.close(sys.stdout.fileno())
os.dup(fd)

# Close stderr and write its output to the output file
os.close(sys.stderr.fileno())
os.dup(fd)

os.close(fd)

time.sleep(10)

# Divide by zero error
1/0