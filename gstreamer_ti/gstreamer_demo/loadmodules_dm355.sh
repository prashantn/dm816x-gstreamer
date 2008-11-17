#!/bin/sh

# 12MB
insmod cmemk.ko phys_start=0x87400000 phys_end=0x88000000 pools=1x2903040,1x1529856,7x829440,1x524288,1x108680,1x81920,2x8192,6x4096

./mapdmaq

insmod dm350mmap.ko
rm -f /dev/dm350mmap
mknod /dev/dm350mmap c `awk "\\$2==\"dm350mmap\" {print \\$1}" /proc/devices` 0
