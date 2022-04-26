#! /bin/bash

MIN=1
MAX=26843

make all
for ((N=1; N<=100; N++))
do
    V=$(shuf -i ${MIN}-${MAX} -n 1)
    for (( I=1; I<=10; I++ ))
    do
        ./tests_perf.sh "${V}"
    done
done
