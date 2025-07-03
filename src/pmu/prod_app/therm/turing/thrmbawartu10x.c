/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    thrmbawartu10x.c
 * @brief   Thermal PMU BA WAR HAL functions for TU10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_chiplet_pwr_addendum.h"
#include "dev_chiplet_pwr.h"
#include "dev_fuse.h"
#include "dev_top.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "pmu_objpmgr.h"
#include "pmu_objpg.h"

#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static LwU8 s_thermBaApplyScalingFactor_TU10X(LwU8 weight, LwU8 max, LwUFXP20_12 fbpScalingFactor)
    GCC_ATTRIB_SECTION("imem_init", "s_thermBaApplyScalingFactor_TU10X");
static LwU8 s_thermGetSetBits_TU10X(LwU32 val)
    GCC_ATTRIB_SECTION("imem_init", "s_thermGetSetBits_TU10X");

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief SW WAR for HW Bug 200433679
 *
 * This require because of HW bug http://lwbugs/200433679. Details of issue below:
 * GPC's and FBP's present on the chip are grouped into either "north" or "south".
 * These values are transmitted to PWR via a MUX that toggles between "north" and 
 * "south" every clock cycle. The mechanism that transmits data from the mux to 
 * PWR operates on a credit system--data is transmitted only when it has a credit. 
 * Credits are given once every 2 cycles. During normal operation, the credits are 
 * quickly consumed, causing only every other piece of data (i.e. north or south) 
 * to be transmitted, causing the BA values to be half of what they should be.
 *
 * This WAR disables the BAs in the GPCs and FBPs on the side (i.e. north/south)
 * with the fewest number of enabled (i.e. unfloorswept) GPCs. The BA data from
 * the GPCs and FBPs are scaled to compensate. GPC BA data is scaled by means of
 * applying scaling factor A, and FBP BA data is scaled by applying an FBP
 * scaling factor to the FBP BA weights. The FBP scaling factor divides out
 * the factor A compensation factor so that the FBP BA data is scaled only by
 * the FBP scaling factor.
 *
 * @return FLCN_OK if the WAR was successfully applied, or is unecessary
 */
FLCN_STATUS 
thermBaControlOverrideWarBug200433679_TU10X(void)
{
    LwU8        chipletIdx;
    LwU8        regIdx = 0;
    LwU8        numEnabledNorthGPCs;
    LwU8        numEnabledSouthGPCs;
    LwU16       weight;
    LwU16       numDisabledTpcs = 0;
    LwU16       numTpcs = 0;
    LwU32       numFbps;
    LwU32       numGpcs;
    LwU32       floorsweptGpcMask;
    LwU32       floorsweptFbpMask;
    LwU32       gpcBaDisableMask;
    LwU32       fbpBaDisableMask;
    LwU32       reg;
    LwU32       tpcsInGpc;
    LwU32       northGpcMask;
    LwU32       southGpcMask;
    LwU32       northFbpMask;
    LwU32       southFbpMask;
    LwU32       maxTpcsPerGpc;
    LwU32       fbpIdleThreshold;
    LwU32       enabledGpcMask;
    LwUFXP20_12 fbpScalingFactor;

    numGpcs       = DRF_VAL(_PTOP, _SCAL_NUM_GPCS, _VALUE,
                            REG_RD32(BAR0, LW_PTOP_SCAL_NUM_GPCS));
    maxTpcsPerGpc = DRF_VAL(_PTOP, _SCAL_NUM_TPC_PER_GPC, _VALUE,
                            REG_RD32(BAR0, LW_PTOP_SCAL_NUM_TPC_PER_GPC));
    numFbps       = DRF_VAL(_PTOP, _SCAL_NUM_FBPS, _VALUE,
                            REG_RD32(BAR0, LW_PTOP_SCAL_NUM_FBPS));

    //
    // This WAR degrades BA based peakpower estimation accuracy--it disables
    // some of the GPC/FBP BAs and scales the remaining GPC/FBP data accordingly.
    // Bug 200433679 does not reproduce on TU117--this is because TU117 is the
    // only Turing chip with few enough GPCs/FBPs enabled that the logic that
    // transmits data from GPCs/FBPs to PWR never runs out of credits. Set the
    // scaleACompensationFactor to 1 so that scaleA is unchanged when it is multipled
    // by it (as opposed to leaving it as 0).
    //
    if (IsGPU(_TU117))
    {
        Pmgr.baInfo.scaleACompensationFactor = LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1);
        return FLCN_OK;
    }

    //
    // Compute which set of masks to use. TU116 uses a different
    // set of masks, but only one PMU binary is used for all Turing
    //
    if (IsGPU(_TU116))
    {
        northGpcMask = LW_CPWR_THERM_GPC_POLARITY_NORTH_MASK_TU116;
        southGpcMask = LW_CPWR_THERM_GPC_POLARITY_SOUTH_MASK_TU116;
        northFbpMask = LW_CPWR_THERM_FBP_POLARITY_NORTH_MASK_TU116;
        southFbpMask = LW_CPWR_THERM_FBP_POLARITY_SOUTH_MASK_TU116;
    }
    else
    {
        northGpcMask = LW_CPWR_THERM_GPC_POLARITY_NORTH_MASK_TU10X;
        southGpcMask = LW_CPWR_THERM_GPC_POLARITY_SOUTH_MASK_TU10X;
        northFbpMask = LW_CPWR_THERM_FBP_POLARITY_NORTH_MASK_TU10X;
        southFbpMask = LW_CPWR_THERM_FBP_POLARITY_SOUTH_MASK_TU10X;
    }

    // Get the floorsweeping masks
    floorsweptGpcMask = REG_RD32(BAR0, LW_FUSE_STATUS_OPT_GPC);
    floorsweptFbpMask = REG_RD32(BAR0, LW_FUSE_STATUS_OPT_FBP);

    // Count # of GPCs enabled in north and south
    enabledGpcMask       = (BIT32(numGpcs) - 1) & (~floorsweptGpcMask);
    numEnabledNorthGPCs = s_thermGetSetBits_TU10X(enabledGpcMask & northGpcMask);
    numEnabledSouthGPCs = s_thermGetSetBits_TU10X(enabledGpcMask & southGpcMask);

    // Choose which side will be disabled. disable the side with fewer enabled GPCs
    if (numEnabledNorthGPCs > numEnabledSouthGPCs)
    {
        gpcBaDisableMask = southGpcMask;
        fbpBaDisableMask = southFbpMask;
    }
    else
    {
        gpcBaDisableMask = northGpcMask;
        fbpBaDisableMask = northFbpMask;
    }

    //
    // Disable the chosen side's GPCs and FBPs, accomodating for
    // physical->logical mapping of enabled GPCs/FBPs to their registers
    //
    regIdx = 0;
    for (chipletIdx = 0; chipletIdx < numGpcs; chipletIdx++)
    {
        //
        // GPC/FBP BA disablement logic. Disable a GPC's/FBP's BA by setting
        // it's idle threshold to the maximum value. We do not enable/disable
        // BAs using the BA_ENABLE signals because of a HW that causes
        // PES to get stuck in the busy state.
        //
        if (FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_GPC, _IDX, chipletIdx, _ENABLE, floorsweptGpcMask))
        {
            // get the number of TPCs that are enabled in this GPC
            reg = REG_RD32(BAR0, LW_FUSE_STATUS_OPT_TPC_GPC(chipletIdx)) & (BIT32(maxTpcsPerGpc) - 1);
            tpcsInGpc = maxTpcsPerGpc - s_thermGetSetBits_TU10X(reg);

            if (FLD_IDX_TEST_DRF(_CPWR_THERM, _GPC_POLARITY, _GPC_ID, chipletIdx, _TRUE, gpcBaDisableMask))
            {
                reg = REG_RD32(BAR0, LW_PCHIPLET_PWR_GPC_CONFIG_1(regIdx));
                reg = FLD_SET_DRF_NUM(_PCHIPLET, _PWR_GPCS_CONFIG_1, _BA_IDLE_THRESHOLD,
                                      DRF_MASK(LW_PCHIPLET_PWR_GPCS_CONFIG_1_BA_IDLE_THRESHOLD), reg);
                REG_WR32(BAR0, LW_PCHIPLET_PWR_GPC_CONFIG_1(regIdx), reg);

                numDisabledTpcs += tpcsInGpc;
            }

            numTpcs += tpcsInGpc;
            regIdx++;
        }
    }

    regIdx = 0;
    for (chipletIdx = 0; chipletIdx < numFbps; chipletIdx++)
    {
        if (FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_FBP, _IDX, chipletIdx, _ENABLE, floorsweptFbpMask))
        {
            if (FLD_IDX_TEST_DRF(_CPWR_THERM, _FBP_POLARITY, _FBP_ID, chipletIdx, _TRUE, fbpBaDisableMask))
            {
                reg = REG_RD32(BAR0, LW_PCHIPLET_PWR_FBP_CONFIG_1(regIdx));
                reg = FLD_SET_DRF_NUM(_PCHIPLET, _PWR_FBPS_CONFIG_1, _BA_IDLE_THRESHOLD_LWVDD,
                                      DRF_MASK(LW_PCHIPLET_PWR_FBPS_CONFIG_1_BA_IDLE_THRESHOLD_LWVDD), reg);
                REG_WR32(BAR0, LW_PCHIPLET_PWR_FBP_CONFIG_1(regIdx), reg);
            }

            regIdx++;
        }
    }

    //
    // Compute TPC scaling factor after handling divide by 0 or negative case
    //
    // Note: the integer portion of scaleACompensationFactor will overflow if the
    // ratio of total enabled TPCs (i.e. unfloorswept) / enabled TPCs with BA enabled
    // is greater than 2^20 = 1048576. For this to happen, there would need to be
    // more than 1048576 TPCs or 0 TPCs on the chip, which we explicitly handle.
    // More realistically, and for the sake of proving we do not have overflow
    // issues in the FBP factor callwlations, we assume that there will be at most
    // 255 (8-bits) TPCs in the chip. Therefore, the largest possible sane value
    // for the integer portion of scaleACompensationFactor would be 255/1 = 255.
    //
    if (numTpcs > numDisabledTpcs)
    {
        Pmgr.baInfo.scaleACompensationFactor = LW_TYPES_U32_TO_UFXP_X_Y(20, 12, numTpcs);
        Pmgr.baInfo.scaleACompensationFactor /= (numTpcs - numDisabledTpcs);
    }
    else
    {
        //
        // If we arrive here, that means there are no TPCs enabled (i.e. unfloorswept)
        // in any of the GPCs that we've kept BA enabled. This is a non-sensical case
        // because we do not expect any GPC to be enabled (i.e. unfloorswept) but still
        // have all of its TPCs floorswept. Handle this with a PMU_BREAKPOINT and
        // passing up an error code
        //
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_STATE;
    }


    //
    // Compute the FBP scaling factor
    // FBP scaling factor = total enabled FBPs / (enabled FBPs with BA enabled * GPC scaling factor)
    //
    // Note: Number of FBPs on a chip is assumed to be less than 8-bits (i.e. 255).
    // This is a very safe assumption for the foreseeable future.
    //

    // Get the number of enabled FBPs with BA enabled
    fbpScalingFactor = s_thermGetSetBits_TU10X((BIT32(numFbps) - 1) & (~(floorsweptFbpMask | fbpBaDisableMask)));

    //
    // Check if the number of FBPs with BA enabled is greater than zero. If there
    // is no FBP enabled, we skip all FBP scaling logic to avoid a divide-by-zero
    // error when callwlating the FBP scaling factor.
    //
    if (fbpScalingFactor > 0)
    {
        //
        // Enabled FBPs with BA enabled = total number of FBPs - enabled FBPs that we
        // disabled BA in. The multiply by GPC scaling factor. The result of this line
        // is the denominator: (enabled FBPs with BA enabled * GPC scaling factor)
        //
        // Note: the below callwlation will not overflow because we assume that there
        // would be at most 255 TPCs (8-bits) and 255 FBPs (8-bits), meaning after
        // multiplication, there would at most be 8 + 8 = 16 integer bits in the product,
        // which is lest than the 20 allocated for fbpScaling Factor
        //
        fbpScalingFactor = fbpScalingFactor * Pmgr.baInfo.scaleACompensationFactor;

        //
        // Divide the total number of FBPs by the denominator, computed and stored in
        // fbpScalingFactor in the previous line of code, to get the final FBP
        // scaling factor
        //
        // Note: the denominator will not overflow because fbpRegIdx holds the number
        // enabled FBPs in the chip at this point in the code, and we assume that there
        // be at most 255 FBPs in the chip. When we colwert to a FXP, we left shift
        // by 12, then left shift by 12 again for to compensate for loss of binary point
        // due to FXP division. in total, we would be shifting fbpRegIdx (an 8-bit value)
        // by 12 + 12 = 24 bits, meaning it would still not overflow.
        //
        fbpScalingFactor = (LW_TYPES_U32_TO_UFXP_X_Y(20, 12, regIdx) << DRF_BASE(LW_TYPES_FXP_INTEGER(20, 12)))
                           / fbpScalingFactor;


        //
        // Multiply all FBP weights by the FBP scaling factor. Steps below:
        // 1) read WEIGHT_0/1 register
        // 2) apply the scaling factor to all weights
        // 3) write the broadcast register for WEIGHT 0/1
        //
        reg    = REG_RD32(BAR0, LW_PCHIPLET_PWR_FBP0_WEIGHT_0);

        weight = s_thermBaApplyScalingFactor_TU10X(DRF_VAL(_PCHIPLET, _PWR_FBP0_WEIGHT_0, _CROP, reg),
                                                  DRF_MASK(LW_PCHIPLET_PWR_FBP0_WEIGHT_0_CROP),
                                                  fbpScalingFactor);
        reg    = FLD_SET_DRF_NUM(_PCHIPLET, _PWR_FBP0_WEIGHT_0, _CROP, weight, reg);

        weight = s_thermBaApplyScalingFactor_TU10X(DRF_VAL(_PCHIPLET, _PWR_FBP0_WEIGHT_0, _ZROP, reg),
                                                  DRF_MASK(LW_PCHIPLET_PWR_FBP0_WEIGHT_0_ZROP),
                                                  fbpScalingFactor);
        reg    = FLD_SET_DRF_NUM(_PCHIPLET, _PWR_FBP0_WEIGHT_0, _ZROP, weight, reg);

        weight = s_thermBaApplyScalingFactor_TU10X(DRF_VAL(_PCHIPLET, _PWR_FBP0_WEIGHT_0, _FBPA, reg),
                                                  DRF_MASK(LW_PCHIPLET_PWR_FBP0_WEIGHT_0_FBPA),
                                                  fbpScalingFactor);
        reg    = FLD_SET_DRF_NUM(_PCHIPLET, _PWR_FBP0_WEIGHT_0, _FBPA, weight, reg);

        REG_WR32(BAR0, LW_PCHIPLET_PWR_FBPS_WEIGHT_0, reg); // write to broadcast register


        reg    = REG_RD32(BAR0, LW_PCHIPLET_PWR_FBP0_WEIGHT_1);

        weight = s_thermBaApplyScalingFactor_TU10X(DRF_VAL(_PCHIPLET, _PWR_FBP0_WEIGHT_1, _LTC, reg),
                                                  DRF_MASK(LW_PCHIPLET_PWR_FBP0_WEIGHT_1_LTC),
                                                  fbpScalingFactor);
        reg    = FLD_SET_DRF_NUM(_PCHIPLET, _PWR_FBP0_WEIGHT_1, _LTC, weight, reg);

        REG_WR32(BAR0, LW_PCHIPLET_PWR_FBPS_WEIGHT_1, reg); // write to broadcast register

        //
        // Scale the FBP idle thresholds for the enabled FBPs.
        // FBP weights are applied to the BAs in the individual FBPs, not after
        // all FBP BA values have been aclwmulatoed in THERM. We must therefore
        // scale the idle threshold (min value that a BA counter must achieve
        // to be transmitted to THERM) to maintain functional equivalence before
        // as when the FBP scaling factor was not applied to the FBP BA values.
        // Failure to do so will cause FBP BA values to be transmitted excessively
        // across XBAR, causing MSCG residency, which considers XBAR traffic, to
        // drop to 0% as in Bug 2423377.
        //
        regIdx = 0;
        for (chipletIdx = 0; chipletIdx < numFbps; chipletIdx++)
        {
            if (FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_FBP, _IDX, chipletIdx,
                                 _ENABLE, floorsweptFbpMask))
            {
                if (FLD_IDX_TEST_DRF(_CPWR_THERM, _FBP_POLARITY, _FBP_ID, chipletIdx,
                    _FALSE, fbpBaDisableMask))
                {
                    reg = REG_RD32(BAR0, LW_PCHIPLET_PWR_FBP_CONFIG_1(regIdx));
                    fbpIdleThreshold = DRF_VAL(_PCHIPLET, _PWR_FBPS_CONFIG_1, _BA_IDLE_THRESHOLD_LWVDD, reg)
                                       * fbpScalingFactor;

                    //
                    // Increase the fbpIdleThreshold by 1% to account for rounding issues as
                    // was the case with Bug 200479837
                    // 
                    fbpIdleThreshold += fbpIdleThreshold / 100;

                    fbpIdleThreshold = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12, fbpIdleThreshold);
                    reg = FLD_SET_DRF_NUM(_PCHIPLET, _PWR_FBPS_CONFIG_1, _BA_IDLE_THRESHOLD_LWVDD,
                                          fbpIdleThreshold, reg);
                    REG_WR32(BAR0, LW_PCHIPLET_PWR_FBP_CONFIG_1(regIdx), reg);
                }
                regIdx++;
            }
        }
    }

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief   Apply the FBP scaling factor to the specified weight, capping the product
 *           to the passed in max value
 *
 * @param[in]   weight  weight to apply FBP scaling factor to
 * @param[in]   max     max value to cap the product of the FBP scaling factor
 *                      and weight to
 * @param[in]   fbpScalingFactor  FBP scaling factor to apply to weight
 *
 * @return  product of weight and fbpScalingFactor, capped to max
 */
 static LwU8
s_thermBaApplyScalingFactor_TU10X
(
    LwU8        weight,
    LwU8        max,
    LwUFXP20_12 fbpScalingFactor
)
{
    //
    // Note: there is no possibility of overflow in the following multiplication
    // because fbpScalingFactor = total number of enabled FBPs / enabled FBPs with BA enabled
    // We assume at most 255 (8-bits) FBPs on the chip, and the weight is 8-bits.
    // Therefore, the product can be at most 8 + 8 = 16 bits, which still fits
    // into the 20 bit integer portion of weightFxp
    //
    LwUFXP20_12 weightFxp = weight * fbpScalingFactor;

    //
    // It is ok to return weightFxp rounded, a 20-bit number, even though we
    // return an 8-bit number, because we cap to max, which is an 8-bit number
    //
    return LW_MIN(max, LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12, weightFxp));
}

/*!
 * @brief   Compute how many bits are set in the passed in 32-bit value
 *
 * @param[in]   val  32-bit number that we want to count the number of set bits for
 *
 * @return  Number of bits set in val
 */
static LwU8
s_thermGetSetBits_TU10X(LwU32 val)
{
    NUMSETBITS_32(val);
    return val;
}
