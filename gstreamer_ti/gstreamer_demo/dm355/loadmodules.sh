#!/bin/sh

# Sample bootargs is given below
# mem=93M console=ttyS0,115200n8 root=/dev/nfs rw nfsroot=<nfsroot> ip=dhcp video=davincifb:vid0=OFF:vid1=OFF:osd0=720x576x16,2025K

rmmod cmemk 2>/dev/null
rmmod irqk 2>/dev/null
rmmod edmak 2>/dev/null
rmmod dm350mmap 2>/dev/null

# Handle any of the use-cases below
insmod cmemk.ko phys_start=0x87500000 phys_end=0x88000000 pools=1x3629056,1x1531904,7x831488,2x16384,1x8192,73x4096

# Image encode
#insmod cmemk.ko phys_start=0x87500000 phys_end=0x88000000 pools=1x1658880,1x831488,1x45056,48x4096

# Image decode
#insmod cmemk.ko phys_start=0x87500000 phys_end=0x88000000 pools=1x3629056,1x692224,1x12288,55x4096

# Resize Loopback
#insmod cmemk.ko phys_start=0x87500000 phys_end=0x88000000 pools=3x831488

# MPEG-4 Encode
#insmod cmemk.ko phys_start=0x87500000 phys_end=0x88000000 pools=1x1658880,1x1531904,3x622592,1x81920,2x16384,1x8192,68x4096

# MPEG-4 Decode
#insmod cmemk.ko phys_start=0x87500000 phys_end=0x88000000 pools=1x2904064,1x1507328,4x831488,1x12288,2x8192,73x4096

./mapdmaq

insmod irqk.ko 
insmod edmak.ko
insmod dm350mmap.ko

rm -f /dev/dm350mmap
mknod /dev/dm350mmap c `awk "\\$2==\"dm350mmap\" {print \\$1}" /proc/devices` 0
