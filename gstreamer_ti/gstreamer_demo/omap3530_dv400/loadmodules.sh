#
# Default Memory Map
#
# Start Addr    Size    Description
# -------------------------------------------
# 0x80000000    99 MB   Linux
# 0x86300000    16 MB   CMEM
# 0x87300000    13 MB   CODEC SERVER

# remove previously inserted cmemk to ensure that we are using the right pool configuration.
rmmod cmemk

#
# CMEM Allocation
#    1x5250000 Circular buffer
#    6x829440  Video buffers (max D1 PAL)
#    1x345600  Underlying software components (codecs, etc.)
#    1x1       Dummy buffer used during final flush
modprobe cmemk phys_start=0x86300000 phys_end=0x87300000 \
    pools=1x5250000,6x829440,1x345600,1x1

# insert DSP/BIOS Link driver
modprobe dsplinkk

# make /dev/dsplink
rm -f /dev/dsplink
mknod /dev/dsplink c `awk "\\$2==\"dsplink\" {print \\$1}" /proc/devices` 0

# insert Local Power Manager driver
modprobe lpm_omap3530

# insert SDMA driver
modprobe sdmak

