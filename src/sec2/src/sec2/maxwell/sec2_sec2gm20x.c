/*
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_sec2gm20x.c
 * @brief  SEC2 HAL Functions for GM20X
 *
 * SEC2 HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_sec_csb.h"
#include "dev_ram.h"
#include "dev_master.h"
#include "dev_bus.h"
#include "lwostimer.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2sw.h"
#include "main.h"
#include "sec2_objsec2.h"
#include "sec2_cmdmgmt.h"
#include "sec2_bar0_hs.h"
#include "sec2_hs.h"
#include "sec2sw.h"
#include "sec2_objic.h"
#include "sec2_chnmgmt.h"

#include "config/g_sec2_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * This define is used to setup the frequency for GP Timer.
 * By default SEC2 uses RT Timer, so this code is compiled out.
 * This code is used on local builds for manual RT Timer test by disbaling
 * feature SEC2RTTIMER_FOR_OS_TICKS.
 */
#define FREQ_FOR_GP_TIMER 0x608F3D00 // 1620KHz

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */

/*!
 * @brief Early initialization for GM20X chips.
 *
 * Any general initialization code not specific to particular engines should be
 * done here. This function must be called prior to starting the scheduler.
 */
FLCN_STATUS
sec2PreInit_GM20X(void)
{
    LwU32 val;

    // DBGCTL is not accessible on GA10X with boot from HS fusing
#ifndef BOOT_FROM_HS_BUILD
    // Allow RM to read SPRs using ICD for LS mode debug
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_DBGCTL, _ICD_CMDWL_RREG_SPR, _ENABLE);
#endif // ifndef BOOT_FROM_HS_BUILD
    //
    // Initialize the timeout value within which the host has to ack a read or
    // write transaction.
    //
    sec2ProgramBar0Timeout_HAL();

    //
    // Set up the general-purpose timer, but don't start it yet.
    //
    // ***********THIS SHOULD NEVER BE USED IN PRODUCTION***********
    //
    // This should be used in local build for manual RT timer test.
    //
    if (!SEC2CFG_FEATURE_ENABLED(SEC2RTTIMER_FOR_OS_TICKS))
    {
        REG_FLD_WR_DRF_NUM(CSB, _CSEC, _FALCON_GPTMRINT, _VAL, FREQ_FOR_GP_TIMER);
        REG_FLD_WR_DRF_NUM(CSB, _CSEC, _FALCON_GPTMRVAL, _VAL, FREQ_FOR_GP_TIMER);
    }

    // Fill in chip information
    val = REG_RD32(BAR0, LW_PMC_BOOT_42);
    Sec2.chipInfo.arch = DRF_VAL(_PMC, _BOOT_42, _ARCHITECTURE,   val);
    Sec2.chipInfo.impl = DRF_VAL(_PMC, _BOOT_42, _IMPLEMENTATION, val);
    Sec2.chipInfo.majorRev = DRF_VAL(_PMC, _BOOT_42, _MAJOR_REVISION, val);
    Sec2.chipInfo.minorRev = DRF_VAL(_PMC, _BOOT_42, _MINOR_REVISION, val);
    Sec2.chipInfo.netList  = REG_RD32(BAR0, LW_PBUS_EMULATION_REV0);

    // Cache the default privilege level
    Sec2.flcnPrivLevelExtDefault = LW_CSEC_FALCON_SCTL1_EXTLVL_MASK_INIT;
    Sec2.flcnPrivLevelCsbDefault = LW_CSEC_FALCON_SCTL1_CSBLVL_MASK_INIT;

    if (!FLD_TEST_DRF(_CSEC, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED,
                      REG_RD32(CSB, LW_CSEC_SCP_CTL_STAT)))
    {
        Sec2.bDebugMode = LW_TRUE;
    }

    return FLCN_OK;
}

/*!
 * @brief Set the falcon privilege level
 *
 * @param[in]  privLevelExt  falcon EXT privilege level
 * @param[in]  privLevelCsb  falcon CSB privilege level
 */
void
sec2FlcnPrivLevelSet_GM20X
(
    LwU8 privLevelExt,
    LwU8 privLevelCsb
)
{
    LwU32 flcnSctl1 = 0;

    flcnSctl1 = FLD_SET_DRF_NUM(_CSEC, _FALCON, _SCTL1_CSBLVL_MASK, privLevelCsb, flcnSctl1);
    flcnSctl1 = FLD_SET_DRF_NUM(_CSEC, _FALCON, _SCTL1_EXTLVL_MASK, privLevelExt, flcnSctl1);

    REG_WR32_STALL(CSB, LW_CSEC_FALCON_SCTL1, flcnSctl1);
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
sec2Bar0RegWr32NonPosted_GM20X
(
    LwU32  addr,
    LwU32  data
)
{
    lwrtosENTER_CRITICAL();
    {
        _sec2Bar0RegWr32NonPosted_GM20X(addr, data);
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
sec2Bar0RegRd32_GM20X
(
    LwU32  addr
)
{
    LwU32 val;

    lwrtosENTER_CRITICAL();
    {
        val = _sec2Bar0RegRd32_GM20X(addr);
    }
    lwrtosEXIT_CRITICAL();
    return val;
}

/*!
 * HS version of @ref sec2Bar0RegWr32NonPosted_GM20X
 */
void
sec2Bar0RegWr32NonPostedHs_GM20X
(
    LwU32  addr,
    LwU32  data
)
{
    _sec2Bar0RegWr32NonPosted_GM20X(addr, data);
}

/*!
 * HS version of @ref sec2Bar0RegRd32_GM20X
 */
LwU32
sec2Bar0RegRd32Hs_GM20X
(
    LwU32  addr
)
{
    return _sec2Bar0RegRd32_GM20X(addr);
}

/*!
 * @brief Pop one method out of the method FIFO
 *
 * @param[out]  pId      Pointer in which to return mthd ID
 * @param[out]  pData    Pointer in which to return mthd data
 *
 * @return      LW_TRUE  if method was successfully popped
 *              LW_FALSE otherwise (invalid pointers, no methods in FIFO)
 */
LwBool
sec2PopMthd_GM20X
(
    LwU16  *pId,
    LwU32  *pData
)
{
    if (pId == NULL || pData == NULL)
    {
        // Make sure pointers are valid
        return LW_FALSE;
    }

    // Make sure there is a method in the FIFO
    if (sec2IsMthdFifoEmpty_HAL())
    {
        return LW_FALSE;
    }

    *pId = DRF_VAL(_CSEC, _FALCON_MTHDID, _ID,
                   REG_RD32(CSB, LW_CSEC_FALCON_MTHDID));

    *pData = REG_RD32(CSB, LW_CSEC_FALCON_MTHDDATA);

    REG_WR32(CSB, LW_CSEC_FALCON_MTHDPOP,
             DRF_DEF(_CSEC, _FALCON_MTHDPOP, _POP, _TRUE));

    return LW_TRUE;
}

/*!
 * @brief Set the PM trigger start/end
 *
 * @param[in]  bStart  LW_TRUE  => PM trigger start
 *                     LW_FALSE => PM trigger end
 */
void
sec2SetPMTrigger_GM20X
(
    LwBool bStart
)
{
    if (bStart)
    {
        REG_WR32(CSB, LW_CSEC_FALCON_SOFT_PM,
                 DRF_NUM(_CSEC, _FALCON_SOFT_PM, _TRIGGER_START, 1));
    }
    else
    {
        REG_WR32(CSB, LW_CSEC_FALCON_SOFT_PM,
                 DRF_NUM(_CSEC, _FALCON_SOFT_PM, _TRIGGER_END, 1));
    }
}

/*!
 * @brief Checks if the current channel context is valid and optionally returns
 * the entire contents of the current context register.
 *
 * @param[out]  pCtx  Pointer to hold entire ctx register value
 *
 * @return  LW_TRUE  => if current channel context is valid
 *          LW_FALSE => if current channel context is invalid
 */
LwBool
sec2IsLwrrentChannelCtxValid_GM20X
(
    LwU32 *pCtx
)
{
    LwU32 ctx = REG_RD32(CSB, LW_CSEC_FALCON_LWRCTX);

    if (pCtx != NULL)
    {
        // Return the value of ctx only if input pointer is valid
        *pCtx = ctx;
    }

    return (FLD_TEST_DRF(_CSEC, _FALCON_LWRCTX, _CTXVLD, _TRUE, ctx));
}

/*!
 * @brief Checks if the next channel context is valid and optionally returns
 * the entire contents of the next context register.
 *
 * @param[out] pCtx  Pointer to hold entire ctx register value
 *
 * @return  LW_TRUE  => if next channel context is valid
 *          LW_FALSE => if next channel context is invalid
 */
LwBool
sec2IsNextChannelCtxValid_GM20X
(
    LwU32 *pCtx
)
{
    LwU32 ctx = REG_RD32(CSB, LW_CSEC_FALCON_NXTCTX);

    if (pCtx != NULL)
    {
        // Return the value of ctx only if input pointer is valid
        *pCtx = ctx;
    }

    return (FLD_TEST_DRF(_CSEC, _FALCON_NXTCTX, _CTXVLD, _TRUE, ctx));
}

/*!
 * @brief DMA the address to the engine's context state from the instance
 * memory and return it
 *
 * @param[in] ctx   entire ctx (current/next) register value
 *
 * @return Memory address of the engine's state context
 */
LwU64
sec2GetEngStateAddr_GM20X
(
    LwU32  ctx
)
{
    //
    // This stores the offset to the block of memory within the instance block
    // that stores the address to the engine context. We subtract 31 since
    // EXTENT gives us the upper bound of the bit field. The lower bound is bit
    // 12 since pages are 4k aligned, hence even if we switched to using the
    // lower bound, we would still have to subtract 12 from it. Note that this
    // is a bit offset within the instance block, so we shift it right by 3 to
    // get to a byte offset.
    //
    LwU16 engCtxAddrOffs = (DRF_EXTENT(LW_RAMIN_ENGINE_WFI_PTR_LO) - 31) >> 3;
    LwU64 instBlkAddr    = DRF_VAL(_CSEC, _FALCON_LWRCTX, _CTXPTR, ctx);
    RM_FLCN_MEM_DESC memDesc;
    static LwU64 engCtx
        GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT);
    LwU32 maxSize        = sizeof(engCtx);
    LwU8 dmaIdx;

    // Initialize before filling up its bit fields
    memDesc.params = 0;

    //
    // Initialize dmaIdx to default value to keep coverity happy. We will catch
    // cases we don't expect in the switch case below.
    //
    dmaIdx = RM_SEC2_DMAIDX_PHYS_VID_FN0;

    //
    // Instance block is at a 4k aligned address and host drops the lowest 12
    // bits, so left shift to get the actual address.
    //
    instBlkAddr <<= 12;

    //
    // Modify the lower bits of the address with the offset to the engine ctx
    // within the instance block. Assumes that _HI is at (_LO + 4)
    //
    instBlkAddr += engCtxAddrOffs;

    RM_FLCN_U64_PACK(&(memDesc.address), &instBlkAddr);

    dmaIdx = sec2GetPhysDmaIdx_HAL(&Sec2, DRF_VAL(_CSEC, _FALCON_LWRCTX, _CTXTGT, ctx));

    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                     dmaIdx, memDesc.params);

    if (FLCN_OK != dmaRead(&engCtx, &memDesc, 0, maxSize))
    {
        SEC2_BREAKPOINT();
    }

    return engCtx;
}

/*!
 * @brief Get the physical DMA index based on the target specified.
 *
 * @return Physical DMA index
 */
LwU8
sec2GetPhysDmaIdx_GM20X
(
    LwU8 target
)
{
    // Ensure that the context targets match expected.
    ct_assert(LW_RAMIN_ENGINE_WFI_TARGET_LOCAL_MEM ==
              LW_CSEC_FALCON_LWRCTX_CTXTGT_LOCAL_FB);

    ct_assert(LW_RAMIN_ENGINE_WFI_TARGET_SYS_MEM_COHERENT ==
              LW_CSEC_FALCON_LWRCTX_CTXTGT_COHERENT_SYSMEM);

    ct_assert(LW_RAMIN_ENGINE_WFI_TARGET_SYS_MEM_NONCOHERENT ==
              LW_CSEC_FALCON_LWRCTX_CTXTGT_NONCOHERENT_SYSMEM);

    switch (target)
    {
        case LW_RAMIN_ENGINE_WFI_TARGET_LOCAL_MEM:
        {
            return RM_SEC2_DMAIDX_PHYS_VID_FN0;
        }
        case LW_RAMIN_ENGINE_WFI_TARGET_SYS_MEM_COHERENT:
        {
            return RM_SEC2_DMAIDX_PHYS_SYS_COH_FN0;
        }
        case LW_RAMIN_ENGINE_WFI_TARGET_SYS_MEM_NONCOHERENT:
        {
            return RM_SEC2_DMAIDX_PHYS_SYS_NCOH_FN0;
        }
        default:
        {
            //
            // Unknown physical target. As long as host behaves properly, we
            // should never get here.
            //
            SEC2_HALT();

            // We can never get here, but need a return to keep compiler happy.
            return RM_SEC2_DMAIDX_PHYS_VID_FN0;
        }
    }
}

/*!
 * @brief Ack the context save request
 */
void
sec2AckCtxSave_GM20X(void)
{
    // First ilwalidate current context
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_LWRCTX, _CTXVLD, _FALSE);

    // Then ack the save request
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_CTXACK, _SAVE_ACK, _SET);
}

/*!
 * @brief Ack the context restore request
 */
void
sec2AckCtxRestore_GM20X(void)
{
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_CTXACK, _REST_ACK, _SET);
}

/*!
 * @brief Enable the General-purpose timer - note this function
 *        is only included for backwards compatibility to implement
 *        OS tick count when RT timer is not enabled.
 */
void
sec2GptmrOsTicksEnable_GM20X(void)
{
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_GPTMRCTL, _GPTMREN, _ENABLE);
}

/*!
 * @brief Do cleanup that must be done in HS mode.
 * Note that this is an HS function
 */
void
sec2CleanupHs_GM20X(void)
{
    //
    // Check return PC to make sure that it is not in HS
    // This must always be the first statement in HS entry function
    //
    VALIDATE_RETURN_PC_AND_HALT_IF_HS();

    FLCN_STATUS status = FLCN_ERROR;

    OS_SEC_HS_LIB_VALIDATE(libCommonHs, HASH_SAVING_DISABLE);
    if ((status = sec2HsPreCheckCommon_HAL(&Sec2, LW_FALSE)) != FLCN_OK)
    {
        // Halt in HS, functionally it should be ok to halt, as here we are already in sec2 unload path
        sec2WriteStatusToMailbox0HS(status);
        SEC2_HALT();
    }

    // Update the PRIV ERROR PLM to have SEC2 and GSP as its Sources.
    if ((status = sec2UpdatePrivErrorPlm_HAL(&sec2)) != FLCN_OK)
    {
        sec2WriteStatusToMailbox0HS(status);
        SEC2_HALT();
    }

    // Unlock(lower) Sec2 Reset PLM to LS settings(Lvl0/1 locked only)
    if ((status = sec2UpdateResetPrivLevelMasksHS_HAL(&sec2, LW_FALSE)) != FLCN_OK)
    {
        sec2WriteStatusToMailbox0HS(status);
        SEC2_HALT();
    }

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))    
    // Cleanup scratch registers
    if ((status = sec2CleanupScratchHs_HAL(&sec2)) != FLCN_OK)
    {
        sec2WriteStatusToMailbox0HS(status);
        SEC2_HALT();
    }
#endif

    // Stop the SCP RNG
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _SCP_CTL1, _RNG_EN, _DISABLED);

    //
    // Disable big hammer lockdown before returning to LS/NS mode
    // NOTE: We are not using EXIT_HS since we specifically intend to leave
    // RNG disabled in the interest of leavign falcon as close to state at
    // time of entry as possible. Practially, we may be able to just no-op
    // this entire function.
    //
    DISABLE_BIG_HAMMER_LOCKDOWN();
}

/*!
 * @brief Check if the method FIFO is empty
 */
LwBool
sec2IsMthdFifoEmpty_GM20X(void)
{
    if (DRF_VAL(_CSEC, _FALCON_MTHDCOUNT, _COUNT,
                REG_RD32(CSB, LW_CSEC_FALCON_MTHDCOUNT)) == 0x0)
    {
        return LW_TRUE;
    }
    return LW_FALSE;
}

/*!
 * @brief Get the  current falcon privilege level value so that it can be
 * restored later.
 *
 * @param[out]  privLevelExtLwrrent The current Falcon Privilege Level for EXT registers
 * @param[out]  privLevelCsbLwrrent The current Falcon Privilege Level for CSB registers
 */
FLCN_STATUS
sec2GetFlcnPrivLevel_GM20X
(
    LwU8 *privLevelExtLwrrent,
    LwU8 *privLevelCsbLwrrent
)
{
    LwU32       flcnSctl1  = 0;
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Get the current falcon Privilege Level 
    CHECK_FLCN_STATUS(CSB_REG_RD32_ERRCHK(LW_CSEC_FALCON_SCTL1, &flcnSctl1));

    *privLevelExtLwrrent = (LwU8)(DRF_VAL(_CSEC, _FALCON_SCTL1, _EXTLVL_MASK, flcnSctl1));
    *privLevelCsbLwrrent = (LwU8)(DRF_VAL(_CSEC, _FALCON_SCTL1, _CSBLVL_MASK, flcnSctl1));

ErrorExit:
    return flcnStatus;
}

/*
 * @brief Perform Sec2 rc recovery step 8 as mentioned in 
 *        https://confluence.lwpu.com/display/SEC2/SEC2+RC+recovery+sequence
 *
 */
void 
sec2ProcessEngineRcRecoveryCmd_GM20X
(
    void
)
{
    // Clear pending ctxsw requests.
    if (icIsCtxSwIntrPending_HAL())
    {
        //
        // Set the NACK bit. We will unset it later after RM resets the
        // host side of SEC2, which will cause host to deassert the
        // ctxsw request.
        //
        sec2SetHostAckMode_HAL(&Sec2, LW_TRUE);

        // Ack that request 
        sec2SendCtxSwIntrTrivialAck();

    }
    //
    // Switch host idle signal to SW-controlled idle. If the ctxsw FSM
    // isn't back to idle yet (because host hasn't deasserted the ctxsw
    // request, we will continue to report busy to host until it has
    // de-asserted that request. If there is no ctxsw request pending,
    // we will report idle.
    //
    sec2HostIdleProgramIdle_HAL();
}


