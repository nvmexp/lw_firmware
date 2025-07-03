MONITOR_ROOT        := $(abspath ../)
SEPKERN_SW          := $(MONITOR_ROOT)/sepkern
NO_RELEASE          := true

######################################################################################
# GPRBUILD ###########################################################################

CHIP                ?= gh100
ENGINE              ?= gsp
PARTITION_COUNT     ?= 2
POLICY_PATH         ?= $(SEPKERN_SW)/src/engine/$(ENGINE)/$(CHIP)
POLICY_FILE_NAME    ?= dummy_policies_$(ENGINE)_$(CHIP).ads

# default plm list path set to chip specific sub directory only if it exists
PLM_LIST_PATH       ?= $(shell if [ -d $(SEPKERN_SW)/src/plm/$(CHIP) ]; \
                               then echo $(SEPKERN_SW)/src/plm/$(CHIP); \
                               else echo $(SEPKERN_SW)/src/plm; fi)
PLM_LIST_FILE_NAME  ?= $(shell if [ -f $(PLM_LIST_PATH)/$(ENGINE)_plm_list.ads ]; \
                               then echo $(ENGINE)_plm_list.ads; \
                               else echo default_plm_list.ads; fi)

# profile to source
PROFILE_MPSK_CONFIG ?= $(SEPKERN_SW)/config/profile_$(ENGINE)_$(CHIP).mk

IMEM_START_ADDRESS  := 0x0000000000100000
DMEM_START_ADDRESS  := 0x0000000000180000

IMEM_LIMIT          := 0x1000
DMEM_LIMIT          := 0x2000

GPR_NAME            := separation_kernel.gpr
BUILD_PATH          ?= obj/$(ENGINE)_$(CHIP)
PREBUILD_PATH       ?= $(BUILD_PATH)/prebuild
OUTPUT_PATH         ?= _out/$(ENGINE)_$(CHIP)
INSTALL_PATH        ?= $(OUTPUT_PATH)
SEPKERN_INSTALL_DIR ?= $(INSTALL_PATH)

SPARK_OBJ_DIR       ?= $(BUILD_PATH)
PREBUILD_DIR        ?= $(PREBUILD_PATH)
OUTPUTDIR           ?= $(OUTPUT_PATH)
TARGET_NAME         ?= g_sepkern_$(ENGINE)_$(CHIP)_riscv


###############################################################################
# Define relationship between prebuild templates and prebuild targets
# Build auto generates dummy partition policy and sepkern policy defition
# Based on PARITION_COUNT
###############################################################################
SEPKERN_POLICY_TEMPLATE = $(SEPKERN_SW)/config/separation_kernel-policies.template
SEPKERN_POLICY_PREBUILD = $(PREBUILD_DIR)/separation_kernel-policies.ads
DUMMY_POLICY_TEMPLATE   = $(SEPKERN_SW)/config/dummy_policies.template
DUMMY_POLICY_PREBUILD   = $(PREBUILD_DIR)/dummy_policies_$(ENGINE)_$(CHIP).ads

###############################################################################
# Define the names and locations for the various images that are generated
# during the build-process (IMG_*). Also create the list of files that need
# updated with those images when the install-mode is enabled.
###############################################################################

IMG_TARGET          := $(OUTPUTDIR)/$(TARGET_NAME)
IMG_TARGET_DEBUG    := $(OUTPUTDIR)/$(TARGET_NAME)_debug
ELF_TARGET          := $(IMG_TARGET)

ELF_FILE            := $(ELF_TARGET).elf
ELF_NO_SYM          := $(ELF_TARGET)_nosym.elf
ELF_OBJDUMP         := $(ELF_TARGET).objdump
ELF_NM              := $(ELF_TARGET).nm
ELF_READELF         := $(ELF_TARGET).readelf

IMG_CODE            := $(IMG_TARGET)_image.text
IMG_DATA            := $(IMG_TARGET)_image.data
IMG_DESC_BIN        := $(IMG_TARGET)_desc.bin
IMG_SIGN            := $(IMG_TARGET)_sign.bin
IMG_SIG_H           := $(IMG_TARGET)_sig.h

######################################################################################
# Warning: do *not* use := here, it has to be lazy expanded.

# Those files are needed to actually build image
RELEASE_FILES       =  $(IMG_CODE) $(IMG_DATA)
# And those files are for debugging
RELEASE_FILES       += $(ELF_FILE) $(ELF_NM) $(ELF_OBJDUMP)
RELEASE_FILES       += $(ELF_READELF)

######################################################################################
# TOOLS ##############################################################################

#GNAT_COMMUNITY_PATH ?= $(LW_TOOLS)/riscv/adacore/gnat_community/linux/target_riscv/20200818
GNAT_PRO_PATH       ?= $(LW_TOOLS)/adacore/gnat_pro_ada/linux/gnat_pro_ent_ada/target_riscv/20200209
SPARK_PRO_PATH      ?= $(LW_TOOLS)/adacore/gnat_pro_ada/linux/spark_pro/20200212

GNAT_PATH           ?= $(GNAT_PRO_PATH)

######################################################################################

export IMEM_LIMIT
export DMEM_LIMIT
export IMEM_START_ADDRESS
export DMEM_START_ADDRESS
export PARTITION_COUNT
export POLICY_PATH
export POLICY_FILE_NAME
export BUILD_PATH
export CHIP
export ENGINE
export PLM_LIST_PATH
export PLM_LIST_FILE_NAME
