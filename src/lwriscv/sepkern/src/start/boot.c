/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   boot.c
 * @brief  Entry point for Separation kernel.
 */

/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
#include <lwtypes.h>
#include "config_ls.h"
#include "sepkern.h"

#if defined(PMU_RTOS) && !defined(LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ1)
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ1             LW_PRGNLCL_RISCV_IRQDELEG_EXT_CTXE
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ1_RISCV_MEIP  LW_PRGNLCL_RISCV_IRQDELEG_EXT_CTXE_RISCV_MEIP
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ1_RISCV_SEIP  LW_PRGNLCL_RISCV_IRQDELEG_EXT_CTXE_RISCV_SEIP
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ2             LW_PRGNLCL_RISCV_IRQDELEG_EXT_LIMITV
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ2_RISCV_MEIP  LW_PRGNLCL_RISCV_IRQDELEG_EXT_LIMITV_RISCV_MEIP
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ2_RISCV_SEIP  LW_PRGNLCL_RISCV_IRQDELEG_EXT_LIMITV_RISCV_SEIP
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ3             LW_PRGNLCL_RISCV_IRQDELEG_EXT_ECC
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ3_RISCV_MEIP  LW_PRGNLCL_RISCV_IRQDELEG_EXT_ECC_RISCV_MEIP
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ3_RISCV_SEIP  LW_PRGNLCL_RISCV_IRQDELEG_EXT_ECC_RISCV_SEIP
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ4             LW_PRGNLCL_RISCV_IRQDELEG_EXT_SECOND
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ4_RISCV_MEIP  LW_PRGNLCL_RISCV_IRQDELEG_EXT_SECOND_RISCV_MEIP
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ4_RISCV_SEIP  LW_PRGNLCL_RISCV_IRQDELEG_EXT_SECOND_RISCV_SEIP
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ5             LW_PRGNLCL_RISCV_IRQDELEG_EXT_THERM
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ5_RISCV_MEIP  LW_PRGNLCL_RISCV_IRQDELEG_EXT_THERM_RISCV_MEIP
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ5_RISCV_SEIP  LW_PRGNLCL_RISCV_IRQDELEG_EXT_THERM_RISCV_SEIP
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ6             LW_PRGNLCL_RISCV_IRQDELEG_EXT_MISCIO
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ6_RISCV_MEIP  LW_PRGNLCL_RISCV_IRQDELEG_EXT_MISCIO_RISCV_MEIP
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ6_RISCV_SEIP  LW_PRGNLCL_RISCV_IRQDELEG_EXT_MISCIO_RISCV_SEIP
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ7             LW_PRGNLCL_RISCV_IRQDELEG_EXT_RTTIMER
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ7_RISCV_MEIP  LW_PRGNLCL_RISCV_IRQDELEG_EXT_RTTIMER_RISCV_MEIP
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ7_RISCV_SEIP  LW_PRGNLCL_RISCV_IRQDELEG_EXT_RTTIMER_RISCV_SEIP
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ8             LW_PRGNLCL_RISCV_IRQDELEG_EXT_RSVD8
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ8_RISCV_MEIP  LW_PRGNLCL_RISCV_IRQDELEG_EXT_RSVD8_RISCV_MEIP
#define LW_PRGNLCL_RISCV_IRQDELEG_EXT_EXTIRQ8_RISCV_SEIP  LW_PRGNLCL_RISCV_IRQDELEG_EXT_RSVD8_RISCV_SEIP
#endif

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
extern LwU32 __monitorCodeSize;
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
static void _partitionEntrypointSet(void);

/* ------------------------ Public Functions  ------------------------------ */

/*!
 * @brief: Sets up Entry point of S mode partition
 */
static void _partitionEntrypointSet(void)
{
    LwU64 fmcSource = DRF_VAL64(_PRGNLCL_RISCV, _BCR_DMACFG, _TARGET,
                                intioRead(LW_PRGNLCL_RISCV_BCR_DMACFG));
#ifdef USE_GSCID
    LwU64 accessId = DRF_VAL64(_PRGNLCL_RISCV, _BCR_DMACFG_SEC, _GSCID,
                               intioRead(LW_PRGNLCL_RISCV_BCR_DMACFG_SEC));
#else // USE_GSCID
    LwU64 accessId = DRF_VAL64(_PRGNLCL_RISCV, _BCR_DMACFG_SEC, _WPRID,
                               intioRead(LW_PRGNLCL_RISCV_BCR_DMACFG_SEC));
#endif // USE_GSCID
    LwBool bCoherent = LW_FALSE;
    LwU32 fmcLo      = 0;
    LwU32 fmcHi      = 0;

    fmcLo   = intioRead(LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCCODE_LO);
    fmcHi   = intioRead(LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCCODE_HI);

    LwU64 partitionOffset = ((LwU64)fmcHi) << 32 | fmcLo;
    partitionOffset      <<= 8;
    partitionOffset      += (LwU64)(&__monitorCodeSize);

    // Align up to 4K
    partitionOffset = LW_ALIGN_UP(partitionOffset, 0x1000);

    switch (fmcSource)
    {
        case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_LOCAL_FB:
            partitionOffset += LW_RISCV_AMAP_FBGPA_START;
#ifdef USE_GSCID
            // For TFBIF, set bit 41 to select physical streamid
            partitionOffset += (1ull << 41);
#endif // LW_PRGNLCL_TFBIF_REGIONCFG
            break;
#ifdef LW_RISCV_AMAP_SYSGPA_START
        case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_COHERENT_SYSMEM:
            bCoherent = LW_TRUE;
            // fall-thru
        case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_NONCOHERENT_SYSMEM:
            partitionOffset += LW_RISCV_AMAP_SYSGPA_START;
            break;
#endif // LW_RISCV_AMAP_SYSGPA_START
        default:
            // Invalid specification
            shutdown();
            break;
    }

    csr_write(LW_RISCV_CSR_MEPC, partitionOffset);

    // Set bare-mode access attributes (cacheable = 1, wpr + coherent inherited)
    csr_write(LW_RISCV_CSR_MFETCHATTR, DRF_NUM64(_RISCV_CSR, _MFETCHATTR, _CACHEABLE, 1) |
                                       DRF_NUM64(_RISCV_CSR, _MFETCHATTR, _COHERENT, bCoherent) |
                                       DRF_NUM64(_RISCV_CSR, _MFETCHATTR, _WPR, accessId) |
                                       0);
    csr_write(LW_RISCV_CSR_MLDSTATTR, DRF_NUM64(_RISCV_CSR, _MLDSTATTR, _CACHEABLE, 1) |
                                      DRF_NUM64(_RISCV_CSR, _MLDSTATTR, _COHERENT, bCoherent) |
                                      DRF_NUM64(_RISCV_CSR, _MLDSTATTR, _WPR, accessId) |
                                      0);
}

LwU64 partitionBootInitial(void)
{
    //
    // Initialize LS mode
    // TODO: Check for errors and propagate to RM
    selwrityInitLS();

    // As implemented by https://confluence.lwpu.com/display/LW/LWWATCH+Debugging+and+Security+-+GA10X+POR
    // Lock registers should really be programmed by the manifest, but reprogram just in case.
#if ICD_MODE == 2
    // Maximum ICD privilege: Mem(Match core) CSR(Machine) PLM(Match core). Halt in M.
    // Typical use case: Debug
    csr_write(LW_RISCV_CSR_MDBGCTL, DRF_DEF64(_RISCV_CSR, _MDBGCTL, _ICDPL, _USE_LWR) |
                                    DRF_DEF64(_RISCV_CSR, _MDBGCTL, _ICDMEMPRV_OVERRIDE, _FALSE) |
                                    DRF_DEF64(_RISCV_CSR, _MDBGCTL, _ICDCSRPRV_S_OVERRIDE, _FALSE) |
                                    DRF_DEF64(_RISCV_CSR, _MDBGCTL, _MICD_DIS, _FALSE) |
                                    0);
    intioWrite(LW_PRGNLCL_RISCV_DBGCTL, DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_STOP,   _ENABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RUN,    _ENABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_STEP,   _ENABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_J,      _ENABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_EMASK,  _ENABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RREG,   _ENABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_WREG,   _ENABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RDM,    _ENABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_WDM,    _ENABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RSTAT,  _ENABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_IBRKPT, _ENABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RCSR,   _ENABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_WCSR,   _ENABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RPC,    _ENABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RFREG,  _ENABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_WFREG,  _ENABLE) |
                                           0);
    // LW_PRGNLCL_RISCV_DBGCTL_LOCK deliberately not programmed.
#elif ICD_MODE == 1
    // Minimized ICD privilege: Mem(Match core) CSR(Supervisor) PLM(Match core). No halt in M.
    // Typical use case: Production LS
    csr_write(LW_RISCV_CSR_MDBGCTL, DRF_DEF64(_RISCV_CSR, _MDBGCTL, _ICDPL, _USE_LWR) |
                                    DRF_DEF64(_RISCV_CSR, _MDBGCTL, _ICDMEMPRV_OVERRIDE, _FALSE) |
                                    DRF_DEF64(_RISCV_CSR, _MDBGCTL, _ICDCSRPRV_S_OVERRIDE, _TRUE) |
                                    DRF_DEF64(_RISCV_CSR, _MDBGCTL, _MICD_DIS, _TRUE) |
                                    0);
    intioWrite(LW_PRGNLCL_RISCV_DBGCTL, DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_STOP,   _ENABLE ) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RUN,    _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_STEP,   _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_J,      _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_EMASK,  _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RREG,   _ENABLE ) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_WREG,   _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RDM,    _ENABLE ) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_WDM,    _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RSTAT,  _ENABLE ) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_IBRKPT, _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RCSR,   _ENABLE ) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_WCSR,   _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RPC,    _ENABLE ) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RFREG,  _ENABLE ) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_WFREG,  _DISABLE) |
                                           0);
    intioWrite(LW_PRGNLCL_RISCV_DBGCTL_LOCK, DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _SINGLE_STEP_MODE, _LOCKED  ) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_STOP,   _UNLOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_RUN,    _LOCKED  ) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_STEP,   _LOCKED  ) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_J,      _LOCKED  ) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_EMASK,  _LOCKED  ) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_RREG,   _UNLOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_WREG,   _LOCKED  ) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_RDM,    _UNLOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_WDM,    _LOCKED  ) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_RSTAT,  _UNLOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_IBRKPT, _LOCKED  ) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_RCSR,   _UNLOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_WCSR,   _LOCKED  ) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_RPC,    _UNLOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_RFREG,  _UNLOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_WFREG,  _LOCKED  ) |
                                                0);
#elif ICD_MODE == 0
    // Lock down ICD to lowest possible privileges.
    // Typical use case: Production HS
    csr_write(LW_RISCV_CSR_MDBGCTL, DRF_DEF64(_RISCV_CSR, _MDBGCTL, _ICDPL, _USE_PL0) |
                                    DRF_DEF64(_RISCV_CSR, _MDBGCTL, _ICDMEMPRV_OVERRIDE, _TRUE) |
                                    DRF_DEF64(_RISCV_CSR, _MDBGCTL, _ICDMEMPRV, _U) |
                                    DRF_DEF64(_RISCV_CSR, _MDBGCTL, _ICDCSRPRV_S_OVERRIDE, _TRUE) |
                                    DRF_DEF64(_RISCV_CSR, _MDBGCTL, _MICD_DIS, _TRUE) |
                                    0);
    intioWrite(LW_PRGNLCL_RISCV_DBGCTL, DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_STOP,   _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RUN,    _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_STEP,   _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_J,      _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_EMASK,  _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RREG,   _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_WREG,   _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RDM,    _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_WDM,    _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RSTAT,  _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_IBRKPT, _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RCSR,   _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_WCSR,   _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RPC,    _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_RFREG,  _DISABLE) |
                                           DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL, _ICD_CMDWL_WFREG,  _DISABLE) |
                                           0);
    intioWrite(LW_PRGNLCL_RISCV_DBGCTL_LOCK, DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _SINGLE_STEP_MODE, _LOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_STOP,   _LOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_RUN,    _LOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_STEP,   _LOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_J,      _LOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_EMASK,  _LOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_RREG,   _LOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_WREG,   _LOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_RDM,    _LOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_WDM,    _LOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_RSTAT,  _LOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_IBRKPT, _LOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_RCSR,   _LOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_WCSR,   _LOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_RPC,    _LOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_RFREG,  _LOCKED) |
                                                DRF_DEF64(_PRGNLCL_RISCV, _DBGCTL_LOCK, _ICD_CMDWL_WFREG,  _LOCKED) |
                                                0);
#else
#error "ICD_MODE must be 0,1,2"
#endif

    // Setup interrupt delegation
    // Apparently timer delegation is required for some reason and I don't understand.
    csr_write(LW_RISCV_CSR_MIDELEG, DRF_DEF64(_RISCV_CSR, _MIDELEG, _SSID, _DELEG) |
                                    DRF_DEF64(_RISCV_CSR, _MIDELEG, _STID, _DELEG) |
                                    DRF_DEF64(_RISCV_CSR, _MIDELEG, _SEID, _DELEG) |
                                    0);

    // Setup exception delegation
    csr_write(LW_RISCV_CSR_MEDELEG, DRF_DEF64(_RISCV_CSR, _MEDELEG, _USS, _DELEG) |
                                    DRF_DEF64(_RISCV_CSR, _MEDELEG, _STORE_PAGE_FAULT, _DELEG) |
                                    DRF_DEF64(_RISCV_CSR, _MEDELEG, _LOAD_PAGE_FAULT, _DELEG) |
                                    DRF_DEF64(_RISCV_CSR, _MEDELEG, _FETCH_PAGE_FAULT, _DELEG) |
                                    DRF_DEF64(_RISCV_CSR, _MEDELEG, _USER_ECALL, _DELEG) |
                                    DRF_DEF64(_RISCV_CSR, _MEDELEG, _FAULT_STORE, _DELEG) |
                                    DRF_DEF64(_RISCV_CSR, _MEDELEG, _MISALIGNED_STORE, _DELEG) |
                                    DRF_DEF64(_RISCV_CSR, _MEDELEG, _FAULT_LOAD, _DELEG) |
                                    DRF_DEF64(_RISCV_CSR, _MEDELEG, _MISALIGNED_LOAD, _DELEG) |
                                    DRF_DEF64(_RISCV_CSR, _MEDELEG, _BREAKPOINT, _DELEG) |
                                    DRF_DEF64(_RISCV_CSR, _MEDELEG, _ILLEGAL_INSTRUCTION, _DELEG) |
                                    DRF_DEF64(_RISCV_CSR, _MEDELEG, _FAULT_FETCH, _DELEG) |
                                    DRF_DEF64(_RISCV_CSR, _MEDELEG, _MISALIGNED_FETCH, _DELEG) |
                                    0);

    // Setup external interrupt delegation
    intioWrite(LW_PRGNLCL_RISCV_IRQDELEG, DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _GPTMR, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _WDTMR, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _MTHD, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _CTXSW, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _HALT, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _EXTERR, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _SWGEN0, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _SWGEN1, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _EXT_EXTIRQ1, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _EXT_EXTIRQ2, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _EXT_EXTIRQ3, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _EXT_EXTIRQ4, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _EXT_EXTIRQ5, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _EXT_EXTIRQ6, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _EXT_EXTIRQ7, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _EXT_EXTIRQ8, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _DMA, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _SHA, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _MEMERR, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _ICD, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _IOPMP, _RISCV_SEIP) |
                                             DRF_DEF64(_PRGNLCL_RISCV, _IRQDELEG, _CORE_MISMATCH, _RISCV_SEIP) |
                                             0);

    // Enable timer CSR, no other performance counters
    csr_write(LW_RISCV_CSR_MCOUNTEREN, DRF_DEF64(_RISCV_CSR, _MCOUNTEREN, _TM, _ENABLE));

    // S-mode may flush d-cache
    csr_write(LW_RISCV_CSR_MMISCOPEN, DRF_DEF64(_RISCV_CSR, _MMISCOPEN, _DCACHEOP, _ENABLE) |
                                      0);

    // Enable all operations (if msysopen exists on this core)
    if (FLD_TEST_DRF_NUM64(_PRGNLCL_RISCV, _LWCONFIG, _SYSOP_CSR_EN, 1,
                           intioRead(LW_PRGNLCL_RISCV_LWCONFIG)))
    {
        csr_write(LW_RISCV_CSR_MSYSOPEN, DRF_DEF64(_RISCV_CSR, _MSYSOPEN, _L2FLHDTY, _ENABLE) |
                                         DRF_DEF64(_RISCV_CSR, _MSYSOPEN, _L2CLNCOMP, _ENABLE) |
                                         DRF_DEF64(_RISCV_CSR, _MSYSOPEN, _L2PEERILW, _ENABLE) |
                                         DRF_DEF64(_RISCV_CSR, _MSYSOPEN, _L2SYSILW, _ENABLE) |
                                         DRF_DEF64(_RISCV_CSR, _MSYSOPEN, _FLUSH, _ENABLE) |
                                         DRF_DEF64(_RISCV_CSR, _MSYSOPEN, _TLBILWOP, _ENABLE) |
                                         DRF_DEF64(_RISCV_CSR, _MSYSOPEN, _TLBILWDATA1, _ENABLE) |
                                         DRF_DEF64(_RISCV_CSR, _MSYSOPEN, _BIND, _ENABLE) |
                                         0);
    }

    // Enable branch prediction (and flush to avoid errors)
    csr_write(LW_RISCV_CSR_MBPCFG, DRF_DEF64(_RISCV_CSR, _MBPCFG, _RAS_EN, _TRUE) |
                                   DRF_DEF64(_RISCV_CSR, _MBPCFG, _RAS_FLUSH, _TRUE) |
                                   DRF_DEF64(_RISCV_CSR, _MBPCFG, _BHT_EN, _TRUE) |
                                   DRF_DEF64(_RISCV_CSR, _MBPCFG, _BHT_FLUSH, _TRUE) |
#if !BTB_DISABLE
                                   DRF_DEF64(_RISCV_CSR, _MBPCFG, _BTB_EN, _TRUE) |
                                   DRF_DEF64(_RISCV_CSR, _MBPCFG, _BTB_FLUSHU, _TRUE) |
                                   DRF_DEF64(_RISCV_CSR, _MBPCFG, _BTB_FLUSHS, _TRUE) |
                                   DRF_DEF64(_RISCV_CSR, _MBPCFG, _BTB_FLUSHM, _TRUE) |
#endif
                                   0);

    // Copy MPLM field to SPLM and UPLM
    unsigned int privilegeMask = DRF_VAL64(_RISCV_CSR, _MSPM, _MPLM, csr_read(LW_RISCV_CSR_MSPM));
    csr_clear(LW_RISCV_CSR_MSPM, DRF_SHIFTMASK64(LW_RISCV_CSR_MSPM_SPLM) |
                                 DRF_SHIFTMASK64(LW_RISCV_CSR_MSPM_UPLM)); // Clear out old S/UPLM
    csr_set(LW_RISCV_CSR_MSPM, DRF_NUM64(_RISCV_CSR, _MSPM, _SPLM, privilegeMask) |
                               DRF_NUM64(_RISCV_CSR, _MSPM, _UPLM, privilegeMask));

    // Set S/URPL to highest privilege enabled
    csr_clear(LW_RISCV_CSR_MRSP, DRF_SHIFTMASK64(LW_RISCV_CSR_MRSP_SRPL) |
                                 DRF_SHIFTMASK64(LW_RISCV_CSR_MRSP_URPL)); // Clear out old S/URPL
    if (privilegeMask & 8)
    {
        csr_set(LW_RISCV_CSR_MRSP, DRF_DEF64(_RISCV_CSR, _MRSP, _SRPL, _LEVEL3) |
                                   DRF_DEF64(_RISCV_CSR, _MRSP, _URPL, _LEVEL3));
    }
    else if (privilegeMask & 4)
    {
        csr_set(LW_RISCV_CSR_MRSP, DRF_DEF64(_RISCV_CSR, _MRSP, _SRPL, _LEVEL2) |
                                   DRF_DEF64(_RISCV_CSR, _MRSP, _URPL, _LEVEL2));
    }
    else if (privilegeMask & 2)
    {
        csr_set(LW_RISCV_CSR_MRSP, DRF_DEF64(_RISCV_CSR, _MRSP, _SRPL, _LEVEL1) |
                                   DRF_DEF64(_RISCV_CSR, _MRSP, _URPL, _LEVEL1));
    }
    else // Level 0 doesn't really need a bit set
    {
        csr_set(LW_RISCV_CSR_MRSP, DRF_DEF64(_RISCV_CSR, _MRSP, _SRPL, _LEVEL0) |
                                   DRF_DEF64(_RISCV_CSR, _MRSP, _URPL, _LEVEL0));
    }

    // Enable timer interrupt only
    csr_write(LW_RISCV_CSR_MIE, DRF_NUM64(_RISCV_CSR, _MIE, _MTIE, 1));

    // Setup mstatus (clear all, except return to S-mode)
    csr_write(LW_RISCV_CSR_MSTATUS, DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPP, _SUPERVISOR));

    // Setup S-mode temporary trap vector
    csr_write(LW_RISCV_CSR_STVEC, halt);

    // Configure FBIF - RM will no longer be able to do so
#if defined(LW_PRGNLCL_FBIF_CTL)
    intioWrite(LW_PRGNLCL_FBIF_CTL,
                 (DRF_DEF(_PRGNLCL_FBIF, _CTL, _ENABLE, _TRUE) |
                  DRF_DEF(_PRGNLCL_FBIF, _CTL, _ALLOW_PHYS_NO_CTX, _ALLOW)));
    intioWrite(LW_PRGNLCL_FBIF_CTL2,
                  LW_PRGNLCL_FBIF_CTL2_NACK_MODE_NACK_AS_ACK);
#elif defined(LW_PRGNLCL_TFBIF_CTL)
    intioWrite(LW_PRGNLCL_TFBIF_CTL,
               DRF_DEF(_PRGNLCL_TFBIF, _CTL, _ENABLE, _TRUE));
#endif // defined(LW_PRGNLCL_FBIF_CTL), defined(LW_PRGNLCL_TFBIF_CTL)

    engineConfigApply();

    // Release lockdown, core is now configured / selwred
    intioWrite(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN,
                   DRF_DEF(_PRGNLCL, _RISCV_BR_PRIV_LOCKDOWN, _LOCK, _UNLOCKED));

    // Initial PC of partition
    _partitionEntrypointSet();
    // Returning transfers exelwtion
    return SBI_VERSION;
}
