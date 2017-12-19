#from PIL import Image
import sys
import time
import os
import struct
import threading
import uuid

import usb.core
import usb.util
import binascii
import zmq
import json

USB_TIMEOUT = 5000 # Timeout in MS

class Serial_Bus(object):
    def __init__(self):
        import serial
        self.ser = serial.Serial('/dev/ttyACM2')  # open serial port
        self.long_values = {}
        self.long_names  = {}
        self.hosts       = {}
    def read(self):
    
        values_list = []
    
        recv_data = bytearray(self._read_device())

        packets = self._decode_packet(recv_data)
        
        for packet in packets:
            print("packet:",packet,"::",packet[2] & 1)
            if len(packet) >= 2:
                if packet[0] not in self.hosts:
                    self.req_addr(packet[0])
                elif packet[2] & 1 == 1:
                    self.long_names[self.hosts[packet[0]]+"-"+str(packet[2] & ~1)] = packet[3]
                    self.long_values[packet[3]] = (self.hosts[packet[0]],packet[2] & ~1)
                    #print(self.long_names)
                else:
                    long_name = str(self.hosts[packet[0]])+"-"+str(packet[2] & ~1)
                    #print("long_name",self.long_names,long_name)
                    if (long_name in self.long_names) and (packet[0] in self.hosts):
                        values_dict = { "station_id": self.hosts[packet[0]], "value" : packet[4], "sequence_no": packet[3] }
                        values_long = [self.long_names[long_name],"*",json.dumps(values_dict),str(packet[4])]
                        values_list.append(values_long)
        return values_list
        
    def _decode_packet(self,recv_data):
	
        if len(recv_data) < 8:
             return []
			 
        args = list(struct.unpack("<BBBBi", recv_data[:8]))
        print("line:",args)
        args[3] = args[3] * 2
        if args[2] == 0x88:
            #print("data:",bytes(recv_data[4:22]))
            uuid_str = str(uuid.UUID(bytes=bytes(recv_data[3:21])))
            
            self.hosts[args[0]] = uuid_str
            
            data = ["UUID/"+uuid_str]
            return [tuple(list(args[0:3]) + data)]
        elif(args[2] & 1 == 1):
            #print("descriptor type",recv_data[5] >> 4)

            if args[4] & 0xf == 1: # Onewire bus address of sensor.
                addr=[]
                #print("raw addr:",recv_data)
                for byte_data in recv_data[5:13]:
                    addr = [byte_data] + addr               
                data = ["Temp/"+binascii.hexlify(bytearray([recv_data[6]]))+'-'+binascii.hexlify(bytearray(addr))]
            else:
                name = []
                for ch in recv_data[4:]:
                    if ch != 0:
                        name.append(ch)
                    else:
                        break
                data = [bytearray(name).decode('UTF-8')+"/"+self.hosts[args[0]]+"/"+str(args[2] & ~0x1),recv_data[3] >> 4]
            return [tuple(list(args[0:3]) + data)]
        else:
            lst = [tuple(args)]
            pos = 8
            for item in range((len(recv_data) - 8) // 4):
               lst.append((args[0],args[1],args[2]+2+item*2,(args[3]+item+1),struct.unpack("<i",recv_data[pos:pos+4])[0]))
               pos = pos + 4
            return lst

    def _read_device(self):
        line=self.ser.readline();
        print(line[0],' ',line)
        if line[0] == 68:
            args = line.strip()[1:-1].split(b',')
            args = map(lambda x: int(x,16),args)
            return args
        if line[0] == 80:
            print('Device DBG:',line[1:])
        return ()
    
class TFT_FastBus(Serial_Bus): 
    def __init__(self): 
        self.long_values = {}
        self.long_names  = {}
        self.hosts       = {}
        #super(TFT_FastBus, self).__init__()
        self.dev = usb.core.find(idVendor=0x0483,idProduct=0x5740)
        self.write_lock = threading.RLock()
        
        for interface in self.dev[0]:
            if interface.bInterfaceClass == 0xff:
                print(interface)
                self.fast_endpoint = interface[0]
                self.rx_endpoint = interface[1]
                #print self.rx_endpoint
        #self.rx_endpoint = self.dev[0][(1,0)][1]
        #self.endpoint_out  = self.dev[0][(1,0)][0] #.bEndpointAddress
        #self.endpoint_in   = self.dev[0][(1,0)][1]
        self.tm_sec = -1
    
    def write(self,str):
        self.write_lock.acquire()
        self.fast_endpoint.write(struct.pack("<H",0x1000) + str)
        self.write_lock.release()
        
    def write_addr(self,str):
        self.write_lock.acquire()
        self.fast_endpoint.write(struct.pack("<H",0x1000) + str)
        self.write_lock.release()
        
    def req_addr(self,count):
        self.write_lock.acquire()
        self.fast_endpoint.write(struct.pack("<HB",0x2000,count))
        self.write_lock.release()
               
    def _read_device(self):
        try:
            print(self.rx_endpoint.wMaxPacketSize)
            recv_data = self.rx_endpoint.read(self.rx_endpoint.wMaxPacketSize, timeout=5000)
            print("recv_data:",recv_data)
            if (len(recv_data) < 8):
                return ()
            
            return recv_data
            #str_out = ""
            #for ch in str[2:]:
            #    str_out = str_out + chr(ch) 
            #if len(str) == 4:
            #return struct.unpack("<HH",str)
            #return str_out
        except usb.core.USBError as e:
            if e.backend_error_code != -116:
                print(e.backend_error_code)
                raise e
            #print(e)
            return ()


if __name__ == "__main__":
    data_bus = TFT_FastBus()
    thread_context = zmq.Context()
    sock_send = thread_context.socket(zmq.PUB)
    sock_send.connect("tcp://192.168.1.116:10901")
    try:
      #data_bus.write("wibble".encode('utf-8'))
      data_bus.req_addr(10);
      while True:
        values_lst = data_bus.read()
        for values in values_lst:
            print(values)
            if values:
                sock_send.send_multipart([str(i).encode('UTF-8') for i in values])
        
    except KeyError:
        tempreture_log_file.close()
        #tft.fast_endpoint.write(struct.pack("<H",0x4000) + str(x))
        #print x
        #print time.gmtime().tm_min,
        
        #.split('$')
        #if (int(args[2]) < 3500):
