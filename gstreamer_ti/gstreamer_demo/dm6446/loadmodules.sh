#!/bin/sh

# insert cmemk, tell it to occupy physical 118MB-128MB. 
#
# CMEM Allocation
#    1x5250000 Circular buffer
#    2x829440  Video buffers (max D1 PAL)
#    3x1036800 Video buffers (larger size needed for MPEG4 encoder)
#    1x8192    Underlying software components (codecs, etc.)
#    1x1       Dummy buffer used during final flush
insmod cmemk.ko phys_start=0x87600000 phys_end=0x88000000 \
    pools=1x5250000,2x829440,3x1036800,1x8192,1x1

# Notes on using the "playbin" element:
# -------------------------------------
#    Playbin requires one 6 video buffers, and we have only allocated five.
#    If you replace the "2x829440,3x1036800" with "6x829440" it should work, but
#    you will not be able to use the MPEG4 encoder.
#
#insmod cmemk.ko phys_start=0x87600000 phys_end=0x88000000 \
#    pools=1x5250000,6x829440,1x8192,1x1

# insert dsplinkk
insmod dsplinkk.ko

# make /dev/dsplink
rm -f /dev/dsplink
mknod /dev/dsplink c `awk "\\$2==\"dsplink\" {print \\$1}" /proc/devices` 0

