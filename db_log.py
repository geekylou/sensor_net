import datetime
from sqlalchemy import Column, String, Integer, DateTime
from sqlalchemy.ext.declarative import declarative_base

Base = declarative_base()

class Readings(Base):
        __tablename__ = 'readings'
        
        id = Column(Integer, primary_key=True)
        sensor_id = Column(String(32))
        value     = Column(Integer)
        datetime = Column(DateTime, default=datetime.datetime.utcnow)
       
class Logger():
    def __init__(self):
        from sqlalchemy import create_engine
        engine = create_engine('sqlite:///sensor_readings.db');

        from sqlalchemy.orm import sessionmaker
        session = sessionmaker()
        session.configure(bind=engine)
        Base.metadata.create_all(engine)

        self.session_inst = session()
        
    def handle_args(self,args):
        print(args)
        reading = Readings(sensor_id = args[0], value = int(float(args[3])))
        self.session_inst.add(reading)
        self.session_inst.commit()
    