#
# uproc/gsp/config/makefile.mk
#
# Run chip-config.pl to build gsp-config.h and gsp-config.mk.
#
# *** Used by *all* builds that are rmconfig aware
#
# Each platform build will set some GSPCFG_* variables, then 'include' this file.
#
# Makefiles that 'include' this file:
#
#  GSP product app:                    //sw/.../uproc/gsp/src/makefile.lwmk
#      $RESMAN_ROOT/../../uproc/gsp/src/makefile.lwmk
#

# Default target, only used when testing this makefile
.PHONY: _gsp-config_default
_gsp-config_default: _gsp-config_all

# input vars, provide some typical defaults
GSPCFG_PROFILE                ?= gsp-gpus
GSPCFG_GSPSW_ROOT             ?= ../src/
GSPCFG_DIR                    ?= ../config
GSPCFG_CHIPCONFIG_DIR         ?= ../../../drivers/common/chip-config
GSPCFG_OUTPUTDIR              ?= $(GSPCFG_DIR)/_out
GSPCFG_VERBOSE                ?= 
GSPCFG_HEADER                 ?= $(GSPCFG_OUTPUTDIR)/gsp-config.h
GSPCFG_MAKEFRAGMENT           ?= $(GSPCFG_OUTPUTDIR)/gsp-config.mk

GSPCFG_BUILD_OS               ?= unknown
GSPCFG_TARGET_PLATFORM        ?= $(RMCFG_BUILD_OS)
GSPCFG_TARGET_ARCH            ?= unknown

# GSPCFG_RUN must be an immediate mode var since it controls an 'include' below
GSPCFG_RUN                    ?= 1
GSPCFG_RUN                    := $(GSPCFG_RUN)

GSPCFG_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
GSPCFG_MKDIR         ?= $(if $(findstring undefined,$(origin MKDIR)), mkdir -p, $(MKDIR))
GSPCFG_Q             ?=

GSPCFG_OPTIONS                ?=

ifeq ("$(SCHEDULER_ENABLED)", "true")
    GSPCFG_OPTIONS            += --enable GSPTASK_SCHEDULER
endif


# Files we create that should be deleted by 'make clean'.
# May be used by "calling" makefile.
GSPCFG_CLEAN          =
GSPCFG_CLEAN         += $(GSPCFG_HEADER) $(GSPCFG_MAKEFRAGMENT)
GSPCFG_CLEAN         += $(GSPCFG_OUTPUTDIR)/g_*.h

# table to colwert GSPCFG_VERBOSE into an option for chip-config.pl
_gspcfgVerbosity_quiet       = --quiet
_gspcfgVerbosity_@           = --quiet
_gspcfgVerbosity_default     =
_gspcfgVerbosity_verbose     = --verbose
_gspcfgVerbosity_veryverbose = --veryverbose
_gspcfgVerbosity_            = $(_gspcfgVerbosity_default)
gspcfgVerbosityFlag := $(_gspcfgVerbosity_$(GSPCFG_VERBOSE))

# Files that implement chip-config.
# Assumes these vpath settings

gspcfgSrcFiles := 
gspcfgSrcFiles += $(GSPCFG_DIR)/gsp-config.cfg
gspcfgSrcFiles += $(GSPCFG_DIR)/makefile.mk

# pull in implementations list of chip-config to supplement $(gspcfgSrcFiles)
include $(GSPCFG_CHIPCONFIG_DIR)/Implementation.mk
gspcfgSrcFiles += $(addprefix $(GSPCFG_CHIPCONFIG_DIR)/, $(CHIPCONFIG_IMPLEMENTATION))

# these are all found via the magic of vpath if not found locally
vpath %.pm $(GSPCFG_DIR):$(GSPCFG_CHIPCONFIG_DIR)
gspcfgSrcFiles += Engines.pm
gspcfgSrcFiles += Features.pm

# update src file list with hal .def files
gspcfgSrcFiles += $(wildcard $(GSPCFG_DIR)/haldefs/*.def)

# pull in templates list to supplement $(gspcfgSrcFiles)
include $(GSPCFG_DIR)/templates/Templates.mk
gspcfgSrcFiles += $(addprefix $(GSPCFG_DIR)/templates/, $(GSPCFG_TEMPLATES))

# update src file list with sources .def files
include $(GSPCFG_DIR)/srcdefs/Srcdefs.mk
gspcfgSrcFiles += $(foreach srcfile,$(GSPCFG_SRCDEFS),$(GSPCFG_DIR)/srcdefs/$(srcfile).def)

# Ensure output directory(s) created before trying to generate gspconfig.mk.
# On some platforms, eg OSX, need gspconfig.h and gspconfig.mk in different dirs.
# Use 'echo keepme > $@' instead of 'touch' which might not be avail on some platforms.
gspcfgOutputDirs = $(sort $(GSPCFG_OUTPUTDIR)/ $(dir $(GSPCFG_HEADER) $(GSPCFG_MAKEFRAGMENT)))
gspcfgKeeps      = $(addsuffix _keep,$(gspcfgOutputDirs))
gspcfgSrcFiles  += $(gspcfgKeeps)

$(gspcfgKeeps):
	$(GSPCFG_MKDIR) $(dir $@)
	echo keepme > $@

# HACK for DJGPP cmdline len
# DJGPP without CYGWIN is *special* - perl cmdline len must be < 128
DJGPP_WITH_CYGWIN ?=
gspcfgCmdLineLenHACK = 0
ifeq "$(GSPCFG_BUILD_OS)-$(DJGPP_WITH_CYGWIN)" "djgpp-false"
  gspcfgCmdLineLenHACK = 1
endif


### Run chip-config.pl to generate gspconfig.mk

# rmconfig invocation args
gspcfgArgs = $(gspcfgVerbosityFlag) \
             --mode gsp-config \
             --config $(GSPCFG_DIR)/gsp-config.cfg \
             --profile $(GSPCFG_PROFILE) \
             --source-root $(GSP_SW)/src \
             --output-dir $(GSPCFG_OUTPUTDIR) \
             --gen-header $(GSPCFG_HEADER) \
             --gen-makefile $(GSPCFG_MAKEFRAGMENT) \
             $(GSPCFG_OPTIONS)

# generate gsp-config.mk
$(GSPCFG_MAKEFRAGMENT): $(gspcfgSrcFiles)
ifeq ($(gspcfgCmdLineLenHACK), 1)
	$(GSPCFG_Q)echo $(gspcfgArgs)  > @gspcfg.cmd
	$(GSPCFG_Q)$(GSPCFG_PERL) -I$(GSPCFG_DIR) $(GSPCFG_CHIPCONFIG_DIR)/chip-config.pl @gspcfg.cmd
	$(RM) @gspcfg.cmd
else
	$(GSPCFG_Q)$(GSPCFG_PERL) -I$(GSPCFG_DIR) $(GSPCFG_CHIPCONFIG_DIR)/chip-config.pl $(gspcfgArgs)
endif

# gsp-config.h is generated as a side-effect of generating gsp-config.mk
$(GSPCFG_HEADER): $(GSPCFG_MAKEFRAGMENT)

# g_sources.mk is generated as a side-effect of generating gsp-config.mk
$(GSPCFG_OUTPUTDIR)/g_sources.mk: $(GSPCFG_MAKEFRAGMENT)

# test targets
.PHONY: _gsp-config_all _gsp-config_clean
_gsp-config_all: $(GSPCFG_MAKEFRAGMENT)
	@true

_gsp-config_clean:
	$(RM) $(GSPCFG_CLEAN)

# Don't run rmconfig on 'make clean' if there is only 1 target in MAKECMDGOALS
_tmpCleanTargets = clean clobber _gsp-config_clean
MAKECMDGOALS ?=
ifneq (,$(filter $(_tmpCleanTargets), $(MAKECMDGOALS)))
  # skip only if no other targets in goal list
  ifeq (,$(filter-out $(_tmpCleanTargets), $(MAKECMDGOALS)))
    GSPCFG_RUN := 0
  endif
endif

# Finally, include the generated makefile which reflects GSPCFG_PROFILE.
# This 'include' triggers the chip-config.pl run.
ifeq (1,$(GSPCFG_RUN))
  -include $(GSPCFG_MAKEFRAGMENT)
endif
