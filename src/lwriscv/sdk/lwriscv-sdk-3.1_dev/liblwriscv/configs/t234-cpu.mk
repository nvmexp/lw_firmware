# Chip configuration, to be included by other configs
LWRISCV_IS_CHIP_FAMILY := t23x
LWRISCV_IS_CHIP        := t234

include $(LIBLWRISCV_CONFIGS)/lwriscv20.mk

# Orin SOC engines use TFBIF, iGpu engines use FBIF
LWRISCV_HAS_FBIF := n
LWRISCV_HAS_TFBIF := y
LWRISCV_HAS_BINARY_PTIMER := n
