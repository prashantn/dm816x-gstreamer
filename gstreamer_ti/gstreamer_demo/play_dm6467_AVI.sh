export LD_LIBRARY_PATH=.:/opt/gstreamer/lib
export GST_PLUGIN_PATH=/opt/gstreamer/lib/gstreamer-0.10/
export PATH=$PATH:/opt/gstreamer/bin

#DEBUG="--gst-debug-no-color --gst-debug=TIViddec2:5"
#DEBUG="--gst-debug-no-color --gst-debug=avidemux:5"
#DEBUG="--gst-debug-no-color --gst-debug=GST_PLUGIN_LOADING:5"
#DEBUG=-v


# Launch line for V4L2
gst-launch ${DEBUG} filesrc location=$1 ! typefind ! avidemux name=demux \
 demux.audio_00 ! queue max-size-buffers=1200 max-size-time=0 max-size-bytes=0 ! typefind ! mad ! osssink \
 demux.video_00 ! typefind ! TIViddec2 !  TIDmaiVideoSink videoStd=720P_60

