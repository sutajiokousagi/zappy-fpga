#!/bin/bash

echo "24" > /sys/class/gpio/export
sleep 0.1
echo "out" > /sys/class/gpio/gpio24/direction
sleep 0.1
echo 0 > /sys/class/gpio/gpio24/value
sleep 1
echo 1 > /sys/class/gpio/gpio24/value
sleep 0.1
echo "in" > /sys/class/gpio/gpio24/direction
sleep 0.1
echo "24" > /sys/class/gpio/unexport
