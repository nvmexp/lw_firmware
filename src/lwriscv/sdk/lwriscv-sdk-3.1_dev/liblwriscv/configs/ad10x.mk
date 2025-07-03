# Chip configuration, to be included by other configs
LWRISCV_IS_CHIP_FAMILY := ada
LWRISCV_IS_CHIP        := ad102

include $(LIBLWRISCV_CONFIGS)/lwriscv20.mk

LWRISCV_HAS_PRI := y
LWRISCV_HAS_CSB_OVER_PRI := y
LWRISCV_HAS_BINARY_PTIMER := n
