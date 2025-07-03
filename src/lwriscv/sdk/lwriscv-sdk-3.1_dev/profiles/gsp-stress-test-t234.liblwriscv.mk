# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := gsp-stress-test-t234

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/t234-gpu.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_m.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/gsp.mk

# Add start file and exception handler
LWRISCV_FEATURE_START := y
LWRISCV_FEATURE_EXCEPTION := y

# we may run with fb locked, pretend FBIF is dead
LWRISCV_WAR_FBIF_IS_DEAD := y

# Enable most of the prints
LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y
LWRISCV_CONFIG_DEBUG_PRINT_USES_SWGEN := y
LWRISCV_CONFIG_DEBUG_PRINT_USES_QUEUES := y

# Use lwstatus as return codes
LWRISCV_CONFIG_USE_STATUS := lwstatus

# we want to turn on DIO to access SE, if available
ifeq ($(LWRISCV_HAS_DIO_SE),y)
    LWRISCV_FEATURE_DIO := y
endif

# we want to turn on extra DIO access, if available
ifeq ($(LWRISCV_HAS_DIO_EXTRA),y)
    LWRISCV_FEATURE_DIO := y
endif

ifeq ($(LWRISCV_HAS_SHA),y)
    LWRISCV_FEATURE_SHA := y
endif

