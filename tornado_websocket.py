import sys
import time
import zmq
import json
from zmq.eventloop import ioloop
ioloop.install()
from zmq.eventloop.zmqstream import ZMQStream

import tornado.ioloop
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
        data[2] = json.loads(data[2])
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
        self.pubsub = ZMQPubSub(self.on_data) 
        print "connect!"
        self.pubsub.connect()
        
        if self.authenticated:
            self.pubsub.subscribe("")
        else:
            self.pubsub.subscribe("Temp/28-031600af4bff")
        
        self.context = zmq.Context()
        self.send_socket = self.context.socket(zmq.PUB)
        self.send_socket.connect('tcp://house-nas:10901')

        print 'ws opened'
  
    def on_message(self, message):
        if self.authenticated:
            print("Message received:",message)
            self.send_socket.send_multipart([str(i) for i in json.loads(message)])

    def on_close(self):
        if (self.pubsub):
            self.pubsub.close()
        if (self.session_ipc):
            self.session_ipc.close()
        print("WebSocket closed")
        
    def on_data(self, data):
        #print data
        self.write_message(json.dumps(data))

class LastMessages(tornado.web.RequestHandler):
    def get(self):
        self.write(json.dumps(last_msg))
        
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
    try:
        pubsub = ZMQPubSub(PubSubCallback)
        pubsub.connect()
        pubsub.subscribe("")
        print("Start event loop")
        application.listen(8889)
        zmq.eventloop.IOLoop.instance().start()
    except KeyboardInterrupt:
        router.close()
        pubsub.close()