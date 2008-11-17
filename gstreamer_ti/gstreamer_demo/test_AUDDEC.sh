export LD_LIBRARY_PATH=.:/opt/system_files_gstreamer:/opt/gstreamer/lib
export GST_PLUGIN_PATH=/opt/gstreamer/lib/gstreamer-0.10/
export PATH=$PATH:/opt/gstreamer/bin

#DEBUG="--gst-debug-no-color --gst-debug=osssink:5"
#DEBUG="--gst-debug-no-color --gst-debug=TIAuddec:5"
#DEBUG="--gst-debug-no-color --gst-debug=TIDmaiBufferTransport:5"
#DEBUG=-v

gst-launch ${DEBUG} filesrc location=$1 ! TIAuddec genTimeStamps=FALSE engineName="decode" codecName="mp3dec" ! audioconvert ! volume volume=5 ! osssink
