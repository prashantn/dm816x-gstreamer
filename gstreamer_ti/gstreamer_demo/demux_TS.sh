export LD_LIBRARY_PATH=.:/opt/gstreamer/lib
export GST_PLUGIN_PATH=/opt/gstreamer/lib/gstreamer-0.10/
export PATH=$PATH:/opt/gstreamer/bin

#DEBUG="--gst-debug-no-color --gst-debug=flutsdemux:5"
#DEBUG=-v

gst-launch ${DEBUG} filesrc location=$1 ! typefind ! flutsdemux name=demux \
 demux. ! 'audio/mpeg' ! queue max-size-buffers=180 ! filesink location="audio.mp2" \
 demux. ! 'video/x-h264' ! queue max-size-buffers=60 ! filesink location="video.264"
