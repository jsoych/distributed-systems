import os
import time

time.sleep(1)
with open("test.txt", "w") as f:
    f.write("hi there, you got this\n")
    f.write(os.getcwd())
