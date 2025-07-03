#
# uproc/soe_riscv/config/makefile.mk
#
# Run chip-config.pl to build soe-config.h and soe-config.mk.
#
# *** Used by *all* builds that are rmconfig aware
#
# Each platform build will set some SOECFG_* variables, then 'include' this file.
#
# Makefiles that 'include' this file:
#
#  SOE product app:                    //sw/.../uproc/soe_riscv/src/makefile.lwmk
#      $RESMAN_ROOT/../../uproc/soe_riscv/src/makefile.lwmk
#

# Default target, only used when testing this makefile
.PHONY: _soe-config_default
_soe-config_default: _soe-config_all

# input vars, provide some typical defaults
SOECFG_PROFILE                ?= soe-gpus
SOECFG_RESMAN_ROOT            ?= ../../../drivers/resman
SOECFG_SOESW_ROOT            = ../src/
SOECFG_DIR                    ?= ../config
SOECFG_CHIPCONFIG_DIR         ?= ../../../drivers/common/chip-config
SOECFG_OUTPUTDIR              ?= $(SOECFG_DIR)
SOECFG_VERBOSE                ?= 
SOECFG_HEADER                 ?= $(SOECFG_OUTPUTDIR)/soe-config.h
SOECFG_MAKEFRAGMENT           ?= $(SOECFG_OUTPUTDIR)/soe-config.mk

SOECFG_OPTIONS                ?=
SOECFG_DEFINES                ?= 

SOECFG_BUILD_OS               ?= unknown
SOECFG_TARGET_PLATFORM        ?= $(RMCFG_BUILD_OS)
SOECFG_TARGET_ARCH            ?= unknown

# SOECFG_RUN must be an immediate mode var since it controls an 'include' below
SOECFG_RUN                    ?= 1
SOECFG_RUN                    := $(SOECFG_RUN)

SOECFG_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
SOECFG_H2PH          ?= $(SOECFG_PERL) $(SOECFG_DIR)/util/h2ph.pl
SOECFG_MKDIR         ?= $(if $(findstring undefined,$(origin MKDIR)), mkdir -p, $(MKDIR))
SOECFG_Q             ?=


# Files we create that should be deleted by 'make clean'.
# May be used by "calling" makefile.
SOECFG_CLEAN          =
SOECFG_CLEAN         += $(SOECFG_HEADER) $(SOECFG_MAKEFRAGMENT)
SOECFG_CLEAN         += $(SOECFG_OUTPUTDIR)/g_*.h
SOECFG_CLEAN         += $(SOECFG_OUTPUTDIR)/halchk.log
SOECFG_CLEAN         += $(SOECFG_OUTPUTDIR)/halinfo.log

# table to colwert SOECFG_VERBOSE into an option for chip-config.pl
_soecfgVerbosity_quiet       = --quiet
_soecfgVerbosity_@           = --quiet
_soecfgVerbosity_default     =
_soecfgVerbosity_verbose     = --verbose
_soecfgVerbosity_veryverbose = --veryverbose
_soecfgVerbosity_            = $(_soecfgVerbosity_default)
soecfgVerbosityFlag := $(_soecfgVerbosity_$(SOECFG_VERBOSE))

# Files that implement chip-config.
# Assumes these vpath settings

soecfgSrcFiles := 
soecfgSrcFiles += $(SOECFG_DIR)/soe-config.cfg
soecfgSrcFiles += $(SOECFG_DIR)/makefile.mk

# pull in implementations list of chip-config to supplement $(soecfgSrcFiles)
include $(SOECFG_CHIPCONFIG_DIR)/Implementation.mk
soecfgSrcFiles += $(addprefix $(SOECFG_CHIPCONFIG_DIR)/, $(CHIPCONFIG_IMPLEMENTATION))

# these are all found via the magic of vpath if not found locally
vpath %.pm $(SOECFG_DIR):$(SOECFG_CHIPCONFIG_DIR)
soecfgSrcFiles += Engines.pm
soecfgSrcFiles += Features.pm

# update src file list with hal .def files
soecfgSrcFiles += $(wildcard $(SOECFG_DIR)/haldefs/*.def)

# pull in templates list to supplement $(soecfgSrcFiles)
include $(SOECFG_DIR)/templates/Templates.mk
soecfgSrcFiles += $(addprefix $(SOECFG_DIR)/templates/, $(SOECFG_TEMPLATES))

# update src file list with sources .def files
include $(SOECFG_DIR)/srcdefs/Srcdefs.mk
soecfgSrcFiles += $(foreach srcfile,$(SOECFG_SRCDEFS),$(SOECFG_DIR)/srcdefs/$(srcfile).def)

# Ensure output directory(s) created before trying to generate soeconfig.mk.
# On some platforms, eg OSX, need soeconfig.h and soeconfig.mk in different dirs.
# Use 'echo keepme > $@' instead of 'touch' which might not be avail on some platforms.
soecfgOutputDirs = $(sort $(SOECFG_OUTPUTDIR)/ $(dir $(SOECFG_HEADER) $(SOECFG_MAKEFRAGMENT)))
soecfgKeeps      = $(addsuffix _keep,$(soecfgOutputDirs))
soecfgSrcFiles  += $(soecfgKeeps)

$(soecfgKeeps):
	$(SOECFG_MKDIR) $(dir $@)
	echo keepme > $@

# HACK for DJGPP cmdline len
# DJGPP without CYGWIN is *special* - perl cmdline len must be < 128
DJGPP_WITH_CYGWIN ?=
soecfgCmdLineLenHACK = 0
ifeq "$(SOECFG_BUILD_OS)-$(DJGPP_WITH_CYGWIN)" "djgpp-false"
  soecfgCmdLineLenHACK = 1
endif

# Extra options for chip-config needed by this makefile
soecfgExtraOptions :=

# NOTE: lwrrently managed ./soe-config script
# # lwrrently, just 1 platform for soe sw
# soecfgExtraOptions += --platform SOE

# NOTE: lwrrently managed ./soe-config script
# # and just 1 arch
# soecfgExtraOptions += --arch FALCON


### Run chip-config.pl to generate soeconfig.mk

# rmconfig invocation args
soecfgArgs = $(soecfgVerbosityFlag) \
             --mode soe-config \
             --config $(SOECFG_DIR)/soe-config.cfg \
             --profile $(SOECFG_PROFILE) \
             --source-root $(SOE_SW)/src \
             --output-dir $(SOECFG_OUTPUTDIR) \
             --gen-header $(SOECFG_HEADER) \
             --gen-makefile $(SOECFG_MAKEFRAGMENT) \
             $(SOECFG_OPTIONS) \
             $(soecfgExtraOptions)

# generate soe-config.mk
$(SOECFG_MAKEFRAGMENT): $(soecfgSrcFiles)
ifeq ($(soecfgCmdLineLenHACK), 1)
	$(SOECFG_Q)echo $(soecfgArgs)  > @soecfg.cmd
	$(SOECFG_Q)$(SOECFG_PERL) -I$(SOECFG_DIR) $(SOECFG_CHIPCONFIG_DIR)/chip-config.pl @soecfg.cmd
	$(RM) @soecfg.cmd
else
	$(SOECFG_Q)$(SOECFG_PERL) -I$(SOECFG_DIR) $(SOECFG_CHIPCONFIG_DIR)/chip-config.pl $(soecfgArgs)
endif

# soe-config.h is generated as a side-effect of generating soe-config.mk
$(SOECFG_HEADER): $(SOECFG_MAKEFRAGMENT)

# g_sources.mk is generated as a side-effect of generating soe-config.mk
$(SOECFG_OUTPUTDIR)/g_sources.mk: $(SOECFG_MAKEFRAGMENT)

# test targets
.PHONY: _soe-config_all _soe-config_clean
_soe-config_all: $(SOECFG_MAKEFRAGMENT)
	@true

_soe-config_clean:
	$(RM) $(SOECFG_CLEAN)

# Don't run rmconfig on 'make clean' if there is only 1 target in MAKECMDGOALS
_tmpCleanTargets = clean clobber _soe-config_clean
MAKECMDGOALS ?=
ifneq (,$(filter $(_tmpCleanTargets), $(MAKECMDGOALS)))
  # skip only if no other targets in goal list
  ifeq (,$(filter-out $(_tmpCleanTargets), $(MAKECMDGOALS)))
    SOECFG_RUN := 0
  endif
endif

# Finally, include the generated makefile which reflects SOECFG_PROFILE.
# This 'include' triggers the chip-config.pl run.
ifeq (1,$(SOECFG_RUN))
  -include $(SOECFG_MAKEFRAGMENT)
endif
