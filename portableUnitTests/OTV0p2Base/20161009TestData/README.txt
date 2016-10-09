Comments on this data set for a little over 24h.

3l hard to detect occupancy all day.
5s hard to detect occupancy in the evening up to 21:00Z.
6k generally easy to spot occupancy.

L files are light levels, O are some occupancy detected by a current code version.

    min	max
3l    1 182
5s    1 181
6k    1 185

5s mean over several days at specific hours (eg as estimated with egrep 'T21:' | awk '{sum+=$3;count++} END {print sum/count}':
Occupancy expected until ~21:00.
19:XX 20:XX 21:XX
  8.7  10.0   3.1

3l mean over several days at specific hours
Occupancy expected until ~21:00.
18:XX 19:XX 20:XX 21:XX 22:XX 23:XX
 35.3  33.2  29.7  10.3   2.6  1.4
 
 Should aim to detect opening blinds/curtains in all rooms in morning.