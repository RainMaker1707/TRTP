#! /bin/bash

(trap 'kill 0' SIGINT;
 ./receiver -s stats_rec.csv :: 8088 1>received_file  2>receiver.log &
 (sleep .1 &&
 ./sender -s stats_send.csv ::1 8088 2>sender.log <input_file 
 ))
