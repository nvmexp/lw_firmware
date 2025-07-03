include ./config/config.mk
include $(PROFILE_MPSK_CONFIG)

BUILD_PROJECT_NAME := $(ENGINE)_sepkern-$(CHIP)
NO_RELEASE         ?= false

###############################################################################
#
###############################################################################
LW_HOST_OS_FAMILY := Unix
LW_HOST_OS        := Linux
LW_HOST_ARCH      := x86

ifeq ($(LW_HOST_OS_FAMILY),Unix)
  ECHO     := /bin/echo
  RM       := /bin/rm -rf
  MKDIR    := /bin/mkdir -p
  RMDIR    := /bin/rm -rf
  COPY     := /bin/cp
  MV       := /bin/mv
  TOUCH    := /bin/touch
  SED      := /bin/sed
  CHMOD    := /bin/chmod
  SHELL    := /bin/bash
endif

# Check p4 path
# in unix-build environment, use the dedicated p4 client unix-build sets up
# in normal build environment, use the one in path, or the one in utils directoy
LW_IN_UNIX_BUILD_CHROOT ?= 0

ifeq ($(LW_IN_UNIX_BUILD_CHROOT),1)
    P4 := /bin/unix-build-p4-proxy-client
else
    ifneq ($(shell which p4),)
        P4 := $(shell which p4)
    else
        P4 := /home/utils/bin/p4
    endif
endif

# Check Make version
MAKE_VER_GE_4 := $(shell if [ `$(ECHO) $(MAKE_VERSION) | $(SED) -r "s/\..*$$//"` -ge 4 ]; then echo "y"; else echo "n"; fi)

ifeq ($(MAKE_VER_GE_4), n)
$(error You must have a make version >= 4, current version is $(MAKE_VERSION))
endif

###############################################################################
#
###############################################################################
GNAT_PROVE  := $(SPARK_PRO_PATH)/bin/gnatprove
GNATSTACK   := $(GNAT_PATH)/bin/gnatstack
GPR_BUILD   := $(GNAT_PATH)/bin/gprbuild
GPR_CLEAN   := $(GNAT_PATH)/bin/gprclean

STRIP       := $(GNAT_PATH)/bin/riscv64-elf-strip
OBJDUMP     := $(GNAT_PATH)/bin/riscv64-elf-objdump
OBJCOPY     := $(GNAT_PATH)/bin/riscv64-elf-objcopy
READELF     := $(GNAT_PATH)/bin/riscv64-elf-readelf
NM          := $(GNAT_PATH)/bin/riscv64-elf-nm
###############################################################################


# GPR Build ###################################################################
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
LW_DVS_BLD ?= 0
ifneq ($(LW_DVS_BLD),1)
  APP_P4_CL := $(word 2, $(shell $(P4) changes -m1 "$(MONITOR_ROOT)/..."))
else
  APP_P4_CL = $(CHANGELIST)
endif
###############################################################################

all: $(RELEASE_FILES)

###############################################################################
# Rule to produce the ELF image. Building all object-files is the prerequisite
# for this rule.
###############################################################################
SPARK_DIR_CREATED:=$(SPARK_OBJ_DIR)/.created
OUTPUTDIR_CREATED:=$(OUTPUTDIR)/.created
INSTALL_DIR_CREATED:=$(SEPKERN_INSTALL_DIR)/.created
PREBUILD_DIR_CREATED:=$(PREBUILD_DIR)/.created

$(SPARK_DIR_CREATED):
	$(MKDIR) $(SPARK_OBJ_DIR)
	$(TOUCH) $@

ifneq ($(OUTPUTDIR_CREATED), $(SPARK_DIR_CREATED))
$(OUTPUTDIR_CREATED):
	$(MKDIR) $(OUTPUTDIR)
	$(TOUCH) $@
endif

$(INSTALL_DIR_CREATED):
	$(MKDIR) $(SEPKERN_INSTALL_DIR)
	$(TOUCH) $@

$(PREBUILD_DIR_CREATED): $(SPARK_DIR_CREATED)
	$(MKDIR) $(PREBUILD_DIR)
	$(TOUCH) $@

$(ELF_FILE): $(SPARK_OBJ_DIR)/$(TARGET_NAME) $(OUTPUTDIR_CREATED)
	$(COPY) $(SPARK_OBJ_DIR)/$(TARGET_NAME) $(ELF_FILE)

# We need to make it phony, as makefile has no way of knowing what spark files need
# for build. Spark has it's own build system.
.PHONY: $(SPARK_OBJ_DIR)/$(TARGET_NAME)
$(SPARK_OBJ_DIR)/$(TARGET_NAME): $(SPARK_DIR_CREATED) $(SEPKERN_POLICY_PREBUILD) $(DUMMY_POLICY_PREBUILD)
	$(info ===============================================================================)
	$(info =========================      Separation Kernel      =========================)
	$(info SK path:         $(SEPKERN_SW))
	$(info Partition Count: $(PARTITION_COUNT))
	$(info Policy Path:     $(POLICY_PATH))
	$(info Policy File:     $(POLICY_FILE_NAME))
	$(info PLM List Path:   $(PLM_LIST_PATH))
	$(info PLM List File:   $(PLM_LIST_FILE_NAME))
	$(info Chip:            $(CHIP))
	$(info Engine:          $(ENGINE))
	$(info ===============================================================================)
	@$(ECHO) [$(BUILD_PROJECT_NAME)] building $(notdir $@)
	$(GPR_BUILD) -P $(GPR_NAME) -d --target=riscv64-elf -XLOADER=USER -XEngine=$(ENGINE) -XChip=$(CHIP) -o $(TARGET_NAME) $(GPR_VERBOSE) -j8

# Split rules for specific files to properly handle generation
$(ELF_NO_SYM): $(ELF_FILE)
	$(COPY) $(ELF_FILE) $(ELF_NO_SYM)
	$(STRIP) -w --strip-debug --strip-symbol='__FUNCTION__.*' $(ELF_NO_SYM)
	$(OBJCOPY) --remove-section .comment  $(ELF_NO_SYM)

$(ELF_READELF): $(ELF_FILE)
	@$(ECHO) "AppVersion: $(APP_P4_CL)" > $(ELF_READELF)
	$(READELF) --all $(ELF_FILE) >> $(ELF_READELF)

$(IMG_DATA): $(ELF_NO_SYM)
	$(OBJCOPY) -O binary -j .sepkern_data $(ELF_NO_SYM) $(IMG_DATA)

$(IMG_CODE): $(ELF_NO_SYM)
	$(OBJCOPY) -O binary -j .sepkern_code $(ELF_NO_SYM) $(IMG_CODE)

$(ELF_OBJDUMP): $(ELF_NO_SYM)
	@$(ECHO) "AppVersion: $(APP_P4_CL). " > $(ELF_OBJDUMP)
	$(OBJDUMP) -D $(ELF_NO_SYM) >> $(ELF_OBJDUMP)

$(ELF_NM): $(ELF_NO_SYM)
	$(ECHO) "AppVersion: $(APP_P4_CL). " > $(ELF_NM)
	$(NM) $(ELF_NO_SYM) >> $(ELF_NM)

#
# Trigger the process by adding a phony prerequisite to 'all' called 'install'.
# The rule to rebuild that target will perform the install process and will copy
# RELEASE_FILES to SEPKERN_INSTALL_DIR if necessary.
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

define PREPROCESS
$(shell $(COPY) $< $@ ;\
        $(CHMOD) +w $@ ;\
        partition=0; \
        while [[ $$partition -lt $(PARTITION_COUNT) ]]; do \
	  $(SED) -i '/TEMPLATE_START/,/TEMPLATE_END/{H; /TEMPLATE_END/G}' $@ ;\
	  $(SED) -i "0,/TEMPLATE_END/ {s/PARTITION_INDEX/$$partition/; /TEMPLATE/d}" $@ ;\
	  ((partition = partition + 1)); \
        done; \
	$(SED) -i '/TEMPLATE_START/,/TEMPLATE_END/d' $@ ;\
	$(SED) -i '0,/COMMA/ s/.*COMMA//g' $@ ;\
	$(SED) -i 's/COMMA/,/g' $@)
endef

$(DUMMY_POLICY_PREBUILD): $(DUMMY_POLICY_TEMPLATE) $(PREBUILD_DIR_CREATED) FORCE
	$(PREPROCESS)

$(SEPKERN_POLICY_PREBUILD): $(SEPKERN_POLICY_TEMPLATE) $(PREBUILD_DIR_CREATED) FORCE
	$(PREPROCESS)

FORCE:

install: $(RELEASE_FILES) $(INSTALL_DIR_CREATED)
	@$(ECHO) [$(BUILD_PROJECT_NAME)]
ifneq ($(SEPKERN_INSTALL_DIR), $(OUTPUTDIR))
	$(COPY) -r $(RELEASE_FILES) $(SEPKERN_INSTALL_DIR)
endif

clean_all:
	$(RMDIR) obj
	$(RMDIR) _out

clean:
	$(RMDIR) $(SPARK_OBJ_DIR)
	$(RMDIR) $(OUTPUTDIR)

prove: $(RELEASE_FILES)
	$(info ===============================================================================)
	$(info =========================      Separation Kernel      =========================)
	$(info SK path:         $(SEPKERN_SW))
	$(info Partition Count: $(PARTITION_COUNT))
	$(info Policy Path:     $(POLICY_PATH))
	$(info Policy File:     $(POLICY_FILE_NAME))
	$(info PLM List Path:   $(PLM_LIST_PATH))
	$(info PLM List File:   $(PLM_LIST_FILE_NAME))
	$(info Chip:            $(CHIP))
	$(info Engine:          $(ENGINE))
	$(info ===============================================================================)
	$(GNAT_PROVE) -P $(GPR_NAME) -j0 -XLOADER=USER -XEngine=$(ENGINE) -XChip=$(CHIP) --level=2 --ide-progress-bar --timeout=32 --mode=all --prover=z3 --checks-as-errors

examine: $(RELEASE_FILES)
	$(GNAT_PROVE) -P $(GPR_NAME) -j0 -XLOADER=USER --mode=flow --ide-progress-bar

stackcheck: $(RELEASE_FILES)
	$(GNATSTACK) -v -P $(GPR_NAME) -l 1 -XLOADER=USER -XEngine=$(ENGINE) -XChip=$(CHIP)
