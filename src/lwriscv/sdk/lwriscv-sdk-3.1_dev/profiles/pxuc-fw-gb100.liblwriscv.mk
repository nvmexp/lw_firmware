# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := pxuc-fw-gb100

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/gb100.mk

# Because we have 2 profiles (3.0 and 3.0 lite)
# MK TODO: this likely has to be reworked to handle the same engine on different
# risc-v types
include $(LIBLWRISCV_CONFIGS)/lwriscv30.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_m.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/pxuc.mk

LWRISCV_FEATURE_START := y
LWRISCV_FEATURE_EXCEPTION := n

LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y
LWRISCV_CONFIG_DEBUG_PRINT_USES_SWGEN := y
LWRISCV_CONFIG_DEBUG_PRINT_USES_QUEUES := y

LWRISCV_CONFIG_USE_STATUS := lwrv
