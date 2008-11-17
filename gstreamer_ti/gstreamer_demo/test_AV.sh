export LD_LIBRARY_PATH=.:/opt/system_files_gstreamer:/opt/gstreamer/lib
export GST_PLUGIN_PATH=/opt/gstreamer/lib/gstreamer-0.10/
export PATH=$PATH:/opt/gstreamer/bin

#DEBUG="--gst-debug-no-color --gst-debug=TIViddec:5"
#DEBUG="--gst-debug-no-color --gst-debug=TIDmaiBufTrans:5"

gst-launch ${DEBUG} filesrc location=$1 name=v filesrc location=$2 name=a v. ! queue max-size-buffers=60 ! TIViddec engineName="decode" codecName="h264dec" ! TIDmaiVideoSink displayDevice=/dev/fb/3 displayStd=fbdev a. ! queue max-size-buffers=180 ! TIAuddec engineName="decode" codecName="mp3dec" ! audioconvert ! volume volume=5 ! osssink
