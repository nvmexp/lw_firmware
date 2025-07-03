# Chip configuration, to be included by other configs
LWRISCV_IS_CHIP_FAMILY := hopper
LWRISCV_IS_CHIP        := gh100

include $(LIBLWRISCV_CONFIGS)/lwriscv20.mk

LWRISCV_HAS_PRI := y
LWRISCV_HAS_CSB_MMIO := y
