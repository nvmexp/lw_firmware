 /* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 1993-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "rmflcnbl.h"
#include "lwmisc.h"
#include "hs_boot.h"

#include "lwuproc.h"
#include "hsutils.h"
#include "hs_entry_checks.h"
#include "falcon-intrinsics.h"

// TODO: Uncomment this when lwdec build has required paths or find alternative to this file.
// #include "falcon.h"

// This is needed for siggen
LwU32              g_signature[16]       ATTR_OVLY(".data") ATTR_ALIGNED(16);

LwBool             g_bUnused             ATTR_OVLY(".data") ATTR_ALIGNED(16);

// 
// Adding these macros here since lwdec build is not able to find falcon_spr.h which is included from falcon.h
// This file is from Open/Safe RTOS whose path is not included in lwdec.
//
#define LW_FLCN_IMBLK_BLK_SELWRE                                           26:26
#define LW_FLCN_IMBLK_BLK_SELWRE_NO                                  0x00000000U
#define LW_FLCN_IMBLK_BLK_SELWRE_YES                                 0x00000001U
#define LW_FLCN_IMBLK_BLK_ADDR                                              23:0

#define IMBLK_IS_SELWRE(s)  FLD_TEST_DRF(_FLCN, _IMBLK, _BLK_SELWRE, _YES, (s))
#define IMBLK_TAG_ADDR(b)   DRF_VAL(_FLCN, _IMBLK, _BLK_ADDR, (b))


// Function prototypes

static void hsMainEntry(void)                           ATTR_OVLY(".imem_bootloader");
static void hsSetupRegisters(void)                      ATTR_OVLY(".imem_bootloader");
static void hsDisableBigHammerLockdown(void)            ATTR_OVLY(".imem_bootloader");
static void hsCheckLSModeSetup(void)                    ATTR_OVLY(".imem_bootloader");

GCC_ATTRIB_NO_STACK_PROTECT() ATTR_OVLY(".start")
void start(void)
{
    hsEntryChecks();

    hsMainEntry();
}

static void hsMainEntry(void)
{
    LwU32   blockInfo;
    LwU32   pc;

    if (g_bUnused)  // force usage
    {
        HS_CSB_REG_WR32_INLINE(FALCON_MAILBOX0, 0);
    }

    //
    // We are calling PA 0 (below), so start code of steady state binary must 
    // always be present at PA 0 for this to work.
    //
    falc_imblk(&blockInfo, 0);

    // HS-FMC must jump to non secure block
    if (IMBLK_IS_SELWRE(blockInfo))
    {
        falc_halt();
    }

    pc = (IMBLK_TAG_ADDR(blockInfo));

    // Verify that LS signature verification is done and ucode has LS privilege.
    hsCheckLSModeSetup();

    // Initialize/sanitize protected registers
    hsSetupRegisters();

    // 
    // Reset stack bottom so that stack underflow exception is 
    // not triggered for steady state ucode
    //
    hsStackBottomReset();

    hsDisableBigHammerLockdown();

    // Cast to function pointer and call it
    void (*func)(void) = (void (*)(void))pc;
    (*func)();

    // The call to ucode does not return, so exelwtion is never expected to reach here
    falc_halt();
}

static void hsSetupRegisters(void)
{
    LwU32 regVal;

    //
    // Reg: DBGCTL
    // Set TRACE_MODE to REDUCED.
    // DBGCTL is writable only through HS from local falcon.
    //
    regVal = HS_CSB_REG_RD32_INLINE(FALCON_DBGCTL);
    regVal = HS_FLD_SET_DRF(HS_FALC, _FALCON_DBGCTL, _TRACE_MODE, _REDUCED, regVal);
    HS_CSB_REG_WR32_INLINE(FALCON_DBGCTL, regVal);

    // FALCON_RESET_PRIV_LEVEL_MASK is not present on LWDEC
#ifndef BOOT_FROM_HS_FALCON_LWDEC
    //
    // Reg: RESET_PLM
    // Reset PLM is set to PL3 protected in hsEntryChecks().
    // Allow write access to PL2 so that LS can update it when required.
    //
    regVal = HS_CSB_REG_RD32_INLINE(FALCON_RESET_PRIV_LEVEL_MASK);
    regVal = HS_FLD_SET_DRF( HS_FALC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE, regVal);
    HS_CSB_REG_WR32_INLINE(FALCON_RESET_PRIV_LEVEL_MASK, regVal);
#endif // !BOOT_FROM_HS_FALCON_LWDEC
}

static void hsDisableBigHammerLockdown(void)
{
    LwU32 regVal = HS_CSB_REG_RD32_INLINE(SCP_CTL_CFG);

    regVal = HS_FLD_SET_DRF(HS_FALC, _SCP_CTL_CFG, _LOCKDOWN, _DISABLE, regVal);
    regVal = HS_FLD_SET_DRF(HS_FALC, _SCP_CTL_CFG, _LOCKDOWN_SCP, _DISABLE, regVal);
    HS_CSB_REG_WR32_INLINE(SCP_CTL_CFG, regVal);
}

//
// Make sure that LS signature verification through ACR is done
// and ucode has LS privilege, halt otherwise.
//
static void hsCheckLSModeSetup(void)
{
    LwU32 regVal = HS_CSB_REG_RD32_INLINE(FALCON_SCTL);

    if (HS_FLD_TEST_DRF(HS_FALC, _FALCON_SCTL, _LSMODE, _FALSE, regVal))
    {
        falc_halt();
    }
}

