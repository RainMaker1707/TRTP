#! /bin/bash


if [ "$#" -lt 1 ]; then
  echo "You need to provide the number of bytes for the file transfer"
elif [ "$1" -lt 1 ] || [ "$1" -gt 134217728 ]; then # 0 < $1 < 125Mo
  echo "You need to provide at least 1 bytes  and at most 50Go file"
else
    dd if=/dev/urandom of=input_file bs=1 count=$1 &> /dev/null  # generate random file of $1 bytes
    echo "File of $1 bytes created"
    # first column number of bytes to transfer, second column time to transfer
    /usr/bin/time -o perf.csv -a -f  "${1}, %e" ./test.sh "${1}"
    rm -f input_file received_file diff_file # garbage
fi
