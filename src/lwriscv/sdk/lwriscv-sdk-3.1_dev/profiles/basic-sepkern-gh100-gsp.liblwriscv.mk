# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := basic-sepkern-gh100-gsp

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/gh100.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_s.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/gsp.mk

# Include basic-sepkern app template
include $(DIR_SDK_PROFILE)/basic-sepkern.liblwriscv-template.mk
