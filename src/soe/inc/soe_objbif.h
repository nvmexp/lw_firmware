/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_OBJBIF_H
#define SOE_OBJBIF_H

/*!
 * @file soe_objbif.h
 * @brief  SOE BIF Library HAL header file.
 */


/* ------------------------ System Includes -------------------------------- */
#include "lwuproc.h"
#include "soesw.h"

/* ------------------------ Application Includes --------------------------- */
#include "unit_dispatch.h"
#include "config/g_bif_hal.h"
/* ------------------------- Global Variables ------------------------------- */
LwrtosQueueHandle Disp2QBifThd;

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
 * BIF Object definition
 */
typedef struct
{
    BIF_HAL_IFACES  hal;
    OBJBIF_MARGINING MarginingData;
} OBJBIF;

/* ------------------------ External Definitions --------------------------- */
extern OBJBIF Bif;

/* ------------------------- Macros and Defines ----------------------------- */
#define XTL_REG_WR32(addr, value)     BAR0_REG_WR32(DEVICE_BASE(LW_XTL) + (addr), (value))
#define XTL_REG_RD32(addr)            BAR0_REG_RD32(DEVICE_BASE(LW_XTL) + (addr))

#define XPL_REG_WR32(addr, value)     BAR0_REG_WR32(LW_PTOP_UNICAST_SW_DEVICE_BASE_XPL_0 + (addr), (value))
#define XPL_REG_RD32(addr)            BAR0_REG_RD32(LW_PTOP_UNICAST_SW_DEVICE_BASE_XPL_0 + (addr))

/*!
 * Macro to get address of MarginingData
 */
#define MARGINING_DATA_GET()              (&Bif.MarginingData)

#define MARGINING_RECOVERY_STATE_GET()    (&((&Bif.MarginingData)->marginRecoveryState))

/*!
 * To indicate that step margin type is invalid
 */
#define BIF_ILWALID_STEP_MARGIN_TYPE LW_U8_MAX

/*!
 * To indicate that step margin offset is invalid
 */
#define BIF_ILWALID_STEP_MARGIN_OFFSET LW_U8_MAX

/*!
 * Timeout in microseconds waiting for the task BIF's queue to be ready
 */
#define BIF_STEP_MARGINING_TIMEOUT_US 5000U

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

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS constructBif(void) GCC_ATTRIB_SECTION("imem_init", "constructBif");
#endif // SOE_OBJBIF_H
