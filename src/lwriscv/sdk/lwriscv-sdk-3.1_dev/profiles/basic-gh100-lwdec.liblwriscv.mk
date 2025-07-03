# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := basic-gh100-lwdec

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/gh100.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_m.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/lwdec.mk

# Include basic app template
include $(DIR_SDK_PROFILE)/basic.liblwriscv-template.mk

# Use lwstatus as return codes
LWRISCV_CONFIG_USE_STATUS := lwstatus
