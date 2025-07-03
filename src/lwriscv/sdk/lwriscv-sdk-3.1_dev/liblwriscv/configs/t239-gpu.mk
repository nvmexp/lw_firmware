# Chip configuration, to be included by other configs
LWRISCV_IS_CHIP_FAMILY := t23x
LWRISCV_IS_CHIP        := t239

include $(LIBLWRISCV_CONFIGS)/lwriscv20.mk

LWRISCV_HAS_PRI := y
LWRISCV_HAS_BINARY_PTIMER := n
