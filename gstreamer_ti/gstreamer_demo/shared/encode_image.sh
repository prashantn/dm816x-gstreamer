#!/bin/sh
#
# encode_image.sh
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
# Enable debugging flags
#------------------------------------------------------------------------------
#DEBUG="--gst-debug-no-color --gst-debug=TIImgenc1:5"
#DEBUG="--gst-debug-no-color --gst-debug=TIDmaiVideoSink:*"
#DEBUG="--gst-debug-no-color --gst-debug=GST_PLUGIN_LOADING:*"
DEBUG="--gst-debug-no-color --gst-debug=TI*:2"

. ../shared/process_imgopts.sh
. ../shared/run_pipe.sh

run_pipe "${GSTLAUNCH} ${DEBUG} filesrc location=$inputFile ! ${image_plugin} ${image_plugin_args} ! filesink location=$outputFile"
exit $?

