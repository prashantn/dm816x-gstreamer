#!/bin/sh
#
# target_env.sh
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
# Configure the target filesystem and environment
#------------------------------------------------------------------------------
GSTREAMER_INSTALL_DIR=/opt/gstreamer
GSTINSPECT=gst-inspect
GSTLAUNCH=gst-launch

export LD_LIBRARY_PATH=${GSTREAMER_INSTALL_DIR}/lib
export GST_PLUGIN_PATH=${GSTREAMER_INSTALL_DIR}/lib/gstreamer-0.10
export PATH=${GSTREAMER_INSTALL_DIR}/bin:$PATH

