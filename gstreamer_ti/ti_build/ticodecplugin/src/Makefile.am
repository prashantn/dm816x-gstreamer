# plugindir is set in configure

XDC_CONFIG_BASENAME = gstticodecplugin_$(PLATFORM)
plugin_LTLIBRARIES  = libgstticodecplugin.la

# sources used to compile this plug-in
libgstticodecplugin_la_SOURCES = gstticodecplugin.c gsttiauddec.c gsttiauddec1.c gsttividdec.c gsttividdec2.c gsttiimgenc1.c gsttiimgdec1.c gsttidmaibuffertransport.c gstticircbuffer.c gsttidmaivideosink.c gstticodecs.c gstticodecs_platform.c  gsttiquicktime_aac.c gsttiquicktime_h264.c gsttividenc.c gsttividenc1.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstticodecplugin_la_CFLAGS  = $(GST_CFLAGS) $(shell cat $(XDC_CONFIG_BASENAME)/compiler.opt)
libgstticodecplugin_la_LIBADD  = $(GST_LIBS) $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) -lgstvideo-0.10 -lm
libgstticodecplugin_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -Wl,$(XDC_CONFIG_BASENAME)/linker.cmd

# headers we need but don't want installed
noinst_HEADERS = gsttiauddec.h gsttiauddec1.h gsttividdec.h gsttividdec2.h gsttiimgenc1.h gsttiimgdec1.h gsttidmaibuffertransport.h gstticircbuffer.h gsttidmaivideosink.h gsttithreadprops.h gstticodecs.h gsttiquicktime_aac.h gsttiquicktime_h264.h gsttividenc.h gsttividenc1.h

# XDC Configuration
CONFIGURO     = $(XDC_INSTALL_DIR)/xs xdc.tools.configuro
BUILT_SOURCES = $(XDC_CONFIG_BASENAME)/compiler.opt $(XDC_CONFIG_BASENAME)/linker.cmd
XDC_PATH      = .;$(XDC_INSTALL_DIR)/packages;$(LINK_INSTALL_DIR)/packages;$(FC_INSTALL_DIR)/packages;$(CE_INSTALL_DIR)/packages;$(XDAIS_INSTALL_DIR)/packages;$(CODEC_INSTALL_DIR)/packages;$(CMEM_INSTALL_DIR)/packages;$(DMAI_INSTALL_DIR)/packages;$(LPM_INSTALL_DIR)/packages;$(XDC_USER_PATH);

gstticodecs_platform.c :
	ln -s $(srcdir)/gstticodecs_$(PLATFORM).c gstticodecs_platform.c

%/compiler.opt %/linker.cmd : %.cfg
	XDCPATH="$(XDC_PATH)" $(CONFIGURO) -c $(MVTOOL_DIR) -o  $(XDC_CONFIG_BASENAME) -t $(XDC_TARGET) -p $(XDC_PLATFORM) -b $(DMAI_INSTALL_DIR)/packages/config.bld $<

clean-local:
	-rm -rf $(XDC_CONFIG_BASENAME) gstticodecs_platform.c

