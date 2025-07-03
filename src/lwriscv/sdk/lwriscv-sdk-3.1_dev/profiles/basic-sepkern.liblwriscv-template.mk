# Don't add start file and exception handler
LWRISCV_FEATURE_START := n
LWRISCV_FEATURE_EXCEPTION := n

# we may run with fb locked, pretend FBIF is dead
LWRISCV_WAR_FBIF_IS_DEAD := y

# We want moar printfs
LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y
