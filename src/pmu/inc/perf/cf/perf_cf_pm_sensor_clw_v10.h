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
 * @file    perf_cf_pm_sensor_clw_v10.h
 * @brief   PMU PERF_CF PM CLW Sensor related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_PM_SENSOR_CLW_V10_H
#define PERF_CF_PM_SENSOR_CLW_V10_H

/* ------------------------ System Includes --------------------------------- */
#include "swref/hopper/gh100/dev_perf_addendum.h"
#include "hwref/hopper/gh100/dev_perf.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_pm_sensor.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Enabled GPCs in a GPU.
 */
#define PERF_CF_PM_SENSORS_CLW_V10_GPC_MASK                 0xFF

/*!
 * Enabled TPCs in a GPC.
 */
#define PERF_CF_PM_SENSORS_CLW_V10_TPC_MASK                 0x1FF

/*!
 * Enabled FBPs in a GPU.
 */
#define PERF_CF_PM_SENSORS_CLW_V10_FBP_MASK                 0xFFF

/*!
 * Enabled LTSPs in a FBP.
 */
#define PERF_CF_PM_SENSORS_CLW_V10_FBP_LTSP_MASK            0xF

/*!
 * Enabled SYS sections in device OOBRC. From dev_perf.h:
 * #define LW_PERF_CLW_SYS0_PRI_INDEX_CLWESCHED     0
 * #define LW_PERF_CLW_SYS0_PRI_INDEX_CLWLWLINK     1
 * #define LW_PERF_CLW_SYS0_PRI_INDEX_CLWPCIE       2
 * #define LW_PERF_CLW_SYS0_PRI_INDEX_CLWSYS0       3
 * #define LW_PERF_CLW_SYS0_PRI_INDEX_CLWSYS1       4
 */
#define PERF_CF_PM_SENSORS_CLW_V10_SYS_MASK                 0x7 /* SYS0/SYS1 are disabled in Hopper */

/*!
 * Each OOBRC row has 8 CLW counters.
 */
#define PERF_CF_PM_SENSORS_CLW_V10_COUNTERS_PER_OOBRC_ROW   8

/*!
 * Partition section rows in OOBRC.
 */
#define PERF_CF_PM_SENSORS_CLW_V10_OOBRC_PAR_SECTION_ROWS   128

/*!
 * Device section rows in OOBRC.
 */
#define PERF_CF_PM_SENSORS_CLW_V10_OOBRC_DEV_SECTION_ROWS   128

/*!
 * Device Sensor Object counter rows in OOBRC.
 */
#define PERF_CF_PM_SENSORS_CLW_V10_DEV_COUNTER_ROWS         80

/*!
 * GPC counter rows in OOBRC.
 */
#define PERF_CF_PM_SENSORS_CLW_V10_GPC_COUNTER_ROWS         2

/*!
 * Each Counter in OOBRC oclwpies 8 bytes.
 */
#define PERF_CF_PM_SENSORS_CLW_V10_BYTES_PER_COUNTER        8

/*!
 * Counters in each FBP LTSP.
 */
#define PERF_CF_PM_SENSORS_CLW_V10_FBP_COUNTERS             24 //3 rows

/*!
 * Rows in each FBP LTSP.
 */
#define PERF_CF_PM_SENSORS_CLW_V10_COUNTER_ROWS_PER_LTSP    3

/*!
 * For Hopper, part of device section (48 rows) are used to FBP counters.
 * There are 16 LTSP sections (3 rows of each) in device section
 */
#define PERF_CF_PM_SENSORS_CLW_V10_FBP_EXT_LTSP_NUM         16 //4 x 4 (FBP8/9/10/11)

/*!
 * @brief   The size of CLW scratch counter buffer
 */
#define PERF_CF_PM_SENSORS_CLW_V10_COUNTER_BUFFER_SIZE      24 //3 rows

/*!
 * The FBP EXT start index.
 */
#define PERF_CF_PM_SENSORS_CLW_V10_FBP_EXT_START_ID         8

/*!
 * MIG Sensor Object counter rows in OOBRC.
 */
#define PERF_CF_PM_SENSORS_CLW_V10_MIG_COUNTER_ROWS         16

/*!
 * All one 32bit mask used to configure CLW.
 */
#define PERF_CF_PM_SENSORS_CLW_V10_ALL_BITS_MASK            0xFFFFFFFFul

/*!
 * CLW_ENGINE_ID_PMA used to trigger PMA record.
 */
#define PERF_CF_PM_SENSORS_CLW_V10_ENGINE_ID_PMA            63

/*! @{
 * Here is Pranav explanation-- We had decided that GPC get 2 credits each (2x8 = 16 credits),
 * FBP get 4 credits each (4x12 = 48), and the rest to SYS (32 IIRC). 
 * Total credits need to be less than 96 across an entire chip
 */
#define PERF_CF_PM_SENSORS_CLW_V10_CREDITS_SYS              32
#define PERF_CF_PM_SENSORS_CLW_V10_CREDITS_PER_GPC          2
#define PERF_CF_PM_SENSORS_CLW_V10_CREDITS_PER_FBP          4
/*! @}*/

/*!
 * @brief   Timeout for HW to snap the CLW signals to OOBRC.
 */
#define PM_SENSOR_CLW_SW_TRIGGER_SNAP_TIMEOUT_US                            (40U)

/*!
 * @brief   Timeout for HW to have data ready in OOBRC read register.
 */
#define PM_SENSOR_CLW_OOBRC_DATA_READY_TIMEOUT_US       5

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Structure storing the configuration of a single CLW.
 */
typedef struct {
    /*!
     * Boolean to enable partition streaming.
     */
    LwBool  bPartitionStreamEn;

    /*!
     * Boolean to enable device streaming.
     */
    LwBool  bDeviceStreamEn;

    /*!
     * partition level start row in OOBRC where the CLW counters are aggregated to.
     */
    LwU8    partitionStartRowIdx;

    /*!
     * device level start row in OOBRC where the CLW counters are aggregated to.
     */
    LwU8    deviceStartRowIdx;
} PERF_CF_PM_SENSORS_CLW_BASE_CONFIG;

/*!
 * Structure storing the configuration of CLWs of a GPC.
 */
typedef struct {
    /*!
     * TPCs enabled masks in a GPC.
     */
    LwU16 tpcMask;

    /*!
     * TPC CLW configuration array from bits set in tpcMask.
     */
    PERF_CF_PM_SENSORS_CLW_BASE_CONFIG tpc[LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_TPC_NUM];
} PERF_CF_PM_SENSORS_CLW_GPC_CONFIG;

/*!
 * Structure storing the configuration of CLWs of all GPCs.
 */
typedef struct {
    /*!
     * GPCs enabled masks in a GPU.
     */
    LwU8                              gpcMask;

    /*!
     * GPC configuration array from bits set in gpccMask.
     */
    PERF_CF_PM_SENSORS_CLW_GPC_CONFIG gpc[LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_GPC_NUM];
} PERF_CF_PM_SENSORS_CLW_GPCS_CONFIG;

/*!
 * Structure storing the configuration of CLWs of a FBP.
 */
typedef struct {
    /*!
     * LTSPs enabled masks in a FBP.
     */
    LwU8 ltspMask;

    /*!
     * LTSP CLW configuration array from bits set in ltspMask.
     */
    PERF_CF_PM_SENSORS_CLW_BASE_CONFIG ltsp[LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_FBP_LTSP_NUM];
} PERF_CF_PM_SENSORS_CLW_FBP_CONFIG;

/*!
 * Structure storing the configuration of CLWs of all FBPs.
 */
typedef struct {
    /*!
     * FBPs enabled masks in a GPU.
     */
    LwU16                             fbpMask;

    /*!
     * FBP configuration array from bits set in fpbMask.
     */
    PERF_CF_PM_SENSORS_CLW_FBP_CONFIG fbp[LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_FBP_NUM];
} PERF_CF_PM_SENSORS_CLW_FBPS_CONFIG;

/*!
 * Structure storing the configuration of CLWs of all SYSs.
 */
typedef struct {
    /*!
     * SYS enabled masks in a GPU.
     */
    LwU8                               sysMask;

    /*!
     * SYS CLW configuration array from bits set in sysMask.
     */
    PERF_CF_PM_SENSORS_CLW_BASE_CONFIG sys[LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_SYS_NUM];
} PERF_CF_PM_SENSORS_CLW_SYS_CONFIG;

/*!
 * Structure storing the masks of TPC/FBP/SYS that need to SNAP.
 */
typedef struct {
    /*!
     * The overall masks of TPC/FBP/SYS that need to SNAP.
     */
    LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_CFG snapMask;

    /*!
     * Will be set if any of previous mask is set.
     */
    LwBool  bMaskSet;
} PERF_CF_PM_SENSORS_CLW_SNAP_MASK;

/* ------------------------ Global Variables -------------------------------- */
/*!
 * strlwture variables storing the CLW configuration of GPCs.
 */
extern PERF_CF_PM_SENSORS_CLW_GPCS_CONFIG pmSensorClwGpcsInitConfig;

/*!
 * strlwture variables storing the CLW configuration of FBPs.
 */
extern PERF_CF_PM_SENSORS_CLW_FBPS_CONFIG pmSensorClwFbpsInitConfig;

/*!
 * strlwture variables storing the CLW configuration of SYS.
 */
extern PERF_CF_PM_SENSORS_CLW_SYS_CONFIG  pmSensorClwSysInitConfig;

/*!
 * strlwture variables storing the CLW Masks that need to SNAP and Update.
 */
extern PERF_CF_PM_SENSORS_CLW_SNAP_MASK   pmSensorClwSnapMask;

/*!
 * Scratch buffer to store the reading counters temporary.
 */
extern LwU64 pmSensorClwCounterBuffer[PERF_CF_PM_SENSORS_CLW_V10_COUNTER_BUFFER_SIZE];


/* ------------------------ Public Functions -------------------------------- */
void perfCfPmSensorsClwV10Load(void)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfPmSensorsClwV10Load");

FLCN_STATUS perfCfPmSensorsClwV10OobrcRead(LwU64 *pBuffer, LwU32 bufferSize, LwU32 startRow, LwU32 nRows, BOARDOBJGRPMASK *pMask, LwU32 maskOffset, LwBool bDevPar)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorsClwV10OobrcRead");

void perfCfPmSensorClwV10SnapMaskClear(void)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorClwV10SnapMaskClear");

FLCN_STATUS perfCfPmSensorClwV10WaitSnapComplete(void)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorClwV10WaitSnapComplete");

/* ------------------------ Inline Functions  ------------------------------- */
/*!
 * @brief Inline helper function to check if any CLW need to snap.
 *
 * @return LW_REUE
 *     There are CLWs need to snap
 * @return LW_FALSE
 *     No CLWs need to snap.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBool
perfCfPmSensorClwV10SnapMaskIsSet(void)
{
    return pmSensorClwSnapMask.bMaskSet;
}

/*!
 * @brief Inline helper function to set snap mask.
 *
 * @param[in]  pSnapMask    Pointer to snap mask
 *
 * @return FLCN_OK
 *     Snap mask is set successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Incorrect pointer parameters (i.e. NULL) provided.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfPmSensorClwV10SnapMaskSet
(
    LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_CFG *pSnapMask
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8  gpcIdx;
    LwU16 fbpIdx;

    PMU_ASSERT_TRUE_OR_GOTO(status, (pSnapMask != NULL), FLCN_ERR_ILWALID_ARGUMENT,
                        perfCfPmSensorClwV10SnapMaskSet_exit);

    /* Setup GPC snap mask */
    pmSensorClwSnapMask.snapMask.gpcMask |= (pSnapMask->gpcMask & pmSensorClwGpcsInitConfig.gpcMask);
    FOR_EACH_INDEX_IN_MASK(8, gpcIdx, pSnapMask->gpcMask)
    {
        pmSensorClwSnapMask.snapMask.gpcTpcMask[gpcIdx] |= (pSnapMask->gpcTpcMask[gpcIdx] & pmSensorClwGpcsInitConfig.gpc[gpcIdx].tpcMask);
        pmSensorClwSnapMask.bMaskSet = LW_TRUE;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    /* Setup FBP snap mask */
    pmSensorClwSnapMask.snapMask.fbpMask |= (pSnapMask->fbpMask & pmSensorClwFbpsInitConfig.fbpMask);
    FOR_EACH_INDEX_IN_MASK(16, fbpIdx, pSnapMask->fbpMask)
    {
        pmSensorClwSnapMask.snapMask.fbpLtspMask[fbpIdx] |= (pSnapMask->fbpLtspMask[fbpIdx] & pmSensorClwFbpsInitConfig.fbp[fbpIdx].ltspMask);
        pmSensorClwSnapMask.bMaskSet = LW_TRUE;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    /* Setup SYS snap mask */
    pmSensorClwSnapMask.snapMask.sysMask |= (pSnapMask->sysMask & pmSensorClwSysInitConfig.sysMask);
    if (pSnapMask->sysMask)
    {
        pmSensorClwSnapMask.bMaskSet = LW_TRUE;
    }

perfCfPmSensorClwV10SnapMaskSet_exit:
    return status;
}

/*!
 * @brief Inline helper function to snap CLW in GPC.
 *
 * @param[in]  gpcIdx    The GPC index where CLW to snap
 * @param[in]  tpcMask   The TPC mask under GPC where CLW to snap
 *
 * @return FLCN_OK
 *     SYS snap mask is set successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Incorrect module index or mask provided.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfPmSensorsClwV10FlushGpc
(
    LwU8 gpcIdx,
    LwU16 tpcMask
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 gpcOffset;
    LwU16 tpcIdx;

    PMU_ASSERT_TRUE_OR_GOTO(status, 
        ((gpcIdx < LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_GPC_NUM) &&
          (tpcMask < LWBIT(LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_TPC_NUM))),
        FLCN_ERR_ILWALID_ARGUMENT, perfCfPmSensorsClwV10FlushGpc_exit);

    FOR_EACH_INDEX_IN_MASK(16, tpcIdx, tpcMask)
    {
        gpcOffset = LW_PERF_CONTROL_GPC(gpcIdx, tpcIdx);
        REG_WR32(BAR0, gpcOffset, DRF_DEF(_PERF_CLW_SYS0, _CONTROL, _FLUSH_RECORDS, _DOIT));
    }
    FOR_EACH_INDEX_IN_MASK_END;

perfCfPmSensorsClwV10FlushGpc_exit:
    return status;
}

/*!
 * @brief Inline helper function to snap CLW in FBP.
 *
 * @param[in]  fbpIdx    The FPB index where CLW to snap
 * @param[in]  ltspMask  The LTSP mask under GPC where CLW to snap
 *
 * @return FLCN_OK
 *     SYS snap mask is set successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Incorrect module index or mask provided.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfPmSensorsClwV10FlushFbp
(
    LwU8 fbpIdx,
    LwU8 ltspMask
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 fbpOffset;
    LwU8 ltspIdx;

    PMU_ASSERT_TRUE_OR_GOTO(status, 
        ((fbpIdx < LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_FBP_NUM) &&
         (ltspMask < LWBIT(LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_FBP_LTSP_NUM))),
        FLCN_ERR_ILWALID_ARGUMENT, perfCfPmSensorsClwV10FlushFbp_exit);

    FOR_EACH_INDEX_IN_MASK(8, ltspIdx, ltspMask)
    {
        fbpOffset = LW_PERF_CONTROL_FBP(fbpIdx, ltspIdx);
        REG_WR32(BAR0, fbpOffset, DRF_DEF(_PERF_CLW_SYS0, _CONTROL, _FLUSH_RECORDS, _DOIT));
    }
    FOR_EACH_INDEX_IN_MASK_END;

perfCfPmSensorsClwV10FlushFbp_exit:
    return status;
}

/*!
 * @brief Inline helper function to snap CLW in SYS.
 *
 * @param[in]  sysIdx    The SYS index where CLW to snap
 *
 * @return FLCN_OK
 *     SYS snap mask is set successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Incorrect module index provided.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfPmSensorsClwV10FlushSys
(
    LwU8 sysIdx
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 sysOffset;

    PMU_ASSERT_TRUE_OR_GOTO(status, 
        (sysIdx < LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_SYS_NUM),
        FLCN_ERR_ILWALID_ARGUMENT, perfCfPmSensorsClwV10FlushSys_exit);
    
    sysOffset = LW_PERF_CONTROL_SYS(sysIdx);
    REG_WR32(BAR0, sysOffset, DRF_DEF(_PERF_CLW_SYS0, _CONTROL, _FLUSH_RECORDS, _DOIT));

perfCfPmSensorsClwV10FlushSys_exit:
    return status;
}

/*!
 * @brief Inline helper function to snap CLWs.
 *
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfCfPmSensorClwV10Snap(void)
{
    LwU8  gpcIdx;
    LwU16 fbpIdx;
    LwU8  sysIdx;

    /* Flush GPCs */
    FOR_EACH_INDEX_IN_MASK(8, gpcIdx, pmSensorClwSnapMask.snapMask.gpcMask)
    {
        perfCfPmSensorsClwV10FlushGpc(gpcIdx, pmSensorClwSnapMask.snapMask.gpcTpcMask[gpcIdx]);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    /* Flush FBPs */
    FOR_EACH_INDEX_IN_MASK(16, fbpIdx, pmSensorClwSnapMask.snapMask.fbpMask)
    {
        perfCfPmSensorsClwV10FlushFbp(fbpIdx, pmSensorClwSnapMask.snapMask.fbpLtspMask[fbpIdx]);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    /* Flush SYS */
    FOR_EACH_INDEX_IN_MASK(8, sysIdx, pmSensorClwSnapMask.snapMask.sysMask)
    {
        perfCfPmSensorsClwV10FlushSys(sysIdx);
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

#endif // PERF_CF_PM_SENSOR_CLW_V10_H
