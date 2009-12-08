# Packages.make
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
# Packages processed by this build script.
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# Package glib
#------------------------------------------------------------------------------
PACKAGE_glib_BUILD_TARGET       = glib
PACKAGE_glib_ARCHIVE_BASENAME   = glib-2.22.2
PACKAGE_glib_PRECONFIG_PATCHES  =
PACKAGE_glib_CONFIGURE_OPTS     = glib_cv_stack_grows=no glib_cv_uscore=no ac_cv_func_posix_getpwuid_r=yes ac_cv_func_posix_getgrgid_r=yes
PACKAGE_glib_POSTCONFIG_PATCHES = glib1_2_22_2
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
PACKAGE_gstreamer_ARCHIVE_BASENAME   = gstreamer-0.10.25
PACKAGE_gstreamer_PRECONFIG_PATCHES  = 
PACKAGE_gstreamer_CONFIGURE_OPTS     = --disable-loadsave --disable-tests --disable-examples
PACKAGE_gstreamer_POSTCONFIG_PATCHES =
PACKAGE_gstreamer_BUILD_DIRS         =
PACKAGE_gstreamer_DESCRIPTION        = GStreamer library
BASE_PACKAGES += $(PACKAGE_gstreamer_BUILD_TARGET)

#------------------------------------------------------------------------------
# Package liboil
#------------------------------------------------------------------------------
PACKAGE_liboil_BUILD_TARGET       = liboil
PACKAGE_liboil_ARCHIVE_BASENAME   = liboil-0.3.16
PACKAGE_liboil_PRECONFIG_PATCHES  =
PACKAGE_liboil_CONFIGURE_OPTS     =
PACKAGE_liboil_POSTCONFIG_PATCHES =
PACKAGE_liboil_BUILD_DIRS         =
PACKAGE_liboil_DESCRIPTION        = Liboil library
BASE_PACKAGES += $(PACKAGE_liboil_BUILD_TARGET)

#------------------------------------------------------------------------------
# Package libid3tag
#------------------------------------------------------------------------------
PACKAGE_id3tag_BUILD_TARGET        = id3tag
PACKAGE_id3tag_ARCHIVE_BASENAME    = libid3tag-0.15.1b
PACKAGE_id3tag_PRECONFIG_PATCHES   = libid3tag1_0_15_1b
ifeq ($(PLATFORM), omap3530)
PACKAGE_id3tag_CONFIGURE_OPTS      = CPPFLAGS="-I$(LINUXLIBS_INSTALL_DIR)/include" LDFLAGS="-L$(LINUXLIBS_INSTALL_DIR)/lib"
endif
PACKAGE_id3tag_POSTCONFIG_PATCHES  =
PACKAGE_id3tag_DESCRIPTION         = id3 tag library
BASE_PACKAGES += $(PACKAGE_id3tag_BUILD_TARGET)

#------------------------------------------------------------------------------
# Package libmad
#------------------------------------------------------------------------------
PACKAGE_mad_BUILD_TARGET           = mad
PACKAGE_mad_ARCHIVE_BASENAME       = libmad-0.15.1b
PACKAGE_mad_PRECONFIG_PATCHES      = libmad1_0_15_1b
PACKAGE_mad_POSTCONFIG_PATCHES     =
PACKAGE_mad_DESCRIPTION            = mpeg audio decoder library
BASE_PACKAGES += $(PACKAGE_mad_BUILD_TARGET)

#------------------------------------------------------------------------------
# Package plugins_base
#------------------------------------------------------------------------------
PACKAGE_plugins_base_BUILD_TARGET       = plugins_base
PACKAGE_plugins_base_ARCHIVE_BASENAME   = gst-plugins-base-0.10.25
PACKAGE_plugins_base_PRECONFIG_PATCHES  = 
PACKAGE_plugins_base_CONFIGURE_OPTS     = --disable-tests --disable-examples --disable-x --disable-ogg --disable-vorbis --disable-pango $(ALSA_SUPPORT)
PACKAGE_plugins_base_POSTCONFIG_PATCHES =
PACKAGE_plugins_base_BUILD_DIRS         =
PACKAGE_plugins_base_DESCRIPTION        = GStreamer plugins base library
BASE_PACKAGES += $(PACKAGE_plugins_base_BUILD_TARGET)

#------------------------------------------------------------------------------
# Package plugins_good
#------------------------------------------------------------------------------
PACKAGE_plugins_good_BUILD_TARGET        = plugins_good
PACKAGE_plugins_good_ARCHIVE_BASENAME    = gst-plugins-good-0.10.15
PACKAGE_plugins_good_PRECONFIG_PATCHES   = plugins_good1_0_10_15 plugins_good2_0_10_15 plugins_good3_0_10_15 plugins_good4_0_10_15
PACKAGE_plugins_good_CONFIGURE_OPTS      = $(USE_V4L2SRC_WORKAROUND)
PACKAGE_plugins_good_POSTCONFIG_PATCHES  =
PACKAGE_plugins_good_BUILD_DIRS          = gst/avi
PACKAGE_plugins_good_BUILD_DIRS         += gst/qtdemux
PACKAGE_plugins_good_BUILD_DIRS         += sys/oss
PACKAGE_plugins_good_BUILD_DIRS         += sys/v4l2
PACKAGE_plugins_good_BUILD_DIRS         += gst/autodetect
PACKAGE_plugins_good_BUILD_DIRS         += gst/rtp
PACKAGE_plugins_good_BUILD_DIRS         += gst/rtsp
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
    PACKAGE_plugins_ugly_PRECONFIG_PATCHES += plugins_ugly1_0_10_13
endif
PACKAGE_plugins_ugly_CONFIGURE_OPTS     =    \
    --enable-lame                            \
    CFLAGS=-I${TARGET_GSTREAMER_DIR}/include \
    LDFLAGS=-L$(TARGET_GSTREAMER_DIR)/lib
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
PACKAGE_lame_CONFIGURE_OPTS     = LDFLAGS=-L$(TARGET_ROOT_DIR)/lib
PACKAGE_lame_DESCRIPTION        = \
        lame lib for encoding mp3
PLUGIN_PACKAGES += $(PACKAGE_lame_BUILD_TARGET)
