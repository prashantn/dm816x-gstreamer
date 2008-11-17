export LD_LIBRARY_PATH=.:/opt/gstreamer/lib
export GST_PLUGIN_PATH=/opt/gstreamer/lib/gstreamer-0.10/
export PATH=$PATH:/opt/gstreamer/bin

#DEBUG="--gst-debug-no-color --gst-debug=TIAuddec:5"
#DEBUG="--gst-debug-no-color --gst-debug=TIViddec:5"
#DEBUG="--gst-debug-no-color --gst-debug=avidemux:5"
#DEBUG="--gst-debug-no-color --gst-debug=TIDmaiBufferTransport:5"
#DEBUG="--gst-debug-no-color --gst-debug=TIViddec:5 --gst-debug=TIAuddec:5"
#DEBUG="--gst-debug-no-color --gst-debug=queue:5 --gst-debug=TIViddec:5"
#DEBUG="--gst-debug-no-color --gst-debug=GST_PLUGIN_LOADING:5"
#DEBUG=-v


# Launch line for FBDev
#gst-launch ${DEBUG} filesrc location=$1 ! typefind ! avidemux name=demux \
# demux.audio_00 ! queue max-size-buffers=180 ! typefind ! TIAuddec ! audioconvert ! volume volume=5  ! osssink  \
# demux.video_00 ! queue max-size-buffers=60 ! typefind ! TIViddec !  TIDmaiVideoSink displayDevice=/dev/fb/3 displayStd=fbdev

# Launch line for V4L2
cat /dev/zero > /dev/fb/2
gst-launch ${DEBUG} filesrc location=$1 ! typefind ! avidemux name=demux \
 demux.audio_00 ! queue max-size-buffers=180 ! typefind ! TIAuddec ! audioconvert ! volume volume=5  ! osssink  \
 demux.video_00 ! queue max-size-buffers=60 ! typefind ! TIViddec !  TIDmaiVideoSink 
