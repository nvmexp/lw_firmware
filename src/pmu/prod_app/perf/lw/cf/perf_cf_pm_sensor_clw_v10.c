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
 * @file    perf_cf_pm_sensor_clw_v10.c
 * @copydoc perf_cf_pm_sensor_clw_v10.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "pmu_bar0.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "hwref/hopper/gh100/dev_fuse.h"
#include "swref/hopper/gh100/dev_pri_ringstation_gpc_addendum.h"
#include "swref/hopper/gh100/dev_pri_ringstation_fbp_addendum.h"
#include "hwref/hopper/gh100/dev_pri_ringstation_control_gpc.h"
#include "hwref/hopper/gh100/dev_pri_ringstation_control_fbp.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_pm_sensor_clw_v10.h"
#include "pmu_objpmu.h"
#include "perf/cf/perf_cf.h"
#include "config/g_perf_cf_private.h"
#include "pmu_objfuse.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static void s_perfCfPmSensorsClwV10DisableHelper(LwU32 dataConfigOffset, LwU32 selwreConfigOffset)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10DisableHelper");
    
static void s_perfCfPmSensorsClwV10DisableGpc(LwU8 gpcIdx, LwU16 tpcMask)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10DisableGpc");
    
static void s_perfCfPmSensorsClwV10DisableFbp(LwU8 fbpIdx, LwU16 ltspMask)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10DisableFbp");
    
static void s_perfCfPmSensorsClwV10DisableSys(LwU8 sysIdx)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10DisableSys");
    
static void s_perfCfPmSensorsClwV10DisableAll(void)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10DisableAll");
    
static void s_perfCfPmSensorsClwV10ResetHelper(LwU32 selwreConfigOffset, LwU32 dataConfigOffset,
                                             LwU32 controlOffset, LwU32 counterInitOffset)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10ResetHelper"); 
    
static void s_perfCfPmSensorsClwV10ResetGpc(LwU8 gpcIdx, LwU16 tpcMask)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10ResetGpc");
    
static void s_perfCfPmSensorsClwV10ResetFbp(LwU8 fbpIdx, LwU16 ltspMask)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10ResetFbp");
    
static void s_perfCfPmSensorsClwV10ResetSys(LwU8 sysIdx)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10ResetSys");
    
static void s_perfCfPmSensorsClwV10ResetAll(void)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10ResetAll");
    
static void s_perfCfPmSensorsClwV10ConfigHelper(LwU32 dataConfigOffset, LwU32 cg2Offset, LwU32 selwreConfigOffset,
                                             LwBool overflowEn, PERF_CF_PM_SENSORS_CLW_BASE_CONFIG *pBaseCfg)          
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10ConfigHelper");
    
static void s_perfCfPmSensorsClwV10ConfigGpc(LwU8 gpcIdx, PERF_CF_PM_SENSORS_CLW_GPC_CONFIG *pGpcCfg)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10ConfigGpc");
    
static void s_perfCfPmSensorsClwV10ConfigFbp(LwU8 fbpIdx, PERF_CF_PM_SENSORS_CLW_FBP_CONFIG *pFbpCfg)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10ConfigFbp");
    
static void s_perfCfPmSensorsClwV10ConfigSys(LwU8 sysIdx, PERF_CF_PM_SENSORS_CLW_BASE_CONFIG  *pSysCfg)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10ConfigSys");
    
static void s_perfCfPmSensorsClwV10OobrcConfig(LwBool enable)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10OobrcConfig");
    
static void s_perfCfPmSensorsClwV10OobrcDisable(void)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10OobrcDisable");
    
static void s_perfCfPmSensorsClwV10OobrcEnable(void)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10OobrcEnable");
    
static void s_perfCfPmSensorsClwV10OobrcReset(void)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10OobrcReset");
    
static void s_perfCfPmSensorsClwV10RouterInit(void)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10RouterInit");
    
static void s_perfCfPmSensorsClwV10PmaTriggersInit(void)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10PmaTriggersInit");
    
static void s_perfCfPmSensorsClwV10Reset(void)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10Reset");

static LwBool s_perfCfPmSensorsClwV10OobrcDataReady(LwU32 *pRowIdx)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10OobrcDataReady");

static LwBool s_perfCfPmSensorClwV10SwTriggerSnapCleared(void *pArgs)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorClwV10SwTriggerSnapCleared");

static void s_perfCfPmSensorsClwV10Floorsweeping(void)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwV10Floorsweeping");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * strlwture variables storing the CLW configuration of GPCs.
 */
PERF_CF_PM_SENSORS_CLW_GPCS_CONFIG pmSensorClwGpcsInitConfig
    GCC_ATTRIB_SECTION("dmem_perfCfPmSensors", "pmSensorClwGpcsInitConfig") = 
{
    PERF_CF_PM_SENSORS_CLW_V10_GPC_MASK,
    {/* GPCs */
        {
            PERF_CF_PM_SENSORS_CLW_V10_TPC_MASK,
            {
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 0}
            },
        }, /* GPC0 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_TPC_MASK,
            {
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 16, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 16, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 16, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 16, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 16, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 16, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 16, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 16, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 16, .deviceStartRowIdx = 0}
            },
        }, /* GPC1 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_TPC_MASK,
            {
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 32, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 32, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 32, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 32, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 32, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 32, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 32, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 32, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 32, .deviceStartRowIdx = 0}
            },
        }, /* GPC2 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_TPC_MASK,
            {
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 48, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 48, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 48, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 48, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 48, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 48, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 48, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 48, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 48, .deviceStartRowIdx = 0}
            },
        }, /* GPC3 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_TPC_MASK,
            {
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 64, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 64, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 64, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 64, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 64, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 64, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 64, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 64, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 64, .deviceStartRowIdx = 0}
            },
        }, /* GPC4 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_TPC_MASK,
            {
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 80, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 80, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 80, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 80, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 80, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 80, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 80, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 80, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 80, .deviceStartRowIdx = 0}
            },
        }, /* GPC5 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_TPC_MASK,
            {
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 96, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 96, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 96, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 96, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 96, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 96, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 96, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 96, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 96, .deviceStartRowIdx = 0}
            },
        }, /* GPC6 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_TPC_MASK,
            {
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 112, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 112, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 112, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 112, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 112, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 112, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 112, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 112, .deviceStartRowIdx = 0},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 112, .deviceStartRowIdx = 0}
            },
        } /* GPC7 */
    }
};

/*!
 * strlwture variables storing the CLW configuration of FBPs.
 */
PERF_CF_PM_SENSORS_CLW_FBPS_CONFIG pmSensorClwFbpsInitConfig
    GCC_ATTRIB_SECTION("dmem_perfCfPmSensors", "pmSensorClwFbpsInitConfig") = 
{
    PERF_CF_PM_SENSORS_CLW_V10_FBP_MASK,
    {/* FBPs */
        {
            PERF_CF_PM_SENSORS_CLW_V10_FBP_LTSP_MASK,
            {
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 2,  .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 5,  .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 8,  .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 11, .deviceStartRowIdx = 2}
            },
        }, /* FBP0 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_FBP_LTSP_MASK,
            {
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 18, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 21, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 24, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 27, .deviceStartRowIdx = 2}
            },
        }, /* FBP1 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_FBP_LTSP_MASK,
            {
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 34, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 37, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 40, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 43, .deviceStartRowIdx = 2}
            },
        }, /* FBP2 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_FBP_LTSP_MASK,
            {
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 50, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 53, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 56, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 59, .deviceStartRowIdx = 2}
            },
        }, /* FBP3 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_FBP_LTSP_MASK,
            {
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 66, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 69, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 72, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 75, .deviceStartRowIdx = 2}
            },
        }, /* FBP4 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_FBP_LTSP_MASK,
            {
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 82, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 85, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 88, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 91, .deviceStartRowIdx = 2}
            },
        }, /* FBP5 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_FBP_LTSP_MASK,
            {
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 98,  .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 101, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 104, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 107, .deviceStartRowIdx = 2}
            },
        }, /* FBP6 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_FBP_LTSP_MASK,
            {
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 114, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 117, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 120, .deviceStartRowIdx = 2},
                {.bPartitionStreamEn = LW_TRUE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 123, .deviceStartRowIdx = 2}
            },
        }, /* FBP7 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_FBP_LTSP_MASK,
            {
                {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 80},
                {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 83},
                {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 86},
                {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 89}
            },
        }, /* FBP8 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_FBP_LTSP_MASK,
            {
                {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 92},
                {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 95},
                {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 98},
                {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 101}
            },
        }, /* FBP9 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_FBP_LTSP_MASK,
            {
                {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 104},
                {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 107},
                {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 110},
                {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 113}
            },
        }, /* FBP10 */
        {
            PERF_CF_PM_SENSORS_CLW_V10_FBP_LTSP_MASK,
            {
                {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 116},
                {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 119},
                {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 122},
                {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 125}
            }
        } /* FBP11 */
    }
};

/*!
 * strlwture variables storing the CLW configuration of SYS.
 */
PERF_CF_PM_SENSORS_CLW_SYS_CONFIG pmSensorClwSysInitConfig
    GCC_ATTRIB_SECTION("dmem_perfCfPmSensors", "pmSensorClwSysInitConfig") = 
{
    PERF_CF_PM_SENSORS_CLW_V10_SYS_MASK,
     {/* SYSs */
        {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE,  .partitionStartRowIdx = 0, .deviceStartRowIdx = 46}, /* CLWESCHED */
        {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE,  .partitionStartRowIdx = 0, .deviceStartRowIdx = 10}, /* CLWLWLINK */
        {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_TRUE,  .partitionStartRowIdx = 0, .deviceStartRowIdx = 6},  /* CLWPCIE */
        {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_FALSE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 0},  /* CLWSYS0 */
        {.bPartitionStreamEn = LW_FALSE, .bDeviceStreamEn = LW_FALSE, .partitionStartRowIdx = 0, .deviceStartRowIdx = 0}   /* CLWSYS1 */
    }
};

/*!
 * structure variables storing the CLW Masks that need to SNAP and Update.
 */
PERF_CF_PM_SENSORS_CLW_SNAP_MASK pmSensorClwSnapMask
    GCC_ATTRIB_SECTION("dmem_perfCfPmSensors", "pmSensorClwSnapMask");

/*!
 * Scratch buffer to store the reading counters temporary.
 */
LwU64 pmSensorClwCounterBuffer[PERF_CF_PM_SENSORS_CLW_V10_COUNTER_BUFFER_SIZE]
    GCC_ATTRIB_SECTION("dmem_perfCfPmSensors", "pmSensorClwCounterBuffer");


/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Helper function to reset CLW system (CLW/OOBRC/PMA/ROUTER) and
 * Configure the data path of all CLWs.
 */
void
perfCfPmSensorsClwV10Load(void)
{
    LwU8  gpcIdx;
    LwU16 fbpIdx;
    LwU8  sysIdx;

    s_perfCfPmSensorsClwV10Floorsweeping();

    s_perfCfPmSensorsClwV10Reset();

    /* Config GPCs */
    FOR_EACH_INDEX_IN_MASK(8, gpcIdx, pmSensorClwGpcsInitConfig.gpcMask)
    {
        s_perfCfPmSensorsClwV10ConfigGpc(gpcIdx, &pmSensorClwGpcsInitConfig.gpc[gpcIdx]);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    /* Config FBPs */
    FOR_EACH_INDEX_IN_MASK(16, fbpIdx, pmSensorClwFbpsInitConfig.fbpMask)
    {
        s_perfCfPmSensorsClwV10ConfigFbp(fbpIdx, &pmSensorClwFbpsInitConfig.fbp[fbpIdx]);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    /* Config SYS */
    FOR_EACH_INDEX_IN_MASK(8, sysIdx, pmSensorClwSysInitConfig.sysMask)
    {
        s_perfCfPmSensorsClwV10ConfigSys(sysIdx, &pmSensorClwSysInitConfig.sys[sysIdx]);
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

 /*!
  * @brief Helper function to reset rows of counters from OOBRC.
  *        Counters to be read either in Device section or Partiton section,
  *        "startRow+nRows" should be in section range (0~127). For Device sensor
  *        object, all coutners are in Device section. For MIG sensor object, the
  *        GPC counters are in Partition section, while the FBP counters could be
  *        in Device section (extension FBP counters)
  *
  * @param[in]  pBuffer      Pointer to buffer to store the read counters
  * @param[in]  bufferSize   The size of pBuffer
  * @param[in]  startRow     OOBRC start row # in Device section or Partition section to read
  * @param[in]  nRows        OOBRC rows to read
  * @param[in]  pMask        Pointer to sensor's signalsSupportedMask
  * @param[in]  maskOffset   The offset to the pMask for the first counter to read
  * @param[in]  bDevPar      Indicator counter location: 1-Device; 0-Partition.
  *
  * @return FLCN_OK
  *     Counters are read successfully
  * @return FLCN_ERR_ILWALID_ARGUMENT
  *     Incorrect pointer parameters (i.e. NULL) provided.
  * @return FLCN_ERR_OUT_OF_RANGE
  *     Read counter is out of the range of the section or the destination buffer.
  */
FLCN_STATUS
perfCfPmSensorsClwV10OobrcRead
(
    LwU64 *pBuffer,
    LwU32 bufferSize,
    LwU32 startRow,
    LwU32 nRows, 
    BOARDOBJGRPMASK *pMask,
    LwU32 maskOffset,
    LwBool bDevPar 
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 regOffset;
    LwU32 cntLow32;
    LwU32 cntHigh32;
    LwU32 oobrcControl;
    LwU32 cntIdx;
    LwU32 clmIdx;
    LwU32 rowIdx;
    LwU32 oobrcRowOffset;
    LwU32 oobrcRowIdx;

    /* pre-check pBuffer */
    PMU_ASSERT_TRUE_OR_GOTO(status, ((pBuffer != NULL) && (pMask != NULL)),
            FLCN_ERR_ILWALID_ARGUMENT, perfCfPmSensorsClwV10OobrcRead_exit);

    /* pre-check read rows are in section */
    if(bDevPar)
    {
        /* Device Section */
        PMU_ASSERT_TRUE_OR_GOTO(status, ((startRow + nRows) <= PERF_CF_PM_SENSORS_CLW_V10_OOBRC_DEV_SECTION_ROWS), FLCN_ERR_OUT_OF_RANGE,
                    perfCfPmSensorsClwV10OobrcRead_exit);

        oobrcRowOffset = PERF_CF_PM_SENSORS_CLW_V10_OOBRC_PAR_SECTION_ROWS + startRow;
    }
    else
    {
        /* Partition Section */
        PMU_ASSERT_TRUE_OR_GOTO(status, ((startRow + nRows) <= PERF_CF_PM_SENSORS_CLW_V10_OOBRC_PAR_SECTION_ROWS), FLCN_ERR_OUT_OF_RANGE,
                    perfCfPmSensorsClwV10OobrcRead_exit);

        oobrcRowOffset = startRow;
    }

    oobrcControl = DRF_DEF(_PERF_PMASYS_OOBRC, _CONTROL, _READ_DOIT, _TRUE);
    for (rowIdx = 0; rowIdx < nRows; rowIdx++)
    {
        oobrcRowIdx = oobrcRowOffset + rowIdx;

        // set read row oobrcRowIdx
        oobrcControl = FLD_SET_DRF_NUM(_PERF_PMASYS_OOBRC, _CONTROL, _READ_IDX, oobrcRowIdx, oobrcControl);
        REG_WR32(BAR0, LW_PERF_PMASYS_OOBRC_CONTROL, oobrcControl);
       
        // Wait until the row has read into the data registers
        // Follow-up -- check if we really need this???
        PMU_ASSERT_OK_OR_GOTO(status,
            OS_PTIMER_SPIN_WAIT_US_COND(s_perfCfPmSensorsClwV10OobrcDataReady, &oobrcRowIdx,
                    PM_SENSOR_CLW_OOBRC_DATA_READY_TIMEOUT_US) ? FLCN_OK : FLCN_ERR_TIMEOUT,
                    perfCfPmSensorsClwV10OobrcRead_exit);

        for (clmIdx = 0; clmIdx < PERF_CF_PM_SENSORS_CLW_V10_COUNTERS_PER_OOBRC_ROW; clmIdx++)
        {
            regOffset = LW_PERF_PMASYS_OOBRC_DATAREGS(2 * clmIdx);
            cntIdx = rowIdx * PERF_CF_PM_SENSORS_CLW_V10_COUNTERS_PER_OOBRC_ROW + clmIdx;

            /* Don't read if signalsSupportedMask is not set */
            if (boardObjGrpMaskBitGet_SUPER(pMask, (maskOffset + cntIdx)))
            {
                /* Check the write boundary */
                PMU_ASSERT_TRUE_OR_GOTO(status, (cntIdx < bufferSize),
                    FLCN_ERR_OUT_OF_RANGE, perfCfPmSensorsClwV10OobrcRead_exit);

                cntLow32 = REG_RD32(BAR0, regOffset);
                cntHigh32 = REG_RD32(BAR0, regOffset + 4);
                pBuffer[cntIdx] = ((LwU64)cntHigh32 << 32) | cntLow32;
            }
        }
    }

perfCfPmSensorsClwV10OobrcRead_exit:
    return status;
}

/*!
 * @brief Helper function to clear all bits in pmSensorClwSnapMask.
 */
void perfCfPmSensorClwV10SnapMaskClear(void)
{
    memset(&pmSensorClwSnapMask,  0, sizeof(pmSensorClwSnapMask));
}

/*!
 * @brief Wait the CLW snap to complete
 *
 * @return   LW_TRUE if SNAP is completed, LW_FALSE otherwise.
 *
 * @return FLCN_OK
 *     Counters are read successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Incorrect pointer parameters (i.e. NULL) provided.
 */
FLCN_STATUS
perfCfPmSensorClwV10WaitSnapComplete(void)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        OS_PTIMER_SPIN_WAIT_US_COND(s_perfCfPmSensorClwV10SwTriggerSnapCleared, NULL,
            PM_SENSOR_CLW_SW_TRIGGER_SNAP_TIMEOUT_US) ? FLCN_OK : FLCN_ERR_TIMEOUT,
            perfCfPmSensorClwV10WaitSnapComplete_exit);

perfCfPmSensorClwV10WaitSnapComplete_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief Helper function to disable CLW modules (GPC/FBP/SYS).
 *
 * @param[in]  dataConfigOffset     CLW module's data configuration register offset
 * @param[in]  selwreConfigOffset   CLW module's secure configuration register offset
 *
 */
static void
s_perfCfPmSensorsClwV10DisableHelper
(
    LwU32 dataConfigOffset,
    LwU32 selwreConfigOffset
)
{
    LwU32 dataConfigValue = REG_RD32(BAR0, dataConfigOffset);
    LwU32 SelwreConfigValue = REG_RD32(BAR0, selwreConfigOffset);

    dataConfigValue = FLD_SET_DRF(_PERF_CLW_GPC0, _DATA_CONFIG, _PARTITION_STREAM, _DISABLE, dataConfigValue);
    dataConfigValue = FLD_SET_DRF(_PERF_CLW_GPC0, _DATA_CONFIG, _DEVICE_STREAM, _DISABLE, dataConfigValue);
    dataConfigValue = FLD_SET_DRF(_PERF_CLW_GPC0, _DATA_CONFIG, _ULC_PM, _DISABLE, dataConfigValue);
    dataConfigValue = FLD_SET_DRF(_PERF_CLW_GPC0, _DATA_CONFIG, _IN_SILICON_CTB, _DISABLE, dataConfigValue);
    REG_WR32(BAR0, dataConfigOffset, dataConfigValue);

    SelwreConfigValue = FLD_SET_DRF(_PERF_CLW_SYS0, _SELWRE_CONFIG, _COMMAND_PKT_DECODER, _DISABLE, SelwreConfigValue);
    REG_WR32(BAR0, selwreConfigOffset, SelwreConfigValue);
}

/*!
 * @brief Helper function to disable CLW in GPC.
 *
 * @param[in]  gpcIdx    The GPC index where CLW to disable
 * @param[in]  tpcMask   The TPC mask under GPC where CLW to disable
 *
 */
static void
s_perfCfPmSensorsClwV10DisableGpc
(
    LwU8 gpcIdx,
    LwU16 tpcMask
)
{
    LwU8 tpcIdx;

    FOR_EACH_INDEX_IN_MASK(16, tpcIdx, tpcMask)
    {
        s_perfCfPmSensorsClwV10DisableHelper(LW_PERF_DATA_CONFIG_GPC(gpcIdx, tpcIdx),
                                          LW_PERF_SELWRE_CONFIG_GPC(gpcIdx, tpcIdx));
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Helper function to disable CLW in FBP.
 *
 * @param[in]  fbpIdx    The FPB index where CLW to disable
 * @param[in]  ltspMask  The LTSP mask under GPC where CLW to disable
 *
 */
static void
s_perfCfPmSensorsClwV10DisableFbp
(
    LwU8 fbpIdx,
    LwU16 ltspMask
)
{
    LwU8 ltspIdx;

    FOR_EACH_INDEX_IN_MASK(8, ltspIdx, ltspMask)
    {
        s_perfCfPmSensorsClwV10DisableHelper(LW_PERF_DATA_CONFIG_FBP(fbpIdx, ltspIdx),
                                          LW_PERF_SELWRE_CONFIG_FBP(fbpIdx, ltspIdx));
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Helper function to disable CLW in SYS.
 *
 * @param[in]  sysIdx    The SYS index where CLW to disable
 *
 */
static void
s_perfCfPmSensorsClwV10DisableSys
(
    LwU8 sysIdx
)
{
        s_perfCfPmSensorsClwV10DisableHelper(LW_PERF_DATA_CONFIG_SYS(sysIdx),
                                          LW_PERF_SELWRE_CONFIG_SYS(sysIdx));
}

/*!
 * @brief Helper function to disable all CLWs.
 */
static void
s_perfCfPmSensorsClwV10DisableAll(void)
{
    LwU8 gpcIdx;
    LwU8 fbpIdx;
    LwU8 sysIdx;
    
    FOR_EACH_INDEX_IN_MASK(8, gpcIdx, pmSensorClwGpcsInitConfig.gpcMask)
    {
        s_perfCfPmSensorsClwV10DisableGpc(gpcIdx, pmSensorClwGpcsInitConfig.gpc[gpcIdx].tpcMask);
    }
    FOR_EACH_INDEX_IN_MASK_END;
    
    FOR_EACH_INDEX_IN_MASK(16, fbpIdx, pmSensorClwFbpsInitConfig.fbpMask)
    {
        s_perfCfPmSensorsClwV10DisableFbp(fbpIdx, pmSensorClwFbpsInitConfig.fbp[fbpIdx].ltspMask);
    }
    FOR_EACH_INDEX_IN_MASK_END;
    
    FOR_EACH_INDEX_IN_MASK(8, sysIdx, pmSensorClwSysInitConfig.sysMask)
    {
        s_perfCfPmSensorsClwV10DisableSys(sysIdx);
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Helper function to reset CLW modules (GPC/FBP/SYS).
 *
 * @param[in]  dataConfigOffset     CLW module's data configuration register offset
 * @param[in]  selwreConfigOffset   CLW module's secure configuration register offset
 * @param[in]  controlOffset        CLW module's control configuration register offset
 * @param[in]  counterInitOffset    CLW module's counter init configuration register offset
 *
 */
static void
s_perfCfPmSensorsClwV10ResetHelper
(
    LwU32 selwreConfigOffset,
    LwU32 dataConfigOffset,
    LwU32 controlOffset,
    LwU32 counterInitOffset
)
{
    LwU32 temp = DRF_DEF(_PERF_CLW_SYS0, _SELWRE_CONFIG, _COMMAND_PKT_DECODER, _DISABLE) |
        DRF_DEF(_PERF_CLW_SYS0, _SELWRE_CONFIG, _CMD_SLICE_ID, _INIT) |
        DRF_DEF(_PERF_CLW_SYS0, _SELWRE_CONFIG, _CHANNEL_ID, _INIT);

    REG_WR32(BAR0, selwreConfigOffset, temp);
    REG_WR32(BAR0, controlOffset, DRF_DEF(_PERF_CLW_SYS0, _CONTROL, _RESET_RAMS, _DOIT));
    REG_WR32(BAR0, dataConfigOffset, 0);
    REG_WR32(BAR0, counterInitOffset, 0);
    REG_WR32(BAR0, controlOffset, DRF_DEF(_PERF_CLW_SYS0, _CONTROL, _UNFREEZE, _DOIT));
}

/*!
 * @brief Helper function to reset CLW in GPC.
 *
 * @param[in]  gpcIdx    The GPC index where CLW to reset
 * @param[in]  tpcMask   The TPC mask under GPC where CLW to reset
 *
 */
static void
s_perfCfPmSensorsClwV10ResetGpc
(
    LwU8 gpcIdx,
    LwU16 tpcMask
)
{
    LwU8 tpcIdx;

    FOR_EACH_INDEX_IN_MASK(16, tpcIdx, tpcMask)
    {
        s_perfCfPmSensorsClwV10ResetHelper(LW_PERF_SELWRE_CONFIG_GPC(gpcIdx, tpcIdx),
                                        LW_PERF_DATA_CONFIG_GPC(gpcIdx, tpcIdx),
                                        LW_PERF_CONTROL_GPC(gpcIdx, tpcIdx),
                                        LW_PERF_COUNTER_INIT_GPC(gpcIdx, tpcIdx));
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Helper function to reset CLW in FBP.
 *
 * @param[in]  fbpIdx    The FPB index where CLW to reset
 * @param[in]  ltspMask  The LTSP mask under GPC where CLW to reset
 *
 */
static void
s_perfCfPmSensorsClwV10ResetFbp
(
    LwU8 fbpIdx,
    LwU16 ltspMask
)
{
    LwU8 ltspIdx;

    FOR_EACH_INDEX_IN_MASK(8, ltspIdx, ltspMask)
    {
        s_perfCfPmSensorsClwV10ResetHelper(LW_PERF_SELWRE_CONFIG_FBP(fbpIdx, ltspIdx),
                                        LW_PERF_DATA_CONFIG_FBP(fbpIdx, ltspIdx),
                                        LW_PERF_CONTROL_FBP(fbpIdx, ltspIdx),
                                        LW_PERF_DATA_CONFIG_FBP(fbpIdx, ltspIdx));
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Helper function to reset CLW in SYS.
 *
 * @param[in]  sysIdx    The SYS index where CLW to reset
 *
 */
static void
s_perfCfPmSensorsClwV10ResetSys
(
    LwU8 sysIdx
)
{
    s_perfCfPmSensorsClwV10ResetHelper(LW_PERF_SELWRE_CONFIG_SYS(sysIdx),
                                    LW_PERF_DATA_CONFIG_SYS(sysIdx),
                                    LW_PERF_CONTROL_SYS(sysIdx),
                                    LW_PERF_COUNTER_INIT_SYS(sysIdx));
}

/*!
 * @brief Helper function to reset all CLWs.
 */
static void
s_perfCfPmSensorsClwV10ResetAll(void)
{
    
    LwU8 gpcIdx;
    LwU8 fbpIdx;
    LwU8 sysIdx;
    
    FOR_EACH_INDEX_IN_MASK(8, gpcIdx, pmSensorClwGpcsInitConfig.gpcMask)
    {
        s_perfCfPmSensorsClwV10ResetGpc(gpcIdx, pmSensorClwGpcsInitConfig.gpc[gpcIdx].tpcMask);
    }
    FOR_EACH_INDEX_IN_MASK_END;
    
    FOR_EACH_INDEX_IN_MASK(16, fbpIdx, pmSensorClwFbpsInitConfig.fbpMask)
    {
        s_perfCfPmSensorsClwV10ResetFbp(fbpIdx, pmSensorClwFbpsInitConfig.fbp[fbpIdx].ltspMask);
    }
    FOR_EACH_INDEX_IN_MASK_END;
    
    FOR_EACH_INDEX_IN_MASK(8, sysIdx, pmSensorClwSysInitConfig.sysMask)
    {
        s_perfCfPmSensorsClwV10ResetSys(sysIdx);
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Helper function to configure CLW modules (GPC/FBP/SYS).
 *
 * @param[in]  dataConfigOffset     CLW module's data configuration register offset
 * @param[in]  selwreConfigOffset   CLW module's secure configuration register offset
 * @param[in]  cg2Offset            CLW module's CG2 configuration register offset
 * @param[in]  overflowEn           Indictor  if overflow mode should be enabled
 * @param[in]  pBaseCfg             Pointer to the CLW configuration data
 *
 */
static void
s_perfCfPmSensorsClwV10ConfigHelper
(
    LwU32 dataConfigOffset,
    LwU32 cg2Offset,
    LwU32 selwreConfigOffset,
    LwBool overflowEn,
    PERF_CF_PM_SENSORS_CLW_BASE_CONFIG *pBaseCfg
)
{
    LwU32 dataConfigVal = REG_RD32(BAR0, dataConfigOffset);
    LwU32 cg2Val = REG_RD32(BAR0, cg2Offset);
    LwU32 selwreConfigVal = REG_RD32(BAR0, selwreConfigOffset);

    cg2Val = FLD_SET_DRF(_PERF_CLW_GPC0, _CG2, _SLCG, _DISABLED, cg2Val);
    REG_WR32(BAR0, cg2Offset, cg2Val);

    dataConfigVal = FLD_SET_DRF(_PERF_CLW_GPC0, _DATA_CONFIG, _IN_SILICON_CTB, _DISABLE, dataConfigVal);
    dataConfigVal = FLD_SET_DRF(_PERF_CLW_GPC0, _DATA_CONFIG, _ULC_PM, _ENABLE, dataConfigVal);
    dataConfigVal = FLD_SET_DRF_NUM(_PERF_CLW_GPC0, _DATA_CONFIG, _PARTITION_STARTING_ROW_INDEX, pBaseCfg->partitionStartRowIdx, dataConfigVal);
    dataConfigVal = FLD_SET_DRF_NUM(_PERF_CLW_GPC0, _DATA_CONFIG, _DEVICE_STARTING_ROW_INDEX, pBaseCfg->deviceStartRowIdx, dataConfigVal);
    
    if (overflowEn == LW_TRUE)
    {
        dataConfigVal = FLD_SET_DRF(_PERF_CLW_GPC0, _DATA_CONFIG, _EMIT_RECORDS_ON_OVERFLOW, _ENABLE, dataConfigVal);
    }
    else
    {
        dataConfigVal = FLD_SET_DRF(_PERF_CLW_GPC0, _DATA_CONFIG, _EMIT_RECORDS_ON_OVERFLOW, _DISABLE, dataConfigVal);
    }

    if (pBaseCfg->bPartitionStreamEn == LW_TRUE)
    {
        dataConfigVal = FLD_SET_DRF(_PERF_CLW_GPC0, _DATA_CONFIG, _PARTITION_STREAM, _ENABLE, dataConfigVal);
    }
    else
    {
        dataConfigVal = FLD_SET_DRF(_PERF_CLW_GPC0, _DATA_CONFIG, _PARTITION_STREAM, _DISABLE, dataConfigVal);
    }

    if (pBaseCfg->bDeviceStreamEn == LW_TRUE)
    {
        dataConfigVal = FLD_SET_DRF(_PERF_CLW_GPC0, _DATA_CONFIG, _DEVICE_STREAM, _ENABLE, dataConfigVal);
    }
    else
    {
        dataConfigVal = FLD_SET_DRF(_PERF_CLW_GPC0, _DATA_CONFIG, _DEVICE_STREAM, _DISABLE, dataConfigVal);
    }

    REG_WR32(BAR0, dataConfigOffset, dataConfigVal);

    selwreConfigVal = FLD_SET_DRF(_PERF_CLW_SYS0, _SELWRE_CONFIG, _COMMAND_PKT_DECODER, _ENABLE, selwreConfigVal);
    REG_WR32(BAR0, selwreConfigOffset, selwreConfigVal);
}

/*!
 * @brief Helper function to Configure CLW in GPC.
 *
 * @param[in]  gpcIdx    The GPC index where CLW to Configure
 * @param[in]  tpcMask   The TPC mask under GPC where CLW to Configure
 *
 */
static void
s_perfCfPmSensorsClwV10ConfigGpc
(
    LwU8 gpcIdx,
    PERF_CF_PM_SENSORS_CLW_GPC_CONFIG *pGpcCfg
)
{
    LwU8 tpcIdx;

    FOR_EACH_INDEX_IN_MASK(16, tpcIdx, pGpcCfg->tpcMask)
    {
        s_perfCfPmSensorsClwV10ConfigHelper(LW_PERF_DATA_CONFIG_GPC(gpcIdx, tpcIdx),
                                         LW_PERF_CG2_GPC(gpcIdx, tpcIdx),
                                         LW_PERF_SELWRE_CONFIG_GPC(gpcIdx, tpcIdx),
                                         LW_TRUE,
                                         &pGpcCfg->tpc[tpcIdx]);
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Helper function to Configure CLW in FBP.
 *
 * @param[in]  fbpIdx    The FPB index where CLW to Configure
 * @param[in]  ltspMask  The LTSP mask under GPC where CLW to Configure
 *
 */
static void
s_perfCfPmSensorsClwV10ConfigFbp
(
    LwU8 fbpIdx,
    PERF_CF_PM_SENSORS_CLW_FBP_CONFIG *pFbpCfg
)
{
    LwU8 ltspIdx;

    FOR_EACH_INDEX_IN_MASK(8, ltspIdx, pFbpCfg->ltspMask)
    {
        s_perfCfPmSensorsClwV10ConfigHelper(LW_PERF_DATA_CONFIG_FBP(fbpIdx, ltspIdx),
                                         LW_PERF_CG2_FBP(fbpIdx, ltspIdx),
                                         LW_PERF_SELWRE_CONFIG_FBP(fbpIdx, ltspIdx),
                                         LW_TRUE,
                                         &pFbpCfg->ltsp[ltspIdx]);
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Helper function to Configure CLW in SYS.
 *
 * @param[in]  sysIdx    The SYS index where CLW to Configure
 *
 */
static void
s_perfCfPmSensorsClwV10ConfigSys
(
    LwU8 sysIdx,
    PERF_CF_PM_SENSORS_CLW_BASE_CONFIG  *pSysCfg
)
{
        s_perfCfPmSensorsClwV10ConfigHelper(LW_PERF_DATA_CONFIG_SYS(sysIdx),
                                         LW_PERF_CG2_SYS(sysIdx),
                                         LW_PERF_SELWRE_CONFIG_SYS(sysIdx),
                                         LW_TRUE,
                                         pSysCfg);
}

/*!
 * @brief Helper function to configure OOBRC.
 *
 * @param[in]  enable    To enable/disable OOBRC
 *
 */
static void
s_perfCfPmSensorsClwV10OobrcConfig
(
    LwBool enable
)
{
    LwU32 oobrConfigValue = REG_RD32(BAR0, LW_PERF_PMASYS_OOBRC_CONFIG);

    if (enable)
    {
        oobrConfigValue = FLD_SET_DRF(_PERF_PMASYS_OOBRC, _CONFIG, _DEVICE_AGGREGATION, _ENABLED, oobrConfigValue);
        oobrConfigValue = FLD_SET_DRF(_PERF_PMASYS_OOBRC, _CONFIG, _PARTITION_AGGREGATION, _ENABLED, oobrConfigValue);
    }
    else
    {
        oobrConfigValue = FLD_SET_DRF(_PERF_PMASYS_OOBRC, _CONFIG, _DEVICE_AGGREGATION, _DISABLED, oobrConfigValue);
        oobrConfigValue = FLD_SET_DRF(_PERF_PMASYS_OOBRC, _CONFIG, _PARTITION_AGGREGATION, _DISABLED, oobrConfigValue);
    }

    REG_WR32(BAR0, LW_PERF_PMASYS_OOBRC_CONFIG, oobrConfigValue);
}

/*!
 * @brief Helper function to disable OOBRC.
 *
 */
static void
s_perfCfPmSensorsClwV10OobrcDisable(void)
{
    s_perfCfPmSensorsClwV10OobrcConfig(LW_FALSE);
}

/*!
 * @brief Helper function to enable OOBRC.
 *
 */
static void
s_perfCfPmSensorsClwV10OobrcEnable(void)
{
    s_perfCfPmSensorsClwV10OobrcConfig(LW_TRUE);
}

/*!
 * @brief Helper function to reset OOBRC.
 *
 */
static void
s_perfCfPmSensorsClwV10OobrcReset(void)
{
    LwU32 temp = DRF_DEF(_PERF_PMASYS_OOBRC, _CONTROL, _RESET_DOIT, _TRUE) |
        DRF_DEF(_PERF_PMASYS_OOBRC, _CONTROL, _OOB_DEBUG_RECORD_CNTR_CLEAR, _DOIT);

    REG_WR32(BAR0, LW_PERF_PMASYS_OOBRC_CONTROL, temp);
}

/*!
 * @brief Helper function to initialize PMM Rounter.
 *
 */
static void
s_perfCfPmSensorsClwV10RouterInit(void)
{
    const LwU32 commonDebug[] = {
        LW_PERF_PMMSYS_SYS0ROUTER_OOB_CHANNEL_DEBUG,
        LW_PERF_PMMFBP_FBPSROUTER_OOB_CHANNEL_DEBUG,
        LW_PERF_PMMGPC_GPCSROUTER_OOB_CHANNEL_DEBUG
    };

    for (LwU32 i = 0; i < (sizeof(commonDebug) / sizeof(commonDebug[0])); i++)
    {
        REG_WR32(BAR0, commonDebug[i], 0);
    }

    REG_WR32(BAR0, LW_PERF_PMMSYS_SYS0ROUTER_OOB_CHANNEL_CONFIG_SELWRE,
        DRF_NUM(_PERF_PMMSYS_SYS0ROUTER_OOB_CHANNEL, _CONFIG_SELWRE, _HS_CREDITS, PERF_CF_PM_SENSORS_CLW_V10_CREDITS_SYS));

    REG_WR32(BAR0, LW_PERF_PMMFBP_FBPSROUTER_OOB_CHANNEL_CONFIG_SELWRE,
        DRF_NUM(_PERF_PMMFBP_FBPSROUTER_OOB_CHANNEL, _CONFIG_SELWRE, _HS_CREDITS, PERF_CF_PM_SENSORS_CLW_V10_CREDITS_PER_FBP));

     REG_WR32(BAR0, LW_PERF_PMMGPC_GPCSROUTER_OOB_CHANNEL_CONFIG_SELWRE,
        DRF_NUM(_PERF_PMMGPC_GPCSROUTER_OOB_CHANNEL, _CONFIG_SELWRE, _HS_CREDITS, PERF_CF_PM_SENSORS_CLW_V10_CREDITS_PER_GPC));
}

/*!
 * @brief Helper function to initialize PMA Trigger.
 *
 */
static void
s_perfCfPmSensorsClwV10PmaTriggersInit(void)
{
    // Only call when OOBRC and CLWs are inactive.
    // I'm not sure this is necessary, this originated from Greg Smith's very early example.
    // LW_PERF_PMASYS_OOB_COMMAND_SLICE_FREEZE_CONFIG
    // @Pranav - do CLWs support freeze.
    // @Greg - review freeze command packet
    // uint32_t reg_freeze_config = (0
    //     | DRF_DEF(_PERF_PMASYS, _OOB_COMMAND_SLICE_FREEZE_CONFIG, _FREEZE_MODE, _NORMAL)
    //     | DRF_DEF(_PERF_PMASYS, _OOB_COMMAND_SLICE_FREEZE_CONFIG, _PMA2ALL_CTXSW, _DISABLED)
    //     | DRF_DEF(_PERF_PMASYS, _OOB_COMMAND_SLICE_FREEZE_CONFIG, _HOLD_MODEC, _DISABLED)
    //     | DRF_DEF(_PERF_PMASYS, _OOB_COMMAND_SLICE_FREEZE_CONFIG, _HOLD_TIMEBASES, _DISABLED)
    //     | DRF_DEF(_PERF_PMASYS, _OOB_COMMAND_SLICE_FREEZE_CONFIG, _HOLD_ALLCOUNTERS, _DISABLED)
    //     );
    // s_perfCfPmSensorsClwV10PriWr32(LW_PERF_PMASYS_OOB_COMMAND_SLICE_FREEZE_CONFIG, reg_freeze_config, 0xFFFFFFFFul);

    // LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_CONTROL
    // Do not set. Only used to send manual commands.

    // LW_PERF_PMA_OOB_COMMAND_SLICE_TRIGGER_MASK_SELWRE0 = 0
    // LW_PERF_PMA_OOB_COMMAND_SLICE_TRIGGER_MASK_SELWRE1 = 0
    // @Pranav - do we need to enable PMA?

    LwU32 reg_mask0 = (PERF_CF_PM_SENSORS_CLW_V10_ENGINE_ID_PMA) <  32 ? (1 << (PERF_CF_PM_SENSORS_CLW_V10_ENGINE_ID_PMA     )) : 0;
    LwU32 reg_mask1 = (PERF_CF_PM_SENSORS_CLW_V10_ENGINE_ID_PMA) >= 32 ? (1 << (PERF_CF_PM_SENSORS_CLW_V10_ENGINE_ID_PMA - 32)) : 0;

    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_MASK_SELWRE0, reg_mask0);
    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_MASK_SELWRE1, reg_mask1);

    // SKIP LW_PERF_PMA_OOB_COMMAND_SLICE_CTXSW_CONFIG_FIRMWARE

    // START and STOP MASK0/1 & with MASK_SELWRE0/1
    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_START_MASK0, PERF_CF_PM_SENSORS_CLW_V10_ALL_BITS_MASK);
    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_START_MASK1, PERF_CF_PM_SENSORS_CLW_V10_ALL_BITS_MASK);

    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_STOP_MASK0, PERF_CF_PM_SENSORS_CLW_V10_ALL_BITS_MASK);
    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_STOP_MASK1, PERF_CF_PM_SENSORS_CLW_V10_ALL_BITS_MASK);

    // Set Tesla mode on all engines
    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_CONFIG_TESLA_MODE0, PERF_CF_PM_SENSORS_CLW_V10_ALL_BITS_MASK);
    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_CONFIG_TESLA_MODE1, PERF_CF_PM_SENSORS_CLW_V10_ALL_BITS_MASK);

    // Disable mixed mode
    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_CONFIG_MIXED_MODE0, 0);
    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_CONFIG_MIXED_MODE1, 0);

    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_START0, 0);
    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_START1, 0);

    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_STOP0, 0);
    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_STOP1, 0);

    // LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_STATUS0/1
    // SKIP updating trigger status. In Tesla mode this should not matter. Toggling this may
    // result in a trigger being output.

    // s_perfCfPmSensorsClwV10PriWr32(LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_STATUS0, 0, PERF_CF_PM_SENSORS_CLW_V10_ALL_BITS_MASK);
    // s_perfCfPmSensorsClwV10PriWr32(LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_STATUS1, 0, PERF_CF_PM_SENSORS_CLW_V10_ALL_BITS_MASK);

    // Reset for debugging
    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_COUNT, 0);
    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_RECORD_START_TRIGGERCNT, 0);
    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_RECORD_STOP_TRIGGERCNT, 0);
    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_RECORD_TOTAL_TRIGGERCNT, 0);

    // LW_PERF_PMA_OOB_COMMAND_SLICE_TRIGGER_CONFIG_USER
    LwU32 reg_config_user = (0
        | DRF_DEF(_PERF_PMASYS, _OOB_COMMAND_SLICE_TRIGGER_CONFIG_USER, _PMA_PULSE, _DISABLE)
        | DRF_DEF(_PERF_PMASYS, _OOB_COMMAND_SLICE_TRIGGER_CONFIG_USER, _PMA_PULSE_WINDOW, _FREERUN)
        | DRF_DEF(_PERF_PMASYS, _OOB_COMMAND_SLICE_TRIGGER_CONFIG_USER, _PULSE_SOURCE, _INTERNAL)
        | DRF_DEF(_PERF_PMASYS, _OOB_COMMAND_SLICE_TRIGGER_CONFIG_USER, _PMA_RECORD_PULSE_CNTR, _ONE)
        | DRF_DEF(_PERF_PMASYS, _OOB_COMMAND_SLICE_TRIGGER_CONFIG_USER, _GLOBAL_TESLA_TRIGGER_MODE, _ENABLE)
// TODO - decide if _RECORD_STREAM == _ENABLE at INIT.
        | DRF_DEF(_PERF_PMASYS, _OOB_COMMAND_SLICE_TRIGGER_CONFIG_USER, _RECORD_STREAM, _ENABLE)
        | DRF_DEF(_PERF_PMASYS, _OOB_COMMAND_SLICE_TRIGGER_CONFIG_USER, _PMACTXSW_MODE, _DISABLE)
        | DRF_DEF(_PERF_PMASYS, _OOB_COMMAND_SLICE_TRIGGER_CONFIG_USER, _FE2ALL_CTXSW_FREEZE, _DISABLE)
        );
    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_TRIGGER_CONFIG_USER, reg_config_user);

    LwU32 reg_selwre = (0
        | DRF_NUM(_PERF_PMASYS, _OOB_COMMAND_SLICE_RECORD_SELECT_SELWRE, _RECORD_TRIGGER_SELECT, PERF_CF_PM_SENSORS_CLW_V10_ENGINE_ID_PMA));
        // | DRF_DEF(_PERF_PMASYS, _OOB_COMMAND_SLICE_TRIGGER_CONFIG_USER, _CTXSW_MODE, _DISABLED)
        // | DRF_NUM(_PERF_PMASYS, _OOB_COMMAND_SLICE_TRIGGER_CONFIG_USER, _CTXSW_ENGINE, 0);

    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_RECORD_SELECT_SELWRE, reg_selwre);

    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_PULSE_TIMEBASESET, 0x1000);
    REG_WR32(BAR0, LW_PERF_PMASYS_OOB_COMMAND_SLICE_PULSE_TIMEBASECNT, 0);
}

/*!
 * @brief Helper function to reset CLW system (CLW/OOBRC/PMA/ROUTER).
 *
 */
static void
s_perfCfPmSensorsClwV10Reset(void)
{
    s_perfCfPmSensorsClwV10DisableAll();
    s_perfCfPmSensorsClwV10OobrcDisable();
    s_perfCfPmSensorsClwV10ResetAll();
    s_perfCfPmSensorsClwV10OobrcReset();
    s_perfCfPmSensorsClwV10PmaTriggersInit();
    s_perfCfPmSensorsClwV10RouterInit();
    s_perfCfPmSensorsClwV10OobrcEnable();
}

/*!
 * @brief Helper function to check if the OOBRC row is ready to read.
 * @param[in]  pRowIdx    Pointer to the OOBRC row to read
 *
 */
static LwBool
s_perfCfPmSensorsClwV10OobrcDataReady
(
    LwU32 *pRowIdx
)
{
    return FLD_TEST_DRF_NUM(_PERF_PMASYS_OOBRC, _DEBUG1, _LAST_READ_IDX, (*pRowIdx), REG_RD32(BAR0, LW_PERF_PMASYS_OOBRC_DEBUG1));
}

/*!
 * @brief Check if the CLW snap to complete
 *
 * @param    pArgs   Unused
 *
 * @return   LW_TRUE if SNAP is completed, LW_FALSE otherwise.
 */
LwBool
s_perfCfPmSensorClwV10SwTriggerSnapCleared
(
    void *pArgs
)
{
    //check readiness of GPCs/FBPs/SYSs
    //return LW_TRUE;
    
    LwU8  gpcIdx;
    LwU16 fbpIdx;

    /* Check readiness of GPCs */
    FOR_EACH_INDEX_IN_MASK(8, gpcIdx, pmSensorClwSnapMask.snapMask.gpcMask)
    {
        if (!FLD_TEST_DRF(_PERF_PMMGPCROUTER, _OOB_CHANNEL_ENGINESTATUS, _STATUS, _EMPTY, REG_RD32(BAR0, LW_PERF_PMMGPCXROUTER_OOB_CHANNEL_ENGINESTATUS(gpcIdx))))
        {
            return LW_FALSE;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    /* Check readiness of FBPs */
    FOR_EACH_INDEX_IN_MASK(16, fbpIdx, pmSensorClwSnapMask.snapMask.fbpMask)
    {
        if (!FLD_TEST_DRF(_PERF_PMMFBPROUTER, _OOB_CHANNEL_ENGINESTATUS, _STATUS, _EMPTY, REG_RD32(BAR0, LW_PERF_PMMFBPXROUTER_OOB_CHANNEL_ENGINESTATUS(fbpIdx))))
        {
            return LW_FALSE;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    /* Check readiness of SYS */
    if (pmSensorClwSnapMask.snapMask.sysMask)
    {
        if (!FLD_TEST_DRF(_PERF_PMMSYSROUTER, _OOB_CHANNEL_ENGINESTATUS, _STATUS, _EMPTY, REG_RD32(BAR0, LW_PERF_PMMSYS_SYS0ROUTER_OOB_CHANNEL_ENGINESTATUS)))
        {
            return LW_FALSE;
        }
    }

    return LW_TRUE;
}

/*!
 * @brief Update the CLW configuration per floorsweeping info
 *
 */
static void
s_perfCfPmSensorsClwV10Floorsweeping(void)
{
    LwU32 gpcMaskPhysical = 0x0;
    LwU32 gpcMaskLogical = 0x0;
    LwU8 gpcPhysicalIdx[LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_GPC_NUM] = {0};
    LwU8 fbpPhysicalIdx[LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_FBP_NUM] = {0};
    LwU32 tpcMask;
    LwU32 fpcMaskPhysical = 0x0;
    LwU32 fpcMaskLogical = 0x0;

    gpcMaskPhysical = ~fuseRead(LW_FUSE_STATUS_OPT_GPC) & PERF_CF_PM_SENSORS_CLW_V10_GPC_MASK;

    /* read gpcPhysicalIdx */
    for(unsigned int rsIdx = 0; rsIdx < LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_GPC_NUM; rsIdx++)
    {
        /* Retrieve map between logical and physical */
        OS_ASSERT(GPC_LOGICAL_INDEX(rsIdx) < LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_GPC_NUM);
        OS_ASSERT(GPC_PHYSICAL_INDEX(rsIdx) < LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_GPC_NUM);
        gpcPhysicalIdx[GPC_LOGICAL_INDEX(rsIdx)] = GPC_PHYSICAL_INDEX(rsIdx);
    }

    for(unsigned int gpcIdx = 0; gpcIdx < LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_GPC_NUM; gpcIdx++)
    {
        /* retrieve TPC FUSE and update tpcMask */
        tpcMask = ~fuseRead(LW_FUSE_STATUS_OPT_TPC_GPC(gpcPhysicalIdx[gpcIdx])) & PERF_CF_PM_SENSORS_CLW_V10_TPC_MASK;
        pmSensorClwGpcsInitConfig.gpc[gpcIdx].tpcMask = tpcMask;

        /* gpcMaskPhysical => gpcMaskLogical */
        if(gpcMaskPhysical & (LWBIT(gpcPhysicalIdx[gpcIdx])))
        {
            gpcMaskLogical |= LWBIT(gpcIdx);
        }
    }

    /* update gpcMask */
    pmSensorClwGpcsInitConfig.gpcMask = gpcMaskLogical;

    fpcMaskPhysical = ~fuseRead(LW_FUSE_STATUS_OPT_FBP) & PERF_CF_PM_SENSORS_CLW_V10_FBP_MASK;

     /* read fbpPhysicalIdx */
    for(unsigned int rsIdx = 0; rsIdx < LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_FBP_NUM; rsIdx++)
    {
        OS_ASSERT(FBP_LOGICAL_INDEX(rsIdx) < LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_FBP_NUM);
        OS_ASSERT(FBP_PHYSICAL_INDEX(rsIdx) < LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_FBP_NUM);
        fbpPhysicalIdx[FBP_LOGICAL_INDEX(rsIdx)] = FBP_PHYSICAL_INDEX(rsIdx);
    }

    for(unsigned int fbpIdx = 0; fbpIdx < LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_FBP_NUM; fbpIdx++)
    {
        /* fpcMaskPhysical => fpcMaskLogical */
        if(fpcMaskPhysical & (LWBIT(fbpPhysicalIdx[fbpIdx])))
        {
            fpcMaskLogical |= LWBIT(fbpIdx);
        }
    }

    /* update fbpMask */
    pmSensorClwFbpsInitConfig.fbpMask = fpcMaskLogical;
}
