export LD_LIBRARY_PATH=.:/opt/system_files_gstreamer:/opt/gstreamer/lib
export GST_PLUGIN_PATH=/opt/gstreamer/lib/gstreamer-0.10/
export PATH=$PATH:/opt/gstreamer/bin

#DEBUG="--gst-debug-no-color --gst-debug=*:2"
#DEBUG="--gst-debug-no-color --gst-debug=TIViddec2:5"
#DEBUG="--gst-debug-no-color --gst-debug=mad:5"
#DEBUG="--gst-debug-no-color --gst-debug=avidemux:5"
#DEBUG="--gst-debug-no-color --gst-debug=TIDmaiVideoSink:5"
#DEBUG="--gst-debug-no-color --gst-debug=TIDmaiBufTrans:5"

nice -n -20 gst-launch ${DEBUG} filesrc location=$1 ! typefind ! avidemux name=demux \
 demux.audio_00 ! queue max-size-buffers=1200 max-size-time=0 max-size-bytes=0 ! typefind ! mad ! audioconvert ! volume volume=5  ! osssink  \
 demux.video_00 ! queue max-size-buffers=60 ! typefind ! TIViddec2 engineName="decode" codecName="mpeg4dec" displayBuffer=FALSE !  TIDmaiVideoSink  displayDevice=/dev/fb/3 displayStd=fbdev videoStd=D1_NTSC
