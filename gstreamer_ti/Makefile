# Makefile
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
# Include common build settings.
#------------------------------------------------------------------------------
include ./Makefile.common

#------------------------------------------------------------------------------
# SUBDIRS
#     list of subdirectories to build
# TARGET_DEMO_DIR
#     target directory for gstreamer demo scripts
#------------------------------------------------------------------------------
SUBDIRS         = opensource_build ti_build
TARGET_DEMO_DIR = $(TARGET_ROOT_DIR)/opt/gstreamer_demo

#------------------------------------------------------------------------------
# Build targets
#------------------------------------------------------------------------------
.PHONY: $(SUBDIRS) default help all gstreamer_demo

default: help

all: 
	$(CMD_PREFIX) $(MAKE) opensource_build $(BUILD_INSTALL)
	$(CMD_PREFIX) $(MAKE) ti_build         $(BUILD_INSTALL)
	$(CMD_PREFIX) $(MAKE) gstreamer_demo   $(BUILD_INSTALL)

clean_dev: 
	@echo "Removing header files....."
	$(CMD_PREFIX) rm -rf $(TARGET_GSTREAMER_DIR)/include
	@echo "Removing pkgconfig files....."
	$(CMD_PREFIX) rm -rf $(TARGET_GSTREAMER_DIR)/lib/pkgconfig
	@echo "Removing static libraries....."
	$(CMD_PREFIX) find $(TARGET_GSTREAMER_DIR) -type f -name "*.a" -exec rm {} \;
	$(CMD_PREFIX) find $(TARGET_GSTREAMER_DIR) -type f -name "*.la" -exec rm {} \;
	@echo "Stripping executables and shared libraries....."
	$(CMD_PREFIX) find $(TARGET_GSTREAMER_DIR) -type f -exec $(MVTOOL_PREFIX)strip {} \; > /dev/null 2>&1

clean_docs: 
	@echo "Removing docs....."
	$(CMD_PREFIX) rm -rf $(TARGET_GSTREAMER_DIR)/share/man
	$(CMD_PREFIX) rm -rf $(TARGET_GSTREAMER_DIR)/share/doc
	$(CMD_PREFIX) rm -rf $(TARGET_GSTREAMER_DIR)/share/gtk-doc
	$(CMD_PREFIX) rm -rf $(TARGET_GSTREAMER_DIR)/share/info

$(SUBDIRS): 
	@echo Building $@
	$(CMD_PREFIX) $(MAKE) $(MAKE_VERBOSE) -C $@ all $(BUILD_INSTALL) VERBOSE=$(VERBOSE)

gstreamer_demo:
	@echo copying gstreamer_demo directory to target
	$(CMD_PREFIX) install -d ./gstreamer_demo $(TARGET_DEMO_DIR)
	$(CMD_PREFIX) install -d ./gstreamer_demo/$(GST_TI_PLATFORM) $(TARGET_DEMO_DIR)/$(GST_TI_PLATFORM)
	$(CMD_PREFIX) install ./gstreamer_demo/$(GST_TI_PLATFORM)/* $(TARGET_DEMO_DIR)/$(GST_TI_PLATFORM)
	$(CMD_PREFIX) install ./gstreamer_demo/shared/* $(TARGET_DEMO_DIR)/$(GST_TI_PLATFORM)
	
	@echo copying relevant kernel modules
	$(CMD_PREFIX) if [ -d $(DVSDK_INSTALL_DIR)/kernel_binaries/$(PLATFORM) ]; then \
	    install $(DVSDK_INSTALL_DIR)/kernel_binaries/$(PLATFORM)/* $(TARGET_DEMO_DIR)/$(GST_TI_PLATFORM); \
	fi
	
	@echo copying mapdmaq if platform needs it
	$(CMD_PREFIX) if [ $(GST_TI_PLATFORM) = "dm355" ]; then \
	    install $(DVSDK_INSTALL_DIR)/mapdmaq/mapdmaq $(TARGET_DEMO_DIR)/$(GST_TI_PLATFORM); \
	fi
	
	@echo copying mapdmaq-hd if platform needs it
	$(CMD_PREFIX) if [ $(GST_TI_PLATFORM) = "dm6467" ]; then \
	    install $(DVSDK_INSTALL_DIR)/mapdmaq-hd/mapdmaq-hd $(TARGET_DEMO_DIR)/$(GST_TI_PLATFORM); \
	fi
	
	@echo copying codec combos if platform needs it
	$(CMD_PREFIX) for file in `find $(CODEC_INSTALL_DIR) -name "*.x64P"`; do \
	    install $${file} $(TARGET_DEMO_DIR)/$(GST_TI_PLATFORM); \
	done

install:

#------------------------------------------------------------------------------
# clean:  clean up everything
#------------------------------------------------------------------------------
CLEAN_SUBDIRS = $(addsuffix .clean, $(SUBDIRS))
.PHONY: clean $(CLEAN_SUBDIRS)
clean: $(CLEAN_SUBDIRS)
$(CLEAN_SUBDIRS):
	@echo "***** Cleaning $(basename $@)"
	$(CMD_PREFIX) make -C $(basename $@) clean

#------------------------------------------------------------------------------
# help:  build target for help screen
#------------------------------------------------------------------------------
.PHONY: help
help:
	@echo
	@echo "Make usage for all components:"
	@echo
	@echo "    Build only:"
	@echo "        make all"
	@echo "    Build and install:"
	@echo "        make all install"
	@echo

