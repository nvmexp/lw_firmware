/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJBIF_H
#define PMU_OBJBIF_H

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"

/* ------------------------ Forward Declartion ----------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "config/g_bif_hal.h"
#include "unit_api.h"
#include "perf/perf_limit_client.h"

/* ------------------------ Defines ---------------------------------------- */

//
// Quote from original CL#22951682 for definition movement
// TODO: These are all numbers I made up with no actual basis other than they
// work for me lwrrently.
// How should I determine them properly?
// LTSSM link ready expected timeout is defined based on distribution of link
// training times.see http://lwbugs/2980823/84. From the data, we can see that
// for some cases >5ms time is required for link training to complete. Keeping
// link ready timeout to 20ms on a very safe side given that we have no data for
// wide set of chipsets.
//

// Maximum time for waiting LW_XVE_MSG_CTRL_TRIGGER goes low, 2ms is fine from ARCH
#define BIF_TLP_MSGSEND_TIMTOUT_NS        (2U*1000U*1000U)

/*!
 * Timeout in microseconds waiting for the task BIF's queue to be ready
 */
#define BIF_STEP_MARGINING_TIMEOUT_US 5000U

/*!
 * Maximum PCIe lanes supported per link is 16 as of PCIe spec 4.0r1.0
 */
#define BIF_MAX_PCIE_LANES 16U

/*!
 * To indicate that step margin type is invalid
 */
#define BIF_ILWALID_STEP_MARGIN_TYPE LW_U8_MAX

/*!
 * To indicate that step margin offset is invalid
 */
#define BIF_ILWALID_STEP_MARGIN_OFFSET LW_U8_MAX

/*!
 * Event type for XUSB to PMU request interrupt
 */
#define BIF_EVENT_ID_XUSB_TO_PMU_MSGBOX   (DISP2UNIT_EVT__COUNT + 0)

/*!
 * Event type for PMU to XUSB ack interrupt
 */
#define BIF_EVENT_ID_PMU_TO_XUSB_ACK      (DISP2UNIT_EVT__COUNT + 1)

/*!
 * Event type for step margining commands
 */
#define BIF_EVENT_ID_LANE_MARGIN          (DISP2UNIT_EVT__COUNT + 2)

/*!
 * Find 2's complement
 */
#define BIF_TWOS_COMPLEMENT_GET(payload)  (~(payload) + 0x1)

/*!
 * Value indicating bifLwlinkTlcLpwrSet has not been called yet.
 */
#define BIF_LWLINK_VBIOS_INDEX_UNSET      (LW_U32_MAX)

/*!
 * Macro to get whether BIF task is loaded.
 */
#define BIF_IS_LOADED()                   (Bif.bLoaded)

/*!
 * Macro to get address of MarginingData
 */
#define MARGINING_DATA_GET()              (&MarginingData)

#define MARGINING_RECOVERY_STATE_GET()    (&((&MarginingData)->marginRecoveryState))

// Enable the multifunction state
#define BIF_ENABLE_GPU_MULTIFUNC_STATE              (0x00000000)

// Disable the multifunction state
#define BIF_DISABLE_GPU_MULTIFUNC_STATE             (0x00000001)

// L1 idle threshold values and their respective encoding from dev_lw_xp.h
#define BIF_L1_IDLE_THRESHOLD_8_US                  0x6
#define BIF_L1_IDLE_THRESHOLD_32_US                 0x8
#define BIF_L1_IDLE_THRESHOLD_256_US                0xB
#define BIF_L1_IDLE_THRESHOLD_FRAC_0                0x0

#define BIF_CHECK_L1_RESIDENCY_CALLBACK_PERIOD_US   1000*1000
#define LW_BIF_BIT_MASK                             0x7FFFFFFF
#define LW_XVE_PCIE_UTIL_CLR_DONE                   0x80000000

/* ------------------------ Types definitions ------------------------------ */

/*!
 * @brief Structure to send lane margining request
 */
typedef struct
{
    /*!
     * Must be the first element of the structure
     */
    DISP2UNIT_HDR       hdr;

    /*!
     * Requested lane number
     */
    LwU8                laneNum;
} DISPATCH_SIGNAL_LANE_MARGINING;


typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;

    DISPATCH_SIGNAL_LANE_MARGINING signalLaneMargining;
} DISPATCH_BIF;

/*!
 * @brief Structure for PCIE VBIOS DATA
 */
typedef struct
{
    RM_PMU_BIF_PCIE_GPU_DATA      pcieGpuData;
    RM_PMU_BIF_PCIE_PLATFORM_DATA pciePlatformData;
} BIF_VBIOS;

/*!
 * BIF object Definition
 */
typedef struct
{
    // BIF capabilities
    LwU32 bifCaps;

    // Mask of disabled Lwlinks
    LwU32 lwlinkDisableMask;

    // Link mask for which Lpwr enablement is allowed
    LwU32 lwlinkLpwrEnableMask;

    //
    // Bitmask of whether 1/8th mode is enabled or disabled for each link.
    // Defaults to true when a link is enabled. When false, overrides bLwlinkGlobalLpEnable.
    //
    LwU32 lwlinkLpwrSetMask;

    // Current VBIOS setting. Initialized to LW_U32_MAX, changed by bifLwlinkTlcLpwrSet.
    LwU32 lwlinkVbiosIdx;

    // Number of times link speed switch failed
    LwU32 linkSpeedSwitchingErrorCount;

    // Has configuration been sent from RM yet?
    LwBool bInitialized;

    // Boolean tracking whether BIF task is loaded.
    LwBool bLoaded;

    // If true, LWLINK changing is suspended.
    LwBool bLwlinkSuspend;

    // LP entry enable for all links (cached from VBIOS setting)
    LwBool bLwlinkGlobalLpEnable;

    // Is the GPU running in RTL simulator?
    LwBool bRtlSim; // Found on GF117, bug 753939, still needed?

    // Did PMU receive response to isoch query sent during PMU BIF load
    LwBool bIsochReplyReceived;

    // Saved registers for enabling and disabling L1 substates
    RM_PMU_BIF_L1SS_REGISTERS   l1ssRegs;

    // PCIE field values(l1 threshold, ltr threshold etc.) for the current chipset
    RM_PMU_BIF_PLATFORM_PARAMS  platformParams;

    // Is USB-C debug mode on?
    LwBool bUsbcDebugMode;

    // Is this a NB GPU?
    LwBool bMobile;

    // Is Lane Margining enabled from the SW side?
    LwBool bMarginingEnabledFromSW;

    // Regkey to control disabling of higher gen speeds during gen speed switch
    LwBool bDisableHigherGenSpeedsDuringGenSpeedSwitch;

    // Used if we want to enable ASPM during GPU load itself
    LwBool bEnableAspmAtLoad;

    /*!
     * Pointer to the PERF_LIMITS_CLIENT structure. This is used to set and
     * clear PERF_LIMITS when using the PMU arbiter.
     */
    PERF_LIMITS_CLIENT *pPerfLimits;

    // Structure to store VBIOS fields
    BIF_VBIOS vbiosData;

    // Ltssm link ready timeout value to be set by regkey in ns
    LwU32 linkReadyTimeoutNs;

    // Is Lane Margining enabled via regkey?
    LwBool bMarginingRegkeyEnabled;

    // Current pstate number
    LwU8   pstateNum;

    // Cancel callback for Pex Power Savings
    LwBool bCancelCallback;

    // Regkey to enable Pex Power Savings
    LwBool bPexPowerSavingsEnabled;

    // Regkey to set lowerLimitDiffPercentage in Pex Power Savings
    LwU8  lowerLimitDiffPercentage;

    // Regkey to set upperLimitDiffPercentage in Pex Power Savings
    LwU8  upperLimitDiffPercentage;

    // Regkey to set lowerLimitNewL1Residency in Pex Power Savings
    LwU8  lowerLimitNewL1Residency;

    // Regkey to set upperLimitNewL1Residency in Pex Power Savings
    LwU8  upperLimitNewL1Residency;

    // Is power source DC?
    LwBool bPowerSrcDc;

    // Store the pcie index from during pstate change
    LwU8 lwrrPstatePcieIdx;
} OBJBIF;

/*!
 * Represents SW state when the Lane Margining is stopped due to link entering
 * recovery. This would be used to restore the Lane Margining state when link
 * comes back to L0 from recovery
 */
typedef struct
{
    /*!
     * Last active lane number when SW stopped Margining
     */
    LwU8 lastActiveLaneNum;

    /*!
     * Last active Margining scheduled mask when SW stopped margining
     */
    LwU32 lastMarginingScheduledMask;
} OBJBIF_MARGINING_RECOVERY_STATE;

typedef struct
{
    /*!
     * Step Margining is already scheduled for which all lanes?
     */
    LwU32 marginingScheduledMask;

    /*!
     * Maximum error count before which link is considered good enough.
     * Set by the margining application.
     */
    LwU32 marginingErrorCountLimit;

    /*!
     * To store the last margin offset used in step margining for each lane
     */
    LwU8 lastStepMarginOffset[BIF_MAX_PCIE_LANES];

    /*!
     * To store the last margin type used in step margining for each lane
     */
    LwU8 lastStepMarginType[BIF_MAX_PCIE_LANES];

    /*!
    * To track if Lane Margining is stopped due to link entering into recovery
    */
    LwBool bMarginingStoppedLinkRecovery;

    /*!
    * To track if Step Margin command is already queued or in progress. If this
    * is set to true, then client must not queue another command for Step
    * Margin.
    */
    LwBool bStepMarginCmdQueuedOrInProgress;

    /*!
    * Represents SW state when the Lane Margining is stopped due to link
    * entering recovery
    */
    OBJBIF_MARGINING_RECOVERY_STATE marginRecoveryState;
} OBJBIF_MARGINING;

/*!
 * Resident BIF object Definition
 */
typedef struct
{
    /*!
     * Semaphore for performing bus actions.
     * Prevents cases of RM and Perf both trying to switch at the same time.
     */
    LwrtosSemaphoreHandle switchLock;
} OBJBIF_RESIDENT;

/*!
 * Parameters to find and store L1 residency during Pex Power Savings
 */
typedef struct
{
    /*!
     * Cirlwlar queue's front
     */
    LwS8  front;
    /*!
     * Cirlwlar queue's rear
     */
    LwS8  rear;
    /*!
     * Size of the cirlwlar queue
     */
    LwU8  size;
    /*!
     * Array to store 5 conselwtive residencies in the queue
     */
    LwU32 l1ResidenciesArray[5];
} OBJBIF_L1_RESIDENCY_QUEUE_PARAMS;

typedef struct
{
    /*!
     * Tx L0 time in us
     */
    LwU32 prevTxL0CountUs;
    /*!
     * Rx L0 time in us
     */
    LwU32 prevRxL0CountUs;
    /*!
     * Tx L0s time in us
     */
    LwU32 prevTxL0sCountUs;
    /*!
     * Rx L0s time in us
     */
    LwU32 prevRxL0sCountUs;
    /*!
     * Non L0 and L0s time in us
     */
    LwU32 prevNonL0L0sCountUs;
} OBJBIF_L1_RESIDENCY_PARAMS;

/*!
 * @brief Defines for PCIE request types
 */
#define BIF_LINK_SPEED_UNLOCK                                        0x00000000
#define BIF_LINK_SPEED_LOCK_GEN1                                     0x00000001
#define BIF_LINK_SPEED_LOCK_GEN2                                     0x00000002
#define BIF_LINK_SPEED_LOCK_GEN3                                     0x00000003
#define BIF_LINK_SPEED_LOCK_GEN4                                     0x00000004
#define BIF_LINK_SPEED_LOCK_UNDEFINED                                0x000000FF

/*!
 * @brief PCIE_LANE_MARGIN_TYPE defines valid lane margining types
 * Value 0 and 6 are not defined(as per PCIe spec 4.0r1.0)
 */
typedef LwU8 PCIE_LANE_MARGIN_TYPE;
#define PCIE_LANE_MARGIN_TYPE_1 (0x1U)
#define PCIE_LANE_MARGIN_TYPE_2 (0x2U)
#define PCIE_LANE_MARGIN_TYPE_3 (0x3U)
#define PCIE_LANE_MARGIN_TYPE_4 (0x4U)
#define PCIE_LANE_MARGIN_TYPE_5 (0x5U)
/*!
 * This is unused as per the current spec
 */
#define PCIE_LANE_MARGIN_TYPE_6 (0x6U)
#define PCIE_LANE_MARGIN_TYPE_7 (0x7U)


/*!
 * @brief Following defines are as per PCIe spec 4.0r1.0, section 4.2.13
 */
#define PCIE_LANE_MARGIN_MAX_ERROR_COUNT                             0x3F
#define PCIE_LANE_MARGIN_PAYLOAD_SET_ERROR_COUNT_CMD_MASK            0xC0
#define PCIE_LANE_MARGIN_PAYLOAD_SET_ERROR_COUNT_VALUE_MASK          0x3F

#define PCIE_LANE_MARGIN_REPORT_CAPABILITIES                         0x88
#define PCIE_LANE_MARGIN_REPORT_NUM_VOLTAGE_STEPS                    0x89
#define PCIE_LANE_MARGIN_REPORT_NUM_TIMING_STEPS                     0x8A
#define PCIE_LANE_MARGIN_REPORT_MAX_TIMING_OFFSET                    0x8B
#define PCIE_LANE_MARGIN_REPORT_MAX_VOLTAGE_OFFSET                   0x8C
#define PCIE_LANE_MARGIN_REPORT_SAMPLING_RATE_VOLTAGE                0x8D
#define PCIE_LANE_MARGIN_REPORT_SAMPLING_RATE_TIMING                 0x8E
#define PCIE_LANE_MARGIN_REPORT_SAMPLE_COUNT                         0x8F
#define PCIE_LANE_MARGIN_REPORT_MAX_LANES                            0x90

#define PCIE_LANE_MARGIN_PAYLOAD_SET_ERROR_COUNT_LIMIT               0xC0
#define PCIE_LANE_MARGIN_PAYLOAD_GO_TO_NORMAL_SETTINGS               0x0F
#define PCIE_LANE_MARGIN_PAYLOAD_CLEAR_ERROR_LOG                     0x55
#define PCIE_LANE_MARGIN_PAYLOAD_NO_COMMAND                          0x9C

#define PCIE_LANE_MARGIN_TOO_MANY_ERRORS                             0x0U
#define PCIE_LANE_MARGIN_SET_UP_IN_PROGRESS                          0x1U
#define PCIE_LANE_MARGIN_MARGINING_IN_PROGRESS                       0x2U

/*!
 * @brief Defines for L1 Enable bits in PCI Platform table
 */
#define LW_PMU_PCIE_PLATFORM_GEN1_L1                                 0:0
#define LW_PMU_PCIE_PLATFORM_GEN1_L1_DISABLED                        0x00000000
#define LW_PMU_PCIE_PLATFORM_GEN1_L1_ENABLED                         0x00000001
#define LW_PMU_PCIE_PLATFORM_GEN2_L1                                 1:1
#define LW_PMU_PCIE_PLATFORM_GEN2_L1_DISABLED                        0x00000000
#define LW_PMU_PCIE_PLATFORM_GEN2_L1_ENABLED                         0x00000001
#define LW_PMU_PCIE_PLATFORM_GEN3_L1                                 2:2
#define LW_PMU_PCIE_PLATFORM_GEN3_L1_DISABLED                        0x00000000
#define LW_PMU_PCIE_PLATFORM_GEN3_L1_ENABLED                         0x00000001
#define LW_PMU_PCIE_PLATFORM_GEN4_L1                                 3:3
#define LW_PMU_PCIE_PLATFORM_GEN4_L1_DISABLED                        0x00000000
#define LW_PMU_PCIE_PLATFORM_GEN4_L1_ENABLED                         0x00000001
#define LW_PMU_PCIE_PLATFORM_GEN5_L1                                 4:4
#define LW_PMU_PCIE_PLATFORM_GEN5_L1_DISABLED                        0x00000000
#define LW_PMU_PCIE_PLATFORM_GEN5_L1_ENABLED                         0x00000001

/*!
 * @brief Defines for pstate numbers
 */
#define PSTATE_NUM_P0                      0
#define PSTATE_NUM_P1                      1
#define PSTATE_NUM_P2                      2
#define PSTATE_NUM_P3                      3
#define PSTATE_NUM_P4                      4
#define PSTATE_NUM_P5                      5
#define PSTATE_NUM_P6                      6
#define PSTATE_NUM_P7                      7
#define PSTATE_NUM_P8                      8
#define PSTATE_NUM_P9                      9
#define PSTATE_NUM_P10                    10
#define PSTATE_NUM_P11                    11
#define PSTATE_NUM_P12                    12
#define PSTATE_NUM_P13                    13
#define PSTATE_NUM_P14                    14
#define PSTATE_NUM_P15                    15
#define PSTATE_NUM_MIN         PSTATE_NUM_P0
#define PSTATE_NUM_MAX        PSTATE_NUM_P15


/* ------------------------ External definitions --------------------------- */
extern OBJBIF           Bif;
extern OBJBIF_RESIDENT  BifResident;
extern OBJBIF_MARGINING MarginingData;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

FLCN_STATUS bifPostInit(void)
    // Called when the bif task is scheduled for the first time.
    GCC_ATTRIB_SECTION("imem_libBifInit", "bifPostInit");

FLCN_STATUS bifSetPmuPerfLimits(LW2080_CTRL_PERF_PERF_LIMIT_ID perfLimitId,
                                LwBool bClearLimit, LwU32 lockSpeed)
    GCC_ATTRIB_SECTION("imem_libBif", "bifSetPmuPerfLimits");

FLCN_STATUS bifPerfChangeStep(LwU32 pstateIdx, LwU8 pcieIdx)
    GCC_ATTRIB_SECTION("imem_libBif", "bifPerfChangeStep");

LwBool bifIsL11Allowed(LwU8 pcieIdx, LwBool bIsPerfChangeInprogress)
    GCC_ATTRIB_SECTION("imem_libBif", "bifIsL11Allowed");

LwBool bifIsL12Allowed(LwU8 pcieIdx, LwBool bIsPerfChangeInprogress)
    GCC_ATTRIB_SECTION("imem_libBif", "bifIsL12Allowed");

LwU32 bifGetDisabledLwlinkMask(void)
    GCC_ATTRIB_SECTION("imem_libBif", "bifGetDisabledLwlinkMask");

#endif // PMU_OBJBIF_H
