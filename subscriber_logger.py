import zmq
import datetime
import db_log

logger = db_log.Logger()

#for reading in logger.session_inst.query(db_log.Readings).all():
#    print(reading.sensor_id,reading.value)

# Socket to talk to server
context = zmq.Context()
sock = context.socket(zmq.SUB)

#UPDATE_INTERVAL = datetime.timedelta(minutes=1)
UPDATE_INTERVAL = datetime.timedelta(seconds=60)
print "Collecting updates from weather server..."
sock.connect ("tcp://house-nas:10900")
sock.setsockopt(zmq.SUBSCRIBE, '')

last_seen = {}

print "Connected."
while True:
    args = sock.recv_multipart()
    
    if not last_seen.has_key(args[0]):
        last_seen[args[0]] = datetime.datetime.utcnow()
        logger.handle_args(args)
    else:
        delta = datetime.datetime.utcnow() - last_seen[args[0]]
        
        #print("Delta:",delta)
        if (delta > UPDATE_INTERVAL):
            last_seen[args[0]] = datetime.datetime.utcnow()
            logger.handle_args(args)
    #print("last_seen:",last_seen)