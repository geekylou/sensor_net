import json
import zmq
import tft_lib
import time
import struct
import urllib2
import threading
import sys
import traceback

global internal_temp
global external_temp
global presure
global running 
running        = True
thread_ended   = False
long_values    = {}

tft = tft_lib.TFT_FastBus()

# ZeroMQ Context
context = zmq.Context()

sock_live = context.socket(zmq.PUB)
sock_live.bind("tcp://*:10900")

sock_sub = context.socket(zmq.SUB)
sock_sub.bind("tcp://*:10901")
sock_sub.setsockopt(zmq.SUBSCRIBE, '')

def radio_thread():
  global thread_ended

  long_names={}
  min=-1
  sec=-1
  thread_context = zmq.Context()
  sock_send = thread_context.socket(zmq.PUB)
  sock_send.connect("tcp://127.0.0.1:10901")
  while running:
    try:
        # Send time sync every minute.
        if min != time.gmtime().tm_min:
            min = time.gmtime().tm_min
            tft.write_radio(struct.pack("<BBBi",0x1,0xff,4,time.mktime(time.localtime())))
        # Send ping every second.
        if sec != time.gmtime().tm_sec:
            sec = time.gmtime().tm_sec
            tft.write_radio(struct.pack("<BBBB",0x1,0xff,0x80,0xfe))
        
        values = tft.read()
        if len(values) >= 2: 
            if values[2] & 1 == 1:
                long_names[str(values[0])+"-"+str(values[2] & ~1)] = values[3]
                long_values[values[3]] = (values[0],values[2] & ~1)
                #print long_names
            else:
                long_name = str(values[0])+"-"+str(values[2] & ~1)
                #print("long_name",long_name)
                if (long_names.has_key(long_name)):
                    values_long = [long_names[long_name],"*","1",str(values[3]),str(values[4])]
                    #print("Radio_long",values_long)
                    sock_send.send_multipart([str(i) for i in values_long])                    
    except:
        traceback.print_exc(file=sys.stderr)
  print("ended")
  thread_ended = True
  sock_send.close()

t = threading.Thread(target=radio_thread)
t.start()
# We cannot shutdown the main loop until the Radio loop has finished so we wait until the radio loop has shutdown
# by waiting for ended = True

def handle_packet(args):
        sock_live.send_multipart(args)
        print("args:",args)
        
        if args[0] == "Button/3-16":
            print urllib2.urlopen("http://house-nas/lights/toggle?light="+str(values[3]-1)).read()

        if args[1] == "pair":
            print("pair ***************************************************************")
            print(long_values)
            tft.write_radio(struct.pack("<BBBB", 1,0xf0, 0x81, int(args[2])))
        #if args[0] == "shed-led":
        #    print("args:",args)
        #    tft.write_radio(struct.pack("<BBBBB", 1,3, 0x10, int(args[2])+1, int(args[3])))
        #    print("send done")
        if long_values.has_key(args[1]):
            values = long_values[args[1]]
            print "send:",values
            print args
            tft.write_radio(struct.pack("<BBBBB", 1, values[0], values[1], int(args[2]), int(args[3])))
            print "send done"

last_seen={}
            
while thread_ended == False:
    try:
        args = sock_sub.recv_multipart()#zmq.NOBLOCK)

        # If the packet has a sequence no. then check the sequence no. is not from a previous packet.
        # we also need to handle wrap around as the sequence no. is a 8bit value.
        if len(args) > 4:
            if not last_seen.has_key(args[0]):
                last_seen[args[0]] = args
            else:
                if int(args[4]) > int(last_seen[args[0]][4]):
                    handle_packet(args)
                    
                    last_seen[args[0]] = args
                else:
                    if (int(last_seen[args[0]][4])-int(args[4])) > 200:
                        last_seen[args[0]] = args
                        
                        handle_packet(args)
        else:
            handle_packet(args)
                        
    except zmq.ZMQError as e:
        print(e)
    except:
        traceback.print_exc(file=sys.stderr)
        print("Shutdown, error:", sys.exc_info()[0])
        running=False

sock_live.close()
sock_sub.close()
