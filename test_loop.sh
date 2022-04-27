#! /bin/bash

MIN=512
MAX=1024
make all
for ((N=1; N<=15; N++))
do
    V=$(shuf -i ${MIN}-${MAX} -n 1)
    echo $V
    for (( I=1; I<=5; I++ ))
    do
        ./tests_perf.sh "${V}"
    done
    MIN=$V
    MAX=$(($MAX * 2))
done
