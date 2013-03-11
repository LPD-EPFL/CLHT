#!/bin/bash

read -p "Enter the max number of cores: " NUM_CORES

duration=1000
buckets="512 16";
rw="0'1 0.1'0.9";

fill_rate=0.5;
payload_size=64;

executables=$(ls throughput_*);
cores=$(seq 1 1 $NUM_CORES);

for bu in $buckets
do
    num_elems=$((24*$bu));

    for r in $rw
    do 
	u=$(echo $r | cut -d"'" -f1);
	g=$(echo $r | cut -d"'" -f2);
	echo "#buckets: $bu / update: $u / get: $g";

	out_dat="data/throughput.b"$bu"_u"$u"_diff_locks.dat"

	printf "#  " | tee $out_dat;
	for f in $(ls throughput_*)
	do
	    prim=$(echo $f | cut -d'_' -f2);
	    printf "%-11s" $prim | tee -a  $out_dat;
	done;
	echo "" | tee -a  $out_dat;

	for c in $cores
	do
	    printf "%-3u" $c | tee -a $out_dat;
	    for ex in $executables
	    do
		p="$bu $c $num_elems $fill_rate $payload_size $duration $u $g";
		./$ex $p | gawk '// { printf "%-11d", $2 }' | tee -a  $out_dat;
	    done;
	    echo "" | tee -a  $out_dat;
	done;
    done;
done;
