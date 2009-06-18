#!/bin/sh

# insert cmemk, tell it to occupy physical 120MB-128MB and create enough
# contiguous buffers for the worst case requirements of the demos.
#
# CMEM Allocation
#     1x8294400 Circular buffer
#    11x3670016 Video buffers (max 1080p)
#     3x2304000 Underlying software components (codecs, etc.)
#     3x1434240 Underlying software components (codecs, etc.)
#     1x1       Dummy buffer used during final flush
insmod cmemk.ko phys_start=0x87800000 phys_end=0x8ba00000 \
    pools=1x8294400,11x3670016,3x2304000,3x1434240,1x1

# insert dsplinkk, tell it that DSP's DDR is at physical 250MB-254MB
insmod dsplinkk.ko

# alter dma queue mapping for better visual performance
if [ -f mapdmaq-hd ]
then
    ./mapdmaq-hd
fi

# make /dev/dsplink
rm -f /dev/dsplink
mknod /dev/dsplink c `awk "\\$2==\"dsplink\" {print \\$1}" /proc/devices` 0

