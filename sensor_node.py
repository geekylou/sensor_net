import json
import zmq
import tft_lib
import time
import struct
import urllib2
import threading
import sys
import traceback

DEBUG_PRINT = True

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

def dbg_print(*args):
  if DEBUG_PRINT:
      # Want to make this python 2 compatible as we haven't yet ported sensor_node.py.
      statement = ""
      for arg in args:
          statement = statement + " " + str(arg)
      print(statement)
      
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
                    values_dict = { "station_id": values[0], "value" : float(values[3])/1000, "sequence_no": values[4] }
                    values_long = [long_names[long_name],"*",json.dumps(values_dict),str(values[3])]
                    #print("Radio_long",values_long)
                    sock_send.send_multipart([str(i) for i in values_long])                    
    except:
        traceback.print_exc(file=sys.stderr)
  dbg_print("ended")
  thread_ended = True
  sock_send.close()

t = threading.Thread(target=radio_thread)
t.start()
# We cannot shutdown the main loop until the Radio loop has finished so we wait until the radio loop has shutdown
# by waiting for ended = True

def handle_packet(args):
        sock_live.send_multipart(args)
        dbg_print("args:",args)
        
        if args[0] == "Button/3-16":
            dbg_print(urllib2.urlopen("http://house-nas/lights/toggle?light="+str(int(args[3])-1)).read())

        if args[1] == "pair":
            dbg_print("pair ***************************************************************")
            dbg_print(long_values)
            tft.write_radio(struct.pack("<BBBB", 1,0xf0, 0x81, int(args[2])))
        if args[1] == "config":
            dbg_print("config ***************************************************************")
            dbg_print(long_values)
            tft.write_radio(struct.pack("<BBBB", 1,0xf0, 0x82, int(args[2])))
        #if args[0] == "shed-led":
        #    print("args:",args)
        #    tft.write_radio(struct.pack("<BBBBB", 1,3, 0x10, int(args[2])+1, int(args[3])))
        #    print("send done")
        if long_values.has_key(args[1]):
            values = long_values[args[1]]
            dbg_print("send:",values)
            dbg_print(args)
            tft.write_radio(struct.pack("<BBBBB", 1, values[0], values[1], int(args[2]), int(args[3])))
            dbg_print("send done")

last_seen={}
            
while thread_ended == False:
    try:
        sequence_no = None
        station_id = None
        
        args = sock_sub.recv_multipart()#zmq.NOBLOCK)
        dbg_print(args)
        if(args[2] != "1"):
            try:
                args_dict = json.loads(args[2])
                if type(args_dict) is dict and args_dict.has_key("sequence_no") and args_dict.has_key("station_id"):
                    sequence_no = int(args_dict["sequence_no"])
                    station_id  = int(args_dict["station_id"])
            except ValueError as e:
                args_dict = {'error':'decoding error','text':str(e)}
                args[2] = json.dumps(args_dict).encode('utf-8')
            print(args_dict)
        # If the packet has a sequence no. then check the sequence no. is not from a previous packet.
        # we also need to handle wrap around as the sequence no. is a 8bit value.
        if sequence_no != None:
            if not last_seen.has_key(station_id):
                last_seen[station_id] = (sequence_no,time.time())
                handle_packet(args)
            else:
                if sequence_no > last_seen[station_id][0] or time.time()-last_seen[station_id][1] > 2.0:
                    dbg_print("seq:",last_seen[station_id],args)
                    dbg_print(last_seen)
                    handle_packet(args)
                    
                    last_seen[station_id] = (sequence_no,time.time())
                else:
                    if last_seen[station_id][0]-sequence_no > 200:
                        last_seen[station_id] = (sequence_no,time.time())
                        
                        handle_packet(args)
                    dbg_print("time:",time.time()-last_seen[station_id][1])
        else:
            handle_packet(args)
                        
    except zmq.ZMQError as e:
        dbg_print(e)
    except:
        traceback.print_exc(file=sys.stderr)
        dbg_print("Shutdown, error:", sys.exc_info()[0])
        running=False

sock_live.close()
sock_sub.close()
