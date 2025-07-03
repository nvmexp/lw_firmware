# Unique profile so (seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := gsp-test-fw-gh100

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/gh100.mk
# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_s.mk
# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/gsp.mk

# we may run with fb locked, pretend FBIF is dead
LWRISCV_WAR_FBIF_IS_DEAD := y

# Enable most of the prints
LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y
