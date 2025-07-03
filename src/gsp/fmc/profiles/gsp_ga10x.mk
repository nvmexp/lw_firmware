# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/ga10x.mk
# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_s.mk
# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/gsp.mk

# Unique profile so (seemingly) the same configurations don't overlap
# TODO rename to corresponding hdcp profile or fork new one for ga10x.
LIBLWRISCV_PROFILE := mp-sk-test

# we may run with fb locked, pretend FBIF is dead
LWRISCV_WAR_FBIF_IS_DEAD := y

# Enable most of the prints
LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y

# Select available lwstatus type. Possible types: lwrv (default), lwstatus, fsp, ...
LWRISCV_CONFIG_USE_STATUS := lwstatus

# Enable DIO for selwreBus access
LWRISCV_FEATURE_DIO := y
