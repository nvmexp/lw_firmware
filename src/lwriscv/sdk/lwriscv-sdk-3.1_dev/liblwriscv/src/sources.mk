src-$(LWRISCV_HAS_CSB_OVER_PRI) += io_csb_over_pri.c
src-$(LWRISCV_FEATURE_START) += start.S
src-$(LWRISCV_FEATURE_EXCEPTION) += exception.S
src-$(LWRISCV_FEATURE_SSP) += ssp.c
src-$(LWRISCV_FEATURE_MPU) += mpu.c
src-y += sleep.c


src-y += memutils.c

ifeq ($(LWRISCV_FEATURE_DMA),y)
src-$(LWRISCV_HAS_FBDMA) += dma.c
src-$(LWRISCV_HAS_GDMA) += gdma.c
endif

# Components for the SCP driver.
subdirs-$(LWRISCV_FEATURE_SCP) += scp
src-$(LWRISCV_FEATURE_SCP) += scp/scp_crypt.c
src-$(LWRISCV_FEATURE_SCP) += scp/scp_direct.c
src-$(LWRISCV_FEATURE_SCP) += scp/scp_general.c
src-$(LWRISCV_FEATURE_SCP) += scp/scp_private.c
src-$(LWRISCV_FEATURE_SCP) += scp/scp_rand.c
src-$(LWRISCV_FEATURE_SCP) += scp/scp_sanity.c

# Shutdown code - use SBI if available
ifeq ($(LWRISCV_HAS_SBI),y)
src-y += shutdown_sbi.c
else
src-y += shutdown_m.c
endif

# FBIF/TFBIF driver - use SBI if available
ifeq ($(LWRISCV_FEATURE_FBIF_TFBIF_CFG_VIA_SBI),y)
src-$(LWRISCV_HAS_FBIF) += fbif_sbi.c
src-$(LWRISCV_HAS_TFBIF) += tfbif_sbi.c
else
src-$(LWRISCV_HAS_FBIF) += fbif.c
src-$(LWRISCV_HAS_TFBIF) += tfbif.c
endif

ifneq ($(LWRISCV_CONFIG_DEBUG_PRINT_LEVEL),0)
src-y += print.c
endif

src-$(LWRISCV_FEATURE_DIO) += io_dio.c

# SHA driver code
src-$(LWRISCV_FEATURE_SHA) += sha.c
src-$(LWRISCV_FEATURE_SHA) += sha_hmac.c

# Subdir features (not working atm :( )
subdirs-y += print
