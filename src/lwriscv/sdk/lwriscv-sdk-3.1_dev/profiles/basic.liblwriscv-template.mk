# Add start file and exception handler
LWRISCV_FEATURE_START := y
LWRISCV_FEATURE_EXCEPTION := y

# we may run with fb locked, pretend FBIF is dead
LWRISCV_WAR_FBIF_IS_DEAD := y

# We want more printfs
LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y

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
