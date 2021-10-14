#!/bin/bash
# for testing
set -e

test_cmd=vlc -v rtsp://192.167.0.105:8554/mjpeg/1

for k in $( seq 1 2 )
do
    $test_cmd
    sleep 10
done
