#from PIL import Image
import sys
import time
import os
import struct
import threading

import usb.core
import usb.util
import binascii

USB_TIMEOUT = 5000 # Timeout in MS

class MyOled(object):
    def __init__(self):
        import serial
        self.ser = serial.Serial('com16')  # open serial port

    def SendLine(self,line_no,text):
        self.write(str(line_no)+"$"+text+"\r\n")
    
    def SendTime(self,yr_offset):
        self.write("-2$"+str(yr_offset)+"\r\n")
        self.write("-1$"+str(time.mktime(time.localtime()))+"\r\n")
    
    def Shutdown(self):
        self.ClearScreen()
    
    def Upgrade(self):
        self.write("-99$\r\n")
    def ClearScreen(self):
        self.write("-3\r\n")
    def write(self,str):
        self.ser.write(str)
    def read(self):
        return self.ser.readline();
        
class TFT_FastBus(MyOled): 
    def __init__(self): 
        #super(TFT_FastBus, self).__init__()
        self.dev = usb.core.find(idVendor=0x1eaf,idProduct=0x0006)
        self.write_lock = threading.RLock()
        
        for interface in self.dev[0]:
            if interface.bInterfaceClass == 0xff:
                print interface
                self.fast_endpoint = interface[0]
                self.rx_endpoint = interface[1]
                #print self.rx_endpoint
        #self.rx_endpoint = self.dev[0][(1,0)][1]
        #self.endpoint_out  = self.dev[0][(1,0)][0] #.bEndpointAddress
        #self.endpoint_in   = self.dev[0][(1,0)][1]
        self.tm_sec = -1
    def load_image(self,filename,xbase,ybase):
        return self.put_image(Image.open(filename),xbase,ybase)
        
    def put_image(self,im,xbase,ybase):
        px = im.load()
        xsize,ysize = im.size
        print xsize,ysize
        
        for y in xrange(ysize):
            buf = ""
            for x in xrange(xsize):
                if len(buf) == 0:
                    xstart = x
                    
                r,g,b = px[x,y]
                r = r >> 3
                g = g >> 2
                b = b >> 3
                buf = buf + struct.pack("<H", b | g << 5 | r << 11)
                if len(buf) >= 60:
                    self.fast_endpoint.write(struct.pack("<HH",xstart + xbase ,y + ybase) + buf)
                    buf = ""
            if len(buf) > 0:
                self.fast_endpoint.write(struct.pack("<HH",xstart + xbase ,y + ybase) + buf)
        #self.fast_endpoint.write(struct.pack("<HH",0x5000,0x5000))
    def put_pixel(self,x,y,r,g,b):
        pixel = b | g << 5 | r << 11
        self.fast_endpoint.write(struct.pack("<HHH",x,y,pixel))
    
    def put_text(self,colour,x,y,line):
        r,g,b,rb,gb,bb = colour
        r = r >> 3
        g = g >> 2
        b = b >> 3
        rb = rb >> 3
        gb = gb >> 2
        bb = bb >> 3
        self.put_text565((r,g,b,rb,gb,bb),x,y,line)
        
    def put_text565(self,colour,x,y,line):
        font = 0#0x200 + 80
        r,g,b,rb,gb,bb = colour
        self.fast_endpoint.write(struct.pack("<HBBHH",0x2000|font,x,y,b | g << 5 | r << 11,bb | gb << 5 | rb << 11)+line[0:58])
        while(len(line)>58):
            line = line[58:]
            self.fast_endpoint.write(struct.pack("<H",0x2400|font)+line[0:58])       
    
    def put_text16colour(self,colour,x,y,line):
        r=0;g=0;b=0;rb=0;gb=0;bb=0
        if (colour & 0x01 != 0):
            r = 31;
        if (colour & 0x02 != 0):
            g = 63;
        if (colour & 0x04 != 0):
            b = 63;
        if (colour & 0x10 != 0):
            rb = 31;
        if (colour & 0x20 != 0):
            gb = 63;
        if (colour & 0x40 != 0):
            bb = 63;
        self.put_text565((r,g,b,rb,gb,bb),x,y,line)
        
    def load565(self,filename,width,height,xbase,ybase):
        file = open(filename,"rb")
        self.fast_endpoint.write(struct.pack("<HH",0,0))
        file.read(53)
        xlen = width
        for y in xrange(height):
            x = 0
            buf =  ''
            
            cont = True
            while cont:
                if (xlen - x) < 30:
                    buf = file.read((xlen-x)*2)
                    #print len(buf)
                    cont = False
                else:
                    buf = file.read(60)
                buf = struct.pack("<HH",xbase+x,y+ybase) + buf
                #print len(buf)
                self.fast_endpoint.write(buf)
                x = x + 30
        self.fast_endpoint.write(struct.pack("<HH",0x1000,0x1000))          
    
    def write(self,str):
        self.write_lock.acquire()
        self.fast_endpoint.write(struct.pack("<H",0x1000) + str)
        self.write_lock.release()
        
    def write_radio(self,str):
        self.write_lock.acquire()
        self.fast_endpoint.write(struct.pack("<H",0x4000) + str, timeout=1000)
        self.write_lock.release()
        
    def read(self):
        try:
            #print(self.rx_endpoint.wMaxPacketSize)
            recv_data = self.rx_endpoint.read(self.rx_endpoint.wMaxPacketSize, timeout=5000)
            
            if (len(recv_data) < 10):
                return []
            args =  struct.unpack("<BBBiB", recv_data[2:10])
            if(args[2] & 1 == 1):
                print(recv_data[5])
                if recv_data[5]== 1: # Onewire bus address of sensor.
                    addr=[]
                    #print("raw addr:",recv_data)
                    for byte_data in recv_data[7:13]:
                            addr = [byte_data] + addr               
                    data = ["Temp/"+binascii.hexlify(bytearray([recv_data[6]]))+'-'+binascii.hexlify(bytearray(addr))]
                else:
                    name = []
                    for ch in recv_data[6:]:
                        if ch != 0:
                            name.append(ch)
                        else:
                            break
                    data = [bytearray(name).decode('UTF-8')+"/"+str(args[0])+"-"+str(args[2] & ~0x1)]
                return tuple(list(args[0:3]) + data)
            else:
                print(args)
                return args
            #str_out = ""
            #for ch in str[2:]:
            #    str_out = str_out + chr(ch) 
            #if len(str) == 4:
            #return struct.unpack("<HH",str)
            #return str_out
        except usb.core.USBError as e:
            if e.backend_error_code != -116:
                raise e
            return ()
            print e
class GLCD():
    def __init__(self):
        import GLCD_SDK
        GLCD_SDK.initDLL("C:\\Program Files\\Logitech Gaming Software\\SDK\\LCDSDK_8.57.148\\Lib\\GameEnginesWrapper\\x86\\LogitechLcdEnginesWrapper.dll")
        GLCD_SDK.LogiLcdInit("Python",GLCD_SDK.TYPE_COLOR+GLCD_SDK.TYPE_MONO)

    def SendLine(self,line_no,text):
        GLCD_SDK.LogiLcdMonoSetText(line_no,text)
        GLCD_SDK.LogiLcdUpdate()
    
    def SendTime(self):
        pass

class VectorImage():
    def str_list(self,list):
        for item in list:
            yield str(item)
    
    def int_list(self,list):
        for item in list:
            yield int(item)        
    def __init__(self,filename):
        self.items = []
        file = open(filename,"r")
        for line in file.readlines():
            line = line.strip().split('$')
            #print line 
            line[1] = int(line[1]) - 80
            line[2] = int(line[2]) - 100
            if line[0] == '-4':
                line[3] = int(line[3]) - 80
                line[4] = int(line[4]) - 100
            else:
                line[3] = int(line[3])
                line[4] = int(line[4])
                
            self.items.append(line)
    def scale(self,scale):
        for item in self.items:
            item[1] = item[1] * scale
            item[2] = item[2] * scale
            item[3] = item[3] * scale
            item[4] = item[4] * scale
    def move(self,x,y):
        for item in self.items:
            item[1] = item[1] + x
            item[2] = item[2] + y
            if item[0] == '-4':
                item[3] = item[3] + x
                item[4] = item[4] + y
    def upload(self,ser):
        for line in self.items:
            ser.write(str.join("$",self.str_list(line))+"\r\n")
if __name__ == "__main__":
    tft = TFT_FastBus()
    #tft.SendTime(0)
    #tft.ClearScreen()
    
    #tft.load565("diamondback.565",320,256,0,0)
    #tft.ClearScreen()
    #tft.load565("ksp.565",320,256,0,0)
    #time.sleep(0.1)
    #for x in xrange(65536):
    #    tft.put_text16colour(0x01,0,0,"test"+str(x))
    #    tft.put_text16colour(0x12,0,1,"test"+str(x))
    #    tft.put_text16colour(0x23,0,2,"test"+str(x))
    #    tft.put_text16colour(0x34,0,3,"test"+str(x))
    #    tft.put_text16colour(0x45,0,4,"test"+str(x))
    #    tft.put_text16colour(0x56,0,5,"test"+str(x))
    #    tft.put_text16colour(0x67,0,6,"test"+str(x))
    #import pyscreenshot as ImageGrab
    #while True:
    #    tft.put_image(ImageGrab.grab().resize((320,240),Image.LANCZOS),0,0)
    #tft.ClearScreen()
    #tft.put_image(Image.open("diamondback.bmp"),0,0)
    x=0
    global internal_temp
    global external_temp
    global presure
    min = -1
    wrote_log = False
    #tempreture_log_file = open('tempreture_log', 'a', 0)
    try:
      while True:
        if min != time.gmtime().tm_min:
            internal_temp = -1
            external_temp = -1
            pressure = -1
            wrote_log = False
            min = time.gmtime().tm_min
        #values = (1,2,3,4)#tft.read()
        values = tft.read()
        print(values)
        if values[2] == 1 and internal_temp == -1:
            print "Internal temp", values[3]
            internal_temp = values[3]
        if values[2] == 2 and external_temp == -1:
            print "External temp", values[3]
            external_temp = values[3]
        if values[2] == 3 and pressure == -1:
            print "Pressure", values[3]
            pressure = values[3]
            
        if pressure >= 0 and internal_temp >= 0 and external_temp >= 0 and wrote_log == False:
            #tempreture_log_file.write(str(time.time())+','+str(pressure)+','+str(internal_temp)+','+str(external_temp)+'\n')
            wrote_log = True
            
        tft.write_radio(struct.pack("<BBBBB", 1,3, 0x12, 1, 1))
    except KeyError:
        tempreture_log_file.close()
        #tft.fast_endpoint.write(struct.pack("<H",0x4000) + str(x))
        #print x
        #print time.gmtime().tm_min,
        
        #.split('$')
        #if (int(args[2]) < 3500):
    #    print (int(args[1])-150)/10/80,(int(args[2])-330)/20/50,args[2]
    #oled = MyOled()
    #oled.SendTime(0)
    #for x in xrange(1000):
    #    tft.SendLine(0,"Line:"+str(x)+"\r\n")
    #    tft.SendLine(1,"Line:"+str(x)+"\r\n")
    #    tft.SendLine(2,"Line:"+str(x)+"\r\n")
    #    tft.SendLine(3,"Line:"+str(x)+"\r\n")
    #    tft.SendLine(4,"Line:"+str(x)+"\r\n")
    #    tft.SendLine(5,"Line:"+str(x)+"\r\n")
    #oled.ser.write("-3\r\n")
    #image = VectorImage("envelope.vector")
    #image.scale(1.8)
    #image.move(15,0)
    #image.upload(oled.ser)