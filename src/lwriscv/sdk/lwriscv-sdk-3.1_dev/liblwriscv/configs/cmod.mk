LWRISCV_PLATFORM_IS_CMOD := y

LWRISCV_HAS_PRI := n



ifeq ($(LIBLWRISCV_PROFILE),fsp-gh100-cmod)

# CMOD only has 64KB FSP DMEM as of peregrine2.0_cmod_51755913.

# TODO: Remove this when it is updated to 96KB to match real HW (bug 3072648)

LWRISCV_DMEM_SIZE := 0x10000

endif

