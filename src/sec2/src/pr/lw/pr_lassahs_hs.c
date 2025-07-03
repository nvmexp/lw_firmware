/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pr_lassahs_hs.c
 *
 * HS code for LASS As HS (LS At Same Security As HS)
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_hs.h"
#include "pr/pr_common.h"
#include "pr/pr_lassahs_hs.h"
#include "pr/pr_lassahs.h"
#include "sec2_objpr.h"
#include "mem_hs.h"
#include "lwosselwreovly.h"
#include "scp_internals.h"
#include "scp_common.h"
#include "sec2_csb.h"
#include "common_hslib.h"

#include "config/g_pr_hal.h"
// For interaction with BSI
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"


/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define SCP_REGISTER_SIZE_BYTES         16
#define PR_LS_SIG_GRP_BUF_SIZE_BYTES    DMA_XFER_SIZE_BYTES(DMA_XFER_ESIZE_MAX_READ)
#define PR_LS_GRP_SIG_SIZE_BYTES        SCP_REGISTER_SIZE_BYTES
#define PR_LS_GRP_IMEM_RD_PORT          0
#define PR_LS_GRP_MAX_OVERLAYS          20

/* ------------------------- Global Variables ------------------------------- */
PR_LASSAHS_DATA_HS g_prLassahsData_Hs PR_ATTR_DATA_OVLY(_g_prLassahsData_Hs) = {0};
LwU8               g_prLassahsUcodeBuf_Hs[PR_LS_SIG_GRP_BUF_SIZE_BYTES] GCC_ATTRIB_ALIGN(SCP_BUF_ALIGNMENT) PR_ATTR_DATA_OVLY(g_prLassahsUcodeBuf_Hs) = {0};
LwU8               g_prLassahsUcodeSig_Hs[PR_LS_GRP_SIG_SIZE_BYTES]     GCC_ATTRIB_ALIGN(SCP_BUF_ALIGNMENT) PR_ATTR_DATA_OVLY(g_prLassahsUcodeSig_Hs) = {0};

// Start and end address for LASSAHS overlays
#ifndef PR_V0404
extern LwU32 _resident_code_start[];
extern LwU32 _load_start_imem_libMemHs[];
extern LwU32 _load_start_imem_libCommonHs[];
extern LwU32 _load_start_imem_prHsCommon[];
extern LwU32 _load_start_imem_prSbHsEntry[];
extern LwU32 _load_start_imem_prSbHsExit[];
extern LwU32 _load_start_imem_prSbHsDecryptMPK[];
extern LwU32 _load_start_imem_prGdkLs1[];
extern LwU32 _load_start_imem_prGdkLs2[];
extern LwU32 _load_start_imem_prGdkLs3[];
extern LwU32 _load_start_imem_prGdkLs4[];
extern LwU32 _load_start_imem_prGdkLs5[];
extern LwU32 _load_start_imem_prGdkLs6[];
extern LwU32 _load_start_imem_libSE[];

extern LwU32 _resident_code_end_aligned[];
extern LwU32 _load_stop_imem_libMemHs[];
extern LwU32 _load_stop_imem_libCommonHs[];
extern LwU32 _load_stop_imem_prHsCommon[];
extern LwU32 _load_stop_imem_prSbHsEntry[];
extern LwU32 _load_stop_imem_prSbHsExit[];
extern LwU32 _load_stop_imem_prSbHsDecryptMPK[];
extern LwU32 _load_stop_imem_prGdkLs1[];
extern LwU32 _load_stop_imem_prGdkLs2[];
extern LwU32 _load_stop_imem_prGdkLs3[];
extern LwU32 _load_stop_imem_prGdkLs4[];
extern LwU32 _load_stop_imem_prGdkLs5[];
extern LwU32 _load_stop_imem_prGdkLs6[];
extern LwU32 _load_stop_imem_libSE[];
#else
extern LwU32 _resident_code_start[];
extern LwU32 _load_start_imem_libMemHs[];
extern LwU32 _load_start_imem_libCommonHs[];
extern LwU32 _load_start_imem_prHsCommon[];
extern LwU32 _load_start_imem_prSbHsEntry[];
extern LwU32 _load_start_imem_prSbHsExit[];
extern LwU32 _load_start_imem_prSbHsDecryptMPK[];
extern LwU32 _load_start_imem_prGdkLs1_44[];
extern LwU32 _load_start_imem_prGdkLs2_44[];
extern LwU32 _load_start_imem_prGdkLs3_44[];
extern LwU32 _load_start_imem_prGdkLs4_44[];
extern LwU32 _load_start_imem_prGdkLs5_44[];
extern LwU32 _load_start_imem_prGdkLs6_44[];
extern LwU32 _load_start_imem_libSE[];

extern LwU32 _resident_code_end_aligned[];
extern LwU32 _load_stop_imem_libMemHs[];
extern LwU32 _load_stop_imem_libCommonHs[];
extern LwU32 _load_stop_imem_prHsCommon[];
extern LwU32 _load_stop_imem_prSbHsEntry[];
extern LwU32 _load_stop_imem_prSbHsExit[];
extern LwU32 _load_stop_imem_prSbHsDecryptMPK[];
extern LwU32 _load_stop_imem_prGdkLs1_44[];
extern LwU32 _load_stop_imem_prGdkLs2_44[];
extern LwU32 _load_stop_imem_prGdkLs3_44[];
extern LwU32 _load_stop_imem_prGdkLs4_44[];
extern LwU32 _load_stop_imem_prGdkLs5_44[];
extern LwU32 _load_stop_imem_prGdkLs6_44[];
extern LwU32 _load_stop_imem_libSE[];
#endif


/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS prVerifyLsSigGrp(void);
static FLCN_STATUS prCallwlateDmHashForOverlay(LwU32 startVa, LwU32 endVa);
static void        prInsertInOrder(LwU32 ovlStartArr[], LwU32 ovlEndArr[], LwU32 *pLwrPos, LwU32 start, LwU32 end);

static void        _prSbHsEntry_Entry(void)
    GCC_ATTRIB_SECTION("imem_prSbHsEntry", "start")
    GCC_ATTRIB_USED();

static void        _prSbHsDecryptMPK_Entry(void)
    GCC_ATTRIB_SECTION("imem_prSbHsDecryptMPK", "start")
    GCC_ATTRIB_USED();

static void        _prSbHsExit_Entry(void)
    GCC_ATTRIB_SECTION("imem_prSbHsExit", "start")
    GCC_ATTRIB_USED();

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief  Entry function of HS entry overlay for LASSAHS
 *
 * Steps for this overlay:
 * a. HW will put the falcon into lock down mode. Don't change anything there to
 *    downgrade that (which is done at the caller of this function)
 * b. Ensure that revocation check passes (which is done by HS pre-check at the
 *    caller of this function)
 * c. Ensure that LSMODE is set (which is done by HS pre-check at the caller of
 *    this function)
 * d. Mark data block containing PR_LASSAHS_DATA_HS as secure
 * e. Save the stack top and bottom.
 * f. Ensure that PSEC_SELWRE_SCRATCH(0) = 0
 * g. Ilwalidate all blocks other than what was provided as "will be used for
 *    LASSAHS". This list is in an inselwre location and can be generated by RM
 *    or ACR
 * h. Compute a signature over all the blocks loaded in IMEM for LASSAHS
 * i. Compare the signature to the one saved off by ACR into two level 3 32-bit
 *    scratch registers and proceed only if signatures match.

 * j. Lower DMEM's PLM to level 2 so that the SE ECC operation can work
 * k. Set PSEC_SELWRE_SCRATCH(0) = 1 indicating "HS Entry overlay" has finished
 *
 * @return FLCN_OK if every step is done successfully
 *         FLCN_ERR_HS_PR_ILLEGAL_LASSAHS_STATE_AT_HS_ENTRY if the LASSAHS stats is unexpected
 *         FLCN_ERR_INSUFFICIENT_PMB_PLM_PROTECTION if the PLM setting of the DMEM is incorrect
 *         Or just bubble up the error code from the callee
 */
FLCN_STATUS
prSbHsEntry(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       privMask   = 0;

    prMarkDataBlocksAsSelwre();

    CHECK_FLCN_STATUS(prSaveStackValues());

    if (!PR_LASSAHS_IS_STATE(LASSAHS_STATE_DEFAULT))
    {
        flcnStatus = FLCN_ERR_HS_PR_ILLEGAL_LASSAHS_STATE_AT_HS_ENTRY;
        goto ErrorExit;
    }

    prIlwalidateBlocksOutsideLassahs();

    CHECK_FLCN_STATUS(prVerifyLsSigGrp());

    // Lower the priv level of DMEM to 2 so that SE can work during LASSAHS
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CSEC_FALCON_DMEM_PRIV_LEVEL_MASK, &privMask));

    // Ensure the returned PLM is valid before caching it
    if (FLD_TEST_DRF(_CSEC, _FALCON_DMEM_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL0, _ENABLE, privMask) ||
        FLD_TEST_DRF(_CSEC, _FALCON_DMEM_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL2, _ENABLE, privMask) ||
        FLD_TEST_DRF(_CSEC, _FALCON_DMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE, privMask) ||
        FLD_TEST_DRF(_CSEC, _FALCON_DMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE, privMask))
    {
        flcnStatus = FLCN_ERR_INSUFFICIENT_PMB_PLM_PROTECTION;
        goto ErrorExit;
    }
    g_prLassahsData_Hs.falconDmemMask = privMask;

    privMask  = FLD_SET_DRF(_CSEC, _FALCON_DMEM_PRIV_LEVEL_MASK,
                          _READ_PROTECTION_LEVEL2, _ENABLE, privMask);
    privMask  = FLD_SET_DRF(_CSEC, _FALCON_DMEM_PRIV_LEVEL_MASK,
                          _WRITE_PROTECTION_LEVEL2, _ENABLE, privMask);
    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(LW_CSEC_FALCON_DMEM_PRIV_LEVEL_MASK, privMask));

    PR_LASSAHS_SET_STATE(LASSAHS_STATE_HS_ENTRY);

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief  Entry function of MPK decryption overlay for LASSAHS
 *
 * Steps for this overlay:
 * a. Ensure that revocation check passes (which is done by HS pre-check at the
 *    caller of this function)
 * b. Ensure that LSMODE is set (which is done by HS pre-check at the caller of
 *    this function)
 * c. Ensure that PSEC_SELWRE_SCRATCH(0) = 1 before proceeding
 * d. Decrypt MPK and save in DMEM variable
 * e. Set PSEC_SELWRE_SCRATCH(0) = 2 indicating "MPK decryption overlay" has
 *    finished
 *
 * @return FLCN_OK if every step is done successfully
 *         FLCN_ERR_HS_PR_ILLEGAL_LASSAHS_STATE_AT_MPK_DECRYPT if the LASSAHS stats is unexpected
 *         Or just bubble up the error code from the callee
 */
FLCN_STATUS
prSbHsDecryptMPK(LwU32 *pRgbModelPrivKey)
{
    FLCN_STATUS status = FLCN_OK;

    if (!PR_LASSAHS_IS_STATE(LASSAHS_STATE_HS_ENTRY))
    {
        return FLCN_ERR_HS_PR_ILLEGAL_LASSAHS_STATE_AT_MPK_DECRYPT;
    }

    // XXX: Need to figure out a macro to collapse the following 3 lines into one
    status = prDecryptMpk_HAL(&PrHal, pRgbModelPrivKey);
    if (status != FLCN_OK)
    {
        goto ErrExit;
    }

    PR_LASSAHS_SET_STATE(LASSAHS_STATE_DECRYPT_MPK);

ErrExit:
    return status;
}

/*!
 * @brief  Entry function of HS exit overlay for LASSAHS
 *
 * Steps for this overlay:
 * a. Ensure that revocation check passes. (which is done by HS pre-check at the
 *    caller of this function)
 * b. Ensure that LSMODE is set. (which is done by HS pre-check at the caller of
 *    this function)
 * c. Ensure that PSEC_SELWRE_SCRATCH(0) = 2.
 * d. Restore DMEM's PLM to level 3
 * e. Set PSEC_SELWRE_SCRATCH(0) = 0

 * f. Scrub the stack
 * g. Unmark data block containing PR_LASSAHS_DATA_HS from being level3 secure
 * h. Clear the lockdown to prepare for exit into LS (which is done at the
 *    caller of this function)
 *
 * @return FLCN_OK if every step is done successfully
 *         FLCN_ERR_HS_PR_ILLEGAL_LASSAHS_STATE_AT_HS_EXIT if the LASSAHS stats is unexpected
 *         FLCN_ERR_INSUFFICIENT_PMB_PLM_PROTECTION if the PLM setting of the DMEM is incorrect
 *         Or just bubble up the error code from the callee
 */
FLCN_STATUS
prSbHsExit(void)
{
    LwU32       privMask   = 0;
    FLCN_STATUS flcnStatus = FLCN_OK;

    if (!PR_LASSAHS_IS_STATE(LASSAHS_STATE_DECRYPT_MPK))
    {
        flcnStatus = FLCN_ERR_HS_PR_ILLEGAL_LASSAHS_STATE_AT_HS_EXIT;
        goto ErrorExit;
    }

    // Restore the priv level mask of the DMEM
    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(LW_CSEC_FALCON_DMEM_PRIV_LEVEL_MASK, g_prLassahsData_Hs.falconDmemMask));

    // Ensure PLM is correctly restored
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CSEC_FALCON_DMEM_PRIV_LEVEL_MASK, &privMask));

    if (privMask != g_prLassahsData_Hs.falconDmemMask)
    {
        flcnStatus = FLCN_ERR_INSUFFICIENT_PMB_PLM_PROTECTION;
        goto ErrorExit;
    }

    PR_LASSAHS_SET_STATE(LASSAHS_STATE_DEFAULT);

    // Clear the data before exit HS mode
    OS_SEC_HS_LIB_VALIDATE(libMemHs, HASH_SAVING_DISABLE);
    memset_hs((void *)&g_prLassahsData_Hs,     0, sizeof(PR_LASSAHS_DATA_HS));
    memset_hs((void *)&g_prLassahsUcodeSig_Hs, 0, PR_LS_GRP_SIG_SIZE_BYTES);
    memset_hs((void *)&g_prLassahsUcodeBuf_Hs, 0, PR_LS_SIG_GRP_BUF_SIZE_BYTES);

    if (g_prLassahsData_Hs.bStackCfgSupported)
    {
        prStkScrub();
    }

    prUnmarkDataBlocksAsSelwre();

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief  Save the SP and STACKCFG.Bottom register.  This will be used later to
 * scrub the stack after LASSAHS.
 *
 */
FLCN_STATUS
prSaveStackValues(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Save the SP.  "saveSp" is the first entry in g_prLassahsData_Hs
    asm
    (
        "rspr a9 SP;"                      // Save the SP in a9
        "mvi a0 _g_prLassahsData_Hs;"      // Move the address of "saveSP" to a0
        "std.w a0 a9;"                     // Do the write to "saveSP"
    );

    //
    // Get STACKCFG.Bottom value in bytes.
    // Save the return type to verify bottom is a valid value.
    // If STACKCFG is not supported,  then our implementation for
    // stack scrubber won't work and can't be called.
    //
    CHECK_FLCN_STATUS(prUpdateStackBottom_HAL(&PrHal, &g_prLassahsData_Hs.bStackCfgSupported));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief  Scrubs memory from bottom of the stack to saved SP
 *         Stack grows downward so scrub happens from bottom up to saved SP
 *
 */
void prStkScrub(void)
{
    LwU32  scrubSize;

    scrubSize = (g_prLassahsData_Hs.saveSp) - g_prLassahsData_Hs.stackCfgBottom;

    //
    // The stack grows downward from higher memory address to lower memory addresses.
    // STACKCFG_BOTTOM  holds the  lowest memory address the stack can grow to
    // If the SP is less than or equal to STACKCFG_BOTTOM, an exception is generated.
    // For security reasons,  we want to clear the data between the saved SP and the
    // current SP.  To makes things easier,  just clear the data between STACKCFG_BOTTOM
    // and SAVED_SP.
    //
    //     ___________________________________ addr = DMEM_MAX
    //     |                                   |
    //     |                                   |
    //     |                                   |
    //     |___________________________________|   | STACK ORIGIN
    //     |                                   |
    //     |                                   |
    //     |                                   |
    //     |                                   |
    //     |         --Saved SP--              |   | SAVED STACK POINTER
    //     |                                   |
    //     |                                   |
    //     |                                   |
    //     |                                   |
    //     |            --SP--                 |   |  CURRENT STACK POINTER
    //     |                                   |
    //     |                                   |
    //     |                                   |
    //     |___________________________________|   | STACKCFG_BOTTOM
    //     |                                   |
    //     |                                   |
    //     |                                   |
    //     |                                   |
    //     |___________________________________| addr = 0
    //

    memset_hs((unsigned int *)g_prLassahsData_Hs.stackCfgBottom, 0, scrubSize);
}

/* ------------------------- Private Functions ------------------------------ */

/*
 * Check if imem block is part of LASSAHS overlays
 */
LwBool
prIsImemBlockPartOfLassahsOverlays(LwU32 blockAddr)
{
#ifndef PR_V0404
    if (((blockAddr >= (LwU32)_resident_code_start)              && (blockAddr < (LwU32)_resident_code_end_aligned))       ||
        ((blockAddr >= (LwU32)_load_start_imem_libMemHs)         && (blockAddr < (LwU32)_load_stop_imem_libMemHs))         ||
        ((blockAddr >= (LwU32)_load_start_imem_libCommonHs)      && (blockAddr < (LwU32)_load_stop_imem_libCommonHs))      ||
        ((blockAddr >= (LwU32)_load_start_imem_prHsCommon)       && (blockAddr < (LwU32)_load_stop_imem_prHsCommon))       ||
        ((blockAddr >= (LwU32)_load_start_imem_prSbHsEntry)      && (blockAddr < (LwU32)_load_stop_imem_prSbHsEntry))      ||
        ((blockAddr >= (LwU32)_load_start_imem_prSbHsExit)       && (blockAddr < (LwU32)_load_stop_imem_prSbHsExit))       ||
        ((blockAddr >= (LwU32)_load_start_imem_prSbHsDecryptMPK) && (blockAddr < (LwU32)_load_stop_imem_prSbHsDecryptMPK)) ||
        ((blockAddr >= (LwU32)_load_start_imem_prGdkLs1)         && (blockAddr < (LwU32)_load_stop_imem_prGdkLs1))         ||
        ((blockAddr >= (LwU32)_load_start_imem_prGdkLs2)         && (blockAddr < (LwU32)_load_stop_imem_prGdkLs2))         ||
        ((blockAddr >= (LwU32)_load_start_imem_prGdkLs3)         && (blockAddr < (LwU32)_load_stop_imem_prGdkLs3))         ||
        ((blockAddr >= (LwU32)_load_start_imem_prGdkLs4)         && (blockAddr < (LwU32)_load_stop_imem_prGdkLs4))         ||
        ((blockAddr >= (LwU32)_load_start_imem_prGdkLs5)         && (blockAddr < (LwU32)_load_stop_imem_prGdkLs5))         ||
        ((blockAddr >= (LwU32)_load_start_imem_prGdkLs6)         && (blockAddr < (LwU32)_load_stop_imem_prGdkLs6))         ||
        ((blockAddr >= (LwU32)_load_start_imem_libSE)            && (blockAddr < (LwU32)_load_stop_imem_libSE)))
    {
        return LW_TRUE;
    }
#else
    if (((blockAddr >= (LwU32)_resident_code_start)              && (blockAddr < (LwU32)_resident_code_end_aligned)) ||
        ((blockAddr >= (LwU32)_load_start_imem_libMemHs)         && (blockAddr < (LwU32)_load_stop_imem_libMemHs)) ||
        ((blockAddr >= (LwU32)_load_start_imem_libCommonHs)      && (blockAddr < (LwU32)_load_stop_imem_libCommonHs)) ||
        ((blockAddr >= (LwU32)_load_start_imem_prHsCommon)       && (blockAddr < (LwU32)_load_stop_imem_prHsCommon)) ||
        ((blockAddr >= (LwU32)_load_start_imem_prSbHsEntry)      && (blockAddr < (LwU32)_load_stop_imem_prSbHsEntry)) ||
        ((blockAddr >= (LwU32)_load_start_imem_prSbHsExit)       && (blockAddr < (LwU32)_load_stop_imem_prSbHsExit)) ||
        ((blockAddr >= (LwU32)_load_start_imem_prSbHsDecryptMPK) && (blockAddr < (LwU32)_load_stop_imem_prSbHsDecryptMPK)) ||
        ((blockAddr >= (LwU32)_load_start_imem_prGdkLs1_44)      && (blockAddr < (LwU32)_load_stop_imem_prGdkLs1_44)) ||
        ((blockAddr >= (LwU32)_load_start_imem_prGdkLs2_44)      && (blockAddr < (LwU32)_load_stop_imem_prGdkLs2_44)) ||
        ((blockAddr >= (LwU32)_load_start_imem_prGdkLs3_44)      && (blockAddr < (LwU32)_load_stop_imem_prGdkLs3_44)) ||
        ((blockAddr >= (LwU32)_load_start_imem_prGdkLs4_44)      && (blockAddr < (LwU32)_load_stop_imem_prGdkLs4_44)) ||
        ((blockAddr >= (LwU32)_load_start_imem_prGdkLs5_44)      && (blockAddr < (LwU32)_load_stop_imem_prGdkLs5_44)) ||
        ((blockAddr >= (LwU32)_load_start_imem_prGdkLs6_44)      && (blockAddr < (LwU32)_load_stop_imem_prGdkLs6_44)) ||
        ((blockAddr >= (LwU32)_load_start_imem_libSE)            && (blockAddr < (LwU32)_load_stop_imem_libSE)))
    {
        return LW_TRUE;
    }
#endif

    return LW_FALSE;
}

/*
 * Ilwalidate all imem blocks except the ones required for LASSAHS
 */
void
prIlwalidateBlocksOutsideLassahs(void)
{
    LwU32 tmp          = 0;
    LwU32 blockInfo    = 0;
    LwU32 blockAddr    = 0;

    //
    // Loop through all IMEM blocks and ilwalidate those that are not reqd for LASSAHS
    //
    for (tmp = 0; tmp < (LwU32)_num_imem_blocks; tmp++)
    {
        falc_imblk(&blockInfo, tmp);

        if (!(IMBLK_IS_ILWALID(blockInfo)))
        {
            blockAddr = IMBLK_TAG_ADDR(blockInfo);

            if (prIsImemBlockPartOfLassahsOverlays(blockAddr))
            {
                continue;
            }
            else
            {
                falc_imilw(tmp);

                g_ilwalidatedBlocks.bitmap[tmp/32] |= BIT(tmp % 32);
                g_ilwalidatedBlocks.blkAddr[tmp]    = blockAddr;
            }
        }
    }
}

/*
 * Mark necessary data blocks for state storage as Secure
 */
void
prMarkDataBlocksAsSelwre(void)
{
    LwU32 addr      = (LwU32)(&(g_prLassahsData_Hs));
    LwU32 numBlocks = sizeof(g_prLassahsData_Hs)/DMEM_BLOCK_SIZE;
    LwU32 dmtag     = 0;
    LwU32 count     = 0;

    falc_dmtag(&dmtag, addr);

    if (!DMTAG_IS_MISS(dmtag))
    {
        LwU16 blockIdx = DMTAG_IDX_GET(dmtag);

        for (count = 0; count < numBlocks; count++)
        {
            falc_dmwait();
            falc_dmlvl(blockIdx, PR_DMEM_ACCESS_ALLOW_LEVEL3);    // mark as secure for level 0,1, and 2
            blockIdx++;
        }
    }
    else
    {
        // We should NEVER hit miss, but adding this as a precaution.
        OS_HALT();
    }
}

/*
 * Unmark necessary data blocks for state storage as secure
 */
void
prUnmarkDataBlocksAsSelwre(void)
{
    LwU32 addr      = (LwU32)(&(g_prLassahsData_Hs));
    LwU32 numBlocks = sizeof(g_prLassahsData_Hs)/DMEM_BLOCK_SIZE;
    LwU32 dmtag     = 0;
    LwU32 count     = 0;

    falc_dmtag(&dmtag, addr);

    if (!DMTAG_IS_MISS(dmtag))
    {
        LwU16 blockIdx = DMTAG_IDX_GET(dmtag);

        for (count = 0; count < numBlocks; count++)
        {
            falc_dmwait();
            falc_dmlvl(blockIdx, PR_DMEM_ACCESS_ALLOW_LEVEL2);    // mark as inselwre for level 2
            blockIdx++;
        }
    }
    else
    {
        // We should NEVER hit miss, but adding this as a precaution.
        OS_HALT();
    }
}

/*!
 * @brief Copies IMEM to DMEM using SCP.
 */
static FLCN_STATUS _prReadFromImemUsingScp(LwU32 va, LwU16 noOfBytes, LwU32 *pOut)
{
    LwU32 dmwaddr;
    LwU32 dewaddr;
    LwU32 blk;
    LwU32 imemOff;
    LwU32 destOff = osDmemAddressVa2PaGet(pOut);

    // Get the block for this VA
    falc_imtag(&blk, va);
    blk = IMTAG_IDX_GET(blk);
    imemOff = (IMEM_IDX_TO_ADDR(blk)) + (va & 0xFF);

    switch (noOfBytes)
    {
        case 16:
            dmwaddr = falc_sethi_i(imemOff, DMSIZE_16B);
            dewaddr = falc_sethi_i(destOff, DMSIZE_16B);
            break;
        case 32:
            dmwaddr = falc_sethi_i(imemOff, DMSIZE_32B);
            dewaddr = falc_sethi_i(destOff, DMSIZE_32B);
            break;
        case 64:
            dmwaddr = falc_sethi_i(imemOff, DMSIZE_64B);
            dewaddr = falc_sethi_i(destOff, DMSIZE_64B);
            break;
        case 128:
            dmwaddr = falc_sethi_i(imemOff, DMSIZE_128B);
            dewaddr = falc_sethi_i(destOff, DMSIZE_128B);
            break;
        case 256:
            dmwaddr = falc_sethi_i(imemOff, DMSIZE_256B);
            dewaddr = falc_sethi_i(destOff, DMSIZE_256B);
            break;
        default:
            return FLCN_ERR_ILWALID_ARGUMENT;
    }

    falc_scp_load_trace0(2);
    falc_scp_push(SCP_R3);
    falc_scp_fetch(SCP_R3);

    // Max 256B
    falc_scp_loop_trace0(noOfBytes/16);

    // Copy the block imem to push buffer
    falc_scp_trap_imem_suppressed(1);
    falc_trapped_dmwrite(dmwaddr);

    // Copy the block from fetch buffer to DMEM
    falc_scp_trap_suppressed(2);
    falc_trapped_dmread(dewaddr);
    falc_dmwait();

    // Done
    falc_scp_trap_imem_suppressed(0);
    falc_scp_trap_suppressed(0);

    return FLCN_OK;
}


/*!
 * @brief Callwlate DMhash with the given buffer
 */
static void _prCallwlateDmhash
(
    LwU8           *pHash,
    LwU8           *pData,
    LwU32          size
)
{
    LwU32  doneSize = 0;
    LwU8   *pKey;
    LwU32  keyPa;
    LwU32  hashPa = osDmemAddressVa2PaGet(pHash);

    while (doneSize < size)
    {
        pKey = (LwU8 *)&(pData[doneSize]);
        keyPa = osDmemAddressVa2PaGet(pKey);

        // H_i = H_{i-1} XOR encrypt(key = m_i, pMessage = H_{i-1})
        falc_scp_trap(TC_INFINITY);
        falc_trapped_dmwrite(falc_sethi_i((LwU32)keyPa, SCP_R2));
        falc_dmwait();
        falc_trapped_dmwrite(falc_sethi_i((LwU32)hashPa, SCP_R3));
        falc_dmwait();
        falc_scp_key(SCP_R2);
        falc_scp_encrypt(SCP_R3, SCP_R4);
        falc_scp_xor(SCP_R4, SCP_R3);
        falc_trapped_dmread(falc_sethi_i((LwU32)hashPa, SCP_R3));
        falc_dmwait();
        falc_scp_trap(0);

        doneSize += 16;
    }
}

/*!
 * @brief Loops over 256B in given overlay and computes DMHASH
 */
static FLCN_STATUS
prCallwlateDmHashForOverlay
(
    LwU32 overlayStartVa,
    LwU32 overlayEndVa
)
{
    LwU32       j      = 0;
    FLCN_STATUS status = FLCN_ERR_ILWALID_ARGUMENT;

    for (j = overlayStartVa; j < overlayEndVa; j += PR_LS_SIG_GRP_BUF_SIZE_BYTES)
    {
        if ((status = _prReadFromImemUsingScp(j, PR_LS_SIG_GRP_BUF_SIZE_BYTES, (LwU32*)g_prLassahsUcodeBuf_Hs)) != FLCN_OK)
        {
            goto label_return;
        }
        _prCallwlateDmhash((LwU8*)g_prLassahsUcodeSig_Hs,(LwU8*)g_prLassahsUcodeBuf_Hs, PR_LS_SIG_GRP_BUF_SIZE_BYTES);
    }

label_return:
    return status;
}

/*!
 * @brief Insert start into ovlStartArr in sorted order. Also inserts end to ovlEndArr in same order
 */
static void
prInsertInOrder(LwU32 ovlStartArr[], LwU32 ovlEndArr[], LwU32 *pLwrPos, LwU32 start, LwU32 end)
{
    LwU32 i = (*pLwrPos);
    while ((i != 0) && (start < ovlStartArr[i-1]))
    {
        ovlStartArr[i] = ovlStartArr[i-1];
        ovlEndArr[i] = ovlEndArr[i-1];
        i--;
    }
    ovlStartArr[i] = start;
    ovlEndArr[i]   = end;
    (*pLwrPos)+=1;
}

/*!
 * @brief Callwlates DMHASH for a list of overlays and compare the sig with ACR generated sig
 */
static FLCN_STATUS
prVerifyLsSigGrp(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       tmp    = 0;
    LwU32       i      = 0;
    LwU32       lwrPos = 0;

    // Using 2 single-D arrays to avoid memcpy
    LwU32       ovlStartArr[PR_LS_GRP_MAX_OVERLAYS];
    LwU32       ovlEndArr[PR_LS_GRP_MAX_OVERLAYS];

    OS_SEC_HS_LIB_VALIDATE(libMemHs, HASH_SAVING_DISABLE);
    memset_hs((void *)ovlStartArr, 0, sizeof(ovlStartArr));
    memset_hs((void *)ovlEndArr,   0, sizeof(ovlEndArr));

#ifndef PR_V0404
    // Sort the overlays in ascending order same as how ACR callwlated the signature
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_resident_code_start, (LwU32)_resident_code_end_aligned);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_libSE, (LwU32)_load_stop_imem_libSE);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_libMemHs, (LwU32)_load_stop_imem_libMemHs);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_libCommonHs, (LwU32)_load_stop_imem_libCommonHs);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prHsCommon, (LwU32)_load_stop_imem_prHsCommon);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prSbHsEntry, (LwU32)_load_stop_imem_prSbHsEntry);
    // prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prSbHsExit, (LwU32)_load_stop_imem_prSbHsExit);
    // prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prSbHsDecryptMPK, (LwU32)_load_stop_imem_prSbHsDecryptMPK);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prGdkLs1, (LwU32)_load_stop_imem_prGdkLs1);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prGdkLs2, (LwU32)_load_stop_imem_prGdkLs2);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prGdkLs3, (LwU32)_load_stop_imem_prGdkLs3);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prGdkLs4, (LwU32)_load_stop_imem_prGdkLs4);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prGdkLs5, (LwU32)_load_stop_imem_prGdkLs5);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prGdkLs6, (LwU32)_load_stop_imem_prGdkLs6);
#else
    // Sort the overlays in ascending order same as how ACR callwlated the signature
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_resident_code_start, (LwU32)_resident_code_end_aligned);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_libSE, (LwU32)_load_stop_imem_libSE);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_libMemHs, (LwU32)_load_stop_imem_libMemHs);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_libCommonHs, (LwU32)_load_stop_imem_libCommonHs);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prHsCommon, (LwU32)_load_stop_imem_prHsCommon);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prSbHsEntry, (LwU32)_load_stop_imem_prSbHsEntry);
    // prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prSbHsExit, (LwU32)_load_stop_imem_prSbHsExit);
    // prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prSbHsDecryptMPK, (LwU32)_load_stop_imem_prSbHsDecryptMPK);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prGdkLs1_44, (LwU32)_load_stop_imem_prGdkLs1_44);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prGdkLs2_44, (LwU32)_load_stop_imem_prGdkLs2_44);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prGdkLs3_44, (LwU32)_load_stop_imem_prGdkLs3_44);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prGdkLs4_44, (LwU32)_load_stop_imem_prGdkLs4_44);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prGdkLs5_44, (LwU32)_load_stop_imem_prGdkLs5_44);
    prInsertInOrder(ovlStartArr, ovlEndArr, &lwrPos, (LwU32)_load_start_imem_prGdkLs6_44, (LwU32)_load_stop_imem_prGdkLs6_44);
#endif

    // If number of overlays is higher than expected, return
    if (lwrPos >= PR_LS_GRP_MAX_OVERLAYS)
    {
        status = FLCN_ERR_HS_PR_LASSAHS_LS_SIG_GRP_OVERLAYS_CNT;
        goto label_return;
    }

    // Loop through sorted overlays and callwlate hash
    while (i < lwrPos)
    {
        if ((status = prCallwlateDmHashForOverlay(ovlStartArr[i], ovlEndArr[i])) != FLCN_OK)
        {
            return status;
        }
        i++;
    }

    //Compare only the first 2 DWORDS of signature
    status = BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_SELWRE_SCRATCH_0, &tmp);
    if (status != FLCN_OK)
    {
        goto label_return;
    }
    if (tmp == ((LwU32*)g_prLassahsUcodeSig_Hs)[0])
    {
        status = BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_SELWRE_SCRATCH_1, &tmp);
        if (status != FLCN_OK)
        {
            goto label_return;
        }

        if (tmp == ((LwU32*)g_prLassahsUcodeSig_Hs)[1])
        {
            status = FLCN_OK;
            goto label_return;
        }
    }

    // Comparison failed
    status = FLCN_ERR_HS_PR_LASSAHS_LS_SIG_GRP_MISMATCH;

label_return:

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Entry function of the .prSbHsEntry HS lib. This function merely returns.
 *        It is used to validate the overlay blocks corresponding to the secure
 *        overlay before jumping to arbitrary functions inside it. The linker
 *        script should make sure that it puts this section at the top of the
 *        overlay to ensure that we can jump to the overlay's 0th offset before
 *        exelwting code at arbitrary offsets inside the overlay.
 */
static void
_prSbHsEntry_Entry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();
    return;
}

/*!
 * @brief Entry function of the .prSbHsDecryptMPK HS lib. This function merely returns.
 *        It is used to validate the overlay blocks corresponding to the secure
 *        overlay before jumping to arbitrary functions inside it. The linker
 *        script should make sure that it puts this section at the top of the
 *        overlay to ensure that we can jump to the overlay's 0th offset before
 *        exelwting code at arbitrary offsets inside the overlay.
 */
static void
_prSbHsDecryptMPK_Entry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();
    return;
}

/*!
 * @brief Entry function of the .prSbHsExit HS lib. This function merely returns.
 *        It is used to validate the overlay blocks corresponding to the secure
 *        overlay before jumping to arbitrary functions inside it. The linker
 *        script should make sure that it puts this section at the top of the
 *        overlay to ensure that we can jump to the overlay's 0th offset before
 *        exelwting code at arbitrary offsets inside the overlay.
 */
static void
_prSbHsExit_Entry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();
    return;
}

