#
# uproc/libs/se/config/makefile.mk
#
# Run chip-config.pl to build se-config.h and se-config.mk.
#
# *** Used by *all* builds that are rmconfig aware
#
# Each platform build will set some SECFG_* variables, then 'include' this file.
#
# Makefiles that 'include' this file:
#
#  SE product app:                    //sw/.../uproc/libs/se/src/makefile.lwmk
#      $RESMAN_ROOT/../../uproc/libs/se/src/makefile.lwmk
#

# Default target, only used when testing this makefile
.PHONY: _se-config_default
_se-config_default: _se-config_all

# input vars, provide some typical defaults
SECFG_PROFILE                ?= se-gpus
SECFG_RESMAN_ROOT            ?= ../../../../drivers/resman
SECFG_SESW_ROOT              ?= ../src/
SECFG_DIR                    ?= $(SECFG_SESW_ROOT)/../config
SECFG_CHIPCONFIG_DIR         ?= ../../../../drivers/common/chip-config
SECFG_OUTPUTDIR              ?= $(SECFG_DIR)
SECFG_VERBOSE                ?= 
SECFG_HEADER                 ?= $(SECFG_OUTPUTDIR)/se-config.h
SECFG_MAKEFRAGMENT           ?= $(SECFG_OUTPUTDIR)/se-config.mk

SECFG_OPTIONS                ?=
SECFG_DEFINES                ?= 

SECFG_BUILD_OS               ?= unknown
SECFG_TARGET_PLATFORM        ?= $(RMCFG_BUILD_OS)
SECFG_TARGET_ARCH            ?= unknown

# SECFG_RUN must be an immediate mode var since it controls an 'include' below
SECFG_RUN                    ?= 1
SECFG_RUN                    := $(SECFG_RUN)

SECFG_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
SECFG_H2PH          ?= $(SECFG_PERL) $(SECFG_DIR)/util/h2ph.pl
SECFG_MKDIR         ?= $(if $(findstring undefined,$(origin MKDIR)), mkdir -p, $(MKDIR))
SECFG_Q             ?=


# Files we create that should be deleted by 'make clean'.
# May be used by "calling" makefile.
SECFG_CLEAN          =
SECFG_CLEAN         += $(SECFG_HEADER) $(SECFG_MAKEFRAGMENT)
SECFG_CLEAN         += $(SECFG_OUTPUTDIR)/g_*.h
SECFG_CLEAN         += $(SECFG_OUTPUTDIR)/halchk.log
SECFG_CLEAN         += $(SECFG_OUTPUTDIR)/halinfo.log

# table to colwert SECFG_VERBOSE into an option for chip-config.pl
_secfgVerbosity_quiet       = --quiet
_secfgVerbosity_@           = --quiet
_secfgVerbosity_default     =
_secfgVerbosity_verbose     = --verbose
_secfgVerbosity_veryverbose = --veryverbose
_secfgVerbosity_            = $(_secfgVerbosity_default)
secfgVerbosityFlag := $(_secfgVerbosity_$(SECFG_VERBOSE))

# Files that implement chip-config.
# Assumes these vpath settings

secfgSrcFiles := 
secfgSrcFiles += $(SECFG_DIR)/se-config.cfg
secfgSrcFiles += $(SECFG_DIR)/makefile.mk

# pull in implementations list of chip-config to supplement $(secfgSrcFiles)
include $(SECFG_CHIPCONFIG_DIR)/Implementation.mk
secfgSrcFiles += $(addprefix $(SECFG_CHIPCONFIG_DIR)/, $(CHIPCONFIG_IMPLEMENTATION))

# these are all found via the magic of vpath if not found locally
vpath %.pm $(SECFG_DIR):$(SECFG_CHIPCONFIG_DIR)
secfgSrcFiles += Engines.pm
secfgSrcFiles += Features.pm

# pull in haldef list to define $(HALDEFS)
include $(SECFG_DIR)/haldefs/Haldefs.mk
secfgSrcFiles += $(foreach hd,$(SECFG_HALDEFS),$(SECFG_DIR)/haldefs/$(hd).def)

# pull in templates list to supplement $(secfgSrcFiles)
include $(SECFG_DIR)/templates/Templates.mk
secfgSrcFiles += $(addprefix $(SECFG_DIR)/templates/, $(SECFG_TEMPLATES))

# update src file list with sources .def files
include $(SECFG_DIR)/srcdefs/Srcdefs.mk
secfgSrcFiles += $(foreach srcfile,$(SECFG_SRCDEFS),$(SECFG_DIR)/srcdefs/$(srcfile).def)

# Ensure output directory(s) created before trying to generate seconfig.mk.
# On some platforms, eg OSX, need seconfig.h and seconfig.mk in different dirs.
# Use 'echo keepme > $@' instead of 'touch' which might not be avail on some platforms.
secfgOutputDirs = $(sort $(SECFG_OUTPUTDIR)/ $(dir $(SECFG_HEADER) $(SECFG_MAKEFRAGMENT)))
secfgKeeps      = $(addsuffix _keep,$(secfgOutputDirs))
secfgSrcFiles  += $(secfgKeeps)

$(secfgKeeps):
	$(SECFG_MKDIR) $(dir $@)
	echo keepme > $@

# HACK for DJGPP cmdline len
# DJGPP without CYGWIN is *special* - perl cmdline len must be < 128
DJGPP_WITH_CYGWIN ?=
secfgCmdLineLenHACK = 0
ifeq "$(SECFG_BUILD_OS)-$(DJGPP_WITH_CYGWIN)" "djgpp-false"
  secfgCmdLineLenHACK = 1
endif

# Extra options for chip-config needed by this makefile
secfgExtraOptions :=

# NOTE: lwrrently managed ./se-config script
# # lwrrently, just 1 platform for se sw
# secfgExtraOptions += --platform SE

# NOTE: lwrrently managed ./se-config script
# # and just 1 arch
# secfgExtraOptions += --arch FALCON


### Run chip-config.pl to generate seconfig.mk

# rmconfig invocation args
secfgArgs = $(secfgVerbosityFlag) \
             --mode se-config \
             --config $(SECFG_DIR)/se-config.cfg \
             --profile $(SECFG_PROFILE) \
             --source-root $(SE_SW)/src \
             --output-dir $(SECFG_OUTPUTDIR) \
             --gen-header $(SECFG_HEADER) \
             --gen-makefile $(SECFG_MAKEFRAGMENT) \
             $(SECFG_OPTIONS) \
             $(secfgExtraOptions)

# generate se-config.mk
$(SECFG_MAKEFRAGMENT): $(secfgSrcFiles)
ifeq ($(secfgCmdLineLenHACK), 1)
	$(SECFG_Q)echo $(secfgArgs)  > @secfg.cmd
	$(SECFG_Q)$(SECFG_PERL) -I$(SECFG_DIR) $(SECFG_CHIPCONFIG_DIR)/chip-config.pl @secfg.cmd
	$(RM) @secfg.cmd
else
	$(SECFG_Q)$(SECFG_PERL) -I$(SECFG_DIR) $(SECFG_CHIPCONFIG_DIR)/chip-config.pl $(secfgArgs)
endif

# se-config.h is generated as a side-effect of generating se-config.mk
$(SECFG_HEADER): $(SECFG_MAKEFRAGMENT)

# g_sources.mk is generated as a side-effect of generating se-config.mk
$(SECFG_OUTPUTDIR)/g_sources.mk: $(SECFG_MAKEFRAGMENT)

# test targets
.PHONY: _se-config_all _se-config_clean
_se-config_all: $(SECFG_MAKEFRAGMENT)
	@true

_se-config_clean:
	$(RM) $(SECFG_CLEAN)

# Don't run rmconfig on 'make clean' if there is only 1 target in MAKECMDGOALS
_tmpCleanTargets = clean clobber _se-config_clean
MAKECMDGOALS ?=
ifneq (,$(filter $(_tmpCleanTargets), $(MAKECMDGOALS)))
  # skip only if no other targets in goal list
  ifeq (,$(filter-out $(_tmpCleanTargets), $(MAKECMDGOALS)))
    SECFG_RUN := 0
  endif
endif

# Finally, include the generated makefile which reflects SECFG_PROFILE.
# This 'include' triggers the chip-config.pl run.
ifeq (1,$(SECFG_RUN))
  -include $(SECFG_MAKEFRAGMENT)
endif
