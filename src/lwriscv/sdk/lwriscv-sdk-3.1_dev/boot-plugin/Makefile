#
# PATH
#
TOOL_PATH       ?= $(LW_TOOLS)/riscv/adacore/gnat_community/linux/target_riscv/20200818

#
# TOOLCHAIN
#
GPR_BUILD       := $(TOOL_PATH)/bin/gprbuild
GPR_CLEAN       := $(TOOL_PATH)/bin/gprclean
GNAT_PROVE      := $(TOOL_PATH)/bin/gnatprove
GNAT_PP         := $(TOOL_PATH)/bin/riscv64-elf-gnatpp
STRIP           := $(TOOL_PATH)/bin/riscv64-elf-strip
RISCV_OBJDUMP   := $(TOOL_PATH)/bin/riscv64-elf-objdump
RISCV_OBJCOPY   := $(TOOL_PATH)/bin/riscv64-elf-objcopy
RISCV_READELF   := $(TOOL_PATH)/bin/riscv64-elf-readelf
RISCV_NM        := $(TOOL_PATH)/bin/riscv64-elf-nm

#
# PROJECT INFO
#
BOOT_PLUGIN_CHIP    ?= gh100
BOOT_PLUGIN_ENGINE  ?= lwdec1
BOOT_PLUGIN_PROFILE_FILE ?= ../profiles/lwdec1-fw-gh100.boot-plugin.mk
include $(BOOT_PLUGIN_PROFILE_FILE)

PROJECT             := rv_boot_plugin
PROJECT_GPR     	:= config/spark/$(PROJECT).gpr
OBJ_PATH            := obj/$(BOOT_PLUGIN_ENGINE)_$(BOOT_PLUGIN_CHIP)

#
# OUTPUT PATH & FILES
# All release files must use $(TARGET_NAME) prefix
#
BOOT_PLUGIN_BUILD_DIR ?= _out/$(BOOT_PLUGIN_ENGINE)_$(BOOT_PLUGIN_CHIP)
TARGET_NAME           := g_$(PROJECT)_$(BOOT_PLUGIN_ENGINE)_$(BOOT_PLUGIN_CHIP)
ELF_FILE              := $(BOOT_PLUGIN_BUILD_DIR)/$(TARGET_NAME).elf
ELF_READELF           := $(BOOT_PLUGIN_BUILD_DIR)/$(TARGET_NAME).readelf
ELF_OBJDUMP           := $(BOOT_PLUGIN_BUILD_DIR)/$(TARGET_NAME).objdump
ELF_OBJDUMP_SRC       := $(BOOT_PLUGIN_BUILD_DIR)/$(TARGET_NAME).objdump.source
ELF_NM                := $(BOOT_PLUGIN_BUILD_DIR)/$(TARGET_NAME).nm
IMG_CODE              := $(BOOT_PLUGIN_BUILD_DIR)/$(TARGET_NAME).text
IMG_DATA              := $(BOOT_PLUGIN_BUILD_DIR)/$(TARGET_NAME).data
IMG_CODE_ENC          := $(BOOT_PLUGIN_BUILD_DIR)/$(TARGET_NAME).text.encrypt.bin
IMG_DATA_ENC          := $(BOOT_PLUGIN_BUILD_DIR)/$(TARGET_NAME).data.encrypt.bin

#
# INSTALL PATH & DEPENDED FILES
#
BOOT_PLUGIN_INSTALL_DIR       ?= release/$(BOOT_PLUGIN_ENGINE)/$(BOOT_PLUGIN_CHIP)
INSTALL_FILES_DEP             :=
INSTALL_FILES_DEP             += $(ELF_FILE)
INSTALL_FILES_DEP             += $(IMG_CODE)
INSTALL_FILES_DEP             += $(IMG_DATA)
INSTALL_FILES_DEP             += $(ELF_READELF)
INSTALL_FILES_DEP             += $(ELF_OBJDUMP)
INSTALL_FILES_DEP             += $(ELF_OBJDUMP_SRC)
INSTALL_FILES_DEP             += $(ELF_NM)

#
# MAKEFILE TARGETS
#
.PHONY: all install build clean

all: install

# Copy release files to install path
# Add changed release files to default changelist
install: $(INSTALL_FILES_DEP) $(BOOT_PLUGIN_INSTALL_DIR)
	cp -p $(INSTALL_FILES_DEP) $(BOOT_PLUGIN_INSTALL_DIR)

build: $(OBJ_PATH)
	$(GPR_BUILD) -P $(PROJECT_GPR) -d --target=riscv64-elf -XLOADER=USER -XEngine=$(BOOT_PLUGIN_ENGINE) -XChip=$(BOOT_PLUGIN_CHIP) -o $(TARGET_NAME)

$(OBJ_PATH):
	mkdir -p $(OBJ_PATH)

$(BOOT_PLUGIN_BUILD_DIR):
	mkdir -p $(BOOT_PLUGIN_BUILD_DIR)

$(BOOT_PLUGIN_INSTALL_DIR):
	mkdir -p $(BOOT_PLUGIN_INSTALL_DIR)

$(OBJ_PATH)/$(TARGET_NAME): build

$(ELF_FILE): $(OBJ_PATH)/$(TARGET_NAME) $(BOOT_PLUGIN_BUILD_DIR)
	cp $(OBJ_PATH)/$(TARGET_NAME) $(ELF_FILE)

$(IMG_DATA): $(ELF_FILE)
	$(RISCV_OBJCOPY) -O binary -j .rv_boot_plugin_data $(ELF_FILE) $(IMG_DATA)

$(IMG_CODE): $(ELF_FILE)
	$(RISCV_OBJCOPY) -O binary -j .rv_boot_plugin_overwritable_code -j .rv_boot_plugin_non_overwritable_code $(ELF_FILE) $(IMG_CODE)

$(ELF_READELF): $(ELF_FILE)
	$(RISCV_READELF) --all -W $(ELF_FILE) > $(ELF_READELF)

$(ELF_OBJDUMP): $(ELF_FILE)
	$(RISCV_OBJDUMP) -d $(ELF_FILE) > $(ELF_OBJDUMP)

$(ELF_OBJDUMP_SRC): $(ELF_FILE)
	$(RISCV_OBJDUMP) -d -S $(ELF_FILE) > $(ELF_OBJDUMP_SRC)

$(ELF_NM): $(ELF_FILE)
	$(RISCV_NM) -S -n $(ELF_FILE) > $(ELF_NM)

clean:
	$(GPR_CLEAN) -P $(PROJECT_GPR)
	rm -rf $(OBJ_PATH)
	rm -rf $(BOOT_PLUGIN_BUILD_DIR)
