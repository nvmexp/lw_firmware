#
# uproc/flcnlib/config/makefile.mk
#
# Run chip-config.pl to build makefile and header files
#
# Each client build will set some FLCNLIBCFG_* variables, then 'include' this file.
#

# Default target, only used when testing this makefile
.PHONY: _flcnlib-config_default
_flcnlib-config_default: _flcnlib-config_all

# LW_SOURCE is provided by lwmake
LW_SOURCE                          ?= ../../../..

# input FLCNLIBCFG_* variables, provide some typical defaults
FLCNLIBCFG_PROFILE                 ?= flcnlib-gpus
FLCNLIBCFG_FLCNLIB_ROOT            ?= $(LW_SOURCE)/uproc/libs
FLCNLIBCFG_SRC_ROOT                ?= $(FLCNLIBCFG_FLCNLIB_ROOT)
FLCNLIBCFG_DIR                     ?= $(FLCNLIBCFG_FLCNLIB_ROOT)/flcnlib_config
FLCNLIBCFG_CHIPCONFIG_DIR          ?= $(LW_SOURCE)/drivers/common/chip-config
FLCNLIBCFG_OUTPUTDIR               ?= $(FLCNLIBCFG_DIR)
FLCNLIBCFG_VERBOSE                 ?=
FLCNLIBCFG_HEADER                  ?= $(FLCNLIBCFG_OUTPUTDIR)/flcnlib-config.h
FLCNLIBCFG_MAKEFRAGMENT            ?= $(FLCNLIBCFG_OUTPUTDIR)/flcnlib-config.mk
FLCNLIBCFG_CONFIGFILE              ?= $(FLCNLIBCFG_DIR)/flcnlib-config.cfg

# options added in this makefile.mk
FLCNLIBCFG_OPTIONS                 ?=


# FLCNLIBCFG_RUN must be an immediate mode var since it controls an 'include' below
FLCNLIBCFG_RUN                     ?= 1
FLCNLIBCFG_RUN                     := $(FLCNLIBCFG_RUN)

FLCNLIBCFG_PERL           ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
FLCNLIBCFG_MKDIR          ?= $(if $(findstring undefined,$(origin MKDIR)), mkdir -p, $(MKDIR))
FLCNLIBCFG_Q              ?=


# Files we create that should be deleted by 'make clean'.
# May be used by "calling" makefile.
FLCNLIBCFG_CLEAN           =
FLCNLIBCFG_CLEAN          += $(FLCNLIBCFG_HEADER) $(FLCNLIBCFG_MAKEFRAGMENT)
FLCNLIBCFG_CLEAN          += $(FLCNLIBCFG_OUTPUTDIR)/g_*.h
FLCNLIBCFG_CLEAN          += $(FLCNLIBCFG_OUTPUTDIR)/halchk.log
FLCNLIBCFG_CLEAN          += $(FLCNLIBCFG_OUTPUTDIR)/halinfo.log

# table to colwert RISCVLIBCFG_VERBOSE into an option for chip-config.pl
_flcnlibcfgVerbosity_quiet        = --quiet
_flcnlibcfgVerbosity_@            = --quiet
_flcnlibcfgVerbosity_default      =
_flcnlibcfgVerbosity_verbose      = --verbose
_flcnlibcfgVerbosity_veryverbose  = --veryverbose
_flcnlibcfgVerbosity_             = $(_flcnlibcfgVerbosity_default)
flcnlibcfgVerbosityFlag := $(_flcnlibcfgVerbosity_$(FLCNLIBCFG_VERBOSE))

# Files that implement chip-config.
# Assumes these vpath settings

flcnlibcfgSrcFiles :=
flcnlibcfgSrcFiles += $(FLCNLIBCFG_CONFIGFILE)
flcnlibcfgSrcFiles += $(FLCNLIBCFG_DIR)/makefile.mk

# pull in implementations list of chip-config to supplement $(flcnlibcfgSrcFiles)
include $(FLCNLIBCFG_CHIPCONFIG_DIR)/Implementation.mk
flcnlibcfgSrcFiles += $(addprefix $(FLCNLIBCFG_CHIPCONFIG_DIR)/, $(CHIPCONFIG_IMPLEMENTATION))

# these are all found via the magic of vpath if not found locally
vpath %.pm $(FLCNLIBCFG_DIR):$(FLCNLIBCFG_CHIPCONFIG_DIR)
flcnlibcfgSrcFiles += Modules.pm
flcnlibcfgSrcFiles += Features.pm

# pull in haldef list to define $(HALDEFS)
include $(FLCNLIBCFG_DIR)/haldefs/Haldefs.mk
flcnlibcfgSrcFiles += $(foreach hd,$(FLCNLIBCFG_HALDEFS),$(FLCNLIBCFG_DIR)/haldefs/$(hd).def)

# pull in templates list to supplement $(flcnlibcfgSrcFiles)
include $(FLCNLIBCFG_DIR)/templates/Templates.mk
flcnlibcfgSrcFiles += $(addprefix $(FLCNLIBCFG_DIR)/templates/, $(FLCNLIBCFG_TEMPLATES))

# update src file list with sources .def files
include $(FLCNLIBCFG_DIR)/srcdefs/Srcdefs.mk
flcnlibcfgSrcFiles += $(foreach srcfile,$(FLCNLIBCFG_SRCDEFS),$(FLCNLIBCFG_DIR)/srcdefs/$(srcfile).def)

# Ensure output directory(s) created before trying to generate flcnlibconfig.mk.
# On some platforms, eg OSX, need flcnlibconfig.h and flcnlibconfig.mk in different dirs.
# Use 'echo keepme > $@' instead of 'touch' which might not be avail on some platforms.
flcnlibcfgOutputDirs = $(sort $(FLCNLIBCFG_OUTPUTDIR)/ $(dir $(FLCNLIBCFG_HEADER) $(FLCNLIBCFG_MAKEFRAGMENT)))
flcnlibcfgKeeps      = $(addsuffix _keep,$(flcnlibcfgOutputDirs))
flcnlibcfgSrcFiles  += $(flcnlibcfgKeeps)

$(flcnlibcfgKeeps):
	$(FLCNLIBCFG_MKDIR) $(dir $@)
	$(ECHO) keepme > $@

### Run chip-config.pl to generate flcnlibconfig.mk
# rmconfig invocation args
flcnlibcfgArgs = $(flcnlibcfgVerbosityFlag) \
             --mode         flcnlib-config \
             --config       $(FLCNLIBCFG_CONFIGFILE) \
             --profile      $(FLCNLIBCFG_PROFILE) \
             --source-root  $(FLCNLIBCFG_SRC_ROOT) \
             --output-dir   $(FLCNLIBCFG_OUTPUTDIR) \
             --gen-header   $(FLCNLIBCFG_HEADER) \
             --gen-makefile $(FLCNLIBCFG_MAKEFRAGMENT) \
             $(FLCNLIBCFG_OPTIONS)

# FLCNLIBCFG_OPTIONS etc. might get changed by different
# make flags. Add a dependency to check the value of flcnlibcfgArgs
# and re-run chip-config if it changed.
FLCNLIBCFG_ARGS_STAMP_FILE := $(OUTPUTDIR)/_flcnLibCfgArgsStamp
FLCNLIBCFG_ARGS_OLD :=

ifneq ($(wildcard $(FLCNLIBCFG_ARGS_STAMP_FILE)),)
  FLCNLIBCFG_ARGS_OLD := $(shell $(CAT) $(FLCNLIBCFG_ARGS_STAMP_FILE))
endif

ifneq ($(flcnlibcfgArgs),$(FLCNLIBCFG_ARGS_OLD))
  .PHONY: $(FLCNLIBCFG_ARGS_STAMP_FILE)
endif

$(FLCNLIBCFG_ARGS_STAMP_FILE):
	$(ECHO) "$(flcnlibcfgArgs)" > $@

# generate flcnlib-config.mk
$(FLCNLIBCFG_MAKEFRAGMENT): $(flcnlibcfgSrcFiles) $(FLCNLIBCFG_ARGS_STAMP_FILE)
	$(FLCNLIBCFG_Q)$(FLCNLIBCFG_PERL) -I$(FLCNLIBCFG_DIR) $(FLCNLIBCFG_CHIPCONFIG_DIR)/chip-config.pl $(flcnlibcfgArgs)

# flcnlib-config.h is generated as a side-effect of generating flcnlib-config.mk
$(FLCNLIBCFG_HEADER): $(FLCNLIBCFG_MAKEFRAGMENT)

# g_sources.mk is generated as a side-effect of generating flcnlib-config.mk
$(FLCNLIBCFG_OUTPUTDIR)/g_sources.mk: $(FLCNLIBCFG_MAKEFRAGMENT)

# test targets
.PHONY: _flcnlib-config_all _flcnlib-config_clean
_flcnlib-config_all: $(FLCNLIBCFG_MAKEFRAGMENT)
	@true

_flcnlib-config_clean:
	$(RM) $(FLCNLIBCFG_CLEAN)

# Don't run rmconfig on 'make clean' if there is only 1 target in MAKECMDGOALS
_tmpCleanTargets = clean clobber _flcnlib-config_clean
MAKECMDGOALS ?=
ifneq (,$(filter $(_tmpCleanTargets), $(MAKECMDGOALS)))
  # skip only if no other targets in goal list
  ifeq (,$(filter-out $(_tmpCleanTargets), $(MAKECMDGOALS)))
    FLCNLIBCFG_RUN := 0
  endif
endif

# Finally, include the generated makefile which reflects FLCNLIBCFG_PROFILE.
# This 'include' triggers the chip-config.pl run.
ifeq (1,$(FLCNLIBCFG_RUN))
  -include $(FLCNLIBCFG_MAKEFRAGMENT)
endif
