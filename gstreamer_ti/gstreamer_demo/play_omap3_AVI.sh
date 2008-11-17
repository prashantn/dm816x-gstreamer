#!/bin/sh
# This script can be used to play AVI files on OMAP3530 EVM
#
# set the env's
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


gst-launch ${DEBUG} filesrc location=$1 ! typefind ! avidemux name=demux \
 demux.audio_00 ! queue max-size-buffers=1200 max-size-time=0 max-size-bytes=0 ! typefind ! mad ! audioconvert ! 'audio/x-raw-int, width=16, depth=16' !  alsasink  \
 demux.video_00 ! queue max-size-buffers=60 ! typefind ! TIViddec engineName="decode" codecName="mpeg4dec" !  TIDmaiVideoSink displayDevice=/dev/video1 displayStd=v4l2 videoStd=VGA videoOutput=LCD rotation=90
