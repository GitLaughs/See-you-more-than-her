#! /bin/bash

### BEGIN INIT
gnome-terminal -- bash -c "killall -wg ros2"
sleep 2

wait
exit 0
