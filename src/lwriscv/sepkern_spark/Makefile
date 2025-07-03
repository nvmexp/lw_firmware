ifneq ($(shell expr `echo $(MAKE_VERSION) | sed -r "s/\.[0-9]+//"` \>= 4),1)
$(error You must have a make version >= 4)
endif

include ./config/config.mk

BUILD_PROJECT_NAME := $(ENGINE)_sepkern-$(PROJ)
NO_RELEASE         ?= false
###############################################################################
#
###############################################################################
LW_HOST_OS_FAMILY := Unix
LW_HOST_OS        := Linux
LW_HOST_ARCH      := x86

ifeq ($(LW_HOST_OS_FAMILY),Unix)
  ECHO     := @/bin/echo
  RM       := /bin/rm -f
  MKDIR    := /bin/mkdir -p
  RMDIR    := /bin/rm -rf
  COPY     := /bin/cp
  MV       := /bin/mv
  P4       := /home/utils/bin/p4

  PERLDIR  := $(LW_TOOLS)/unix/hosts/$(LW_HOST_OS)-$(LW_HOST_ARCH)/perl-5.10.0
  PERLLIB  := $(PERLDIR)/lib/5.10.0
  PERL     := $(PERLDIR)/bin/perl
endif

###############################################################################
#
###############################################################################
GNAT_PROVE  := $(SPARK_PRO_PATH)/bin/gnatprove
GPR_BUILD   := $(GNAT_PRO_PATH)/bin/gprbuild
GPR_CLEAN   := $(GNAT_PRO_PATH)/bin/gprclean
GNAT_PP     := $(GNAT_PRO_PATH)/bin/riscv64-elf-gnatpp
GNATSTACK   := $(GNAT_PRO_PATH)/bin/gnatstack

CC          :=$(RISCV_TOOLCHAIN)gcc
LD          :=$(RISCV_TOOLCHAIN)ld
STRIP       :=$(RISCV_TOOLCHAIN)strip
OBJDUMP     :=$(RISCV_TOOLCHAIN)objdump
OBJCOPY     :=$(RISCV_TOOLCHAIN)objcopy
READELF     :=$(RISCV_TOOLCHAIN)readelf
NM          :=$(RISCV_TOOLCHAIN)nm

###############################################################################
#
###############################################################################
ARCH:=rv64im
CFLAGS+=-std=c11 -Wall -Wextra
CFLAGS+=-mabi=lp64 -march=$(ARCH)

# SPARK ####################################################
TARGET_PATH := $(SPARK_OBJ_DIR)
VERBOSE ?= 0
ifneq ($(VERBOSE), 0)
    GPR_VERBOSE := -vh
else
    GPR_VERBOSE :=
endif


###############################################################################
# Make variable used to tell mkimage which changelist number to include in the
# descriptor for the c-file image.
###############################################################################
APP_P4_CL := $(word 2, $(shell $(P4) changes -m1 "$(LW_SOURCE)/..."))
#####################################################

all: $(RELEASE_FILES)

###############################################################################
# Rule to produce the ELF image. Building all object-files is the prerequisite
# for this rule.
###############################################################################
SPARK_DIR_CREATED:=$(SPARK_OBJ_DIR)/.created
OUTPUTDIR_CREATED:=$(OUTPUTDIR)/.created

$(SPARK_DIR_CREATED):
	$(MKDIR) $(SPARK_OBJ_DIR)
	touch $@

$(OUTPUTDIR_CREATED):
	$(MKDIR) $(OUTPUTDIR)
	touch $@

$(ELF_FILE): $(SPARK_OBJ_DIR)/$(TARGET_NAME) $(OUTPUTDIR_CREATED)
	$(COPY) $(SPARK_OBJ_DIR)/$(TARGET_NAME) $(ELF_FILE)

# We need to make it phony, as makefile has no way of knowing what spark files need
# for build. Spark has it's own build system.
.PHONY: $(SPARK_OBJ_DIR)/$(TARGET_NAME)
$(SPARK_OBJ_DIR)/$(TARGET_NAME): $(SPARK_DIR_CREATED)
	$(ECHO) [$(BUILD_PROJECT_NAME)] building $(notdir $@)
	$(GPR_BUILD) -P $(GPR_NAME) -d --target=riscv64-elf -XLOADER=USER -XEngine=$(ENGINE) -XChip=$(PROJ) -o $(TARGET_NAME) $(GPR_VERBOSE)

# Split rules for specific files to properly handle generation
$(ELF_NO_SYM): $(ELF_FILE)
	$(COPY) $(ELF_FILE) $(ELF_NO_SYM)
	$(STRIP) -w --strip-debug --strip-symbol='__FUNCTION__.*' $(ELF_NO_SYM)
	$(OBJCOPY) --remove-section .comment  $(ELF_NO_SYM)

$(ELF_READELF): $(ELF_FILE)
	$(ECHO) "AppVersion: $(APP_P4_CL)" > $(ELF_READELF)
	$(READELF) --all $(ELF_FILE) >> $(ELF_READELF)

$(IMG_DATA): $(ELF_NO_SYM)
	$(OBJCOPY) -O binary -j .monitor_data $(ELF_NO_SYM) $(IMG_DATA)

$(IMG_CODE): $(ELF_NO_SYM)
	$(OBJCOPY) -O binary -j .monitor_code $(ELF_NO_SYM) $(IMG_CODE)

$(ELF_OBJDUMP): $(ELF_NO_SYM)
	$(ECHO) "AppVersion: $(APP_P4_CL). " > $(ELF_OBJDUMP)
	$(OBJDUMP) -D $(ELF_NO_SYM) >> $(ELF_OBJDUMP)

$(ELF_NM): $(ELF_NO_SYM)
	$(ECHO) "AppVersion: $(APP_P4_CL). " > $(ELF_NM)
	$(NM) $(ELF_NO_SYM) >> $(ELF_NM)

###############################################################################
# Manifest / signing rules
###############################################################################

# Input / $1: source file, $2 - output (withouht extensions)
define MANIFEST_BIN_template =
MANIFEST_BINS += $(2).bin
MANIFEST_HEXS += $(2).hex
$(2).o: $(1) $$(MANIFEST_H) $(OUTPUTDIR_CREATED)
	$$(ECHO) [$$(BUILD_PROJECT_NAME)] compiling $$(notdir $$<)
	$$(CC) $$< $$(CFLAGS) -DIMEM_LIMIT=$$(IMEM_LIMIT) -DDMEM_LIMIT=$$(DMEM_LIMIT) -c -o $$@

$(2).bin: $(2).o
	$$(ECHO) [$$(BUILD_PROJECT_NAME)] generating $$(notdir $$@)
	$$(OBJCOPY) -O binary -j .rodata $$< $$@
endef

$(foreach src,$(MANIFEST_SUFFIXES),$(eval $(call MANIFEST_BIN_template,$(MANIFEST_LOC)/manifest_$$(ENGINE)_$$(PROJ)$(src),$(IMG_TARGET)_manifest$(basename $(src)))))
MANIFEST_OUT_SUFFIX := _enc.bin
MANIFEST_OUTS_DBG  := $(addsuffix _dbg$(MANIFEST_OUT_SUFFIX),$(MANIFEST_BINS))

# We can't parallelize signing because siggen doesn't work well in parallel :(
# Make it depend on one of binaries
$(IMG_CODE_ENC) $(IMG_DATA_ENC) $(IMG_CODE_ENC_DEBUG) $(IMG_DATA_ENC_DEBUG) : $(MANIFEST_OUTS_DBG)

SIGGEN_PROFILE :=
ifeq ($(PROJ),ga10x)
  SIGGEN_PROFILE := ga102_rsa3k_0
else
  ifeq ($(PROJ),ad10x)
    # MMINTS-TODO: remove -staticIV and generate an sk_mf combined image when building AD10X!
    SIGGEN_PROFILE := ad10x_rsa3k_0 -staticIV
  endif
endif

ifneq ($(SKIP_PROD_SIGN), 1)
MANIFEST_OUTS_PRD  := $(addsuffix _prd$(MANIFEST_OUT_SUFFIX),$(MANIFEST_BINS))

$(IMG_CODE_ENC) $(IMG_DATA_ENC) : $(MANIFEST_OUTS_PRD)

$(MANIFEST_OUTS_PRD): $(MANIFEST_BINS) $(IMG_CODE) $(IMG_DATA)
	$(ECHO) [$(BUILD_PROJECT_NAME)] prod signing $(notdir $(MANIFEST_BINS))
	$(foreach manifest,$(MANIFEST_BINS),$(MANIFEST_SIGGEN) -manifest $(manifest) -code $(IMG_CODE) -data $(IMG_DATA) -chip $(SIGGEN_PROFILE) -mode 1;$(MV) $(addsuffix .encrypt.bin.out.bin,$(manifest))  $(addsuffix _prd$(MANIFEST_OUT_SUFFIX),$(manifest));)
endif

$(MANIFEST_OUTS_DBG): $(MANIFEST_BINS) $(IMG_CODE) $(IMG_DATA)
	$(ECHO) [$(BUILD_PROJECT_NAME)] debug signing $(notdir $(MANIFEST_BINS))
	$(foreach manifest,$(MANIFEST_BINS),$(MANIFEST_SIGGEN) -manifest $(manifest) -code $(IMG_CODE) -data $(IMG_DATA) -chip $(SIGGEN_PROFILE);$(MV) $(addsuffix .encrypt.bin.out.bin,$(manifest))  $(addsuffix _dbg$(MANIFEST_OUT_SUFFIX),$(manifest));)
	$(MV) $(IMG_CODE_ENC) $(IMG_CODE_ENC_DEBUG)
	$(MV) $(IMG_DATA_ENC) $(IMG_DATA_ENC_DEBUG)


###############################################################################
# The install process is handled by a perl-script to avoid the unnecessary
# complication of host-specific process in make.
###############################################################################

RELEASE_SCRIPT = $(BUILD_SCRIPTS)/release-imgs-if-changed.pl

COMMA := ,
EMPTY :=
SPACE := $(EMPTY) $(EMPTY)

# We don't update monitor or manifest often, this should always be released
RELEASE_IMG_ARGS += --force

RELEASE_IMG_ARGS += --output-prefix $(BUILD_PROJECT_NAME)
RELEASE_IMG_ARGS += --p4            $(P4)

RELEASE_IMG_ARGS += --release-path $(RELEASE_PATH)

RELEASE_IMG_ARGS += --release-files            \
  $(subst $(SPACE),$(COMMA),$(RELEASE_FILES))

#
# Trigger the process by adding a phony prerequisite to 'all' called 'install'.
# The rule to rebuild that target will perform the install process and will copy
# the files to the RM if necessary.
# Skip this process if the user has requested that files NOT be installed via
# NO_INSTALL=true.
#
ifneq ("$(NO_RELEASE)","true")
  all: install
endif

###############################################################################
#
###############################################################################
.PHONY: clean prove examine stackcheck install

install: $(RELEASE_FILES)
	$(ECHO) [$(BUILD_PROJECT_NAME)] releasing maybe... check timestamps...
# We need to call release script several times as it can check only one "target" file
# Release comparing .elf (in case sepkern changed)
	$(PERL) $(RELEASE_SCRIPT) $(RELEASE_IMG_ARGS) --image $(ELF_FILE)
# Release comparing hexdump of manifests (in case either manifest changed)
	$(foreach manifest_hex,$(MANIFEST_HEXS),$(PERL) $(RELEASE_SCRIPT) $(RELEASE_IMG_ARGS) --image $(manifest_hex);)

prove:
	$(GNAT_PROVE) -P $(GPR_NAME) -j0 -XLOADER=USER -XEngine=$(ENGINE) -XChip=$(PROJ) --level=2 --ide-progress-bar --timeout=32

examine:
	$(GNAT_PROVE) -P $(GPR_NAME) -j0 -XLOADER=USER --mode=flow --ide-progress-bar

stackcheck:
	$(GNATSTACK) -v -P $(GPR_NAME) -l 1 -XLOADER=USER -XEngine=$(ENGINE) -XChip=$(PROJ)

# MK TODO: this has to be update once we support windows builds.
%.hex: %.bin
	$(ECHO) [$(BUILD_PROJECT_NAME)] Generating $(notdir $@)
	/usr/bin/xxd $< $@
