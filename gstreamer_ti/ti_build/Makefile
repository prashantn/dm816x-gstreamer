# Makefile
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
# Include common build settings.
#------------------------------------------------------------------------------
include ../Makefile.common

#------------------------------------------------------------------------------
# TI_PLUGINS
#     list of subdirectories containing TI plugins
#------------------------------------------------------------------------------
TI_PLUGINS = ticodecplugin

#------------------------------------------------------------------------------
# Build targets
#------------------------------------------------------------------------------
.PHONY: $(TI_PLUGINS) default help
default: help

all: $(TI_PLUGINS)

$(TI_PLUGINS): 
	@echo Building $@
	$(CMD_PREFIX) cd $@; ./autogen.sh --noconfigure
	$(CMD_PREFIX) cd $@; ./configure $(CONFIGURE_VERBOSE) $(CC_CONFIGURE_OPTS) CPPFLAGS="-DPlatform_$(PLATFORM)"
	$(CMD_PREFIX) $(MAKE) $(MAKE_VERBOSE) -C $@ clean
	$(CMD_PREFIX) $(MAKE) $(MAKE_VERBOSE) -C $@ $(BUILD_INSTALL)

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

