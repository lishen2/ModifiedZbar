#!/bin/bash

rm pid > /dev/null 2>&1

raspiyuv --timeout 0 --width 1792 --height 1344 --signal --output - | ./stdin_test --width 1792 --height 1344 --pidfile pid &
ps -eo pid,comm | grep raspiyuv | sed -n 's/[^0-9]*\([0-9]\{1,\}\).*/\1/p' - > pid

#./stdin_test raspiyuv --exposure antishake --width 800 --height 600 --timeout 0 --signal --output img.bin

