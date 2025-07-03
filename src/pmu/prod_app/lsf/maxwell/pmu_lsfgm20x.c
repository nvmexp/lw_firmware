/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_lsfgm20x.c
 * @brief  Light Secure Falcon (LSF) GM20X Hal Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "pmu/pmumailboxid.h"
/* ------------------------ Application includes --------------------------- */
#include "pmu_objlsf.h"
#include "rmlsfm.h"
#include "main.h"
#include "pmu_objpg.h"

#include "config/g_lsf_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */


/*!
 * @brief Initializes Light Secure Falcon settings.
 */
FLCN_STATUS
lsfInit_GM20X(void)
{
    FLCN_STATUS status;

    // Before we change anything, ensure secure.
    if (!lsfVerifyFalconSelwreRunMode_HAL(&(Lsf.hal)))
    {
        // Returning FLCN_OK here, given that we used to silently return no error
        status = FLCN_OK;
        goto lsfInit_GM20X_exit;
    }

    // Setup DMEM carveout regions
    lsfInitDmemCarveoutRegions_HAL(&(Lsf.hal));

    // Setup aperture settings (protected TRANSCONFIG regs)
    lsfInitApertureSettings_HAL(&(Lsf.hal));

    // Copy BRSS from BSI RAM to brssData
    status = lsfInitBrssData_HAL(&(Lsf.hal));
    if (status != FLCN_OK)
    {
        // Ignore the error as this may have been failing in the past.
        status = FLCN_OK;
    }

    // Ensure that PRIV_SEC is enabled
    status = lsfVerifyPrivSecEnabled_HAL(&(Lsf.hal));
    if (status != FLCN_OK)
    {
        goto lsfInit_GM20X_exit;
    }

lsfInit_GM20X_exit:
    return status;
}

/*!
 * @brief Verifies this Falcon is running in secure mode.
 */
LwBool
lsfVerifyFalconSelwreRunMode_GM20X(void)
{
    LwU32 data32;

    // Verify that we are in LSMODE
    data32 = REG_RD32(CSB, LW_CPWR_FALCON_SCTL);
    if (FLD_TEST_DRF(_CPWR, _FALCON_SCTL, _LSMODE, _TRUE, data32))
    {
        // Verify that UCODE_LEVEL > the init value
        if (DRF_VAL(_CPWR, _FALCON_SCTL, _UCODE_LEVEL,  data32) >
            LW_CPWR_FALCON_SCTL_UCODE_LEVEL_INIT)
        {
            //
            // Verify that the FBIF REGIONCTL for TRANSCFG(ucode) > the init
            // value
            //
            data32 = REG_RD32(CSB, LW_CPWR_FBIF_REGIONCFG);
            if (DRF_IDX_VAL(_CPWR, _FBIF_REGIONCFG, _T, RM_PMU_DMAIDX_UCODE,
                            data32) == LSF_WPR_EXPECTED_REGION_ID)
            {
                //
                // Falcon (PMU) IS secure.
                // Clear rest of REGIONCFG to INIT value (0)
                //
                data32 = data32 &
                    FLD_IDX_SET_DRF_NUM(_CPWR, _FBIF_REGIONCFG, _T,
                                        RM_PMU_DMAIDX_UCODE,
                                        LSF_WPR_EXPECTED_REGION_ID, 0x0);
                REG_WR32(CSB, LW_CPWR_FBIF_REGIONCFG, data32);

                return LW_TRUE;
            }
        }
    }

    return LW_FALSE;
}

/*!
 * @brief Setup aperture settings (protected TRANSCONFIG regs)
 */
void
lsfInitApertureSettings_GM20X(void)
{
    //
    // The following setting updates only take affect if the
    // the PMU is LSMODE && UCODE_LEVEL > 0
    //

    // UCODE aperture
    REG_WR32(CSB, LW_CPWR_FBIF_TRANSCFG(RM_PMU_DMAIDX_UCODE),
             (DRF_DEF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CPWR, _FBIF_TRANSCFG, _TARGET,   _LOCAL_FB)));

    // Generic PHYS_VID aperture
    REG_WR32(CSB, LW_CPWR_FBIF_TRANSCFG(RM_PMU_DMAIDX_PHYS_VID),
             (DRF_DEF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CPWR, _FBIF_TRANSCFG, _TARGET,   _LOCAL_FB)));

    // Generic COHERENT_SYSMEM aperture
    REG_WR32(CSB, LW_CPWR_FBIF_TRANSCFG(RM_PMU_DMAIDX_PHYS_SYS_COH),
             (DRF_DEF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CPWR, _FBIF_TRANSCFG, _TARGET,   _COHERENT_SYSMEM)));

    // Generic NONCOHERENT_SYSMEM aperture
    REG_WR32(CSB, LW_CPWR_FBIF_TRANSCFG(RM_PMU_DMAIDX_PHYS_SYS_NCOH),
             (DRF_DEF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CPWR, _FBIF_TRANSCFG, _TARGET,   _NONCOHERENT_SYSMEM)));
}

/*!
 * Ensure the BRSS header has the right checksum.
 * NOTE: Checksum logic copied from VBIOS source
 */
static FLCN_STATUS
s_lsfInitVerifyBrssChecksum(LwU8 *pBrssBuf, LwU16 size, LwU8 givenChecksum)
{
    LwU16 ind      = 0;
    LwU16 checksum = 0;

    for (ind = 0; ind < size; ind++)
    {
        checksum += (LwU16)(pBrssBuf[ind]);
        checksum &= 0xFFU;
    }

    // Magic number used by vbios code
    checksum = 0x100U - checksum;

    return (givenChecksum == checksum)? FLCN_OK : FLCN_ERROR;
}

/*!
 * @brief Copy BRSS struct from BSI RAM into ACR BRSS data
 */
FLCN_STATUS
lsfInitBrssData_GM20X(void)
{
    FLCN_STATUS                          status      = FLCN_OK;
    LwU32                                bsiRamSize  = 0;
    LwU32                                brssOff     = 0;
    LwU8                                 givenChkSum = 0;
    BSI_RAM_SELWRE_SCRATCH_DATA          brss;
    PBSI_RAM_SELWRE_SCRATCH_DATA         pBrss       = &brss;
    RM_PMU_BSI_RAM_SELWRE_SCRATCH_DATA  *pBrssPmu    = &(Lsf.brssData);

    // Initialize BRSS PMU structure
    pBrssPmu->bIsValid = LW_FALSE;
    pBrssPmu->status   = BRSS_PARSE_ERROR_UNKNOWN;

    // See if pg island is enabled or not
    if (!pgIslandIsEnabled_HAL(&Pg))
    {
        goto label_exit;
    }

    // Get BSI RAM size in bytes
    bsiRamSize = pgIslandGetBsiRamSize_HAL(&Pg);

    //
    // BRSS struct is stored at end of BSI RAM
    // Read the header first.
    //
    brssOff = bsiRamSize - sizeof(BSI_RAM_SELWRE_SCRATCH_HDR);

    if ((status = pgIslandBsiRamRW_HAL(&Pg, (LwU32*)&(pBrss->brssHdr),
        ((sizeof(BSI_RAM_SELWRE_SCRATCH_HDR))/(sizeof(LwU32))), brssOff, LW_TRUE)) == FLCN_OK)
    {

        // Check if BRSS header is present by looking at identifier
        if (pBrss->brssHdr.identifier != LW_BRSS_IDENTIFIER)
        {
            pBrssPmu->status = BRSS_PARSE_ERROR_UNAVAILABLE;
            goto label_exit;
        }

        // Version is known now, update the BRSS PMU struct
        pBrssPmu->version = pBrss->brssHdr.version;

        // Header is present. Read the entire structure based on the version
        brssOff = bsiRamSize - pBrss->brssHdr.size;
        switch (pBrss->brssHdr.version)
        {
            case LW_BRSS_VERSION_V1:
            case LW_BRSS_VERSION_V2:
                // Read the entire structure now. Header is read again to have clean code.
                if ((status = pgIslandBsiRamRW_HAL(&Pg, (LwU32*)&(pBrss->brssDataU),
                       (pBrss->brssHdr.size)/sizeof(LwU32), brssOff, LW_TRUE)) != FLCN_OK)
                {
                    pBrssPmu->status = BRSS_PARSE_ERROR_UNKNOWN;
                    goto label_exit;

                }

                givenChkSum                  = pBrss->brssDataU.V1.checksum;
                pBrss->brssDataU.V1.checksum = 0;

                // Verify the checksum to make sure we have the right structure
                if ((status = s_lsfInitVerifyBrssChecksum((LwU8*)&(pBrss->brssDataU), pBrss->brssHdr.size,
                          givenChkSum )) != FLCN_OK)
                {
                    pBrssPmu->status = BRSS_PARSE_ERROR_CHECKSUM_FAIL;
                    goto label_exit;
                }

                // Its a valid structure, mark the structure to be valid
                pBrssPmu->status      = BRSS_PARSE_OK;
                pBrssPmu->bIsValid    = LW_TRUE;

                // Copy 4 bytes of HULK data
                pBrssPmu->hulkData[0] = pBrss->brssDataU.V1.hulkData[0];
                pBrssPmu->hulkData[1] = pBrss->brssDataU.V1.hulkData[1];
                pBrssPmu->hulkData[2] = pBrss->brssDataU.V1.hulkData[2];
                pBrssPmu->hulkData[3] = pBrss->brssDataU.V1.hulkData[3];

                // Copy VPR data
                pBrssPmu->vprData[0]  = pBrss->brssDataU.V1.vprData[0];
                pBrssPmu->vprData[1]  = pBrss->brssDataU.V1.vprData[1];

                if (PMUCFG_FEATURE_ENABLED(PMU_SPI_DEVICE_ROM))
                {
                    // Initialize Inforom if present
                    lsfInitInforomData_HAL(&(Lsf.hal), (pBrss->brssDataU.V1.inforomData));
                }
                break;

            default:
                // Other versions are not supported yet. Error
                pBrssPmu->status = BRSS_PARSE_ERROR_VERSION_UNSUPPORTED;
                status = FLCN_ERROR;
                goto label_exit;
        }

    }

label_exit:
    return status;
}


/*!
 * @brief Copy BRSS struct to pointer
 *
 * @param[in] pBrssData  RM_PMU_BSI_RAM_SELWRE_SCRATCH_DATA pointer
 */
void
lsfCopyBrssData_GM20X
(
    RM_PMU_BSI_RAM_SELWRE_SCRATCH_DATA *pBrssData
)
{
    // Copy BRRS data
    *pBrssData = Lsf.brssData;
}

/*!
 * Checks whether an access using the specified dmaIdx is permitted.
 *
 * @param[in]  dmaIdx    DMA index to check
 *
 * @return     LwBool    LW_TRUE  if the access is permitted
 *                       LW_FALSE if the access is not permitted
 */
LwBool
lsfIsDmaIdxPermitted_GM20X
(
    LwU8 dmaIdx
)
{
    LwU32 val = REG_RD32(CSB, LW_CPWR_FBIF_REGIONCFG);

    return (DRF_IDX_VAL(_CPWR, _FBIF_REGIONCFG, _T, dmaIdx, val) == 
            LSF_UNPROTECTED_REGION_ID);
}
