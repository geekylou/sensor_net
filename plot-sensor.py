import sys
import matplotlib as mpl
import datetime
mpl.use('Agg')

import matplotlib
import matplotlib.pyplot as plt
from matplotlib.dates import DateFormatter
import numpy as np
import time

import db_log

logger = db_log.Logger()

# Plot a graph from sensor data use python plot-sensor.py <sensor-name> <output file> <scale, usually 1000> to plot a graph

x = []
y = []

since = datetime.datetime.now() - datetime.timedelta(hours=24)

for reading in logger.session_inst.query(db_log.Readings).filter(db_log.Readings.datetime > since):
    if (reading.sensor_id == sys.argv[1]):
    #"Temp/28-031600af4bff"): #"Pressure/1-4"):
        x.append(reading.datetime)
        y.append(float(reading.value)/float(sys.argv[3]))

fig, ax = plt.subplots()

ax.grid(True)

y_formatter = matplotlib.ticker.ScalarFormatter(useOffset=False)
ax.yaxis.set_major_formatter(y_formatter)

#plt.ylim([980,1020])
#ax.format_xdata = mdates.DateFormatter('%Y-%m-%d')
ax.xaxis.set_major_formatter( DateFormatter( '%H:%M' ) )


ax.plot(x,y)

plt.savefig(sys.argv[2])
plt.close(fig)
