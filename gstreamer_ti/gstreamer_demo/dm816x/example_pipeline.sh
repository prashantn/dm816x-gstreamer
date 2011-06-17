#!/bin/sh

# export environment
export GST_REGISTRY=/tmp/gst_registry.bin
export LD_LIBRARY_PATH=/opt/gstreamer/lib
export GST_PLUGIN_PATH=/opt/gstreamer/lib/gstreamer-0.10
export PATH=/opt/gstreamer/bin:$PATH
export GST_PLUGIN_SCANNER=/opt/gstreamer/libexec/gstreamer-0.10/gst-plugin-scanner

# pipeline decode elemenatry H.264 stream
gst-launch filesrc location=sample.264  ! typefind ! h264parse access-unit=true ! omx_h264dec ! omx_colorconv ! omx_ctrl display-mode=OMX_DC_MODE_1080P_60 ! gstperf ! omx_videosink sync=false -v

# pipeline to decode MP4 container (H.264 + AAC). Note that currently AV is disabled.
gst-launch -v filesrc location=sample.mp4 ! qtdemux name=demux demux.audio_00 ! queue max-size-buffers=8000 max-size-time=0 max-size-bytes=0 ! faad ! alsasink sync=false demux.video_00 ! queue !  nal2bytestream_h264  !  omx_h264dec ! omx_colorconv ! omx_ctrl display-mode=OMX_DC_MODE_1080P_60  ! gstperf ! omx_videosink sync=false

# pipeline to display the videotest pattern using omx sink
gst-launch -v videotestsrc ! omx_ctrl display-mode=OMX_DC_MODE_1080P_60  ! omx_videosink sync=false 

# pipeline to color convert videotestsrc pattern from NV12 to YUY2 using HW accelerated omx element and then display using omx sink
gst-launch -v videotestsrc ! omx_colorconv ! omx_ctrl display-mode=OMX_DC_MODE_1080P_60  ! omx_videosink sync=false


