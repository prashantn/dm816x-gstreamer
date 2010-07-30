#
# README.TXT
#
# This file describes the build and run process for GStreamer on TI
# DaVinci and OMAP platforms.
#
# Copyright (C) 2008-2010 Texas Instruments Incorporated - http://www.ti.com/
#

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

gstreamer-0.10.25
    Description:   GStreamer library
    Project Page:  http://gstreamer.freedesktop.org/
    Download URL:  http://gstreamer.freedesktop.org/src/gstreamer/

gst-plugins-base-0.10.25
    Description:   GStreamer plugins base library
    Project Page:  http://gstreamer.freedesktop.org/
    Download URL:  http://gstreamer.freedesktop.org/src/gst-plugins-base/

gst-plugins-good-0.10.16
    Description:   Select plugins from gStreamer good-plugins (avi, oss)
    Project Page:  http://gstreamer.freedesktop.org/
    Download URL:  http://gstreamer.freedesktop.org/src/gst-plugins-good/

gst-plugins-bad-0.10.16
    Description:   Select plugins from gStreamer good-plugins (mpegdemux)
    Project Page:  http://gstreamer.freedesktop.org/
    Download URL:  http://gstreamer.freedesktop.org/src/gst-plugins-bad/

gst-plugins-ugly-0.10.13
    Description:   Select plugins from gStreamer ugly-plugins (mad)
    Project Page:  http://gstreamer.freedesktop.org
    Download URL:  http://gstreamer.freedesktop.org/src/gst-plugins-ugly

glib-2.22.2
    Description:   GLib library
    Project Page:  http://www.gtk.org/
    Download URL:  http://ftp.gnome.org/pub/gnome/sources/glib/

check-0.9.8
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
Video Elements:
* TIViddec2:  xDM 1.2 Video Decoder
* TIVidenc1:  xDM 1.x Video Encoder

Audio Elements:
* TIAuddec1:  xDM 1.0 Audio Decoder
* TIAudenc1:  xDM 1.x Audio Encoder

Imaging Elements:
* TIImgenc1:  xDM 1.0 Image Encoder
* TIImgdec1:  xDM 1.0 Image Decoder

Other Elements:
* TIDmaiVideoSink:  Video sink supporting both framebuffer and V4L2 displays
* TIVidResize:      This element resizes video frames.

Supported Platforms
===================
This GStreamer plug-in has been built and tested on the following platforms:

Platform    DVSDK Version
--------    -------------
DM6446      2.00.00.22
DM355       2.00.00.22
DM6467      2.00.00.22
DM6467T     3.10.00.19
OMAP35xx    3.00.02.44
DM365       3.10.00.19

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
                            CS_2008Q1:  CodeSourcery v2008q1
                            CS_2009Q1:  CodeSourcery v2009q1

Execute this command from the top-level "gstreamer_ti" directory:
% make all install VERBOSE=true

If you want less output during the build, you can omit the "VERBOSE=true"
above.

By default, the GStreamer binaries will be installed to "/opt/gstreamer" on
the target, and some demonstration scripts will be installed to
"/opt/gstreamer_demo".

Special Notes on DM6467 and DM6467T Build
-----------------------------------------
When building for DM6467 or DM6467T you must edit the Makefile.common file and
set GST_TI_PLATFORM to the EVM you are using.

DM6467 example:
    GST_TI_PLATFORM = dm6467

DM6467T example:
    GST_TI_PLATFORM = dm6467t

If you fail to set this variable, you will get the following error when you
build:
    
    Makefile.common:72: *** when building for dm6467 or dm6467t, the platform
    type must be specified explicity at the top of Makefile.commom with
    GST_TI_PLATFORM=dm6467 or GST_TI_PLATFORM=dm6467t.  Stop.

Board Set-Up
============
After installation, there will be a target directory named
"/opt/gstreamer_demo/<platform>", where <platform> is the name of your target
(dm6446, dm355, etc.).  In this directory there is a script for board set-up
named "loadmodules.sh".  Run this script once the board is running to
initialize the relevant kernel modules for your platform.

Other scripts are also available in this directory that demonstrate how you can
use the TI plugin via the gst-launch command, and are explained below.

Special Notes:  DVSDK 2.00
--------------------------
DVSDK 2.00 comes with linuxutils version 2.23.01.  This version of linuxutils
has a bug in the CMEM module that will cause CMEM to incorrectly throw an error
at the end of playback that looks like this:
    
    CMEMK Error: FREE: Not a registered user of physical buffer 0x87968000
    CMEM Error: free: failed to free 0x43316000
    
To resolve this issue, you must update your linuxutils component to version
2.24.04.05.  Linuxutils 2.24.04.05 can be downloaded from this page:
    
http://software-dl.ti.com/dsps/dsps_public_sw/sdo_sb/targetcontent/linuxutils/index.html
    
Un-tar the updated version of linuxutils into your DVSDK 2.00 directory, and
update the following line in your Rules.make:
    
Old setting:
CMEM_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/linuxutils_2_23_01
    
New setting:
CMEM_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/linuxutils_2_24_04_05
    
Then re-build the DVSDK components as follows:
    
1)  Change to the DVSDK directory
2)  Double-check your Rules.make settings if needed to make sure all the
    components point to valid component directories
3)  Run "make linux"
4)  Run "make clean; make"
    
Special Notes:  DVSDK 3.00
--------------------------
These additional set-up steps are required when using DVSDK 3.00 with an
OMAP35xx EVM.

* Before running the loadmodules.sh script mentioned above, you must first run 
  /opt/dvsdk/omap3530/unloadmodules.sh on the EVM to ensure the kernel
  modules are configured properly for GStreamer.
* On OMAP35xx EVMs with only 128M of memory, it may be desirable to use the
  following bootargs setting in U-Boot to reduce the amount of memory allocated
  by the video driver and make it available to the system:

  OMAP3_EVM # setenv nfshost <ip address of nfs host> 
  OMAP3_EVM # setenv rootpath <directory to mount> 
  OMAP3_EVM # setenv bootargs 'console=ttyS0,115200n8 noinitrd rw ip=dhcp
                  root=/dev/nfs nfsroot=$(nfshost):$(rootpath),nolock mem=99M
                  mpurate=600 omapfb.rotate=1 omapfb.rotate_type=1
                  omap_vout.vid1_static_vrfb_alloc=y
                  omap_vout.video1_numbuffers=3 omap_vout.video1_bufsize=829440
                  omap_vout.video2_numbuffers=0'

  Each "setenv" command should be entered on a single line.

  Depending on the complexity of your GStreamer pipeline and the clips you are
  playing, failure to adjust the video driver's memory allocation could result
  in OOM killers.

  Other platforms should use the bootargs settings in the DVSDK Getting
  Started Guide.

Special Notes:  DVSDK 3.10
--------------------------
By default, GStreamer uses mmap buffer drivers for the capture and display.
With the more recent kernels, there are no guarantees in the kernel that it
will be able to allocate enough space for these buffers when requested.  For
DVSDK 3.10, you need to use the additional patches available on this kernel
staging branch in order to make mmap buffer use reliable:

http://arago-project.org/git/projects/?p=linux-davinci.git;a=shortlog;h=refs/heads/video-boot-time-mem-alloc

With these kernel patches applied, you should be using the kernel boot
arguments below to make sure space is allocated for mmap driver buffers.  Note
that all of the bootargs options should be on a single line.

DM6467T:

bootargs=console=ttyS0,115200n8 noinitrd rw ip=dhcp root=/dev/nfs
  nfsroot=<nfsserver_ip>:<path to root filesystem>,nolock
  vpif_display.ch2_numbuffers=0 vpif_display.ch3_numbuffers=0 mem=104M
  vpif_display.cont_bufsize=12582912 vpif_capture.cont_bufoffset=12582912
  vpif_capture.cont_bufsize=12582912 

DM365:

bootargs=mem=65M console=ttyS0,115200n8 root=/dev/nfs
  nfsroot=<nfsserver_ip>:<path to root filesystem>,nolock ip=dhcp
  video=davincifb:vid0=OFF:vid1=OFF:osd0=720x576x16,4050K
  dm365_imp.oper_mode=0 davinci_capture.device_type=4
  davinci_enc_mngr.ch0_mode=ntsc davinci_display.cont2_bufsize=6291456
  vpfe_capture.cont_bufoffset=6291456 vpfe_capture.cont_bufsize=6291456 

  Note:  Changing dm365_imp.oper_mode=0 to dm365_imp.oper_mode=1 above will
  enable usage of the TIVidResize element, but only UYVY capture can be done
  in this mode.  Do not use dm365_imp.oper_mode=1 if you are encoding video
  as the codecs require NV12 capture input.

Example GStreamer Usage
=======================
Many example GStreamer pipelines that use the TI GStreamer Plugin are available
at:

    http://wiki.davincidsp.com/index.php/Example_GStreamer_Pipelines

These examples cover how to play clips of various types, perform capture and
encode, as well as network streaming.  Additional example contributions are
welcome!

Known Issues
============
Known issues are listed in the RELEASE_NOTES.TXT file.  For a list of all 
issues please the the tracker items on the GStreamer project site at:
https://gstreamer.ti.com/gf/project/gstreamer_ti/tracker/

