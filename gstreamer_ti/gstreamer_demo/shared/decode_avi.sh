#!/bin/sh
#
# decode_avi.sh
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

#------------------------------------------------------------------------------
# Execute common code
#------------------------------------------------------------------------------
cat /dev/zero > /dev/fb/2 2>/dev/null
. ../shared/process_avopts.sh
. ../shared/run_pipe.sh

#------------------------------------------------------------------------------
# Enable debugging flags (optional)
#------------------------------------------------------------------------------
#DEBUG="--gst-debug-no-color --gst-debug=${audio_plugin}:5"
#DEBUG="--gst-debug-no-color --gst-debug=${video_plugin}:5"
#DEBUG="--gst-debug-no-color --gst-debug=TIDmaiVideoSink:5"
#DEBUG="--gst-debug-no-color --gst-debug=TIDmaiVideoSink:*"
#DEBUG="--gst-debug-no-color --gst-debug=GST_PLUGIN_LOADING:*"

#------------------------------------------------------------------------------
# Execute the pipeline
#------------------------------------------------------------------------------
run_pipe "${GSTLAUNCH} ${DEBUG} filesrc location=$fileName ! typefind !  avidemux name=demux demux.audio_00 ! queue max-size-buffers=1200 max-size-time=0 max-size-bytes=0 ! typefind ! ${audio_plugin} ${audio_plugin_args} ! ${audio_sink} demux.video_00 ! typefind ! ${video_plugin} ${video_plugin_args} ! ${video_sink} ${rotation_opts}"
exit $?

