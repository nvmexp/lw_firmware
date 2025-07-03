# Chip configuration, to be included by other configs
LWRISCV_IS_CHIP_FAMILY := t25x
LWRISCV_IS_CHIP        := t254

# SOC engines use TFBIF, iGpu engines use FBIF
LWRISCV_HAS_FBIF := n
LWRISCV_HAS_TFBIF := y
LWRISCV_HAS_BINARY_PTIMER := n
