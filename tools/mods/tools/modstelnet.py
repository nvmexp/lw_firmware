#! /usr/bin/elw python

import socket, sys

# Check arguments
if len(sys.argv) < 3:
    print("Usage: modstelnet.py <PORT> <MODS-ARGUMENTS>\n")
    print("Example: modstelnet.py 1024 gputest.js -engr")
    sys.exit(1)

# Open socket on selected port
s = socket.socket()
s.bind(("0.0.0.0", int(sys.argv[1])))
s.listen(1)

# Wait for connection
c,addr = s.accept()

# Close server socket (not needed anymore)
s.close()
del s

# Send MODS arguments
c.send(" ".join(sys.argv[2:]) + "\n")

# Receive MODS log and print it to stdout
while True:
    buf = c.recv(4096)
    if len(buf) == 0:
        break
    sys.stdout.write(buf)

# Close socket
c.close()
