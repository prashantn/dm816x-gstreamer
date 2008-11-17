insmod cmemk.ko phys_start=0x87800000 phys_end=0x88E00000 pools=20x4096,8x202752,10x131072,2x1048576,1x2097152,10x829440,1x6750000
insmod dsplinkk.ko ddr_start=0x8F800000  ddr_size=0x600000

rm -rf /dev/dsplink
mknod /dev/dsplink c `awk "\\$2==\"dsplink\" {print \\$1}" /proc/devices` 0
