# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := basic-ga10x-pmu

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/ga10x.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_m.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/pmu.mk

# Include basic app template
include $(DIR_SDK_PROFILE)/basic.liblwriscv-template.mk
