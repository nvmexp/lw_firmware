/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SBILIB_H
#define SBILIB_H

/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <riscv_sbi.h>

/*
 * This is an SBI library wrapper for driver code. Should not be called from
 * task or shared code.
 * Also, must not have global variables of any kind (it will be shared RO/RX only).
 */

void             sbiShutdown            (void);
SBI_RETURN_VALUE sbiSetTimer            (LwU64 value);

#ifdef LWRISCV_MMODE_REGS_RO
SBI_RETURN_VALUE sbiReleasePrivLockdown (void);
SBI_RETURN_VALUE sbiTraceCtlSet         (LwU64 value);
SBI_RETURN_VALUE sbiTransCfgSet         (LwU64 region, LwU64 value);
SBI_RETURN_VALUE sbiRegionCfgSet        (LwU64 region, LwU64 value);

# define riscvReleasePrivLockdown()         sbiReleasePrivLockdown()
# define riscvTraceCtlSet(val)              sbiTraceCtlSet(val)
# define riscvFbifTransCfgSet(region, val)  sbiTransCfgSet((region), (val))
# define riscvFbifRegionCfgSet(region, val) sbiRegionCfgSet((region), (val))
// TODO: add TFBIF_TRANSCFG and TFBIF_REGIONCFG here
#else // LWRISCV_MMODE_REGS_RO
# define riscvReleasePrivLockdown()         intioWrite(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN,                                  \
                                                        DRF_DEF(_PRGNLCL, _RISCV_BR_PRIV_LOCKDOWN, _LOCK, _UNLOCKED))
# define riscvTraceCtlSet(val)              intioWrite(LW_PRGNLCL_RISCV_TRACECTL, (val))
# define riscvFbifTransCfgSet(region, val)  intioWrite(LW_PRGNLCL_FBIF_TRANSCFG(region), (val))
# define riscvFbifRegionCfgSet(region, val) intioWrite(LW_PRGNLCL_FBIF_REGIONCFG,                                          \
                                                        FLD_IDX_SET_DRF_NUM(_PRGNLCL, _FBIF_REGIONCFG, _T, (region), (val), \
                                                                            intioRead(LW_PRGNLCL_FBIF_REGIONCFG)))
// TODO: add TFBIF_TRANSCFG and TFBIF_REGIONCFG here
#endif // LWRISCV_MMODE_REGS_RO

#define shutdown()                          sbiShutdown()

#endif // SBILIB_H
