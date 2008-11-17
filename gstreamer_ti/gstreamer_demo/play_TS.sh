export LD_LIBRARY_PATH=.:/opt/gstreamer/lib
export GST_PLUGIN_PATH=/opt/gstreamer/lib/gstreamer-0.10/
export PATH=$PATH:/opt/gstreamer/bin

#DEBUG="--gst-debug-no-color --gst-debug=TIAuddec:5"
#DEBUG="--gst-debug-no-color --gst-debug=TIViddec:5"
#DEBUG="--gst-debug-no-color --gst-debug=flutsdemux:4"
#DEBUG="--gst-debug-no-color --gst-debug=TIDmaiBufferTransport:5"
#DEBUG="--gst-debug-no-color --gst-debug=TIViddec:5 --gst-debug=TIAuddec:5"
#DEBUG=-v

# Launch line for FBDev
gst-launch ${DEBUG} filesrc location=$1 ! typefind ! flutsdemux name=demux \
 demux. ! 'video/x-h264' ! queue max-size-buffers=60 ! typefind ! TIViddec ! TIDmaiVideoSink displayDevice=/dev/fb/3 displayStd=fbdev \
 demux. ! 'audio/mpeg' ! queue max-size-buffers=12000 ! typefind ! TIAuddec ! audioconvert ! volume volume=5  ! osssink 
