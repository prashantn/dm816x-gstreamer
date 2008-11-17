export LD_LIBRARY_PATH=.:/opt/system_files_gstreamer:/opt/gstreamer/lib
export GST_PLUGIN_PATH=/opt/gstreamer/lib/gstreamer-0.10/
export PATH=$PATH:/opt/gstreamer/bin

#DEBUG="--gst-debug-no-color --gst-debug=TIViddec:5"
#DEBUG="--gst-debug-no-color --gst-debug=TIDmaiBufTrans:5"

gst-launch ${DEBUG} filesrc location=$1 ! TIViddec engineName="decode" codecName="mpeg4dec" ! TIDmaiVideoSink displayDevice=/dev/fb/3 displayStd=fbdev
