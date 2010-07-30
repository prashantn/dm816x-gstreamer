#!/bin/sh
rmmod cmemk 2>/dev/null
rmmod irqk 2>/dev/null
rmmod edmak 2>/dev/null
rmmod dm365mmap 2>/dev/null

# Pools configuration
insmod cmemk.ko phys_start=0x84d00000 phys_end=0x88000000 pools=1x16539648,1x4841472,4x1843200,14x1646592,1x282624,1x176128,1x147456,1x69632,1x61440,1x32768,2x20480,1x16384,1x12288,4x8192,69x4096

insmod irqk.ko 
insmod edmak.ko
insmod dm365mmap.ko

rm -f /dev/dm365mmap
mknod /dev/dm365mmap c `awk "\\$2==\"dm365mmap\" {print \\$1}" /proc/devices` 0
