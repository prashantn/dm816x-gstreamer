plugin_LTLIBRARIES = libgstomx.la

libgstomx_la_SOURCES = gstomx.c gstomx.h \
		       gstomx_interface.c gstomx_interface.h \
		       gstomx_base_filter.c gstomx_base_filter.h \
		       gstomx_base_videodec.c gstomx_base_videodec.h \
		       gstomx_base_videoenc.c gstomx_base_videoenc.h \
		       gstomx_base_audiodec.c gstomx_base_audiodec.h \
		       gstomx_util.c gstomx_util.h \
		       gstomx_core.c gstomx_core.h \
		       gstomx_port.c gstomx_port.h \
		       gstomx_dummy.c gstomx_dummy.h \
		       gstomx_volume.c gstomx_volume.h \
		       gstomx_mpeg4dec.c gstomx_mpeg4dec.h \
		       gstomx_h263dec.c gstomx_h263dec.h \
		       gstomx_h264dec.c gstomx_h264dec.h \
		       gstomx_wmvdec.c gstomx_wmvdec.h \
		       gstomx_mpeg4enc.c gstomx_mpeg4enc.h \
		       gstomx_h264enc.c gstomx_h264enc.h \
		       gstomx_h263enc.c gstomx_h263enc.h \
		       gstomx_vorbisdec.c gstomx_vorbisdec.h \
		       gstomx_amrnbdec.c gstomx_amrnbdec.h \
		       gstomx_amrnbenc.c gstomx_amrnbenc.h \
		       gstomx_amrwbdec.c gstomx_amrwbdec.h \
		       gstomx_amrwbenc.c gstomx_amrwbenc.h \
		       gstomx_aacdec.c gstomx_aacdec.h \
		       gstomx_aacenc.c gstomx_aacenc.h \
		       gstomx_mp3dec.c gstomx_mp3dec.h \
		       gstomx_mp2dec.c gstomx_mp2dec.h \
		       gstomx_adpcmdec.c gstomx_adpcmdec.h \
		       gstomx_adpcmenc.c gstomx_adpcmenc.h \
		       gstomx_g711dec.c gstomx_g711dec.h \
		       gstomx_g711enc.c gstomx_g711enc.h \
		       gstomx_g729dec.c gstomx_g729dec.h \
		       gstomx_g729enc.c gstomx_g729enc.h \
		       gstomx_ilbcdec.c gstomx_ilbcdec.h \
		       gstomx_ilbcenc.c gstomx_ilbcenc.h \
		       gstomx_jpegenc.c gstomx_jpegenc.h \
		       gstomx_jpegdec.c gstomx_jpegdec.h \
		       gstomx_base_sink.c gstomx_base_sink.h \
		       gstomx_audiosink.c gstomx_audiosink.h \
		       gstomx_videosink.c gstomx_videosink.h \
		       gstomx_base_src.c gstomx_base_src.h \
		       gstomx_filereadersrc.c gstomx_filereadersrc.h \
               swcsc.c swcsc.h \
               gstperf.c gstperf.h  \
               gstomx_buffertransport.c gstomx_buffertransport.h \
               gstomx_base_vfpc.c gstomx_base_vfpc.h \
               gstomx_colorconv.c gstomx_colorconv.h  \
               gstomx_base_ctrl.c gstomx_base_ctrl.h 

libgstomx_la_LIBADD = $(OMXCORE_LIBS) $(GST_LIBS) $(GST_BASE_LIBS) -lgstvideo-0.10 $(top_builddir)/util/libutil.la
libgstomx_la_CFLAGS = $(OMXCORE_CFLAGS) $(OMXTIAUDIODEC_CFLAGS) $(USE_OMXTIAUDIODEC) $(GST_CFLAGS) $(GST_BASE_CFLAGS) -I$(top_srcdir)/util \
	-Dxdc_target_name__=GCArmv5T -Dxdc_target_types__=gnu/targets/arm/std.h -Dxdc__RESTRICT__ \
    	-I$(OMX_INSTALL_DIR)/include/openMaxv11 \
    	-I$(OMX_INSTALL_DIR)/packages \
    	-I$(OSAL_INSTALL_DIR)/sources \
    	-I$(CE_INSTALL_DIR)/packages \
    	-I$(XDAIS_INSTALL_DIR)/packages  \
    	-I$(CODEC_INSTALL_DIR)/packages  \
    	-I$(SYSBIOS_INSTALL_DIR)/packages \
    	-I$(XDC_INSTALL_DIR)/packages \
    	-I$(IPC_INSTALL_DIR)/packages \
    	-I$(SYSLINK_INSTALL_DIR) \
    	-I$(HDVPSS_INSTALL_DIR)/packages \
    	-I$(OMX_INSTALL_DIR)/include \
    	-I$(OMX_INSTALL_DIR)/packages/ti/omx/comp/ctrl
        

libgstomx_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -L../ext/lib -lOMX_Core -pthread 

#libgstomx_la_LDFLAGS +=
#	-Wl,$(OMX_INSTALL_DIR)/lib/omxcore.av5T \
#	-Wl,$(OMX_INSTALL_DIR)/lib/domx.av5T \
#	-Wl,$(OSAL_INSTALL_DIR)/packages/linuxdist/build/lib/osal.a \
#	-Wl,$(SYSLINK_INSTALL_DIR)/ti/syslink/lib/syslink.a_release \
#	-Wl,$(OMX_INSTALL_DIR)/rebuilt-binaries/ti/omx/omxutils/lib/a8host/debug/omxutils.av5T \
#	-Wl,$(OMX_INSTALL_DIR)/rebuilt-binaries/ti/timmosal/lib/a8host/debug/timmosal.av5T \
#	-Wl,$(FC_INSTALL_DIR)/packages/ti/sdo/rcm/lib/debug/rcm_syslink.av5T \
#	-Wl,$(OMX_INSTALL_DIR)/rebuilt-binaries/ti/omx/domx/delegates/shmem/lib/a8host/debug/domx_delegates_shmem.av5T \
#	-Wl,$(FC_INSTALL_DIR)/packages/ti/sdo/fc/memutils/lib/release/memutils.av5T \
#	-Wl,$(OSAL_INSTALL_DIR)/packages/linuxdist/cstubs/lib/cstubs.a \
#	-Wl,$(OMX_INSTALL_DIR)/rebuilt-binaries/ti/omx/domx/lib/a8host/debug/domx.av5T \
#	-Wl,$(OMX_INSTALL_DIR)/rebuilt-binaries/vdec_snt/obj/ti816x-evm/a8host/debug/domx_delegates_shmem_cfg.ov5T \
#	-Wl,$(FC_INSTALL_DIR)/packages/ti/sdo/fc/global/lib/debug/fcsettings.av5T \
#	-Wl,$(LINUXUTILS_INSTALL_DIR)/packages/ti/sdo/linuxutils/cmem/lib/cmem.a470MV

