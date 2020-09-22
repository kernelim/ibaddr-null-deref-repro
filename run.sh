#!/bin/bash

echo
echo "This script needs to be modified for local IPs in your network."
echo "Both existing and non-existing IPs should be used for local and remote addreses."
echo

while [ 1 ] ; do
    for i in `seq 0 100`; do
	echo 10.101.127.11,10.101.127.201 > /sys/module/ibtest/parameters/callme
	echo 10.101.127.11,10.101.127.240 > /sys/module/ibtest/parameters/callme
	echo 10.101.127.12,10.101.127.201 > /sys/module/ibtest/parameters/callme
	echo 10.101.127.12,10.101.127.240 > /sys/module/ibtest/parameters/callme
	echo 10.101.127.13,10.101.127.201 > /sys/module/ibtest/parameters/callme
	echo 10.101.127.13,10.101.127.240 > /sys/module/ibtest/parameters/callme
	sleep 0.02
	cat /sys/module/ibtest/parameters/callme
    done

    sleep 10
done
