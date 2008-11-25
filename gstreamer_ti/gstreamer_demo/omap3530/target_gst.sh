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
PLATFORM=omap3530
. ../shared/target_env.sh

audio_plugin="TIAuddec1"
audiocodecName=""
video_plugin="TIViddec2"
videocodeName=""
soundStd="alsa"
dispStd="v4l2"
dispDevice="/dev/video1"
videoStd="VGA"
videoOut="LCD"
resizer="FALSE";
rotation="90";
