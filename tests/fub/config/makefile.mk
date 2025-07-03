#
# uproc/fub/config/makefile.mk
#
# Run chip-config.pl to build fub-config.h and fub-config.mk.
#
# *** Used by *all* builds that are rmconfig aware
#
# Each platform build will set some FUBCFG_* variables, then 'include' this file.
#
# Makefiles that 'include' this file:
#
#  FUB product app:                    //sw/.../uproc/fub/src/makefile.lwmk
#      $RESMAN_ROOT/../../uproc/fub/src/makefile.lwmk
#

# Default target, only used when testing this makefile
.PHONY: _fub-config_default
_fub-config_default: _fub-config_all

# input vars, provide some typical defaults
FUBCFG_PROFILE                ?= fub-lwdec_tu10x_aes
FUBCFG_RESMAN_ROOT            ?= ../../../drivers/resman
FUBCFG_FUBSW_ROOT             ?= ../src/
FUBCFG_DIR                    ?= $(FUBCFG_FUBSW_ROOT)/../config
FUBCFG_CHIPCONFIG_DIR         ?= ../../../drivers/common/chip-config
FUBCFG_OUTPUTDIR              ?= $(FUBCFG_DIR)
FUBCFG_VERBOSE                ?= 
FUBCFG_HEADER                 ?= $(FUBCFG_OUTPUTDIR)/fub-config.h
FUBCFG_MAKEFRAGMENT           ?= $(FUBCFG_OUTPUTDIR)/fub-config.mk

FUBCFG_OPTIONS                ?=
FUBCFG_DEFINES                ?= 

FUBCFG_BUILD_OS               ?= unknown
FUBCFG_TARGET_PLATFORM        ?= $(RMCFG_BUILD_OS)
FUBCFG_TARGET_ARCH            ?= unknown

# FUBCFG_RUN must be an immediate mode var since it controls an 'include' below
FUBCFG_RUN                    ?= 1
FUBCFG_RUN                    := $(FUBCFG_RUN)

FUBCFG_PERL                   ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
FUBCFG_MKDIR                  ?= $(if $(findstring undefined,$(origin MKDIR)), mkdir -p, $(MKDIR))
FUBCFG_Q                      ?=

# Files we create that should be deleted by 'make clean'.
# May be used by "calling" makefile.
FUBCFG_CLEAN          =
FUBCFG_CLEAN         += $(FUBCFG_HEADER) $(FUBCFG_MAKEFRAGMENT)
FUBCFG_CLEAN         += $(FUBCFG_OUTPUTDIR)/g_*.h
FUBCFG_CLEAN         += $(FUBCFG_OUTPUTDIR)/halchk.log
FUBCFG_CLEAN         += $(FUBCFG_OUTPUTDIR)/halinfo.log

# table to colwert FUBCFG_VERBOSE into an option for chip-config.pl
_fubcfgverbosity_quiet       = --quiet
_fubcfgverbosity_@           = --quiet
_fubcfgverbosity_default     =
_fubcfgverbosity_verbose     = --verbose
_fubcfgverbosity_veryverbose = --veryverbose
_fubcfgverbosity_            = $(_fubcfgverbosity_default)
fubcfgverbosityflag         := $(_fubcfgverbosity_$(FUBCFG_VERBOSE))

# Files that implement chip-config.
# Assumes these vpath settings
fubcfgsrcfiles := 
fubcfgsrcfiles += $(FUBCFG_DIR)/fub-config.cfg
fubcfgsrcfiles += $(FUBCFG_DIR)/makefile.mk

# pull in implementations list of chip-config to supplement $(fubcfgsrcfiles)
include $(FUBCFG_CHIPCONFIG_DIR)/Implementation.mk
fubcfgsrcfiles += $(addprefix $(FUBCFG_CHIPCONFIG_DIR)/, $(CHIPCONFIG_IMPLEMENTATION))

# these are all found via the magic of vpath if not found locally
vpath %.pm $(FUBCFG_DIR):$(FUBCFG_CHIPCONFIG_DIR)
fubcfgsrcfiles += Engines.pm
fubcfgsrcfiles += Features.pm

# pull in haldef list to define $(HALDEFS)
include $(FUBCFG_DIR)/haldefs/Haldefs.mk
fubcfgsrcfiles += $(foreach hd,$(FUBCFG_HALDEFS),$(FUBCFG_DIR)/haldefs/$(hd).def)

# pull in templates list to supplement $(fubcfgsrcfiles)
include $(FUBCFG_DIR)/templates/Templates.mk
fubcfgsrcfiles += $(addprefix $(FUBCFG_DIR)/templates/, $(FUBCFG_TEMPLATES))

# update src file list with sources .def files
include $(FUBCFG_DIR)/srcdefs/Srcdefs.mk
fubcfgsrcfiles += $(foreach srcfile,$(FUBCFG_SRCDEFS),$(FUBCFG_DIR)/srcdefs/$(srcfile).def)

# Ensure output directory(s) created before trying to generate ifubconfig.mk.
# On some platforms, eg OSX, need fubconfig.h and fubconfig.mk in different dirs.
# Use 'echo keepme > $@' instead of 'touch' which might not be avail on some platforms.
fubcfgoutputDirs = $(sort $(FUBCFG_OUTPUTDIR)/ $(dir $(FUBCFG_HEADER) $(FUBCFG_MAKEFRAGMENT)))
fubcfgkeeps      = $(addsuffix _keep,$(fubcfgoutputDirs))
fubcfgsrcfiles  += $(fubcfgkeeps)

$(fubcfgkeeps):
	$(FUBCFG_MKDIR) $(dir $@)
	echo keepme > $@

# HACK for DJGPP cmdline len
# DJGPP without CYGWIN is *special* - perl cmdline len must be < 128
DJGPP_WITH_CYGWIN    ?=
fubcfgcmdlinelenhack  = 0
ifeq "$(FUBCFG_BUILD_OS)-$(DJGPP_WITH_CYGWIN)" "djgpp-false"
  fubcfgcmdlinelenhack = 1
endif

# Extra options for chip-config needed by this makefile
fubcfgextraoptions :=

# NOTE: lwrrently managed ./fub-config script
# # lwrrently, just 1 platform for fub sw
# fubcfgextraoptions += --platform FUB

# NOTE: lwrrently managed ./fub-config script
# # and just 1 arch
# fubcfgextraoptions += --arch FALCON

### Run chip-config.pl to generate fubconfig.mk

# rmconfig invocation args
fubcfgrgs = $(fubcfgverbosityflag) \
             --mode fub-config \
             --config $(FUBCFG_DIR)/fub-config.cfg \
             --profile $(FUBCFG_PROFILE) \
             --source-root $(FUBSW)/src \
             --output-dir $(FUBCFG_OUTPUTDIR) \
             --gen-header $(FUBCFG_HEADER) \
             --gen-makefile $(FUBCFG_MAKEFRAGMENT) \
             $(FUBCFG_OPTIONS) \
             $(fubcfgextraoptions)

# generate fub-config.mk
$(FUBCFG_MAKEFRAGMENT): $(fubcfgsrcfiles)
ifeq ($(fubcfgcmdlinelenhack),1)
	$(FUBCFG_Q)echo $(fubcfgrgs)  > @fubcfg.cmd
	$(FUBCFG_Q)$(FUBCFG_PERL) -I$(FUBCFG_DIR) $(FUBCFG_CHIPCONFIG_DIR)/chip-config.pl @fubcfg.cmd
	$(RM) @fubcfg.cmd
else
	$(FUBCFG_Q)$(FUBCFG_PERL) -I$(FUBCFG_DIR) $(FUBCFG_CHIPCONFIG_DIR)/chip-config.pl $(fubcfgrgs)
endif

# fub-config.h is generated as a side-effect of generating fub-config.mk
$(FUBCFG_HEADER): $(FUBCFG_MAKEFRAGMENT)

# g_sources.mk is generated as a side-effect of generating fub-config.mk
$(FUBCFG_OUTPUTDIR)/g_sources.mk: $(FUBCFG_MAKEFRAGMENT)

# test targets
.PHONY: _fub-config_all _fub-config_clean
_fub-config_all: $(FUBCFG_MAKEFRAGMENT)
	@true

_fub-config_clean:
	$(RM) $(FUBCFG_CLEAN)

# Don't run rmconfig on 'make clean' if there is only 1 target in MAKECMDGOALS
_tmpCleanTargets = clean clobber _fub-config_clean
MAKECMDGOALS ?=
ifneq (,$(filter $(_tmpCleanTargets), $(MAKECMDGOALS)))
  # skip only if no other targets in goal list
  ifeq (,$(filter-out $(_tmpCleanTargets), $(MAKECMDGOALS)))
    FUBCFG_RUN := 0
  endif
endif

# Finally, include the generated makefile which reflects FUBCFG_PROFILE.
# This 'include' triggers the chip-config.pl run.
ifeq (1,$(FUBCFG_RUN))
  -include $(FUBCFG_MAKEFRAGMENT)
endif
