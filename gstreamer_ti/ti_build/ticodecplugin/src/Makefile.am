# plugindir is set in configure

XDC_CONFIG_BASENAME = gstticodecplugin_$(GST_TI_PLATFORM)
plugin_LTLIBRARIES  = libgstticodecplugin.la

# Add TIC6Accel elements
if HAVE_C6ACCEL
C6ACCEL_SRC = gsttic6xcolorspace.c
C6ACCEL_HEAD = gsttic6xcolorspace.h
C6ACCEL_LIB = $(C6ACCEL_INSTALL_DIR)/soc/c6accelw/lib/c6accelw_$(GST_TI_PLATFORM).a470MV
endif


# sources used to compile this plug-in
libgstticodecplugin_la_SOURCES = gstticodecplugin.c gsttiauddec1.c gsttividdec2.c gsttiimgenc1.c gsttiimgdec1.c gsttidmaibuffertransport.c gsttidmaibuftab.c gstticircbuffer.c gsttidmaivideosink.c gstticodecs.c gstticodecs_platform.c  gsttiquicktime_aac.c gsttiquicktime_h264.c gsttividenc1.c gsttiaudenc1.c gstticommonutils.c gsttividresize.c gsttidmaiperf.c gsttiquicktime_mpeg4.c $(C6ACCEL_SRC)

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstticodecplugin_la_CFLAGS  = $(GST_CFLAGS) $(shell cat $(XDC_CONFIG_BASENAME)/compiler.opt)
libgstticodecplugin_la_LIBADD  = $(GST_LIBS) $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) -lgstvideo-0.10 -lgstaudio-0.10 -lm
libgstticodecplugin_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -Wl,$(XDC_CONFIG_BASENAME)/linker.cmd -Wl,$(C6ACCEL_LIB)

# headers we need but don't want installed
noinst_HEADERS = gsttiauddec1.h gsttividdec2.h gsttiimgenc1.h gsttiimgdec1.h gsttidmaibuffertransport.h gsttidmaibuftab.h gstticircbuffer.h gsttidmaivideosink.h gsttithreadprops.h gstticodecs.h gsttiquicktime_aac.h gsttiquicktime_h264.h gsttividenc1.h gsttiaudenc1.h gstticommonutils.h gsttividresize.h gsttiquicktime_mpeg4.h $(C6ACCEL_HEAD)

# XDC Configuration
CONFIGURO     = $(XDC_INSTALL_DIR)/xs xdc.tools.configuro
BUILT_SOURCES = $(XDC_CONFIG_BASENAME)/compiler.opt $(XDC_CONFIG_BASENAME)/linker.cmd
XDC_PATH      = .;$(XDC_INSTALL_DIR)/packages;$(LINK_XDC_ROOT);$(FC_INSTALL_DIR)/packages;$(CE_INSTALL_DIR)/packages;$(XDAIS_INSTALL_DIR)/packages;$(CODEC_INSTALL_DIR)/packages;$(CMEM_INSTALL_DIR)/packages;$(DMAI_INSTALL_DIR)/packages;$(LPM_INSTALL_DIR)/packages;$(XDC_USER_PATH);$(EDMA3_LLD_INSTALL_DIR)/packages;$(C6ACCEL_INSTALL_DIR)/soc/c6accelw;$(C6ACCEL_INSTALL_DIR)/soc/packages

gstticodecs_platform.c :
	ln -s $(srcdir)/gstticodecs_$(GST_TI_PLATFORM).c gstticodecs_platform.c

%/compiler.opt %/linker.cmd : %.cfg
	XDCPATH="$(XDC_PATH)" $(CONFIGURO) -c $(MVTOOL_DIR) -o  $(XDC_CONFIG_BASENAME) -t $(XDC_TARGET) -p $(XDC_PLATFORM) -b $(DMAI_INSTALL_DIR)/packages/config.bld $<

clean-local:
	-rm -rf $(XDC_CONFIG_BASENAME) gstticodecs_platform.c

