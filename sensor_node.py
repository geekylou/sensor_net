import json
import zmq
import tft_lib
import time
import struct
import urllib2
import threading
import sys
#tft.SendTime(0)

global internal_temp
global external_temp
global presure
global running 
running = True
#wrote_log = False
#tempreture_log_file = open('tempreture_log', 'a', 0)

# ZeroMQ Context
context = zmq.Context()

sock_live = context.socket(zmq.PUB)
sock_live.bind("tcp://*:10900")

sock_sub = context.socket(zmq.SUB)
sock_sub.bind("tcp://*:10901")
sock_sub.setsockopt(zmq.SUBSCRIBE, '')

def radio_thread():
  min=-1
  tft = tft_lib.TFT_FastBus()
  thread_context = zmq.Context()
  sock_send = thread_context.socket(zmq.PUB)
  sock_send.connect("tcp://127.0.0.1:10901")
  while running:
    if min != time.gmtime().tm_min:
        internal_temp = -1
        external_temp = -1
        pressure = -1
        wrote_log = False
        min = time.gmtime().tm_min
        tft.write_radio(struct.pack("<BBBi",1,0xff,4,time.mktime(time.localtime())))
    values = tft.read()
    sock_send.send_multipart([str(i) for i in values])
    #print values
    
    if values[0] == 1:
        if values[2] == 1 and internal_temp == -1:
            print "Internal temp", values[3]
            internal_temp = values[3]
        if values[2] == 3 and pressure == -1:
            print "Pressure", values[3]
            pressure = values[3]
    elif values[0] == 3:
        if values[2] == 2 and external_temp == -1:
            print "External temp", values[3]
            external_temp = values[3]
        
    #if pressure >= 0 and internal_temp >= 0 and external_temp >= 0 and wrote_log == False:
    #    tempreture_log_file.write(str(time.time())+','+str(pressure)+','+str(internal_temp)+','+str(external_temp)+'\n')
    #    wrote_log = True
    
    if values[0] == 3 and values[2] == 5:
        print urllib2.urlopen("http://house-nas/lights/toggle?light=0").read()
  sock_send.close()
t = threading.Thread(target=radio_thread)
t.start()
try:
  while True:
    try:
        args = sock_sub.recv_multipart()#zmq.NOBLOCK)
        print("args:",args)
        sock_live.send_multipart(args)
    except zmq.ZMQError as e:
        pass
#        print(e)
except:
    print("Shutdown, error:", sys.exc_info()[0])
    running=False
    #tempreture_log_file.close()
    sock_live.close()
    sock_sub.close()
    