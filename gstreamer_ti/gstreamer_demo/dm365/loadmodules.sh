#!/bin/sh
rmmod cmemk 2>/dev/null
rmmod irqk 2>/dev/null
rmmod edmak 2>/dev/null
rmmod dm365mmap 2>/dev/null

# Pools configuration
insmod cmemk.ko phys_start=0x85400000 phys_end=0x88000000 pools=1x2097152,7x829440,1x524288,1x108680,1x81920,2x8192,6x4096,8x96,64x56,1x2938880,1x6650880,4x675840,1x75840,8x1548288,1x5988352,1x6045696

insmod edmak.ko
insmod irqk.ko
insmod dm365mmap.ko
lsmod

