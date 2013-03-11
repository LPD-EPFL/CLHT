#!/bin/sh

buckets="512 16";
rw="0'1 0.1'0.9";

for bu in $buckets
do
    echo "-- Number of buckets: $bu";
    num_elems=$((24*$bu));

    for r in $rw
    do 
	u=$(echo $r | cut -d"'" -f1);
	g=$(echo $r | cut -d"'" -f2);

	echo "U / get percentage: $u  $g";

	gp="gp/ssht.b"$bu"_u"$u".gp";
	out="plots/ssht.b"$bu"_u"$u".eps";
	dat="../data/throughput.b"$bu"_u"$u"_diff_locks.dat";
	title="Buckets: $bu / Range: $num_elems / Update: $u";


	cp cssht.gp $gp

cat << EOF >> $gp

set title "$title"
set output "$out"

plot \\
"$dat" using 1:($10/1e6) title  "TTAS" ls 1 with linespoints, \\
"$dat" using 1:($2/1e6) title  "Array" ls 2 with linespoints, \\
"$dat" using 1:($6/1e6) title  "MCS" ls 3 with linespoints, \\
"$dat" using 1:($5/1e6) title  "HTicket" ls 8 with linespoints, \\
"$dat" using 1:($8/1e6) title  "Spinlock" ls 9 with linespoints, \\
"$dat" using 1:($4/1e6) title  "HCLH" ls 4 with linespoints, \\
"$dat" using 1:($3/1e6) title  "CLH" ls 5 with linespoints, \\
"$dat" using 1:($7/1e6) title  "Mutex" ls 7 with linespoints, \\
"$dat" using 1:($9/1e6) title  "Ticket" ls 6 with linespoints 

EOF

gnuplot $gp

    done;
done;

