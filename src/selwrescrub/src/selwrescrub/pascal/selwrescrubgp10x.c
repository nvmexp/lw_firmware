/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: selwrescrubgp10x.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "hwproject.h"
#include "dev_fb.h"
#include "dev_fb_addendum.h"
#include "dev_disp.h"
#include "dev_disp_addendum.h"
#include "dev_lwdec_csb.h"
#include "dev_master.h"
#include "dev_fuse.h"
#include "dev_sec_pri.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "dev_therm.h"
#include "dev_therm_addendum.h"
#include "dev_graphics_nobundle.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "selwrescrub.h"
#include "selwrescrubreg.h"
#include "sec2mutexreservation.h"
#include "config/g_selwrescrub_private.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * Wrapper function for blocking falcon read instruction to halt in case of Priv Error.
 */
LwU32
selwrescrubReadRegister_GP10X
(
    LwU32 addr
)
{
    LwU32   val           = 0;
    LwU32   csbErrStatVal = 0;

    //
    // TODO: Bug 200269221: We should not halt but return error from here,
    // so caller can take appropriate action.
    //
    // DO NOT COPY THIS CODE FOR PRIV ERROR HANDLING. THIS CODE HALTS IF ANY
    // PRIV ERROR OCLWRS. THIS IS A BAD MOVE, WE MUST RETURN TO THE CALLER
    // SO CALLER CAN TAKE APPROPRIATE ACTION.
    //

    val = falc_ldxb_i ((LwU32*)(addr), 0);

    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CLWDEC_FALCON_CSBERRSTAT), 0);

    if (FLD_TEST_DRF(_CLWDEC, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        // If CSBERRSTAT is true, halt no matter what register was read.
        falc_halt();
    }
    else if ((val & CSB_INTERFACE_MASK_VALUE) == CSB_INTERFACE_BAD_VALUE)
    {
        //
        // If we get here, we thought we read a bad value. However, there are
        // certain cases where the values may lead us to think we read something
        // bad, but in fact, they were just valid values that the register could
        // report. For example, the host ptimer PTIMER1 and PTIMER0 registers
        // can return 0xbadfxxxx value that we interpret to be "bad".
        //

        if (addr == LW_CLWDEC_FALCON_PTIMER1)
        {
            //
            // PTIMER1 can report a constant 0xbadfxxxx for a long time, so
            // just make an exception for it.
            //
            return val;
        }
        else if (addr == LW_CLWDEC_FALCON_PTIMER0)
        {
            //
            // PTIMER0 can report a 0xbadfxxxx value for up to 2^16 ns, but it
            // should keep changing its value every nanosecond. We just check
            // to make sure we can detect its value changing by reading it a
            // few times.
            //
            LwU32   val1    = 0;
            LwU32   i       = 0;

            for (i = 0; i < 3; i++)
            {
                val1 = falc_ldx_i ((LwU32 *)(addr), 0);
                if (val1 != val)
                {
                    // Timer0 is progressing, so return latest value, i.e. val1.
                    return val1;
                }
            }
        }

        // Halt here. PTIMER0 or not, the register is constantly returning a bad value.
        falc_halt();
    }

    return val;
}

/*!
 * Wrapper function for blocking falcon write instruction to halt in case of Priv Error.
 */
void
selwrescrubWriteRegister_GP10X
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
        falc_halt();
    }
}

/*!
 * @brief Wait for BAR0 to become idle
 */
void selwrescrubBar0WaitIdle_GP10X(void)
{
    LwBool bDone = LW_FALSE;

    // As this is an infinite loop, timeout should be considered on using below loop elsewhere.
    do
    {
        switch (DRF_VAL(_CLWDEC, _BAR0_CSR, _STATUS,
                    FALC_REG_RD32_STALL(CSB, LW_CLWDEC_BAR0_CSR)))
        {
            case LW_CLWDEC_BAR0_CSR_STATUS_IDLE:
                bDone = LW_TRUE;
                break;

            case LW_CLWDEC_BAR0_CSR_STATUS_BUSY:
                break;

            // unexpected error conditions
            case LW_CLWDEC_BAR0_CSR_STATUS_TMOUT:
            case LW_CLWDEC_BAR0_CSR_STATUS_DIS:
            default:
                falc_halt();
        }
    }
    while (!bDone);
}

/*!
 * @brief Reads BAR0
 */
LwU32 selwrescrubReadBar0_GP10X(LwU32 addr)
{
    LwU32 val32;

    selwrescrubBar0WaitIdle_HAL();
    FALC_REG_WR32_STALL(CSB, LW_CLWDEC_BAR0_ADDR, addr);

    FALC_REG_WR32_STALL(CSB, LW_CLWDEC_BAR0_CSR,
        DRF_DEF(_CLWDEC, _BAR0_CSR, _CMD ,        _READ) |
        DRF_NUM(_CLWDEC, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
        DRF_DEF(_CLWDEC, _BAR0_CSR, _TRIG,        _TRUE));

    (void)FALC_REG_RD32_STALL(CSB, LW_CLWDEC_BAR0_CSR);

    selwrescrubBar0WaitIdle_HAL();
    val32 = FALC_REG_RD32(CSB, LW_CLWDEC_BAR0_DATA);

    return val32;
}

/*!
 * @brief Writes BAR0
 */
void selwrescrubWriteBar0_GP10X(LwU32 addr, LwU32 data)
{
    selwrescrubBar0WaitIdle_HAL();
    FALC_REG_WR32_STALL(CSB, LW_CLWDEC_BAR0_ADDR, addr);
    FALC_REG_WR32_STALL(CSB, LW_CLWDEC_BAR0_DATA, data);

    FALC_REG_WR32_STALL(CSB, LW_CLWDEC_BAR0_CSR,
        DRF_DEF(_CLWDEC, _BAR0_CSR, _CMD ,        _WRITE) |
        DRF_NUM(_CLWDEC, _BAR0_CSR, _BYTE_ENABLE, 0xF   ) |
        DRF_DEF(_CLWDEC, _BAR0_CSR, _TRIG,        _TRUE ));

    (void)FALC_REG_RD32_STALL(CSB, LW_CLWDEC_BAR0_CSR);
    selwrescrubBar0WaitIdle_HAL();
}

/*!
 * @brief: reports error code in mailbox 0 and additional information in
 *          mailbox1
 */
void
selwrescrubReportStatus_GP10X
(
    LwU32 mailbox0,
    LwU32 mailbox1
)
{
    FALC_REG_WR32(CSB, LW_CLWDEC_FALCON_MAILBOX0, mailbox0);

    // Do not update mailbox1 if it is 0. it carries additional error information
    if (mailbox1)
    {
        FALC_REG_WR32(CSB, LW_CLWDEC_FALCON_MAILBOX1, mailbox1);
    }
}

#define SELWRESCRUB_UCODE_HIGHEST_VERSION_GP104_THAT_DID_NOT_LOOK_AT_BIT_2  (1)

/*
 * @brief: Returns supported binary version blown in fuses.
 */
SELWRESCRUB_RC
selwrescrubGetHwFuseVersion_GP10X(LwU32 *pFuseVersionHw)
{
    if (!pFuseVersionHw)
    {
        return SELWRESCRUB_RC_ILWALID_POINTER;
    }

    //
    // SPARE_BIT_4, 5 and 6 offsets are same across all GP10X, so no need to fork a separate HAL for reading
    // these fuse bits. Note that some spare bits differ even between gp10x (for e.g. SPARE_BIT_13 offser
    // differs between gp104 and gp107/8.
    //
    LwU32  bit0 = FALC_REG_RD32(BAR0, LW_FUSE_SPARE_BIT_5) & 0x1;
    LwU32  bit1 = FALC_REG_RD32(BAR0, LW_FUSE_SPARE_BIT_6) & 0x1;
    LwU32  bit2 = FALC_REG_RD32(BAR0, LW_FUSE_SPARE_BIT_4) & 0x1; /* Yes, this is MSB for GP10X */
    LwU32  fuseVersionHW = (bit1 << 1) | (bit0);

    //
    // We added support for checking LW_FUSE_SPARE_BIT_4 only in version 2 of scrubber and beyond when we realized
    // that we are close to running out of revocation possibilities. However, doing so means that versions
    // 100 (binary) and 101 (binary) are invalid combinations since v0 of binary will not revoke itself on 100
    // and v1 of binary will not revoke itself on 100 or 101. So, we need to blacklist these two combinations.
    //
    if (bit2 && fuseVersionHW <= SELWRESCRUB_UCODE_HIGHEST_VERSION_GP104_THAT_DID_NOT_LOOK_AT_BIT_2)
    {
        return SELWRESCRUB_RC_UNSUPPORTED_CHIP;
    }
    else
    {
        fuseVersionHW |= (bit2 << 2);
    }

    *pFuseVersionHw = fuseVersionHW;
    return SELWRESCRUB_RC_OK;
}

/*
 * @brief: Check if UCODE is running on valid chip
 */
LwBool
selwrescrubIsChipSupported_GP10X(void)
{
    LwBool bSupportedChip = LW_FALSE;

    LwU32 chipId  = FALC_REG_RD32(BAR0, LW_PMC_BOOT_42);
    chipId        = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chipId);

    switch (chipId)
    {
        case LW_PMC_BOOT_42_CHIP_ID_GP104:
        case LW_PMC_BOOT_42_CHIP_ID_GP102:
        case LW_PMC_BOOT_42_CHIP_ID_GP106:
        case LW_PMC_BOOT_42_CHIP_ID_GP107:
        case LW_PMC_BOOT_42_CHIP_ID_GP108:
            bSupportedChip = LW_TRUE;
        break;
    }

    return bSupportedChip;
}

/*!
 * @brief: Revoke scrubber if necessary, based on HW & SW fuse version, and chip ID in PMC_BOOT_42

 */
SELWRESCRUB_RC
selwrescrubRevokeHsBin_GP10X(void)
{
    LwU32           fuseVersionHW   = 0;
    LwU32           ucodeVersion    = 0;
    SELWRESCRUB_RC  status          = SELWRESCRUB_RC_OK;

    LwBool bSupportedChip   = selwrescrubIsChipSupported_HAL(pSelwrescrub);

    if (!bSupportedChip)
    {
        return SELWRESCRUB_RC_UNSUPPORTED_CHIP;
    }

    if (SELWRESCRUB_RC_OK != (status = selwrescrubGetHwFuseVersion_HAL(pSelwrescrub, &fuseVersionHW)))
    {
        return status;
    }

    status = selwrescrubGetSwUcodeVersion_HAL(pSelwrescrub, &ucodeVersion);
    if (SELWRESCRUB_RC_OK != status)
    {
        return status;
    }

    if (ucodeVersion < fuseVersionHW)
    {
        return SELWRESCRUB_RC_BIN_REVOKED;
    }

    return SELWRESCRUB_RC_OK;
}


/*!
 * @brief: Check for devinit handoff value and halt if handoff value doesn't match expected behavior.
 */
SELWRESCRUB_RC
selwrescrubCheckDevinitHandoffIsAsExpected_GP10X(void)
{
    LwU32 vprInfo = FALC_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15);

    if (!FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_15, _VPR_HANDOFF, _VALUE_DEVINIT_DONE, vprInfo))
    {
        return SELWRESCRUB_RC_UNEXPECTED_VPR_HANDOFF_VALUE;
    }

    return SELWRESCRUB_RC_OK;
}


/*!
 * @brief: Update handoff value to indicate that scrubber binary is taking over.
 */
SELWRESCRUB_RC selwrescrubUpdateHandoffValScrubberBinTakingOver_GP10X()
{
    //
    // We are expecting here that the SEC2_MUTEX_VPR_SCRATCH mutex has been acquired by the caller
    // of this function so we can directly go and do RMW on LW_PGC6_BSI_VPR_SELWRE_SCRATCH_* registers.
    //
    LwU32 vprInfo = FALC_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15);

    vprInfo = FLD_SET_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_15, _VPR_HANDOFF, _VALUE_SCRUBBER_BIN_TAKING_OVER, vprInfo);

    FALC_REG_WR32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15, vprInfo);

    // Read back the handoff value and verify
    if (vprInfo != FALC_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15))
    {
        return SELWRESCRUB_RC_ILWALID_STATE;
    }

    return SELWRESCRUB_RC_OK;
}

/*!
 * @brief: Update handoff value to indicate that scrubber binary is done.
 */
void selwrescrubUpdateHandoffValScrubberBinDone_GP10X()
{
    //
    // We are expecting here that the SEC2_MUTEX_VPR_SCRATCH mutex has been acquired by the caller
    // of this function so we can directly go and do RMW on LW_PGC6_BSI_VPR_SELWRE_SCRATCH_* registers.
    //
    LwU32 vprInfo = FALC_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15);

    vprInfo = FLD_SET_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_15, _VPR_HANDOFF, _VALUE_SCRUBBER_BIN_DONE, vprInfo);

    FALC_REG_WR32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15, vprInfo);
}

SELWRESCRUB_RC
selwrescrubWriteScrubberBilwersionToBsiSelwreScratch_GP10X(void)
{
    SELWRESCRUB_RC  status    = SELWRESCRUB_RC_OK;
    LwU32           myVersion = 0;

    status = selwrescrubGetSwUcodeVersion_HAL(pSelwrescrub, &myVersion);
    if(SELWRESCRUB_RC_OK !=  status)
    {
        return status;
    }

    LwU32 val = FALC_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15);
    LwU32 scrubberBinaryVersionInScratch = DRF_VAL(_PGC6, _BSI_VPR_SELWRE_SCRATCH_15, _SCRUBBER_BINARY_VERSION, val);

    if (scrubberBinaryVersionInScratch == 0 ||
        myVersion < scrubberBinaryVersionInScratch)
    {
        val = FLD_SET_DRF_NUM(_PGC6, _BSI_VPR_SELWRE_SCRATCH_15, _SCRUBBER_BINARY_VERSION, myVersion, val);
        FALC_REG_WR32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15, val);

        // Now read back the version and verify it was written correctly.
        if (myVersion !=
                REG_RD_DRF(BAR0, _PGC6, _BSI_VPR_SELWRE_SCRATCH_15, _SCRUBBER_BINARY_VERSION))
        {
            status = SELWRESCRUB_RC_ILWALID_STATE;
            goto Cleanup;
        }
    }

Cleanup:
    return (status);
}

LwBool
selwrescrubInGC6Exit_GP10X()
{
    //
    // Check LW_PGC6_SCI_SW_INTR0_STATUS_LWRR contains GC6 exit pending interrupt. But this can be
    // turned off by level2+ ucoode from LW_PGC6_SCI_SW_INTR0_EN. Although we wouldn't do that, but
    // we have another way of figuring out GC6 exit: LW_THERM_SELWRE_WR_SCRATCH_1_ACR_BSI_LOCK_PHASE_TO_GC6_UDE_HANDOFF
    // ACR BSI lock phase binary sends out handoff to GC6 to signal that it is done.
    // This binary will only execute on GC6 exit (it makes sure of that) using multiple secure handoffs.
    // So this handoff being present can indicate that we are in GC6 exit.
    //
    return FLD_TEST_DRF_DEF(BAR0, _PGC6, _SCI_SW_INTR0_STATUS_LWRR, _GC6_EXIT, _PENDING) ||
           FLD_TEST_DRF_DEF(BAR0, _THERM, _SELWRE_WR_SCRATCH_1, _ACR_BSI_LOCK_PHASE_TO_GC6_UDE_HANDOFF, _VAL_ACR_DONE);
}


/*!
 * Utility macro that may be used validate an index to ensure a valid SEC2 HW
 * mutex corresponds to that index.  A non-zero return value represents a
 * valid mutex; zero represents an invalid mutex.
 *
 * @param[in]  mutexId  The SEC2 mutex to validate
 */
#define SEC2_MUTEX_ID_WITHIN_BOUNDS(mutexId)                                  \
    ((mutexId) < LW_PSEC_MUTEX__SIZE_1)

/*!
 * Utility macro for retrieving the owner-id for a specific mutex.  When the
 * mutex is not acquired (free), LW_PSEC_MUTEX_VALUE_INITIAL_LOCK will be
 * returned.
 *
 * @param[in]  mutexId  The SEC2 mutex to read for getting owner.
 */
#define SEC2_MUTEX_GET_OWNER_FROM_ID_GP10X(mutexId)                           \
    REG_IDX_RD_DRF(BAR0, _PSEC, _MUTEX, (mutexId), _VALUE)


SELWRESCRUB_RC
selwrescrubAcquireSec2Mutex_GP10X
(
    FLCNMUTEX *pMutex
)
{
    //
    // This function acquires SEC2 mutex. Adopted from SEC2 ucode's implementation of function sec2MutexAcquire_GP10X in
    // http://rmopengrok.lwpu.com/source/xref/r367_00/uproc/sec2/src/sec2/pascal/sec2_mutexgp10x.c#126
    //
    LwU8 genId = 0;
    LwU8 owner = 0;

    // validate the arguments
    if ((!pMutex) || (!SEC2_MUTEX_ID_WITHIN_BOUNDS(pMutex->mutexId)))
    {
        return SELWRESCRUB_RC_ILWALID_ARGUMENT;
    }

    // Use Mutex ID generator to get unique mutex ID i.e. token that may be used to lock the target Mutex.
    genId = REG_RD_DRF(BAR0, _PSEC, _MUTEX_ID, _VALUE);
    if (( genId == LW_PSEC_MUTEX_ID_VALUE_NOT_AVAIL) ||
        ( genId == LW_PSEC_MUTEX_ID_VALUE_INIT))
    {
        return SELWRESCRUB_RC_MUTEX_ACQUIRE_FAILED;
    }

    do
    {
        // Attempt to acquire Mutex
        REG_IDX_WR_DRF_NUM(BAR0, _PSEC, _MUTEX, pMutex->mutexId, _VALUE, genId);

        //Read value to check if Mutex acquired.
        owner = SEC2_MUTEX_GET_OWNER_FROM_ID_GP10X(pMutex->mutexId);
    } while (owner != genId);

    pMutex->hwToken  = genId;
    pMutex->bValid = LW_TRUE;

    return SELWRESCRUB_RC_OK;
}



SELWRESCRUB_RC
selwrescrubReleaseSec2Mutex_GP10X
(
    FLCNMUTEX *pMutex
)
{
    // validate the arguments
    if ((!pMutex) || (!SEC2_MUTEX_ID_WITHIN_BOUNDS(pMutex->mutexId)))
    {
        return SELWRESCRUB_RC_ILWALID_ARGUMENT;
    }

    //
    // First verify that the caller owns mutex that is about to be released.
    // This is done by verifying that the current mutex register contains the
    // token that the caller has provided.  If the caller does not own the
    // mutex, bail (do NOT release the provided token).
    //
    if (SEC2_MUTEX_GET_OWNER_FROM_ID_GP10X(pMutex->mutexId) != pMutex->hwToken)
    {
        return SELWRESCRUB_RC_ILWALID_OPERATION;
    }

    // Caller owns the mutex, release the mutex.
    REG_IDX_WR_DRF_DEF(BAR0, _PSEC, _MUTEX, pMutex->mutexId, _VALUE, _INITIAL_LOCK);

    // Release the token and return success.
    REG_WR_DRF_NUM(BAR0, _PSEC, _MUTEX_ID_RELEASE, _VALUE, pMutex->hwToken);

    pMutex->hwToken   = LW_PSEC_MUTEX_ID_RELEASE_VALUE_INIT;
    pMutex->bValid  = LW_FALSE;

    return SELWRESCRUB_RC_OK;
}

LwBool
selwrescrubIsDispEngineEnabledInFuse_GP10X()
{
    return FLD_TEST_DRF_DEF(BAR0, _FUSE, _STATUS_OPT_DISPLAY, _DATA, _ENABLE);
}


/*
 * @brief: Programs decode traps
 */
SELWRESCRUB_RC
selwrescrubProgramRequiredDecodeTraps_GP10X()
{
    SELWRESCRUB_RC status = SELWRESCRUB_RC_OK;

    if (SELWRESCRUB_RC_OK != (status = selwrescrubProgramDecodeTrapToUpgradeSec2MutexToLevel3_HAL(pSelwrescrub)))
    {
        goto ErrorExit;
    }

    if (SELWRESCRUB_RC_OK != (status = selwrescrubProgramDecodeTrapsForFECS_HAL(pSelwrescrub)))
    {
        goto ErrorExit;
    }

ErrorExit:
    return status;
}

SELWRESCRUB_RC
selwrescrubGetUsableFbSizeInMb_GP10X(LwU64 *pUsableFbSizeMB)
{
    LwU16          lowerMag             = 0;    // LwU16 on purpose
    LwU16          lowerScale           = 0;    // LwU16 on purpose
    LwBool         bEccMode;

    if (pUsableFbSizeMB == NULL)
    {
        return SELWRESCRUB_RC_ILWALID_POINTER;
    }

    //
    // For simplicity, lets not support any mixed memory configs in this callwlation:
    // So, MIDDLE_MAG, MIDDLE_SCALE, UPPER_MAG, UPPER_SCALE must all be at INIT value, i.e. 0,
    // otherwise lets not support such configuration in this function and jump to ErrorExit
    // without taking down the type1 lock.
    // We have a buy in from LW mangement that we will not support mixed memory configuration
    // but adding this check here just in the rare case it appears again
    //
    CT_ASSERT(LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_MIDDLE_SCALE_INIT == 0);
    CT_ASSERT(LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_MIDDLE_MAG_INIT   == 0);
    CT_ASSERT(LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_UPPER_SCALE_INIT  == 0);
    CT_ASSERT(LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_UPPER_MAG_INIT    == 0);

    if (!FLD_TEST_DRF_DEF(BAR0, _PFB, _PRI_MMU_LOCAL_MEMORY_RANGE, _MIDDLE_SCALE, _INIT) ||
        !FLD_TEST_DRF_DEF(BAR0, _PFB, _PRI_MMU_LOCAL_MEMORY_RANGE, _MIDDLE_MAG,   _INIT) ||
        !FLD_TEST_DRF_DEF(BAR0, _PFB, _PRI_MMU_LOCAL_MEMORY_RANGE, _UPPER_SCALE,  _INIT) ||
        !FLD_TEST_DRF_DEF(BAR0, _PFB, _PRI_MMU_LOCAL_MEMORY_RANGE, _UPPER_MAG,    _INIT))
    {
        return SELWRESCRUB_RC_ILWALID_STATE;
    }

    //
    // Another errorcheck: LOWER_SCALE, LOWER_MAG must not be _INIT, they should have been
    // initialized to sane values by devinit. If we find them at _INIT values, something is
    // wrong, so bail out with error.
    //
    lowerScale = REG_RD_DRF(BAR0, _PFB, _PRI_MMU_LOCAL_MEMORY_RANGE, _LOWER_SCALE);
    lowerMag   = REG_RD_DRF(BAR0, _PFB, _PRI_MMU_LOCAL_MEMORY_RANGE, _LOWER_MAG);
    bEccMode   = REG_RD_DRF(BAR0, _PFB, _PRI_MMU_LOCAL_MEMORY_RANGE, _ECC_MODE);

    CT_ASSERT(LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_SCALE_INIT == 0);
    CT_ASSERT(LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_MAG_INIT   == 0);

    if (lowerScale == LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_SCALE_INIT ||
        lowerMag   == LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_MAG_INIT)
    {
        return SELWRESCRUB_RC_ILWALID_STATE;
    }

    //
    // Start of FB = address 0x0
    // End of FB   = lowerMag * (2^(lowerScale+20))
    // Avoiding the +20 in the equation will give us FB size in MB instead of bytes
    // (2 ^ someValue) can be easily computed as (1 << someValue)
    // For FB, it is even easier to callwlate "totalFbSizeMB = lowerMag << lowerScale", instead of "totalFbSizeMB = lowerMag *  (1 << lowerScale)"
    // lowerScale is 4 bits in HW, so it will max out at value 0xF, and lowerMag can be max 0x3f (6 bits)
    // With max values, the multiplication result will still fit in an LwU32
    //
    *pUsableFbSizeMB = (LwU64)( (LwU32)lowerMag << lowerScale );
    //
    // With ECC_MODE set, the callwlations are:
    // lower_range = 15/16 * LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_MAG * (2^(LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_SCALE+20))
    // TODO: Update this logic to read FB size directly from secure scratch registers, Bug 2168823 has details
    //
    if (bEccMode)
    {
        *pUsableFbSizeMB = ( *pUsableFbSizeMB >> 4 ) * (LwU64)15;
    }

    return SELWRESCRUB_RC_OK;
}

/*!
 * @brief: To mitigate the risk of a  NS restart from HS mode we follow #19 from
 * https://wiki.lwpu.com/gpuhwmaxwell/index.php/MaxwellSW/Resman/Security#Guidelines_for_HS_binary.
 * Please go through bug 2534981 for more details
 */
void
selwrescrubMitigateNSRestartFromHS_GP10X(void)
{
    LwU32 cpuctlAliasEn;

    cpuctlAliasEn = FALC_REG_RD32(CSB, LW_CLWDEC_FALCON_CPUCTL);
    cpuctlAliasEn = FLD_SET_DRF(_CLWDEC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, cpuctlAliasEn);
    FALC_REG_WR32(CSB, LW_CLWDEC_FALCON_CPUCTL, cpuctlAliasEn);

    // We read-back the value of CPUCTL until ALIAS_EN  is set to FALSE.
    cpuctlAliasEn = FALC_REG_RD32(CSB, LW_CLWDEC_FALCON_CPUCTL);
    while(!FLD_TEST_DRF(_CLWDEC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, cpuctlAliasEn))
    {
        cpuctlAliasEn = FALC_REG_RD32(CSB, LW_CLWDEC_FALCON_CPUCTL);
    }
}

