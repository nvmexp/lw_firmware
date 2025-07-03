#
# pmu_sw/config/makefile.mk
#
# Run chip-config.pl to build pmu-config.h and pmu-config.mk.
#
# *** Used by *all* builds that are rmconfig aware
#
# Each platform build will set some PMUCFG_* variables, then 'include' this file.
#
# Makefiles that 'include' this file:
#
#  PMU product app:                    //sw/.../pmu_sw/prod_app/makefile.lwmk
#      $RESMAN_ROOT/../../pmu_sw/prod_app/makefile.lwmk
#

# Default target, only used when testing this makefile
.PHONY: _pmu-config_default
_pmu-config_default: _pmu-config_all

# input vars, provide some typical defaults
PMUCFG_PROFILE                ?= pmu-gpus
PMUCFG_RESMAN_ROOT            ?= ../../drivers/resman
PMUCFG_PMUSW_ROOT             ?= ../../pmu_sw/prod_app
PMUCFG_DIR                    ?= $(PMUCFG_PMUSW_ROOT)/../config
PMUCFG_CHIPCONFIG_DIR         ?= ../../drivers/common/chip-config
PMUCFG_OUTPUTDIR              ?= $(PMUCFG_DIR)
PMUCFG_VERBOSE                ?= 
PMUCFG_HEADER                 ?= $(PMUCFG_OUTPUTDIR)/pmu-config.h
PMUCFG_MAKEFRAGMENT           ?= $(PMUCFG_OUTPUTDIR)/pmu-config.mk

PMUCFG_OPTIONS                ?=
PMUCFG_DEFINES                ?= 

PMUCFG_BUILD_OS               ?= unknown
PMUCFG_TARGET_PLATFORM        ?= $(RMCFG_BUILD_OS)
PMUCFG_TARGET_ARCH            ?= unknown

# PMUCFG_RUN must be an immediate mode var since it controls an 'include' below
PMUCFG_RUN                    ?= 1
PMUCFG_RUN                    := $(PMUCFG_RUN)

PMUCFG_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
PMUCFG_H2PH          ?= $(PMUCFG_PERL) $(PMUCFG_DIR)/util/h2ph.pl
PMUCFG_MKDIR         ?= $(if $(findstring undefined,$(origin MKDIR)), mkdir -p, $(MKDIR))
PMUCFG_Q             ?=


# Files we create that should be deleted by 'make clean'.
# May be used by "calling" makefile.
PMUCFG_CLEAN          =
PMUCFG_CLEAN         += $(PMUCFG_HEADER) $(PMUCFG_MAKEFRAGMENT)
PMUCFG_CLEAN         += $(PMUCFG_OUTPUTDIR)/g_*.h
PMUCFG_CLEAN         += $(PMUCFG_OUTPUTDIR)/halchk.log
PMUCFG_CLEAN         += $(PMUCFG_OUTPUTDIR)/halinfo.log

# table to colwert PMUCFG_VERBOSE into an option for chip-config.pl
_pmucfgVerbosity_quiet       = --quiet
_pmucfgVerbosity_@           = --quiet
_pmucfgVerbosity_default     = 
_pmucfgVerbosity_verbose     = --verbose
_pmucfgVerbosity_veryverbose = --veryverbose
_pmucfgVerbosity_            = $(_pmucfgVerbosity_default)
pmucfgVerbosityFlag := $(_pmucfgVerbosity_$(PMUCFG_VERBOSE))

# Files that implement chip-config.
# Assumes these vpath settings

pmucfgSrcFiles := 
pmucfgSrcFiles += $(PMUCFG_DIR)/pmu-config.cfg
pmucfgSrcFiles += $(PMUCFG_DIR)/makefile.mk

# pull in implementations list of chip-config to supplement $(pmucfgSrcFiles)
include $(PMUCFG_CHIPCONFIG_DIR)/Implementation.mk
pmucfgSrcFiles += $(addprefix $(PMUCFG_CHIPCONFIG_DIR)/, $(CHIPCONFIG_IMPLEMENTATION))

# these are all found via the magic of vpath if not found locally
vpath %.pm $(PMUCFG_DIR):$(PMUCFG_CHIPCONFIG_DIR)
pmucfgSrcFiles += Engines.pm
pmucfgSrcFiles += Features.pm

# update src file list with hal .def files
pmucfgSrcFiles += $(wildcard $(PMUCFG_DIR)/haldefs/*.def)

# pull in templates list to supplement $(pmucfgSrcFiles)
include $(PMUCFG_DIR)/templates/Templates.mk
pmucfgSrcFiles += $(addprefix $(PMUCFG_DIR)/templates/, $(PMUCFG_TEMPLATES))

# update src file list with sources .def files
include $(PMUCFG_DIR)/srcdefs/Srcdefs.mk
pmucfgSrcFiles += $(foreach srcfile,$(PMUCFG_SRCDEFS),$(PMUCFG_DIR)/srcdefs/$(srcfile).def)

# Ensure output directory(s) created before trying to generate pmuconfig.mk.
# On some platforms, eg OSX, need pmuconfig.h and pmuconfig.mk in different dirs.
# Use 'echo keepme > $@' instead of 'touch' which might not be avail on some platforms.
pmucfgOutputDirs = $(sort $(PMUCFG_OUTPUTDIR)/ $(dir $(PMUCFG_HEADER) $(PMUCFG_MAKEFRAGMENT)))
pmucfgKeeps      = $(addsuffix _keep,$(pmucfgOutputDirs))
pmucfgSrcFiles  += $(pmucfgKeeps)

$(pmucfgKeeps):
	$(PMUCFG_MKDIR) $(dir $@)
	echo keepme > $@

# HACK for DJGPP cmdline len
# DJGPP without CYGWIN is *special* - perl cmdline len must be < 128
DJGPP_WITH_CYGWIN ?=
pmucfgCmdLineLenHACK = 0
ifeq "$(PMUCFG_BUILD_OS)-$(DJGPP_WITH_CYGWIN)" "djgpp-false"
  pmucfgCmdLineLenHACK = 1
endif

# Extra options for chip-config needed by this makefile
pmucfgExtraOptions :=

# NOTE: lwrrently managed ./pmu-config script
# # lwrrently, just 1 platform for pmu sw
# pmucfgExtraOptions += --platform PMU

# NOTE: lwrrently managed ./pmu-config script
# # and just 1 arch
# pmucfgExtraOptions += --arch FALCON


### Run chip-config.pl to generate pmuconfig.mk

# rmconfig invocation args
pmucfgArgs = $(pmucfgVerbosityFlag) \
             --mode pmu-config \
             --config $(PMUCFG_DIR)/pmu-config.cfg \
             --profile $(PMUCFG_PROFILE) \
             --source-root $(PMU_SW)/prod_app \
             --output-dir $(PMUCFG_OUTPUTDIR) \
             --gen-header $(PMUCFG_HEADER) \
             --gen-makefile $(PMUCFG_MAKEFRAGMENT) \
             $(PMUCFG_OPTIONS) \
             $(pmucfgExtraOptions)

# generate pmu-config.mk
$(PMUCFG_MAKEFRAGMENT): $(pmucfgSrcFiles)
ifeq ($(pmucfgCmdLineLenHACK), 1)
	$(PMUCFG_Q)echo $(pmucfgArgs)  > @pmucfg.cmd
	$(PMUCFG_Q)$(PMUCFG_PERL) -I$(PMUCFG_DIR) $(PMUCFG_CHIPCONFIG_DIR)/chip-config.pl @pmucfg.cmd
	$(RM) @pmucfg.cmd
else
	$(PMUCFG_Q)$(PMUCFG_PERL) -I$(PMUCFG_DIR) $(PMUCFG_CHIPCONFIG_DIR)/chip-config.pl $(pmucfgArgs)
endif

# pmu-config.h is generated as a side-effect of generating pmu-config.mk
$(PMUCFG_HEADER): $(PMUCFG_MAKEFRAGMENT)

# g_sources.mk is generated as a side-effect of generating pmu-config.mk
$(PMUCFG_OUTPUTDIR)/g_sources.mk: $(PMUCFG_MAKEFRAGMENT)

# test targets
.PHONY: _pmu-config_all _pmu-config_clean
_pmu-config_all: $(PMUCFG_MAKEFRAGMENT)
	@true

_pmu-config_clean:
	$(RM) $(PMUCFG_CLEAN)

# Don't run rmconfig on 'make clean' if there is only 1 target in MAKECMDGOALS
_tmpCleanTargets = clean clobber _pmu-config_clean
MAKECMDGOALS ?=
ifneq (,$(filter $(_tmpCleanTargets), $(MAKECMDGOALS)))
  # skip only if no other targets in goal list
  ifeq (,$(filter-out $(_tmpCleanTargets), $(MAKECMDGOALS)))
    PMUCFG_RUN := 0
  endif
endif

# Finally, include the generated makefile which reflects PMUCFG_PROFILE.
# This 'include' triggers the chip-config.pl run.
#
# If pmu-config.mk doesn't exist, the 'include' below will generate a gmake
# warning even though gmake will recreate it.  The warning is unavoidable
# so we include a supplemental msg after gmake's warning so it should be
# apparent the missing pmu-config.mk is not a real problem.
#
# FAQ:  Why not use '-include' here to avoid a warning when pmu-config.mk does not exist?
#
#  '-include' would avoid the warning but it has a very bad side effect: any
#  errors during the regeneration are *ignored*.   So if chip-config failed with
#  an error, gmake would simply ignore that error and continue with a missing,
#  or even worse, stale, pmu-config.mk.
#
ifeq (1,$(PMUCFG_RUN))
  include $(PMUCFG_MAKEFRAGMENT)
  ifeq (,$(wildcard $(PMUCFG_MAKEFRAGMENT)))
    $(warning $(notdir $(PMUCFG_MAKEFRAGMENT)) does not exist.  Creating it now ... )
  endif
endif

