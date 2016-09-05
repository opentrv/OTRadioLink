#!/usr/bin/python3

import csv
import os
import numpy as np
import matplotlib.pyplot as plt


## SETTINGS
FILE_PATH = os.path.realpath("./battery_test-valve.log")
PLOT_START = 0.0
PLOT_END = 120.0
MAX_VOLTAGE = 2901 # Max-voltage-inclusive to plot
MIN_VOLTAGE = 2800 # Min-voltage-inclusive to plot

## CONSTANTS
CLMN_VOLTAGE = 0
CLMN_TIME = 1
CLMN_CURRENT = 2
CLMN_ENDSTOPS = 3


results = []

# This section is for limiting the results to a particular time-slice.
axis = plt.gca()
#axis.set_xlim([PLOT_START, PLOT_END])
#axis.set_ylim([0.0, 40.0])

# Read the .csv, discarding any text.
with open(FILE_PATH, 'r') as csvfile:
    spamreader = csv.reader(csvfile, delimiter=',')
    
    next(spamreader)
    for row in spamreader:
        try:
            temp = [float(x) for x in row]
            #temp[0] = int(row[0]) // 8
            results.append(temp)
        except:
            continue
            
result_matrix = np.asarray(results)  # convert to np array.

results = np.copy(result_matrix[(result_matrix[:,CLMN_VOLTAGE] <= MAX_VOLTAGE) &  
                            (result_matrix[:,CLMN_VOLTAGE] >= MIN_VOLTAGE),:])  # Extract the relevant voltage range

# TODO!!! Need to do something more to filter out readings from wrong voltages.
 
results[:,CLMN_TIME] = results[:,CLMN_TIME] - results[0,CLMN_TIME]  # Take time to start from first reading.
# Plot values
plt.plot(results[:,CLMN_TIME], results[:,CLMN_CURRENT])
plt.plot(results[:,CLMN_TIME], results[:,CLMN_VOLTAGE]*0.1)
# May not always count endstops.
try:
    plt.plot(results[:,CLMN_TIME], results[:,CLMN_ENDSTOPS]* 10)
except:
    pass

# show plot
plt.legend()
plt.show()
