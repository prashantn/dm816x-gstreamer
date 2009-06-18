#
# README.TXT
#
# This file describes the build and run process for GStreamer on TI
# DaVinci and OMAP platforms.
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

Contents
========
This package contains open-source base components required to build and run
GStreamer on TI DaVinci platforms, as well as a plugin that provides GStreamer
elements that interfaces with Codec Engine.

Open Source Components
----------------------
The following components are provided in the opensource_build/distfiles
directory, and are automatically built during the build process
(see the Build Instructions section below).

gstreamer-0.10.21
    Description:   GStreamer library
    Project Page:  http://gstreamer.freedesktop.org/
    Download URL:  http://gstreamer.freedesktop.org/src/gstreamer/

gst-plugins-base-0.10.21
    Description:   GStreamer plugins base library
    Project Page:  http://gstreamer.freedesktop.org/
    Download URL:  http://gstreamer.freedesktop.org/src/gst-plugins-base/

gst-plugins-good-0.10.10
    Description:   Select plugins from gStreamer good-plugins (avi, oss)
    Project Page:  http://gstreamer.freedesktop.org/
    Download URL:  http://gstreamer.freedesktop.org/src/gst-plugins-good/

gst-plugins-bad-0.10.9
    Description:   Select plugins from gStreamer good-plugins (mpegdemux)
    Project Page:  http://gstreamer.freedesktop.org/
    Download URL:  http://gstreamer.freedesktop.org/src/gst-plugins-bad/

gst-plugins-ugly-0.10.10
    Description:   Select plugins from gStreamer ugly-plugins (mad)
    Project Page:  http://gstreamer.freedesktop.org
    Download URL:  http://gstreamer.freedesktop.org/src/gst-plugins-ugly

glib-2.21.1
    Description:   GLib library
    Project Page:  http://www.gtk.org/
    Download URL:  http://ftp.gnome.org/pub/gnome/sources/glib/

check-0.9.6
    Description:   Check: a unit test framework for C
    Project Page:  http://sourceforge.net/projects/check/

liboil-0.3.16
    Description:   Liboil library
    Project Page:  http://liboil.freedesktop.org/wiki/
    Download URL:  http://liboil.freedesktop.org/download/

libid3tag-0.15.1b
    Description:   ID3 tag manipulation library
    Project Page:  http://www.underbit.com/products/mad
    Download URL:  ftp://ftp.mars.org/pub/mpeg

libmad-0.15.1b
    Description:   MPEG audio decoder library
    Project Page:  http://www.underbit.com/products/mad
    Download URL:  ftp://ftp.mars.org/pub/mpeg

lame-398-2
    Description:   MPEG Audio Layer III (MP3) encoder
    Project Page:  http://lame.sourceforge.net/
    Download URL:  http://lame.sourceforge.net/download.php
 
TI Plugin Elements
------------------
TIViddec
    xDM 0.9 Video Decoder

TIViddec2
    xDM 1.2 Video Decoder

TIImgenc1
    xDM 1.0 Image Encoder

TIImgenc
    xDM 0.9 Image Encoder

TIImgdec1
    xDM 1.0 Image Decoder

TIImgdec
    xDM 0.9 Image Decoder

TIAuddec
    xDM 0.9 Audio Decoder

TIAuddec1
    xDM 1.0 Audio Decoder

TIDmaiVideoSink
    Video sink supporting both framebuffer and V4L2 displays

TIVidenc
    xDM 0.9 Video Encoder

TIVidenc1
    xDM 1.x Video Encoder

Supported Platforms
===================
This GStreamer plug-in has been built and tested on the following platforms:

Platform    DVSDK Version
--------    -------------
DM6446      2.00.00.22
DM355       2.00.00.22
DM6467      2.00.00.22
OMAP35xx    3.00.00.29
DM357       2.05.00.14

Prerequisites
=============
- Linux host running RHEL 4.6.  Other linux distributions may work, but have
  not been tested.  Some of the tools in RHEL 4.6 are too old to build the
  GStreamer components.  The following opensource components needed to be
  installed on a RHEL 4.6 linux host in order to complete the build:
  
  pkg-config-0.18
      Description:   pkg-config helper build tool
      Project Page:  http://pkg-config.freedesktop.org/wiki/
      Download URL:  http://pkgconfig.freedesktop.org/releases/

  flex-2.5.35
      Description:   flex: The Fast Lexical Analyzer
      Project Page:  http://flex.sourceforge.net/
      Download URL:  http://sourceforge.net/projects/flex/

  The above components are provided in the opensource_build/host_tools
  directory, but you must buid and install them manually on your system.

- The DVSDK for the platform is installed on the Linux host.  DVSDK versions
  earlier than 2.00 are not actively supported.
- The DVSDK is configured to use the Linux kernel provided by the LSP package
  associated with the DVSDK being used.
- The proper version of DMAI must be installed.  Currently, all platforms
  require DMAI version 1.20.
- For OMAP35xx platforms, the "zlib" library needs to be installed on the
  target filesystem.  To instal it, please cd to the root of the target
  filesystem and extract OMAP35x_SDK/bin/zlib-1.2.3.tar.gz.

Build Instructions
==================
Make sure the file named "Rules.make" in your top level DVSDK directory is
configured to rebuild the DVSDK components, and the PLATFORM variable is set
correctly.  The settings in Rules.make will also be used to build the GStreamer
components.

Next, change to the top level "gstreamer_ti" directory, and open the
file named "Makefile.common" in your favorite editor (vi, emacs, etc.).
Set the following variables at the top of the file:

    DVSDK_INSTALL_DIR:  Location of your DVSDK installation directory
    TARGET_ROOT_DIR:    Location of the target filesystem's root directory
                        on the Linux host machine.
    CROSS_COMPILER:     Cross compiler being used.  A default is chosen based
                        on the platform you are building for, but you can set
                        it explicitly to one of these values:
                            MVL_5_0:    MontaVista   v5.0
                            CS_2007Q3:  CodeSourcery v2007q3

Execute this command from the top-level "gstreamer_ti" directory:
% make all install VERBOSE=true

If you want less output during the build, you can omit the "VERBOSE=true"
above.

By default, the GStreamer binaries will be installed to "/opt/gstreamer" on
the target, and some demonstration scripts will be installed to
"/opt/gstreamer_demo".

Board Set-Up
============
After installation, there will be a target directory named
"/opt/gstreamer_demo/<platform>", where <platform> is the name of your target
(dm6446, dm355, etc.).  In this directory there is a script for board set-up
named "loadmodules.sh".  Run this script once the board is running to
initialize the relevant kernel modules for your platform.

Other scripts are also available in this directory that demonstrate how you can
use the TI plugin via the gst-launch command, and are explained below.

Playing Clips
=============
Once the board is set up, you can use the provided example scripts to play
multimedia files:

./decode_avi.sh:         Play an AVI file
./decode_ts.sh:          Play a trasport stream
./decode_qt.sh:          Play a quicktime file (i.e *.mp4)
./decode_elementary.sh:  Play an audio or video elementary stream
./encode_image.sh:       Encode an image file
./test_sink.sh           Test audio or video sink
./encode_elementry.sh:   Encode elementry stream using live source or file

Note that you may need to specify command line arguments to some scripts for
them to work properly.  Run a script with -h to see what options are available.

Known issues are listed in the RELEASE_NOTES.TXT file.  For a list of all 
issues please the the tracker items on the GStreamer project site at:
https://gstreamer.ti.com/gf/project/gstreamer_ti/tracker/

