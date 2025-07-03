/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    sbilib.c
 * @brief   S-mode Kernel to SK interface functions
 */

/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <sections.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <sbilib/sbilib.h>

// Set mtimecmp
sysKERNEL_CODE SBI_RETURN_VALUE sbiSetTimer(LwU64 value)
{
    return sbicall1(SBI_EXTENSION_SET_TIMER, 0, value);
}

// Shut down the core
__attribute__((noreturn)) sysKERNEL_CODE void sbiShutdown(void)
{
    sbicall0(SBI_EXTENSION_SHUTDOWN, 0);
    __builtin_unreachable();
}

#ifdef LWRISCV_MMODE_REGS_RO

sysKERNEL_CODE SBI_RETURN_VALUE sbiReleasePrivLockdown(void)
{
    return sbicall0(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_RELEASE_PRIV_LOCKDOWN);
}

sysKERNEL_CODE SBI_RETURN_VALUE sbiTraceCtlSet(LwU64 value)
{
    return sbicall1(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_TRACECTL_SET, value);
}

sysKERNEL_CODE SBI_RETURN_VALUE sbiTransCfgSet(LwU64 region, LwU64 value)
{
    return sbicall2(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_FBIF_TRANSCFG_SET, region, value);
}

sysKERNEL_CODE SBI_RETURN_VALUE sbiRegionCfgSet(LwU64 region, LwU64 value)
{
    return sbicall2(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_FBIF_REGIONCFG_SET, region, value);
}

// TODO: add TFBIF_TRANSCFG and TFBIF_REGIONCFG here

#endif // LWRISCV_MMODE_REGS_RO
