#
# Default Memory Map
#
# Start Addr    Size    Description
# -------------------------------------------
# 0x80000000    88 MB   Linux
# 0x85800000    08 MB   CMEM
# 0x86800000    24 MB   DDRALGHEAP
# 0x87800000     6 MB   DDR2 (BIOS, Codecs, Applications)
# 0x87E00000     1 MB   DSPLINK (MEM)
# 0x87F00000     4 KB   DSPLINK (RESET)
# 0x87F01000  1020 KB   unused

insmod cmemk.ko phys_start=0x85800000 phys_end=0x86800000 pools=20x4096,8x131072,5x1500000,1x1429440,1x256000


# insert DSP/BIOS Link driver
#
insmod dsplinkk.ko

# make /dev/dsplink
rm -f /dev/dsplink
mknod /dev/dsplink c `awk "\\$2==\"dsplink\" {print \\$1}" /proc/devices` 0


# insert Local Power Manager driver
#
insmod lpm_omap3530.ko
