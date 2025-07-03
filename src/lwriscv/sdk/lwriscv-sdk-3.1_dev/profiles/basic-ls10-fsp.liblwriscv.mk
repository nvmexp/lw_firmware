# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := basic-ls10-fsp

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/ls10.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_m.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/fsp.mk

# Include basic app template
include $(DIR_SDK_PROFILE)/basic.liblwriscv-template.mk
