#!/bin/sh
rmmod cmemk 2>/dev/null
rmmod irqk 2>/dev/null
rmmod edmak 2>/dev/null
rmmod dm365mmap 2>/dev/null

# Pools configuration
insmod cmemk.ko phys_start=0x85400000 phys_end=0x88000000 \
    pools=1x6651904,1x3670016,18x1548288,1x282624,1x159744,1x49152,1x32768,1x28672,1x16384,3x12288,2x8192,36x4096

insmod irqk.ko 
insmod edmak.ko
insmod dm365mmap.ko
