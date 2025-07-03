# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := basic-t23x-lwdec-s

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/t234-cpu.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_s.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/lwdec.mk

# Include basic app template
include $(DIR_SDK_PROFILE)/basic.liblwriscv-template.mk

# Use lwstatus as return codes
LWRISCV_CONFIG_USE_STATUS := lwstatus
LWRISCV_FEATURE_MPU := y
