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
 * @file   pr_lassahs.c
 *
 * LS code for LASS As HS (LS At Same Security As HS)
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pr/pr_lassahs.h"
#include "pr/pr_common.h"
#include "config/g_pr_hal.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
PR_LASSAHS_DATA           g_prLassahsData     PR_ATTR_DATA_OVLY(_g_prLassahsData)     = { 0 };
PR_ILWALIDATE_BLOCKS_DATA g_ilwalidatedBlocks PR_ATTR_DATA_OVLY(_g_ilwalidatedBlocks) = {{0}};

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief  Alloc memory from heap of passed in size.  Save the address
 * in LASSAHS data to free later.
 * 
 * This is called in the pre entry to LASSAHS and is a LS mode function.
 *
 * @return FLCN_OK if the required memory is allocated
 *         FLCN_ERR_NO_FREE_MEM otherwise
 */
FLCN_STATUS
prPreAllocMemory(LwU32 allocSize)
{
    g_prLassahsData.pPreAllocHeapAddress = pvPortMallocFreeable(allocSize);
    if (g_prLassahsData.pPreAllocHeapAddress == NULL)
    {
        return FLCN_ERR_NO_FREE_MEM;
    }

    return FLCN_OK;
}

/*!
 * @brief  Free saved memory from address saved in LASSAHS
 *
 * This is called in the pre entry to LASSAHS and is a LS mode function.
 */
void
prFreePreAllocMemory(void)
{
    vPortFreeFreeable(g_prLassahsData.pPreAllocHeapAddress);
}

/*
 * @brief  Loads the IMEM overlays used for LASSAHS
 */
void
prLoadLassahsImemOverlays(void)
{
    //
    // In this function we need to load all requried overlays before starting
    // running LASSAHS code. But .libCommonHs is an exception since it's a
    // resident HS overlay thus we don't need to explicitly load it here.
    //
#ifndef PR_V0404
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD_HS(_IMEM, _ATTACH, prSbHsEntry)
        OSTASK_OVL_DESC_BUILD   (_IMEM, _ATTACH, prGdkLs1)
        OSTASK_OVL_DESC_BUILD   (_IMEM, _ATTACH, prGdkLs2)
        OSTASK_OVL_DESC_BUILD   (_IMEM, _ATTACH, prGdkLs3)
        OSTASK_OVL_DESC_BUILD   (_IMEM, _ATTACH, prGdkLs4)
        OSTASK_OVL_DESC_BUILD   (_IMEM, _ATTACH, prGdkLs5)
        OSTASK_OVL_DESC_BUILD   (_IMEM, _ATTACH, prGdkLs6)
        OSTASK_OVL_DESC_BUILD_HS(_IMEM, _ATTACH, prHsCommon)
        OSTASK_OVL_DESC_BUILD_HS(_IMEM, _ATTACH, prSbHsDecryptMPK)
        OSTASK_OVL_DESC_BUILD_HS(_IMEM, _ATTACH, prSbHsExit)
        OSTASK_OVL_DESC_BUILD   (_IMEM, _ATTACH, libSE)
        OSTASK_OVL_DESC_BUILD_HS(_IMEM, _ATTACH, libMemHs)
    };
#else
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD_HS(_IMEM, _ATTACH, prSbHsEntry)
        OSTASK_OVL_DESC_BUILD   (_IMEM, _ATTACH, prGdkLs1_44)
        OSTASK_OVL_DESC_BUILD   (_IMEM, _ATTACH, prGdkLs2_44)
        OSTASK_OVL_DESC_BUILD   (_IMEM, _ATTACH, prGdkLs3_44)
        OSTASK_OVL_DESC_BUILD   (_IMEM, _ATTACH, prGdkLs4_44)
        OSTASK_OVL_DESC_BUILD   (_IMEM, _ATTACH, prGdkLs5_44)
        OSTASK_OVL_DESC_BUILD   (_IMEM, _ATTACH, prGdkLs6_44)
        OSTASK_OVL_DESC_BUILD_HS(_IMEM, _ATTACH, prHsCommon)
        OSTASK_OVL_DESC_BUILD_HS(_IMEM, _ATTACH, prSbHsDecryptMPK)
        OSTASK_OVL_DESC_BUILD_HS(_IMEM, _ATTACH, prSbHsExit)
        OSTASK_OVL_DESC_BUILD   (_IMEM, _ATTACH, libSE)
        OSTASK_OVL_DESC_BUILD_HS(_IMEM, _ATTACH, libMemHs)
    };
#endif

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    
    // Yield to ensure all the attached overlays got loaded
    lwrtosYIELD();
}

/*
 * @brief Revalidate blocks which were ilwalidated during LASSAHS as they were not required
 */
FLCN_STATUS
prRevalidateImemBlocks(void)
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

        if (IMBLK_IS_ILWALID(blockInfo) && !(IMTAG_IS_SELWRE(blockInfo)))
        {
            if ((g_ilwalidatedBlocks.bitmap[tmp/32] & BIT(tmp % 32)) != 0)
            {
                blockAddr = g_ilwalidatedBlocks.blkAddr[tmp]; 

                falc_imread(blockAddr, IMEM_IDX_TO_ADDR(tmp));
                falc_imwait();
            }
        }
    }

    return FLCN_OK;
}


/*
 * @brief  Unload all PR overlays which were not used in LASSAHS
 */
void
prUnloadOverlaysIlwalidatedDuringLassahs(void)
{
    //
    // This is required because when we ilwalidate all overlays except the overlays required for LASSAHS,
    // RTOS is not aware of this, so it fails in its logic of auto-loading overlays through MRU list
    // For this issue, we are validating the blocks again by doing an imread
    // To maintain overlay structure, we should detach these validated overlays, otherwise we might get a double hit
    //
#ifndef PR_V0404
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, prPreEntryGdkLs)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, prPostExitGdkLs)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr1)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr2)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr3)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr4)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr5)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr6)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr7)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr8)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr9)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr10)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr11)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr12)
    };
#else
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, prPreEntryGdkLs_44)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, prPostExitGdkLs_44)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr1_44)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr2_44)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr3_44)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr4_44)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr5_44)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr6_44)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr7_44)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr8_44)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr9_44)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr10_44)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr11_44)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr12_44)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pr13_44)
    };
#endif

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
}

/*!
 * @brief  Loads the DMEM overlays used for LASSAHS
 */
void
prLoadLassahsDmemOverlays(void)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pr)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);

    // Yield to ensure the attached overlay got loaded
    lwrtosYIELD();
}

/* ------------------------- Private Functions ------------------------------ */

