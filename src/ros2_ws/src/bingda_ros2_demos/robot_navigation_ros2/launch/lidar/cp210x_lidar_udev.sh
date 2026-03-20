#!/bin/bash

echo  'KERNEL=="ttyUSB*", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="10c4", MODE:="0777", GROUP:="dialout",  SYMLINK+="lidar"' >/etc/udev/rules.d/lidar.rules

service udev reload
sleep 1
service udev restart