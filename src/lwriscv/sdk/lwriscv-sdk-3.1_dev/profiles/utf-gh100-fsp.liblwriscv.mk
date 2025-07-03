# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := utf-gh100-fsp

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/gh100.mk
# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_s.mk
# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/fsp.mk

# Overwrite abi/arch to not use fp. FPU is not yet enabled in UTF.
# When it is enabled, we can remove these, switching to lp64f/rv64imfc.
LWRISCV_HAS_ABI := lp64
LWRISCV_HAS_ARCH := rv64im


# Add start file and exception handler
LWRISCV_FEATURE_START := y
LWRISCV_FEATURE_EXCEPTION := y

LWRISCV_FEATURE_MPU := y
LWRISCV_FEATURE_DIO := y

# we may run with fb locked, pretend FBIF is dead
LWRISCV_WAR_FBIF_IS_DEAD := y

# We want moar printfs
LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y


