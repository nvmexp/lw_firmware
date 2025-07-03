#
# uproc/sec2_riscv/config/makefile.mk
#
# Run chip-config.pl to build sec2-config.h and sec2-config.mk.
#
# *** Used by *all* builds that are rmconfig aware
#
# Each platform build will set some SEC2CFG_* variables, then 'include' this file.
#
# Makefiles that 'include' this file:
#
#  SEC2 product app:                    //sw/.../uproc/sec2_riscv/src/makefile.lwmk
#      $RESMAN_ROOT/../../uproc/sec2_riscv/src/makefile.lwmk
#

# Default target, only used when testing this makefile
.PHONY: _sec2-config_default
_sec2-config_default: _sec2-config_all

# input vars, provide some typical defaults
SEC2CFG_PROFILE                ?= sec2-gpus
SEC2CFG_RESMAN_ROOT            ?= ../../../drivers/resman
SEC2CFG_SEC2SW_ROOT            = ../src/
SEC2CFG_DIR                    ?= ../config
SEC2CFG_CHIPCONFIG_DIR         ?= ../../../drivers/common/chip-config
SEC2CFG_OUTPUTDIR              ?= $(SEC2CFG_DIR)
SEC2CFG_VERBOSE                ?= 
SEC2CFG_HEADER                 ?= $(SEC2CFG_OUTPUTDIR)/sec2-config.h
SEC2CFG_MAKEFRAGMENT           ?= $(SEC2CFG_OUTPUTDIR)/sec2-config.mk

SEC2CFG_OPTIONS                ?=
SEC2CFG_DEFINES                ?= 

SEC2CFG_BUILD_OS               ?= unknown
SEC2CFG_TARGET_PLATFORM        ?= $(RMCFG_BUILD_OS)
SEC2CFG_TARGET_ARCH            ?= unknown

# SEC2CFG_RUN must be an immediate mode var since it controls an 'include' below
SEC2CFG_RUN                    ?= 1
SEC2CFG_RUN                    := $(SEC2CFG_RUN)

SEC2CFG_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
SEC2CFG_H2PH          ?= $(SEC2CFG_PERL) $(SEC2CFG_DIR)/util/h2ph.pl
SEC2CFG_MKDIR         ?= $(if $(findstring undefined,$(origin MKDIR)), mkdir -p, $(MKDIR))
SEC2CFG_Q             ?=


# Files we create that should be deleted by 'make clean'.
# May be used by "calling" makefile.
SEC2CFG_CLEAN          =
SEC2CFG_CLEAN         += $(SEC2CFG_HEADER) $(SEC2CFG_MAKEFRAGMENT)
SEC2CFG_CLEAN         += $(SEC2CFG_OUTPUTDIR)/g_*.h
SEC2CFG_CLEAN         += $(SEC2CFG_OUTPUTDIR)/halchk.log
SEC2CFG_CLEAN         += $(SEC2CFG_OUTPUTDIR)/halinfo.log

# table to colwert SEC2CFG_VERBOSE into an option for chip-config.pl
_sec2cfgVerbosity_quiet       = --quiet
_sec2cfgVerbosity_@           = --quiet
_sec2cfgVerbosity_default     =
_sec2cfgVerbosity_verbose     = --verbose
_sec2cfgVerbosity_veryverbose = --veryverbose
_sec2cfgVerbosity_            = $(_sec2cfgVerbosity_default)
sec2cfgVerbosityFlag := $(_sec2cfgVerbosity_$(SEC2CFG_VERBOSE))

# Files that implement chip-config.
# Assumes these vpath settings

sec2cfgSrcFiles := 
sec2cfgSrcFiles += $(SEC2CFG_DIR)/sec2-config.cfg
sec2cfgSrcFiles += $(SEC2CFG_DIR)/makefile.mk

# pull in implementations list of chip-config to supplement $(sec2cfgSrcFiles)
include $(SEC2CFG_CHIPCONFIG_DIR)/Implementation.mk
sec2cfgSrcFiles += $(addprefix $(SEC2CFG_CHIPCONFIG_DIR)/, $(CHIPCONFIG_IMPLEMENTATION))

# these are all found via the magic of vpath if not found locally
vpath %.pm $(SEC2CFG_DIR):$(SEC2CFG_CHIPCONFIG_DIR)
sec2cfgSrcFiles += Engines.pm
sec2cfgSrcFiles += Features.pm

# update src file list with hal .def files
sec2cfgSrcFiles += $(wildcard $(SEC2CFG_DIR)/haldefs/*.def)

# pull in templates list to supplement $(sec2cfgSrcFiles)
include $(SEC2CFG_DIR)/templates/Templates.mk
sec2cfgSrcFiles += $(addprefix $(SEC2CFG_DIR)/templates/, $(SEC2CFG_TEMPLATES))

# update src file list with sources .def files
include $(SEC2CFG_DIR)/srcdefs/Srcdefs.mk
sec2cfgSrcFiles += $(foreach srcfile,$(SEC2CFG_SRCDEFS),$(SEC2CFG_DIR)/srcdefs/$(srcfile).def)

# Ensure output directory(s) created before trying to generate sec2config.mk.
# On some platforms, eg OSX, need sec2config.h and sec2config.mk in different dirs.
# Use 'echo keepme > $@' instead of 'touch' which might not be avail on some platforms.
sec2cfgOutputDirs = $(sort $(SEC2CFG_OUTPUTDIR)/ $(dir $(SEC2CFG_HEADER) $(SEC2CFG_MAKEFRAGMENT)))
sec2cfgKeeps      = $(addsuffix _keep,$(sec2cfgOutputDirs))
sec2cfgSrcFiles  += $(sec2cfgKeeps)

$(sec2cfgKeeps):
	$(SEC2CFG_MKDIR) $(dir $@)
	echo keepme > $@

# HACK for DJGPP cmdline len
# DJGPP without CYGWIN is *special* - perl cmdline len must be < 128
DJGPP_WITH_CYGWIN ?=
sec2cfgCmdLineLenHACK = 0
ifeq "$(SEC2CFG_BUILD_OS)-$(DJGPP_WITH_CYGWIN)" "djgpp-false"
  sec2cfgCmdLineLenHACK = 1
endif

# Extra options for chip-config needed by this makefile
sec2cfgExtraOptions :=

# NOTE: lwrrently managed ./sec2-config script
# # lwrrently, just 1 platform for sec2 sw
# sec2cfgExtraOptions += --platform SEC2

# NOTE: lwrrently managed ./sec2-config script
# # and just 1 arch
# sec2cfgExtraOptions += --arch FALCON


### Run chip-config.pl to generate sec2config.mk

# rmconfig invocation args
sec2cfgArgs = $(sec2cfgVerbosityFlag) \
             --mode sec2-config \
             --config $(SEC2CFG_DIR)/sec2-config.cfg \
             --profile $(SEC2CFG_PROFILE) \
             --source-root $(SEC2_SW)/src \
             --output-dir $(SEC2CFG_OUTPUTDIR) \
             --gen-header $(SEC2CFG_HEADER) \
             --gen-makefile $(SEC2CFG_MAKEFRAGMENT) \
             $(SEC2CFG_OPTIONS) \
             $(sec2cfgExtraOptions)

# generate sec2-config.mk
$(SEC2CFG_MAKEFRAGMENT): $(sec2cfgSrcFiles)
ifeq ($(sec2cfgCmdLineLenHACK), 1)
	$(SEC2CFG_Q)echo $(sec2cfgArgs)  > @sec2cfg.cmd
	$(SEC2CFG_Q)$(SEC2CFG_PERL) -I$(SEC2CFG_DIR) $(SEC2CFG_CHIPCONFIG_DIR)/chip-config.pl @sec2cfg.cmd
	$(RM) @sec2cfg.cmd
else
	$(SEC2CFG_Q)$(SEC2CFG_PERL) -I$(SEC2CFG_DIR) $(SEC2CFG_CHIPCONFIG_DIR)/chip-config.pl $(sec2cfgArgs)
endif

# sec2-config.h is generated as a side-effect of generating sec2-config.mk
$(SEC2CFG_HEADER): $(SEC2CFG_MAKEFRAGMENT)

# g_sources.mk is generated as a side-effect of generating sec2-config.mk
$(SEC2CFG_OUTPUTDIR)/g_sources.mk: $(SEC2CFG_MAKEFRAGMENT)

# test targets
.PHONY: _sec2-config_all _sec2-config_clean
_sec2-config_all: $(SEC2CFG_MAKEFRAGMENT)
	@true

_sec2-config_clean:
	$(RM) $(SEC2CFG_CLEAN)

# Don't run rmconfig on 'make clean' if there is only 1 target in MAKECMDGOALS
_tmpCleanTargets = clean clobber _sec2-config_clean
MAKECMDGOALS ?=
ifneq (,$(filter $(_tmpCleanTargets), $(MAKECMDGOALS)))
  # skip only if no other targets in goal list
  ifeq (,$(filter-out $(_tmpCleanTargets), $(MAKECMDGOALS)))
    SEC2CFG_RUN := 0
  endif
endif

# Finally, include the generated makefile which reflects SEC2CFG_PROFILE.
# This 'include' triggers the chip-config.pl run.
ifeq (1,$(SEC2CFG_RUN))
  -include $(SEC2CFG_MAKEFRAGMENT)
endif
