#
# uproc/lwriscv/riscvlib/config/makefile.mk
#
# Run chip-config.pl to build makefile and header files
#
# Each client build will set some RISCVLIBCFG_* variables, then 'include' this file.
#

# Default target, only used when testing this makefile
.PHONY: _riscvlib-config_default
_riscvlib-config_default: _riscvlib-config_all

# LW_SOURCE is provided by lwmake
LW_SOURCE                          ?= ../../../..

# input RISCVLIBCFG_* variables, provide some typical defaults
RISCVLIBCFG_PROFILE                ?= riscvlib-gpus
RISCVLIBCFG_RISCVLIB_ROOT          ?= $(LW_SOURCE)/uproc/lwriscv
RISCVLIBCFG_SRC_ROOT               ?= $(RISCVLIBCFG_RISCVLIB_ROOT)
RISCVLIBCFG_DIR                    ?= $(RISCVLIBCFG_RISCVLIB_ROOT)/config
RISCVLIBCFG_CHIPCONFIG_DIR         ?= $(LW_SOURCE)/drivers/common/chip-config
RISCVLIBCFG_OUTPUTDIR              ?= $(RISCVLIBCFG_DIR)
RISCVLIBCFG_VERBOSE                ?=
RISCVLIBCFG_HEADER                 ?= $(RISCVLIBCFG_OUTPUTDIR)/riscvlib-config.h
RISCVLIBCFG_MAKEFRAGMENT           ?= $(RISCVLIBCFG_OUTPUTDIR)/riscvlib-config.mk
RISCVLIBCFG_CONFIGFILE             ?= $(RISCVLIBCFG_DIR)/riscvlib-config.cfg

# options added in this makefile.mk
RISCVLIBCFG_OPTIONS                ?=

# Variables from PMU, GSP, SEC2, and SOE
GSP_RTOS                           ?=
PMU_RTOS                           ?=
SOE_RTOS                           ?=
SEC2_RTOS                          ?=

# pass PMU_BUILD/GSP_BUILD/SEC2_BUILD/SOE_BUILD information to riscvlib-config
ifneq (,$(GSP_RTOS))
  RISCVLIBCFG_OPTIONS              += --enable GSP_BUILD
else ifneq (,$(PMU_RTOS))
  RISCVLIBCFG_OPTIONS              += --enable PMU_BUILD
else ifneq (,$(SEC2_RTOS))
  RISCVLIBCFG_OPTIONS              += --enable SEC2_BUILD
else ifneq (,$(SOE_RTOS))
  RISCVLIBCFG_OPTIONS              += --enable SOE_BUILD
endif

ifdef ODP_ENABLED
  ifeq ("$(ODP_ENABLED)","true")
    RISCVLIBCFG_OPTIONS              += --enable ON_DEMAND_PAGING
  endif
endif

ifeq ("$(LWRISCV_CORE_DUMP)", "true")
    RISCVLIBCFG_OPTIONS              += --enable LWRISCV_CORE_DUMP
endif
ifeq ("$(LWRISCV_PARTITION_SWITCH)", "true")
    RISCVLIBCFG_OPTIONS              += --enable LWRISCV_PARTITION_SWITCH
endif
ifeq ("$(LWRISCV_MPU_DUMP_ENABLED)", "true")
    RISCVLIBCFG_OPTIONS              += --enable LWRISCV_MPU_DUMP_ENABLED
endif
ifneq ("$(LWRISCV_DEBUG_PRINT_LEVEL)","0")
    RISCVLIBCFG_OPTIONS              += --enable LWRISCV_DEBUG_PRINT
endif
ifeq ("$(LWRISCV_SYMBOL_RESOLVER)", "true")
    RISCVLIBCFG_OPTIONS              += --enable LWRISCV_SYMBOL_RESOLVER
endif
ifeq ("$(SCHEDULER_ENABLED)", "true")
    RISCVLIBCFG_OPTIONS              += --enable SCHEDULER_ENABLED
endif


# RISCVLIBCFG_RUN must be an immediate mode var since it controls an 'include' below
RISCVLIBCFG_RUN                    ?= 1
RISCVLIBCFG_RUN                    := $(RISCVLIBCFG_RUN)

RISCVLIBCFG_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
RISCVLIBCFG_MKDIR         ?= $(if $(findstring undefined,$(origin MKDIR)), mkdir -p, $(MKDIR))
RISCVLIBCFG_Q             ?=


# Files we create that should be deleted by 'make clean'.
# May be used by "calling" makefile.
RISCVLIBCFG_CLEAN          =
RISCVLIBCFG_CLEAN         += $(RISCVLIBCFG_HEADER) $(RISCVLIBCFG_MAKEFRAGMENT)
RISCVLIBCFG_CLEAN         += $(RISCVLIBCFG_OUTPUTDIR)/g_*.h
RISCVLIBCFG_CLEAN         += $(RISCVLIBCFG_OUTPUTDIR)/halchk.log
RISCVLIBCFG_CLEAN         += $(RISCVLIBCFG_OUTPUTDIR)/halinfo.log

# table to colwert RISCVLIBCFG_VERBOSE into an option for chip-config.pl
_riscvlibcfgVerbosity_quiet       = --quiet
_riscvlibcfgVerbosity_@           = --quiet
_riscvlibcfgVerbosity_default     =
_riscvlibcfgVerbosity_verbose     = --verbose
_riscvlibcfgVerbosity_veryverbose = --veryverbose
_riscvlibcfgVerbosity_            = $(_riscvlibcfgVerbosity_default)
riscvlibcfgVerbosityFlag := $(_riscvlibcfgVerbosity_$(RISCVLIBCFG_VERBOSE))

# Files that implement chip-config.
# Assumes these vpath settings

riscvlibcfgSrcFiles :=
riscvlibcfgSrcFiles += $(RISCVLIBCFG_CONFIGFILE)
riscvlibcfgSrcFiles += $(RISCVLIBCFG_DIR)/makefile.mk

# pull in implementations list of chip-config to supplement $(riscvlibcfgSrcFiles)
include $(RISCVLIBCFG_CHIPCONFIG_DIR)/Implementation.mk
riscvlibcfgSrcFiles += $(addprefix $(RISCVLIBCFG_CHIPCONFIG_DIR)/, $(CHIPCONFIG_IMPLEMENTATION))

# these are all found via the magic of vpath if not found locally
vpath %.pm $(RISCVLIBCFG_DIR):$(RISCVLIBCFG_CHIPCONFIG_DIR)
riscvlibcfgSrcFiles += Modules.pm
riscvlibcfgSrcFiles += Features.pm

# pull in haldef list to define $(HALDEFS)
include $(RISCVLIBCFG_DIR)/haldefs/Haldefs.mk
riscvlibcfgSrcFiles += $(foreach hd,$(RISCVLIBCFG_HALDEFS),$(RISCVLIBCFG_DIR)/haldefs/$(hd).def)

# pull in templates list to supplement $(riscvlibcfgSrcFiles)
include $(RISCVLIBCFG_DIR)/templates/Templates.mk
riscvlibcfgSrcFiles += $(addprefix $(RISCVLIBCFG_DIR)/templates/, $(RISCVLIBCFG_TEMPLATES))

# update src file list with sources .def files
include $(RISCVLIBCFG_DIR)/srcdefs/Srcdefs.mk
riscvlibcfgSrcFiles += $(foreach srcfile,$(RISCVLIBCFG_SRCDEFS),$(RISCVLIBCFG_DIR)/srcdefs/$(srcfile).def)

# Ensure output directory(s) created before trying to generate riscvlibconfig.mk.
# On some platforms, eg OSX, need riscvlibconfig.h and riscvlibconfig.mk in different dirs.
# Use 'echo keepme > $@' instead of 'touch' which might not be avail on some platforms.
riscvlibcfgOutputDirs = $(sort $(RISCVLIBCFG_OUTPUTDIR)/ $(dir $(RISCVLIBCFG_HEADER) $(RISCVLIBCFG_MAKEFRAGMENT)))
riscvlibcfgKeeps      = $(addsuffix _keep,$(riscvlibcfgOutputDirs))
riscvlibcfgSrcFiles  += $(riscvlibcfgKeeps)

$(riscvlibcfgKeeps):
	$(RISCVLIBCFG_MKDIR) $(dir $@)
	$(ECHO) keepme > $@

### Run chip-config.pl to generate riscvlibconfig.mk
# rmconfig invocation args
riscvlibcfgArgs = $(riscvlibcfgVerbosityFlag) \
             --mode         riscvlib-config \
             --config       $(RISCVLIBCFG_CONFIGFILE) \
             --profile      $(RISCVLIBCFG_PROFILE) \
             --source-root  $(RISCVLIBCFG_SRC_ROOT) \
             --output-dir   $(RISCVLIBCFG_OUTPUTDIR) \
             --gen-header   $(RISCVLIBCFG_HEADER) \
             --gen-makefile $(RISCVLIBCFG_MAKEFRAGMENT) \
             $(RISCVLIBCFG_OPTIONS)

# RISCVLIBCFG_OPTIONS etc. might get changed by different
# make flags. Add a dependency to check the value of riscvlibcfgArgs
# and re-run chip-config if it changed.
RISCVLIBCFG_ARGS_STAMP_FILE := $(OUTPUTDIR)/_riscvLibCfgArgsStamp
RISCVLIBCFG_ARGS_OLD :=

ifneq ($(wildcard $(RISCVLIBCFG_ARGS_STAMP_FILE)),)
  RISCVLIBCFG_ARGS_OLD := $(shell $(CAT) $(RISCVLIBCFG_ARGS_STAMP_FILE))
endif

ifneq ($(riscvlibcfgArgs),$(RISCVLIBCFG_ARGS_OLD))
  .PHONY: $(RISCVLIBCFG_ARGS_STAMP_FILE)
endif

$(RISCVLIBCFG_ARGS_STAMP_FILE):
	$(ECHO) "$(riscvlibcfgArgs)" > $@

# generate riscvlib-config.mk
$(RISCVLIBCFG_MAKEFRAGMENT): $(riscvlibcfgSrcFiles) $(RISCVLIBCFG_ARGS_STAMP_FILE)
	$(RISCVLIBCFG_Q)$(RISCVLIBCFG_PERL) -I$(RISCVLIBCFG_DIR) $(RISCVLIBCFG_CHIPCONFIG_DIR)/chip-config.pl $(riscvlibcfgArgs)

# riscvlib-config.h is generated as a side-effect of generating riscvlib-config.mk
$(RISCVLIBCFG_HEADER): $(RISCVLIBCFG_MAKEFRAGMENT)

# g_sources.mk is generated as a side-effect of generating riscvlib-config.mk
$(RISCVLIBCFG_OUTPUTDIR)/g_sources.mk: $(RISCVLIBCFG_MAKEFRAGMENT)

# test targets
.PHONY: _riscvlib-config_all _riscvlib-config_clean
_riscvlib-config_all: $(RISCVLIBCFG_MAKEFRAGMENT)
	@true

_riscvlib-config_clean:
	$(RM) $(RISCVLIBCFG_CLEAN)

# Don't run rmconfig on 'make clean' if there is only 1 target in MAKECMDGOALS
_tmpCleanTargets = clean clobber _riscvlib-config_clean
MAKECMDGOALS ?=
ifneq (,$(filter $(_tmpCleanTargets), $(MAKECMDGOALS)))
  # skip only if no other targets in goal list
  ifeq (,$(filter-out $(_tmpCleanTargets), $(MAKECMDGOALS)))
    RISCVLIBCFG_RUN := 0
  endif
endif

# Finally, include the generated makefile which reflects RISCVLIBCFG_PROFILE.
# This 'include' triggers the chip-config.pl run.
ifeq (1,$(RISCVLIBCFG_RUN))
  -include $(RISCVLIBCFG_MAKEFRAGMENT)
endif
