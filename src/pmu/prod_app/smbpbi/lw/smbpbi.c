/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   smbpbi.c
 * @brief  SMBus Post-Box Interface
 *
 * SMBPBI represents the LWPU SMBus Post-Box interface. The SMBPBI is a bi-
 * directional SMBus-based intefaced living in the GPU's I2C secondary logic.
 * The interface was introduced in the Tesla2 GPU family and offers the ability
 * for SMBus primaries (such as the System Embedded Controller) to communicate
 * with the GPU without relying on PCIE, SBIOS, and the GPU driver.
 *
 * This code lives as a sub-task of the THERM task.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objsmbpbi.h"
#include "pmu_objpmu.h"
#include "therm/objtherm.h"
#include "pmu_objpmgr.h"
#include "pmu_objperf.h"
#include "pmu_objtimer.h"
#include "pmgr/pwrpolicy.h"
#include "oob/smbpbi_shared.h"
#include "dbgprintf.h"
#include "cmdmgmt/cmdmgmt.h"
#include "pmu_objclk.h"
#include "pmu_selwrity.h"
#include "pmgr/pwrchannel_pmumon.h"
#include "perf/cf/perf_cf.h"
#include "perf/cf/client_perf_cf_pwr_model_profile.h"

#include "config/g_smbpbi_private.h"

#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"

#if PMUCFG_FEATURE_ENABLED(PMU_FSP_RPC)
    #include "dev_fsp_addendum.h"
    #include "fsp/fsp_smbpbi_rpc.h"
    #include "fsp_rpc.h"
#endif

/* ------------------------- Types and Enums -------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * PCONTROL::GET_STATUS requests are filtered to deter clients from over-
 * sampling and thrashing the GPU with requests for the current vPstate. This
 * period defined here represents the filter period. Any time that valid/fresh
 * status is returned from the command, the filter period is restarted. If a
 * request to retrieve the status comes in while this filter period is active
 * the previous (potentially stale) status is returned.
 */
#define SMBPBI_PCONTROL_GET_STATUS_FILTER_PERIOD          1000000000ull // 1 sec
#define REQ_STATUS_UNUSED                                 0U

/* ------------------------- Prototypes ------------------------------------- */
static LwBool   s_smbpbiEventSourceResetRequired(void);

/* ------------------------- Constants -------------------------------------- */
const LwU32 smbpbiEventsEdgeTriggered =
                BIT(LW_MSGBOX_EVENT_TYPE_SERVER_RESTART) |
                BIT(LW_MSGBOX_EVENT_TYPE_TGP_LIMIT_SET_SUCCESS)|
                BIT(LW_MSGBOX_EVENT_TYPE_CLOCK_LIMIT_SET_SUCCESS)|
                BIT(LW_MSGBOX_EVENT_TYPE_ECC_TOGGLE_SUCCESS)|
                BIT(LW_MSGBOX_EVENT_TYPE_MIG_TOGGLE_SUCCESS);
/* ------------------------- Global Variables ------------------------------- */
/*!
 * Static system ID (infoROM) data type attributes to drive the exelwtion
 * of LW_MSGBOX_CMD_OPCODE_GET_SYS_ID_DATA msgbox request
 */

#define LW_SMBPBI_SYS_ID_SRCTYPE                                            1:0
#define LW_SMBPBI_SYS_ID_SRCTYPE_DMEM                                       0
#define LW_SMBPBI_SYS_ID_SRCTYPE_IFR_U8                                     1
#define LW_SMBPBI_SYS_ID_SRCTYPE_IFR_U32                                    2
#define LW_SMBPBI_SYS_ID_SRCTYPE_IFR_STRING                                 3
#define LW_SMBPBI_SYS_ID_OFFSET                                             12:2
#define LW_SMBPBI_SYS_ID_SIZE                                               21:13
#define LW_SMBPBI_SYS_ID_CAPBIT                                             26:22
#define LW_SMBPBI_SYS_ID_CAPINDEX                                           28:27

#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_OBD_VERSION_2))
#define OBD_VERSION v2
#else
#define OBD_VERSION v1
#endif

#define OBD_SERIAL_NUMBER          OBD_VERSION.serialNumber
#define OBD_MARKETING_NAME         OBD_VERSION.marketingName
#define OBD_MEMORY_MANUFACTURER    OBD_VERSION.memoryManufacturer
#define OBD_MEMORY_PART_ID         OBD_VERSION.memoryPartID
#define OBD_BUILD_DATE             OBD_VERSION.buildDate

#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_OBD_VERSION_2))
#define OBD_PRODUCT_LENGTH         OBD_VERSION.productLength
#define OBD_PRODUCT_WIDTH          OBD_VERSION.productWidth
#define OBD_PRODUCT_HEIGHT         OBD_VERSION.productHeight
#else
#define OBD_PRODUCT_LENGTH         OBD_VERSION
#define OBD_PRODUCT_WIDTH          OBD_VERSION
#define OBD_PRODUCT_HEIGHT         OBD_VERSION
#endif


#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_PCIE_INFO))
#define PCIE_SPEED pcieSpeed_V1
#define PCIE_WIDTH pcieWidth_V1
#else
#define PCIE_SPEED INFOROMHALINFO_FERMI
#define PCIE_WIDTH RM_PMU_SYSMON_INFOROM_DATA
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_TGP_LIMIT))
#define TGP_LIMIT tgpLimit_V1
#else
#define TGP_LIMIT RM_PMU_SYSMON_INFOROM_DATA
#endif

/**
 * The static info like OBD and OEM is cached in INFOROMHALINFO_FERMI and now
 * the attrs use 11 bits to store the offsets of the member in the structure.
 * The assertion is to check the offsets does not exceed 11 bits. Therefore,
 * future elements to be added should be placed closed to the head of the
 * structure to avoid exceeding.
 */
#define LW_ASSERT_EXPR(expr)                              (!sizeof(char[((expr)? 1 : -1)]))
#define LW_ASSERT_INFOROM_FERMI_OFFSET(name)                                            \
    LW_ASSERT_EXPR(LW_OFFSETOF(INFOROMHALINFO_FERMI, name) <= 0x7ff)
#define LW_ASSERT_SYSMON_INFOROM_OFFSET(name)                                           \
    LW_ASSERT_EXPR(LW_OFFSETOF(RM_PMU_SYSMON_INFOROM_DATA, name) <= 0x7ff)

#define SMBPBI_SYS_ID_ATTR_INIT_DMEM(name,index,cap)                                    \
    (DRF_DEF(_SMBPBI, _SYS_ID, _SRCTYPE, _DMEM) |                                       \
    LW_ASSERT_SYSMON_INFOROM_OFFSET(name) |                                             \
    DRF_NUM(_SMBPBI, _SYS_ID, _OFFSET,                                                  \
            LW_OFFSETOF(RM_PMU_SYSMON_INFOROM_DATA, name)) |                            \
    DRF_NUM(_SMBPBI, _SYS_ID, _SIZE, sizeof Smbpbi.config.sysInforomData.name) |        \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPINDEX, index) |                                       \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPBIT, DRF_BASE(LW_MSGBOX_DATA_CAP_##index##cap)))

#define SMBPBI_SYS_ID_ATTR_INIT_INFOROM_U8(obj,name,index,cap)                          \
    (DRF_DEF(_SMBPBI, _SYS_ID, _SRCTYPE, _IFR_U8) |                                     \
    LW_ASSERT_INFOROM_FERMI_OFFSET(obj.object.name) |                                   \
    DRF_NUM(_SMBPBI, _SYS_ID, _OFFSET,                                                  \
            LW_OFFSETOF(INFOROMHALINFO_FERMI, obj.object.name)) |                       \
    DRF_NUM(_SMBPBI, _SYS_ID, _SIZE, sizeof(LwU8)) |                                    \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPINDEX, index) |                                       \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPBIT, DRF_BASE(LW_MSGBOX_DATA_CAP_##index##cap)))

#define SMBPBI_SYS_ID_ATTR_INIT_INFOROM_U32(obj,name,index,cap)                         \
    (DRF_DEF(_SMBPBI, _SYS_ID, _SRCTYPE, _IFR_U32) |                                    \
    LW_ASSERT_INFOROM_FERMI_OFFSET(obj.object.name) |                                   \
    DRF_NUM(_SMBPBI, _SYS_ID, _OFFSET,                                                  \
            LW_OFFSETOF(INFOROMHALINFO_FERMI, obj.object.name)) |                       \
    DRF_NUM(_SMBPBI, _SYS_ID, _SIZE, sizeof(LwU32)) |                                   \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPINDEX, index) |                                       \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPBIT, DRF_BASE(LW_MSGBOX_DATA_CAP_##index##cap)))

#define SMBPBI_SYS_ID_ATTR_INIT_INFOROM_STRING(obj,name,index,cap)                      \
    (DRF_DEF(_SMBPBI, _SYS_ID, _SRCTYPE, _IFR_STRING) |                                 \
    LW_ASSERT_INFOROM_FERMI_OFFSET(obj.object.name) |                                   \
    DRF_NUM(_SMBPBI, _SYS_ID, _OFFSET,                                                  \
            LW_OFFSETOF(INFOROMHALINFO_FERMI, obj.object.name)) |                       \
    DRF_NUM(_SMBPBI, _SYS_ID, _SIZE,                                                    \
            LW_ARRAY_ELEMENTS(((PINFOROMHALINFO_FERMI)0)->obj.object.name)) |           \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPINDEX, index) |                                       \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPBIT, DRF_BASE(LW_MSGBOX_DATA_CAP_##index##cap)))

#define SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(index,cap)                             \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPINDEX, index) |                                       \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPBIT, DRF_BASE(LW_MSGBOX_DATA_CAP_##index##cap))

LwU32   SysIdDataTypeAttr[LW_MSGBOX_CMD_ARG1_SYS_ID_DATA_TYPE_MAX + 1] =
{
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(boardPartNum_V1, 1, _BOARD_PART_NUM_V1),
    SMBPBI_SYS_ID_ATTR_INIT_INFOROM_STRING(OEM, oemInfo, 1, _OEM_INFO_V1),
    SMBPBI_SYS_ID_ATTR_INIT_INFOROM_STRING(OBD, OBD_SERIAL_NUMBER, 1, _SERIAL_NUM_V1),
    SMBPBI_SYS_ID_ATTR_INIT_INFOROM_STRING(OBD, OBD_MARKETING_NAME, 1, _MARKETING_NAME_V1),
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(gpuPartNum_V1, 1, _GPU_PART_NUM_V1),
    SMBPBI_SYS_ID_ATTR_INIT_INFOROM_U8(OBD, OBD_MEMORY_MANUFACTURER, 1, _MEMORY_VENDOR_V1),
    SMBPBI_SYS_ID_ATTR_INIT_INFOROM_STRING(OBD, OBD_MEMORY_PART_ID, 1, _MEMORY_PART_NUM_V1),
    SMBPBI_SYS_ID_ATTR_INIT_INFOROM_U32(OBD, OBD_BUILD_DATE, 1, _BUILD_DATE_V1),
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(firmwareVer_V1, 1, _FIRMWARE_VER_V1),
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(vendorId_V1, 1, _VENDOR_ID_V1),
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(devId_V1, 1, _DEV_ID_V1),
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(subVendorId_V1, 1, _SUB_VENDOR_ID_V1),
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(subId_V1, 1, _SUB_ID_V1),
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(gpuGuid_V1, 1, _GPU_GUID_V1),
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(inforomVer_V1, 1, _INFOROM_VER_V1),
#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_OBD_VERSION_2))
    SMBPBI_SYS_ID_ATTR_INIT_INFOROM_U32(OBD, OBD_PRODUCT_LENGTH, 2, _PRODUCT_LENGTH_V1),
    SMBPBI_SYS_ID_ATTR_INIT_INFOROM_U32(OBD, OBD_PRODUCT_WIDTH, 2, _PRODUCT_WIDTH_V1),
    SMBPBI_SYS_ID_ATTR_INIT_INFOROM_U32(OBD, OBD_PRODUCT_HEIGHT, 2, _PRODUCT_HEIGHT_V1),
#else
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(2, _PRODUCT_LENGTH_V1),
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(2, _PRODUCT_WIDTH_V1),
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(2, _PRODUCT_HEIGHT_V1),
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_PCIE_INFO))
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(pcieSpeed_V1, 2, _PCIE_SPEED_V1),
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(pcieWidth_V1, 2, _PCIE_WIDTH_V1),
#else
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(2, _PCIE_SPEED_V1),
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(2, _PCIE_WIDTH_V1),
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_TGP_LIMIT))
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(tgpLimit_V1, 2, _TGP_LIMIT_V1),
#else
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(2, _TGP_LIMIT_V1),
#endif
};

#undef SMBPBI_SYS_ID_ATTR_INIT_DMEM
#undef SMBPBI_SYS_ID_ATTR_INIT_INFOROM_U8
#undef SMBPBI_SYS_ID_ATTR_INIT_INFOROM_U32
#undef SMBPBI_SYS_ID_ATTR_INIT_INFOROM_STRING
#undef SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static void s_smbpbiSetStatus(OBJSMBPBI *pSmbpbi, LwU32 cmd, LwU32 data, LwU32 extData, LwU32 reqStatus);
static LwU8 s_smbpbiMapPmuStatusToMsgBoxStatus(FLCN_STATUS status);
static LwU8 s_smbpbiDebugHandler(LwU32 cmd, LwU32 *pData);
static LwU8 s_smbpbiRegWrClient(LwU8 reg, LwU32 data);
static LwU8 s_smbpbiEventSet(LwMsgboxEventType event);
static FLCN_STATUS s_smbpbiMapRegOpStatus(LwU8 status);
static LwU8 s_smbpbiPwrMonitorQueryTotalGpuPower(LwU32 *pData);
static LwU8 s_smbpbiPControl(LwU32 cmd, LwU32 *pData);
static void s_smbpbiAdjustTgpCap(LwU32 *pCap);
static LwU8 s_smbpbiGetPower(LwU8 source, LwU32 *pData);
static LwU8 s_smbpbiPwrPolicyIdxQuery(LwU8 idx, LwU32 *pData);
static LwU8 s_smbpbiTempGet(LwU8 sensor, LwBool bHiPrecision, LwTemp *pTemp);
static LwU8 s_smbpbiGetThermParam(LwU8 paramId, LwU32 *pData);
static LwU8 s_smbpbiGetSysId(LwU32 attr, LwU16 offset, LwU32 *pData);
static LwU8 s_smbpbiGetEccEnableState(RM_PMU_MSGBOX_CONFIG *pConfig, LwU32 *pData);
static LwU8 s_smbpbiGetSmcEnableState(OBJSMBPBI *pSmbpbi, LwU32 *pData);
static LwU8 s_smbpbiGetUtilCounter(LwU32 counter, LwU32 *pData);
static LwU8 s_smbpbiResetUtilCountersHelper(void);
static LwU8 s_smbpbiResetUtilCounter(LwU32 counter);
static LwU8 s_smbpbiGetWriteProtectionMode(LwU8 arg1, LwU32 *pData);
static FLCN_STATUS s_smbpbiGetLwrrentClockFreq(LwU32 apiDomain, LwU32 *freqKhz);
static FLCN_STATUS s_smbpbiGetPstateEntry(PSTATE *pPstate, LwU8 apiDomain, LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY *pstateClkEntry);
static LwU8 s_smbpbiGetClockFreqInfo(LwU8 arg1, LwU8 arg2, LwU32 *pData);
static void s_smbpbiHandleDataCopy(LwU32 *pCmd, LwU32 data, LwU32 extData);
static LwU8 s_smbpbiGetLwlinkInfoHandler(LwU8 arg1, LwU8 arg2, LwU32 *pCmd, LwU32 *pData, LwU32 *pExtData);
static LwU8 s_smbpbiGetPcieLinkInfoHandler(LwU8 arg1, LwU8 arg2, LwU32 *pData, LwU32 *pExtData);
static LwU8 s_smbpbiGetEnergyCounterHandler(LwU32 *pData, LwU32 *pExtData);
static LwU8 s_smbpbiGetInstantaneousPower(LwU32 *pData);
static LwU8 s_smbpbiGetAveragePower(LwU32 *pData);
static LwU8 s_smbpbiGetLwrrentPstate(LwU32 *pData);
#if PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X)
static LwU8 s_smbpbiGetPowerHintInfoHandler(LwU8 arg1, LwU8 arg2, LwU32 *pData, LwU32 *pExtData);
#endif
static FLCN_STATUS s_smbpbiGetRequest(LwU32 *cmd, LwU32 *data, LwU32 *extData);
static FLCN_STATUS s_smbpbiSetResponse(LwU32 cmd, LwU32 data, LwU32 extData, LwU32 reqStatus);

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Basic SMBPBI software initialization
 */
void
smbpbiInit(void)
{
    // when PMU_FSP_RPC is enabled, SMBPBI server is running on FSP.
    if (!PMUCFG_FEATURE_ENABLED(PMU_FSP_RPC))
    {
        // Initialize the SMBPBI interrupt defaults
        smbpbiInitInterrupt_HAL(&Smbpbi);
    }
}

void
sysMonInit(void)
{
    LwU32 cmd;

    // Initially task is being _INACTIVE until activated by RM.
    cmd  = DRF_DEF(_MSGBOX, _CMD, _STATUS, _INACTIVE);
    smbpbiExelwte_HAL(&Smbpbi, &cmd, NULL, NULL, PMU_SMBPBI_SET_RESPONSE);
}

/*!
 * Complete the SMBPBI request processing by posting status and
 * returning data.
 *
 * @param[in]   pSmbpbi   Pointer to the OBJSMBPBI object
 * @param[in]   cmd       command/status register
 * @param[in]   data      data register
 * @param[in]   extData   extended data register
 * @param[in]   reqStatus Status of the smbpbi request
 */
static void
s_smbpbiSetStatus
(
    OBJSMBPBI  *pSmbpbi,
    LwU32       cmd,
    LwU32       data,
    LwU32       extData,
    LwU32       reqStatus
)
{
    appTaskCriticalEnter();
    {
        switch (SmbpbiResident.requestCnt)
        {
            case 0:
                // No request is in flight. Internal error.
                PMU_HALT();
                break;
            default:
                //
                // More than one request was submitted.
                // Protocol violation by the client.
                //
                cmd = FLD_SET_DRF(_MSGBOX, _CMD, _STATUS, _ERR_REQUEST, cmd);
                /* fall through */
            case 1:
                SmbpbiResident.requestCnt = 0;
                s_smbpbiSetResponse(cmd, data, extData, reqStatus);
                break;
        }
    }
    appTaskCriticalExit();
}

/*!
 * Processes and acknowledges all SMBPBI-related commands sent to the PMU
 * (either externally or internally).
 *
 * @param[in] pDispSmbpbi  Pointer to the DISPATCH_SMBPBI message to process
 */
FLCN_STATUS
smbpbiDispatch
(
    DISPATCH_SMBPBI *pDispSmbpbi
)
{
    LwU32 cmd = 0;
    LwU32 data = 0;
    LwU32 extData = 0;
    FLCN_STATUS status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;
    FLCN_STATUS reqStatus;

    switch (pDispSmbpbi->hdr.eventType)
    {
        case DISP2UNIT_EVT_RM_RPC:
        {
            status = pmuRpcProcessUnitSmbpbi(&(pDispSmbpbi->rpc));
            break;
        }
        case SMBPBI_EVT_MSGBOX_REQUEST:
        {
            // Retrieve pending command and optional input data.
            reqStatus = s_smbpbiGetRequest(&cmd, &data, &extData);
            if (reqStatus != FLCN_OK)
            {
                goto SMBPBI_EVT_MSGBOX_REQUEST_done;
            }

            //
            // When PMU_FSP_RPC is enabled, the cmd, data and extData values
            // are supplied via an RPC from FSP. So, the interrupt handler does
            // not have access to the value stored in the command register and
            // hence the check should be bypasssed.
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_FSP_RPC) ||
                (cmd == pDispSmbpbi->irq.msgBoxCmd))
            {
                //
                // SMBPBI server always posts LW_MSGBOX_CMD_STATUS_READY for
                // the first SMBPBI request. When servicing requests redirected
                // from FSP, we don't need to do that since this logic is
                // implemented on FSP side.
                //
                static LwBool   bDoOnce = !PMUCFG_FEATURE_ENABLED(PMU_FSP_RPC);

                // Two conselwtive reads are identical -> process cmd.
                if (bDoOnce)
                {
                    bDoOnce = LW_FALSE;
                    // Instruct the client to re-read the caps
                    status = LW_MSGBOX_CMD_STATUS_READY;
                    goto SMBPBI_EVT_MSGBOX_REQUEST_done;
                }

#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_BUNDLED_REQUESTS))
                //
                // Special case the Bundle Launch request
                // in order to avoid relwrsion.
                //
                if (FLD_TEST_DRF(_MSGBOX, _CMD, _ENCODING, _STANDARD, cmd) &&
                    FLD_TEST_DRF(_MSGBOX, _CMD, _OPCODE, _BUNDLE_LAUNCH, cmd))
                {
                    status = smbpbiBundleLaunch(&cmd, &data, &extData);
                }
                else
#endif // PMU_SMBPBI_BUNDLED_REQUESTS
                {
                    status = smbpbiHandleMsgboxRequest(&cmd, &data, &extData);
                }

            }
            else
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_REQUEST;
            }

SMBPBI_EVT_MSGBOX_REQUEST_done:
            //
            // Update status and send out response unless RM-side processing is
            // required. MORE_PROCESSING_REQUIRED indicates that the MSGBOX
            // request must be handled by the RM. Consequently, the RM is
            // responsible for performing the SET_RESPONSE.
            //
            if (status != LW_MSGBOX_CMD_ERR_MORE_PROCESSING_REQUIRED)
            {
                cmd = FLD_SET_DRF_NUM(_MSGBOX,_CMD,_STATUS, status, cmd);

                // Copy data[23:0] into cmd[23:0] if requested and status is good
                if (LW_MSGBOX_CMD_IS_COPY_DATA_SET(cmd) &&
                        (status == LW_MSGBOX_CMD_STATUS_SUCCESS))
                {
                    s_smbpbiHandleDataCopy(&cmd, data, extData);
                }

                s_smbpbiSetStatus(&Smbpbi, cmd, data, extData, reqStatus);
            }
            status = FLCN_OK;
            break;
        }
#if PMUCFG_FEATURE_ENABLED(PMU_SMBPBI)
        case SMBPBI_EVT_PWR_MONITOR_TGP_UPDATE:
        {
            // Cache the latest TGP update from the PWR_MONITOR
            Smbpbi.totalGpuPowermW = pDispSmbpbi->pwrMonTgpUpdate.totalGpuPowermW;
            Smbpbi.bTgpAvailable = LW_TRUE;
            status = FLCN_OK;
            break;
        }
#endif
    }

    return status;
}

/*!
 * @brief   Processes and acknowledges all MSGBOX_CFG RPC command.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_SMBPBI_MSGBOX_CFG
 */
FLCN_STATUS
pmuRpcSmbpbiMsgboxCfg
(
    RM_PMU_RPC_STRUCT_SMBPBI_MSGBOX_CFG *pParams
)
{
    LwBool      bEnable;
    LwU32       cmd;
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU8        status;

    // Copy in config data.
    Smbpbi.config = pParams->cfgData;

    // Smbpbi.config structure was updated.  Enable msgbox IRQ.
    bEnable = SMBPBI_CONFIG_IS_ENABLED(&Smbpbi.config, _ACTION);

    // Announce new task state on I2C as requested by RM.
    if (bEnable)
    {
        status = s_smbpbiEventSet(LW_MSGBOX_EVENT_TYPE_SERVER_RESTART);

        if (status == LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            // Unmask all events
            status = smbpbiRegWr(LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENT_MASK, 0);
        }

        flcnStatus = s_smbpbiMapRegOpStatus(status);

        if (flcnStatus != FLCN_OK)
        {
            bEnable = LW_FALSE;
            goto pmuRpcSmbpbiMsgboxCfg_exit;
        }

        // Adjust HAL-dependent thermal capabilities
        smbpbiSetThermCaps_HAL(&Smbpbi,
                        &Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_0]);

        // Adjust TGP capability
        s_smbpbiAdjustTgpCap(&Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_0]);

#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_BUNDLED_REQUESTS))
        LW_MSGBOX_CAP_SET_AVAILABLE(Smbpbi.config.cap, 4, _REQUEST_BUNDLING);
#endif

        if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_DRIVER_EVENT_MSG))
        {
            smbpbiInitDem(&Smbpbi);
        }
    }

pmuRpcSmbpbiMsgboxCfg_exit:
    cmd = bEnable ? DRF_DEF(_MSGBOX, _CMD, _STATUS, _READY) :
                    DRF_DEF(_MSGBOX, _CMD, _STATUS, _INACTIVE);

    smbpbiExelwte_HAL(&Smbpbi, &cmd, NULL, NULL, PMU_SMBPBI_SET_RESPONSE);

    if (PMUCFG_FEATURE_ENABLED(PMU_FSP_RPC))
    {
        if (bEnable)
        {
            flcnStatus = smbpbiFspRpcSendUcodeLoad();
        }
        else
        {
            flcnStatus = smbpbiFspRpcSendUcodeUnload();
        }
    }

    // when PMU_FSP_RPC is enabled, SMBPBI server is running on FSP.
    if (!PMUCFG_FEATURE_ENABLED(PMU_FSP_RPC))
    {
        smbpbiEnableInterrupt_HAL(&Smbpbi, bEnable);
    }

    return flcnStatus;
}

FLCN_STATUS
pmuRpcSmbpbiSetResponse
(
    RM_PMU_RPC_STRUCT_SMBPBI_SET_RESPONSE *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_ASYNC_REQUESTS) &&
        FLD_TEST_DRF(RM_PMU_SMBPBI, _COMMAND_FLAG, _ASYNC, _YES, pParams->flags))
    {
        if ((Smbpbi.asyncRequestState != PMU_SMBPBI_ASYNC_REQUEST_STATE_IN_FLIGHT) ||
            (Smbpbi.asyncRequestSeqNo != pParams->seqNo))
        {
            // Response received to a command we did not submit.
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto pmuRpcSmbpbiSetResponse_exit;
        }

        // Register the response
        Smbpbi.asyncRequestState     = PMU_SMBPBI_ASYNC_REQUEST_STATE_COMPLETED;
        Smbpbi.asyncRequestRegStatus = pParams->cmd;
        Smbpbi.asyncRequestRegData   = pParams->data;
        Smbpbi.asyncRequestRegExtData = pParams->extData;
    }
    else if (Smbpbi.bRmResponsePending)
    {
        if (SmbpbiResident.requestCnt != 0)
        {
            //
            // Don't do that if no request is in flight:
            // we don't own the status register
            //
            s_smbpbiSetStatus(&Smbpbi, pParams->cmd, pParams->data,
                              pParams->extData, REQ_STATUS_UNUSED);
        }

        Smbpbi.bRmResponsePending = LW_FALSE;
    }

pmuRpcSmbpbiSetResponse_exit:
    return status;
}

FLCN_STATUS
pmuRpcSmbpbiUpdatePblCounts
(
    RM_PMU_RPC_STRUCT_SMBPBI_UPDATE_PBL_COUNTS *pParams
)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_PG_RETIREMENT_STATS))
    {
        Smbpbi.retiredPagesCntSbe = pParams->sbeCount;
        Smbpbi.retiredPagesCntDbe = pParams->dbeCount;

        Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_1] =
            FLD_SET_DRF(_MSGBOX, _DATA_CAP_1, _RET_PG_CNT, _AVAILABLE,
                        Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_1]);
    }

    //
    //  THIS IS TEMPORARY, remove the following statement once VDVS test is fixed.
    //  There is a potential bug in the RM sending invalid command to SMBPBI task.
    //  Until that bug is triaged and fixed, let us not validate input.
    //

    return FLCN_OK;
}

FLCN_STATUS
pmuRpcSmbpbiUpdateRrlStats
(
    RM_PMU_RPC_STRUCT_SMBPBI_UPDATE_RRL_STATS *pParams
)
{
#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_RRL_STATS))
    Smbpbi.remappedRowsCntCor = pParams->corrCount;
    Smbpbi.remappedRowsCntUnc = pParams->uncorrCount;
    Smbpbi.bRemappingFailed   = pParams->bRemappingFailed;
    Smbpbi.bRemappingPending  = pParams->bRemappingPending;
    Smbpbi.rowRemapHistogram  = pParams->histogram;

    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_2, _REMAP_ROW_STATS, _AVAILABLE,
                    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_2, _REMAP_ROW_PENDING, _AVAILABLE,
                    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_2, _REMAP_ROW_HISTOGRAM, _AVAILABLE,
                    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2]);
#endif // PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_RRL_STATS)

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Handler for LW_MSGBOX_CMD_ENCODING_DEBUG command.
 * Reads the contents of the GPU BAR0 register, as specified in
 * ARG.
 *
 * @param[in]     cmd
 *     MSGBOX command, as received from the requestor.
 *
 * @param[in,out] pData
 *     Pointer to DATA dword supplied by requester.  Will populate this dword
 *     with the requested information.
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     Successfully serviced request.
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_OPCODE
 *     Unsupported debug request opcode.
 */
static LwU8
s_smbpbiDebugHandler
(
    LwU32   cmd,
    LwU32  *pData
)
{
    LwU8    status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    LwU32   offset;

    if (FLD_TEST_DRF(_MSGBOX, _DEBUG_CMD, _OPCODE, _READ_PRIV, cmd))
    {
        //
        // Offset is always a multiple of 4 (size of priv reg),
        // so 2 LSBits are not being transmitted.
        //
        offset = DRF_VAL(_MSGBOX, _DEBUG_CMD, _ARG, cmd) << 2;

        //
        // If it's a special debug mode, just read from the address,
        // otherwise lower privilege levels to disallow possible access
        // to priv registers.
        //
        if (FLD_TEST_DRF(_VSB, _REG0, _SMBPBI_DEBUG, _ENABLED, PmuVSBCache))
        {
            *pData = REG_RD32(BAR0, offset);
        }
        else
        {
            OS_TASK_SET_PRIV_LEVEL_BEGIN(RM_FALC_PRIV_LEVEL_CPU,
                                         RM_FALC_PRIV_LEVEL_CPU)
            {
                *pData = REG_RD32(BAR0, offset);
            }
            OS_TASK_SET_PRIV_LEVEL_END();
        }
    }
    else
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_OPCODE;
    }

    return status;
}

/*!
 * @brief This function process MSGBOX request
 *
 * @param[in]       cmd     requested command
 * @param[in,out]   pData   input/output data buffer
 *
 * @return      MSGBOX status
 */
LwU8
smbpbiHandleMsgboxRequest
(
    LwU32  *pCmd,
    LwU32  *pData,
    LwU32  *pExtData
)
{
    PMU_RM_RPC_STRUCT_SMBPBI_HANDLE_COMMAND rpc;

    LwU32   cmd = *pCmd;
    LwU8    opcode;
    LwU8    arg1;
    LwU8    arg2;
    LwU8    status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
    LwU8    type;

    DBG_PRINTF_SMBPBI(("MBs: c: %08x, d: %08x\n", cmd, *pData, 0, 0));

    switch (DRF_VAL(_MSGBOX, _CMD, _ENCODING, cmd))
    {
        case LW_MSGBOX_CMD_ENCODING_STANDARD:
        {
            // standard SMBPBI request
            break;
        }
        case LW_MSGBOX_CMD_ENCODING_DEBUG:
        {
            // debug SMBPBI request
            status = s_smbpbiDebugHandler(cmd, pData);
            goto done;
        }
        default:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_OPCODE;
            goto done;
        }
    }

    opcode = DRF_VAL(_MSGBOX, _CMD, _OPCODE, cmd);
    arg1 = DRF_VAL(_MSGBOX, _CMD, _ARG1, cmd);
    arg2 = DRF_VAL(_MSGBOX, _CMD, _ARG2, cmd);

    switch (opcode)
    {
        case LW_MSGBOX_CMD_OPCODE_NULL_CMD:
        {
            // NULL command always succeeds (for testing SMBPBI).
            status = LW_MSGBOX_CMD_STATUS_SUCCESS;
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_CAP_DWORD:
        {
            if (arg1 < LW_MSGBOX_DATA_CAP_COUNT)
            {
                *pData = Smbpbi.config.cap[arg1];
            }
            else
            {
                *pData = 0;
            }
            status = LW_MSGBOX_CMD_STATUS_SUCCESS;
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_TEMP:
        case LW_MSGBOX_CMD_OPCODE_GET_EXT_TEMP:
        {
            status = s_smbpbiTempGet(arg1,
                        opcode == LW_MSGBOX_CMD_OPCODE_GET_EXT_TEMP,
                        (LwTemp *)pData);
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_POWER:
        {
            status = s_smbpbiGetPower(arg1, pData);
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_SYS_ID_DATA:
        {
            type = arg1;

            if (type > LW_MSGBOX_CMD_ARG1_SYS_ID_DATA_TYPE_MAX)
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
                break;
            }

            status = s_smbpbiGetSysId(SysIdDataTypeAttr[type],
                                    arg2 * sizeof(*pData), pData);
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_ECC_V2:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_ECC_V3:
        {
            status = smbpbiEccV3Query_HAL(&Smbpbi, cmd, pData);
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_ECC_V4:
        {
            status = smbpbiEccV4Query_HAL(&Smbpbi, cmd, pData);
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_ECC_V5:
        {
            status = smbpbiEccV5Query_HAL(&Smbpbi, cmd, pData);
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_ECC_V6:
        {
            status = smbpbiEccV6Query_HAL(&Smbpbi, cmd, pData, pExtData);
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_SCRATCH_READ:
        case LW_MSGBOX_CMD_OPCODE_SCRATCH_WRITE:
        case LW_MSGBOX_CMD_OPCODE_SCRATCH_COPY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_ASYNC_REQUESTS))
            {
                status = smbpbiHostMemOp(cmd, pData);
            }
            else
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
            }
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_REGISTER_ACCESS:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_ASYNC_REQUESTS))
            {
                switch (arg1)
                {
                    case LW_MSGBOX_CMD_ARG1_REGISTER_ACCESS_WRITE:
                    {
                        status = s_smbpbiRegWrClient(arg2, *pData);
                        break;
                    }
                    case LW_MSGBOX_CMD_ARG1_REGISTER_ACCESS_READ:
                    {
                        status = smbpbiRegRd(arg2, pData);
                        break;
                    }
                    default:
                    {
                        status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
                        break;
                    }
                }
            }
            else
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
            }
            break;
        }
        //
        // Certain MSGBOX commands cannot be handled by the PMU (entirely or
        // partially) and must be redirected to the RM. Once redirected, the RM
        // is responsible for setting the response on the interface.
        //
        case LW_MSGBOX_CMD_OPCODE_GPU_PCONTROL:
        {
            status = s_smbpbiPControl(cmd, pData);
            if (status != LW_MSGBOX_CMD_ERR_MORE_PROCESSING_REQUIRED)
            {
                break;
            }
            // otherwise, fall-through and redirect the request to the RM
        }
        case LW_MSGBOX_CMD_OPCODE_ASYNC_REQUEST:
        {
            if (!PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_ASYNC_REQUESTS))
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                break;
            }
            else
            {
                // We fall through.
                // No "break"!
            }
        }
        case LW_MSGBOX_CMD_OPCODE_GPU_REQUEST_CPL:
        case LW_MSGBOX_CMD_OPCODE_SET_PRIMARY_CAPS:
        case LW_MSGBOX_CMD_OPCODE_GPU_SYSCONTROL:
        {
            rpc.cmd       = cmd;
            rpc.data      = *pData;
            rpc.flags     = 0;

            if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_ASYNC_REQUESTS) &&
                (opcode == LW_MSGBOX_CMD_OPCODE_ASYNC_REQUEST))
            {
                if (!SMBPBI_CONFIG_IS_ENABLED(&Smbpbi.config, _ASYNC_REQUESTS))
                {
                    status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                    break;
                }

                if (arg1 == LW_MSGBOX_CMD_ARG1_ASYNC_REQUEST_POLL)
                {
                    if (arg2 != Smbpbi.asyncRequestSeqNo)
                    {
                        // Never heard of this async request
                        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
                        break;
                    }

                    switch (Smbpbi.asyncRequestState)
                    {
                        case PMU_SMBPBI_ASYNC_REQUEST_STATE_IDLE:
                        {
                            // Haven't been issued yet
                            status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
                            break;
                        }
                        case PMU_SMBPBI_ASYNC_REQUEST_STATE_IN_FLIGHT:
                        {
                            // Still working
                            status = LW_MSGBOX_CMD_STATUS_ACCEPTED;
                            break;
                        }
                        case PMU_SMBPBI_ASYNC_REQUEST_STATE_COMPLETED:
                        {
                            // Report status from the RM
                            status = DRF_VAL(_MSGBOX, _CMD, _STATUS, Smbpbi.asyncRequestRegStatus);
                            *pData = Smbpbi.asyncRequestRegData;

                            // Bump the state machine
                            Smbpbi.asyncRequestState = PMU_SMBPBI_ASYNC_REQUEST_STATE_IDLE;
                            ++Smbpbi.asyncRequestSeqNo;
                            break;
                        }
                    }
                }
                else
                {
                    if (Smbpbi.asyncRequestState != PMU_SMBPBI_ASYNC_REQUEST_STATE_IDLE)
                    {
                        status = LW_MSGBOX_CMD_STATUS_ERR_BUSY;
                        *pData = Smbpbi.asyncRequestSeqNo;
                        break;
                    }

                    rpc.seqNo = Smbpbi.asyncRequestSeqNo;
                    rpc.flags = FLD_SET_DRF(RM_PMU_SMBPBI, _COMMAND_FLAG, _ASYNC,
                                            _YES, rpc.flags);
                    PMU_RM_RPC_EXELWTE_NON_BLOCKING(status, SMBPBI, HANDLE_COMMAND, &rpc);
                    if (status != FLCN_OK)
                    {
                        status = LW_MSGBOX_CMD_STATUS_ERR_AGAIN;
                        break;
                    }

                    *pData = Smbpbi.asyncRequestSeqNo;
                    Smbpbi.asyncRequestState = PMU_SMBPBI_ASYNC_REQUEST_STATE_IN_FLIGHT;
                    status = LW_MSGBOX_CMD_STATUS_ACCEPTED;
                }
            }
            else
            {
                Smbpbi.bRmResponsePending = LW_TRUE;
                // RC-TODO , properly handle status.
                PMU_RM_RPC_EXELWTE_BLOCKING(status, SMBPBI, HANDLE_COMMAND, &rpc);
                status = LW_MSGBOX_CMD_ERR_MORE_PROCESSING_REQUIRED;
            }

            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_POWER_CONNECTOR_STATE:
        {
            //
            // We know this, because if the external power conector was not
            // connected, we would have bailed out from the RM startup.
            //
            *pData = DRF_DEF(_MSGBOX, _DATA, _POWER_CONNECTED, _SUFFICIENT);
            status = LW_MSGBOX_CMD_STATUS_SUCCESS;
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_PAGE_RETIREMENT_STATS:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_PG_RETIREMENT_STATS) &&
                LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 1, _RET_PG_CNT))
            {
                *pData =
                    DRF_NUM(_MSGBOX, _DATA, _RETIRED_PAGES_CNT_SBE, Smbpbi.retiredPagesCntSbe) |
                    DRF_NUM(_MSGBOX, _DATA, _RETIRED_PAGES_CNT_DBE, Smbpbi.retiredPagesCntDbe);
                status = LW_MSGBOX_CMD_STATUS_SUCCESS;
            }
            else
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
            }
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_REMAP_ROW_STATS:
        {
            status = smbpbiGetRemapRowStats_HAL(&Smbpbi, pCmd, pData, pExtData);
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_THERM_PARAM:
        {
            status = s_smbpbiGetThermParam(arg1, pData);
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_ACCESS_WP_MODE:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
            if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_FW_WRITE_PROTECT_MODE))
            {
                status = s_smbpbiGetWriteProtectionMode(arg1, pData);
            }
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_MISC_GPU_FLAGS:
        {
            switch (arg1)
            {
                case LW_MSGBOX_CMD_ARG1_GET_MISC_GPU_FLAGS_PAGE_0:
                {
                    *pData = 0;
                    status = s_smbpbiGetEccEnableState(&Smbpbi.config, pData);
                    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
                    {
                        goto done;
                    }
                    status = s_smbpbiGetSmcEnableState(&Smbpbi, pData);
                    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
                    {
                        goto done;
                    }
                    break;
                }
                case LW_MSGBOX_CMD_ARG1_GET_MISC_GPU_FLAGS_PAGE_1:
                {
                    *pData = 0;
                    status = smbpbiGetGpuResetRequiredFlag_HAL(&Smbpbi, pData);

                    if (status == LW_MSGBOX_CMD_STATUS_SUCCESS)
                    {
                        status = smbpbiGetGpuDrainAndResetRecommendedFlag_HAL(&Smbpbi, pData);
                    }

                    break;
                }
                default:
                    status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
                    break;
            }
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GPU_UTIL_COUNTERS:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_ACLWMULATE_UTIL_COUNTERS))
            {
                switch (arg1)
                {
                    case LW_MSGBOX_CMD_ARG1_GPU_UTIL_COUNTERS_CONTEXT_TIME:
                        status = s_smbpbiGetUtilCounter(PMU_SMBPBI_UTIL_COUNTERS_CONTEXT, pData);
                        break;
                    case LW_MSGBOX_CMD_ARG1_GPU_UTIL_COUNTERS_SM_TIME:
                        status = s_smbpbiGetUtilCounter(PMU_SMBPBI_UTIL_COUNTERS_SM, pData);
                        break;
                    case LW_MSGBOX_CMD_ARG1_GPU_UTIL_COUNTERS_RESET_COUNTERS:
                        status = s_smbpbiResetUtilCountersHelper();
                        break;
                    default:
                        status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
                        break;
                }
            }
            else
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
            }
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_LWLINK_INFO:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_LWLINK_INFO))
            {
                status = s_smbpbiGetLwlinkInfoHandler(arg1, arg2, pCmd, pData, pExtData);
            }
            else
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
            }
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_CLOCK_FREQ_INFO:
        {
            if (arg1 == LW_MSGBOX_CMD_ARG1_GET_CLOCK_FREQ_INFO_PAGE_3)
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_CLOCK_LWRRENT_PSTATE))
                {
                    status = s_smbpbiGetLwrrentPstate(pData);
                }
                else
                {
                    status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                }
            }
            else if ((arg1 == LW_MSGBOX_CMD_ARG1_GET_CLOCK_FREQ_INFO_LWRRENT) ||
                     (arg1 == LW_MSGBOX_CMD_ARG1_GET_CLOCK_FREQ_INFO_MIN) ||
                     (arg1 == LW_MSGBOX_CMD_ARG1_GET_CLOCK_FREQ_INFO_MAX))
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_CLOCK_FREQ_INFO))
                {
                    status = s_smbpbiGetClockFreqInfo(arg1, arg2, pData);
                }
                else
                {
                    status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                }
            }
            else
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
            }
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_BUNDLE_LAUNCH:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_BUNDLED_REQUESTS))
            {
                //
                // Disallow launching a bundle from inside a bundle,
                // as that is the only way to hit this case.
                //
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
            }
            else
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
            }
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_PCIE_LINK_INFO:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_GET_PCIE_LINK_INFO))
            {
                status = s_smbpbiGetPcieLinkInfoHandler(arg1, arg2, pData, pExtData);
            }
            else
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
            }
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_DRIVER_EVENT_MSG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_DRIVER_EVENT_MSG))
            {
                status = smbpbiGetDem(arg1, arg2);
            }
            else
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
            }
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_ENERGY_COUNTER:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_CHANNEL_35))
            {
                status = s_smbpbiGetEnergyCounterHandler(pData, pExtData);
            }
            else
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
            }
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_POWER_HINT_INFO:
        {
#if PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X)
            status = s_smbpbiGetPowerHintInfoHandler(arg1, arg2, pData, pExtData);
#else
            status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
#endif
            break;
        }
        default:
        {
            // Unknown command
            status = LW_MSGBOX_CMD_STATUS_ERR_OPCODE;
            break;
        }
    }

done:
    DBG_PRINTF_SMBPBI(("MBe: c: %08x, d: %08x, e: %02x\n", cmd, *pData, status, 0));
    return status;
}

/*!
 * Colwert FLCN_STATUS into MSGBOX status.
 *
 * @param[in]  status     PMU status code
 *
 * @return corresponding MSGBOX status value
 */
static LwU8
s_smbpbiMapPmuStatusToMsgBoxStatus
(
    FLCN_STATUS status
)
{
    switch (status)
    {
        case FLCN_OK:
        {
            return LW_MSGBOX_CMD_STATUS_SUCCESS;
        }
        case FLCN_ERR_I2C_BUSY:
        case FLCN_ERR_I2C_NACK_ADDRESS:
        case FLCN_ERR_I2C_NACK_BYTE:
        case FLCN_ERR_I2C_SIZE:
        case FLCN_ERR_I2C_BUS_ILWALID:
        {
            return LW_MSGBOX_CMD_STATUS_ERR_I2C_ACCESS;
        }
        case FLCN_ERR_NOT_SUPPORTED:
        {
            return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        }
        default:
        {
            break;
        }
    }
    return LW_MSGBOX_CMD_STATUS_ERR_MISC;
}

/*!
 * @brief Internal state registers' logic that is in addition
 * to simple storage
 *
 * @param[in] reg
 *      register ID (index)
 * @param[in] data
 *      the value to be written
 *
 * @return whatever inferiors return
 */
static LwU8
s_smbpbiRegWrClient
(
    LwU8    reg,
    LwU32   data
)
{
    LwU32   pending;
    LwU8    status;

    switch (reg)
    {
        case LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENTS_PENDING:
        {
            status = smbpbiRegRd(LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENTS_PENDING, &pending);
            if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
            {
                return status;
            }

            pending &= data | ~smbpbiEventsEdgeTriggered;
            data = pending;
            break;
        }

        case LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENT_MASK:
        {
            data &= LW_MSGBOX_EVENT_TYPE__ALL;
            break;
        }
    }

    return smbpbiRegWr(reg, data);
}

/*!
 * @brief Map SMBPBI status returned by an internal state register operation
 *        to FLCN_STATUS.
 *
 * @param[in] status
 *      SMBPBI status
 *
 * @return translated status
 */
static FLCN_STATUS
s_smbpbiMapRegOpStatus
(
    LwU8    status
)
{
    LwU8    flcnStatus;

    switch (status)
    {
        case LW_MSGBOX_CMD_STATUS_SUCCESS:
        case LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED:
        {
            flcnStatus = FLCN_OK;
            break;
        }
        case LW_MSGBOX_CMD_STATUS_ERR_ARG1:
        case LW_MSGBOX_CMD_STATUS_ERR_ARG2:
        {
            flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
            break;
        }
        default:
        {
            flcnStatus = FLCN_ERR_DMA_GENERIC;
            break;
        }
    }
    return flcnStatus;
}

/*!
 * @brief Set an edge triggered SMBPBI event
 *
 * @param[in] event
 *      SMBPBI event type
 *
 * @return whatever inferiors return
 */
static LwU8
s_smbpbiEventSet
(
    LwMsgboxEventType   event
)
{
    LwU32   pending;
    LwU8    status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2, _NUM_SCRATCH_BANKS))
    {
        goto s_smbpbiEventSet_exit;
    }
    if ((status = smbpbiRegRd(LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENTS_PENDING, &pending))
                                    != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        PMU_TRUE_BP();
        goto s_smbpbiEventSet_exit;
    }

    if ((pending & BIT(event)) != 0)
    {
        goto s_smbpbiEventSet_exit;
    }

    pending |= BIT(event);

    if ((status = smbpbiRegWr(LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENTS_PENDING,
                    pending)) != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        PMU_TRUE_BP();
        goto s_smbpbiEventSet_exit;
    }

s_smbpbiEventSet_exit:
    return status;
}

/*!
 * @brief Query event sources and fill the event mask
 *
 * @param[in/out]  pStatus  STATUS register value
 */
void
smbpbiCheckEvents
(
    LwU32   *pStatus
)
{
    unsigned    event;
    LwU32       pending;
    LwU32       mask;
    LwU32       pendingNew;
    unsigned    eventBit;
    LwU8        status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (FLD_TEST_DRF(_MSGBOX, _CMD, _STATUS, _INACTIVE, *pStatus) ||
        !LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2, _NUM_SCRATCH_BANKS))
    {
        //
        // Don't scan/post events unless the server is open for business
        // or the internal state registers are unavailable.
        //
        goto smbpbiCheckEvents_exit;
    }

    status = smbpbiRegRd(LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENTS_PENDING, &pending);

    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        PMU_TRUE_BP();
        goto smbpbiCheckEvents_exit;
    }

    status = smbpbiRegRd(LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENT_MASK, &mask);

    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        PMU_TRUE_BP();
        goto smbpbiCheckEvents_exit;
    }

    pendingNew = pending & smbpbiEventsEdgeTriggered;

    for (event = 0; event < (unsigned)LW_MSGBOX_NUM_EVENTS; ++event)
    {
        if ((BIT(event) & smbpbiEventsEdgeTriggered) == 0)
        {
            // Level triggered
            LwBool  bPending;

            switch (event)
            {
                case LW_MSGBOX_EVENT_TYPE_GPU_RESET_REQUIRED:
                {
                    bPending = s_smbpbiEventSourceResetRequired();
                    break;
                }
                case LW_MSGBOX_EVENT_TYPE_DRIVER_ERROR_MESSAGE:
                {
                    if PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_DRIVER_EVENT_MSG)
                    {
                        bPending = smbpbiDemEventSource();
                        break;
                    }
                    // else fall through to default:
                }
                default:
                {
                    bPending = LW_FALSE;
                    break;
                }
            }
            if (bPending)
            {
                pendingNew |= BIT(event);
            }
        }
    }

    if (pendingNew != pending)
    {
        if ((status = smbpbiRegWr(LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENTS_PENDING,
                        pendingNew)) != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            PMU_TRUE_BP();
            goto smbpbiCheckEvents_exit;
        }
    }

    if (SMBPBI_CONFIG_IS_ENABLED(&Smbpbi.config, _EVENT_POSTING))
    {
        eventBit = (pendingNew & ~mask) != 0;
        *pStatus = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _EVENT_PENDING, eventBit, *pStatus);
    }

smbpbiCheckEvents_exit:
    return;
}

static LwBool   s_smbpbiEventSourceResetRequired(void)
{
    LwU32   data;

    if (smbpbiGetGpuResetRequiredFlag_HAL(&Smbpbi, &data)
        != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        return LW_FALSE;
    }

    return FLD_TEST_DRF(_MSGBOX, _DATA, _GPU_RESET_REQUIRED, _ON, data);
}

/*!
 * Read requested temperature, given the channel index and
 * the high precision flag.
 *
 * @param[in]   sensor          SMBPBI sensor ID (arg1)
 * @param[in]   bHiPrecision    high precision flag
 * @param[out]  pTemp           pointer to the value
 *
 * @return      status          SMBPBI status
 */
static LwU8
s_smbpbiTempGet
(
    LwU8    sensor,
    LwBool  bHiPrecision,
    LwTemp  *pTemp
)
{
    THERM_CHANNEL  *pChannel;
    FLCN_STATUS     status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_LIB_THERM_CHANNEL
    };
    LwU8            ret;

    if (sensor >= LW_MSGBOX_CMD_ARG1_TEMP_NUM_SENSORS)
    {
        ret = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
        goto s_smbpbiTempGet_exit;
    }

    if (((DRF_DEF(_MSGBOX, _DATA_CAP_0, _TEMP_GPU_0, _AVAILABLE) << sensor) &
          Smbpbi.config.cap[0]) == 0)
    {
        ret = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        goto s_smbpbiTempGet_exit;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        pChannel = THERM_CHANNEL_GET(Smbpbi.config.thermSensorMap[sensor]);
        if (pChannel == NULL)
        {
            PMU_BREAKPOINT();
            ret = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
            goto s_smbpbiTempGet_detach;
        }
        status = thermChannelTempValueGet(pChannel, pTemp);
        ret = s_smbpbiMapPmuStatusToMsgBoxStatus(status);

        if ((status == FLCN_OK) && !bHiPrecision)
        {
            *pTemp = RM_PMU_CELSIUS_TO_LW_TEMP(
                RM_PMU_LW_TEMP_TO_CELSIUS_ROUNDED(*pTemp)); // clear the fraction
        }

s_smbpbiTempGet_detach:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

s_smbpbiTempGet_exit:
    return ret;
}

/*!
 * Returns power consumption by the power policy index
 *
 * @param[in]     idx
       Power policy index
 * @param[in,out] pData
 *     Pointer to DATA dword supplied by requester.  Will populate this dword
 *     with the requested value in mW.
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     Successfully serviced request.
 * @return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE
 *     Still initializing, try later.
 * @return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED
 *     PWR_POLICY not configured
 */
static LwU8
s_smbpbiPwrPolicyIdxQuery
(
    LwU8    idx,
    LwU32   *pData
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY)
    PWR_POLICY *pPolicy = PMGR_GET_PWR_POLICY_IDX(idx);
    LwU8        status  = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (pPolicy != NULL)
    {
        *pData = pwrPolicyValueLwrrGet(pPolicy);
    }
    else
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
    }
#else
    LwU8 status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
#endif

    return status;
}

/*
 * Adjust TGP query capability depending on the PMGR configuration
 *
 * @param[out] pCap
 *     Pointer to the capability word
 *
 * @return void
 */
static void
s_smbpbiAdjustTgpCap(LwU32 *pCap)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY)
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgr)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if (!PMU_PMGR_PWR_MONITOR_SUPPORTED())
        {
            if (PMGR_PWR_POLICY_IDX_IS_ENABLED(LW2080_CTRL_PMGR_PWR_POLICY_IDX_TGP))
            {
                *pCap = FLD_SET_DRF(_MSGBOX, _DATA_CAP_0, _POWER_TOTAL, _AVAILABLE, *pCap);
            }
            else
            {
                *pCap = FLD_SET_DRF(_MSGBOX, _DATA_CAP_0, _POWER_TOTAL, _NOT_AVAILABLE, *pCap);
            }
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
#endif
}

/*!
 * @brief Helper function to get instantaneous TGP
 *
 * @param[out]   pData  Pointer to power reading
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS on success.
 *
 */
static LwU8
s_smbpbiGetInstantaneousPower
(
    LwU32 *pData
)
{
    LwU8 status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (PMU_PMGR_PWR_MONITOR_SUPPORTED())
    {
        status = s_smbpbiPwrMonitorQueryTotalGpuPower(pData);
    }
    else
    {
        status = s_smbpbiPwrPolicyIdxQuery(LW2080_CTRL_PMGR_PWR_POLICY_IDX_TGP,
                                          pData);
    }

    return status;
}

/*!
 * @brief Helper function to get average TGP in last second.
 *
 * @param[out]   pData  Pointer to the computed value
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS on success.
 *
 */
static LwU8
s_smbpbiGetAveragePower
(
    LwU32 *pData
)
{
    LwU8 status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    *pData = pmgrPwrChannelsPmumonGetAverageTotalGpuPowerUsage();

    if ((*pData) == 0)
    {
        // No samples available, return instantaneous value.
        status = s_smbpbiGetInstantaneousPower(pData);
    }

    return status;
}

/*!
 * Handler for LW_MSGBOX_CMD_OPCODE_GET_POWER SMBPBI request
 *
 * @param[in,out] pData
 *     Pointer to DATA dword supplied by requester.  Will populate this dword
 *     with the requested value in mW.
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     Successfully serviced request.
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG1
 *     Incorrect source requested.
 *
 * @return others
 *     As returned by the inferior functions.
 */
static LwU8
s_smbpbiGetPower
(
    LwU8    source,
    LwU32   *pData
)
{
    LwBool  bOvlsAttached   = LW_FALSE;
    LwU8    status          = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgr)
    };

    switch (source)
    {
        case LW_MSGBOX_CMD_ARG1_POWER_TOTAL:
        {
            if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 0, _POWER_TOTAL))
            {
                break;
            }

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList); // NJ??
            ATTACH_AND_LOAD_OVERLAY_PWR_POLICY_CLIENT_LOCKED;
            bOvlsAttached = LW_TRUE;

            if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_CHANNELS_PMUMON))
            {
                status = s_smbpbiGetAveragePower(pData);
            }
            else
            {
                status = s_smbpbiGetInstantaneousPower(pData);
            }

            break;
        }
        case LW_MSGBOX_CMD_ARG1_SMBPBI_POWER:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_SMBPBI_PWR_POLICY_INDEX_ENABLED);

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList); // NJ??
            ATTACH_AND_LOAD_OVERLAY_PWR_POLICY_CLIENT_LOCKED;
            bOvlsAttached = LW_TRUE;

            status = s_smbpbiPwrPolicyIdxQuery(LW2080_CTRL_PMGR_PWR_POLICY_IDX_SMBUS,
                                              pData);

            break;
        }
        default:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
            break;
        }
    }

    if (bOvlsAttached)
    {
        DETACH_OVERLAY_PWR_POLICY_CLIENT_LOCKED;
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    return status;
}

/*!
 * Handler for LW_MSGBOX_CMD_OPCODE_GET_POWER:LW_MSGBOX_CMD_ARG1_POWER_TOTAL
 * MSGBOX request. Reads total GPU power.
 *
 * @param[in,out] pData
 *     Pointer to DATA dword supplied by requester.  Will populate this dword
 *     with total GPU power in mW.
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     Successfully serviced request.
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED
 *     Current PMU configuration does not support this request.
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE
 *     Still initializing, try later.
 */
static LwU8
s_smbpbiPwrMonitorQueryTotalGpuPower
(
    LwU32 *pData
)
{
    // check if the capability is present
    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 0, _POWER_TOTAL))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
    }
    //
    // If no cached values, then PWR_MONITOR is not enabled (possibly not yet)
    // and this request cannot be satisfied at the moment
    //
    if (!Smbpbi.bTgpAvailable)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
    }

    *pData = Smbpbi.totalGpuPowermW;

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*!
 * Handler for the LW_MSGBOX_CMD_OPCODE_GPU_PCONTROL MSGBOX request.
 * Responsible for looking at the PCONTROL request and either servicing it
 * entirely, or handing it off/redirecting it to the RM (some PCONTROL actions
 * are not possible to handle without help from the RM).
 *
 * @param[in,out] pData
 *     Pointer to DATA dword supplied by requester. Populated with the output
 *     (if any) of the PCONTROL request when the PMU is able to process it.
 *     Ignored when the request is redirected to the RM.  See the return status
 *     value to disambiguate.
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     Successfully serviced request. No RM action required. 'pData' is valid.
 *
 * @return LW_MSGBOX_CMD_ERR_ARG2
 *     If a non-zero value was provided in arg2. Arg2 is reserved so it is
 *     required that the client pass in zero for its value.
 *
 * @return LW_MSGBOX_CMD_ERR_NOT_AVAILABLE
 *     Returned if the client has request vPstate status before it is known to
 *     the PMU (ie. before the RM has informed the PMU of the current vPstate).
 *
 * @return LW_MSGBOX_CMD_ERR_MORE_PROCESSING_REQUIRED
 *     PMU cannot process the request. RM assistence is required.
 */
static LwU8
s_smbpbiPControl
(
    LwU32  cmd,
    LwU32 *pData
)
{
    FLCN_TIMESTAMP          timeLwrrent;
    LwU64                   diffTime;
    static FLCN_TIMESTAMP   SmbpbiPcontrolTimeLastUpdate = {0};
    static LwU8             SmbpbiPcontrolVPstateLwrrIdx =
        PERF_VPSTATE_ILWALID_IDX;

    LwU8 arg1   = LW_MSGBOX_GET_CMD_ARG1(cmd);
    LwU8 arg2   = LW_MSGBOX_GET_CMD_ARG2(cmd);
    LwU8 action = (LwU8)LW_MSGBOX_CMD_GPU_PCONTROL_ARG1_GET_ACTION(arg1);
    LwU8 target = (LwU8)LW_MSGBOX_CMD_GPU_PCONTROL_ARG1_GET_TARGET(arg1);

    //
    // SMBPBI spec requires the primary to set all reserved bits to zero. Not
    // doing so should result in failure.
    //
    if (arg2 != 0)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    //
    // GET_STATUS for target=VPSTATE can be handled entirely without RM
    // assistance. All other PCONTROL sub-commands and target combinations
    // need redirected to the RM.
    //
    if (target == LW_MSGBOX_CMD_GPU_PCONTROL_ARG1_TARGET_VPSTATE)
    {
        switch (action)
        {
            case LW_MSGBOX_CMD_GPU_PCONTROL_ARG1_ACTION_GET_STATUS:
            {
                //
                // Get the current time and check it to see if it falls within
                // the filter period (relative to the last GET_STATUS request).
                // If it does, return the same status that was returned in the
                // previous request. Otherwise, provide the current status and
                // restart the filter period. The filter period is implemented
                // as a way to deter the primary from thrashing too heavily on
                // the SMBPBI interface.
                //
                osPTimerTimeNsLwrrentGet(&timeLwrrent);
                diffTime = timeLwrrent.data - SmbpbiPcontrolTimeLastUpdate.data;
                if (diffTime >= SMBPBI_PCONTROL_GET_STATUS_FILTER_PERIOD)
                {
                    SmbpbiPcontrolTimeLastUpdate = timeLwrrent;
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_2X)
                    SmbpbiPcontrolVPstateLwrrIdx = Perf.status.vPstateLwrrIdx;
#endif
                }

                if (SmbpbiPcontrolVPstateLwrrIdx == PERF_VPSTATE_ILWALID_IDX)
                {
                    return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
                }

                LW_MSGBOX_CMD_GPU_PCONTROL_DATA_VPSTATE_GET_STATUS_SET_LWRRENT(
                    *pData, SmbpbiPcontrolVPstateLwrrIdx);
                return LW_MSGBOX_CMD_STATUS_SUCCESS;
            }
            case LW_MSGBOX_CMD_GPU_PCONTROL_ARG1_ACTION_SET_LIMIT:
            {
                //
                // Ensure that the client receives a fresh status value if they
                // query for it immediately after setting a limit (which is
                // likely).
                //
                SmbpbiPcontrolTimeLastUpdate.data = 0;
                //
                // Do not return here. Break out and allow the request to be
                // redirected to the RM.
                //
                break;
            }
        }
    }
    return LW_MSGBOX_CMD_ERR_MORE_PROCESSING_REQUIRED;
}

/*!
 * @brief Handles DMA read from SMBPBI host side buffers
 *
 * @param[in]   offset
 *      byte offset into the surface
 * @param[in]   size
 *      transaction size
 * @param[in, out] pDst
 *      destination buffer pointer
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *      read successfully
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *      DMA malfunction
 */
LwU8
smbpbiHostMemRead(LwU32 offset, LwU32 size, LwU8 *pDst)
{
    LwU8       dmaBufSpace[SMBPBI_IFR_XFER_SIZE + DMA_MIN_READ_ALIGNMENT - 1];
    LwU8       *dmaBuf = PMU_ALIGN_UP_PTR(dmaBufSpace, DMA_MIN_READ_ALIGNMENT);
    LwU8       retVal  = LW_MSGBOX_CMD_STATUS_SUCCESS;
    LwU8       lwrSz;
    LwU8       off;
    LwU32      base;
    LwU32      limit;

    limit = DRF_VAL(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                                  Smbpbi.config.memdescInforom.params);
    if ((offset > limit) || (size > limit) || ((offset + size) > limit))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_MISC;
    }

    // Its recommended to use DMA section when task is doing back to back DMA.
    lwosDmaLockAcquire();
    {
        off  = offset % DMA_MIN_READ_ALIGNMENT;
        base = offset - off;

        while (size > 0)
        {
            lwrSz = LW_MIN(size, SMBPBI_IFR_XFER_SIZE - off);

            if (dmaRead(dmaBuf, &PMU_SMBPBI_HOST_MEM_SURFACE(), base, SMBPBI_IFR_XFER_SIZE) == FLCN_OK)

            {
                memcpy(pDst, dmaBuf + off, lwrSz);
                pDst += lwrSz;
                base += SMBPBI_IFR_XFER_SIZE;
                size -= lwrSz;
                off = 0;
            }
            else
            {
                retVal = LW_MSGBOX_CMD_STATUS_ERR_MISC;
                break;
            }
        }
    }
    lwosDmaLockRelease();

    return retVal;
}

static LwU8
s_smbpbiGetThermParam
(
    LwU8 paramId,
    LwU32 *pData
)
{
    LwU8        status;
    FLCN_STATUS pmuStatus;
    LwTemp      param;

    if (paramId < DRF_SIZE(LW_MSGBOX_DATA_CAP_0_THERMP_BITS))
    {
        if ((BIT(paramId) & DRF_VAL(_MSGBOX_DATA_CAP_0, _THERMP, _BITS,
                                   Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_0])) != 0)
        {
            switch (paramId)
            {
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY)
                case LW_MSGBOX_CMD_ARG1_THERM_PARAM_TEMP_TARGET:
                {
                    THERM_POLICY *pThermPolicy =
                                THERM_POLICY_GET(Smbpbi.config.acousticPolicyIdx);

                    if (pThermPolicy == NULL)
                    {
                        PMU_BREAKPOINT();
                        pmuStatus = FLCN_ERR_ILWALID_INDEX;
                        break;
                    }

                    param = THERM_POLICY_LIMIT_GET(pThermPolicy);
                    pmuStatus = FLCN_OK;
                    break;
                }
                case LW_MSGBOX_CMD_ARG1_THERM_PARAM_TEMP_HBM_SLOWDN:
                {
                    THERM_POLICY *pThermPolicy =
                                THERM_POLICY_GET(Smbpbi.config.memThermPolicyIdx);

                    if (pThermPolicy == NULL)
                    {
                        PMU_BREAKPOINT();
                        pmuStatus = FLCN_ERR_ILWALID_INDEX;
                        break;
                    }

                    param = THERM_POLICY_LIMIT_GET(pThermPolicy);
                    pmuStatus = FLCN_OK;
                    break;
                }
                case LW_MSGBOX_CMD_ARG1_THERM_PARAM_TEMP_SW_SLOWDN:
                {
                    THERM_POLICY *pThermPolicy =
                                THERM_POLICY_GET(Smbpbi.config.gpuSwSlowdownPolicyIdx);

                    if (pThermPolicy == NULL)
                    {
                        PMU_BREAKPOINT();
                        pmuStatus = FLCN_ERR_ILWALID_INDEX;
                        break;
                    }

                    param = THERM_POLICY_LIMIT_GET(pThermPolicy);
                    pmuStatus = FLCN_OK;
                    break;
                }
#endif
                case LW_MSGBOX_CMD_ARG1_THERM_PARAM_TEMP_HW_SLOWDN:
                {
                    pmuStatus = smbpbiGetLimitSlowdn_HAL(&Smbpbi, &param);
                    break;
                }
                case LW_MSGBOX_CMD_ARG1_THERM_PARAM_TEMP_SHUTDN:
                {
                    pmuStatus = smbpbiGetLimitShutdn_HAL(&Smbpbi, &param);
                    break;
                }
                default:
                {
                    pmuStatus = FLCN_ERR_NOT_SUPPORTED;
                    break;
                }
            }

            status = s_smbpbiMapPmuStatusToMsgBoxStatus(pmuStatus);
            if (LW_MSGBOX_CMD_STATUS_SUCCESS == status)
            {
                *pData = RM_PMU_LW_TEMP_TO_CELSIUS_TRUNCED(param);
            }
        }
        else
        {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        }
    }
    else
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
    }

    return status;
}

static LwU8
s_smbpbiGetWriteProtectionMode
(
    LwU8   arg1,
    LwU32 *pData
)
{
    LwU8    status;
    LwBool  bWpEnabled;

    //
    // RM/PMU SMBPBI server only supports the GET action. Switch
    // on all valid values to detect if the reserved bits in
    // arg1[7:1] were zeroed by the SMBPBI primary correctly.
    //
    switch (arg1)
    {
        case LW_MSGBOX_CMD_ARG1_ACCESS_WP_MODE_ACTION_GET:
            smbpbiGetFwWriteProtMode_HAL(&Smbpbi, &bWpEnabled);
            *pData = bWpEnabled ? LW_MSGBOX_DATA_ACCESS_WP_MODE_GET_STATE_ENABLED:
                                  LW_MSGBOX_DATA_ACCESS_WP_MODE_GET_STATE_DISABLED;
            status = LW_MSGBOX_CMD_STATUS_SUCCESS;
            break;

        case LW_MSGBOX_CMD_ARG1_ACCESS_WP_MODE_ACTION_SET:
            return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;

        default:
            return LW_MSGBOX_CMD_STATUS_ERR_ARG1;
    }

    return status;
}

static LwU8
s_smbpbiGetSysId
(
    LwU32 attr,
    LwU16 offset,
    LwU32 *pData
)
{
    LwU32   cnt;
    LwU16   orig;
    LwU16   size;
    LwU8    srctype;
    LwU8    status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    int     i;
    union
    {
        inforom_U032    u32[4];
        inforom_U008    u8[4];
    }       buffer;

    if ((BIT(DRF_VAL(_SMBPBI, _SYS_ID, _CAPBIT, attr)) &
        Smbpbi.config.cap[DRF_VAL(_SMBPBI, _SYS_ID, _CAPINDEX, attr)]) == 0)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        goto exits_smbpbiGetSysId;
    }

    srctype = DRF_VAL(_SMBPBI, _SYS_ID, _SRCTYPE, attr);
    orig = DRF_VAL(_SMBPBI, _SYS_ID, _OFFSET, attr);
    size = DRF_VAL(_SMBPBI, _SYS_ID, _SIZE, attr);

    if (offset >= size)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto exits_smbpbiGetSysId;
    }

    cnt = size - offset;
    if (cnt > sizeof(*pData))
    {
        cnt = sizeof(*pData);
    }
    else
    {
        *pData = 0;
    }

    if (srctype == LW_SMBPBI_SYS_ID_SRCTYPE_DMEM)
    {
        memcpy(pData,
               (LwU8 *)&Smbpbi.config.sysInforomData + orig + offset, cnt);
        goto exits_smbpbiGetSysId;
    }

    if (srctype == LW_SMBPBI_SYS_ID_SRCTYPE_IFR_STRING)
    {
        offset *= sizeof(inforom_U008);
    }

    status = smbpbiHostMemRead_INFOROM_GF100(orig + offset,
                                        sizeof(buffer), (LwU8 *)&buffer);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto exits_smbpbiGetSysId;
    }

    switch (srctype)
    {
        case LW_SMBPBI_SYS_ID_SRCTYPE_IFR_U8:
            *pData = buffer.u8[0] & 0xff;
            break;

        case LW_SMBPBI_SYS_ID_SRCTYPE_IFR_U32:
            *pData = buffer.u32[0];
            break;

        case LW_SMBPBI_SYS_ID_SRCTYPE_IFR_STRING:
            for (i = 0; i < cnt; ++i)
            {
                ((LwU8 *)pData)[i] = buffer.u8[i];
            }
            break;
    }

exits_smbpbiGetSysId:
    return status;
}

/*!
 * @brief Get ECC Enable States
 *
 * @param[in]  pSmbpbi MSGBOX config data
 * @param[out] pData   Pointer to data
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *              if success
 *              and ERR_NOT_SUPPORTED since use first bit to
 *              inform supported or not
 *
 * @return other errors
 */
static LwU8
s_smbpbiGetEccEnableState
(
    RM_PMU_MSGBOX_CONFIG *pConfig,
    LwU32 *pData
)
{
    LwBool  bEenEnabled;
    LwU8    status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(pConfig->cap, 1, _GET_ECC_ENABLED_STATE))
    {
        goto smbpbiGetEccEnable_exit;
    }

    if (!SMBPBI_CONFIG_IS_ENABLED(pConfig, _ECC_TOGGLE))
    {
        goto smbpbiGetEccEnable_exit;
    }

    *pData = FLD_SET_DRF(_MSGBOX, _DATA, _ECC_ENABLED_STATE_SUPPORTED, _ON, *pData);

    if (SMBPBI_CONFIG_IS_ENABLED(pConfig, _ECC))
    {
        *pData = FLD_SET_DRF(_MSGBOX, _DATA, _ECC_ENABLED_STATE_LWRRENT, _ON, *pData);
    }

    status = smbpbiHostMemRead_INFOROM_GK104(
                LW_OFFSETOF(INFOROMHALINFO_GK104, EEN.bEccEnabled), sizeof(bEenEnabled),
                &bEenEnabled);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetEccEnable_exit;
    }

    if (bEenEnabled)
    {
        *pData = FLD_SET_DRF(_MSGBOX, _DATA, _ECC_ENABLED_STATE_PENDING, _ON, *pData);
    }

smbpbiGetEccEnable_exit:
    return status;
}

/*!
 * @brief Get Smc Enable States
 *
 * @param[in]  pSmbpbi Pointer to SMBPBI
 * @param[out] pData   Pointer to data
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *              if success
 *              and ERR_NOT_SUPPORTED since use first bit to
 *              inform supported or not
 *
 * @return other errors
 */
static LwU8
s_smbpbiGetSmcEnableState
(
    OBJSMBPBI *pSmbpbi,
    LwU32     *pData
)
{
    LwBool  bSmcToggleSupported = LW_FALSE;
    LwBool  bSmcEnabled         = LW_FALSE;
    LwBool  bSenEnabled         = LW_FALSE;
    LwU8    status              = LW_MSGBOX_CMD_STATUS_SUCCESS;


    if (!LW_MSGBOX_CAP_IS_AVAILABLE(pSmbpbi->config.cap, 1, _GET_MIG_ENABLED_STATE))
    {
        goto s_smbpbiGetSmcEnableState_exit;
    }

    smbpbiGetSmcStates_HAL(pSmbpbi,
                           &bSmcToggleSupported,
                           &bSmcEnabled,
                           &bSenEnabled);

    if (!bSmcToggleSupported)
    {
        goto s_smbpbiGetSmcEnableState_exit;
    }

    *pData = FLD_SET_DRF(_MSGBOX, _DATA, _MIG_ENABLED_STATE_SUPPORTED, _ON, *pData);

    if (bSmcEnabled)
    {
        *pData = FLD_SET_DRF(_MSGBOX, _DATA, _MIG_ENABLED_STATE_LWRRENT, _ON, *pData);
    }

    if (bSenEnabled)
    {
        *pData = FLD_SET_DRF(_MSGBOX, _DATA, _MIG_ENABLED_STATE_PENDING, _ON, *pData);
    }

s_smbpbiGetSmcEnableState_exit:
    return status;
}

/*!
 * Enable/Disable Gpu Utilization accounting
 */
FLCN_STATUS
pmuRpcSmbpbiEnableUtilAcc
(
    RM_PMU_RPC_STRUCT_SMBPBI_ENABLE_UTIL_ACC *pParams
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

#if PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_ACLWMULATE_UTIL_COUNTERS)
    appTaskCriticalEnter();

    Smbpbi.bEnableUtilAcc = pParams->bEnableUtilAcc;
    status = FLCN_OK;

    appTaskCriticalExit();
#endif //PMU_PERFMON_ACLWMULATE_UTIL_COUNTERS

    return status;
}

/*!
 * @brief   Helper call to accumulate the Context/SM utilization time
 */
FLCN_STATUS
smbpbiAclwtilCounterHelper
(
    LwU32 delayUs,
    RM_PMU_GPUMON_PERFMON_SAMPLE *pSample
)
{
    LwU8  status = FLCN_OK;
    LwU32 smUtilTimeUs;

    if (pSample == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    appTaskCriticalEnter();

    if (PMU_SMBPBI_UTIL_ACC_IS_ENABLED())
    {
        // callwlate SM usage time as a percentage of util and time
        smUtilTimeUs = (pSample->gr.util * delayUs) / 10000;

        status = smbpbiAclwtilCounter(delayUs, PMU_SMBPBI_UTIL_COUNTERS_CONTEXT);
        if (status != FLCN_OK)
        {
            goto _smbpbiAclwtilCounterHelper_EXIT;
        }

        status = smbpbiAclwtilCounter(smUtilTimeUs, PMU_SMBPBI_UTIL_COUNTERS_SM);
        if (status != FLCN_OK)
        {
            goto _smbpbiAclwtilCounterHelper_EXIT;
        }
    }

_smbpbiAclwtilCounterHelper_EXIT:
    appTaskCriticalExit();

    return status;
}

/*!
 * @brief   Call to accumulate the time, in ms, in the specified utilization register
 */
FLCN_STATUS
smbpbiAclwtilCounter
(
    LwU32 delayUs,
    LwU32 utilCounter
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       delayMs;
    LwU32       offset;
    LwU32       aclwmulatedTime;

    status = smbpbiGetUtilCounterOffset_HAL(&Smbpbi, utilCounter, &offset);
    if (status != FLCN_OK)
    {
        return status;
    }

    delayMs = delayUs / 1000;

    aclwmulatedTime = REG_RD32(BAR0, offset);
    REG_WR32(BAR0, offset, aclwmulatedTime + delayMs);

    return status;
}

/*!
 * @brief   Get the Gpu utilization counter value
 */
static LwU8
s_smbpbiGetUtilCounter
(
    LwU32 counter,
    LwU32 *pData
)
{
    LwU8        status;
    FLCN_STATUS pmuStatus;
    LwU32       offset = 0;

    if (!PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_ACLWMULATE_UTIL_COUNTERS) ||
        !LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 1, _GPU_UTIL_COUNTERS))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
    }

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libEngUtil)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        pmuStatus = smbpbiGetUtilCounterOffset_HAL(&Pmu, counter, &offset);
        status = s_smbpbiMapPmuStatusToMsgBoxStatus(pmuStatus);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto s_smbpbiGetUtilCounter_exit;
        }

        *pData = REG_RD32(BAR0, offset);

s_smbpbiGetUtilCounter_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief   Helper to reset the Gpu utilization counters
 */
static LwU8
s_smbpbiResetUtilCountersHelper(void)
{
    LwU8 status;

    if (!PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_ACLWMULATE_UTIL_COUNTERS) ||
        !LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 1, _GPU_UTIL_COUNTERS))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
    }

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libEngUtil)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        appTaskCriticalEnter();

        status = s_smbpbiResetUtilCounter(PMU_SMBPBI_UTIL_COUNTERS_CONTEXT);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto s_smbpbiResetUtilCountersHelper_exit;
        }

        status = s_smbpbiResetUtilCounter(PMU_SMBPBI_UTIL_COUNTERS_SM);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto s_smbpbiResetUtilCountersHelper_exit;
        }

s_smbpbiResetUtilCountersHelper_exit:
        appTaskCriticalExit();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief   Reset Gpu utilization counters
 */
static LwU8
s_smbpbiResetUtilCounter
(
    LwU32 counter
)
{
    LwU8        status;
    FLCN_STATUS pmuStatus;
    LwU32       offset = 0;

    pmuStatus = smbpbiGetUtilCounterOffset_HAL(&Pmu, counter, &offset);
    status = s_smbpbiMapPmuStatusToMsgBoxStatus(pmuStatus);
    if (status == LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        REG_WR32(BAR0, offset, 0);
    }

    return status;
}

/*!
 * @brief   Helper to get the current clock freq
 */
static FLCN_STATUS
s_smbpbiGetLwrrentClockFreq
(
    LwU32 apiDomain,
    LwU32 *pFreqkHz
)
{
    FLCN_STATUS status;
    LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM domainItem = {0};

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkReadAnyTask)
    };

    domainItem.clkDomain = apiDomain;

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = clkReadDomain_AnyTask(&domainItem);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_smbpbiGetLwrrentClockFreq_exit;
    }

    *pFreqkHz = domainItem.clkFreqKHz;

s_smbpbiGetLwrrentClockFreq_exit:
    return status;
}

/*!
 * @brief   Helper to get the pstate clock entry
 */
static FLCN_STATUS
s_smbpbiGetPstateEntry
(
    PSTATE *pPstate,
    LwU8    apiDomain,
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY *pPStateClkEntry
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       clkDomainIdx;

    if (pPstate == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_smbpbiGetPstateEntry_exit;
    }

    status = clkDomainsGetIndexByApiDomain(apiDomain, &clkDomainIdx);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_smbpbiGetPstateEntry_exit;
    }

    status = perfPstateClkFreqGet(pPstate,
                                  clkDomainIdx,
                                  pPStateClkEntry);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_smbpbiGetPstateEntry_exit;
    }

s_smbpbiGetPstateEntry_exit:
    return status;
}

/*!
 * Get requested clock freq info
 *
 * @param[in]   arg1            clock freq info request (arg1)
 * @param[in]   arg2            requested clock domain (arg2)
 * @param[out]  pData           pointer to the clock freq in Khz
 *
 * @return      status          SMBPBI status
 */
static LwU8
s_smbpbiGetClockFreqInfo
(
    LwU8  arg1,
    LwU8  arg2,
    LwU32 *pData
)
{
    LwU8                                 status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    FLCN_STATUS                          pmuStatus;
    LwU8                                 apiDomain;
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY  pstateClkEntry;
    PSTATE                              *pPstate;
    LwU32                                freqkHz = 0;
    LwBool                               bIsMinQuery = LW_FALSE;

    if (!PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_CLOCK_FREQ_INFO) ||
        !LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 1, _CLOCK_FREQ_INFO))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
    }

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_PSTATE_CLIENT
    };

    switch (arg2)
    {
        case LW_MSGBOX_CMD_ARG2_GET_CLOCK_FREQ_INFO_GPCCLK:
        {
            apiDomain = LW2080_CTRL_CLK_DOMAIN_GPCCLK;
            break;
        }
        case LW_MSGBOX_CMD_ARG2_GET_CLOCK_FREQ_INFO_MEMCLK:
        {
            apiDomain = LW2080_CTRL_CLK_DOMAIN_MCLK;
            break;
        }
        default:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
            goto s_smbpbiGetClockFreqInfo_exit;
        }
    }

    switch (arg1)
    {
        case LW_MSGBOX_CMD_ARG1_GET_CLOCK_FREQ_INFO_LWRRENT:
        {
            pmuStatus = s_smbpbiGetLwrrentClockFreq(apiDomain, &freqkHz);
            if (pmuStatus != FLCN_OK)
            {
                PMU_BREAKPOINT();
                status = s_smbpbiMapPmuStatusToMsgBoxStatus(pmuStatus);
                goto s_smbpbiGetClockFreqInfo_exit;
            }

            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_CLOCK_FREQ_INFO_MIN:
        case LW_MSGBOX_CMD_ARG1_GET_CLOCK_FREQ_INFO_MAX:
        {
            // Attach overlays required to get pstate
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                bIsMinQuery = (arg1 == LW_MSGBOX_CMD_ARG1_GET_CLOCK_FREQ_INFO_MIN);

                /*!
                 * Take semaphore to protect PSTATE pointer access,
                 * and clkFreqGet interface
                 */
                perfReadSemaphoreTake();
                {
                    pPstate = bIsMinQuery ?
                              PSTATE_GET_BY_LOWEST_INDEX() :
                              PSTATE_GET_BY_HIGHEST_INDEX();
                    pmuStatus = s_smbpbiGetPstateEntry(pPstate,
                                                       apiDomain,
                                                       &pstateClkEntry);
                }
                perfReadSemaphoreGive();
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            if (pmuStatus != FLCN_OK)
            {
                PMU_BREAKPOINT();
                status = s_smbpbiMapPmuStatusToMsgBoxStatus(pmuStatus);
                goto s_smbpbiGetClockFreqInfo_exit;
            }

            freqkHz = bIsMinQuery ?
                      pstateClkEntry.min.freqkHz :
                      pstateClkEntry.max.freqkHz;
            break;
        }
        default:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
            break;
        }
    }

s_smbpbiGetClockFreqInfo_exit:
    *pData = freqkHz;
    return status;
}

static void
s_smbpbiHandleDataCopy
(
    LwU32 *pCmd,
    LwU32  data,
    LwU32  extData
)
{
    // Only perform data-copy result size encoding if the command supports it
    LwU32 opcode = DRF_VAL(_MSGBOX, _CMD, _OPCODE, *pCmd);

    switch (opcode)
    {
        case LW_MSGBOX_CMD_OPCODE_GET_ECC_V6:
        {
            *pCmd = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _COPY_SIZE_ENCODING_DATA, data, *pCmd);

            //
            // If extData is non-zero then client needs to read both data-out and
            // ext-data registers for accurate result
            //
            if (extData)
            {
                *pCmd = FLD_SET_DRF(_MSGBOX, _CMD, _COPY_SIZE_ENCODING_READ_DATA_OUT, _SET, *pCmd);
                *pCmd = FLD_SET_DRF(_MSGBOX, _CMD, _COPY_SIZE_ENCODING_READ_EXT_DATA_OUT, _SET, *pCmd);
            }
            else
            {
                *pCmd = FLD_SET_DRF(_MSGBOX, _CMD, _COPY_SIZE_ENCODING_READ_EXT_DATA_OUT, _NOT_SET, *pCmd);

                // Indicate if entire DATA_OUT needs to be read
                if ((data >> DRF_SIZE(LW_MSGBOX_CMD_COPY_SIZE_ENCODING_DATA)) != 0)
                {
                    *pCmd = FLD_SET_DRF(_MSGBOX, _CMD, _COPY_SIZE_ENCODING_READ_DATA_OUT, _SET, *pCmd);
                }
                else
                {
                    *pCmd = FLD_SET_DRF(_MSGBOX, _CMD, _COPY_SIZE_ENCODING_READ_DATA_OUT, _NOT_SET, *pCmd);
                }
            }

            break;
        }

        //
        // If the command doesn't support data-copy result size encoding then copy
        // the data-out register as ususal
        //
        default:
        {
            *pCmd = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _DATA_COPY, data, *pCmd);
            break;

        }
    }
}

/*!
 * Get requested LWLink info
 *
 * @param[in]   arg1      Requested LwLink Info query (arg1)
 * @param[in]   arg2      Requested Link number or Info Page
 * @param[out]  pCmd      Pointer to command which contains
 *                        additional status or data specific
 *                        to the command
 * @param[out]  pData     Pointer to output Data
 * @param[out]  pExtData  Pointer to output ExtData
 *
 * @return      status    SMBPBI status
 */
static LwU8
s_smbpbiGetLwlinkInfoHandler
(
    LwU8  arg1,
    LwU8  arg2,
    LwU32 *pCmd,
    LwU32 *pData,
    LwU32 *pExtData
)
{
    LwU8 status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, smbpbiLwlink)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Check if command is supported
        switch (arg1)
        {
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_COUNT:
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_STATE_V1:
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_BANDWIDTH:
                if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 1,
                                                _LWLINK_INFO_STATE_V1))
                {
                    status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                    goto s_smbpbiGetLwlinkInfoHandler_exit;
                }
                break;
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_REPLAY:
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_RECOVERY:
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_FLIT_CRC:
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_DATA_CRC:
                if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 1,
                                                _LWLINK_ERROR_COUNTERS))
                {
                    status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                    goto s_smbpbiGetLwlinkInfoHandler_exit;
                }
                break;
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_STATE_V2:
                if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2,
                                                _LWLINK_INFO_LINK_STATE_V2))
                {
                    status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                    goto s_smbpbiGetLwlinkInfoHandler_exit;
                }
                break;
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_SUBLINK_WIDTH:
                if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2,
                                                _LWLINK_INFO_SUBLINK_WIDTH))
                {
                    status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                    goto s_smbpbiGetLwlinkInfoHandler_exit;
                }
                break;
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_DATA_TX:
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_DATA_RX:
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_RAW_TX:
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_RAW_RX:
                if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2,
                                                _LWLINK_INFO_THROUGHPUT))
                {
                    status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                    goto s_smbpbiGetLwlinkInfoHandler_exit;
                }
                break;
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_AVAILABILITY:
            {
                if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2,
                                                _LWLINK_INFO_AVAILABILITY))
                {
                    status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                    goto s_smbpbiGetLwlinkInfoHandler_exit;
                }
            }
            break;
            default:
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
                goto s_smbpbiGetLwlinkInfoHandler_exit;
            }
        }

        // Execute command
        switch (arg1)
        {
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_COUNT:
                status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, pData);
                break;
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_STATE_V1:
                status = smbpbiGetLwlinkStateV1_HAL(&Smbpbi, arg1, arg2, pData,
                                                    pExtData);
                break;
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_BANDWIDTH:
                status = smbpbiGetLwlinkBandwidth_HAL(&Smbpbi, arg1, arg2, pData,
                                                      pExtData);
                break;
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_REPLAY:
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_RECOVERY:
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_FLIT_CRC:
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_DATA_CRC:
                status = smbpbiGetLwlinkErrorCount_HAL(&Smbpbi, arg1, arg2, pData);
                break;
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_STATE_V2:
                if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_LWLINK_INFO_LINK_STATE_V2))
                {
                    status = smbpbiGetLwlinkStateV2_HAL(&Smbpbi, arg1, arg2, pData,
                                                        pExtData);
                }
                else
                {
                    status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                }
                break;
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_SUBLINK_WIDTH:
                if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_LWLINK_INFO_SUBLINK_WIDTH))
                {
                    status = smbpbiGetLwlinkSublinkWidth_HAL(&Smbpbi, arg1, arg2,
                                                             pData, pExtData);
                }
                else
                {
                    status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                }
                break;
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_DATA_TX:
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_DATA_RX:
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_RAW_TX:
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_RAW_RX:
                if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_LWLINK_INFO_THROUGHPUT))
                {
                    OSTASK_OVL_DESC ovlDescList[]  = {
                        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, smbpbiLwlink)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64)
                    };
                    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                    {
                        status = smbpbiGetLwlinkThroughput_HAL(&Smbpbi, arg1, arg2,
                                                               pCmd, pData, pExtData);
                    }
                    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
                }
                else
                {
                    status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                }
                break;
            case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_AVAILABILITY:
            {
                //
                // So far, the maximum number of lwlinks is 12, only page 0 is used
                //
                *pData = 0;
                *pExtData = 0;
                if (arg2 == LW_MSGBOX_CMD_LWLINK_INFO_GET_LWLINK_INFO_AVAILABILTY_PAGE_0)
                {
                    *pData = Smbpbi.config.availableLwlinks;
                }
            }
            //
            // The default case can never be hit, since the argument check is done
            // in the previous switch statement. Removing to pacify Coverity.
            //
        }

s_smbpbiGetLwlinkInfoHandler_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Gets PCIe status and error counts
 *
 * @param[in]  arg1     Page index
 * @param[in]  arg2     Must be zero
 * @param[out] pData    Pointer to info:
 *                      - link speed
 *                      - link width
 *                      - non fatal error count
 *                      - fatal error count
 *                      - unsupported req count
 *                      - L0 to recovery count
 *                      - replay rollover count
 *                      - NAKS received count
 * @param[out] pExtData Pointer to info:
 *                      - correctable error count
 *                      - replay count
 *                      - NAKS sent count
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *      if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG1
 *      if arg1 indicates invalid page index
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *      if arg2 is not 0
 */
LwU8
s_smbpbiGetPcieLinkInfoHandler
(
    LwU8  arg1,
    LwU8  arg2,
    LwU32 *pData,
    LwU32 *pExtData
)
{
    LwU8  status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    LwU32 pcieSpeed = 0;
    LwU32 pcieWidth = 0;
    LwU32 fatalErrCount = 0;
    LwU32 nonFatalErrCount = 0;
    LwU32 unsuppReqErrCount = 0;
    LwU32 val = 0;
    LwU32 dataVal = 0;
    LwU32 extDataVal = 0;

    OSTASK_OVL_DESC ovlDescList[] =
    {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBif)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if (arg2 != 0)
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
            goto s_smbpbiGetPcieLinkInfoHandler_exit;
        }

        if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2, _GET_PCIE_LINK_INFO))
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
            goto s_smbpbiGetPcieLinkInfoHandler_exit;
        }

        switch (arg1)
        {
            case LW_MSGBOX_CMD_ARG1_GET_PCIE_LINK_INFO_PAGE_0:
            {
                smbpbiGetPcieSpeedWidth_HAL(&Smbpbi, &pcieSpeed, &pcieWidth);

                dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                          _PCIE_LINK_INFO_PAGE_0_LINK_SPEED,
                                          pcieSpeed, dataVal);

                dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                          _PCIE_LINK_INFO_PAGE_0_LINK_WIDTH,
                                          pcieWidth, dataVal);

                smbpbiGetPcieFatalNonFatalUnsuppReqCount_HAL(&Smbpbi, &fatalErrCount,
                                                             &nonFatalErrCount,
                                                             &unsuppReqErrCount);

                dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                          _PCIE_LINK_INFO_PAGE_0_NONFATAL_ERROR_COUNT,
                                          nonFatalErrCount, dataVal);

                dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                          _PCIE_LINK_INFO_PAGE_0_FATAL_ERROR_COUNT,
                                          fatalErrCount, dataVal);

                dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                          _PCIE_LINK_INFO_PAGE_0_UNSUPP_REQ_COUNT,
                                          unsuppReqErrCount, dataVal);

                smbpbiGetPcieCorrectableErrorCount_HAL(&Smbpbi, &val);

                extDataVal = FLD_SET_DRF_NUM(_MSGBOX, _EXT_DATA,
                                             _PCIE_LINK_INFO_PAGE_0_CORRECTABLE_ERROR_COUNT,
                                             val, extDataVal);

                break;
            }
            case LW_MSGBOX_CMD_ARG1_GET_PCIE_LINK_INFO_PAGE_1:
            {
                smbpbiGetPcieL0ToRecoveryCount_HAL(&Smbpbi, &val);
                dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                          _PCIE_LINK_INFO_PAGE_1_L0_TO_RECOVERY_COUNT,
                                          val, dataVal);

                smbpbiGetPcieReplayCount_HAL(&Smbpbi, &val);
                extDataVal = FLD_SET_DRF_NUM(_MSGBOX, _EXT_DATA,
                                             _PCIE_LINK_INFO_PAGE_1_REPLAY_COUNT,
                                             val, extDataVal);
                break;
            }
            case LW_MSGBOX_CMD_ARG1_GET_PCIE_LINK_INFO_PAGE_2:
            {
                smbpbiGetPcieReplayRolloverCount_HAL(&Smbpbi, &val);
                dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                          _PCIE_LINK_INFO_PAGE_2_REPLAY_ROLLOVER_COUNT,
                                          val, dataVal);

                smbpbiGetPcieNaksRcvdCount_HAL(&Smbpbi, &val);
                dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                          _PCIE_LINK_INFO_PAGE_2_NAKS_RCVD_COUNT,
                                          val, dataVal);

                smbpbiGetPcieNaksSentCount_HAL(&Smbpbi, &val);
                extDataVal = FLD_SET_DRF_NUM(_MSGBOX, _EXT_DATA,
                                             _PCIE_LINK_INFO_PAGE_2_NAKS_SENT_COUNT,
                                             val, extDataVal);

                break;
            }
            case LW_MSGBOX_CMD_ARG1_GET_PCIE_LINK_INFO_PAGE_3:
            {
                // LINK INFO is pre-requisite of this feature
                if ((!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2, _GET_PCIE_LINK_TARGET_SPEED)) ||
                    (!PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_GET_PCIE_LINK_TARGET_SPEED)))
                {
                    status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                    goto s_smbpbiGetPcieLinkInfoHandler_exit;
                }

                smbpbiGetPcieTargetSpeed_HAL(&Smbpbi, &val);
                dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                          _PCIE_LINK_INFO_PAGE_3_TARGET_LINK_SPEED,
                                          val, dataVal);
                break;
            }
            default:
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
                goto s_smbpbiGetPcieLinkInfoHandler_exit;
            }
        }

        *pData = dataVal;
        *pExtData = extDataVal;

s_smbpbiGetPcieLinkInfoHandler_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Get Energy Counter
 *
 * @param[out] pData    Pointer to lower 32-bit
 * @param[out] pExtData Pointer to higher 32-bit
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *      if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED
 *      if fail to get power policy
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *      if power channel fetched is invalid
 * @return LW_MSGBOX_CMD_STATUS_ERR_*
 *      if other FLCN errors parsed into
 *      s_smbpbiMapPmuStatusToMsgBoxStatus()
 */
LwU8
s_smbpbiGetEnergyCounterHandler
(
    LwU32 *pData,
    LwU32 *pExtData
)
{
    LwU8                       status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    FLCN_STATUS                flcnStatus = FLCN_OK;
    PWR_POLICY                *pPolicy = NULL;
    LW2080_CTRL_PMGR_PWR_TUPLE tuple = {0};

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgr)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPwrMonitor)
    };

    PWR_MONITOR_SEMAPHORE_TAKE; // NJ??
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);

    pPolicy = PMGR_GET_PWR_POLICY_IDX(LW2080_CTRL_PMGR_PWR_POLICY_IDX_TGP);
    if (pPolicy == NULL)
    {
        status =  LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
        goto s_smbpbiGetEnergyCounterHandler_exit;
    }


    if (!PWR_CHANNEL_IS_VALID(pPolicy->chIdx))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
        PMU_BREAKPOINT();
        goto s_smbpbiGetEnergyCounterHandler_exit;
    }

    flcnStatus = pwrMonitorPolledTupleGet(pPolicy->chIdx, &tuple);
    status = s_smbpbiMapPmuStatusToMsgBoxStatus(flcnStatus);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        PMU_BREAKPOINT();
        goto s_smbpbiGetEnergyCounterHandler_exit;
    }

    *pData = tuple.energymJ.lo;
    *pExtData = tuple.energymJ.hi;

s_smbpbiGetEnergyCounterHandler_exit:
    PWR_MONITOR_SEMAPHORE_GIVE;
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/**
 * @brief Get current pstate
 *
 * @param[out] pData
 */
static LwU8
s_smbpbiGetLwrrentPstate
(
    LwU32 *pData
)
{
    PSTATE       *pPstate;
    LwBoardObjIdx idx;
    LwU8          status    = LW_MSGBOX_CMD_STATUS_SUCCESS;
    LwU32         index     = LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID;
    LwU32         pstateNum = LW_U32_MAX;
    FLCN_STATUS   pmuStatus = FLCN_OK;

    OSTASK_OVL_DESC ovlDescList[] = { OSTASK_OVL_DESC_DEFINE_PSTATE_CLIENT };
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2, _LWRRENT_PSTATE))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        goto s_smbpbiGetLwrrentPstate_exit;
    }

    perfReadSemaphoreTake();
    index = perfChangeSeqChangeLastPstateIndexGet();
    perfReadSemaphoreGive();
    if (index == LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID)
    {
        // There is pstate presented, mark as not available
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
        goto s_smbpbiGetLwrrentPstate_exit;
    }
    BOARDOBJGRP_ITERATOR_DYNAMIC_CAST_BEGIN(PERF, PSTATE, BASE, pPstate,
                                            idx, NULL, &pmuStatus,
                                            s_smbpbiGetLwrrentPstate_dynEnd)
    {
        if (index == BOARDOBJ_GRP_IDX_TO_8BIT(idx))
        {
            pstateNum = pPstate->pstateNum;
        }
    }
    BOARDOBJGRP_ITERATOR_DYNAMIC_CAST_END;

s_smbpbiGetLwrrentPstate_dynEnd:
    if (pmuStatus != FLCN_OK)
    {
        status = s_smbpbiMapPmuStatusToMsgBoxStatus(pmuStatus);
        goto s_smbpbiGetLwrrentPstate_exit;
    }

    /**
     * @see PSTATE_NUM_MAX
     * Given current max num of pstate is 15, allocate 3:0 of page 3 for pstate.
     */
    *pData = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
        _GET_CLOCK_FREQ_INFO_PAGE_3_LWRRENT_PSTATE, pstateNum, *pData);

s_smbpbiGetLwrrentPstate_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

#if PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X)
/**
 * Helper function to fill in min/max clock frequency in MHz
 *
 * @param[in]    arg2    Arg2 choose clock type
 * @param[out]   pData   Pointer to the data
 *
 * @return _SUCCESS if success
 *         _ERR_ARG2 if clock domain is invalid
 */
static LwU8
s_smbpbiGetPowerHintInfoClkHelper
(
    LwU8   arg2,
    LwU32 *pData
)
{
    LwU8     status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    LwU32    clkDomain  = 0;
    LwU32    minFreqKHz = 0;
    LwU32    maxFreqKHz = 0;

    switch (arg2)
    {
        case LW_MSGBOX_CMD_ARG2_GET_POWER_HINT_INFO_CLK_GR:
        {
            clkDomain = LW_MSGBOX_CMD_ARG2_GET_CLOCK_FREQ_INFO_GPCCLK;
            break;
        }
        case LW_MSGBOX_CMD_ARG2_GET_POWER_HINT_INFO_CLK_MEM:
        {
            clkDomain = LW_MSGBOX_CMD_ARG2_GET_CLOCK_FREQ_INFO_MEMCLK;
            break;
        }
        default:
            return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    status = s_smbpbiGetClockFreqInfo(
                LW_MSGBOX_CMD_ARG1_GET_CLOCK_FREQ_INFO_MIN,
                clkDomain, &minFreqKHz);
    if (LW_MSGBOX_CMD_STATUS_SUCCESS != status)
    {
        return status;
    }

    status = s_smbpbiGetClockFreqInfo(
                LW_MSGBOX_CMD_ARG1_GET_CLOCK_FREQ_INFO_MAX,
                clkDomain, &maxFreqKHz);
    if (LW_MSGBOX_CMD_STATUS_SUCCESS != status)
    {
        return status;
    }

    switch (arg2)
    {
        case LW_MSGBOX_CMD_ARG2_GET_POWER_HINT_INFO_CLK_GR:
        {
            *pData = FLD_SET_DRF_NUM(_MSGBOX_DATA,
                                     _POWER_HINT_INFO_CLK_GR,
                                     _MIN, minFreqKHz / 1000, *pData);
            *pData = FLD_SET_DRF_NUM(_MSGBOX_DATA,
                                     _POWER_HINT_INFO_CLK_GR,
                                     _MAX, maxFreqKHz / 1000, *pData);
            break;
        }
        case LW_MSGBOX_CMD_ARG2_GET_POWER_HINT_INFO_CLK_MEM:
        {
            *pData = FLD_SET_DRF_NUM(_MSGBOX_DATA,
                                     _POWER_HINT_INFO_CLK_MEM,
                                     _MIN, minFreqKHz / 1000, *pData);
            *pData = FLD_SET_DRF_NUM(_MSGBOX_DATA,
                                     _POWER_HINT_INFO_CLK_MEM,
                                     _MAX, maxFreqKHz / 1000, *pData);
            break;
        }
        default:
            return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    return status;
}

/**
 * Helper function that fill in temperature information
 *
 * @param[out]      pData  Pointer to the return data
 *
 * @return _SUCCESS if success
 *         _NOT_SUPPORTED if thermal channel is not supported
 *                        or no thermal channels are supporting temp simulation
 */
static LwU8
s_smbpbiGetPowerHintInfoTempHelper
(
    LwU32 *pData
)
{
    // Using macro along with OBJTHERM
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_CHANNEL)
    LwBoardObjIdx i;
    LwS32 minTemp = LW_S32_MIN;
    LwS32 maxTemp = LW_S32_MAX;
    THERM_CHANNEL  *pChannel;

    BOARDOBJGRP_ITERATOR_BEGIN(THERM_CHANNEL, pChannel, i, NULL)
    {
        if (pChannel->tempSim.bSupported)
        {
            minTemp = LW_MAX(minTemp,
                         LW_TYPES_LW_TEMP_TO_CELSIUS_TRUNCED(pChannel->lwTempMin));
            maxTemp = LW_MIN(maxTemp,
                         LW_TYPES_LW_TEMP_TO_CELSIUS_TRUNCED(pChannel->lwTempMax));
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    if ((minTemp == LW_S32_MIN) || (maxTemp == LW_S32_MAX))
    {
        // No thermal channels supporting temp simulation
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
    }

    *pData = FLD_SET_DRF_NUM(_MSGBOX_DATA, _POWER_HINT_INFO_TEMP, _MIN,
                             minTemp, *pData);
    *pData = FLD_SET_DRF_NUM(_MSGBOX_DATA, _POWER_HINT_INFO_TEMP, _MAX,
                             maxTemp, *pData);

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
#else
    return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
#endif
}

/**
 * Helper function to fill in proiles mask
 * One page consists of 32-bit data and 32-bit ext data which represents 64
 * profiles
 *
 * @param[in]  page      Page index
 * @param[out] pData     Pointer to the data
 * @param[out] pExtData  Pointer to the ext data
 *
 * return _SUCCESS  if success
 *        _ERR_MISC if profile id is invalid
 */
static LwU8
s_smbpbiGetPowerHintInfoProfilesHelper
(
    LwU8   page,
    LwU32 *pData,
    LwU32 *pExtData
)
{
    LwU8            status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    LwBoardObjIdx   i;
    CLIENT_PERF_CF_PWR_MODEL_PROFILE    *pProfile;

    *pData = 0;
    *pExtData = 0;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCf)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfModel)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);

    if (page >= LW_MSGBOX_CMD_ARG2_GET_POWER_HINT_INFO_PROFILES_TOTAL_PAGES)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto s_smbpbiGetPowerHintInfoProfilesHelper_exit;
    }

    BOARDOBJGRP_ITERATOR_BEGIN(CLIENT_PERF_CF_PWR_MODEL_PROFILE, pProfile, i, NULL)
    {
        if (page == 0)
        {
            switch (pProfile->profileId)
            {
                case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_ID_DMMA_PERF:
                    *pData = FLD_SET_DRF(_MSGBOX,
                                _DATA_POWER_HINT_INFO_PROFILES_PAGE_0, _DMMA_PERF,
                                _AVAILABLE, *pData);
                    break;
                case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_ID_DMMA_HIGH_K:
                    *pData = FLD_SET_DRF(_MSGBOX,
                                _DATA_POWER_HINT_INFO_PROFILES_PAGE_0, _DMMA_HIGH_K,
                                _AVAILABLE, *pData);
                    break;
                case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_ID_DMMA_LOW_K:
                    *pData = FLD_SET_DRF(_MSGBOX,
                                _DATA_POWER_HINT_INFO_PROFILES_PAGE_0, _DMMA_LOW_K,
                                _AVAILABLE, *pData);
                    break;
                case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_ID_HMMA:
                    *pData = FLD_SET_DRF(_MSGBOX,
                                _DATA_POWER_HINT_INFO_PROFILES_PAGE_0, _HMMA,
                                _AVAILABLE, *pData);
                    break;
                case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_ID_IMMA:
                    *pData = FLD_SET_DRF(_MSGBOX,
                                _DATA_POWER_HINT_INFO_PROFILES_PAGE_0, _IMMA,
                                _AVAILABLE, *pData);
                    break;
                case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_ID_SGEMM:
                    *pData = FLD_SET_DRF(_MSGBOX,
                                _DATA_POWER_HINT_INFO_PROFILES_PAGE_0, _SGEMM,
                                _AVAILABLE, *pData);
                    break;
                case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_ID_TRANSFORMER:
                    *pData = FLD_SET_DRF(_MSGBOX,
                                _DATA_POWER_HINT_INFO_PROFILES_PAGE_0, _TRANSFORMER,
                                _AVAILABLE, *pData);
                    break;
                case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_ID_LWSTOMER_LWSTOM_0:
                    *pData = FLD_SET_DRF(_MSGBOX,
                                _DATA_POWER_HINT_INFO_PROFILES_PAGE_0, _LWSTOMER_LWSTOM_0,
                                _AVAILABLE, *pData);
                    break;
                case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_ID_LWSTOMER_LWSTOM_1:
                    *pData = FLD_SET_DRF(_MSGBOX,
                                _DATA_POWER_HINT_INFO_PROFILES_PAGE_0, _LWSTOMER_LWSTOM_1,
                                _AVAILABLE, *pData);
                    break;
                case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_ID_LWSTOMER_LWSTOM_2:
                    *pData = FLD_SET_DRF(_MSGBOX,
                                _DATA_POWER_HINT_INFO_PROFILES_PAGE_0, _LWSTOMER_LWSTOM_2,
                                _AVAILABLE, *pData);
                    break;
                case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_ID_LWSTOMER_LWSTOM_3:
                    *pData = FLD_SET_DRF(_MSGBOX,
                                _DATA_POWER_HINT_INFO_PROFILES_PAGE_0, _LWSTOMER_LWSTOM_3,
                                _AVAILABLE, *pData);
                    break;
                case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_ID_LWSTOMER_LWSTOM_4:
                    *pData = FLD_SET_DRF(_MSGBOX,
                                _DATA_POWER_HINT_INFO_PROFILES_PAGE_0, _LWSTOMER_LWSTOM_4,
                                _AVAILABLE, *pData);
                    break;
                case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_ID_LWSTOMER_LWSTOM_5:
                    *pData = FLD_SET_DRF(_MSGBOX,
                                _DATA_POWER_HINT_INFO_PROFILES_PAGE_0, _LWSTOMER_LWSTOM_5,
                                _AVAILABLE, *pData);
                    break;
                case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_ID_LWSTOMER_LWSTOM_6:
                    *pData = FLD_SET_DRF(_MSGBOX,
                                _DATA_POWER_HINT_INFO_PROFILES_PAGE_0, _LWSTOMER_LWSTOM_6,
                                _AVAILABLE, *pData);
                    break;
                case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_ID_LWSTOMER_LWSTOM_7:
                    *pData = FLD_SET_DRF(_MSGBOX,
                                _DATA_POWER_HINT_INFO_PROFILES_PAGE_0, _LWSTOMER_LWSTOM_7,
                                _AVAILABLE, *pData);
                    break;
                default:
                    // The profile id is invalid
                    status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
                    goto s_smbpbiGetPowerHintInfoProfilesHelper_exit;
            }
        }
    }
    BOARDOBJGRP_ITERATOR_END;

s_smbpbiGetPowerHintInfoProfilesHelper_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/**
 * @brief Get Power Hint Information
 *
 * @param[in]   arg1         Arg1 - CLK/TEMP/PROFILES
 * @param[in]   arg2         Arg2 - Clock type/profile page
 * @param[out]  pData       Pointer to data
 * @param[out]  pExtData    Pointer to ext data
 *
 * @return _SUCCESS       if success
 *         _NOT_SUPPORTED if power hint is not supported
 *         _ERR_ARG1      if arg1 does not exist
 *         _ERR_ARG2      if arg2 does not exist
 */
static LwU8
s_smbpbiGetPowerHintInfoHandler
(
    LwU8   arg1,
    LwU8   arg2,
    LwU32 *pData,
    LwU32 *pExtData
)
{
    LwU8    status     = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 4, _POWER_HINT))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
    }

    switch (arg1)
    {
        case LW_MSGBOX_CMD_ARG1_GET_POWER_HINT_INFO_CLK:
        {
            status = s_smbpbiGetPowerHintInfoClkHelper(arg2, pData);
            if (LW_MSGBOX_CMD_STATUS_SUCCESS != status)
            {
                return status;
            }
            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_POWER_HINT_INFO_TEMP:
        {
            status = s_smbpbiGetPowerHintInfoTempHelper(pData);
            if (LW_MSGBOX_CMD_STATUS_SUCCESS != status)
            {
                return status;
            }
            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_POWER_HINT_INFO_PROFILES:
        {
            status = s_smbpbiGetPowerHintInfoProfilesHelper(
                        arg2, pData, pExtData);
            if (LW_MSGBOX_CMD_STATUS_SUCCESS != status)
            {
                return status;
            }
            break;
        }
        default:
            return LW_MSGBOX_CMD_STATUS_ERR_ARG1;
    }
    return status;
}
#endif

static FLCN_STATUS
s_smbpbiGetRequest
(
    LwU32 *pCmd,
    LwU32 *pData,
    LwU32 *pExtData
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_FSP_RPC)
    LW_STATUS lwStatus;
    LwU32 payloadSizeDwords;
    FSP_SMBPBI_RPC_REQUEST_PARAMS reqParams;

    lwStatus = fspRpcMessageReceive((LwU32 *)&reqParams, sizeof(reqParams),
                                    &payloadSizeDwords, LWDM_TYPE_SMBPBI,
                                    0U /* timeoutUs */);
    if (lwStatus != LW_OK)
    {
        dbgPrintf(LEVEL_ERROR, "Failed to retrieve message from FSP. Error %d\n", lwStatus);

        return FLCN_ERROR;
    }

    if (payloadSizeDwords != sizeof(reqParams) / FSP_RPC_BYTES_PER_DWORD)
    {
        dbgPrintf(LEVEL_ERROR, "Unexpected payload\n");

        return FLCN_ERROR;
    }

    if (DRF_VAL(_FSP, _SMBPBI_RPC, _RESERVED, reqParams.header.data) != 0)
    {
        dbgPrintf(LEVEL_ERROR, "Reserved bits not 0.\n");

        return FLCN_ERROR;
    }

    *pCmd  = reqParams.command;
    *pData = reqParams.data;
    *pExtData = reqParams.extData;

    return FLCN_OK;
#else
    return smbpbiExelwte_HAL(&Smbpbi, pCmd, pData, pExtData,
                             PMU_SMBPBI_GET_REQUEST);
#endif
}

static FLCN_STATUS
s_smbpbiSetResponse
(
    LwU32 cmd,
    LwU32 data,
    LwU32 extData,
    LwU32 reqStatus
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_FSP_RPC)
    LW_STATUS lwStatus;
    FSP_SMBPBI_RPC_RESPONSE_PARAMS respParams = {{ 0 }};

    respParams.header.data = DRF_DEF(_FSP, _SMBPBI_RPC, _TYPE, _RESPONSE);

    respParams.cmdResponse.commandLwdmType = LWDM_TYPE_SMBPBI;

    if (reqStatus == LW_OK)
    {
        respParams.data = data;
        respParams.extData = extData;
        respParams.cmdStatus = cmd;
        respParams.cmdResponse.errorCode = FLCN_OK;
    }
    else
    {
        respParams.cmdResponse.errorCode = FLCN_ERROR;
    }

    lwStatus = fspRpcMessageSend((LwU32 *)&respParams,
                                 sizeof(respParams) / FSP_RPC_BYTES_PER_DWORD,
                                 LWDM_TYPE_SMBPBI, 0, LW_FALSE);
    if (lwStatus != LW_OK)
    {
        dbgPrintf(LEVEL_ERROR, "Failed to send message to FSP. Error %d\n", lwStatus);

        return FLCN_ERROR;
    }

    return FLCN_OK;
#else
    return smbpbiExelwte_HAL(pSmbpbi, &cmd, &data, &extData,
                                PMU_SMBPBI_SET_RESPONSE);
#endif
}
