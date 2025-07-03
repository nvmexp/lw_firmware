#
# uproc/disp/dpu/config/makefile.mk
#
# Run chip-config.pl to build dpu-config.h and dpu-config.mk.
#
# *** Used by *all* builds that are rmconfig aware
#
# Each platform build will set some DPUCFG_* variables, then 'include' this file.
#
# Makefiles that 'include' this file:
#
#  DPU src:                    //sw/.../uproc/disp/dpu/src/makefile
#      $RESMAN_ROOT/../../uproc/disp/dpu/src/makefile
#

# Default target, only used when testing this makefile
.PHONY: _dpu-config_default
_dpu-config_default: _dpu-config_all

# input vars, provide some typical defaults
DPUCFG_PROFILE                ?= dpu-v0200
DPUCFG_RESMAN_ROOT            ?= ../../../../drivers/resman
DPUCFG_DPUSW_ROOT             ?= ../../../../uproc/disp/dpu/src
DPUCFG_DIR                    ?= $(DPUCFG_DPUSW_ROOT)/../config
DPUCFG_CHIPCONFIG_DIR         ?= ../../../../drivers/common/chip-config
DPUCFG_OUTPUTDIR              ?= $(DPUCFG_DIR)
DPUCFG_VERBOSE                ?=
DPUCFG_HEADER                 ?= $(DPUCFG_OUTPUTDIR)/dpu-config.h
DPUCFG_MAKEFRAGMENT           ?= $(DPUCFG_OUTPUTDIR)/dpu-config.mk

DPUCFG_OPTIONS                ?=
DPUCFG_DEFINES                ?=

DPUCFG_BUILD_OS               ?= unknown
DPUCFG_TARGET_PLATFORM        ?= $(RMCFG_BUILD_OS)
DPUCFG_TARGET_ARCH            ?= unknown

# DPUCFG_RUN must be an immediate mode var since it controls an 'include' below
DPUCFG_RUN                    ?= 1
DPUCFG_RUN                    := $(DPUCFG_RUN)

DPUCFG_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
DPUCFG_H2PH          ?= $(DPUCFG_PERL) $(DPUCFG_DIR)/util/h2ph.pl
DPUCFG_MKDIR         ?= $(if $(findstring undefined,$(origin MKDIR)), mkdir -p, $(MKDIR))
DPUCFG_Q             ?=


# Files we create that should be deleted by 'make clean'.
# May be used by "calling" makefile.
DPUCFG_CLEAN          =
DPUCFG_CLEAN         += $(DPUCFG_HEADER) $(DPUCFG_MAKEFRAGMENT)
DPUCFG_CLEAN         += $(DPUCFG_OUTPUTDIR)/g_*.h
DPUCFG_CLEAN         += $(DPUCFG_OUTPUTDIR)/halchk.log
DPUCFG_CLEAN         += $(DPUCFG_OUTPUTDIR)/halinfo.log

# table to colwert DPUCFG_VERBOSE into an option for chip-config.pl
_dpucfgVerbosity_quiet       = --quiet
_dpucfgVerbosity_@           = --quiet
_dpucfgVerbosity_default     =
_dpucfgVerbosity_verbose     = --verbose
_dpucfgVerbosity_veryverbose = --veryverbose
_dpucfgVerbosity_            = $(_dpucfgVerbosity_default)
dpucfgVerbosityFlag := $(_dpucfgVerbosity_$(DPUCFG_VERBOSE))

# Files that implement chip-config.
# Assumes these vpath settings

dpucfgSrcFiles :=
dpucfgSrcFiles += $(DPUCFG_DIR)/dpu-config.cfg
dpucfgSrcFiles += $(DPUCFG_DIR)/makefile.mk

# pull in implementations list of chip-config to supplement $(dpucfgSrcFiles)
include $(DPUCFG_CHIPCONFIG_DIR)/Implementation.mk
dpucfgSrcFiles += $(addprefix $(DPUCFG_CHIPCONFIG_DIR)/, $(CHIPCONFIG_IMPLEMENTATION))

# these are all found via the magic of vpath if not found locally
vpath %.pm $(DPUCFG_DIR):$(DPUCFG_CHIPCONFIG_DIR)
dpucfgSrcFiles += Engines.pm
dpucfgSrcFiles += Features.pm

# update src file list with hal .def files
dpucfgSrcFiles += $(wildcard $(DPUCFG_DIR)/haldefs/*.def)

# pull in templates list to supplement $(dpucfgSrcFiles)
include $(DPUCFG_DIR)/templates/Templates.mk
dpucfgSrcFiles += $(addprefix $(DPUCFG_DIR)/templates/, $(DPUCFG_TEMPLATES))

# update src file list with sources .def files
include $(DPUCFG_DIR)/srcdefs/Srcdefs.mk
dpucfgSrcFiles += $(foreach srcfile,$(DPUCFG_SRCDEFS),$(DPUCFG_DIR)/srcdefs/$(srcfile).def)

# Ensure output directory(s) created before trying to generate dpuconfig.mk.
# On some platforms, eg OSX, need dpuconfig.h and dpuconfig.mk in different dirs.
# Use 'echo keepme > $@' instead of 'touch' which might not be avail on some platforms.
dpucfgOutputDirs = $(sort $(DPUCFG_OUTPUTDIR)/ $(dir $(DPUCFG_HEADER) $(DPUCFG_MAKEFRAGMENT)))
dpucfgKeeps      = $(addsuffix _keep,$(dpucfgOutputDirs))
dpucfgSrcFiles  += $(dpucfgKeeps)

$(dpucfgKeeps):
	$(DPUCFG_MKDIR) $(dir $@)
	echo keepme > $@

# HACK for DJGPP cmdline len
# DJGPP without CYGWIN is *special* - perl cmdline len must be < 128
DJGPP_WITH_CYGWIN ?=
dpucfgCmdLineLenHACK = 0
ifeq "$(DPUCFG_BUILD_OS)-$(DJGPP_WITH_CYGWIN)" "djgpp-false"
  dpucfgCmdLineLenHACK = 1
endif

# Extra options for chip-config needed by this makefile
dpucfgExtraOptions :=

# NOTE: lwrrently managed ./dpu-config script
# # lwrrently, just 1 platform for dpu sw
# dpucfgExtraOptions += --platform DPU

# NOTE: lwrrently managed ./dpu-config script
# # and just 1 arch
# dpucfgExtraOptions += --arch FALCON


### Run chip-config.pl to generate dpuconfig.mk

# rmconfig invocation args
dpucfgArgs = $(dpucfgVerbosityFlag) \
             --mode dpu-config \
             --config $(DPUCFG_DIR)/dpu-config.cfg \
             --profile $(DPUCFG_PROFILE) \
             --source-root $(DPU_SW)/src \
             --output-dir $(DPUCFG_OUTPUTDIR) \
             --gen-header $(DPUCFG_HEADER) \
             --gen-makefile $(DPUCFG_MAKEFRAGMENT) \
             --no-halgen-share-similar \
             --no-halgen-share \
             $(DPUCFG_OPTIONS) \
             $(dpucfgExtraOptions)

# generate dpu-config.mk
$(DPUCFG_MAKEFRAGMENT): $(dpucfgSrcFiles)
ifeq ($(dpucfgCmdLineLenHACK), 1)
	$(DPUCFG_Q)echo $(dpucfgArgs)  > @dpucfg.cmd
	$(DPUCFG_Q)$(DPUCFG_PERL) -I$(DPUCFG_DIR) $(DPUCFG_CHIPCONFIG_DIR)/chip-config.pl @dpucfg.cmd
	$(RM) @dpucfg.cmd
else
	$(DPUCFG_Q)$(DPUCFG_PERL) -I$(DPUCFG_DIR) $(DPUCFG_CHIPCONFIG_DIR)/chip-config.pl $(dpucfgArgs)
endif

# dpu-config.h is generated as a side-effect of generating dpu-config.mk
$(DPUCFG_HEADER): $(DPUCFG_MAKEFRAGMENT)

# g_sources.mk is generated as a side-effect of generating dpu-config.mk
$(DPUCFG_OUTPUTDIR)/g_sources.mk: $(DPUCFG_MAKEFRAGMENT)

# test targets
.PHONY: _dpu-config_all _dpu-config_clean
_dpu-config_all: $(DPUCFG_MAKEFRAGMENT)
	@true

_dpu-config_clean:
	$(RM) $(DPUCFG_CLEAN)

# Don't run rmconfig on 'make clean' if there is only 1 target in MAKECMDGOALS
_tmpCleanTargets = clean clobber _dpu-config_clean
MAKECMDGOALS ?=
ifneq (,$(filter $(_tmpCleanTargets), $(MAKECMDGOALS)))
  # skip only if no other targets in goal list
  ifeq (,$(filter-out $(_tmpCleanTargets), $(MAKECMDGOALS)))
    DPUCFG_RUN := 0
  endif
endif

# Finally, include the generated makefile which reflects DPUCFG_PROFILE.
# This 'include' triggers the chip-config.pl run.
ifeq (1,$(DPUCFG_RUN))
  -include $(DPUCFG_MAKEFRAGMENT)
endif
