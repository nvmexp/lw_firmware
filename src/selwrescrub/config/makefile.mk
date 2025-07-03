#
# uproc/selwrescrub/config/makefile.mk
#
# Run chip-config.pl to build selwrescrub-config.h and selwrescrub-config.mk.
#
# *** Used by *all* builds that are rmconfig aware
#
# Each platform build will set some SELWRESCRUBCFG_* variables, then 'include' this file.
#
# Makefiles that 'include' this file:
#
#  SELWRESCRUB product app:                    //sw/.../uproc/selwrescrub/src/makefile.lwmk
#      $RESMAN_ROOT/../../uproc/selwrescrub/src/makefile.lwmk
#

# Default target, only used when testing this makefile
.PHONY: _selwrescrub-config_default
_selwrescrub-config_default: _selwrescrub-config_all

# input vars, provide some typical defaults
SELWRESCRUBCFG_PROFILE                ?= selwrescrub_lwdec-gp10x
SELWRESCRUBCFG_RESMAN_ROOT            ?= ../../../drivers/resman
SELWRESCRUBCFG_SELWRESCRUBSW_ROOT     ?= ../src/
SELWRESCRUBCFG_DIR                    ?= $(SELWRESCRUBCFG_SELWRESCRUBSW_ROOT)/../config
SELWRESCRUBCFG_CHIPCONFIG_DIR         ?= ../../../drivers/common/chip-config
SELWRESCRUBCFG_OUTPUTDIR              ?= $(SELWRESCRUBCFG_DIR)
SELWRESCRUBCFG_VERBOSE                ?= 
SELWRESCRUBCFG_HEADER                 ?= $(SELWRESCRUBCFG_OUTPUTDIR)/selwrescrub-config.h
SELWRESCRUBCFG_MAKEFRAGMENT           ?= $(SELWRESCRUBCFG_OUTPUTDIR)/selwrescrub-config.mk

SELWRESCRUBCFG_OPTIONS                ?=
SELWRESCRUBCFG_DEFINES                ?= 

SELWRESCRUBCFG_BUILD_OS               ?= unknown
SELWRESCRUBCFG_TARGET_PLATFORM        ?= $(RMCFG_BUILD_OS)
SELWRESCRUBCFG_TARGET_ARCH            ?= unknown

# SELWRESCRUBCFG_RUN must be an immediate mode var since it controls an 'include' below
SELWRESCRUBCFG_RUN                    ?= 1
SELWRESCRUBCFG_RUN                    := $(SELWRESCRUBCFG_RUN)

SELWRESCRUBCFG_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
SELWRESCRUBCFG_H2PH          ?= $(SELWRESCRUBCFG_PERL) $(SELWRESCRUBCFG_DIR)/util/h2ph.pl
SELWRESCRUBCFG_MKDIR         ?= $(if $(findstring undefined,$(origin MKDIR)), mkdir -p, $(MKDIR))
SELWRESCRUBCFG_Q             ?=


# Files we create that should be deleted by 'make clean'.
# May be used by "calling" makefile.
SELWRESCRUBCFG_CLEAN          =
SELWRESCRUBCFG_CLEAN         += $(SELWRESCRUBCFG_HEADER) $(SELWRESCRUBCFG_MAKEFRAGMENT)
SELWRESCRUBCFG_CLEAN         += $(SELWRESCRUBCFG_OUTPUTDIR)/g_*.h
SELWRESCRUBCFG_CLEAN         += $(SELWRESCRUBCFG_OUTPUTDIR)/halchk.log
SELWRESCRUBCFG_CLEAN         += $(SELWRESCRUBCFG_OUTPUTDIR)/halinfo.log

# table to colwert SELWRESCRUBCFG_VERBOSE into an option for chip-config.pl
_SELWRESCRUBCFGVerbosity_quiet       = --quiet
_SELWRESCRUBCFGVerbosity_@           = --quiet
_SELWRESCRUBCFGVerbosity_default     =
_SELWRESCRUBCFGVerbosity_verbose     = --verbose
_SELWRESCRUBCFGVerbosity_veryverbose = --veryverbose
_SELWRESCRUBCFGVerbosity_            = $(_SELWRESCRUBCFGVerbosity_default)
SELWRESCRUBCFGVerbosityFlag := $(_SELWRESCRUBCFGVerbosity_$(SELWRESCRUBCFG_VERBOSE))

# Files that implement chip-config.
# Assumes these vpath settings

SELWRESCRUBCFGSrcFiles := 
SELWRESCRUBCFGSrcFiles += $(SELWRESCRUBCFG_DIR)/selwrescrub-config.cfg
SELWRESCRUBCFGSrcFiles += $(SELWRESCRUBCFG_DIR)/makefile.mk

# pull in implementations list of chip-config to supplement $(SELWRESCRUBCFGSrcFiles)
include $(SELWRESCRUBCFG_CHIPCONFIG_DIR)/Implementation.mk
SELWRESCRUBCFGSrcFiles += $(addprefix $(SELWRESCRUBCFG_CHIPCONFIG_DIR)/, $(CHIPCONFIG_IMPLEMENTATION))

# these are all found via the magic of vpath if not found locally
vpath %.pm $(SELWRESCRUBCFG_DIR):$(SELWRESCRUBCFG_CHIPCONFIG_DIR)
SELWRESCRUBCFGSrcFiles += Engines.pm
SELWRESCRUBCFGSrcFiles += Features.pm

# pull in haldef list to define $(HALDEFS)
include $(SELWRESCRUBCFG_DIR)/haldefs/Haldefs.mk
SELWRESCRUBCFGSrcFiles += $(foreach hd,$(SELWRESCRUBCFG_HALDEFS),$(SELWRESCRUBCFG_DIR)/haldefs/$(hd).def)

# pull in templates list to supplement $(SELWRESCRUBCFGSrcFiles)
include $(SELWRESCRUBCFG_DIR)/templates/Templates.mk
SELWRESCRUBCFGSrcFiles += $(addprefix $(SELWRESCRUBCFG_DIR)/templates/, $(SELWRESCRUBCFG_TEMPLATES))

# update src file list with sources .def files
include $(SELWRESCRUBCFG_DIR)/srcdefs/Srcdefs.mk
SELWRESCRUBCFGSrcFiles += $(foreach srcfile,$(SELWRESCRUBCFG_SRCDEFS),$(SELWRESCRUBCFG_DIR)/srcdefs/$(srcfile).def)

# Ensure output directory(s) created before trying to generate selwrescrubconfig.mk.
# On some platforms, eg OSX, need selwrescrubconfig.h and selwrescrubconfig.mk in different dirs.
# Use 'echo keepme > $@' instead of 'touch' which might not be avail on some platforms.
SELWRESCRUBCFGOutputDirs = $(sort $(SELWRESCRUBCFG_OUTPUTDIR)/ $(dir $(SELWRESCRUBCFG_HEADER) $(SELWRESCRUBCFG_MAKEFRAGMENT)))
SELWRESCRUBCFGKeeps      = $(addsuffix _keep,$(SELWRESCRUBCFGOutputDirs))
SELWRESCRUBCFGSrcFiles  += $(SELWRESCRUBCFGKeeps)

$(SELWRESCRUBCFGKeeps):
	$(SELWRESCRUBCFG_MKDIR) $(dir $@)
	echo keepme > $@

# HACK for DJGPP cmdline len
# DJGPP without CYGWIN is *special* - perl cmdline len must be < 128
DJGPP_WITH_CYGWIN ?=
SELWRESCRUBCFGCmdLineLenHACK = 0
ifeq "$(SELWRESCRUBCFG_BUILD_OS)-$(DJGPP_WITH_CYGWIN)" "djgpp-false"
  SELWRESCRUBCFGCmdLineLenHACK = 1
endif

# Extra options for chip-config needed by this makefile
SELWRESCRUBCFGExtraOptions :=

# NOTE: lwrrently managed ./selwrescrub-config script
# # lwrrently, just 1 platform for selwrescrub sw
# SELWRESCRUBCFGExtraOptions += --platform SELWRESCRUB

# NOTE: lwrrently managed ./selwrescrub-config script
# # and just 1 arch
# SELWRESCRUBCFGExtraOptions += --arch FALCON


### Run chip-config.pl to generate selwrescrubconfig.mk

# rmconfig invocation args
SELWRESCRUBCFGArgs = $(SELWRESCRUBCFGVerbosityFlag) \
             --mode selwrescrub-config \
             --config $(SELWRESCRUBCFG_DIR)/selwrescrub-config.cfg \
             --profile $(SELWRESCRUBCFG_PROFILE) \
             --source-root $(SELWRESCRUBSW)/src \
             --output-dir $(SELWRESCRUBCFG_OUTPUTDIR) \
             --gen-header $(SELWRESCRUBCFG_HEADER) \
             --gen-makefile $(SELWRESCRUBCFG_MAKEFRAGMENT) \
             $(SELWRESCRUBCFG_OPTIONS) \
             $(SELWRESCRUBCFGExtraOptions)

# generate selwrescrub-config.mk
$(SELWRESCRUBCFG_MAKEFRAGMENT): $(SELWRESCRUBCFGSrcFiles)
ifeq ($(SELWRESCRUBCFGCmdLineLenHACK), 1)
	$(SELWRESCRUBCFG_Q)echo $(SELWRESCRUBCFGArgs)  > @SELWRESCRUBCFG.cmd
	$(SELWRESCRUBCFG_Q)$(SELWRESCRUBCFG_PERL) -I$(SELWRESCRUBCFG_DIR) $(SELWRESCRUBCFG_CHIPCONFIG_DIR)/chip-config.pl @SELWRESCRUBCFG.cmd
	$(RM) @SELWRESCRUBCFG.cmd
else
	$(SELWRESCRUBCFG_Q)$(SELWRESCRUBCFG_PERL) -I$(SELWRESCRUBCFG_DIR) $(SELWRESCRUBCFG_CHIPCONFIG_DIR)/chip-config.pl $(SELWRESCRUBCFGArgs)
endif

# selwrescrub-config.h is generated as a side-effect of generating selwrescrub-config.mk
$(SELWRESCRUBCFG_HEADER): $(SELWRESCRUBCFG_MAKEFRAGMENT)

# g_sources.mk is generated as a side-effect of generating selwrescrub-config.mk
$(SELWRESCRUBCFG_OUTPUTDIR)/g_sources.mk: $(SELWRESCRUBCFG_MAKEFRAGMENT)

# test targets
.PHONY: _selwrescrub-config_all _selwrescrub-config_clean
_selwrescrub-config_all: $(SELWRESCRUBCFG_MAKEFRAGMENT)
	@true

_selwrescrub-config_clean:
	$(RM) $(SELWRESCRUBCFG_CLEAN)

# Don't run rmconfig on 'make clean' if there is only 1 target in MAKECMDGOALS
_tmpCleanTargets = clean clobber _selwrescrub-config_clean
MAKECMDGOALS ?=
ifneq (,$(filter $(_tmpCleanTargets), $(MAKECMDGOALS)))
  # skip only if no other targets in goal list
  ifeq (,$(filter-out $(_tmpCleanTargets), $(MAKECMDGOALS)))
    SELWRESCRUBCFG_RUN := 0
  endif
endif

# Finally, include the generated makefile which reflects SELWRESCRUBCFG_PROFILE.
# This 'include' triggers the chip-config.pl run.
ifeq (1,$(SELWRESCRUBCFG_RUN))
  -include $(SELWRESCRUBCFG_MAKEFRAGMENT)
endif
