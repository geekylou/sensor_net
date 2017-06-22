import zmq
import time
import sys
print sys.argv[1:]

# ZeroMQ Context
context = zmq.Context()

sock_live = context.socket(zmq.PUB)
sock_live.connect("tcp://"+sys.argv[1])

time.sleep(1)
# Send multipart only allows send byte arrays, so we convert everything to strings before sending
# [TODO] add .encode('UTF-8') when we switch to python3.
sock_live.send_multipart(["set-address",'pair',sys.argv[2],"0"])
sock_live.close()
