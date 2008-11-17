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

gstreamer-0.10.20
    Description:   GStreamer library
    Project Page:  http://gstreamer.freedesktop.org/
    Download URL:  http://gstreamer.freedesktop.org/src/gstreamer/

gst-plugins-base-0.10.20
    Description:   GStreamer plugins base library
    Project Page:  http://gstreamer.freedesktop.org/
    Download URL:  http://gstreamer.freedesktop.org/src/gst-plugins-base/

gst-plugins-good-0.10.10
    Description:   Select plugins from gStreamer good-plugins (avi, oss)
    Project Page:  http://gstreamer.freedesktop.org/
    Download URL:  http://gstreamer.freedesktop.org/src/gst-plugins-good/

gst-fluendo-mpegdemux-trunk.1483
    Description:   MPEG demuxers plugin
    Company Page:  http://www.fluendo.com/
    Download:      svn export -r1483 https://core.fluendo.com/gstreamer/svn/trunk/gst-fluendo-mpegdemux

glib-2.18.1
    Description:   GLib library
    Project Page:  http://www.gtk.org/
    Download URL:  http://ftp.gnome.org/pub/gnome/sources/glib/

liboil-0.3.15
    Description:   Liboil library
    Project Page:  http://liboil.freedesktop.org/wiki/
    Download URL:  http://liboil.freedesktop.org/download/

check-0.9.5
    Description:   Check: a unit test framework for C
    Project Page:  http://sourceforge.net/projects/check/

gst-plugins-ugly-0.10.10
    Description:   Select plugins from gStreamer ugly-plugins (mad)
    Project Page:  http://gstreamer.freedesktop.org
    Download URL:  http://gstreamer.freedesktop.org/src/gst-plugins-ugly

libid3tag-0.15.1b
    Description:   ID3 tag manipulation library
    Project Page:  http://www.underbit.com/products/mad
    Download URL:  ftp://ftp.mars.org/pub/mpeg

libmad-0.15.1b
    Description:   MPEG audio decoder library
    Project Page:  http://www.underbit.com/products/mad
    Download URL:  ftp://ftp.mars.org/pub/mpeg
    
TI Plugin Elements
------------------
TIViddec
    xDM 0.9 Video Decoder

TIViddec2
    xDM 1.2 Video Decoder

TIImgenc1
    xDM 1.0 Image Encoder (BETA!!!!)

TIAuddec
    xDM 0.9 Audio Decoder

TIDmaiVideoSink
    Video sink supporting both framebuffer and V4L2 displays


Supported Platforms
===================
This GStreamer plug-in has been built and tested on the following platforms:

- DM6446
- DM355
- DM6467
- OMAP35xx

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
  earlier than 1.30 are not supported.
- The DVSDK is configured to use the Linux kernel provided by the LSP package
  associated with the DVSDK being used.
- DMAI 1.16 or later is installed on the Linux host for DaVinci targets.
  DMAI 1.20 or later is required for OMAP35xx targets.
- For OMAP35xx platforms, the "zlib" library needs to be installed on the
  target filesystem.  To instal it, please cd to the root of the target
  filesystem and extract OMAP35x_SDK/bin/zlib-1.2.3.tar.gz.


Build Instructions
==================
Make sure the file named "Rules.make" in your top level DVSDK directory is
configured to rebuild the DVSDK components, and the PLATFORM variable is set
correctly.  The settings in Rules.make will also be used to build the GStreamer
components.

  * Special note for DVSDK 1.30 users:  If you are using DVSDK 1.30, you need
    to add a variable named "DMAI_INSTALL_DIR" to Rules.make that points to
    your DMAI installation, and you must upgrade the XDC tools to version
    3.00.04 if your platform is DM355.

Next, change to the top level "gstreamer_ti" directory, and open the
file named "Makefile.common" in your favorite editor (vi, emacs, etc.).
Set the following variables at the top of the file:

    DVSDK_INSTALL_DIR:  Location of your DVSDK installation directory
    TARGET_ROOT_DIR:    Location of the target filesystem's root directory
                        on the Linux host machine.
    MVL_VERSION:        If MontaVista tool is used for cross compiling, then 
                        this variable specifies the version of the MontaVista
                        toolchain used.  Currently versions 4.0.1 and 5.0
                        are supported.

Execute this command from the top-level "gstreamer_ti" directory:
% make all install VERBOSE=true

If you want less output during the build, you can omit the "VERBOSE=true"
above.

By default, the GStreamer binaries will be installed to "/opt/gstreamer" on
the target, and some demonstration scripts will be installed to
"/opt/gstreamer_demo".

Board Set-Up
============
In the "/opt/gstreamer_demo" target directory there is a shell script for board
set-up named "loadmodules_<platform>.sh", and others that demonstrate how you
can use the TI plugin via the gst-launch command.  Copy this directory to the
target filesystem so you can access them on the target.

Once the board is running, run the "loadmodules_<platform>.sh" script to
initialize the relevant kernel modules for your platform.  Substitute
<platform> with the name of your target (dm6446, dm355, etc.).

Playing Clips
=============
Once the board is set up, you can use the provided example scripts to play
multimedia files:

./play_AVI.sh:    Play an AVI file
./play_TS.sh:     Play a trasport stream
./test_VIDDEC.sh: Play a video file
./test_AUDDEC.sh: Play an audio file
./test_AV.sh:     Play video and audio files at the same time
./demux_AVI.sh:   Demux an AVI file into audio and video files.
./demux_TS.sh:    Demux a transport stream into audio and video files.

Note that these scripts may need minor adjustments depending on the streams
in your clip and the type of file.  For example, play_AVI may be able to auto-
detect the streams in the file, but you may need to modify test_VIDDEC to tell
it what video codec to use.

Known Issues
============
- If you don't have "gperf" installed on your system, it may manifest as a
  mysterious run issue with libid3tag involving unresolved symbols.  The gperf
  package that comes with RHEL 4.6 is sufficient, but you need to make sure
  it is installed.
- The TIImgenc1 plugin is still in a beta phase and is not fully operational.
  As of now it requires that all options be set on the gst-launch command line.
  This will be cleaned-up.  There is a list of things TODO in the Notes
  section of the gsttiimgenc1.c header.  This plugin was successfully tested
  using the frida_720x480.yuv file from the examples diretory of the DM355
  DVSDK.  This plugin has only been tested on the DM355.
