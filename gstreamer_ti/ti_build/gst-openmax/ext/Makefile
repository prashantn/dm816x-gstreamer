# makefile to build shared lib
all:
	@mkdir -p lib
	@echo "  CCLD   lib/libOMX_Core.so"
	@$(CROSS_COMPILE)gcc -Wl,-soname,libOMX_Core -shared  \
	$(OMX_INSTALL_DIR)/rebuilt-binaries/ti/omx/omxcore/lib/a8host/debug/omxcore.av5T \
	$(OMX_INSTALL_DIR)/rebuilt-binaries/ti/omx/domx/lib/a8host/debug/domx.av5T \
	$(OSAL_INSTALL_DIR)/packages/linuxdist/build/lib/osal.a \
	$(OMTB_INSTALL_DIR)/rebuilt-binaries/dm816xbm/obj/ti816x-evm/a8host/debug/omxcore_cfg.ov5T \
	$(OMTB_INSTALL_DIR)/rebuilt-binaries/dm816xbm/obj/ti816x-evm/a8host/debug/domx_cfg.ov5T \
    $(OMX_INSTALL_DIR)/rebuilt-binaries/ti/omx/memcfg/lib/a8host/debug/memcfg.av5T \
    $(OMTB_INSTALL_DIR)/rebuilt-binaries/dm816xbm/obj/ti816x-evm/a8host/debug/memtbl_cfg.ov5T  \
    $(OMX_INSTALL_DIR)/rebuilt-binaries/ti/timmosal/lib/a8host/debug/timmosal.av5T \
	$(SYSLINK_INSTALL_DIR)/ti/syslink/lib/syslink.a_release \
	$(FC_INSTALL_DIR)/packages/ti/sdo/rcm/lib/debug/rcm_syslink.av5T \
	$(OMX_INSTALL_DIR)/rebuilt-binaries/ti/omx/domx/delegates/shmem/lib/a8host/debug/domx_delegates_shmem.av5T \
	$(FC_INSTALL_DIR)/packages/ti/sdo/fc/memutils/lib/release/memutils.av5T \
	$(OSAL_INSTALL_DIR)/packages/linuxdist/cstubs/lib/cstubs.a \
	$(OMX_INSTALL_DIR)/rebuilt-binaries/ti/omx/domx/obj/a8host/debug/domxProxyWrappers.ov5T \
	$(FC_INSTALL_DIR)/packages/ti/sdo/fc/global/lib/debug/fcsettings.av5T \
	$(LINUXUTILS_INSTALL_DIR)/packages/ti/sdo/linuxutils/cmem/lib/cmem.a470MV \
	$(OMX_INSTALL_DIR)/rebuilt-binaries/ti/omx/domx/obj/a8host/debug/OmxRpcCbSkel.ov5T \
	$(OMTB_INSTALL_DIR)/rebuilt-binaries/dm816xbm/obj/ti816x-evm/a8host/debug/domx_delegates_shmem_cfg.ov5T \
	-lc -lstdc++ -lm -pthread -lrt -o lib/libOMX_Core.so -Wl,--fix-cortex-a8 -Wl,--no-undefined \
	-Wl,-u,OMX_Init -Wl,-u,OMX_Deinit -Wl,-u,OMX_ComponentNameEnum -Wl,-u,OMX_GetHandle -Wl,-u,OMX_FreeHandle \
	 -Wl,-u,OMX_GetRolesOfComponent

clean:
	rm -rf lib

install: all
