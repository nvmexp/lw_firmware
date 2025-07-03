/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: selwrescrubgv10x.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "hwproject.h"
#include "dev_disp.h"
#include "dev_disp_addendum.h"
#include "dev_se_seb.h"
#include "dev_se_seb_addendum.h"
#include "selwrescrub.h"
#include "selwrescrubreg.h"
#include "dev_master.h"
#include "dev_fuse.h"
#include "dev_lwdec_csb.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "dev_fb.h"
#include "config/g_selwrescrub_private.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define FALCON_DIO_D0_READ_BIT 0x10000

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/*
 * @brief: Check if UCODE is running on valid chip
 */
LwBool
selwrescrubIsChipSupported_GV10X(void)
{
    LwBool bSupportedChip = LW_FALSE;

    LwU32 chipId  = FALC_REG_RD32(BAR0, LW_PMC_BOOT_42);
    chipId        = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chipId);

    switch (chipId)
    {
        case LW_PMC_BOOT_42_CHIP_ID_GV100:
            bSupportedChip = LW_TRUE;
        break;
    }

    return bSupportedChip;
}


/*
 * @brief: Returns supported binary version blowned in fuses.
 */
SELWRESCRUB_RC
selwrescrubGetHwFuseVersion_GV10X(LwU32 *pFuseVersionHw)
{
    if (!pFuseVersionHw)
    {
        return SELWRESCRUB_RC_ILWALID_POINTER;
    }

    LwU32  bit0 = FALC_REG_RD32(BAR0, LW_FUSE_SPARE_BIT_5) & 0x1;
    LwU32  bit1 = FALC_REG_RD32(BAR0, LW_FUSE_SPARE_BIT_6) & 0x1;
    LwU32  bit2 = FALC_REG_RD32(BAR0, LW_FUSE_SPARE_BIT_4) & 0x1; /* Yes, this is MSB for GV100 as well */
    LwU32  hwFuseVersion = (bit2 << 2) | (bit1 << 1) | (bit0);

    *pFuseVersionHw  = hwFuseVersion;

    return SELWRESCRUB_RC_OK;
}


/*
 * Utility macro that may be used validate an index to ensure a valid common HW
 * mutex corresponds to that index.  A non-zero return value represents a
 * valid mutex; zero represents an invalid mutex.
 *
 * @param[in] groupId: Group index
 * @param[in] mutexId: Mutex index
 */
#define SELWRE_MUTEX_GROUP_INDEX_IS_VALID(groupId, mutexId)                            \
    (groupId < LW_SSE_SE_COMMON_MUTEX_MUTEX__SIZE_1 &&                          \
        mutexId < LW_SSE_SE_COMMON_MUTEX_MUTEX__SIZE_2)

/*!
 * @brief: Acquires common mutex
 *
 * @param[in] busTarget: Since this is common binary, bus target could be PMU or
 *                          SEC2
 * @param[in] groupId:  From GV100 onwards, mutexes are grouped into HS and LS.
 *                          Indicate groud ID of mutex.
 * @param[in] mutexId:  Mutex id of register.
 * @param[out] pToken:  HW generated token which is used to acquire mutex. This
 *                          toekn is used to release mutex.
 *
 * @return  SELWRESCRUB_RC_ILWALID_ARGUMENT:     Invalid mutex group or id
            SELWRESCRUB_RC_ILWALID_ARGUMENT      pToken is null
 *          SELWRESCRUB_RC_MUTEX_ACQUIRE_FAILED: Error while accessing bus or failed
 *                                          to acquire mutex
 *          SELWRESCRUB_RC_OK: No Error scenario
 */
SELWRESCRUB_RC
selwrescrubAcquireSelwreSECommonMutex_GV10X
(
    FLCNMUTEX *pMutex
)
{
    LwU32           genId;
    LwU32           mutexOwner;
    LwU8            mutexId, groupId;
    SELWRESCRUB_RC  status          = SELWRESCRUB_RC_OK;
    SELWRESCRUB_RC  statusCleanup   = SELWRESCRUB_RC_OK;

    if (NULL == pMutex || LW_TRUE == pMutex->bValid)
    {
        return SELWRESCRUB_RC_ILWALID_ARGUMENT;
    }

    // Colwert into groupId and actual mutexId
    groupId = SELWRE_MUTEX_DERIVE_GROUPID(pMutex->mutexId);
    mutexId = SELWRE_MUTEX_DERIVE_MUTEXID(pMutex->mutexId);

    // Check validity of group and mutex
    if (!SELWRE_MUTEX_GROUP_INDEX_IS_VALID(groupId, mutexId))
    {
        return SELWRESCRUB_RC_ILWALID_ARGUMENT;
    }

    // Get HW generated token
    if (SELWRESCRUB_RC_OK != (status = selwrescrubSelwreBusReadRegister_HAL(pSelwrescrub,
                                       LW_SSE_SE_COMMON_MUTEX_ID(groupId), &genId)))
    {
        return status;
    }

    genId = DRF_VAL(_SSE, _SE_COMMON_MUTEX_ID, _VALUE, genId);

    if (genId == LW_SSE_SE_COMMON_MUTEX_ID_VALUE_INIT ||
        genId == LW_SSE_SE_COMMON_MUTEX_ID_VALUE_NOT_AVAIL)
    {
        return SELWRESCRUB_RC_MUTEX_ACQUIRE_FAILED;
    }

    genId = FLD_SET_DRF_NUM(_SSE, _SE_COMMON_MUTEX_MUTEX, _VALUE, genId, 0);

    while(LW_TRUE)
    {
        // Write generated token to Mutex register
        if (SELWRESCRUB_RC_OK != (status = selwrescrubSelwreBusWriteRegister_HAL(pSelwrescrub,
                                               LW_SSE_SE_COMMON_MUTEX_MUTEX(groupId, mutexId),
                                               genId)))
        {
            goto fail_and_bail_mutex;
        }

        // Read back Mutex register
        if (SELWRESCRUB_RC_OK != (status = selwrescrubSelwreBusReadRegister_HAL(pSelwrescrub,
                                               LW_SSE_SE_COMMON_MUTEX_MUTEX(groupId, mutexId),
                                               &mutexOwner)))
        {
            goto fail_and_bail_mutex;
        }

        // Mutex is acquire successfully if our genID stuck in the mutexID register
        if (DRF_VAL(_SSE, _SE_COMMON_MUTEX_MUTEX, _VALUE, mutexOwner) == genId)
        {
            pMutex->hwToken = genId;
            pMutex->bValid  = LW_TRUE;
            return SELWRESCRUB_RC_OK;
        }
    }

fail_and_bail_mutex:
    pMutex->hwToken = 0;
    // Release the genID generated by this function
    if (genId != LW_SSE_SE_COMMON_MUTEX_ID_VALUE_INIT &&
        genId != LW_SSE_SE_COMMON_MUTEX_ID_VALUE_NOT_AVAIL)
    {
        statusCleanup = selwrescrubSelwreBusWriteRegister_HAL(pSelwrescrub,
                                                              LW_SSE_SE_COMMON_MUTEX_ID_RELEASE(groupId),
                                                              genId);
    }

    return (status == SELWRESCRUB_RC_OK ? statusCleanup : status);
}

/*!
 * @brief: Releases common mutex
 *
 * @param[in|out] pMutex: Mutex structre
 *
 * @return  SELWRESCRUB_RC_ILWALID_ARGUMENT:   Invalid mutex group or id
 *          SELWRESCRUB_RC_OK: No Error scenario
 */

SELWRESCRUB_RC
selwrescrubReleaseSelwreSECommonMutex_GV10X
(
    FLCNMUTEX *pMutex
)
{
    SELWRESCRUB_RC status   = SELWRESCRUB_RC_OK;
    SELWRESCRUB_RC status2  = SELWRESCRUB_RC_OK;
    LwU32 mutexOwnerInHw;
    LwU8 mutexId, groupId;


    if (NULL == pMutex || LW_FALSE == pMutex->bValid)
    {
        return SELWRESCRUB_RC_ILWALID_ARGUMENT;
    }

    // Colwert into groupId and actual mutexId
    groupId = SELWRE_MUTEX_DERIVE_GROUPID(pMutex->mutexId);
    mutexId = SELWRE_MUTEX_DERIVE_MUTEXID(pMutex->mutexId);

    // Check validity of group and mutex
    if (!SELWRE_MUTEX_GROUP_INDEX_IS_VALID(groupId, mutexId))
    {
        return SELWRESCRUB_RC_ILWALID_ARGUMENT;
    }

    // Read back mutex register and confirm ownership against hwToken passed by caller
    if (SELWRESCRUB_RC_OK != (status = selwrescrubSelwreBusReadRegister_HAL(pSelwrescrub,
                                           LW_SSE_SE_COMMON_MUTEX_MUTEX(groupId, mutexId),
                                           &mutexOwnerInHw)))
    {
        return status;
    }

    // Mutex is owned by caller if the caller's hwToken matches with HW, otherwise not
    if (DRF_VAL(_SSE, _SE_COMMON_MUTEX_MUTEX, _VALUE, mutexOwnerInHw) != pMutex->hwToken)
    {
        return SELWRESCRUB_RC_MUTEX_OWNERSHIP_MATCH_FAILED;
    }

    status = selwrescrubSelwreBusWriteRegister_HAL(pSelwrescrub,
                 LW_SSE_SE_COMMON_MUTEX_MUTEX(groupId, mutexId),
                 LW_SSE_SE_COMMON_MUTEX_MUTEX_VALUE_INIT);


    status2 = selwrescrubSelwreBusWriteRegister_HAL(pSelwrescrub,
                  LW_SSE_SE_COMMON_MUTEX_ID_RELEASE(groupId),
                  FLD_SET_DRF_NUM(_SSE, _SE_COMMON_MUTEX_ID_RELEASE, _VALUE, pMutex->hwToken, 0));

    pMutex->bValid  = LW_FALSE;
    pMutex->hwToken = 0;

    return (status == SELWRESCRUB_RC_OK ? status2 : status);
}


/*!
 * @brief Sends a read or write request to the Secure private
 * bus between SEC2 and HUB
 *
 * @param[in] requestType   Whether it is a read or a write request
 * @param[in] addr          Address to write/read
 * @param[in] valToWrite    If its a write request, the value to be written
 *
 * @return
 */
SELWRESCRUB_RC
selwrescrubSelwreBusSendRequest_GV10X
(
    SELWREBUS_REQUEST_TYPE  requestType,
    LwU32                   addr,
    LwU32                   valueToWrite
)
{
    LwU32 dioStatus;

    LwU32 d0Addr        = LW_CLWDEC_FALCON_DOC_D0;
    LwU32 d1Addr        = LW_CLWDEC_FALCON_DOC_D1;
    LwU32 dioReadyVal   = DRF_DEF(_CLWDEC, _FALCON_DOC_CTRL, _EMPTY,       _INIT) |
                          DRF_DEF(_CLWDEC, _FALCON_DOC_CTRL, _WR_FINISHED, _INIT) |
                          DRF_DEF(_CLWDEC, _FALCON_DOC_CTRL, _RD_FINISHED, _INIT);

    //
    // Send out the read/write request onto the DIO
    // 1. Wait for channel to become empty
    //
    do
    {
        dioStatus = FALC_REG_RD32(CSB, LW_CLWDEC_FALCON_DOC_CTRL) & dioReadyVal;
    }
    while (dioStatus != dioReadyVal);

    //
    // 2. If it is a write request the push value onto D1,
    //    otherwise set the read bit in the address
    //
    if (SELWREBUS_REQUEST_TYPE_READ_REQUEST == requestType)
    {
        addr |= FALCON_DIO_D0_READ_BIT;
    }
    else
    {
        FALC_REG_WR32(CSB, d1Addr, valueToWrite);
    }

    // 3. Issue request
    FALC_REG_WR32(CSB, d0Addr, addr);

    return SELWRESCRUB_RC_OK;
}

/*!
 * @brief Once a read request happens through selwrescrubSelwreBusSendRequest,
 *        this function reads the actual data
 *
 * @param[out] *pVal  Pointer where the value read will be stored
 *
 * @return SELWRESCRUB_RC_OK if value read successfully.
 *         specific error if the request failed.
 */
SELWRESCRUB_RC
selwrescrubSelwreBusGetData_GV10X
(
    LwU32   *pVal
)
{
    LwU32 error;
    LwU32 data;
    LwU32 pop1Dword = 0;
    LwU32 dicCtrlAddr;
    LwU32 docCtrlAddr;
    LwU32 d0Addr;

    if (NULL == pVal)
    {
        return SELWRESCRUB_RC_ILWALID_ARGUMENT;
    }

    pop1Dword   = FLD_SET_DRF_NUM(_CLWDEC, _FALCON_DIC_CTRL, _POP, 0x1, pop1Dword);
    dicCtrlAddr = LW_CLWDEC_FALCON_DIC_CTRL;
    docCtrlAddr = LW_CLWDEC_FALCON_DOC_CTRL;
    d0Addr      = LW_CLWDEC_FALCON_DIC_D0;

    //
    // Read data from DIO
    // 1. Wait until data is available
    // Reading DIC_CTRL register. It returns number of free entries in DIC.
    // Wait till DIC_CTRL is not 0
    //
    do
    {
        data = FALC_REG_RD32(CSB, dicCtrlAddr);
    }
    while (FLD_TEST_DRF_NUM(_CLWDEC, _FALCON_DIC_CTRL, _COUNT, 0x0, data));

    // 2. Pop read data
    FALC_REG_WR32(CSB, dicCtrlAddr, pop1Dword);

    // 2.5 Check read error from the DOC control
    error = FALC_REG_RD32(CSB, docCtrlAddr);

    //Check if there is a read error or a protocol error
    if (FLD_TEST_DRF_NUM(_CLWDEC, _FALCON_DOC_CTRL, _RD_ERROR,       0x1, error) ||
        FLD_TEST_DRF_NUM(_CLWDEC, _FALCON_DOC_CTRL, _PROTOCOL_ERROR, 0x1, error))
    {
        return SELWRESCRUB_RC_SELWRE_BUS_REQUEST_FAILED;
    }

    // 3. Get the data itself
    *pVal = FALC_REG_RD32(CSB, d0Addr);

    return SELWRESCRUB_RC_OK;
}


/*!
 * @brief Write a register on the secure bus
 *
 * @param[in] addr  Address to write to
 * @param[in] val   The value to be written
 *
 * @return    SELWRESCRUB_RC whether the write was successful or not
 */
SELWRESCRUB_RC
selwrescrubSelwreBusWriteRegister_GV10X
(
    LwU32 addr,
    LwU32 val
)
{
    return selwrescrubSelwreBusSendRequest_HAL(pSelwrescrub,
                                               SELWREBUS_REQUEST_TYPE_WRITE_REQUEST,    /* write request */
                                               addr,                                    /* address to write to */
                                               val);                                    /* value to write */
}


/*!
 * @brief Read a register on the secure bus
 *
 * @param[in]  addr   Address
 * @param[out] *pVal  Pointer where the value read will be stored
 *
 * @return SELWRESCRUB_RC whether the read was successful or not
 */
SELWRESCRUB_RC
selwrescrubSelwreBusReadRegister_GV10X
(
    LwU32 addr,
    LwU32 *pVal
)
{
    SELWRESCRUB_RC status = SELWRESCRUB_RC_OK;

    if (NULL == pVal)
    {
        return SELWRESCRUB_RC_ILWALID_ARGUMENT;
    }

    status = selwrescrubSelwreBusSendRequest_HAL(pSelwrescrub,
                                                 SELWREBUS_REQUEST_TYPE_READ_REQUEST,   /* read request */
                                                 addr,                                  /* address to read */
                                                 0);                                    /* value to write, 0 for a read request */

    if (status == SELWRESCRUB_RC_OK)
    {
        return selwrescrubSelwreBusGetData_HAL(pSelwrescrub, pVal);
    }

    return status;
}


/*
 * Check if the VPR range is write-locked with appropriate protection - WPR2/memlock range.
 */
SELWRESCRUB_RC
selwrescrubValidateWriteProtectionIsSetOlwPRRange_GV10X
(
    LwU64   vprStartAddressInBytes,
    LwU64   vprEndAddressInBytes,
    LwBool *pIsWpr2ProtectionAppliedToVPR,
    LwBool *pIsMemLockRangeProtectionAppliedToVPR
)
{
    LwU64 memlockLwrStartAddressInBytes = 0;
    LwU64 memlockLwrEndAddressInBytes   = 0;
    LwU64 hwScrubberScrubbedStartAddressInBytes = 0;
    LwU64 hwScrubberScrubbedEndAddressInBytes   = 0;

    // Initialize error code to a safe value
    SELWRESCRUB_RC status = SELWRESCRUB_RC_NO_WRITE_PROTECTION_FOUND_ON_VPR;

    if (pIsWpr2ProtectionAppliedToVPR         == NULL ||
        pIsMemLockRangeProtectionAppliedToVPR == NULL)
    {
        return SELWRESCRUB_RC_ILWALID_POINTER;
    }

    //
    // We don't support WPR2 locking for write protection for VPR scrub needs from GV100+ (also removed from UDE)
    //
    *pIsWpr2ProtectionAppliedToVPR          = LW_FALSE;

    // Lets also initialize this to a safe value, i.e. false
    *pIsMemLockRangeProtectionAppliedToVPR  = LW_FALSE;

    // Read the memory lock range lwrrently programmed in MMU
    if (SELWRESCRUB_RC_OK != (status = selwrescrubGetLwrMemlockRangeInBytes_HAL(pSelwrescrub, 
                                                                                &memlockLwrStartAddressInBytes, 
                                                                                &memlockLwrEndAddressInBytes)))
    {
        goto ErrorExit;
    }

    //
    // This is the new version of UDE, which fixes memlock range such that, excluding the cbcAdjustmentSize at the top
    // and 256MB at the bottom of FB, the rest of FB will be locked using memory lock range. Starting this version,
    // UDE will also scrub the entire FB. The bottom 256 MB and top cbcAdjustmentSize will be scrubbed first for which UDE will wait.
    // The remainder of the FB scrubbing will be initiated and UDE will halt after that.
    // Memory lock range will be set from 256 MB to (fbEnd - cbcAdjustmentSize).
    //
    LwU64 memlockExpectedStartAddrInBytes  = NUM_BYTES_IN_256_MB;
    LwU64 memlockExpectedEndAddressInBytes = 0; // callwlating below
    LwU64 usableFBSizeInMB                 = 0;
    LwU64 usableFBSizeInBytes              = 0;
    LwU32 vprSyncScrubSizeMB               = 0;
    LwU64 vprSyncScrubSizeInBytes          = 0;
    LwU64 fbSyncScrubSizeInBytes           = 0;

    // Get the total FB size
    if (SELWRESCRUB_RC_OK != (status = selwrescrubGetUsableFbSizeInMb_HAL(pSelwrescrub, &usableFBSizeInMB)))
    {
        goto ErrorExit;
    }

    if (usableFBSizeInMB == 0)
    {
        status = SELWRESCRUB_RC_ILWALID_USABLE_FB_SIZE;
        goto ErrorExit;
    }

    usableFBSizeInBytes = usableFBSizeInMB << VPR_ADDR_ALIGN_IN_BSI_1MB_SHIFT;

    if (SELWRESCRUB_RC_OK != (status = selwrescrubGetFbSyncScrubSizeAtFbEndInBytes_HAL(pSelwrescrub, usableFBSizeInMB, &fbSyncScrubSizeInBytes)))
    {
        goto ErrorExit;
    }

    // Expected memory lock range end will be (FB_End - fbSyncScrubSizeInBytes) - 1. The -1 is because in HW, the last block is inclusive.
    memlockExpectedEndAddressInBytes = usableFBSizeInBytes - fbSyncScrubSizeInBytes - 1;

    if (memlockLwrStartAddressInBytes  != memlockExpectedStartAddrInBytes)
    {
        status = SELWRESCRUB_RC_MEMLOCK_START_ADDR_NOT_AS_EXPECTED;
        goto ErrorExit;
     }

     if (memlockLwrEndAddressInBytes != memlockExpectedEndAddressInBytes)
     {
        status = SELWRESCRUB_RC_MEMLOCK_END_ADDR_NOT_AS_EXPECTED;
        goto ErrorExit;
     }

     // VPR start must always be higher than memory lock range start
     if (vprStartAddressInBytes < memlockLwrStartAddressInBytes)
     {
        status = SELWRESCRUB_RC_VPR_START_CANNOT_BE_BELOW_MEMLOCK_START;
        goto ErrorExit;
     }

     if (SELWRESCRUB_RC_OK != (status = selwrescrubGetVbiosVprSyncScrubSize_HAL(pSelwrescrub, &vprSyncScrubSizeMB)))
     {
        goto ErrorExit;
     }
     vprSyncScrubSizeInBytes = vprSyncScrubSizeMB << VPR_ADDR_ALIGN_IN_BSI_1MB_SHIFT;

     // VPR end should match memlock end.
     if (vprEndAddressInBytes != memlockLwrEndAddressInBytes)
     {
        status = SELWRESCRUB_RC_VPR_END_MISMATCH_WITH_MEMLOCK_END;
        goto ErrorExit;
     }

     // VPR end should always be within scrubbed range (we sync until mem lock current End + cbcAdjustmentSize more).
     if ((vprEndAddressInBytes + vprSyncScrubSizeInBytes) >= memlockLwrEndAddressInBytes + fbSyncScrubSizeInBytes)
     {
        status = SELWRESCRUB_RC_VPR_START_CANNOT_BE_HIGHER_THAN_END_OF_FB;
        goto ErrorExit;
     }

     // Read the HW Scrubber range lwrrently programmed in MMU
     if (SELWRESCRUB_RC_OK != (status = selwrescrubGetHwScrubberRangeInBytes_HAL(pSelwrescrub, 
                                                                                  &hwScrubberScrubbedStartAddressInBytes, 
                                                                                  &hwScrubberScrubbedEndAddressInBytes)))
     {
        goto ErrorExit;
     }

     // Scrubbed range in HW Scrubber should match exactly with memlock range. If not, then flag error
     if (hwScrubberScrubbedStartAddressInBytes != memlockLwrStartAddressInBytes)
     {
        status = SELWRESCRUB_RC_SCRUBBED_START_ADDR_MISMATCH_WITH_MEMLOCK_START_ADDR;
        goto ErrorExit;
     }

     if (hwScrubberScrubbedEndAddressInBytes != memlockLwrEndAddressInBytes)
     {
        status = SELWRESCRUB_RC_SCRUBBED_START_ADDR_MISMATCH_WITH_MEMLOCK_START_ADDR;
        goto ErrorExit;
     }

     // Everything is good. Let's mark write protection present for VPR range and mark status as OK.
     *pIsMemLockRangeProtectionAppliedToVPR = LW_TRUE;

     //
     // Mark status = OK. This is redundant since (status=OK) happens implicitly above when selwrescrubGetUsableFbSizeInMb_HAL
     // and selwrescrubGetVbiosSyncScrubSize_HAL calls succeed, but good to keep an explicit status = OK here for code readibility.
     //
     status = SELWRESCRUB_RC_OK;

ErrorExit:
    return status;
}

/*
 * @brief: Programs decode traps
 */
SELWRESCRUB_RC
selwrescrubProgramRequiredDecodeTraps_GV10X()
{
    SELWRESCRUB_RC status = SELWRESCRUB_RC_OK;

    if (SELWRESCRUB_RC_OK != (status = selwrescrubProgramDecodeTrapsForFECS_HAL(pSelwrescrub)))
    {
        goto ErrorExit;
    }

ErrorExit:
    return status;
}

/*!
 * @brief: Returns the CBC adjustment size required to callwlate the range locked by Vbios
 *
 * @param[in] totalFbSizeInMB: Total FB size in MB
 *
 * @return CBC adjustment size
 */
SELWRESCRUB_RC
selwrescrubGetFbSyncScrubSizeAtFbEndInBytes_GV10X
(
    LwU64 totalFbSizeInMB,
    LwU64 *pFbSyncScrubSizeInBytes
)
{
    if(pFbSyncScrubSizeInBytes == NULL)
    {
        return SELWRESCRUB_RC_ILWALID_POINTER;
    }

    *pFbSyncScrubSizeInBytes = NUM_BYTES_IN_256_MB;

    return SELWRESCRUB_RC_OK;
}
