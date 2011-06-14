#!/bin/sh

# export environment
export GST_REGISTRY=/tmp/gst_registry.bin
export LD_LIBRARY_PATH=/opt/gstreamer/lib
export GST_PLUGIN_PATH=/opt/gstreamer/lib/gstreamer-0.10
export PATH=/opt/gstreamer/bin:$PATH

# disable the graphics plane so that we can see video
echo 0 > /sys/devices/platform/vpss/graphics0/enabled

# pipeline decode elemenatry H.264 stream
gst-launch filesrc location=sample.264  ! typefind ! h264parse access-unit=true ! omx_h264dec ! omx_colorconv ! omx_ctrl display-mode=OMX_DC_MODE_1080P_60 ! gstperf ! omx_videosink -v

# pipeline to decode MP4 container (H.264 + AAC). Note that currently AV is disabled.
gst-launch -v filesrc location=sample.mp4 ! qtdemux name=demux demux.audio_00 ! queue max-size-buffers=8000 max-size-time=0 max-size-bytes=0 ! faad ! alsasink sync=false demux.video_00 ! queue !  nal2bytestream_h264  !  omx_h264dec ! omx_colorconv ! omx_ctrl display-mode=OMX_DC_MODE_1080P_60  ! gstperf ! omx_videosink sync=false

