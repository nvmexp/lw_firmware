/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_DISP_H
#define PMU_DISP_H

/*!
 * @file    pmu_disp.h
 * @copydoc task_disp.c
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "lib/lib_pwm.h"

/* ------------------------ Application includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu/pmuifdisp.h"
#include "unit_api.h"
#include "main.h"
#include "lwtypes.h"
#include "lwostimer.h"

/* ------------------------ Defines ---------------------------------------- */

/*!
 * @brief Disp task signal used for RG VBLANK signal.
 */
#define DISP_SIGNAL_EVTID               (DISP2UNIT_EVT__COUNT + 0U)
enum
{
    DISP_SIGNAL_RG_VBLANK,
    DISP_SIGNAL_LAST_DATA
};

/*!
 * @brief PBI task event used for interrupt from PBI.
 */
#define DISP_PBI_MODESET_EVTID          (DISP2UNIT_EVT__COUNT + 1U)
#define DISP_PBI_SR_EXIT_EVTID          (DISP2UNIT_EVT__COUNT + 2U)

#define LW_DISP_PMU_REG_POLL_RG_STATUS_VBLNK_TIMEOUT_NS  (35U * 1000U * 1000U) // 35 ms

/*!
 * Enumeration for various SR Entry substates.
 */
enum
{
    DISP_SR_ENTRY_SUBSTATE_IDLE,
    DISP_SR_ENTRY_SUBSTATE_WAITING_FOR_VBLANK,
    DISP_SR_ENTRY_SUBSTATE_TRIGGERED,
    DISP_SR_ENTRY_SUBSTATE_DONE
} DISP_SR_ENTRY_SUBSTATE;

/*!
 * Enumeration for various lane counts.
 */
typedef enum
{
    laneCount_0         = 0x0,
    laneCount_1         = 0x1,
    laneCount_2         = 0x2,
    laneCount_4         = 0x4,
    laneCount_8         = 0x8,
    laneCount_Supported
} DP_LANE_COUNT;

/*!
 * Enumeration of DISP task's PMU_OS_TIMER callback entries.
 */
typedef enum
{
    /*!
     * Entry for 1Hz DISP callbacks
     */
    DISP_OS_TIMER_ENTRY_1HZ_CALLBACK,

    /*!
     * Must always be the last entry
     */
    DISP_OS_TIMER_ENTRY_NUM_ENTRIES
} DISPM_OS_TIMER_ENTRIES;

/* ------------------------ Types definitions ------------------------------ */
/*!
 * @brief Oneshot context information for the RM based oneshot state machine.
 */
typedef struct
{
    // Oneshot mode is enabled
    LwBool bActive;

    // Oneshot mode is allowed by RM
    LwBool bRmAllow;

    // NisoMscg state
    LwBool bNisoMsActive;

    // ACK for PBI command is pending
    LwBool bPbiAckPending;

    struct
    {
        // Head Index
        LwU8  idx;

        // Trapping channel number
        LwU32 trappingChan;
    } head;
} DISP_RM_ONESHOT_CONTEXT;

/*!
 * Flat panel info which contains all necessary data for backlight control.
 * PWM and luminance map info will be followed by pointer of luminance map.
 */
typedef struct
{
    /*!
     * Common RM/PMU data.
     */
    RM_PMU_DISP_BACKLIGHT_FP_ENTRY fpEntry;

    /*! Pointer of PWM info data */
    RM_PMU_DISP_BACKLIGHT_PWM_INFO *pBacklightPwmInfo;

    /*! Pointer of luminance map */
    RM_PMU_DISP_BACKLIGHT_LUMINANCE_ENTRY *pLuminanceMap;
} DISP_BACKLIGHT_FP_ENTRY;

/*!
 * @brief Context information for the Disp task.
 */
typedef struct
{
    /*! Indicate if PMU backlight control is ready for use. */
    LwBool  bInitialized;

    /*! Indicate if it needs to update PWM registers in vblank. */
    LwBool  bUpdateBrightness;

    /*! Indicate if it needs to lock updating PWM registers in vblank. */
    LwBool  bLockBrightness;

    /*! Indicate if vblank event is queued. */
    LwBool  bVblankEventQueued;

    /*! Flat panel info entry. */
    DISP_BACKLIGHT_FP_ENTRY *pFpEntry;

    /*! Crystal clock frequency on a ext. device. */
    LwU32 crystalClkFreqHz;

    /*! Last PWM_DIV register value. */
    LwU32 lastPwmDiv;

    /*! Last PWM_CTRL register value. */
    LwU32 lastPwmCtl;

    /*! Last brightness level. */
    LwU16 lastBrightnessLevel;

    /*! Line number of line that was scanning out when LAST_DATA interrupt was received */
    LwU16 lastDataLineCnt;

    /*! Frame time for disabling Vblank MSCG WAR Bug 200637376 */
    LwU32 frameTimeVblankMscgDisableUs;

    /*! Indicate if Mclk switch sequence owns frame timer */
    LwBool bMclkSwitchOwnsFrameCounter;

    /*! RM based Oneshot context */
    DISP_RM_ONESHOT_CONTEXT    *pRmOneshotCtx;
} DISP_CONTEXT;

/*!
 * @brief Signal structure used for signals (RG_VBLANK and oneshot signals) to Disp task.
 */
typedef struct
{
    DISP2UNIT_HDR   hdr;    //<! Must be first element of the structure

    LwU8            signalType;   /*!< DISP_SIGNAL_* */
} DISPATCH_SIGNAL_DISP;

typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;

    DISPATCH_SIGNAL_DISP    signal;
} DISPATCH_DISP;

/*!
 * For some brightness callwlations, we will use unsigned 10.22 fixed point
 * values to minimize errors caused by integer division.
 */
typedef LwUFXP10_22 DISP_BRIGHTC_BRIGHTNESS_FXP;
typedef LwUFXP10_22 DISP_BRIGHTC_DUTY_CYCLE_FXP;

/*!
 * Used by both brightness and duty cycle, since they are both 10.22 in fxp
 * format, and both U16 in integer format.  We will need separate macros if
 * we ever change them to different types.
 */
#define DISP_BRIGHTC_TO_FXP(value) \
    LW_TYPES_U32_TO_UFXP_X_Y(10, 22, value)
#define DISP_BRIGHTC_FROM_FXP(fxpValue) \
    ((LwU16) (LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(10, 22, fxpValue)))

extern void dispReceiveRGVblankISR(LwU8 headIndex)
    GCC_ATTRIB_SECTION("imem_resident", "dispReceiveRGVblankISR");

/* ------------------------ Global Variables ------------------------------- */
extern OS_TIMER                             DispOsTimer;
extern DISP_CONTEXT                         DispContext;
extern RM_PMU_DISP_IMP_MCLK_SWITCH_SETTINGS DispImpMclkSwitchSettings;

#endif // PMU_DISP_H
