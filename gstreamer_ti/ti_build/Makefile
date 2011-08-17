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
include ../Makefile.common

#------------------------------------------------------------------------------
# Check if we need to enable C6Accel elements
#------------------------------------------------------------------------------
ifeq ($(ENABLE_C6ACCEL), true)
 EXTRA_CONFIGURE_OPTS="--enable-c6accel"
endif

#------------------------------------------------------------------------------
# Check if we need to enable tidisplaysink2 element
#------------------------------------------------------------------------------
ifeq ($(ENABLE_TIDISPLAYSINK2), true)
 EXTRA_CONFIGURE_OPTS +="--enable-tidisplaysink2"
endif

#------------------------------------------------------------------------------
# TI_PLUGINS
#     list of subdirectories containing TI plugins
#------------------------------------------------------------------------------
ifeq ($(GST_TI_PLATFORM), dm816x)
 TI_PLUGINS = gst-openmax
 CFLAGS_dm816x     += -DUSE_OMXTICORE
else
 TI_PLUGINS = ticodecplugin
endif

#------------------------------------------------------------------------------
# Build targets
#------------------------------------------------------------------------------
.PHONY: $(TI_PLUGINS) default help
default: help

all: $(TI_PLUGINS)

$(TI_PLUGINS): 
	@echo Building $@
	$(CMD_PREFIX) cd $@; ./autogen.sh --noconfigure
	$(CMD_PREFIX) cd $@; ./configure $(CONFIGURE_VERBOSE) $(CC_CONFIGURE_OPTS) $(EXTRA_CONFIGURE_OPTS)
	$(CMD_PREFIX) $(MAKE) $(MAKE_VERBOSE) -C $@ $(BUILD_INSTALL)

my: 
	@echo Building gst-openmax
	$(CMD_PREFIX) $(MAKE) $(MAKE_VERBOSE) -C gst-openmax $(BUILD_INSTALL)

install:

clean:
	for dir in $(TI_PLUGINS); do \
	    if [ -r $${dir}/Makefile ] ; then \
	        cd $${dir}; $(MAKE) distclean; \
	    fi ; \
	done

#------------------------------------------------------------------------------
# help:  build target for help screen
#------------------------------------------------------------------------------
.PHONY: help
help:
	@echo
	@echo "Make usage for building TI plugins:"
	@echo
	@echo "    Build only:"
	@echo "        make all"
	@echo "    Build and install:"
	@echo "        make all install"
	@echo

