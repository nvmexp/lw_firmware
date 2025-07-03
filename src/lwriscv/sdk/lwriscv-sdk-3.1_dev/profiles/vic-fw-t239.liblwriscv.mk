# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := vic-fw-t239

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/t239-cpu.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_m.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/vic.mk

# Don't add start file and exception handler
LWRISCV_FEATURE_START := y
LWRISCV_FEATURE_EXCEPTION := y

LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y

# Use lwstatus as return codes
LWRISCV_CONFIG_USE_STATUS := lwrv
