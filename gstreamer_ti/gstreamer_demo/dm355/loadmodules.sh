#!/bin/sh

# CMEM Allocation
#    1x3628800 Circular buffer
#    4x829440  Video buffers (max D1 PAL)
#    1x829440  Underlying software components (codecs, etc.)
#    1x518400  Underlying software components (codecs, etc.)
#    1x4948    Underlying software components (codecs, etc.)
#    1x1505280 Underlying software components (codecs, etc.)
#    1x5760    Underlying software components (codecs, etc.)
#    1x8192    Underlying software components (codecs, etc.)
#    1x1       Dummy buffer used during final flush
insmod cmemk.ko phys_start=0x87400000 phys_end=0x88000000 \
    pools=1x3628800,5x829440,1x518400,1x4948,1x1505280,1x5760,1x8192,1x1

./mapdmaq

insmod dm350mmap.ko
rm -f /dev/dm350mmap
mknod /dev/dm350mmap c `awk "\\$2==\"dm350mmap\" {print \\$1}" /proc/devices` 0
