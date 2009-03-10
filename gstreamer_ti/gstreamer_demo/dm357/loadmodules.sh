# insert cmemk, tell it to occupy physical 248MB-256MB.
insmod cmemk.ko phys_start=0x8F800000 phys_end=0x90000000 pools=1x5260000,3x829440,1x61440,1x10240

# insert dsplinkk
insmod dsplinkk.ko

# make /dev/dsplink
rm -f /dev/dsplink
mknod /dev/dsplink c `awk "\\$2==\"dsplink\" {print \\$1}" /proc/devices` 0

