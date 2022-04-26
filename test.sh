#! /bin/bash



if [ "$#" -lt 1 ]; then
  echo "You need to provide the number of bytes for the file transfer"
elif [ "$1" -lt 1 ] || [ "$1" -gt 134217728 ]; then # 0 < $1 < 125Mo
  echo "You need to provide at least 1 bytes  and at most 50Go file"
else
    dd if=/dev/urandom of=input_file bs=1 count=$1 &> /dev/null  # generate random file of $1 bytes
    echo "File of $1 bytes created"
    make all
    # /usr/bin/time -o ${PHILO_PATH} -a -f  "${I}, %e" make philo N=${I} -s
    /usr/bin/time -o perf.csv -a -f "${1}, %e"
    (trap 'kill 0' SIGINT;
      ./sender -s stats_send.csv ::1 8088 2>sender.log <input_file &
      ./receiver -s stats_rec.csv :: 8088 1>received_file  2>receiver.log
      )
    wait
    diff input_file received_file > diff_file # extract diff and store ii in diff_file
    value=$(wc -c diff_file | xargs | cut -d ' ' -f1) # extract  the size in bytes of diff_file
    if [ $value -eq 0 ]; then
        echo "Test: all ok"
    elif [ $value -gt 0 ]; then
        echo "Test: file corrupted"
    else
        echo "Test: error on diff_file creation"
    fi
    rm -f input_file received_file diff_file # garbage
fi

#dd if=/dev/urandom of=input_file bs=1 count=512 &> /dev/null  # generate random file of 512 bytes
#rm -f received_file input_file # clean after test
