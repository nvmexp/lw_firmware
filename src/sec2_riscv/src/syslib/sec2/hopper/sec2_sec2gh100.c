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
 * @file   sec2_sec2gh100.c
 * @brief  SEC2 HAL Functions for GH100
 *
 * SEC2 HAL functions implement chip-specific initialization, helper functions
 */
/* ------------------------- System Includes -------------------------------- */

#include "lwrtos.h"
#include "dev_sec_csb.h"
#include "dev_ram.h"
#include "engine.h"
#include "class/cl95a1.h"
#include "lwmisc.h"
#include "dev_sec_pri.h"
#include "dev_fuse.h"
#include "dev_master.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "dev_top.h"
/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <lwuproc.h>
#include <rmsec2cmdif.h>

/* ------------------------ Register Includes ------------------------------ */
#include <engine.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <lwrtos.h>
#include <sections.h>

/* ------------------------ RISC-V system library  -------------------------- */
#include <lwriscv/print.h>
#include <syslib/syslib.h>
#include <shlib/syscall.h>
#include "lwos_dma.h"

/* ------------------------ Module Includes -------------------------------- */
#include "config/g_sections_riscv.h"
#include "config/g_sec2_hal.h"
#include "config/g_ic_hal.h"
#include "tasks.h"
#include "sec2utils.h"
#include "sec2_objsec2.h"
#include "sec2_hostif.h"
#include "chnlmgmt.h"


/* ------------------------- External Definitions --------------------------- */
extern LwBool g_isRcRecoveryInProgress;

/* ------------------------- Macros and Defines ----------------------------- */
#define SEC2_CSB_REG_RD_DRF(d,r,f)                       (((csbRead(LW ## d ## r))>>DRF_SHIFT(LW ## d ## r ## f)) & DRF_MASK(LW ## d ## r ## f))
#define SEC2_CSB_REG_WR_DEF(d,r,f,n)                     csbWrite                                                                              \
                                                         (                                                                                     \
                                                            LW##d##r,                                                                          \
                                                            ((csbRead(LW ## d ## r) & ~DRF_SHIFTMASK(LW ## d ## r ## f)) |                     \
                                                            DRF_DEF(d,r,f,n))                                                                  \
                                                          )    

/*!
 * Issue a membar.sys
 */
sysSYSLIB_CODE FLCN_STATUS
sec2FbifFlush_GH100(void)
{
    LwU32 val = 0;
    val = csbRead(LW_CSEC_FBIF_CTL);
    val = (val & ~DRF_SHIFTMASK(LW_CSEC_FBIF_CTL_FLUSH)) | DRF_DEF(_CSEC, _FBIF_CTL, _FLUSH, _SET);
    csbWrite(LW_CSEC_FBIF_CTL, val);

    //
    // Poll for the flush to finish. HW says 1 ms is a reasonable value to use
    // It is the same as our PCIe timeout in LW_PBUS_FB_TIMEOUT.
    //
    if (!SEC2_REG32_POLL_US(LW_CSEC_FBIF_CTL,
                            DRF_SHIFTMASK(LW_CSEC_FBIF_CTL_FLUSH),
                            LW_CSEC_FBIF_CTL_FLUSH_CLEAR,
                            1000, // 1 ms
                            SEC2_REG_POLL_MODE_CSB_EQ))
    {
        //
        // Flush didn't finish. There is really no good action we can take here
        // We'll return an error and let the caller decide what to do.
        //
        return FLCN_ERR_TIMEOUT;
    }
    return FLCN_OK;
}

/*!
 * @brief Program the SW override bit in the host idle signal to idle
 *
 * SEC2 will report idle to host unless there is a method in our FIFO or a
 * ctxsw request pending as long as the bit field remains at this value.
 */
sysSYSLIB_CODE void
sec2HostIdleProgramIdle_GH100(void)
{
    LwU32 data32 = 0;
    data32 = bar0Read(LW_PSEC_FALCON_IDLESTATE);
    data32 = FLD_SET_DRF(_PSEC, _FALCON_IDLESTATE, _ENGINE_BUSY_CYA, _SW_IDLE, data32);
    bar0Write(LW_PSEC_FALCON_IDLESTATE, data32);
}

/*!
 * @brief Program the SW override bit in the host idle signal to busy
 *
 * SEC2 will report busy to host as long as the bit field remains at this
 * value.
 */
sysSYSLIB_CODE void
sec2HostIdleProgramBusy_GH100(void)
{
    LwU32 val = 0;
    val = csbRead(LW_CSEC_FALCON_IDLESTATE);
    val = (val & ~DRF_SHIFTMASK(LW_CSEC_FALCON_IDLESTATE_ENGINE_BUSY_CYA)) | DRF_DEF(_CSEC, _FALCON_IDLESTATE, _ENGINE_BUSY_CYA, _SW_BUSY);
    csbWrite(LW_CSEC_FALCON_IDLESTATE, val);
}

/*!
 * @brief Check if the method FIFO is empty
 */
sysSYSLIB_CODE LwBool
sec2IsMthdFifoEmpty_GH100(void)
{
    if (DRF_VAL(_CSEC, _FALCON_MTHDCOUNT, _COUNT,
                csbRead(LW_CSEC_FALCON_MTHDCOUNT)) == 0x0)
    {
        return LW_TRUE;
    }
    return LW_FALSE;
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
sysSYSLIB_CODE LwBool
sec2PopMthd_GH100
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
    if (sec2IsMthdFifoEmpty_GH100())
    {
        dbgPrintf(LEVEL_ALWAYS, "Method Fifo is empty\n");
        return LW_FALSE;
    }

    *pId = DRF_VAL(_CSEC, _FALCON_MTHDID, _ID,
                   csbRead(LW_CSEC_FALCON_MTHDID));

    *pData = csbRead(LW_CSEC_FALCON_MTHDDATA);

    csbWrite(LW_CSEC_FALCON_MTHDPOP, DRF_DEF(_CSEC, _FALCON_MTHDPOP, _POP, _TRUE));

    return LW_TRUE;
}

/*!
 * @brief Process HOST internal methods unrelated to SEC2 applications or OS.
 *
 * @param [in] mthdId   Method ID
 * @param [in] mthdData Method Data
 *
 * @return FLCN_OK if method handled else FLCN_ERR_NOT_SUPPORTED
 */

//
//TODO-gdhanuskodi: Use ref2h to fetch host_internal ref file into hwref
//
#define LW_PMETHOD_SET_CHANNEL_INFO                       0x00003f00
sysSYSLIB_CODE FLCN_STATUS
sec2ProcessHostInternalMethods_GH100
(
    LwU16 mthdId,
    LwU32 mthdData
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (SEC2_METHOD_ID_TO_METHOD(mthdId))
    {
        case LW_PMETHOD_SET_CHANNEL_INFO:
            //
            // Bug: 1723537 requires ucode to NOP this method
            // Consider handled.
            //
            break;
        default:
            status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}

/*!
 * @brief Get the physical DMA index based on the target specified in the
 * instance block
 *
 * @return Physical DMA index
 */
LwU8
sysSYSLIB_CODE sec2GetEngCtxPhysDmaIdx_GH100
(
    LwU8 target
)
{
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
            //SEC2_HALT();

            // We can never get here, but need a return to keep compiler happy.
            return RM_SEC2_DMAIDX_PHYS_VID_FN0;
        }
    }
}

/*!
 * @brief Return the address and dma IDX to use for a DMA to the instance block.
 *
 * @param[in] ctx   entire ctx (current/next) register value
 * @param[in] bEngState   LW_TRUE => return the address of the engine pointer
 * @param[in] bEngState   LW_TRUE => return the address of the IV
 *
 * @return Address and DMA idx to use for a DMA
 */
sysSYSLIB_CODE void
sec2GetInstBlkOffsetAndDmaIdx_GH100
(
    LwU32  ctx,
    LwBool bEngState,
    LwU64 *pAddress,
    LwU8  *pDmaIdx
)
{
    //
    // SEC2 uses unused subcontext fields in the instance block to store the
    // decrypt IV.
    //
    LwU16 offset = bEngState ?
            ((DRF_EXTENT(LW_RAMIN_ENGINE_WFI_PTR_LO) - 31) >> 3) :
            (DRF_BASE(LW_RAMIN_SC_PAGE_DIR_BASE_TARGET(0)) >> 3);
    LwU64 instBlkAddr = DRF_VAL(_CSEC, _FALCON_LWRCTX, _CTXPTR, ctx);

    //
    // Instance block is at a 4k aligned address and host drops the lowest 12
    // bits, so left shift to get the actual address.
    //
    instBlkAddr <<= 12;

    *pAddress = instBlkAddr + offset;
    *pDmaIdx = sec2GetEngCtxPhysDmaIdx_HAL(pSec2, DRF_VAL(_CSEC, _FALCON_LWRCTX, _CTXTGT, ctx));
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
sysSYSLIB_CODE LwBool
sec2IsLwrrentChannelCtxValid_GH100
(
    LwU32 *pCtx
)
{
    LwU32 ctx = csbRead(LW_CSEC_FALCON_LWRCTX);
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
sysSYSLIB_CODE LwBool
sec2IsNextChannelCtxValid_GH100
(
    LwU32 *pCtx
)
{
    LwU32 ctx = csbRead(LW_CSEC_FALCON_NXTCTX);
    if (pCtx != NULL)
    {
        // Return the value of ctx only if input pointer is valid
        *pCtx = ctx;
    }
    return (FLD_TEST_DRF(_CSEC, _FALCON_NXTCTX, _CTXVLD, _TRUE, ctx));
}

/*!
 * @brief Ack the context restore request
 */
sysSYSLIB_CODE void
sec2AckCtxRestore_GH100(void)
{
    csbWrite(LW_CSEC_FALCON_CTXACK, DRF_DEF(_CSEC, _FALCON_CTXACK, _REST_ACK, _SET));
}

/*!
 * @brief Ack the context save request
 */
sysSYSLIB_CODE void
sec2AckCtxSave_GH100(void)
{
    // First ilwalidate current context
    csbWrite(LW_CSEC_FALCON_LWRCTX, DRF_DEF(_CSEC, _FALCON_LWRCTX, _CTXVLD, _FALSE));

    // Then ack the save request
    csbWrite(LW_CSEC_FALCON_CTXACK, DRF_DEF(_CSEC, _FALCON_CTXACK, _SAVE_ACK, _SET));
}


/*!
 * @brief Fetch the next ctx GFID
 */
sysSYSLIB_CODE LwU32
sec2GetNextCtxGfid_GH100(void)
{
    return SEC2_CSB_REG_RD_DRF(_CSEC, _FALCON_NXTCTX2, _CTXGFID);
}


/*!
 * @brief Lower the privlevel mask for secure reset and other registers
 * so that RM can write into them
 */
sysSYSLIB_CODE void
sec2LowerPrivLevelMasks_GH100(void)
{
    LwU32 regVal = 0;

    regVal = csbRead(ENGINE_REG(_FALCON_RESET_PRIV_LEVEL_MASK));
    regVal = FLD_SET_DRF(_CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE, regVal);
    csbWrite(ENGINE_REG(_FALCON_RESET_PRIV_LEVEL_MASK), regVal);

    regVal = csbRead(ENGINE_REG(_FALCON_ENGCTL_PRIV_LEVEL_MASK));
    regVal = FLD_SET_DRF(_CSEC, _FALCON_ENGCTL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE, regVal);
    csbWrite(ENGINE_REG(_FALCON_ENGCTL_PRIV_LEVEL_MASK), regVal);
}
/*
 * @brief Perform Sec2 rc recovery step 8 as mentioned in
 *        Hopper-187 RFD
 *
 */
sysSYSLIB_CODE void
sec2ProcessEngineRcRecoveryCmd_GH100
(
    void
)
{
    // b) Sets an internal state to do trivial acks for ctxsw requests from now on
    g_isRcRecoveryInProgress = LW_TRUE;

    // c) Switch the host idle signal to _SW_IDLE.
    sec2HostIdleProgramIdle_HAL();

    // d) If a CTXSW request is pending, send trivial ack immediately
    if (icIsCtxSwIntrPending_HAL())
    {
        sec2SendCtxSwIntrTrivialAck();
    }
    // e) Unmasks the _CTXSW interrupt
    icHostIfIntrCtxswUnmask_HAL();
}

/*!
 * @brief Enables or disables the mode that decides whether HW will send an ack
 * to host for ctxsw request
 */
sysSYSLIB_CODE void
sec2SetHostAckMode_GH100
(
    LwBool bEnableNack
)
{

    if (bEnableNack)
    {
        SEC2_CSB_REG_WR_DEF(_CSEC, _FALCON_ITFEN, _CTXSW_NACK, _TRUE);
    }
    else
    {
        SEC2_CSB_REG_WR_DEF( _CSEC, _FALCON_ITFEN, _CTXSW_NACK, _FALSE);
    }
}

/*
 * @brief Get the SW fuse version
 */
#define SEC2_GH100_UCODE_VERSION            (0x0)
#define SEC2_GH100_UCODE_VERSION_DEFAULT    (0x0)

sysKERNEL_CODE FLCN_STATUS
sec2GetSWFuseVersion_GH100
(
    LwU32* pFuseVersion
)
{
    FLCN_STATUS flcnStatus   = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       chip;
    LwU32       platformType;
    
    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, bar0Read(LW_PMC_BOOT_42));
    if(chip == LW_PMC_BOOT_42_CHIP_ID_GH100)
    {
         *pFuseVersion = SEC2_GH100_UCODE_VERSION;
         return FLCN_OK;
    }
    else 
    {
         platformType = DRF_VAL(_PTOP, _PLATFORM, _TYPE, bar0Read(LW_PTOP_PLATFORM)); 
         if((chip == LW_PMC_BOOT_42_CHIP_ID_G000) && (platformType == LW_PTOP_PLATFORM_TYPE_FMODEL))
         {
              return FLCN_OK;
         }
    }
    return flcnStatus;
}

/*
 * @brief Get the HW fpf version
 */
sysKERNEL_CODE FLCN_STATUS
sec2GetHWFpfVersion_GH100
(
    LwU32* pFpfVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       count      = 0;
    LwU32       fpfFuse    = 0;

    if (!pFpfVersion)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    fpfFuse = DRF_VAL(_FUSE, _OPT_FPF_UCODE_SEC2_REV, _DATA, bar0Read(LW_FUSE_OPT_FPF_UCODE_SEC2_REV));
    /*
     * FPF Fuse and SW Ucode version has below binding
     * FPF FUSE      SW Ucode Version
     *   0              0
     *   1              1
     *   3              2
     *   7              3
     * and so on. */
    while (fpfFuse != 0)
    {
        count++;
        fpfFuse >>= 1;
    }
    *pFpfVersion = count;

    return flcnStatus;
}

/*!
 * @brief Check the SW fuse version revocation against HW fpf version
 */
sysKERNEL_CODE FLCN_STATUS
sec2CheckFuseRevocationAgainstHWFpfVersion_GH100
(
    LwU32 fuseVersionSW
)
{
    FLCN_STATUS flcnStatus   = FLCN_ERROR;
    LwU32       fpfVersionHW = 0;

    // Get HW Fpf Version
    CHECK_FLCN_STATUS(sec2GetHWFpfVersion_HAL(&sec2, &fpfVersionHW));

    //
    // Compare SW fuse version against HW fpf version.
    // If SW fuse Version is lower return error.
    //
    if (fuseVersionSW < fpfVersionHW)
    {
        return FLCN_ERR_HS_CHK_UCODE_REVOKED;
    }
ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Check for LS revocation by comparing LS ucode version against SEC2 rev in HW fuse
 *
 * @return FLCN_OK  if HW fuse version and LS ucode version matches
 *         FLCN_ERR_LS_CHK_UCODE_REVOKED  if mismatch
 */
sysKERNEL_CODE FLCN_STATUS
sec2CheckForLSRevocation_GH100(void)
{
    FLCN_STATUS flcnStatus    = FLCN_ERROR;
    LwU32       fuseVersionSW = 0;

    // Get SW Fuse version
    CHECK_FLCN_STATUS(sec2GetSWFuseVersion_HAL(&Sec2, &fuseVersionSW));

    // Compare SW fuse version against HW fpf version
    CHECK_FLCN_STATUS(sec2CheckFuseRevocationAgainstHWFpfVersion_HAL(&sec2, fuseVersionSW));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Check reset PLM and set source isolation, Reset source should be set to SEC2 & PLM to L2 & L3 on entry and GSP on unload.
 *
 * @returns FLCN_OK  
 */
sysSYSLIB_CODE FLCN_STATUS
sec2CheckPlmAndUpdateResetSource_GH100(LwBool bIsolateToSec2)
{
    LwU32 regVal                    = 0;
    LwU32 writeProtectionPLML2L3Val = 0;

    regVal = csbRead(ENGINE_REG(_FALCON_RESET_PRIV_LEVEL_MASK));

    // TODO : Remove this if condition to check all source are enabled in production and remove usage COT_ENABLED flag [Jira ID HOPPER-2159]
#ifdef COT_ENABLED
    if (!FLD_TEST_DRF(_CSEC, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, csbRead(ENGINE_REG(_SCP_CTL_STAT))))
    {
        if(DRF_VAL(_CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_ENABLE, regVal) == LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_SOURCE_ENABLE_ALL_SOURCES_ENABLED)
        {
            return FLCN_OK;
        }
    }
#endif

    if (bIsolateToSec2)
    {
        // TODO : Remove usage NON_PRODUCTION_CODE  flag and make sure COT is enabled in production [Jira ID HOPPER-2159]
        if ((DRF_VAL(_CSEC, _RISCV_BCR_DMACFG_SEC, _WPRID, csbRead(ENGINE_REG(_RISCV_BCR_DMACFG_SEC))) != 0) && 
            (DRF_VAL(_CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_ENABLE, regVal) == (BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP) | BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2))))
        {
            writeProtectionPLML2L3Val = FLD_SET_DRF(_CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE, writeProtectionPLML2L3Val);
            writeProtectionPLML2L3Val = FLD_SET_DRF(_CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL3, _ENABLE, writeProtectionPLML2L3Val);
            if (DRF_VAL(_CSEC,_FALCON_RESET_PRIV_LEVEL_MASK,_WRITE_PROTECTION, regVal) != writeProtectionPLML2L3Val)
            {
                return FLCN_ERR_HS_CHK_ILWALID_LS_PRIV_LEVEL;
            }
            
            regVal = FLD_SET_DRF_NUM(_CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2), regVal);
        }
#ifdef COT_ENABLED
        else
        {
             if (FLD_TEST_DRF(_CSEC, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, csbRead(ENGINE_REG(_SCP_CTL_STAT))))
             {
                 return FLCN_ERR_ILLEGAL_OPERATION;
             }
        }
#endif
    }
    else
    {
        regVal = FLD_SET_DRF_NUM(_CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP), regVal);
    }

    csbWrite(ENGINE_REG(_FALCON_RESET_PRIV_LEVEL_MASK), regVal);
    return FLCN_OK;
}

/*!
 * @brief Check if priv sec is enabled on prod board
 *
 * @return FLCN_ERR_HS_CHK_PRIV_SEC_DISABLED_ON_PROD if priv sec is disable on prod board else FLCN_OK.
 */
sysSYSLIB_CODE FLCN_STATUS
sec2CheckIfPrivSecEnabledOnProd_GH100(void)
{
    if (FLD_TEST_DRF(_CSEC, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, csbRead(ENGINE_REG(_SCP_CTL_STAT))))
    {
        if (FLD_TEST_DRF(_FUSE, _OPT_PRIV_SEC_EN, _DATA, _NO, bar0Read(LW_FUSE_OPT_PRIV_SEC_EN)))
        {
            // Error: Priv sec is not enabled
            return FLCN_ERR_HS_CHK_PRIV_SEC_DISABLED_ON_PROD;
        }
    }
    return FLCN_OK;
}

/*! 
 * @brief Check if HW has resported any CTXSW error
 *
 * @return FLCN_ERR_CTXSW_ERROR_REPORTED if CTXSW_ERROR is detected else FLCN_OK
 */
sysSYSLIB_CODE FLCN_STATUS
sec2CheckIfCtxswErrorReported_GH100(void)
{
    LwU32 ctxsw_error = csbRead(LW_CSEC_FALCON_CTXSW_ERROR);
    if (FLD_TEST_DRF(_CSEC, _FALCON_CTXSW_ERROR, _CTX_LOAD_AFTER_HALT, _TRUE, ctxsw_error) ||
        FLD_TEST_DRF(_CSEC, _FALCON_CTXSW_ERROR, _RESET_BEFORE_CTXILW, _TRUE, ctxsw_error))
    {
        csbWrite(LW_CSEC_FALCON_MAILBOX0, FLCN_ERR_CTXSW_ERROR);
        return FLCN_ERR_CTXSW_ERROR;
    }
    return FLCN_OK;
}


/*!
 * @brief Application entry checks
 *
 * @return FLCN_STATUS
 */
sysSYSLIB_CODE FLCN_STATUS
sec2EntryChecks_GH100(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    // 
    // LS revocation by checking for LS ucode version against SEC2 rev in HW fuse
    //
    CHECK_FLCN_STATUS(sec2CheckForLSRevocation_HAL());
    
    //
    // Ensure that priv sec is enabled on prod board
    //
    CHECK_FLCN_STATUS(sec2CheckIfPrivSecEnabledOnProd_HAL());

    // 
    // Set reset source enable to SEC2 while entry
    //
    CHECK_FLCN_STATUS(sec2CheckPlmAndUpdateResetSource_HAL(&Sec2, LW_TRUE));

ErrorExit:
    return flcnStatus;
}


