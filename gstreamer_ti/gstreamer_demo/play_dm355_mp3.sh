export LD_LIBRARY_PATH=.:/opt/system_files_gstreamer:/opt/gstreamer/lib
export GST_PLUGIN_PATH=/opt/gstreamer/lib/gstreamer-0.10/
export PATH=$PATH:/opt/gstreamer/bin

DEBUG="--gst-debug-no-color --gst-debug=*:2"

gst-launch ${DEBUG} filesrc location=$1 ! mad ! osssink
