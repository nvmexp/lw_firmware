#
# uproc/booter/config/makefile.mk
#
# Run chip-config.pl to build booter-config.h and booter-config.mk.
#
# *** Used by *all* builds that are rmconfig aware
#
# Each platform build will set some BOOTERCFG_* variables, then 'include' this file.
#
# Makefiles that 'include' this file:
#
#  Booter product app:                    //sw/.../uproc/booter/src/makefile.lwmk
#      $RESMAN_ROOT/../../uproc/booter/src/makefile.lwmk
#

# Default target, only used when testing this makefile
.PHONY: _booter-config_default
_booter-config_default: _booter-config_all

# input vars, provide some typical defaults
BOOTERCFG_PROFILE                ?= booter-load-tu10x
BOOTERCFG_RESMAN_ROOT            ?= ../../../drivers/resman
BOOTERCFG_BOOTERSW_ROOT          ?= ../src/
BOOTERCFG_DIR                    ?= $(BOOTERCFG_BOOTERSW_ROOT)/../config
BOOTERCFG_CHIPCONFIG_DIR         ?= ../../../drivers/common/chip-config
BOOTERCFG_OUTPUTDIR              ?= $(BOOTERCFG_DIR)
BOOTERCFG_VERBOSE                ?= 
BOOTERCFG_HEADER                 ?= $(BOOTERCFG_OUTPUTDIR)/booter-config.h
BOOTERCFG_MAKEFRAGMENT           ?= $(BOOTERCFG_OUTPUTDIR)/booter-config.mk

BOOTERCFG_OPTIONS                ?=
BOOTERCFG_DEFINES                ?= 

BOOTERCFG_BUILD_OS               ?= unknown
BOOTERCFG_TARGET_PLATFORM        ?= $(RMCFG_BUILD_OS)
BOOTERCFG_TARGET_ARCH            ?= unknown

# BOOTERCFG_RUN must be an immediate mode var since it controls an 'include' below
BOOTERCFG_RUN                    ?= 1
BOOTERCFG_RUN                    := $(BOOTERCFG_RUN)

BOOTERCFG_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
BOOTERCFG_H2PH          ?= $(BOOTERCFG_PERL) $(BOOTERCFG_DIR)/util/h2ph.pl
BOOTERCFG_MKDIR         ?= $(if $(findstring undefined,$(origin MKDIR)), mkdir -p, $(MKDIR))
BOOTERCFG_Q             ?=


# Files we create that should be deleted by 'make clean'.
# May be used by "calling" makefile.
BOOTERCFG_CLEAN          =
BOOTERCFG_CLEAN         += $(BOOTERCFG_HEADER) $(BOOTERCFG_MAKEFRAGMENT)
BOOTERCFG_CLEAN         += $(BOOTERCFG_OUTPUTDIR)/g_*.h
BOOTERCFG_CLEAN         += $(BOOTERCFG_OUTPUTDIR)/halchk.log
BOOTERCFG_CLEAN         += $(BOOTERCFG_OUTPUTDIR)/halinfo.log

# table to colwert BOOTERCFG_VERBOSE into an option for chip-config.pl
_bootercfgVerbosity_quiet       = --quiet
_bootercfgVerbosity_@           = --quiet
_bootercfgVerbosity_default     =
_bootercfgVerbosity_verbose     = --verbose
_bootercfgVerbosity_veryverbose = --veryverbose
_bootercfgVerbosity_            = $(_bootercfgVerbosity_default)
bootercfgVerbosityFlag := $(_bootercfgVerbosity_$(BOOTERCFG_VERBOSE))

# Files that implement chip-config.
# Assumes these vpath settings

bootercfgSrcFiles := 
bootercfgSrcFiles += $(BOOTERCFG_DIR)/booter-config.cfg
bootercfgSrcFiles += $(BOOTERCFG_DIR)/makefile.mk

# pull in implementations list of chip-config to supplement $(bootercfgSrcFiles)
include $(BOOTERCFG_CHIPCONFIG_DIR)/Implementation.mk
bootercfgSrcFiles += $(addprefix $(BOOTERCFG_CHIPCONFIG_DIR)/, $(CHIPCONFIG_IMPLEMENTATION))

# these are all found via the magic of vpath if not found locally
vpath %.pm $(BOOTERCFG_DIR):$(BOOTERCFG_CHIPCONFIG_DIR)
bootercfgSrcFiles += Engines.pm
bootercfgSrcFiles += Features.pm

# pull in haldef list to define $(HALDEFS)
include $(BOOTERCFG_DIR)/haldefs/Haldefs.mk
bootercfgSrcFiles += $(foreach hd,$(BOOTERCFG_HALDEFS),$(BOOTERCFG_DIR)/haldefs/$(hd).def)

# pull in templates list to supplement $(bootercfgSrcFiles)
include $(BOOTERCFG_DIR)/templates/Templates.mk
bootercfgSrcFiles += $(addprefix $(BOOTERCFG_DIR)/templates/, $(BOOTERCFG_TEMPLATES))

# update src file list with sources .def files
include $(BOOTERCFG_DIR)/srcdefs/Srcdefs.mk
bootercfgSrcFiles += $(foreach srcfile,$(BOOTERCFG_SRCDEFS),$(BOOTERCFG_DIR)/srcdefs/$(srcfile).def)

# Ensure output directory(s) created before trying to generate booterconfig.mk.
# On some platforms, eg OSX, need booterconfig.h and booterconfig.mk in different dirs.
# Use 'echo keepme > $@' instead of 'touch' which might not be avail on some platforms.
bootercfgOutputDirs = $(sort $(BOOTERCFG_OUTPUTDIR)/ $(dir $(BOOTERCFG_HEADER) $(BOOTERCFG_MAKEFRAGMENT)))
bootercfgKeeps      = $(addsuffix _keep,$(bootercfgOutputDirs))
bootercfgSrcFiles  += $(bootercfgKeeps)

$(bootercfgKeeps):
	$(BOOTERCFG_MKDIR) $(dir $@)
	echo keepme > $@

# HACK for DJGPP cmdline len
# DJGPP without CYGWIN is *special* - perl cmdline len must be < 128
DJGPP_WITH_CYGWIN ?=
bootercfgCmdLineLenHACK = 0
ifeq "$(BOOTERCFG_BUILD_OS)-$(DJGPP_WITH_CYGWIN)" "djgpp-false"
  bootercfgCmdLineLenHACK = 1
endif

# Extra options for chip-config needed by this makefile
bootercfgExtraOptions :=

# NOTE: lwrrently managed ./booter-config script
# # lwrrently, just 1 platform for booter sw
# bootercfgExtraOptions += --platform BOOTER

# NOTE: lwrrently managed ./booter-config script
# # and just 1 arch
# bootercfgExtraOptions += --arch FALCON


### Run chip-config.pl to generate booterconfig.mk

# rmconfig invocation args
bootercfgArgs = $(bootercfgVerbosityFlag) \
             --mode booter-config \
             --config $(BOOTERCFG_DIR)/booter-config.cfg \
             --profile $(BOOTERCFG_PROFILE) \
             --source-root $(BOOTER_SW)/src \
             --output-dir $(BOOTERCFG_OUTPUTDIR) \
             --gen-header $(BOOTERCFG_HEADER) \
             --gen-makefile $(BOOTERCFG_MAKEFRAGMENT) \
             $(BOOTERCFG_OPTIONS) \
             $(bootercfgExtraOptions)

# generate booter-config.mk
$(BOOTERCFG_MAKEFRAGMENT): $(bootercfgSrcFiles)
ifeq ($(bootercfgCmdLineLenHACK), 1)
	$(BOOTERCFG_Q)echo $(bootercfgArgs)  > @bootercfg.cmd
	$(BOOTERCFG_Q)$(BOOTERCFG_PERL) -I$(BOOTERCFG_DIR) $(BOOTERCFG_CHIPCONFIG_DIR)/chip-config.pl @bootercfg.cmd
	$(RM) @bootercfg.cmd
else
	$(BOOTERCFG_Q)$(BOOTERCFG_PERL) -I$(BOOTERCFG_DIR) $(BOOTERCFG_CHIPCONFIG_DIR)/chip-config.pl $(bootercfgArgs)
endif

# booter-config.h is generated as a side-effect of generating booter-config.mk
$(BOOTERCFG_HEADER): $(BOOTERCFG_MAKEFRAGMENT)

# g_sources.mk is generated as a side-effect of generating booter-config.mk
$(BOOTERCFG_OUTPUTDIR)/g_sources.mk: $(BOOTERCFG_MAKEFRAGMENT)

# test targets
.PHONY: _booter-config_all _booter-config_clean
_booter-config_all: $(BOOTERCFG_MAKEFRAGMENT)
	@true

_booter-config_clean:
	$(RM) $(BOOTERCFG_CLEAN)

# Don't run rmconfig on 'make clean' if there is only 1 target in MAKECMDGOALS
_tmpCleanTargets = clean clobber _booter-config_clean
MAKECMDGOALS ?=
ifneq (,$(filter $(_tmpCleanTargets), $(MAKECMDGOALS)))
  # skip only if no other targets in goal list
  ifeq (,$(filter-out $(_tmpCleanTargets), $(MAKECMDGOALS)))
    BOOTERCFG_RUN := 0
  endif
endif

# Finally, include the generated makefile which reflects BOOTERCFG_PROFILE.
# This 'include' triggers the chip-config.pl run.
ifeq (1,$(BOOTERCFG_RUN))
  -include $(BOOTERCFG_MAKEFRAGMENT)
endif
