#
# uproc/fbflcn/config/makefile.mk
#
# Run chip-config.pl to build fbfalcon-config.h and fbfalcon-config.mk.
#
# *** Used by *all* builds that are rmconfig aware
#
# Each platform build will set some FBFALCONCFG_* variables, then 'include' this file.
#
# Makefiles that 'include' this file:
#
#  FBFALCON src:                    //sw/.../uproc/fbflcn/src/makefile
#      $RESMAN_ROOT/../../uproc/fbflcn/src/makefile
#

# Default target, only used when testing this makefile
.PHONY: _fbfalcon-config_default
_fbfalcon-config_default: _fbfalcon-config_all

# input vars, provide some typical defaults
FBFALCONCFG_PROFILE                ?= fbfalcon-v0200
FBFALCONCFG_RESMAN_ROOT            ?= ../../../drivers/resman
FBFALCONCFG_FBFALCONSW_ROOT        ?= ../../../uproc/fbflcn/src
FBFALCONCFG_DIR                    ?= $(FBFALCONCFG_FBFALCONSW_ROOT)/../config
FBFALCONCFG_CHIPCONFIG_DIR         ?= ../../../drivers/common/chip-config
FBFALCONCFG_OUTPUTDIR              ?= $(FBFALCONCFG_DIR)
FBFALCONCFG_VERBOSE                ?=
FBFALCONCFG_HEADER                 ?= $(FBFALCONCFG_OUTPUTDIR)/fbfalcon-config.h
FBFALCONCFG_MAKEFRAGMENT           ?= $(FBFALCONCFG_OUTPUTDIR)/fbfalcon-config.mk

FBFALCONCFG_OPTIONS                ?=
FBFALCONCFG_DEFINES                ?=

FBFALCONCFG_BUILD_OS               ?= unknown
FBFALCONCFG_TARGET_PLATFORM        ?= $(RMCFG_BUILD_OS)
FBFALCONCFG_TARGET_ARCH            ?= unknown

# FBFALCONCFG_RUN must be an immediate mode var since it controls an 'include' below
FBFALCONCFG_RUN                    ?= 1
FBFALCONCFG_RUN                    := $(FBFALCONCFG_RUN)

FBFALCONCFG_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
FBFALCONCFG_H2PH          ?= $(FBFALCONCFG_PERL) $(FBFALCONCFG_DIR)/util/h2ph.pl
FBFALCONCFG_MKDIR         ?= $(if $(findstring undefined,$(origin MKDIR)), mkdir -p, $(MKDIR))
FBFALCONCFG_Q             ?=


# Files we create that should be deleted by 'make clean'.
# May be used by "calling" makefile.
FBFALCONCFG_CLEAN          =
FBFALCONCFG_CLEAN         += $(FBFALCONCFG_HEADER) $(FBFALCONCFG_MAKEFRAGMENT)
FBFALCONCFG_CLEAN         += $(FBFALCONCFG_OUTPUTDIR)/g_*.h
FBFALCONCFG_CLEAN         += $(FBFALCONCFG_OUTPUTDIR)/halchk.log
FBFALCONCFG_CLEAN         += $(FBFALCONCFG_OUTPUTDIR)/halinfo.log

# table to colwert FBFALCONCFG_VERBOSE into an option for chip-config.pl
_fbfalconcfgVerbosity_quiet       = --quiet
_fbfalconcfgVerbosity_@           = --quiet
_fbfalconcfgVerbosity_default     =
_fbfalconcfgVerbosity_verbose     = --verbose
_fbfalconcfgVerbosity_veryverbose = --veryverbose
_fbfalconcfgVerbosity_            = $(_fbfalconcfgVerbosity_default)
fbfalconcfgVerbosityFlag := $(_fbfalconcfgVerbosity_$(FBFALCONCFG_VERBOSE))

# Files that implement chip-config.
# Assumes these vpath settings

fbfalconcfgSrcFiles :=
fbfalconcfgSrcFiles += $(FBFALCONCFG_DIR)/fbfalcon-config.cfg
fbfalconcfgSrcFiles += $(FBFALCONCFG_DIR)/makefile.mk

# pull in implementations list of chip-config to supplement $(fbfalconcfgSrcFiles)
include $(FBFALCONCFG_CHIPCONFIG_DIR)/Implementation.mk
fbfalconcfgSrcFiles += $(addprefix $(FBFALCONCFG_CHIPCONFIG_DIR)/, $(CHIPCONFIG_IMPLEMENTATION))

# these are all found via the magic of vpath if not found locally
vpath %.pm $(FBFALCONCFG_DIR):$(FBFALCONCFG_CHIPCONFIG_DIR)
fbfalconcfgSrcFiles += Engines.pm
fbfalconcfgSrcFiles += Features.pm

# update src file list with hal .def files
fbfalconcfgSrcFiles += $(wildcard $(FBFALCONCFG_DIR)/haldefs/*.def)

# pull in templates list to supplement $(fbfalconcfgSrcFiles)
include $(FBFALCONCFG_DIR)/templates/Templates.mk
fbfalconcfgSrcFiles += $(addprefix $(FBFALCONCFG_DIR)/templates/, $(FBFALCONCFG_TEMPLATES))

# update src file list with sources .def files
include $(FBFALCONCFG_DIR)/srcdefs/Srcdefs.mk
fbfalconcfgSrcFiles += $(foreach srcfile,$(FBFALCONCFG_SRCDEFS),$(FBFALCONCFG_DIR)/srcdefs/$(srcfile).def)

# Ensure output directory(s) created before trying to generate fbfalconconfig.mk.
# On some platforms, eg OSX, need fbfalconconfig.h and fbfalconconfig.mk in different dirs.
# Use 'echo keepme > $@' instead of 'touch' which might not be avail on some platforms.
fbfalconcfgOutputDirs = $(sort $(FBFALCONCFG_OUTPUTDIR)/ $(dir $(FBFALCONCFG_HEADER) $(FBFALCONCFG_MAKEFRAGMENT)))
fbfalconcfgKeeps      = $(addsuffix _keep,$(fbfalconcfgOutputDirs))
fbfalconcfgSrcFiles  += $(fbfalconcfgKeeps)

$(fbfalconcfgKeeps):
	$(FBFALCONCFG_MKDIR) $(dir $@)
	echo keepme > $@

# HACK for DJGPP cmdline len
# DJGPP without CYGWIN is *special* - perl cmdline len must be < 128
DJGPP_WITH_CYGWIN ?=
fbfalconcfgCmdLineLenHACK = 0
ifeq "$(FBFALCONCFG_BUILD_OS)-$(DJGPP_WITH_CYGWIN)" "djgpp-false"
  fbfalconcfgCmdLineLenHACK = 1
endif

# Extra options for chip-config needed by this makefile
fbfalconcfgExtraOptions :=

# NOTE: lwrrently managed ./fbfalcon-config script
# # lwrrently, just 1 platform for fbfalcon sw
# fbfalconcfgExtraOptions += --platform FBFALCON

# NOTE: lwrrently managed ./fbfalcon-config script
# # and just 1 arch
# fbfalconcfgExtraOptions += --arch FALCON


### Run chip-config.pl to generate fbfalconconfig.mk

# rmconfig invocation args
fbfalconcfgArgs = $(fbfalconcfgVerbosityFlag) \
             --mode fbfalcon-config \
             --config $(FBFALCONCFG_DIR)/fbfalcon-config.cfg \
             --profile $(FBFALCONCFG_PROFILE) \
             --source-root $(FBFALCON_SW)/src \
             --output-dir $(FBFALCONCFG_OUTPUTDIR) \
             --gen-header $(FBFALCONCFG_HEADER) \
             --gen-makefile $(FBFALCONCFG_MAKEFRAGMENT) \
             --no-halgen-share-similar \
             --no-halgen-share \
             $(FBFALCONCFG_OPTIONS) \
             $(fbfalconcfgExtraOptions)

# generate fbfalcon-config.mk
$(FBFALCONCFG_MAKEFRAGMENT): $(fbfalconcfgSrcFiles)
ifeq ($(fbfalconcfgCmdLineLenHACK), 1)
	$(FBFALCONCFG_Q)echo $(fbfalconcfgArgs)  > @fbfalconcfg.cmd
	$(FBFALCONCFG_Q)$(FBFALCONCFG_PERL) -I$(FBFALCONCFG_DIR) $(FBFALCONCFG_CHIPCONFIG_DIR)/chip-config.pl @fbfalconcfg.cmd
	$(RM) @fbfalconcfg.cmd
else
	$(FBFALCONCFG_Q)$(FBFALCONCFG_PERL) -I$(FBFALCONCFG_DIR) $(FBFALCONCFG_CHIPCONFIG_DIR)/chip-config.pl $(fbfalconcfgArgs)
endif

# fbfalcon-config.h is generated as a side-effect of generating fbfalcon-config.mk
$(FBFALCONCFG_HEADER): $(FBFALCONCFG_MAKEFRAGMENT)

# g_sources.mk is generated as a side-effect of generating fbfalcon-config.mk
$(FBFALCONCFG_OUTPUTDIR)/g_sources.mk: $(FBFALCONCFG_MAKEFRAGMENT)

# test targets
.PHONY: _fbfalcon-config_all _fbfalcon-config_clean
_fbfalcon-config_all: $(FBFALCONCFG_MAKEFRAGMENT)
	@true

_fbfalcon-config_clean:
	$(RM) $(FBFALCONCFG_CLEAN)

# Don't run rmconfig on 'make clean' if there is only 1 target in MAKECMDGOALS
_tmpCleanTargets = clean clobber _fbfalcon-config_clean
MAKECMDGOALS ?=
ifneq (,$(filter $(_tmpCleanTargets), $(MAKECMDGOALS)))
  # skip only if no other targets in goal list
  ifeq (,$(filter-out $(_tmpCleanTargets), $(MAKECMDGOALS)))
    FBFALCONCFG_RUN := 0
  endif
endif

# Finally, include the generated makefile which reflects FBFALCONCFG_PROFILE.
# This 'include' triggers the chip-config.pl run.
ifeq (1,$(FBFALCONCFG_RUN))
  -include $(FBFALCONCFG_MAKEFRAGMENT)
endif
