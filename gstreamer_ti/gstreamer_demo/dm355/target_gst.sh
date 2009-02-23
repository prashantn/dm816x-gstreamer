#!/bin/sh
#
# target_gst.sh
#
# Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation version 2.1 of the License.
#
# This program is distributed #as is# WITHOUT ANY WARRANTY of any kind,
# whether express or implied; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

#-----------------------------------------------------------------------------
# Set GStreamer attributes for this platform
#-----------------------------------------------------------------------------
PLATFORM=dm355
. ../shared/target_env.sh

# Audio / Video Decode defaults
audio_plugin="mad"
audiocodecName=""
video_plugin="TIViddec2"
videocodeName="mpeg4dec"
soundStd="oss"
dispStd="fbdev"
dispDevice=""
videoStd="D1_NTSC"
videoOut="COMPOSITE"
resizer="FALSE";
accelFrameCopy="TRUE"

# Imaging defaults
encode_image_plugin="TIImgenc1"
decode_image_plugin="TIImgdec1"
resolution="720x480"
inputcolorSpace="UYVY"
outputcolorSpace="UYVY"
qValue="75"

# audio encoder defaults
audio_live_source="audiotestsrc"
audio_encoder="lame"
audio_src_args="num-buffers=100"

# video encoder defauls
video_encoder="TIVidenc1"
video_encoder_codec_name="mpeg4enc"
video_live_source="videotestsrc"
video_encoder_color_space="UYVY"
video_src_args="num-buffers=1000"
video_encoder_resolution="720x480"
video_encoder_args="contiguousInputFrame=FALSE"
