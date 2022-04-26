#! /bin/bash

MIN=1
MAX=268435456

for (( I=1; I<=100; I++ ))
do
    V=$(shuf -i MIN-MAX -n 1)
  ./tests_perf.sh "${V}"
done