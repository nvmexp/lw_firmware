# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/gh100.mk
# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_m.mk
# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/fsp.mk

# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := fsp-gh100

# Add start file and exception handler
LWRISCV_FEATURE_START := y
LWRISCV_FEATURE_EXCEPTION := y

# we may run with fb locked, pretend FBIF is dead
LWRISCV_WAR_FBIF_IS_DEAD := y

# We want moar printfs
LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y

# we want to turn on DIO to access SE
LWRISCV_FEATURE_DIO := y
