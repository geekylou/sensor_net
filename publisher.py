import zmq
import time
import sys
print sys.argv[1:]

# 1Wire bus sensor data publisher.
# ZeroMQ Context
context = zmq.Context()

sock_live = context.socket(zmq.PUB)
sock_live.connect("tcp://"+sys.argv[2])

while True:
    sensor = open("/sys/bus/w1/devices/"+sys.argv[1]+"/w1_slave")
    
    line = sensor.read().split("\n")[1]
    sensor.close()
    line = line[line.find('=')+1:]
    print(line)
    sock_live.send_multipart(["Temp/"+sys.argv[1],'*',str(1),line])
    time.sleep(1)