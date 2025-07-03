/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_top.h"
#include "dev_top_addendum.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_perf_private.h"
#include "perf/3x/vfe_var_single_sensed_fuse_20.h"
#include "dev_fuse.h"
#include "dev_gc6_island.h"
#include "pmu_objfuse.h"
#include "pmu_bar0.h"
#include "ctrl/ctrl2080/ctrl2080fuse.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/*!
 * @brief  Read the hw fuse value for the input fuse id
 *
 * @param[in]   fuseId     FUSE ID to to read
 * @param[out]  pFuseSize  Size of the read fuse (required for sign extension)
 * @param[out]  pFuseVal   Pointer to fuse value for the input fuse id
 *
 * @return FLCN_OK                    If the fuse has been read successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT  If the fuse id is not supported
 */
FLCN_STATUS
fuseStateGet_AD10X
(
    LwU8    fuseId,
    LwU8   *pFuseSize,
    LwU32  *pFuseVal
)
{
    LwU32       rawState = 0;
    FLCN_STATUS status   = FLCN_OK;
    LwU32       bitMask;
    LwU32       bitStart;

    switch (fuseId)
    {
        case LW2080_CTRL_FUSE_ID_STRAP_SPEEDO:
            rawState = fuseRead(LW_FUSE_OPT_SPEEDO0);
            bitStart = DRF_BASE(LW_FUSE_OPT_SPEEDO0_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_SPEEDO0_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_SPEEDO0_DATA);
            break;
        case LW2080_CTRL_FUSE_ID_STRAP_IDDQ:
            rawState = fuseRead(LW_FUSE_OPT_IDDQ);
            bitStart = DRF_BASE(LW_FUSE_OPT_IDDQ_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_IDDQ_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_IDDQ_DATA);
            break;
        case LW2080_CTRL_FUSE_ID_STRAP_IDDQ_VERSION:
            rawState = fuseRead(LW_FUSE_OPT_IDDQ_REV);
            bitStart = DRF_BASE(LW_FUSE_OPT_IDDQ_REV_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_IDDQ_REV_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_IDDQ_REV_DATA);
            break;
        case LW2080_CTRL_FUSE_ID_STRAP_SRAM_VMIN:
            rawState = fuseRead(LW_FUSE_OPT_SRAM_VMIN_BIN);
            bitStart = DRF_BASE(LW_FUSE_OPT_SRAM_VMIN_BIN_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_SRAM_VMIN_BIN_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_SRAM_VMIN_BIN_DATA);
            break;
        case LW2080_CTRL_FUSE_ID_STRAP_SRAM_VMIN_VERSION:
            rawState = fuseRead(LW_FUSE_OPT_SRAM_VMIN_BIN_REV);
            bitStart = DRF_BASE(LW_FUSE_OPT_SRAM_VMIN_BIN_REV_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_SRAM_VMIN_BIN_REV_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_SRAM_VMIN_BIN_REV_DATA);
            break;
        case LW2080_CTRL_FUSE_ID_STRAP_BOOT_VMIN_LWVDD:
            rawState = fuseRead(LW_PGC6_BSI_SELWRE_SCRATCH_BOOT_VOLTAGE(0));
            bitStart = DRF_BASE(LW_PGC6_BSI_SELWRE_SCRATCH_BOOT_VOLTAGE_VALUE);
            bitMask  = DRF_MASK(LW_PGC6_BSI_SELWRE_SCRATCH_BOOT_VOLTAGE_VALUE);
            *pFuseSize = DRF_SIZE(LW_PGC6_BSI_SELWRE_SCRATCH_BOOT_VOLTAGE_VALUE);
            break;
        case LW2080_CTRL_FUSE_ID_STRAP_BOOT_VMIN_MSVDD:
            rawState = fuseRead(LW_PGC6_BSI_SELWRE_SCRATCH_BOOT_VOLTAGE(1));
            bitStart = DRF_BASE(LW_PGC6_BSI_SELWRE_SCRATCH_BOOT_VOLTAGE_VALUE);
            bitMask  = DRF_MASK(LW_PGC6_BSI_SELWRE_SCRATCH_BOOT_VOLTAGE_VALUE);
            *pFuseSize = DRF_SIZE(LW_PGC6_BSI_SELWRE_SCRATCH_BOOT_VOLTAGE_VALUE);
            break;
        case LW2080_CTRL_FUSE_ID_ISENSE_VCM_OFFSET:
            rawState = fuseRead(LW_FUSE_OPT_ADC_CAL_SENSE);
            bitStart = DRF_BASE(LW_FUSE_OPT_ADC_CAL_SENSE_VCM_OFFSET_CALIBRATION_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_ADC_CAL_SENSE_VCM_OFFSET_CALIBRATION_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_ADC_CAL_SENSE_VCM_OFFSET_CALIBRATION_DATA);
            break;
        case LW2080_CTRL_FUSE_ID_ISENSE_DIFF_OFFSET:
            rawState = fuseRead(LW_FUSE_OPT_ADC_CAL_SENSE);
            bitStart = DRF_BASE(LW_FUSE_OPT_ADC_CAL_SENSE_DIFFERENTIAL_OFFSET_CALIBRATION_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_ADC_CAL_SENSE_DIFFERENTIAL_OFFSET_CALIBRATION_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_ADC_CAL_SENSE_DIFFERENTIAL_OFFSET_CALIBRATION_DATA);
            break;
        case LW2080_CTRL_FUSE_ID_ISENSE_CALIBRATION_VERSION:
            rawState = fuseRead(LW_FUSE_OPT_ADC_CAL_SENSE_FUSE_REV);
            bitStart = DRF_BASE(LW_FUSE_OPT_ADC_CAL_SENSE_FUSE_REV_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_ADC_CAL_SENSE_FUSE_REV_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_ADC_CAL_SENSE_FUSE_REV_DATA);
            break;
        case LW2080_CTRL_FUSE_ID_ISENSE_DIFF_GAIN:
            rawState = fuseRead(LW_FUSE_OPT_ADC_CAL_SENSE);
            bitStart = DRF_BASE(LW_FUSE_OPT_ADC_CAL_SENSE_DIFFERENTIAL_GAIN_CALIBRATION_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_ADC_CAL_SENSE_DIFFERENTIAL_GAIN_CALIBRATION_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_ADC_CAL_SENSE_DIFFERENTIAL_GAIN_CALIBRATION_DATA);
            break;
         case LW2080_CTRL_FUSE_ID_STRAP_SPEEDO_1:
            rawState = fuseRead(LW_FUSE_OPT_SPEEDO1);
            bitStart = DRF_BASE(LW_FUSE_OPT_SPEEDO1_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_SPEEDO1_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_SPEEDO1_DATA);
            break;
        case LW2080_CTRL_FUSE_ID_CPM_VERSION:
            rawState = fuseRead(LW_FUSE_OPT_CPM_REV);
            bitStart = DRF_BASE(LW_FUSE_OPT_CPM_REV_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_CPM_REV_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_CPM_REV_DATA);
            break;
        case LW2080_CTRL_FUSE_ID_CPM_0:
            rawState = fuseRead(LW_FUSE_OPT_CPM0);
            bitStart = DRF_BASE(LW_FUSE_OPT_CPM0_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_CPM0_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_CPM0_DATA);
            break;
        case LW2080_CTRL_FUSE_ID_CPM_1:
            rawState = fuseRead(LW_FUSE_OPT_CPM1);
            bitStart = DRF_BASE(LW_FUSE_OPT_CPM1_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_CPM1_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_CPM1_DATA);
            break;
        case LW2080_CTRL_FUSE_ID_CPM_2:
            rawState = fuseRead(LW_FUSE_OPT_CPM2);
            bitStart = DRF_BASE(LW_FUSE_OPT_CPM2_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_CPM2_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_CPM2_DATA);
            break;
        case LW2080_CTRL_FUSE_ID_ISENSE_VCM_COARSE_OFFSET:
            rawState = fuseRead(LW_FUSE_OPT_ADC_CAL_SENSE);
            bitStart = DRF_BASE(LW_FUSE_OPT_ADC_CAL_SENSE_VCM_COARSE_CALIBRATION_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_ADC_CAL_SENSE_VCM_COARSE_CALIBRATION_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_ADC_CAL_SENSE_VCM_COARSE_CALIBRATION_DATA);
            break;
        case  LW2080_CTRL_FUSE_ID_IDDQ_LWVDD:
            rawState = fuseRead(LW_FUSE_OPT_IDDQ);
            bitStart = DRF_BASE(LW_FUSE_OPT_IDDQ_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_IDDQ_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_IDDQ_DATA);
            break;
        case  LW2080_CTRL_FUSE_ID_IDDQ_MSVDD:
            rawState = fuseRead(LW_FUSE_OPT_IDDQ_CP);
            bitStart = DRF_BASE(LW_FUSE_OPT_IDDQ_CP_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_IDDQ_CP_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_IDDQ_CP_DATA);
            break;
        case LW2080_CTRL_FUSE_ID_STRAP_SPEEDO_2:
            rawState = fuseRead(LW_FUSE_OPT_SPEEDO2);
            bitStart = DRF_BASE(LW_FUSE_OPT_SPEEDO2_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_SPEEDO2_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_SPEEDO2_DATA);
            break;
        case LW2080_CTRL_FUSE_ID_ISENSE_DIFFERENTIAL_COARSE_GAIN:
            rawState = fuseRead(LW_FUSE_OPT_ADC_CAL_SENSE);
            bitStart = DRF_BASE(LW_FUSE_OPT_ADC_CAL_SENSE_DIFFERENTIAL_GAIN_COARSE_CALIBRATION_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_ADC_CAL_SENSE_DIFFERENTIAL_GAIN_COARSE_CALIBRATION_DATA);
            *pFuseSize = DRF_SIZE(LW_FUSE_OPT_ADC_CAL_SENSE_DIFFERENTIAL_GAIN_COARSE_CALIBRATION_DATA);
            break;
        default:
             PMU_ASSERT_OK_OR_GOTO(status,
                FLCN_ERR_ILWALID_ARGUMENT,
                fuseVfeVarFuseStateGet_AD10X_exit);
    }

    *pFuseVal = (rawState >> bitStart) & bitMask;

fuseVfeVarFuseStateGet_AD10X_exit:
    return status;
}

/*!
 * @brief  Retrieve the number of enabled MXBARs.
 *
 * @param[in, out]  pNumMxbar  Number of enabled MXBARs.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT  If @ref pNumMxbar is NULL.
 * @return  FLCN_OK                    If the number of MXBARs was successfully retrieved.
 */
FLCN_STATUS
fuseNumMxbarGet_AD10X
(
    LwU8 *pNumMxbar
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pNumMxbar != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        fuseNumMxbarGet_AD10X_exit);

    *pNumMxbar = LW_PTOP_SCAL_NUM_MXBARS;

fuseNumMxbarGet_AD10X_exit:
    return status;
}

/*!
 * @brief  Retrieve the number of enabled FBPAs.
 *
 * @param[in, out]  pNumFbpa  Number of enabled FBPAs.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT  If @ref pNumFbpa is NULL.
 * @return  FLCN_OK                    If the number of FBPAs was successfully retrieved.
 */
FLCN_STATUS
fuseNumFbpaGet_AD10X
(
    LwU8 *pNumFbpa
)
{
    LwU32       maxNumFbpa;
    LwU32       fbpaEnableMask;
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pNumFbpa != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        fuseNumFbpaGet_AD10X_exit);

    // Obtain the maximum number of FBPAs as per the design spec.
    maxNumFbpa = REG_FLD_RD_DRF(BAR0, _PTOP, _SCAL_NUM_FBPAS, _VALUE);

    //
    // Obtain the actual number of enabled FBPAs on the GPU. For GDDR, FBPAs
    // and FBIOs have a 1:1 mapping, hence, this register read also provides
    // the correct FBIO count. For HBM, FBPAs and FBIOs have a many:1 mapping,
    // hence, this register read may not provide the correct FBIO count even
    // though it will provide the correct FBPA count.
    //
    fbpaEnableMask = (LWBIT32(maxNumFbpa) - 1) &
        (~fuseRead(LW_FUSE_STATUS_OPT_FBIO));
    NUMSETBITS_32(fbpaEnableMask);
    *pNumFbpa      = fbpaEnableMask;

fuseNumFbpaGet_AD10X_exit:
    return status;
}
