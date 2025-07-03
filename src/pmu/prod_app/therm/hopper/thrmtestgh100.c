/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    thrmtestgh100.c
 * @brief   PMU HAL functions related to therm tests for GH100+
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_therm.h"
#include "dev_fuse.h"
#include "dbgprintf.h"

//#define SHOW_PRINTS // enable for debug info

#ifdef SHOW_PRINTS
#define testPrintf(f, ...)   dbgPrintf((f), ##__VA_ARGS__)
#else
#define testPrintf(f, ...)   (void)0
#endif

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "pmu_objpmu.h"
#include "pmu_objfuse.h"
#include "config/g_therm_private.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define LW_GPC_MAX          (LW_THERM_GPC_TSENSE_INDEX_GPC_INDEX_MAX + 1)
#define LW_BJT_MAX          (LW_THERM_GPC_TSENSE_INDEX_GPC_BJT_INDEX_MAX + 1)
#define LW_LWL_MAX          (LW_THERM_LWL_TSENSE_INDEX_INDEX_MAX + 1)
// For Each Arch We get these values from HW TEAM(TSENSE designer, Jason Lee)
#define LW_BAND_GAP_VOLT            0x40
#define LW_VTPT_BIAS_LWRR           0x3
#define BCAST_FAKE_RAWCODE          0x500
#define TSENSE_ADC_OUT_RAWCODE      0x60010500


/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
static LwU32 s_thermTestGpcEnMaskGet_GH100(void);
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief  Programs the GPC index in the GPC_INDEX register.
 *         Read and write Increment will be disabled.
 *
 *         It should be re-triggered when any of following configuration
 *         registers are changed:
 *            - GPC_TSENSE_CONFIG_COPY
 *            - GPC_TSENSE
 *            - GPC_TS_ADJ
 *            - GPC_RAWCODE_OVERRIDE
 *            - GPC_TS_AUTO_CONTROL
 *
 * @param[in]   idxGpc      GPC index to select.
 */
void
thermTestGpcTsenseConfigCopyIndexSet_GH100
(
    LwU8    idxGpc
)
{
    LwU32 reg32 = 0;

    // Set GPC Index for GPC_INDEX register.
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_INDEX, _INDEX,
                                idxGpc, reg32);

    // Disable Read Increment.
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_INDEX, _READINCR, _DISABLED, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_INDEX, _WRITEINCR, _DISABLED, reg32);

    REG_WR32(CSB, LW_CPWR_THERM_GPC_INDEX, reg32);
}

/*!
 * @brief  Get mask of BJTs that are enabled for GPC. 1: enabled, 0: disabled.
 *
 * @param[in]   idxGpc  GPC TSENSE index.
 *
 * @return      Enabled BJT-s mask for the GPC Index Requested.
 */
LwU32
thermTestBjtEnMaskForGpcGet_GH100
(
    LwU8 idxGpc
)
{
    LwU64 bjtMask;

    //
    // BJT Mask Fuse value interpretation -
    // b0: GPC0BJT0, b1: GPC0BJT1, ..., b6: GPC0BJT6;
    // b7: GPC1BJT0, b8: GPC1BJT1, ..., b13: GPC1BJT2; similarly it continues.
    //
    bjtMask = (DRF_VAL(_FUSE_OPT, _GPC_TS_DIS_MASK_0, _DATA, fuseRead(LW_FUSE_OPT_GPC_TS_DIS_MASK_0)) |
                    (((LwU64)DRF_VAL(_FUSE_OPT, _GPC_TS_DIS_MASK_1, _DATA, fuseRead(LW_FUSE_OPT_GPC_TS_DIS_MASK_1))) << 32));

    return ((idxGpc > LW_GPC_MAX) ? 0 : (~(bjtMask >> (idxGpc * LW_BJT_MAX)) & ((1 << LW_BJT_MAX) - 1)));
}

/*!
 * @brief  Test the Feature of BCAST access to BJTs, introduced with GH100.
 *
 * @return      Falcon Status.
 */
FLCN_STATUS
thermTestGpcBcastAccess_GH100(RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE* pParams)
{
    // First Check the TSENSE Config Broadcast Acees across Enabled GPCs (Broadcast Enabled)
    LwU32 gpcEnMask = 0;
    LwU8 idxGpc = 0;
    LwU8 idxBjt = 0;
    LwU32 reg32 = 0;
    LwU32 tempReg32 = 0;
    LwU32 bjtEnMask = 0;
    LwU32 lastValSet = 0;
    LwU32 gpcNum = 0;
    LwU32 startVal = 0;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
    pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(BCAST_ACCESS, SUCCESS);

    startVal = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TS_ADJ, _BANDGAP_VOLT, LW_BAND_GAP_VOLT, startVal);
    startVal = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TS_ADJ, _VPTAT_BIAS_LWRRENT, LW_VTPT_BIAS_LWRR, startVal);
    startVal = FLD_SET_DRF(_CPWR_THERM, _GPC_TS_ADJ, _BANDGAP_BYPASS, _YES, startVal);
    gpcEnMask = s_thermTestGpcEnMaskGet_GH100();

    /* 
     * Enable Broadcast + Set Read INCR Enabled
     * Set Read Increment + Broadcast Set
     * Set GPC Index for GPC_INDEX register.
     */
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_GPC_INDEX);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_INDEX, _BROADCAST, _ENABLED, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_INDEX, _READINCR, _ENABLED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_INDEX, reg32);

    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        // Set Band-Gap Temp Coefficient for each GPC.
        tempReg32 = REG_RD32(CSB, LW_CPWR_THERM_GPC_INDEX);
        gpcNum = DRF_VAL(_CPWR_THERM, _GPC_INDEX, _INDEX, tempReg32);
        if (gpcNum != idxGpc)
        {
            return FLCN_ERROR;
        }

        // Based on current index set the value of Bandgap Voltage Temperature Coefficient
        tempReg32 = startVal;                             // LW_CPWR_THERM_GPC_TS_ADJ
        // Set Band Gap Coeeficient based on GPC ID
        tempReg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TS_ADJ, _BANDGAP_TEMP_COEF, idxGpc, tempReg32);
        REG_WR32(CSB, LW_THERM_GPC_TS_ADJ, tempReg32);
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_GPC_TS_ADJ);
        if (reg32 != tempReg32)
        {
            return FLCN_ERROR;
        }
        lastValSet = tempReg32;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Now Start Verification ...


    // The Last Config done i.e.GPC 7 should be Reflected in each GPC
    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        tempReg32 = 0;
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_GPC_INDEX);
        tempReg32 = REG_RD32(CSB, LW_THERM_GPC_TS_ADJ);
        // Since Read is set to Auto INCR Enabled, we should be reading same _GPC_TS_ADJ register value.
        if (lastValSet != tempReg32)
        {
            testPrintf(LEVEL_ALWAYS, "[BCAST_TEST_FAIL]: GPC Index register is 0x%x ...\n", reg32);
            testPrintf(LEVEL_ALWAYS, "[BCAST_TEST_FAIL]: _THERM_GPC_TS_ADJ value at index %d  is 0x%x ...\n", idxGpc, tempReg32);
            return FLCN_ERROR;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;
    testPrintf(LEVEL_ALWAYS, "[Part 1] BCAST Write UNICAST Read ...PASS\n");

    /* ======================== Raw Code Override Test ======================= */
    /*
     * Now Check Raw Code Override broadcast
     * Enable Broadcast + Set Read INCR Enabled
     * Set Read Increment + Broadcast Set
     * Set GPC Index for GPC_INDEX register.
     */
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_GPC_INDEX);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_INDEX, _BROADCAST, _ENABLED, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_INDEX, _READINCR, _DISABLED, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_INDEX, _WRITEINCR, _DISABLED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_INDEX, reg32);

    /*
     * Write a fake rawcode.(0x500)
     * LW_THERM_GPC_RAWCODE_OVERRIDE_EN_YES
     * LW_THERM_GPC_RAWCODE_OVERRIDE_VALUE = 0x500
     * (0x10500)
     */
    reg32 = 0;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_RAWCODE_OVERRIDE, _EN, _YES, reg32);
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_RAWCODE_OVERRIDE, _VALUE, BCAST_FAKE_RAWCODE, reg32);
    REG_WR32(CSB, LW_THERM_GPC_RAWCODE_OVERRIDE, reg32);

    /*
     * Enable Stream (LW_THERM_GPC_TSENSE)
     * Set Calibration Source as register(not fuse)
     * LW_CPWR_THERM_GPC_TSENSE_CALI_SRC_REG 
     * LW_CPWR_THERM_GPC_TSENSE_TC_SRC_REG
     * Since we are going to to do UNICAST BJT reads
     * Set CONTROL_SRC=_LOGIC and STREAM_RAW_CODE=_DISABLED
     */
    reg32 = 0;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE, _STREAM_RAW_CODE, _DISABLED, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE, _TS_PD, _DEASSERT, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE, _CONTROL_SRC, _LOGIC, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE, _CALI_SRC, _REG, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE, _TC_SRC, _REG, reg32);
    REG_WR32(CSB, LW_THERM_GPC_TSENSE, reg32);

    //  CONFIG_COPY_SEND (_NEW_SETTING_SEND command)
    reg32 = 0;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE_CONFIG_COPY, _NEW_SETTING, _SEND, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_CONFIG_COPY, reg32);
    tempReg32 = REG_RD32(CSB, LW_THERM_GPC_TSENSE_CONFIG_COPY);
    OS_PTIMER_SPIN_WAIT_US(60000);

    // Read INCR Enabled
    reg32 = 0;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_INDEX, _READINCR, _ENABLED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_INDEX, reg32);

    // Read at TS_ADC_OUT
    tempReg32 = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSENSE);
    tempReg32 = DRF_VAL(_CPWR_THERM, _GPC_TSENSE, _TS_ADC_OUT, tempReg32);
    if (tempReg32 != BCAST_FAKE_RAWCODE)
    {
        testPrintf(LEVEL_ALWAYS, "GPC TSENSE = 0x%x\n", tempReg32);
        testPrintf(LEVEL_ALWAYS, "RAW CODE = 0x%x\n", BCAST_FAKE_RAWCODE);
        return FLCN_ERROR;
    }
    testPrintf(LEVEL_ALWAYS, "[Part 2] BCAST Override Unicast Read... PASS\n");
    /*
     * Now Set incremental Read TSENSE_HUB for each GPC-BJTs
     * LW_THERM_GPC_TSENSE_INDEX_READINCR_ENABLED
     * Select GPC Index as required First (Set bit 16:19)
     */ 
    tempReg32 = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSENSE_INDEX);
    tempReg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE_INDEX, _READINCR, _ENABLED, tempReg32);
    tempReg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSENSE_INDEX, _GPC_INDEX, 0, tempReg32);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_INDEX, tempReg32);
    testPrintf(LEVEL_ALWAYS, "[BCAST_TEST]: Written val LW_CPWR_THERM_GPC_TSENSE_INDEX  = 0x%x ...\n", REG_RD32(CSB, LW_CPWR_THERM_GPC_TSENSE_INDEX));

    // Get BJT-s enabled for the GPC index.
    bjtEnMask = thermTestBjtEnMaskForGpcGet_HAL(&Therm, idxGpc);
    testPrintf(LEVEL_ALWAYS, "bjtEnMask = 0x%x\n", bjtEnMask);
    OS_PTIMER_SPIN_WAIT_US(60000);
    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
    FOR_EACH_INDEX_IN_MASK(32, idxBjt, bjtEnMask)
    {
            tempReg32 = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSENSE_RAW_CODE);
            if (DRF_VAL(_THERM, _GPC_TSENSE_RAW_CODE, _VALUE, tempReg32) != BCAST_FAKE_RAWCODE)
            {
                testPrintf(LEVEL_ALWAYS, "Failure for GPC Index 0x%x and BJT Index 0x%x\n", idxGpc, idxBjt);
                testPrintf(LEVEL_ALWAYS, "_TSENSE_RAW_CODE = 0x%x\n", tempReg32);
                testPrintf(LEVEL_ALWAYS, "Expected Value = 0x%x\n", BCAST_FAKE_RAWCODE);
                return FLCN_ERROR;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    testPrintf(LEVEL_ALWAYS, "[Part 3] BCAST Write to UNICAST BJT Read ...PASS\n");

    // For Emulation non-zero GPC is not defined and it will fail
    if (IsEmulation())
    {
        // Select GPC Index as 1
        tempReg32 = REG_RD32(CSB, LW_THERM_GPC_TSENSE_INDEX);
        tempReg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE_INDEX, _READINCR, _ENABLED, tempReg32);
        tempReg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSENSE_INDEX, _GPC_INDEX, 1, tempReg32);
        REG_WR32(CSB, LW_THERM_GPC_TSENSE_INDEX, tempReg32);

        bjtEnMask = thermTestBjtEnMaskForGpcGet_HAL(&Therm, 1);
        FOR_EACH_INDEX_IN_MASK(32, idxBjt, bjtEnMask)
        {
            testPrintf(LEVEL_ALWAYS, "[BCAST_TEST]: Reading GPC Index %d and BJT Index %d\n", idxGpc, idxBjt);
            //  LW_THERM_GPC_TSENSE_RAW_CODE
            tempReg32 = REG_RD32(CSB, LW_THERM_GPC_TSENSE_RAW_CODE);
            if (DRF_VAL(_THERM, _GPC_TSENSE_RAW_CODE, _VALUE, tempReg32) != 0)
            {
                testPrintf(LEVEL_ALWAYS, "[0]_TSENSE_RAW_CODE = 0x%x\n", DRF_VAL(_THERM, _GPC_TSENSE_RAW_CODE, _VALUE, tempReg32));
                return FLCN_ERROR;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
        testPrintf(LEVEL_ALWAYS, "[Part 3.5] UNICAST Test in Emulation  ...PASS\n");
    }

    /*
     *  Negative Testing (Set UNICAST ==> should not get BCAST)
     * =====================================================================
     */

    /*
     * Set BCAST Disabled for a GPC
     * Set GPC Index for GPC_INDEX register.
     */
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_INDEX, _INDEX, idxGpc, reg32);

    /*
     * Disable Broadcast.
     * Disable Write Increment.
     * Disable Read Increment.
     */
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_INDEX, _READINCR, _ENABLED, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_INDEX, _WRITEINCR, _DISABLED, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_INDEX, _BROADCAST, _DISABLED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_INDEX, reg32);

    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        //First verify the GPC index before setting
        tempReg32 = REG_RD32(CSB, LW_CPWR_THERM_GPC_INDEX);
        int gpcNum = DRF_VAL(_CPWR_THERM, _GPC_INDEX, _INDEX, tempReg32);
        if (gpcNum != idxGpc)
        {
            testPrintf(LEVEL_ALWAYS, "Improper Index :gpcNum 0x%x idxGpc = 0x%x\n", gpcNum, idxGpc);
            return FLCN_ERROR;
        }

        // Based on current index set the value of Bandgap Voltage Temperature Coefficient
        tempReg32 = startVal;                         // LW_CPWR_THERM_GPC_TS_ADJ

        // Set Band Gap Coeeficient based on GPC ID
        tempReg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TS_ADJ, _BANDGAP_TEMP_COEF, idxGpc, tempReg32);
        REG_WR32(CSB, LW_THERM_GPC_TS_ADJ, tempReg32);
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_GPC_TS_ADJ);
        if (reg32 != tempReg32)
        {
            testPrintf(LEVEL_ALWAYS, "Write Fail :reg32 0x%x tempReg32 = 0x%x\n", reg32, tempReg32);
            return FLCN_ERROR;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    /*
     * Now Verify
     * Each GPC config should reflect its own value only
     * Since Read is set to Auto INCR Enabled reading same _GPC_TS_ADJ register
     */
    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        tempReg32 = REG_RD32(CSB, LW_CPWR_THERM_GPC_INDEX);
        int gpcNum = DRF_VAL(_CPWR_THERM, _GPC_INDEX, _INDEX, tempReg32);
        if (gpcNum != idxGpc)
        {
            testPrintf(LEVEL_ALWAYS, "Improper Index :gpcNum 0x%x idxGpc = 0x%x\n", gpcNum, idxGpc);
            return FLCN_ERROR;
        }

        tempReg32 = startVal;             // LW_CPWR_THERM_GPC_TS_ADJ

        // Check Band Gap Coeeficient based on GPC ID
        tempReg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TS_ADJ, _BANDGAP_TEMP_COEF, idxGpc, tempReg32);
        reg32 = REG_RD32(CSB, LW_THERM_GPC_TS_ADJ);
        if (reg32 != tempReg32)
        {
        testPrintf(LEVEL_ALWAYS, "[BCAST_TEST]: GPC Index register is 0x%x \n", reg32);
        testPrintf(LEVEL_ALWAYS, "[BCAST_TEST]: _THERM_GPC_TS_ADJ value at index %d  is 0x%x \n", idxGpc, tempReg32);
            testPrintf(LEVEL_ALWAYS, "Write Failure :reg32 0x%x tempReg32 = 0x%x\n", reg32, tempReg32);
            return FLCN_ERROR;
    }
    }
    FOR_EACH_INDEX_IN_MASK_END;
    testPrintf(LEVEL_ALWAYS, "[Part 4] UNICAST Write to UNICAST Read ...PASS\n");
    testPrintf(LEVEL_ALWAYS, "Broadcast Test Finished !!!\n");
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;
    return FLCN_OK;
}
/* ------------------------- Private Functions ------------------------------- */

/*!
 * @brief  Get mask of GPCs that are enabled. 1: enabled, 0: disabled.
 *
 * @return      Enabled GPC-s mask.
 */
static LwU32 
s_thermTestGpcEnMaskGet_GH100(void)
{
    LwU32 gpcMask;
    LwU32 maxGpcMask;

    //
    // GPC MASK Fuse value interpretation - for each bit: 1: disable 0: enable.
    // Colwert it into a normal bitmask with - 1: enabled, 0: disabled.
    //
    gpcMask = fuseRead(LW_FUSE_STATUS_OPT_GPC);
    maxGpcMask = ((1 << LW_GPC_MAX) - 1);

    gpcMask = (~gpcMask & maxGpcMask);

    return gpcMask;
}
