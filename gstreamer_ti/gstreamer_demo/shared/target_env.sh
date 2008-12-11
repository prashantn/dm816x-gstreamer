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

# Make sure we store the registry cache in /tmp so we don't fail when running
# from a read-only file system.  If we don't do this, GStreamer tries to
# create the cache in the current directory.
export GST_REGISTRY=/tmp/gst_registry.bin

# Configure our LD_LIBRARY_PATH so GStreamer applications can find shared
# object libraries while loading.
export LD_LIBRARY_PATH=${GSTREAMER_INSTALL_DIR}/lib

# Set the GST_PLUGIN_PATH to the directory where GStreamer should look for
# its plugins
export GST_PLUGIN_PATH=${GSTREAMER_INSTALL_DIR}/lib/gstreamer-0.10

# Put the GStreamer binaries in our path
export PATH=${GSTREAMER_INSTALL_DIR}/bin:$PATH

