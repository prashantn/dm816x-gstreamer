# Packages.make
#
# Copyright (C) 2008-2010 Texas Instruments Incorporated - http://www.ti.com/
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
# Packages processed by this build script.
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# Package glib
#------------------------------------------------------------------------------
PACKAGE_glib_BUILD_TARGET       = glib
PACKAGE_glib_ARCHIVE_BASENAME   = glib-2.24.2
PACKAGE_glib_PRECONFIG_PATCHES  =
PACKAGE_glib_CONFIGURE_OPTS     = glib_cv_stack_grows=no glib_cv_uscore=no ac_cv_func_posix_getpwuid_r=yes ac_cv_func_posix_getgrgid_r=yes
PACKAGE_glib_POSTCONFIG_PATCHES =
PACKAGE_glib_BUILD_DIRS         =
PACKAGE_glib_DESCRIPTION        = GLib library
BASE_PACKAGES += $(PACKAGE_glib_BUILD_TARGET)

#------------------------------------------------------------------------------
# Package check
#------------------------------------------------------------------------------
PACKAGE_check_BUILD_TARGET       = check
PACKAGE_check_ARCHIVE_BASENAME   = check-0.9.8
PACKAGE_check_PRECONFIG_PATCHES  =
PACKAGE_check_CONFIGURE_OPTS     = 
PACKAGE_check_POSTCONFIG_PATCHES =
PACKAGE_check_BUILD_DIRS         =
PACKAGE_check_DESCRIPTION        = Check: a unit test framework for C
BASE_PACKAGES += $(PACKAGE_check_BUILD_TARGET)

#------------------------------------------------------------------------------
# Package gstreamer
#------------------------------------------------------------------------------
PACKAGE_gstreamer_BUILD_TARGET       = gstreamer
PACKAGE_gstreamer_ARCHIVE_BASENAME   = gstreamer-0.10.30
PACKAGE_gstreamer_PRECONFIG_PATCHES  = \
    0001-Disable-last-buffer-feature-by-default.patch
PACKAGE_gstreamer_CONFIGURE_OPTS     = --disable-loadsave --disable-tests --disable-examples
PACKAGE_gstreamer_POSTCONFIG_PATCHES =
PACKAGE_gstreamer_BUILD_DIRS         =
PACKAGE_gstreamer_DESCRIPTION        = GStreamer library
BASE_PACKAGES += $(PACKAGE_gstreamer_BUILD_TARGET)

#------------------------------------------------------------------------------
# Package libid3tag
#------------------------------------------------------------------------------
PACKAGE_id3tag_BUILD_TARGET        = id3tag
PACKAGE_id3tag_ARCHIVE_BASENAME    = libid3tag-0.15.1b
PACKAGE_id3tag_PRECONFIG_PATCHES   = \
    0001-Converted-from-libid3tag1_0_15_1b.patch.patch
PACKAGE_id3tag_CONFIGURE_OPTS      = 
PACKAGE_id3tag_POSTCONFIG_PATCHES  =
PACKAGE_id3tag_DESCRIPTION         = id3 tag library
BASE_PACKAGES += $(PACKAGE_id3tag_BUILD_TARGET)

#------------------------------------------------------------------------------
# Package libmad
#------------------------------------------------------------------------------
PACKAGE_mad_BUILD_TARGET           = mad
PACKAGE_mad_ARCHIVE_BASENAME       = libmad-0.15.1b
PACKAGE_mad_PRECONFIG_PATCHES      = \
    0001-Converted-from-libmad1_0_15_1b.patch \
    0002-Remove-fforce-mem-option-when-building-with-O2.patch
PACKAGE_mad_POSTCONFIG_PATCHES     =
PACKAGE_mad_DESCRIPTION            = mpeg audio decoder library
BASE_PACKAGES += $(PACKAGE_mad_BUILD_TARGET)

#------------------------------------------------------------------------------
# Package plugins_base
#------------------------------------------------------------------------------
PACKAGE_plugins_base_BUILD_TARGET       = plugins_base
PACKAGE_plugins_base_ARCHIVE_BASENAME   = gst-plugins-base-0.10.30
PACKAGE_plugins_base_PRECONFIG_PATCHES  =
PACKAGE_plugins_base_CONFIGURE_OPTS     = --disable-examples --disable-x --disable-ogg --disable-vorbis --disable-pango $(ALSA_SUPPORT)
PACKAGE_plugins_base_POSTCONFIG_PATCHES =
PACKAGE_plugins_base_BUILD_DIRS         =
PACKAGE_plugins_base_DESCRIPTION        = GStreamer plugins base library
BASE_PACKAGES += $(PACKAGE_plugins_base_BUILD_TARGET)

#------------------------------------------------------------------------------
# Package plugins_good
#------------------------------------------------------------------------------
PACKAGE_plugins_good_BUILD_TARGET        = plugins_good
PACKAGE_plugins_good_ARCHIVE_BASENAME    = gst-plugins-good-0.10.16
PACKAGE_plugins_good_PRECONFIG_PATCHES   = \
    0001-rtph264pay-fix-for-streaming-encode.patch \
    0002-v4l2src-add-input-src-property-to-specify-capture-in.patch \
    0003-v4l2src-keep-track-of-the-input-ID-that-will-be-used.patch \
    0004-v4l2src-make-sure-capture-buffer-size-is-aligned-on-.patch \
    0005-osssink-handle-all-supported-sample-rates.patch \
    0006-v4l2src-add-support-for-DaVinci-platforms-using-MVL-.patch \
    0007-v4l2src-support-NV12-capture-on-DM365-using-the-IPIP.patch \
    0008-v4l2src-accept-EPERM-as-a-non-fatal-error-for-VIDIOC.patch \
    0009-v4l2src-try-progressive-mode-first-for-component-inp.patch \
    0010-v4l2src-add-support-for-NV16-colorspace.patch \
    0011-v4l2src-set-bytesperline-and-sizeimage-before-callin.patch \
    0012-v4l2src-update-gst_v4l2_get_norm-to-handle-DM6467T-a.patch \
    0013-v4l2src-add-V4L2-ioctl-calls-to-initialize-capture-d.patch \
    0014-v4l2src-disable-video-device-polling-by-default-on-D.patch
PACKAGE_plugins_good_CONFIGURE_OPTS      = 
PACKAGE_plugins_good_POSTCONFIG_PATCHES  =
PACKAGE_plugins_good_BUILD_DIRS          = gst/avi
PACKAGE_plugins_good_BUILD_DIRS         += gst/qtdemux
PACKAGE_plugins_good_BUILD_DIRS         += sys/oss
PACKAGE_plugins_good_BUILD_DIRS         += sys/v4l2
PACKAGE_plugins_good_BUILD_DIRS         += gst/autodetect
PACKAGE_plugins_good_BUILD_DIRS         += gst/rtp
PACKAGE_plugins_good_BUILD_DIRS         += gst/rtsp
PACKAGE_plugins_good_BUILD_DIRS         += gst/rtpmanager
PACKAGE_plugins_good_BUILD_DIRS         += gst/udp
PACKAGE_plugins_good_DESCRIPTION         = \
    Select plugins from GStreamer good-plugins (avi, qtdemux, oss, v4l2)
PLUGIN_PACKAGES += $(PACKAGE_plugins_good_BUILD_TARGET)

#------------------------------------------------------------------------------
# Package plugins_bad
#------------------------------------------------------------------------------
PACKAGE_plugins_bad_BUILD_TARGET        = plugins_bad
PACKAGE_plugins_bad_ARCHIVE_BASENAME    = gst-plugins-bad-0.10.16
PACKAGE_plugins_bad_PRECONFIG_PATCHES   = 
PACKAGE_plugins_bad_POSTCONFIG_PATCHES  =
PACKAGE_plugins_bad_BUILD_DIRS          = gst/mpegdemux
PACKAGE_plugins_bad_BUILD_DIRS          += gst/rtpmux
PACKAGE_plugins_bad_BUILD_DIRS          += gst/qtmux
PACKAGE_plugins_bad_DESCRIPTION         = \
    Select plugins from GStreamer bad-plugins
PLUGIN_PACKAGES += $(PACKAGE_plugins_bad_BUILD_TARGET)

#-------------------------------------------------------------------------------
# Package plugins_ugly
#-------------------------------------------------------------------------------
PACKAGE_plugins_ugly_BUILD_TARGET       = plugins_ugly
PACKAGE_plugins_ugly_ARCHIVE_BASENAME   = gst-plugins-ugly-0.10.13
PACKAGE_plugins_ugly_PRECONFIG_PATCHES  = 
ifeq ($(ALSA_SUPPORT), --disable-alsa)
    PACKAGE_plugins_ugly_PRECONFIG_PATCHES += \
        0001-Converted-from-plugins_ugly1_0_10_13.patch.patch
endif
PACKAGE_plugins_ugly_CONFIGURE_OPTS     = --enable-lame
PACKAGE_plugins_ugly_POSTCONFIG_PATCHES =
PACKAGE_plugins_ugly_BUILD_DIRS         = ext/mad
PACKAGE_plugins_ugly_BUILD_DIRS        += ext/lame
PACKAGE_plugins_ugly_DESCRIPTION        = \
        Selected plugins from Gstreamer ugly-plugins (id3tag, mad, lame)
PLUGIN_PACKAGES += $(PACKAGE_plugins_ugly_BUILD_TARGET)

#-------------------------------------------------------------------------------
# Package lame
#-------------------------------------------------------------------------------
PACKAGE_lame_BUILD_TARGET       = lame
PACKAGE_lame_ARCHIVE_BASENAME   = lame-398-2
PACKAGE_lame_PRECONFIG_PATCHES  =
PACKAGE_lame_CONFIGURE_OPTS     = 
PACKAGE_lame_DESCRIPTION        = \
        lame lib for encoding mp3
PLUGIN_PACKAGES += $(PACKAGE_lame_BUILD_TARGET)
