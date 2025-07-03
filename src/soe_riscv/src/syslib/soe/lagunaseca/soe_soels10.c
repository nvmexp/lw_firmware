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
 * @file   soe_soels10.c
 * @brief  SOE HAL Functions for LS10
 *
 * SOE HAL functions implement chip-specific initialization, helper functions
 */
/* ------------------------- System Includes -------------------------------- */

#include "lwrtos.h"
#include "dev_soe_csb.h"
#include "engine.h"
#include "class/cl95a1.h"
#include "lwmisc.h"
#include "dev_soe_ip.h"
#include "dev_fuse.h"
#include "dev_master.h"
#include "dev_nport_ip.h"
#include "dev_route_ip.h"
#include "dev_ingress_ip.h"
#include "dev_egress_ip.h"
#include "dev_tstate_ip.h"
#include "dev_sourcetrack_ip.h"
#include "dev_npg_ip.h"
#include "dev_lwlsaw_ip.h"

/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <lwuproc.h>
#include <rmsoecmdif.h>

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
#include "config/g_soe_hal.h"
#include "config/g_ic_hal.h"

#include "tasks.h"
#include "soesw.h"
#include "soe_objsoe.h"
#include "soe_hostif.h"
#include "chnlmgmt.h"
#include "soe_objdiscovery.h"
#include "soe_bar0.h"
/* ------------------------- External Definitions --------------------------- */
extern LwBool g_isRcRecoveryInProgress;

/* ------------------------- Macros and Defines ----------------------------- */
#define SOE_CSB_REG_RD_DRF(d,r,f)                       (((csbRead(LW ## d ## r))>>DRF_SHIFT(LW ## d ## r ## f)) & DRF_MASK(LW ## d ## r ## f))
#define SOE_CSB_REG_WR_DEF(d,r,f,n)                     csbWrite                                                                              \
                                                         (                                                                                     \
                                                            LW##d##r,                                                                          \
                                                            ((csbRead(LW ## d ## r) & ~DRF_SHIFTMASK(LW ## d ## r ## f)) |                     \
                                                            DRF_DEF(d,r,f,n))                                                                  \
                                                          )    

/*!
 * Issue a membar.sys
 */
sysSYSLIB_CODE FLCN_STATUS
soeFbifFlush_LS10(void)
{
    LwU32 val = 0;
    val = csbRead(LW_CSOE_FBIF_CTL);
    val = (val & ~DRF_SHIFTMASK(LW_CSOE_FBIF_CTL_FLUSH)) | DRF_DEF(_CSOE, _FBIF_CTL, _FLUSH, _SET);
    csbWrite(LW_CSOE_FBIF_CTL, val);

    //
    // Poll for the flush to finish. HW says 1 ms is a reasonable value to use
    // It is the same as our PCIe timeout in LW_PBUS_FB_TIMEOUT.
    //
    if (!SOE_REG32_POLL_US(LW_CSOE_FBIF_CTL,
                            DRF_SHIFTMASK(LW_CSOE_FBIF_CTL_FLUSH),
                            LW_CSOE_FBIF_CTL_FLUSH_CLEAR,
                            1000, // 1 ms
                            SOE_REG_POLL_MODE_CSB_EQ))
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
 * SOE will report idle to host unless there is a method in our FIFO or a
 * ctxsw request pending as long as the bit field remains at this value.
 */
sysSYSLIB_CODE void
soeHostIdleProgramIdle_LS10(void)
{
    LwU32 data32 = 0;
    data32 = bar0Read(LW_SOE_FALCON_IDLESTATE);
    data32 = FLD_SET_DRF(_SOE, _FALCON_IDLESTATE, _ENGINE_BUSY_CYA, _SW_IDLE, data32);
    bar0Write(LW_SOE_FALCON_IDLESTATE, data32);
}

/*!
 * @brief Program the SW override bit in the host idle signal to busy
 *
 * SOE will report busy to host as long as the bit field remains at this
 * value.
 */
sysSYSLIB_CODE void
soeHostIdleProgramBusy_LS10(void)
{
    LwU32 val = 0;
    val = csbRead(LW_CSOE_FALCON_IDLESTATE);
    val = (val & ~DRF_SHIFTMASK(LW_CSOE_FALCON_IDLESTATE_ENGINE_BUSY_CYA)) | DRF_DEF(_CSOE, _FALCON_IDLESTATE, _ENGINE_BUSY_CYA, _SW_BUSY);
    csbWrite(LW_CSOE_FALCON_IDLESTATE, val);
}

/*!
 * @brief Check if the method FIFO is empty
 */
sysSYSLIB_CODE LwBool
soeIsMthdFifoEmpty_LS10(void)
{
    if (DRF_VAL(_CSOE, _FALCON_MTHDCOUNT, _COUNT,
                csbRead(LW_CSOE_FALCON_MTHDCOUNT)) == 0x0)
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
soePopMthd_LS10
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
    if (soeIsMthdFifoEmpty_LS10())
    {
        dbgPrintf(LEVEL_ALWAYS, "soePopMthd_LS10 Method Fifo is empty\n");
        return LW_FALSE;
    }

    *pId = DRF_VAL(_CSOE, _FALCON_MTHDID, _ID,
                   csbRead(LW_CSOE_FALCON_MTHDID));

    *pData = csbRead(LW_CSOE_FALCON_MTHDDATA);

    csbWrite(LW_CSOE_FALCON_MTHDPOP, DRF_DEF(_CSOE, _FALCON_MTHDPOP, _POP, _TRUE));

    return LW_TRUE;
}

/*!
 * @brief Process HOST internal methods unrelated to SOE applications or OS.
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
soeProcessHostInternalMethods_LS10
(
    LwU16 mthdId,
    LwU32 mthdData
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (SOE_METHOD_ID_TO_METHOD(mthdId))
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
sysSYSLIB_CODE soeGetEngCtxPhysDmaIdx_LS10
(
    LwU8 target
)
{
        return RM_SOE_DMAIDX_PHYS_VID_FN0;
}

/*!
 * @brief Return the address and dma IDX to use for a DMA to the instance block.
 *
 * @param[in] ctx   entire ctx (current/next) register value
 * @param[in] bEngState   LW_TRUE => return the address of the engine pointer
 * @param[in] bEngState   LW_TRUE => return the address of the IV
 *
 * @return Address and DMA idx to use for a DMA
 *  REMOVE THIS FUNCTION
 */
sysSYSLIB_CODE void
soeGetInstBlkOffsetAndDmaIdx_LS10
(
    LwU32  ctx,
    LwBool bEngState,
    LwU64 *pAddress,
    LwU8  *pDmaIdx
)
{
    //
    // SOE uses unused subcontext fields in the instance block to store the
    // decrypt IV.
    //
    LwU64 instBlkAddr = DRF_VAL(_CSOE, _FALCON_LWRCTX, _CTXPTR, ctx);

    //
    // Instance block is at a 4k aligned address and host drops the lowest 12
    // bits, so left shift to get the actual address.
    //
    instBlkAddr <<= 12;

    *pAddress = instBlkAddr;
    *pDmaIdx = soeGetEngCtxPhysDmaIdx_HAL(pSoe, DRF_VAL(_CSOE, _FALCON_LWRCTX, _CTXTGT, ctx));
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
soeIsLwrrentChannelCtxValid_LS10
(
    LwU32 *pCtx
)
{
    LwU32 ctx = csbRead(LW_CSOE_FALCON_LWRCTX);
    if (pCtx != NULL)
    {
        // Return the value of ctx only if input pointer is valid
        *pCtx = ctx;
    }

    return (FLD_TEST_DRF(_CSOE, _FALCON_LWRCTX, _CTXVLD, _TRUE, ctx));
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
soeIsNextChannelCtxValid_LS10
(
    LwU32 *pCtx
)
{
    LwU32 ctx = csbRead(LW_CSOE_FALCON_NXTCTX);
    if (pCtx != NULL)
    {
        // Return the value of ctx only if input pointer is valid
        *pCtx = ctx;
    }
    return (FLD_TEST_DRF(_CSOE, _FALCON_NXTCTX, _CTXVLD, _TRUE, ctx));
}

/*!
 * @brief Ack the context restore request
 */
sysSYSLIB_CODE void
soeAckCtxRestore_LS10(void)
{
    csbWrite(LW_CSOE_FALCON_CTXACK, DRF_DEF(_CSOE, _FALCON_CTXACK, _REST_ACK, _SET));
}

/*!
 * @brief Ack the context save request
 */
sysSYSLIB_CODE void
soeAckCtxSave_LS10(void)
{
    // First ilwalidate current context
    csbWrite(LW_CSOE_FALCON_LWRCTX, DRF_DEF(_CSOE, _FALCON_LWRCTX, _CTXVLD, _FALSE));

    // Then ack the save request
    csbWrite(LW_CSOE_FALCON_CTXACK, DRF_DEF(_CSOE, _FALCON_CTXACK, _SAVE_ACK, _SET));
}


/*!
 * @brief Fetch the next ctx GFID
 */
sysSYSLIB_CODE LwU32
soeGetNextCtxGfid_LS10(void)
{
    return SOE_CSB_REG_RD_DRF(_CSOE, _FALCON_NXTCTX2, _CTXGFID);
}


/*!
 * @brief Lower the privlevel mask for secure reset and other registers
 * so that RM can write into them
 */
sysSYSLIB_CODE void
soeLowerPrivLevelMasks_LS10(void)
{
    LwU32 regVal = 0;

    regVal = csbRead(ENGINE_REG(_FALCON_RESET_PRIV_LEVEL_MASK));
    regVal = FLD_SET_DRF(_CSOE, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE, regVal);
    csbWrite(ENGINE_REG(_FALCON_RESET_PRIV_LEVEL_MASK), regVal);

    regVal = csbRead(ENGINE_REG(_FALCON_ENGCTL_PRIV_LEVEL_MASK));
    regVal = FLD_SET_DRF(_CSOE, _FALCON_ENGCTL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE, regVal);
    csbWrite(ENGINE_REG(_FALCON_ENGCTL_PRIV_LEVEL_MASK), regVal);
}
/*
 * @brief Perform SOE rc recovery step 8 as mentioned in
 *        Hopper-187 RFD
 *
 */
sysSYSLIB_CODE void
soeProcessEngineRcRecoveryCmd_LS10
(
    void
)
{
    // b) Sets an internal state to do trivial acks for ctxsw requests from now on
    g_isRcRecoveryInProgress = LW_TRUE;

    // c) Switch the host idle signal to _SW_IDLE.
    soeHostIdleProgramIdle_HAL();

    // d) If a CTXSW request is pending, send trivial ack immediately
    if (icIsCtxSwIntrPending_HAL())
    {
        soeSendCtxSwIntrTrivialAck();
    }
    // e) Unmasks the _CTXSW interrupt
    icHostIfIntrCtxswUnmask_HAL();
}

/*!
 * @brief Enables or disables the mode that decides whether HW will send an ack
 * to host for ctxsw request
 */
sysSYSLIB_CODE void
soeSetHostAckMode_LS10
(
    LwBool bEnableNack
)
{

    if (bEnableNack)
    {
        SOE_CSB_REG_WR_DEF(_CSOE, _FALCON_ITFEN, _CTXSW_NACK, _TRUE);
    }
    else
    {
        SOE_CSB_REG_WR_DEF( _CSOE, _FALCON_ITFEN, _CTXSW_NACK, _FALSE);
    }
}

/*
 * @brief Get the SW fuse version
 */
#define SOE_LS10_UCODE_VERSION            (0x0)
#define SOE_LS10_UCODE_VERSION_DEFAULT    (0x0)
#define LW_PMC_BOOT_42_CHIP_ID_LS10    0x00000007 /* R---V */

sysKERNEL_CODE FLCN_STATUS
soeGetSWFuseVersion_LS10
(
    LwU32* pFuseVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       chip       = 0l;

    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, bar0Read(LW_PMC_BOOT_42));
    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_LS10 :
        {
            *pFuseVersion = SOE_LS10_UCODE_VERSION;
            return FLCN_OK;
            break;
        }
        default:
        {
            return FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
            break;
        }
    }
    return flcnStatus;
}

/*
 * @brief Get the HW fpf version
 */
sysKERNEL_CODE FLCN_STATUS
soeGetHWFpfVersion_LS10
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
soeCheckFuseRevocationAgainstHWFpfVersion_LS10
(
    LwU32 fuseVersionSW
)
{
    FLCN_STATUS flcnStatus   = FLCN_ERROR;
    LwU32       fpfVersionHW = 0;

    // Get HW Fpf Version
    CHECK_FLCN_STATUS(soeGetHWFpfVersion_HAL(&soe, &fpfVersionHW));

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
 * @brief Check for LS revocation by comparing LS ucode version against SOE rev in HW fuse
 *
 * @return FLCN_OK  if HW fuse version and LS ucode version matches
 *         FLCN_ERR_LS_CHK_UCODE_REVOKED  if mismatch
 */
sysKERNEL_CODE FLCN_STATUS
soeCheckForLSRevocation_LS10(void)
{
    FLCN_STATUS flcnStatus    = FLCN_ERROR;
    LwU32       fuseVersionSW = 0;

    // Get SW Fuse version
    CHECK_FLCN_STATUS(soeGetSWFuseVersion_HAL(&Soe, &fuseVersionSW));

    // Compare SW fuse version against HW fpf version
    CHECK_FLCN_STATUS(soeCheckFuseRevocationAgainstHWFpfVersion_HAL(&soe, fuseVersionSW));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Set the reset source to SOE or GSP, Reset source should be set to SOE on entry and GSP on unload
 *
 * @returns FLCN_OK  
 */
sysSYSLIB_CODE FLCN_STATUS
soeSetResetSourceIsolation_GH100(LwBool bIsolateToSoe)
{
    LwU32 regVal = 0;
    
    regVal = intioRead(LW_PRGNLCL_FALCON_RESET_PRIV_LEVEL_MASK);
       
    // Early exit as source is isolated to source cpu
    if(REF_VAL(LW_PRGNLCL_FALCON_RESET_PRIV_LEVEL_MASK_SOURCE_ENABLE, regVal) == LW_PRGNLCL_FALCON_RESET_PRIV_LEVEL_MASK_SOURCE_ENABLE_ALL_SOURCES_ENABLED)
    {
        return FLCN_OK;
    }

    if (bIsolateToSoe) 
    {
        regVal = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(2), regVal);
    }
    else
    {
        regVal = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(3), regVal);
    }

    intioWrite(LW_PRGNLCL_FALCON_RESET_PRIV_LEVEL_MASK, regVal);

    return FLCN_OK;
}

/*!
 * @brief Application entry checks
 *
 * @return FLCN_STATUS
 */
sysSYSLIB_CODE FLCN_STATUS
soeEntryChecks_GH100(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    // 
    // LS revocation by checking for LS ucode version against SOE rev in HW fuse
    //
    CHECK_FLCN_STATUS(soeCheckForLSRevocation_HAL());

    // 
    // Set reset source enable to SOE while entry
    //
    CHECK_FLCN_STATUS(soeSetResetSourceIsolation_HAL(&Soe, LW_TRUE));

ErrorExit:
    return flcnStatus;
}

/*
 * @brief Setup the PROD values for LS10 registers
 * that requires LS privileges.
 */
sysSYSLIB_CODE void
soeSetupProdValues_LS10(void)
{
}

/*
 * @brief Setup the Priv Level Masks for LR10 registers
 * required for L0 access.
 */
sysSYSLIB_CODE void
soeSetupPrivLevelMasks_LS10(void)
{
}

/*!
 * @brief Early initialization for LR10 chips.
 *
 * Any general initialization code not specific to particular engines should be
 * done here. This function must be called prior to starting the scheduler.
 *
 * todo : Check with falcon code
 */
sysSYSLIB_CODE FLCN_STATUS
soePreInit_LS10(void)
{
    // Initilize priv ring before touching full BAR0
    soeInitPrivRing_HAL();

    // Setup privlevel mask for L0 access of registers.
    soeSetupPrivLevelMasks_HAL();

    // Setup PROD Values for LR10 registers. 
    soeSetupProdValues_HAL();

    return FLCN_OK;
}
