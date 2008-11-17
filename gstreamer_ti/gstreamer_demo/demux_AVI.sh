export LD_LIBRARY_PATH=.:/opt/system_files_gstreamer:/opt/gstreamer/lib
export GST_PLUGIN_PATH=/opt/gstreamer/lib/gstreamer-0.10/
export PATH=$PATH:/opt/gstreamer/bin

#DEBUG="--gst-debug-no-color --gst-debug=avidemux:5"
#DEBUG=-v

gst-launch ${DEBUG} filesrc location=$1 ! avidemux name=demux \
   demux.audio_00 ! queue max-size-buffers=180 ! filesink location="audio.mp3"\
   demux.video_00 ! queue max-size-buffers=60  ! filesink location="video.m4v"

