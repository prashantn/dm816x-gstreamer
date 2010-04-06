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
# Location of distribution files (.tar.gz archives)
#------------------------------------------------------------------------------
DISTFILES_DIR=$(CURDIR)/distfiles
PATCHFILES_DIR=$(CURDIR)/patchfiles

#------------------------------------------------------------------------------
# Packages processed by this build script.
#------------------------------------------------------------------------------
include ./Packages.make
PACKAGES = $(BASE_PACKAGES) $(PLUGIN_PACKAGES)

#------------------------------------------------------------------------------
# Build targets
#------------------------------------------------------------------------------
.PHONY: default install all base plugins

default:     help

all: .plugins

.base:
	$(CMD_PREFIX) $(MAKE) base    $(BUILD_INSTALL) && touch $@

.plugins: .base
	$(CMD_PREFIX) $(MAKE) plugins $(BUILD_INSTALL) && touch $@

base:
	$(CMD_PREFIX) $(MAKE) glib         $(BUILD_INSTALL)
	$(CMD_PREFIX) $(MAKE) check        $(BUILD_INSTALL)
	$(CMD_PREFIX) $(MAKE) gstreamer    $(BUILD_INSTALL)
	$(CMD_PREFIX) $(MAKE) liboil       $(BUILD_INSTALL)
	$(CMD_PREFIX) $(MAKE) id3tag       $(BUILD_INSTALL)
	$(CMD_PREFIX) $(MAKE) mad          $(BUILD_INSTALL)
	$(CMD_PREFIX) $(MAKE) lame         $(BUILD_INSTALL)
	$(CMD_PREFIX) $(MAKE) plugins_base $(BUILD_INSTALL)

plugins:     $(PLUGIN_PACKAGES)
$(PACKAGES): % : %.build

install:

#------------------------------------------------------------------------------
# clean: Remove all build sources
#------------------------------------------------------------------------------
CLEAN_PACKAGES	 = $(addsuffix .clean, $(PACKAGES))
.PHONY: clean $(CLEAN_PACKAGES)
clean:	$(CLEAN_PACKAGES)
	rm -f .base .plugins

$(CLEAN_PACKAGES):
	@echo "***** Cleaning $(basename $@)"
	$(CMD_PREFIX) rm -rf $(PACKAGE_$(basename $@)_ARCHIVE_BASENAME)

#------------------------------------------------------------------------------
# Build a package
#------------------------------------------------------------------------------
BUILD_PACKAGES	 = $(addsuffix .build, $(PACKAGES))
.PHONY: $(BUILD_PACKAGES)
$(BUILD_PACKAGES): %.build : %.patch_postconfig
	$(CMD_PREFIX) if [ "$(PACKAGE_$(basename $@)_BUILD_DIRS)" = "" ]; then \
	    echo "***** building $(basename $@)"; \
	    $(MAKE) $(MAKE_VERBOSE) -C $(PACKAGE_$(basename $@)_ARCHIVE_BASENAME) $(BUILD_INSTALL); \
	else \
	    ( for dir in $(PACKAGE_$(basename $@)_BUILD_DIRS); do \
		echo "***** building $(basename $@) ($${dir})"; \
		$(MAKE) $(MAKE_VERBOSE) -C $(PACKAGE_$(basename $@)_ARCHIVE_BASENAME)/$${dir} $(BUILD_INSTALL) || exit -1; \
	    done; ) ;\
	fi

#------------------------------------------------------------------------------
# Apply patch files to a package after the configure step
#------------------------------------------------------------------------------
PATCH_PACKAGES	 = $(addsuffix .patch_postconfig, $(PACKAGES))
.PHONY: $(PATCH_PACKAGES)
$(PATCH_PACKAGES): %.patch_postconfig : %.configure
	$(CMD_PREFIX) if [ "$(PACKAGE_$(basename $@)_POSTCONFIG_PATCHES)" != "" ]; then \
	    echo "***** Patching $(basename $@)"; \
	    for patch in $(PACKAGE_$(basename $@)_POSTCONFIG_PATCHES); do \
		patch $(PATCH_VERBOSE) -d $(PACKAGE_$(basename $@)_ARCHIVE_BASENAME) -p1 < $(PATCHFILES_DIR)/$(PACKAGE_$(basename $@)_ARCHIVE_BASENAME)/$${patch}; \
	    done; \
	fi

#------------------------------------------------------------------------------
# Configure a package
#------------------------------------------------------------------------------
CONFIGURE_PACKAGES   = $(addsuffix .configure, $(PACKAGES))
.PHONY: $(CONFIGURE_PACKAGES)
$(CONFIGURE_PACKAGES): %.configure : %.patch_preconfig
	@echo "***** Configuring $(basename $@)"
	$(CMD_PREFIX) cd $(PACKAGE_$(basename $@)_ARCHIVE_BASENAME); ./configure $(CONFIGURE_VERBOSE) $(CC_CONFIGURE_OPTS) $(PACKAGE_$(basename $@)_CONFIGURE_OPTS)

#------------------------------------------------------------------------------
# Apply patch files to a package before the configure step
#------------------------------------------------------------------------------
PATCH_PACKAGES	 = $(addsuffix .patch_preconfig, $(PACKAGES))
.PHONY: $(PATCH_PACKAGES)
$(PATCH_PACKAGES): %.patch_preconfig : %.extract
	$(CMD_PREFIX) if [ "$(PACKAGE_$(basename $@)_PRECONFIG_PATCHES)" != "" ]; then \
	    echo "***** Patching $(basename $@)"; \
	    for patch in $(PACKAGE_$(basename $@)_PRECONFIG_PATCHES); do \
		patch $(PATCH_VERBOSE) -d $(PACKAGE_$(basename $@)_ARCHIVE_BASENAME) -p1 < $(PATCHFILES_DIR)/$(PACKAGE_$(basename $@)_ARCHIVE_BASENAME)/$${patch}; \
	    done; \
	fi

#------------------------------------------------------------------------------
# Extract a package
#------------------------------------------------------------------------------
EXTRACT_PACKAGES   = $(addsuffix .extract, $(PACKAGES))
.PHONY: $(EXTRACT_PACKAGES)
$(EXTRACT_PACKAGES): %.extract : 
	@echo "***** Extracting $(basename $@)"
	$(CMD_PREFIX) tar $(TAR_VERBOSE)xzf $(DISTFILES_DIR)/$(PACKAGE_$(basename $@)_ARCHIVE_BASENAME).tar.gz

#------------------------------------------------------------------------------
# help:  build target for help screen
#------------------------------------------------------------------------------
HELP_DESC_BASE = $(foreach pkg,$(BASE_PACKAGES)," $(pkg) :\n	 $(PACKAGE_$(pkg)_DESCRIPTION)\n")
HELP_DESC_PLUGINS = $(foreach pkg,$(PLUGIN_PACKAGES)," $(pkg) :\n     $(PACKAGE_$(pkg)_DESCRIPTION)\n")

.PHONY: help
help:
	@echo
	@echo "You must specify a build target.  Available targets are:"
	@echo
	@echo " General Targets"
	@echo " ---------------"
	@echo "  all :"
	@echo "      build everything"
	@echo "  base :"
	@echo "      build all base components"
	@echo "  plugins :"
	@echo "      build all plugins"
	@echo
	@echo " Base Component Targets"
	@echo " ----------------------"
	@echo -e " "$(HELP_DESC_BASE)
	@echo " Plugin Targets"
	@echo " --------------"
	@echo -e " "$(HELP_DESC_PLUGINS)

