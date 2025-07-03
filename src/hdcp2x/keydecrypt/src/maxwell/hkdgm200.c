/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: hkdgm200.c
 */

//
// Includes
//
#include "dev_lwdec_csb.h"
#include "dev_disp.h"
#include "dev_fuse.h"
#if (defined(HKD_PROFILE_TU10X) || defined(HKD_PROFILE_TU11X))
#include "dev_master.h"
#endif
#include "hkd.h"

#ifndef LW_CLWDEC_BAR0_TMOUT_CYCLES__PROD
// This is default for pre-turing. Turing ownward it should be defined.
#define LW_CLWDEC_BAR0_TMOUT_CYCLES__PROD    (0x1000)
#endif

//
// Extern Variables
//
extern LwU8 g_lc128e[];
extern LwU8 g_lc128e_debug[];
extern LwU8 g_lc128[];

/*!
 * Wrapper function for blocking falcon read instruction to halt in case of Priv Error.
 */
LwU32
falc_ldxb_i_with_halt
(
    LwU32 addr
)
{
    LwU32   val           = 0;
    LwU32   csbErrStatVal = 0;

    val = falc_ldxb_i ((LwU32*)(addr), 0);

    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CLWDEC_FALCON_CSBERRSTAT), 0);
    if (FLD_TEST_DRF(_CLWDEC, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        // If CSBERRSTAT is true, halt no matter what register was read.
        falc_stxb_i ((LwU32*)LW_CLWDEC_FALCON_MAILBOX0, 0, HKD_ERROR_CSB_RD_FAILED);
        falc_halt();
    }
    else if (CSB_INTERFACE_BAD_VALUE == (val & CSB_INTERFACE_MASK_VALUE))
    {
        //
        // If we get here, we thought we read a bad value.
        // We are not reading RNG or timer values here.
        //
        falc_stxb_i ((LwU32*)LW_CLWDEC_FALCON_MAILBOX0, 0, HKD_ERROR_CSB_RD_FAILED);
        falc_halt();
    }

    return val;
}

/*!
 * Wrapper function for blocking falcon write instruction to halt in case of Priv Error.
 */
void
falc_stxb_i_with_halt
(
    LwU32 addr,
    LwU32 data
)
{
    LwU32 csbErrStatVal = 0;

    falc_stxb_i ((LwU32*)(addr), 0, data);
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CLWDEC_FALCON_CSBERRSTAT), 0);
    if (FLD_TEST_DRF(_CLWDEC, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        // If CSBERRSTAT is true, halt no matter what register was read.
        falc_stxb_i ((LwU32*)LW_CLWDEC_FALCON_MAILBOX0, 0, HKD_ERROR_CSB_RD_FAILED);
        falc_halt();
    }
}

#ifdef HKD_PROFILE_TU10X
/*
 * @brief Make sure the chip is allowed to decrypt LC128
 */
void hkdEnforceAllowedChipsForTuring(void)
{
    LwU32 chip;
    LwU32 pmcBoot42 = 0;

    pmcBoot42 = hkdReadBar0(LW_PMC_BOOT_42);


    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, pmcBoot42);

    if ((chip != LW_PMC_BOOT_42_CHIP_ID_TU102) &&
        (chip != LW_PMC_BOOT_42_CHIP_ID_TU104) &&
        (chip != LW_PMC_BOOT_42_CHIP_ID_TU106))
    {
        HKD_REG_WR32_STALL(CSB, LW_CLWDEC_FALCON_MAILBOX0, HKD_ERROR_CHIP_NOT_ALLOWED);
        falc_halt();
    }
}
#endif

#ifdef HKD_PROFILE_TU11X
/*
 * @brief Make sure the chip is allowed to decrypt LC128
 */
void hkdEnforceAllowedChipsForTuring(void)
{
    LwU32 chip;
    LwU32 pmcBoot42 = 0;

    pmcBoot42 = hkdReadBar0(LW_PMC_BOOT_42);

    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, pmcBoot42);

    if ((chip != LW_PMC_BOOT_42_CHIP_ID_TU116) &&
        (chip != LW_PMC_BOOT_42_CHIP_ID_TU117))
    {
        HKD_REG_WR32_STALL(CSB, LW_CLWDEC_FALCON_MAILBOX0, HKD_ERROR_CHIP_NOT_ALLOWED);
        falc_halt();
    }
}
#endif

/*!
 * @brief Wait for BAR0 to become idle
 */
void hkdBar0WaitIdle()
{
    LwU32 csr_data;
    LwU32 lwr_status = LW_CLWDEC_BAR0_CSR_STATUS_BUSY;

    //wait for the previous transaction to complete
    while(lwr_status == LW_CLWDEC_BAR0_CSR_STATUS_BUSY) {
        csr_data = HKD_CSB_REG_RD32(LW_CLWDEC_BAR0_CSR);
        lwr_status = DRF_VAL(_CLWDEC,_BAR0_CSR,_STATUS,csr_data);
    }

    if (lwr_status != LW_CLWDEC_BAR0_CSR_STATUS_IDLE)
    {
        HKD_REG_WR32_STALL(CSB, LW_CLWDEC_FALCON_MAILBOX0, HKD_ERROR_BAR0_RD_FAILED);
        falc_halt();
    }
}

/*!
 * @brief Reads BAR0
 */
LwU32 hkdReadBar0(LwU32 addr)
{

    LwU32  val32;

    HKD_CSB_REG_WR32(LW_CLWDEC_BAR0_ADDR, addr);

    HKD_CSB_REG_WR32_STALL(LW_CLWDEC_BAR0_CSR,
        DRF_DEF(_CLWDEC, _BAR0_CSR, _CMD , _READ) |
        DRF_DEF(_CLWDEC, _BAR0_CSR, _TRIG, _TRUE));

    (void)HKD_CSB_REG_RD32(LW_CLWDEC_BAR0_CSR);

    hkdBar0WaitIdle();
    val32 = HKD_CSB_REG_RD32(LW_CLWDEC_BAR0_DATA);

    //
    // If we get here, we thought we read a bad value.
    // We are not reading RNG or timer values here
    //
    if (EXT_INTERFACE_BAD_VALUE == (val32 & EXT_INTERFACE_MASK_VALUE))
    {
        HKD_REG_WR32_STALL(CSB, LW_CLWDEC_FALCON_MAILBOX0, HKD_ERROR_BAR0_RD_FAILED);
        falc_halt();
    }

    return val32;
}

/*!
 * @brief Writes BAR0
 */
LwU32 hkdWriteBar0(LwU32 addr, LwU32 data)
{

    LwU32  val32;

    HKD_CSB_REG_WR32(LW_CLWDEC_BAR0_ADDR, addr);
    HKD_CSB_REG_WR32(LW_CLWDEC_BAR0_DATA, data);

    HKD_CSB_REG_WR32_STALL(LW_CLWDEC_BAR0_CSR,
        DRF_DEF(_CLWDEC, _BAR0_CSR, _CMD , _WRITE) |
        DRF_DEF(_CLWDEC, _BAR0_CSR, _TRIG, _TRUE));

    (void)HKD_CSB_REG_RD32(LW_CLWDEC_BAR0_CSR);

    hkdBar0WaitIdle();
    val32 = HKD_CSB_REG_RD32(LW_CLWDEC_BAR0_DATA);
    return val32;
}

/*!
 * @brief Swap endianness
 */
void hkdSwapEndianness(void* pOutData, const void* pInData, const LwU32 size)
{
    LwU32 i;

    for (i = 0; i < size / 2; i++)
    {
        LwU8 b1 = ((LwU8*)pInData)[i];
        LwU8 b2 = ((LwU8*)pInData)[size - 1 - i];
        ((LwU8*)pOutData)[i]            = b2;
        ((LwU8*)pOutData)[size - 1 - i] = b1;
    }
}

/*!
 * @brief Start HDCP key decryption
 */
#define LW_CLWDEC_SCP_CTL_STAT_DEBUG_MODE_ENABLED 1
void hkdStart()
{
    LwU32 ctrl            = 0;
    LwU32 index           = 0;
    LwU8  *pLc128e        = g_lc128e;
    LwU8  *pLc128         = g_lc128;
    LwU32  hdcpEn         = 0;

    // Set CPUCTL_ALIAS_EN to FALSE
    ctrl = FLD_SET_DRF(_CLWDEC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, 0);
    HKD_REG_WR32_STALL(CSB, LW_CLWDEC_FALCON_CPUCTL, ctrl);

    HKD_REG_WR32_STALL(CSB, LW_CLWDEC_FALCON_MAILBOX0, HKD_START);

    // Configure BAR0 timeouts
    HKD_CSB_REG_WR32(LW_CLWDEC_BAR0_TMOUT, LW_CLWDEC_BAR0_TMOUT_CYCLES__PROD);

    // Check  HDCP_EN fuse, if it is disabled do not decrypt LC128
    hdcpEn = hkdReadBar0(LW_FUSE_OPT_HDCP_EN);
    if (!FLD_TEST_DRF(_FUSE, _OPT_HDCP_EN, _DATA, _YES, hdcpEn))
    {
        HKD_REG_WR32_STALL(CSB, LW_CLWDEC_FALCON_MAILBOX0, HKD_ERROR_HDCP_FUSE_DISABLED);
        falc_halt();
    }

    // Added check only for turing profile as HAL support is not there in LC128 decryption ucode
#if (defined(HKD_PROFILE_TU10X) || defined(HKD_PROFILE_TU11X))
    hkdEnforceAllowedChipsForTuring();
#endif

    // Check if HDCP1X is ready and HDCP2X is not loaded
    ctrl = hkdReadBar0(LW_PDISP_HDCPRIF_CTRL);

    if (FLD_TEST_DRF(_PDISP, _HDCPRIF_CTRL, _2X_KEY_READY, _TRUE, ctrl))
    {
        HKD_REG_WR32_STALL(CSB, LW_CLWDEC_FALCON_MAILBOX0, HKD_ERROR_2X_ALREADY_LOADED);
        falc_halt();
    }

    if (FLD_TEST_DRF(_PDISP, _HDCPRIF_CTRL, _1X_KEY_READY, _FALSE, ctrl)) 
    {
        HKD_REG_WR32_STALL(CSB, LW_CLWDEC_FALCON_MAILBOX0, HKD_ERROR_1X_KEY_NOT_READY);
        falc_halt();
    }

    // Find if running debug mode or prod mode
    ctrl = HKD_REG_RD32_STALL(CSB, LW_CLWDEC_SCP_CTL_STAT);
    if (DRF_VAL(_CLWDEC_SCP, _CTL_STAT, _DEBUG_MODE, ctrl) == LW_CLWDEC_SCP_CTL_STAT_DEBUG_MODE_ENABLED)
    {
        pLc128e = g_lc128e_debug;
    }

    // Decrypt lc128
    hkdDecryptLc128Hs(pLc128e, pLc128);

    ctrl = (DRF_DEF(_PDISP,_HDCPRIF_CTRL,_LOCAL,_ENABLED) |
            DRF_DEF(_PDISP,_HDCPRIF_CTRL,_AUTOINC,_ENABLED) |
            DRF_DEF(_PDISP,_HDCPRIF_CTRL,_WRITE4,_TRIGGER));

    while (index < 4)
    {
        HKD_CSB_REG_WR32(LW_CLWDEC_CTL_HDCP1, 0x0);
        HKD_CSB_REG_WR32(LW_CLWDEC_CTL_HDCP0, *((LwU32 *)&(pLc128[(index * 4) ])));
        hkdWriteBar0(LW_PDISP_HDCPRIF_KEY(0), 0x0);
        hkdWriteBar0(LW_PDISP_HDCPRIF_KEY(1), 0x0);
        hkdWriteBar0(LW_PDISP_HDCPRIF_CTRL, ctrl);
        hkdReadBar0(LW_PDISP_HDCPRIF_CTRL);

        // Scrub LC128
        *((LwU32 *)&(pLc128[(index * 4) ])) = 0;
        *((LwU32 *)&(pLc128[(index * 4) ])) = (0xDEAD0000 + index);

        ++index;
    }

    //
    // Cleanup
    // Reset SCP registers
    //
    falc_scp_xor(SCP_R0, SCP_R0);
    falc_scp_xor(SCP_R1, SCP_R1);
    falc_scp_xor(SCP_R2, SCP_R2);
    falc_scp_xor(SCP_R3, SCP_R3);
    falc_scp_xor(SCP_R4, SCP_R4);
    falc_scp_xor(SCP_R5, SCP_R5);
    falc_scp_xor(SCP_R6, SCP_R6);
    falc_scp_xor(SCP_R7, SCP_R7);
 
    // Check if HDCP2X is ready 
    ctrl = hkdReadBar0(LW_PDISP_HDCPRIF_CTRL);

    if (FLD_TEST_DRF(_PDISP, _HDCPRIF_CTRL, _2X_KEY_READY, _FALSE, ctrl) ||
       FLD_TEST_DRF(_PDISP, _HDCPRIF_CTRL, _FULL, _FALSE, ctrl))
    {
        HKD_REG_WR32_STALL(CSB, LW_CLWDEC_FALCON_MAILBOX0, HKD_ERROR_2X_KEY_NOT_READY);
        falc_halt();
    }

    HKD_REG_WR32_STALL(CSB, LW_CLWDEC_FALCON_MAILBOX0, HKD_PASS);

    falc_halt();
    return;
}

void hkdInitErrorCodeNs(void)
{
    falc_stxb_i ((LwU32*)LW_CLWDEC_FALCON_MAILBOX0, 0, HKD_ERROR_UNKNOWN);
}
