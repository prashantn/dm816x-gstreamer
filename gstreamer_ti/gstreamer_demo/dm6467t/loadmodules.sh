rmmod cmemk.ko 2>/dev/null
rmmod dsplinkk.ko 2>/dev/null 

# insert cmemk, tell it to occupy physical 128MB-186MB and create enough
# contiguous buffers for the worst case requirements of typical GStreamer
# example use-cases.
insmod cmemk.ko phys_start=0x88000000 phys_end=0x8ba00000 pools=1x8294400,1x3686400,9x3239936,3x3215360,1x1437696,1x4096

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

