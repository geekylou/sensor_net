import sys
import time
import zmq
import json
from zmq.eventloop import ioloop
ioloop.install()
from zmq.eventloop.zmqstream import ZMQStream

import tornado.ioloop
import tornado.gen
import tornado.web
import tornado.websocket
import threading

import session_ipc

def tprint(msg):
    """like print, but won't get newlines confused with multiple threads"""
    sys.stdout.write(msg + '\n')
    sys.stdout.flush()
        
class SendHandler(tornado.web.RequestHandler):
    def initialize(self):
        self.context = zmq.Context()
        self.send_socket = self.context.socket(zmq.PUB)
        self.send_socket.connect('tcp://house-nas:10901')
        
    def get(self,arg,value):
        self.send_socket.send_multipart(["WebServer/"+str(arg),"*","1",str(value)])
        self.write("done")
	
class ZMQPubSub(object):
    def __init__(self, callback):
        self.callback = callback

    def connect(self):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.SUB)
        self.socket.connect('tcp://house-nas:10900')
        
        print("socket connect:",self.socket)
        self.stream = ZMQStream(self.socket)
        self.stream.on_recv(self.callback)

    def subscribe(self, channel_id):
        self.socket.setsockopt(zmq.SUBSCRIBE, channel_id)
	
    def close(self):
        self.stream.close()
        print self.socket.close()

class MultiplexPubSub(object):
    def __init__(self):
        self.callbacks = set()

    def add_callback(self,callback):
        self.callbacks.add(callback)
        
    def remove_callback(self,callback):
        self.callbacks.remove(callback)
        
    @tornado.gen.coroutine
    def on_recv(self,data):
        data[2] = json.loads(data[2])
        if data[2]==1:
            data[2]= {"value":data[3]}
        if not "timestamp" in data[2]:
            data[2]['timestamp'] = time.time()
#        print("callback:",data,self.callbacks)
        for callback in self.callbacks:
            callback(data)
        
    def connect(self):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.SUB)
        self.socket.connect('tcp://house-nas:10900')
        
        self.send_socket = self.context.socket(zmq.PUB)
        self.send_socket.connect('tcp://house-nas:10901')
        
        print("socket connect:",self.socket,self.on_recv)
        self.stream = ZMQStream(self.socket)
        self.stream.on_recv(self.on_recv)

    def send(self,data):
        self.send_socket.send_multipart([str(i) for i in json.loads(data)])

    def subscribe(self, channel_id):
        print("subscribe:",channel_id)
        self.socket.setsockopt(zmq.SUBSCRIBE, channel_id)
	
    def close(self):
        self.stream.close()
        print self.socket.close()
        
class RelayEventSource(tornado.web.RequestHandler):
    @tornado.web.asynchronous
    def get(self):
         self.session_ipc = None
         self.pubsub = None
         print(self.request.remote_ip)
         if (self.request.remote_ip == '127.0.0.1'):
             self.authenticated = True
             return(self.init_pubsub())
         else:
             self.session_ipc = session_ipc.SessionIPC(context,self.on_data)
             self.session_ipc.retrieve_session(self.on_session_data,self.get_cookie("sessionid"))
        
    def on_session_data(self,data):
        self.authenticated = False
        #print(data)
        if (data[0] == 'OK'):
            args = json.loads(data[1])
            
            if "authenticated" in args and args["authenticated"] == True:
                self.authenticated = True
        self.init_pubsub()

    def init_pubsub(self):
        self.pubsub = ZMQPubSub(self.on_data)    
        self.pubsub.connect()

        if self.authenticated:
            self.pubsub.subscribe("")
        else:
            self.pubsub.subscribe("Temp/28-031600af4bff")
            
        self.set_header("Content-Type", 'text/event-stream')
        self.set_header("Cache-Control", 'no-cache')
        print(self.get_cookie("sessionid"))

    def on_data(self, data):
#        data[2] = json.loads(data[2])
        self.write(u"data: " + json.dumps(data)+ "\n\n")   
        self.flush()
    
    def on_connection_close(self):
        print "on_close"
        if (self.session_ipc):
            self.session_ipc.close()
        if (self.pubsub):
            self.pubsub.close()
        self.finish()
        
class RelayWebSocket(tornado.websocket.WebSocketHandler):
    def __init__(self,a,b):
        super(RelayWebSocket,self).__init__(a,b)
        self.pubsub      = None
        self.session_ipc = None
    def check_origin(self, origin):
        print("origin:",origin)
        return True

    def open(self):
         print(self.request.remote_ip)
         if (self.request.remote_ip == '127.0.0.1'):
             self.authenticated = True
             return(self._init_pubsub())
         else:
             self.session_ipc = session_ipc.SessionIPC(context,self.on_data)
             self.session_ipc.retrieve_session(self.on_session_data,self.get_cookie("sessionid"))
        
    def on_session_data(self,data):
        self.authenticated = False
        #print(data)
        if (data[0] == 'OK'):
            args = json.loads(data[1])
            
            if "authenticated" in args and args["authenticated"] == True:
                self.authenticated = True
        self._init_pubsub()
    
    
    def _init_pubsub(self):
        #self.pubsub = ZMQPubSub(self.on_data) 
        #print "connect!"
        #self.pubsub.connect()
        
        #if self.authenticated:
        #    self.pubsub.subscribe("")
        #else:
        #    self.pubsub.subscribe("Temp/28-031600af4bff")
        
        #self.context = zmq.Context()
        #self.send_socket = self.context.socket(zmq.PUB)
        #self.send_socket.connect('tcp://house-nas:10901')

        pubsub.add_callback(self.on_data)
        print 'ws opened'
  
    def on_message(self, message):
        if self.authenticated:
            print("Message received:",message)
            #self.send_socket.send_multipart([str(i) for i in json.loads(message)])
            pubsub.send(message)
            
    def on_close(self):
        pubsub.remove_callback(self.on_data)
        if (self.pubsub):
            self.pubsub.close()
        if (self.session_ipc):
            self.session_ipc.close()
        print("WebSocket closed")
        
    def on_data(self, data):
        print data
        if not self.authenticated:
            if not is_valid(data):
                return
        self.write_message(json.dumps(data))

def is_valid(data):
    valid = False
    valid = valid | data[0].startswith("Temp/28-031600af4bff")
    valid = valid | data[0].startswith("mqtt/octoprint/temperature/")
    valid = valid | data[0].startswith("mqtt/octoprint/progress/")
    return valid
    
def filter(values):
    for item in values:
        if is_valid(item):
            yield item
    
class LastMessages(tornado.web.RequestHandler):
    @tornado.web.asynchronous
    def get(self):
        if (self.request.remote_ip == '127.0.0.1'):
            self.authenticated = True
            return(self._process())
        else:
             self.session_ipc = session_ipc.SessionIPC(context,None)
             self.session_ipc.retrieve_session(self.on_session_data,self.get_cookie("sessionid"))
        
    def on_session_data(self,data):
        self.authenticated = False
        print(data)
        if (data[0] == 'OK'):
            args = json.loads(data[1])
            
            if "authenticated" in args and args["authenticated"] == True:
                self.authenticated = True
        self._process()
    
    def _process(self):
        if not self.authenticated:
            vals = list(filter(last_msg.values()))
        else:
            vals = last_msg.values()

        self.write(json.dumps(vals))
        self.finish()
        
context = zmq.Context()
last_msg = {}

def PubSubCallback(data):
    last_msg[data[0]] = data

application = tornado.web.Application([
    (r"/weather_ws/send/([^/]*)/([^/]*)", SendHandler),
    (r"/weather_ws/event", RelayWebSocket),
    (r"/weather_ws/event_src", RelayEventSource),
    (r"/weather_ws/last", LastMessages),
], cookie_secret="<secret-key>") # Cookie secret is not used here.  This should also be loaded in from a config file if needed.

if __name__ == "__main__":
    def main():
        global pubsub
        pubsub = MultiplexPubSub()
        pubsub.connect()
        pubsub.subscribe("")
        pubsub.add_callback(PubSubCallback)
        
    try:
        print("Start event loop")
        application.listen(8889)
        zmq.eventloop.IOLoop.current().run_sync(main)
        zmq.eventloop.IOLoop.instance().start()
    except KeyboardInterrupt:
        router.close()
        pubsub.close()