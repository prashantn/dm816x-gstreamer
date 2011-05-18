#!/bin/sh

# pipeline decode elemenatry H.264 stream using OMX elements
gst-launch filesrc location=sample.264 ! typefind ! h264parse access-unit=true ! omx_h264dec ! video/x-raw-yuv-strided  ! fakesink -v

# pipeline to decode video from MP4 container
gst-launch filesrc location=sample.mp4 ! typefind ! qtdemux name=demux demux.video_00 ! nal2bytestream_h264 ! omx_h264dec ! video/x-raw-yuv-strided ! fakesink -v


