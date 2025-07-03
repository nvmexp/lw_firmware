# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/t234-cpu.mk
# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_s.mk
# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/tsec.mk

# Unique profile so (seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := hdcp-t234-tsec

# Add start file and exception handler
# LWRISCV_FEATURE_START := y
LWRISCV_FEATURE_EXCEPTION := y

# we may run with fb locked, pretend FBIF is dead
LWRISCV_WAR_FBIF_IS_DEAD := y

# Enable most of the prints
LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y

# Select available lwstatus type. Possible types: lwrv (default), lwstatus, fsp, ...
LWRISCV_CONFIG_USE_STATUS := lwstatus

# Enable DIO for selwreBus access
LWRISCV_FEATURE_DIO := y

LWRISCV_TEGRA_BUILD := y
LWRISCV_TSEC_USE_GSCID := y
