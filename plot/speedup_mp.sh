#! /bin/sh
export seq=$(cat ../results/sequential.txt | awk '{ print $2 }')

gnuplot <<EOF
# Other UNIX (Linux, etc.) specific stuff
#gnuplot <</EOF

#! /usr/bin/gnuplot

set xlabel "Number of threads"
set ylabel "Speed-up [n core / sequential]"
set title "Speed-up of message passing"

#nomirror - no tics on the right and top
#scale - the size of the tics
set xtics auto nomirror scale 2
set ytics auto nomirror scale 2

#remove top and right borders
set border 3 back
#add grid
set style line 12 lc rgb '#808080' lt 2 lw 1
set grid back ls 12

set xrange [0:]
set yrange [0:]

#the size of the graph
set size 2.0, 1.0

#some nice line colors
#lc: line color; lt: line type (1 - continuous); pt: point type
#ps: point size; lw: line width
set style line 1 lc rgb '#0060ad' lt 1 pt 2 ps 2 lw 5
set style line 2 lc rgb '#dd181f' lt 1 pt 5 ps 2 lw 5
set style line 3 lc rgb '#8b1a0e' pt 1 ps 2 lt 1 lw 5
set style line 4 lc rgb '#5e9c36' pt 6 ps 2 lt 1 lw 5
set style line 5 lc rgb '#663399' lt 1 pt 3 ps 2 lw 5
set style line 6 lc rgb '#cc6600' lt 1 pt 4 ps 2 lw 5
set style line 7 lc rgb '#cccc00' lt 1 pt 7 ps 2 lw 5
set style line 8 lc rgb '#299fff' lt 1 pt 8 ps 2 lw 5

#move the legend to a custom position (can also be moved to absolute coordinates)
set key left top

set output "speedup_mp.eps"
set terminal postscript color  "Helvetica" 24 eps enhanced

#set term tikz standalone color solid size 5in,3in
#set output "test.tex"

#for more details on latex, see http://www.gnuplotting.org/introduction/output-terminals/
#set term epslatex #size 3.5, 2.62 #color colortext
#size can also be in cm: set terminal epslatex size 8.89cm,6.65cm color colortext
#set output "test.eps"

plot \
"../results/throughput_mp.txt" using 1:($2/system("echo $seq")) title "Message Passing" ls 1 with linespoints

EOF