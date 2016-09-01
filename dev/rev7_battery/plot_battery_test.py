#!/usr/bin/python3

import csv
import os
import numpy as np
import matplotlib.pyplot as plt

FILE_PATH = os.path.realpath("./battery_test-secureTx2.log")

PLOT_START = 0.0
PLOT_END = 600.0

results = []
foo = []

axis = plt.gca()
axis.set_xlim([PLOT_START, PLOT_END])
#axis.set_ylim([0.0, 40.0])

with open(FILE_PATH, 'r') as csvfile:
    spamreader = csv.reader(csvfile, delimiter=',')
    
    next(spamreader)
    for row in spamreader:
        try:
            temp = [float(x) for x in row]
            #temp[0] = int(row[0]) // 8
            foo.append(temp)
        except:
            if row[0] == "Settings:":
                #results.append(foo)
                break
            continue
            
result_matrix = np.asarray(foo)
#print(result_matrix)

#foo = np.copy(result_matrix)
#foo = foo[foo[:,2] == 2.2,:]
#foo[:,0] = foo[:,0] - foo[0,0]
#plt.plot(foo[:200,0], foo[:200,1])
#for v in range(len(result_matrix[:,0,0])):
bar = np.copy(result_matrix)
start = 0

bar[:,1] = bar[:,1] - bar[start,1]

bar[:,0] = bar[:,0] * 0.01
#bar[:,3] = bar[:,3] * 10

plt.plot(bar[:,1], bar[:,0])
plt.plot(bar[:,1], bar[:,2])
#plt.plot(bar[:,1], bar[:,3])

#bar += 0.1

# show plot
plt.legend()
plt.show()
