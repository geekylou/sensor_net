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

def tprint(msg):
    """like print, but won't get newlines confused with multiple threads"""
    sys.stdout.write(msg + '\n')
    sys.stdout.flush()
        
class MainHandler(tornado.web.RequestHandler):
    def get(self):
        self.write("main handler test")
	
class ZMQPubSub(object):
    def __init__(self, callback):
        self.callback = callback

    def connect(self):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.SUB)
        self.socket.connect('tcp://house-nas:10900')
        self.send_socket = self.context.socket(zmq.PUB)
        self.send_socket.connect('tcp://house-nas:10901')
        
        print self.socket
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
        self.pubsub = ZMQPubSub(self.on_data)
        self.pubsub.connect()
        self.pubsub.subscribe("")
        self.set_header("Content-Type", 'text/event-stream')
        self.set_header("Cache-Control", 'no-cache')
    
    def on_data(self, data):
        print data
        self.write(u"data: " + json.dumps(data)+ "\n\n")   
        self.flush()
    
    def on_connection_close(self):
        print "on_close"
        if (self.pubsub):                                                                                                                                                                     
            self.pubsub.close()
        self.finish()
        
class RelayWebSocket(tornado.websocket.WebSocketHandler):
    def __init__(self,a,b):
        super(RelayWebSocket,self).__init__(a,b)
        self.pubsub = None
    def check_origin(self, origin):
        return True

    def open(self):
        self.pubsub = ZMQPubSub(self.on_data) 
        print "connect!"
        self.pubsub.connect()
        self.pubsub.subscribe("")
        
        self.context = zmq.Context()
        self.send_socket = self.context.socket(zmq.PUB)
        self.send_socket.connect('tcp://house-nas:10901')

        print 'ws opened'
  
    def on_message(self, message):
        print("Message received:",message)
        self.send_socket.send_multipart([str(i) for i in json.loads(message)])
        #self.write_message(u"You said: " + message)

    def on_close(self):
        if (self.pubsub):
            self.pubsub.close()
        print("WebSocket closed")
        
    def on_data(self, data):
        print data
        self.write_message(json.dumps(data))

context = zmq.Context()

application = tornado.web.Application([
    (r"/weather_ws/test", MainHandler),
    (r"/weather_ws/event", RelayWebSocket),
    (r"/weather_ws/event_src", RelayEventSource),
], cookie_secret="hkhkhugruekrhwkhe")

if __name__ == "__main__":
    try:
        application.listen(8889)
        zmq.eventloop.IOLoop.instance().start()
    except KeyboardInterrupt:
        router.close()