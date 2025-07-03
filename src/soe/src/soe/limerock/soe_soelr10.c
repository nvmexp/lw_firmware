/*
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soelr10.c
 * @brief  SOE HAL Functions for LR10 and later chips
 *
 * SOE HAL functions implement chip-specific initialization, helper functions
 */
/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"
#include "soe_bar0.h"
#include "dev_soe_csb.h"
#include "dev_lws_master_addendum.h"
#include "dev_nport_ip.h"
#include "dev_route_ip.h"
#include "dev_ingress_ip.h"
#include "dev_egress_ip.h"
#include "dev_tstate_ip.h"
#include "dev_sourcetrack_ip.h"
#include "dev_perf.h"
#include "dev_pmgr.h"
#include "dev_lwlsaw_ip.h"
#include "dev_npg_ip.h"
#include "soe_objlwlink.h"
#include "soe_objdiscovery.h"
#include "dev_lwlipt_lnk_ip.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_soe_private.h"
#include "config/g_bif_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Early initialization for LR10 chips.
 *
 * Any general initialization code not specific to particular engines should be
 * done here. This function must be called prior to starting the scheduler.
 */
FLCN_STATUS
soePreInit_LR10(void)
{
    LwU32 val;

    // Set up TRACEPC in stack mode
    REG_FLD_WR_DRF_DEF(CSB, _CSOE, _FALCON_DBGCTL, _TRACE_MODE, _STACK);

    //
    // The below bit controls the fbif NACK behavior. When fbif receives a
    // NACK, it will consume and complete this request anyway. It will send
    // write data to FB anyway, and return garbade data (0xdead5ee0) to Falcon
    // for a read request. It will also continue to serve further DMA requests
    // after a NACKed DMA. If this bit is left at its init value, fbif will
    // continue to function in legacy mode.
    //
    REG_FLD_WR_DRF_DEF(CSB, _CSOE, _FBIF_CTL2, _NACK_MODE, _NACK_AS_ACK);

    val = REG_RD32(CSB, LW_CSOE_FALCON_DEBUG1);

    //
    // Ask HW to bypass engine idle checks in the ctxsw FSM. As we're running
    // an RTOS, we will likely never be idle unless we're exelwting the IDLE
    // task. See bug 200139194 for more details.
    //
    val = FLD_SET_DRF_NUM(_CSOE, _FALCON_DEBUG1, _CTXSW_MODE1, 0x1, val);

    // Set the TRACEPC format to compressed to aid debugging
    val = FLD_SET_DRF(_CSOE, _FALCON_DEBUG1, _TRACE_FORMAT, _COMPRESSED, val);

    REG_WR32(CSB, LW_CSOE_FALCON_DEBUG1, val);

#ifdef DMEM_VA_SUPPORTED
    //
    // Initialize DMEM_VA_BOUND. This can be done as soon as the initialization
    // arguments are cached. RM copies those to the top of DMEM, and once we
    // have cached them into our local structure, all of the memory above
    // DMEM_VA_BOUND can be used as the swappable region.
    //
    REG_FLD_WR_DRF_NUM(CSB, _CSOE, _FALCON_DMVACTL, _BOUND,
               PROFILE_DMEM_VA_BOUND);

    // Enable SP to take values above 64kB
    REG_FLD_WR_DRF_DEF(CSB, _CSOE, _FALCON_DMCYA, _LDSTVA_DIS, _FALSE);
#endif

    // Raise the privlevel mask for secure reset and other registers
    soeRaisePrivLevelMasks_HAL();

    // Allow RM to read SPRs using ICD for LS mode debug
    REG_FLD_WR_DRF_DEF(CSB, _CSOE, _FALCON_DBGCTL, _ICD_CMDWL_RREG_SPR, _ENABLE);

    //
    // Initialize the timeout value within which the host has to ack a read or
    // write transaction.
    //
    _soeBar0InitTmout_LR10();

    // Initilize priv ring before touching full BAR0
    soeInitPrivRing_HAL();

    // Fill in chip information
    val = REG_RD32(BAR0, LW_PSMC_BOOT_42);
    Soe.chipInfo.arch = DRF_VAL(_PSMC, _BOOT_42, _ARCHITECTURE,   val);
    Soe.chipInfo.impl = DRF_VAL(_PSMC, _BOOT_42, _IMPLEMENTATION, val);
    Soe.chipInfo.majorRev = DRF_VAL(_PSMC, _BOOT_42, _MAJOR_REVISION, val);
    Soe.chipInfo.minorRev = DRF_VAL(_PSMC, _BOOT_42, _MINOR_REVISION, val);
    // LR10 lwrrently does not have a netlist colweyance method lwrrently
    //Soe.chipInfo.netList = REG_RD32(BAR0, LW_PBUS_EMULATION_REV0);
    Soe.chipInfo.netList = 0; // Si

    // // Cache the default privilege level
    Soe.flcnPrivLevelExtDefault = LW_CSOE_FALCON_SCTL1_EXTLVL_MASK_INIT;
    Soe.flcnPrivLevelCsbDefault = LW_CSOE_FALCON_SCTL1_CSBLVL_MASK_INIT;

    if (!FLD_TEST_DRF(_CSOE, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED,
                      REG_RD32(CSB, LW_CSOE_SCP_CTL_STAT)))
    {
        Soe.bDebugMode = LW_TRUE;
    }

    // Setup privlevel mask for L0 access of registers.
    soeSetupPrivLevelMasks_HAL();

    // Setup PROD Values for LR10 registers. 
    soeSetupProdValues_HAL();

    return FLCN_OK;
}

/*!
 * @brief Initialize the register used by falc_debug.
 */
void
soeFalcDebugInit_LR10(void)
{
    falc_debug_init(LW_CSOE_MAILBOX(3));
}

/*!
 * @brief Enable the EMEM aperture to map EMEM to the top of DMEM VA space
 */
void
soeEnableEmemAperture_LR10(void)
{
    //
    // keep timeout at default (256 cycles) and enable DMEM Aperature
    // total timeout in cycles := (DMEMAPERT.time_out + 1) * 2^(DMEMAPERT.time_unit + 8)
    //
    LwU32 apertVal = DRF_DEF(_CSOE, _FALCON_DMEMAPERT, _ENABLE, _TRUE);
    REG_WR32(CSB, LW_CSOE_FALCON_DMEMAPERT, apertVal);
}


/*!
 * @brief Returns status of DMA NACK bit, clearing bit if set
 *
 * The DMA NACK bit is set by hardware whenever the feature is enabled and
 * a virtual DMA fault has oclwred.
 *
 * @returns  LW_TRUE  if DMA NACK was received
 *           LW_FALSE if DMA NACK was not received
 */
LwBool
soeDmaNackCheckAndClear_LR10(void)
{
    LwU32 val = REG_RD32(CSB, LW_CSOE_FALCON_DMAINFO_CTL);

    if (FLD_TEST_DRF(_CSOE, _FALCON_DMAINFO_CTL, _INTR_ERR_COMPLETION,
                     _TRUE, val))
    {
        val = FLD_SET_DRF(_CSOE, _FALCON_DMAINFO_CTL, _INTR_ERR_COMPLETION,
                          _CLR, val);
        REG_WR32(CSB, LW_CSOE_FALCON_DMAINFO_CTL, val);
        return LW_TRUE;
    }
    else
    {
        return LW_FALSE;
    }
}

/*
 * @brief Setup the Priv Level Masks for LR10 registers
 * required for L0 access.
 */
void
soeSetupPrivLevelMasks_LR10(void)
{
    LwU32 ucastBaseAddress;
    LwU32 bcastBaseAddress;
    LwU32 data32;

    data32 = REG_RD32_STALL(CSB, LW_CSOE_FALCON_SCTL);
    if (FLD_TEST_DRF(_CSOE, _FALCON_SCTL, _LSMODE, _TRUE, data32))
    {
        //
        // Only adjust priv level masks when we're booted in LS mode
        // so as to avoid soe halt.
        //

        //
        // Setup PRIV LEVEL MASKS for NPORT Engines
        //
        ucastBaseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_NPORT, 0, ADDRESS_UNICAST, 0);
        bcastBaseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_NPORT_MULTICAST, 0, ADDRESS_BROADCAST, 0);

        data32 = REG_RD32(BAR0, ucastBaseAddress + LW_NPORT_PRIV_LEVEL_MASK7);
        data32 = FLD_SET_DRF(_NPORT, _PRIV_LEVEL_MASK7, _WRITE_PROTECTION_LEVEL0, _ENABLE, data32);
        REG_WR32(BAR0, bcastBaseAddress + LW_NPORT_PRIV_LEVEL_MASK7, data32);

        data32 = REG_RD32(BAR0, ucastBaseAddress + LW_ROUTE_PRIV_LEVEL_MASK7);
        data32 = FLD_SET_DRF(_ROUTE, _PRIV_LEVEL_MASK7, _WRITE_PROTECTION_LEVEL0, _ENABLE, data32);
        REG_WR32(BAR0, bcastBaseAddress + LW_ROUTE_PRIV_LEVEL_MASK7, data32);

        data32 = REG_RD32(BAR0, ucastBaseAddress + LW_INGRESS_PRIV_LEVEL_MASK7);
        data32 = FLD_SET_DRF(_INGRESS, _PRIV_LEVEL_MASK7, _WRITE_PROTECTION_LEVEL0, _ENABLE, data32);
        REG_WR32(BAR0, bcastBaseAddress + LW_INGRESS_PRIV_LEVEL_MASK7, data32);

        data32 = REG_RD32(BAR0, ucastBaseAddress + LW_EGRESS_PRIV_LEVEL_MASK7);
        data32 = FLD_SET_DRF(_EGRESS, _PRIV_LEVEL_MASK7, _WRITE_PROTECTION_LEVEL0, _ENABLE, data32);
        REG_WR32(BAR0, bcastBaseAddress + LW_EGRESS_PRIV_LEVEL_MASK7, data32);

        data32 = REG_RD32(BAR0, ucastBaseAddress + LW_TSTATE_PRIV_LEVEL_MASK7);
        data32 = FLD_SET_DRF(_TSTATE, _PRIV_LEVEL_MASK7, _WRITE_PROTECTION_LEVEL0, _ENABLE, data32);
        REG_WR32(BAR0, bcastBaseAddress + LW_TSTATE_PRIV_LEVEL_MASK7, data32);

        data32 = REG_RD32(BAR0, ucastBaseAddress + LW_SOURCETRACK_PRIV_LEVEL_MASK7);
        data32 = FLD_SET_DRF(_SOURCETRACK, _PRIV_LEVEL_MASK7, _WRITE_PROTECTION_LEVEL0, _ENABLE, data32);
        REG_WR32(BAR0, bcastBaseAddress + LW_SOURCETRACK_PRIV_LEVEL_MASK7, data32);

        //
        // Setup PRIV LEVEL MASKS for NXBAR Engines
        //
        soeSetupNxbarPrivLevelMasks_HAL();
    }
}

/*
 * @brief Setup the PROD values for LR10 registers
 * that requires LS privileges.
 */
#if (SOECFG_FEATURE_ENABLED(SOE_ONLY_LR10))
void
soeSetupProdValues_LR10(void)
{
    LwU32 baseAddress;
    LwU32 data32;

    //
    // Only adjust priv level masks when we're booted in LS mode
    // so as to avoid soe halt.
    //

    data32 = REG_RD32_STALL(CSB, LW_CSOE_FALCON_SCTL);
    if (FLD_TEST_DRF(_CSOE, _FALCON_SCTL, _LSMODE, _TRUE, data32))
    {
        //
        // Setup PRODs for NXBAR Engines
        //
        soeSetupNxbarProdValues_HAL();

        //
        // Setup PRODs for NPG Engines
        //
        baseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_NPG, 0, ADDRESS_UNICAST, 0);
        data32 = REG_RD32(BAR0, baseAddress + LW_NPG_CTRL_CLOCK_GATING);
        data32 = FLD_SET_DRF(_NPG, _CTRL_CLOCK_GATING, _CG1_SLCG, __PROD, data32);
        baseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_NPG_BCAST, 0, ADDRESS_BROADCAST, 0);
        REG_WR32(BAR0, baseAddress + LW_NPG_CTRL_CLOCK_GATING, data32);

        //
        // Setup PRODs for SAW
        //
        baseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_SAW, 0, ADDRESS_UNICAST, 0);

        REG_WR32(BAR0, baseAddress + LW_LWLSAW_PCIE_PRI_CLOCK_GATING,
            DRF_DEF(_LWLSAW, _PCIE_PRI_CLOCK_GATING, _CG1_SLCG, __PROD));

        REG_WR32(BAR0, baseAddress + LW_LWLSAW_HOST_PRI_CLOCK_GATING,
            DRF_DEF(_LWLSAW, _HOST_PRI_CLOCK_GATING, _CG1_SLCG, __PROD));

        REG_WR32(BAR0, baseAddress + LW_LWLSAW_RESILIENCY_TMR_CLOCK_GATING,
            DRF_DEF(_LWLSAW, _RESILIENCY_TMR_CLOCK_GATING, _CG1_SLCG, __PROD));

        REG_WR32(BAR0, baseAddress + LW_LWLSAW_CLOCK_EN,
            DRF_DEF(_LWLSAW, _CLOCK_EN, _CLOCK_ENB2, __PROD));

        REG_WR32(BAR0, baseAddress + LW_LWLSAW_STEP_ADJ0,
            DRF_DEF(_LWLSAW, _STEP_ADJ0, _STEP_ADJ0_VAL, __PROD));

        REG_WR32(BAR0, baseAddress + LW_LWLSAW_STEP_ADJ1,
            DRF_DEF(_LWLSAW, _STEP_ADJ1, _STEP_ADJ1_VAL, __PROD));

        REG_WR32(BAR0, baseAddress + LW_LWLSAW_STEP_ADJ2,
            DRF_DEF(_LWLSAW, _STEP_ADJ2, _STEP_ADJ2_VAL, __PROD));

        data32 = REG_RD32(BAR0, baseAddress + LW_LWLSAW_CTRL_CLOCK_GATING);
        data32 = FLD_SET_DRF(_LWLSAW, _CTRL_CLOCK_GATING, _CG1_SLCG, __PROD, data32);
        REG_WR32(BAR0, baseAddress + LW_LWLSAW_CTRL_CLOCK_GATING, data32);

        //
        // Setup PRODs for PMGR
        //
        data32 = REG_RD32(BAR0, LW_PMGR_PAD_MIOA);
        data32 = FLD_SET_DRF(_PMGR, _PAD_MIOA, _PWR, __PROD, data32);
        REG_WR32(BAR0, LW_PMGR_PAD_MIOA, data32);

        data32 = REG_RD32(BAR0, LW_PMGR_PAD_MIOB);
        data32 = FLD_SET_DRF(_PMGR, _PAD_MIOB, _PWR, __PROD, data32);
        REG_WR32(BAR0, LW_PMGR_PAD_MIOB, data32);

        //
        // Setup PRODs for XP/XVE
        //
        bifSetupProdValues_HAL();

        //
        // Setup PRODs for SOE Engine
        //
        REG_WR32(CSB, LW_CSOE_PRIV_BLOCKER_CTRL_CG1,
            DRF_DEF(_CSOE, _PRIV_BLOCKER_CTRL_CG1, _SLCG, __PROD));

        //
        // This is temporary and must be moved to VBIOS.
        // Tracking in bugs 3126893 and 3157733
        //
        
    }
}
#endif

/*
 * @brief Raise the privlevel mask for secure reset register so that RM
 *  doesn't interfere with SOE operation until they are lowered again.
 */
void
soeRaisePrivLevelMasks_LR10(void)
{
    LwU32 data32;
    data32 = REG_RD32_STALL(CSB, LW_CSOE_FALCON_SCTL);

    if (FLD_TEST_DRF(_CSOE, _FALCON_SCTL, _LSMODE, _TRUE, data32))
    {    //
        // Only raise priv level masks when we're in booted in LS mode,
        // else we'll take away our own ability to reset the masks when we
        // unload.
        //
        REG_FLD_WR_DRF_DEF_STALL(CSB, _CSOE, _FALCON_RESET_PRIV_LEVEL_MASK,
                         _WRITE_PROTECTION_LEVEL0, _DISABLE);
        REG_FLD_WR_DRF_DEF_STALL(CSB, _CSOE, _FALCON_ENGCTL_PRIV_LEVEL_MASK,
                         _WRITE_PROTECTION_LEVEL0, _DISABLE);
    }
}

/*!
 * @brief Lower the privlevel mask for secure reset register
 * so that RM can perform the reset.
 */
void
soeLowerPrivLevelMasks_LR10(void)
{  //
   // Only lower priv level masks when we're in LS mode,
   // else we'll hit SOE halt due to write access error.
   //
    LwU32 data32;
    data32 = REG_RD32_STALL(CSB, LW_CSOE_FALCON_SCTL);

    if (FLD_TEST_DRF(_CSOE, _FALCON_SCTL, _LSMODE, _TRUE, data32))
    {
        REG_FLD_WR_DRF_DEF_STALL(CSB, _CSOE, _FALCON_RESET_PRIV_LEVEL_MASK,
                         _WRITE_PROTECTION_LEVEL0, _ENABLE);
        REG_FLD_WR_DRF_DEF_STALL(CSB, _CSOE, _FALCON_ENGCTL_PRIV_LEVEL_MASK,
                         _WRITE_PROTECTION_LEVEL0, _ENABLE);
    }
}

/*!
 * @brief Set the falcon privilege level
 *
 * @param[in]  privLevelExt  falcon EXT privilege level
 * @param[in]  privLevelCsb  falcon CSB privilege level
 */
void
soeFlcnPrivLevelSet_LR10
(
    LwU8 privLevelExt,
    LwU8 privLevelCsb
)
{
    LwU32 flcnSctl1 = 0;

    flcnSctl1 = FLD_SET_DRF_NUM(_CSOE, _FALCON, _SCTL1_CSBLVL_MASK, privLevelCsb, flcnSctl1);
    flcnSctl1 = FLD_SET_DRF_NUM(_CSOE, _FALCON, _SCTL1_EXTLVL_MASK, privLevelExt, flcnSctl1);

    REG_WR32_STALL(CSB, LW_CSOE_FALCON_SCTL1, flcnSctl1);
}

/*!
 * Writes a 32-bit value to the given BAR0 address.  This is a nonposted write
 * (will wait for transaction to complete).  It is safe to call it repeatedly
 * and in any combination with other BAR0MASTER functions.
 *
 * Note that the actual functionality is implemented inside an inline function
 * to facilitate exposing the inline function to heavy secure code which cannot
 * jump to light secure code without ilwalidating the HS blocks.
 *
 * @param[in]  addr  The BAR0 address to write
 * @param[in]  data  The data to write to the address
 */
void
soeBar0RegWr32NonPosted_LR10
(
    LwU32  addr,
    LwU32  data
)
{
    lwrtosENTER_CRITICAL();
    {
        _soeBar0RegWr32NonPosted_LR10(addr, data);
    }
    lwrtosEXIT_CRITICAL();
}

/*!
 * Reads the given BAR0 address. The read transaction is nonposted (will wait
 * for transaction to complete). It is safe to call it repeatedly and in any
 * combination with other BAR0MASTER functions.
 *
 * Note that the actual functionality is implemented inside an inline function
 * to facilitate exposing the inline function to heavy secure code which cannot
 * jump to light secure code without ilwalidating the HS blocks.
 *
 * @param[in]  addr  The BAR0 address to read
 *
 * @return The 32-bit value of the requested BAR0 address
 */
LwU32
soeBar0RegRd32_LR10
(
    LwU32  addr
)
{
    LwU32 val;

    lwrtosENTER_CRITICAL();
    {
        val = _soeBar0RegRd32_LR10(addr);
    }
    lwrtosEXIT_CRITICAL();
    return val;
}

/*!
 * @brief Enable the General-purpose timer
 */
void
soeGptmrEnable_LR10(void)
{
    REG_FLD_WR_DRF_DEF(CSB, _CSOE, _FALCON_GPTMRCTL, _GPTMREN, _ENABLE);
}

/*
 * @brief Checks if caller is running on the LR10-SOE falcon.
 * @return : LW_TRUE  : caller is on the LR10-SOE 
 *         : LW_FALSE : otherwise.
 */

#if !defined(LW_PSMC_BOOT_42_CHIP_ID_LS10)
#define LW_PSMC_BOOT_42_CHIP_ID_LS10 0x7
#endif

LwBool soeVerifyChip_LR10(void) 
{
    LwU32 falconFamily, falconInstId;
    LwU32 pmcBoot42Val = REG_RD32(BAR0, LW_PSMC_BOOT_42);

    // Make sure this is running on LR10.
    if ((LW_PSMC_BOOT_42_CHIP_ID_LR10 != DRF_VAL(_PSMC, _BOOT_42, _CHIP_ID, pmcBoot42Val)) &&
        (LW_PSMC_BOOT_42_CHIP_ID_LS10 != DRF_VAL(_PSMC, _BOOT_42, _CHIP_ID, pmcBoot42Val)))
    {
        return LW_FALSE;
    }

    // Make sure this is running on SOE.
    falconFamily = CSB_REG_RD32(LW_CSOE_FALCON_ENGID);
    falconFamily = DRF_VAL(_CSOE, _FALCON_ENGID, _FAMILYID, falconFamily);
    if (LW_CSOE_FALCON_ENGID_FAMILYID_SOE != falconFamily)
    {
        return LW_FALSE;
    }

    // Make sure this is running on SOE instance 0.
    falconInstId = CSB_REG_RD32(LW_CSOE_FALCON_ENGID);
    falconInstId = DRF_VAL(_CSOE, _FALCON_ENGID, _INSTID, falconInstId);
    if (0 != falconInstId)
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}
