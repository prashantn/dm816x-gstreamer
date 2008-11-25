#
# loadmodules.sh
#
# Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation version 2.1 of the License.
#
# This program is distributed #as is# WITHOUT ANY WARRANTY of any kind,
# whether express or implied; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# insert cmemk, tell it to occupy physical 120MB-128MB and create enough
# contiguous buffers for the worst case requirements of the demos.
insmod cmemk.ko phys_start=0x87800000 phys_end=0x8ba00000 pools=1x4147200,10x3458400,10x1434240,11x663552,4x60000

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
