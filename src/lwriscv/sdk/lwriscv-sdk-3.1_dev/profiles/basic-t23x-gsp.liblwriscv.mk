# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := basic-t23x-gsp

# Chip family we want to compile to
# We want this to run on both t234 and t239.
# Include t239-gpu.mk here because t239 GSP has smaller TCMs.
include $(LIBLWRISCV_CONFIGS)/t239-gpu.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_m.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/gsp.mk

# Include basic app template
include $(DIR_SDK_PROFILE)/basic.liblwriscv-template.mk
