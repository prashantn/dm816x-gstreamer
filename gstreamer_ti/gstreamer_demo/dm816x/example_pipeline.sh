#!/bin/sh

# export environment
export GST_REGISTRY=/tmp/gst_registry.bin
export LD_LIBRARY_PATH=/opt/gstreamer/lib
export GST_PLUGIN_PATH=/opt/gstreamer/lib/gstreamer-0.10
export PATH=/opt/gstreamer/bin:$PATH

# setup fbdev in RBG565 24-bit mode - this mainly because fbdevsink does not like RBG888
fbset -depth 24 -rgba 5/11,6/5,5/0,0/0

# pipeline decode elemenatry H.264 stream
gst-launch filesrc location=sample.264 ! typefind ! h264parse access-unit=true ! omx_h264dec ! swcsc ! ffmpegcolorspace ! gstperf ! fbdevsink  -v

# pipeline to decode video from MP4 container
gst-launch filesrc location=sample.mp4 ! typefind ! qtdemux name=demux demux.video_00 ! nal2bytestream_h264 ! omx_h264dec ! swcsc ! ffmpegcolorspace ! gstperf ! fbdevsink -v


