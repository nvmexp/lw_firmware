#
# uproc/acr_riscv/config/makefile.mk
#
# Run chip-config.pl to build acr-config.h and acr-config.mk.
#
# *** Used by *all* builds that are rmconfig aware
#
# Each platform build will set some ACRCFG_* variables, then 'include' this file.
#
# Makefiles that 'include' this file:
#
#  ACR product app:                    //sw/.../uproc/acr_riscv/src/makefile.lwmk
#      $RESMAN_ROOT/../../uproc/acr_riscv/src/makefile.lwmk
#

# Default target, only used when testing this makefile
.PHONY: _acr-config_default
_acr-config_default: _acr-config_all

# input vars, provide some typical defaults
ACRCFG_PROFILE                ?= acr_pmu-gm20x_load
ACRCFG_RESMAN_ROOT            ?= ../../../drivers/resman
ACRCFG_ACRSW_ROOT             ?= ../src/
ACRCFG_DIR                    ?= $(ACRCFG_ACRSW_ROOT)/../config
ACRCFG_CHIPCONFIG_DIR         ?= ../../../drivers/common/chip-config
ACRCFG_OUTPUTDIR              ?= $(ACRCFG_DIR)
ACRCFG_VERBOSE                ?= 
ACRCFG_HEADER                 ?= $(ACRCFG_OUTPUTDIR)/acr-config.h
ACRCFG_MAKEFRAGMENT           ?= $(ACRCFG_OUTPUTDIR)/acr-config.mk

ACRCFG_OPTIONS                ?=
ACRCFG_DEFINES                ?= 

ACRCFG_BUILD_OS               ?= unknown
ACRCFG_TARGET_PLATFORM        ?= $(RMCFG_BUILD_OS)
ACRCFG_TARGET_ARCH            ?= unknown

# ACRCFG_RUN must be an immediate mode var since it controls an 'include' below
ACRCFG_RUN                    ?= 1
ACRCFG_RUN                    := $(ACRCFG_RUN)

ACRCFG_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
ACRCFG_H2PH          ?= $(ACRCFG_PERL) $(ACRCFG_DIR)/util/h2ph.pl
ACRCFG_MKDIR         ?= $(if $(findstring undefined,$(origin MKDIR)), mkdir -p, $(MKDIR))
ACRCFG_Q             ?=


# Files we create that should be deleted by 'make clean'.
# May be used by "calling" makefile.
ACRCFG_CLEAN          =
ACRCFG_CLEAN         += $(ACRCFG_HEADER) $(ACRCFG_MAKEFRAGMENT)
ACRCFG_CLEAN         += $(ACRCFG_OUTPUTDIR)/g_*.h
ACRCFG_CLEAN         += $(ACRCFG_OUTPUTDIR)/halchk.log
ACRCFG_CLEAN         += $(ACRCFG_OUTPUTDIR)/halinfo.log

# table to colwert ACRCFG_VERBOSE into an option for chip-config.pl
_acrcfgVerbosity_quiet       = --quiet
_acrcfgVerbosity_@           = --quiet
_acrcfgVerbosity_default     =
_acrcfgVerbosity_verbose     = --verbose
_acrcfgVerbosity_veryverbose = --veryverbose
_acrcfgVerbosity_            = $(_acrcfgVerbosity_default)
acrcfgVerbosityFlag := $(_acrcfgVerbosity_$(ACRCFG_VERBOSE))

# Files that implement chip-config.
# Assumes these vpath settings

acrcfgSrcFiles := 
acrcfgSrcFiles += $(ACRCFG_DIR)/acr-config.cfg
acrcfgSrcFiles += $(ACRCFG_DIR)/makefile.mk

# pull in implementations list of chip-config to supplement $(acrcfgSrcFiles)
include $(ACRCFG_CHIPCONFIG_DIR)/Implementation.mk
acrcfgSrcFiles += $(addprefix $(ACRCFG_CHIPCONFIG_DIR)/, $(CHIPCONFIG_IMPLEMENTATION))

# these are all found via the magic of vpath if not found locally
vpath %.pm $(ACRCFG_DIR):$(ACRCFG_CHIPCONFIG_DIR)
acrcfgSrcFiles += Engines.pm
acrcfgSrcFiles += Features.pm

# pull in haldef list to define $(HALDEFS)
include $(ACRCFG_DIR)/haldefs/Haldefs.mk
acrcfgSrcFiles += $(foreach hd,$(ACRCFG_HALDEFS),$(ACRCFG_DIR)/haldefs/$(hd).def)

# pull in templates list to supplement $(acrcfgSrcFiles)
include $(ACRCFG_DIR)/templates/Templates.mk
acrcfgSrcFiles += $(addprefix $(ACRCFG_DIR)/templates/, $(ACRCFG_TEMPLATES))

# update src file list with sources .def files
include $(ACRCFG_DIR)/srcdefs/Srcdefs.mk
acrcfgSrcFiles += $(foreach srcfile,$(ACRCFG_SRCDEFS),$(ACRCFG_DIR)/srcdefs/$(srcfile).def)

# Ensure output directory(s) created before trying to generate acrconfig.mk.
# On some platforms, eg OSX, need acrconfig.h and acrconfig.mk in different dirs.
# Use 'echo keepme > $@' instead of 'touch' which might not be avail on some platforms.
acrcfgOutputDirs = $(sort $(ACRCFG_OUTPUTDIR)/ $(dir $(ACRCFG_HEADER) $(ACRCFG_MAKEFRAGMENT)))
acrcfgKeeps      = $(addsuffix _keep,$(acrcfgOutputDirs))
acrcfgSrcFiles  += $(acrcfgKeeps)

$(acrcfgKeeps):
	$(ACRCFG_MKDIR) $(dir $@)
	echo keepme > $@

# HACK for DJGPP cmdline len
# DJGPP without CYGWIN is *special* - perl cmdline len must be < 128
DJGPP_WITH_CYGWIN ?=
acrcfgCmdLineLenHACK = 0
ifeq "$(ACRCFG_BUILD_OS)-$(DJGPP_WITH_CYGWIN)" "djgpp-false"
  acrcfgCmdLineLenHACK = 1
endif

# Extra options for chip-config needed by this makefile
acrcfgExtraOptions :=

# NOTE: lwrrently managed ./acr-config script
# # lwrrently, just 1 platform for acr sw
# acrcfgExtraOptions += --platform ACR

# NOTE: lwrrently managed ./acr-config script
# # and just 1 arch
# acrcfgExtraOptions += --arch FALCON


### Run chip-config.pl to generate acrconfig.mk

# rmconfig invocation args
acrcfgArgs = $(acrcfgVerbosityFlag) \
             --mode acr-config \
             --config $(ACRCFG_DIR)/acr-config.cfg \
             --profile $(ACRCFG_PROFILE) \
             --source-root $(ACR_SW)/src \
             --output-dir $(ACRCFG_OUTPUTDIR) \
             --gen-header $(ACRCFG_HEADER) \
             --gen-makefile $(ACRCFG_MAKEFRAGMENT) \
             $(ACRCFG_OPTIONS) \
             $(acrcfgExtraOptions)

# generate acr-config.mk
$(ACRCFG_MAKEFRAGMENT): $(acrcfgSrcFiles)
ifeq ($(acrcfgCmdLineLenHACK), 1)
	$(ACRCFG_Q)echo $(acrcfgArgs)  > @acrcfg.cmd
	$(ACRCFG_Q)$(ACRCFG_PERL) -I$(ACRCFG_DIR) $(ACRCFG_CHIPCONFIG_DIR)/chip-config.pl @acrcfg.cmd
	$(RM) @acrcfg.cmd
else
	$(ACRCFG_Q)$(ACRCFG_PERL) -I$(ACRCFG_DIR) $(ACRCFG_CHIPCONFIG_DIR)/chip-config.pl $(acrcfgArgs)
endif

# acr-config.h is generated as a side-effect of generating acr-config.mk
$(ACRCFG_HEADER): $(ACRCFG_MAKEFRAGMENT)

# g_sources.mk is generated as a side-effect of generating acr-config.mk
$(ACRCFG_OUTPUTDIR)/g_sources.mk: $(ACRCFG_MAKEFRAGMENT)

# test targets
.PHONY: _acr-config_all _acr-config_clean
_acr-config_all: $(ACRCFG_MAKEFRAGMENT)
	@true

_acr-config_clean:
	$(RM) $(ACRCFG_CLEAN)

# Don't run rmconfig on 'make clean' if there is only 1 target in MAKECMDGOALS
_tmpCleanTargets = clean clobber _acr-config_clean
MAKECMDGOALS ?=
ifneq (,$(filter $(_tmpCleanTargets), $(MAKECMDGOALS)))
  # skip only if no other targets in goal list
  ifeq (,$(filter-out $(_tmpCleanTargets), $(MAKECMDGOALS)))
    ACRCFG_RUN := 0
  endif
endif

# Finally, include the generated makefile which reflects ACRCFG_PROFILE.
# This 'include' triggers the chip-config.pl run.
ifeq (1,$(ACRCFG_RUN))
  -include $(ACRCFG_MAKEFRAGMENT)
endif
