/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dispflcnga10x.c
 * @brief  GSP(GA10X) Specific Hal Functions
 *
 * GSP HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#include "dev_master.h"
#include "dev_fuse.h"
#include "dispflcn_regmacros.h"
#include "dev_gsp_csb_addendum.h"
#include "gsp_csb.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "dev_disp.h"
#include "dev_disp_addendum.h"

#ifdef RUNTIME_HS_OVL_SIG_PATCHING
#include "rmlsfm_new_wpr_blob.h"
#include "mmu/mmucmn.h"
#include "dev_fb.h"
#endif // RUNTIME_HS_OVL_SIG_PATCHING

/* ------------------------- OpenRTOS Includes ------------------------------ */
/* ------------------------- Application Includes --------------------------- */
#include "gsp_bar0_hs.h"
#include "lwosselwreovly.h"

/* ------------------------- Type Definitions ------------------------------- */
#define GSP_GA10X_DEFAULT_UCODE_VERSION    (0x0)
#define GSP_GA10X_UCODE_VERSION            (0x1)
#if (DPU_PROFILE_ga10x)
#define CHIP_ID_AD102F                     0x0000019F
#endif

/* ------------------------- External Definitions --------------------------- */
#ifdef RUNTIME_HS_OVL_SIG_PATCHING
extern LwU32 _imem_hs_signatures[][SIG_SIZE_IN_BITS / 32];
extern LwU32 _num_imem_hs_overlays[];
#endif // RUNTIME_HS_OVL_SIG_PATCHING

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static inline void _hdcpCheckAllowedChip(LwU32 data32)
    GCC_ATTRIB_ALWAYSINLINE();

/* ------------------------- Implementation --------------------------------- */

/*!
 * This function is used to check if the chip on which code is running is correct.
 *
 * @param[in]  data32  LW_PMC_BOOT_42 read register value.
 */
static inline void 
_hdcpCheckAllowedChip
(
    LwU32 data32
)
{
    LwU32 chip;
    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, data32);

    if ((chip != LW_PMC_BOOT_42_CHIP_ID_GA102) &&
        (chip != LW_PMC_BOOT_42_CHIP_ID_GA103) &&
        (chip != LW_PMC_BOOT_42_CHIP_ID_GA104) &&
        (chip != LW_PMC_BOOT_42_CHIP_ID_GA106) &&
        (chip != LW_PMC_BOOT_42_CHIP_ID_GA107) &&
        (chip != LW_PMC_BOOT_42_CHIP_ID_GA102F)
#if (DPU_PROFILE_ga10x) 
        // Using GA10X FPGA binary for ADA FPGA as well
        && (chip != CHIP_ID_AD102F)
#endif
        )
    {
        DPU_HALT();
    }
}


/*
 * @brief HS function to make sure the chip is allowed to do HDCP
 */
void
dpuEnforceAllowedChipsForHdcpHS_GA10X(void)
{
    LwU32 data32 = 0;

    if (FLCN_OK != EXT_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &data32))
    {
        DPU_HALT();
    }

    _hdcpCheckAllowedChip(data32);
}

/*
 * @brief LS Function to make sure the chip is allowed to do HDCP 
 */
void
dpuEnforceAllowedChipsForHdcp_GA10X(void)
{
    LwU32 data32 = 0;

    data32 = EXT_REG_RD32(LW_PMC_BOOT_42);

    _hdcpCheckAllowedChip(data32);
}


/*!
 * @brief  HS function to get the SW Ucode version
 *
 * @params[out] pUcodeVersion  Pointer to sw fuse version. 
 * @returns     FLCN_STATUS    FLCN_OK on successfull exelwtion
 *                             Appropriate error status on failure.
 */
FLCN_STATUS
dpuGetUcodeSWVersionHS_GA10X
(
    LwU32* pUcodeVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       chip;
    LwU32       data32;

    if (pUcodeVersion == NULL)
    {
        flcnStatus = FLCN_ERR_HS_CHK_ILWALID_INPUT;
        goto ErrorExit;
    }

    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &data32));

    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, data32);

    if ((chip == LW_PMC_BOOT_42_CHIP_ID_GA102) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_GA103) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_GA104) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_GA106) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_GA107))
    {
        *pUcodeVersion = GSP_GA10X_UCODE_VERSION;
    }
    else
    {
        if (OS_SEC_FALC_IS_DBG_MODE())
        {
            *pUcodeVersion = GSP_GA10X_DEFAULT_UCODE_VERSION;
        }
        else
        {
            flcnStatus = FLCN_ERR_HS_PROD_MODE_NOT_YET_SUPPORTED;
        }
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief  HS function to get the HW fuse version
 *
 * @params[out] pFuseVersion  Pointer to hw fuse version. 
 * @returns     FLCN_STATUS   FLCN_OK on successfull exelwtion
 *                            Appropriate error status on failure.
 */
FLCN_STATUS
dpuGetHWFuseVersionHS_GA10X
(
    LwU32* pFuseVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       data32;

    if (pFuseVersion == NULL)
    {
        flcnStatus = FLCN_ERR_HS_CHK_ILWALID_INPUT;
        goto ErrorExit;
    }

    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_FUSE_UCODE_GSP_REV, &data32));
    *pFuseVersion = DRF_VAL(_FUSE, _OPT_FUSE_UCODE_GSP_REV, _DATA, data32);

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief  LS function to get the HW fuse version
 *
 * @params[out] pFuseVersion  Pointer to hw fuse version. 
 * @returns     FLCN_STATUS   FLCN_OK on successfull exelwtion
 *                            Appropriate error status on failure.
 */
FLCN_STATUS
dpuGetHWFuseVersion_GA10X
(
    LwU32* pFuseVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       data32;

    if (pFuseVersion == NULL)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    CHECK_FLCN_STATUS(EXT_REG_RD32_ERRCHK(LW_FUSE_OPT_FUSE_UCODE_GSP_REV, &data32));
    *pFuseVersion = DRF_VAL(_FUSE, _OPT_FUSE_UCODE_GSP_REV, _DATA, data32);

ErrorExit:
    return flcnStatus;
}


/*!
 * @brief  HS function to get the HW fpf version
 *
 * @params[out] pFpfVersion  Pointer to hw fpf fuse version. 
 * @returns     FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                           Appropriate error status on failure.
 */
FLCN_STATUS
dpuGetHWFpfVersionHS_GA10X
(
    LwU32* pFpfVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
    LwU32       count      = 0;
    LwU32       fpfFuse    = 0;

    if (!pFpfVersion)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_FPF_UCODE_GSP_REV, &fpfFuse));
    fpfFuse = DRF_VAL(_FUSE, _OPT_FPF_UCODE_GSP_REV, _DATA, fpfFuse);

    /*
     * FPF Fuse and SW Ucode version has below binding
     * MSB of FPF Fuse will gives us the SW Ucode version.
     * FPF FUSE      SW Ucode Version
     *   0              0
     *   1              1
     *   3              2
     *   7              3
     * and so on. 
     */
    while (fpfFuse != 0)
    {
        count++;
        fpfFuse >>= 1;
    }

    *pFpfVersion = count;

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief  LS function to get the HW fpf version
 *
 * @params[out] pFpfVersion  Pointer to hw fpf fuse version. 
 * @returns     FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                           Appropriate error status on failure.
 */
FLCN_STATUS
dpuGetHWFpfVersion_GA10X
(
    LwU32* pFpfVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       count      = 0;
    LwU32       fpfFuse    = 0;

    if (!pFpfVersion)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    CHECK_FLCN_STATUS(EXT_REG_RD32_ERRCHK(LW_FUSE_OPT_FPF_UCODE_GSP_REV, &fpfFuse));
    fpfFuse = DRF_VAL(_FUSE, _OPT_FPF_UCODE_GSP_REV, _DATA, fpfFuse);

    /*
     * FPF Fuse and SW Ucode version has below binding
     * MSB of FPF Fuse will gives us the SW Ucode version.
     * FPF FUSE      SW Ucode Version
     *   0              0
     *   1              1
     *   3              2
     *   7              3
     * and so on. 
     */
    while (fpfFuse != 0)
    {
        count++;
        fpfFuse >>= 1;
    }

    *pFpfVersion = count;

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Ensure that HW fuse version matches SW ucode version
 *
 * @return FLCN_OK  if HW fuse version and SW ucode version matches
 *         FLCN_ERR_HS_CHK_UCODE_REVOKED  if mismatch
 */
FLCN_STATUS
dpuCheckFuseRevocationHS_GA10X(void)
{
  // TODO: Define a compile time variable to specify this via make arguments
    LwU32       fuseVersionHW = 0;
    LwU32       fpfVersionHW  = 0;
    LwU32       ucodeVersionSW = 0;
    FLCN_STATUS status        = FLCN_OK;

    // Read the version number from FPF fuse
    status = dpuGetHWFpfVersionHS_HAL(&Dpu.hal, &fpfVersionHW);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Read the version number from FUSE
    status = dpuGetHWFuseVersionHS_HAL(&Dpu.hal, &fuseVersionHW);
    if (status != FLCN_OK)
    {
        return status;
    }

    status = dpuGetUcodeSWVersionHS_HAL(&Dpu.hal, &ucodeVersionSW);
    if (status != FLCN_OK)
    {
        return status;
    }

    if ((ucodeVersionSW < fpfVersionHW) ||
        (ucodeVersionSW < fuseVersionHW))
    {
        return FLCN_ERR_HS_CHK_UCODE_REVOKED;
    }
    return FLCN_OK;
}

/*
 * Program IMB/IMB1/DMB/DMB1 register by getting the value from common scratch
 * group registers, programmed by acrlib during falcon bootstrap.
 */
FLCN_STATUS
dpuProgramImbDmbFromCommonScratch_GA10X(void)
{
    LwU32 val;
    FLCN_STATUS flcnStatus = FLCN_OK;

    CHECK_FLCN_STATUS(CSB_REG_RD32_ERRCHK(LW_CGSP_FALCON_IMB_BASE_COMMON_SCRATCH, &val));
    falc_wspr(IMB, val);

    CHECK_FLCN_STATUS(CSB_REG_RD32_ERRCHK(LW_CGSP_FALCON_IMB1_BASE_COMMON_SCRATCH, &val));
    falc_wspr(IMB1, val);

    CHECK_FLCN_STATUS(CSB_REG_RD32_ERRCHK(LW_CGSP_FALCON_DMB_BASE_COMMON_SCRATCH, &val));
    falc_wspr(DMB, val);

    CHECK_FLCN_STATUS(CSB_REG_RD32_ERRCHK(LW_CGSP_FALCON_DMB1_BASE_COMMON_SCRATCH, &val));
    falc_wspr(DMB1, val);

ErrorExit:
    return flcnStatus;
}

 /*!
 * @brief Check whether Bootstrap and FWSEC have run successfully to ensure Chain of Trust.
 * See https://confluence.lwpu.com/display/GFWGA10X/GA10x%3A+CoT+Enforcement+v1.0 for more information.
 *
 * @return FLCN_OK  If Chain of trust is maintain 
 *         FLCN_ERR_HS_CHK_GFW_CHAIN_OF_TRUST_BROKEN  If Chain of trust is broken
 */
FLCN_STATUS
dpuCheckChainOfTrustHS_GA10X(void)
{
    LwU32  val               = 0;
    LwU32  bootstrapCOTValue = 0;
    LwU32  fwsecCOTValue     = 0;
    FLCN_STATUS flcnStatus   = FLCN_OK;

    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_40_GFW_COT_BOOTSTRAP, &val));
    bootstrapCOTValue = DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_40_GFW_COT_BOOTSTRAP, _PROG, val);

    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_41_GFW_COT_FWSEC, &val));
    fwsecCOTValue     = DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_41_GFW_COT_FWSEC, _PROG, val); 

    if (!FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_40, _GFW_COT_BOOTSTRAP_PROG, _COMPLETED, bootstrapCOTValue) ||
        !FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_41, _GFW_COT_FWSEC_PROG,     _COMPLETED, fwsecCOTValue))
    {
       return FLCN_ERR_HS_CHK_GFW_CHAIN_OF_TRUST_BROKEN;
    }

ErrorExit:
    return flcnStatus;
}

 /*!
 * @brief  MST/ECF Security: Check and Set Source enable mask of LW_PDISP_SOR_DP_MST_MS_CTL_PRIV_LEVEL_MASK and LW_PDISP_SF_DP_MST_STREAM_CTL_PRIV_LEVEL_MASK
 *         to GSP and CPU. Associated registers will need access from CPU and GSP.
 *         Explicitly GSP access is controlled with Priv Level field (by disabling access to L0)
 *
 * @return FLCN_OK  If ECF Security Init is done
 */
FLCN_STATUS
dpuIsMstPlmsSelwrityInitDoneHS_GA10X(void)
{
    FLCN_STATUS flcnStatus   = FLCN_OK;
    LwU32       data32       = 0;
    LwU8        maxNoOfSors  = 0;
    LwU8        maxNoOfHeads = 0;
    LwU8        index        = 0;

    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_PDISP_SEC_SELWRE_WR_SCRATCH1_0, &data32));

    // Check if ECF security init is already done, else need to do it
    if(!FLD_TEST_DRF(_PDISP, _SEC_SELWRE_WR_SCRATCH1_0, _MST_PLMS_SELWRITY_INIT, _DONE, data32))
    {
        CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_PDISP_FE_MISC_CONFIGA, &data32));
        maxNoOfSors = DRF_VAL(_PDISP, _FE_MISC_CONFIGA, _NUM_SORS, data32);
        maxNoOfHeads = DRF_VAL(_PDISP, _FE_MISC_CONFIGA, _NUM_HEADS, data32);

        // Set Source enable to GSP and CPU for SOR_DP_MST_MS_CTRL register for all SORs
        for (index = 0; index < maxNoOfSors; index++)
        {
            CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_PDISP_SOR_DP_MST_MS_CTL_PRIV_LEVEL_MASK(index), &data32));
            data32 = FLD_SET_DRF(_PDISP, _SOR_DP_MST_MS_CTL_PRIV_LEVEL_MASK, _SOURCE_ENABLE, _GSP_CPU, data32);
            CHECK_FLCN_STATUS(EXT_REG_WR32_HS_ERRCHK(LW_PDISP_SOR_DP_MST_MS_CTL_PRIV_LEVEL_MASK(index), data32));
        }

        // Set Source enable to GSP and CPU for SF_DP_MST_STREAM_CTL register for all Heads
        for (index = 0; index < maxNoOfHeads; index++)
        {
            CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_PDISP_SF_DP_MST_STREAM_CTL_PRIV_LEVEL_MASK(index), &data32));
            data32 = FLD_SET_DRF(_PDISP, _SF_DP_MST_STREAM_CTL_PRIV_LEVEL_MASK, _SOURCE_ENABLE, _GSP_CPU, data32);
            CHECK_FLCN_STATUS(EXT_REG_WR32_HS_ERRCHK(LW_PDISP_SF_DP_MST_STREAM_CTL_PRIV_LEVEL_MASK(index), data32));
        }

        // Once ECF Security Init is done the set SEC_SELWRE_WR_SCRATCH1(0) to done
        CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_PDISP_SEC_SELWRE_WR_SCRATCH1_0, &data32));
        data32 = FLD_SET_DRF(_PDISP, _SEC_SELWRE_WR_SCRATCH1_0, _MST_PLMS_SELWRITY_INIT, _DONE, data32);
        CHECK_FLCN_STATUS(EXT_REG_WR32_HS_ERRCHK(LW_PDISP_SEC_SELWRE_WR_SCRATCH1_0, data32));
    }

ErrorExit:
    return flcnStatus;
}

//
// This function DMA's the HS and HS LIB Ovl signatures from gsp's Data subwpr into gsp's DMEM
// RM patches these signatures to gsp's subWpr at runtime. This Runtime patching of signatures can be done only for
// PKC HS Sig validation. Please refer to bug 200617410 for more info.
//
#ifdef RUNTIME_HS_OVL_SIG_PATCHING
FLCN_STATUS
dpuDmaHsOvlSigBlobFromWpr_GA10X(void)
{
    FLCN_STATUS      flcnStatus                     = FLCN_ERROR;  
    LwU32            hsOvlSigBlobArraySize          = ((SIG_SIZE_IN_BITS/8) * (LwU32)_num_imem_hs_overlays);
    LwU32            regAddr                        = (LW_PFB_PRI_MMU_FALCON_GSP_CFGB(GSP_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1));
    LwU32            data32                         = 0;
    LwU64            gspDataSubWprEndAddr4kAligned  = 0;
    LwU64            hsSigBlobAddr                  = 0;
    RM_FLCN_MEM_DESC memDesc;

    CHECK_FLCN_STATUS(EXT_REG_RD32_ERRCHK(regAddr, &data32));

    gspDataSubWprEndAddr4kAligned  = DRF_VAL(_PFB, _PRI_MMU_FALCON_GSP_CFGB, _ADDR_HI, data32);        

    hsSigBlobAddr                  = ((gspDataSubWprEndAddr4kAligned << SHIFT_4KB) - MAX_SUPPORTED_HS_OVL_SIG_BLOB_SIZE);    

    RM_FLCN_U64_PACK(&(memDesc.address), &hsSigBlobAddr);

    memDesc.params = 0;
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                     RM_GSP_DMAIDX_UCODE, memDesc.params);
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                                     hsOvlSigBlobArraySize, memDesc.params);

    CHECK_FLCN_STATUS(dmaReadUnrestricted((void*)_imem_hs_signatures, &memDesc,
                                          0, hsOvlSigBlobArraySize));

ErrorExit:
    return flcnStatus;
}
#endif // RUNTIME_HS_OVL_SIG_PATCHING

