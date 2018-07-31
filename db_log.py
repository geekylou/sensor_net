import datetime,json
from sqlalchemy import MetaData,Column, String, Integer, DateTime,Table
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.ext.automap import automap_base
from sqlalchemy.dialects.postgresql import JSON, JSONB

#Base = declarative_base()

# automap base
Base = automap_base()

from sqlalchemy import create_engine
engine = create_engine('postgres://sensor_net:fjw34jkw@localhost/sensor_net')

Base.prepare(engine, reflect=True)

Readings = Base.classes.sensors_reading

#class Readings(Base):
#        __table__ = Table('mytable', Base.metadata,
#                        autoload=True, autoload_with=engine)
        #__tablename__ = 'sensors_readings'
        #__table_args__ = {'autoload':True}
#        id = Column(Integer, primary_key=True)
#        sensor_id = Column(String(64))
#        #value     = Column(Integer)
#        datetime  = Column(DateTime, default=datetime.datetime.utcnow)
#        msg       = Column(JSONB)
       
class Logger():
    def __init__(self):
        #'sqlite:///sensor_readings.db');

        from sqlalchemy.orm import sessionmaker
        session = sessionmaker()
        session.configure(bind=engine)
        Base.metadata.create_all(engine)

        self.session_inst = session()
        
    def handle_args(self,args):
        print(args)
        
        reading = Readings(sensor_id = args[0]) #, value = int(float(args[3])))
        
        if args[2] != '1':
            print(json.loads(args[2]))
            reading.msg = json.loads(args[2])
        else:
            reading.msg = {'value': int(float(args[3]))}
        self.session_inst.add(reading)
        self.session_inst.commit()
    