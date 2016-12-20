# The OpenTRV project licenses this file to you
# under the Apache Licence, Version 2.0 (the "Licence");
# you may not use this file except in compliance
# with the Licence. You may obtain a copy of the Licence at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the Licence is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Licence for the
# specific language governing permissions and limitations
# under the Licence.
#
# Author(s) / Copyright (s): Damon Hart-Davis 2016

# Generic plot of ambient light sensor output.

# Title is supplied as:
#     gnuplot -e "title='Multisensor Battery Plot (V)'"
if (!exists("title")) title="AmbLight"
# Input filename supplied as:
#     gnuplot -e "filename='foo.dat'"
# Output filename is the input filename with .svg appended.
if (!exists("filename")) filename='amblightinput.dat'
# Columns of data to to expect to show (will default to 5).
#     gnuplot -e "ncol=3"
if (!exists("ncol")) ncol=3
finalcol = ncol + 2

# Input of form (led by column titles, note the leading 'G'):
# G ddThh:mm light% occ% setback%
# G 10T0:24 0.392 0 50
# G 10T0:25 0.392 0 50


set timefmt "%dT%H:%M"
set datafile missing "-"

# Extract stats first.
set terminal unknown
# Could select more detailed timestamp for intra-day data periods.
set xdata time
plot for [i=3:finalcol] filename using 2:(column(i))
xmax = GPVAL_DATA_X_MAX
xmaxsd = strftime("%d", xmax)
xmaxst = strftime("%H:%MZ", xmax)
#show variables all

# Attempt to avoid slamming integral values into the grid and losing them.
# LHS of graph generally less interesting...
set auto fix
set offsets graph 0, graph 0.01, graph 0.02, graph 0.02

# Start to draw (PNG) output.
set format x "%d|%H"
set xdata time
set terminal svg size 1280,480
set output filename.".svg"
#if (exists("title")) set title title
set label title."\nto\n".xmaxsd."\n".xmaxst at screen 1, screen .3 right
set key outside
set key right
#set key box
#set lmargin 1.5
# Deliberately truncate left-most X-axis label on left as low-information.
#set lmargin 0
#set rmargin 13.5
#set tmargin 0.5
#set bmargin 1.5
#set grid xtics y2tics
#set border 0
#set xrange [0.1:12]
#set yrange [0:60]
#set xtics rotate
#set format y ""
#set noytics
#set y2tics
#set format y "%gV"
if (exists("unit")) set format y "%g".unit
#set tics scale 3
set style line 1 lc 3
plot for [i=3:finalcol] filename \
        using 2:(column(i)) title columnheader(i) with lines
