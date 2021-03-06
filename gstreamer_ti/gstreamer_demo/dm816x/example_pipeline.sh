#!/bin/sh

# export environment
export GST_REGISTRY=/tmp/gst_registry.bin
export LD_LIBRARY_PATH=/opt/gstreamer/lib
export GST_PLUGIN_PATH=/opt/gstreamer/lib/gstreamer-0.10
export PATH=/opt/gstreamer/bin:$PATH
export GST_PLUGIN_SCANNER=/opt/gstreamer/libexec/gstreamer-0.10/gst-plugin-scanner

# pipeline decode elemenatry H.264 stream
gst-launch filesrc location=sample.264  ! typefind ! h264parse access-unit=true ! omx_h264dec ! omx_scaler ! omx_ctrl display-mode=OMX_DC_MODE_1080P_60 ! gstperf ! omx_videosink sync=false -v

# pipeline to decode MP4 container (H.264 + AAC). Note that currently AV is disabled.
gst-launch -v filesrc location=sample.mp4 ! qtdemux name=demux demux.audio_00 ! queue max-size-buffers=8000 max-size-time=0 max-size-bytes=0 ! faad ! alsasink sync=false demux.video_00 ! queue !  nal2bytestream_h264  !  omx_h264dec ! omx_scaler ! omx_ctrl display-mode=OMX_DC_MODE_1080P_60  ! gstperf ! omx_videosink sync=false

# pipeline to display the videotest pattern using omx sink
gst-launch -v videotestsrc ! omx_ctrl display-mode=OMX_DC_MODE_1080P_60  ! omx_videosink sync=false 

# pipeline to color convert videotestsrc pattern from NV12 to YUY2 using HW accelerated omx element and then display using omx sink
gst-launch -v videotestsrc ! omx_scaler ! omx_ctrl display-mode=OMX_DC_MODE_1080P_60  ! omx_videosink sync=false

# pipeline to scale the decoded video to 720P using omx_scaler element
gst-launch filesrc location=sample.264  ! typefind ! h264parse access-unit=true ! omx_h264dec ! omx_scaler ! 'video/x-raw-yuv,width=1280,height=720' ! omx_ctrl display-mode=OMX_DC_MODE_1080P_60 ! gstperf ! omx_videosink sync=false -v

# pipeline to scale the QVGA video test pattern to VGA
gst-launch -v videotestsrc ! 'video/x-raw-yuv,width=320,height=240' ! omx_scaler ! 'video/x-raw-yuv,width=640,height=480' ! omx_ctrl display-mode=OMX_DC_MODE_1080P_60 ! omx_videosink sync=false -v

# pipeline to encode videotest pattern in H.264
gst-launch -v videotestsrc num-buffers=1000 ! omx_h264enc ! filesink location=sample.264 

# play mp4 using playbin2
gst-launch playbin2 uri=file:///home/root/sample.mp4 -v 

# playback rtsp stream
gst-launch rtspsrc location=rtsp://<server_url> ! gstrtpjitterbuffer ! rtph264depay ! h264parse access-unit=true ! omx_h264dec ! omx_scaler ! omx_ctrl ! omx_videosink -v

