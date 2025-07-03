/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2008-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJIC_H
#define PMU_OBJIC_H

/*!
 * @file    pmu_objic.h
 * @copydoc pmu_objic.c
 */

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"

/* ------------------------ Application includes --------------------------- */
#include "pmu_didle.h"
#include "pmu_fbflcn_if.h"

#include "config/g_ic_hal.h"

/* ------------------------ Types definitions ------------------------------ */
typedef void (*IC_SERVICE_ROUTINE)(void);

/* ------------------------ External definitions --------------------------- */
/*!
 * IC object Definition
 */
typedef struct
{
    LwU8    dummy;    // unused -- for compilation purposes only
} OBJIC;

extern OBJIC Ic;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
extern LwBool IcCtxSwitchReq;
/* ------------------------ Macros & Defines ------------------------------- */
/*!
 * Utility macro used to determine if an interrupt is pending for a specific
 * GPIO in the first GPIO bank. This macro may be used to check for both rising-
 * and falling- edge interrupts.  Returns a non-zero value if the interrupt is
 * pending; zero if NOT pending.
 *
 * @param[in]  g  The name of the specific GPIO to check
 * @param[in]  e  The edge (_RISING/_FALLING) of interest
 * @param[in]  v  The current value of GPIO interrupt register (passed-in to
 *                avoid unnecessary re-reading).
 */
#define PENDING_IO_BANK0(g,e,v)                                               \
    FLD_TEST_DRF(_CPWR_PMU, _GPIO_INTR##e  , g, _PENDING, (v))

/*!
 * Utility macro used to determine if an interrupt is pending for a specific
 * GPIO in the second GPIO bank. This macro may be used to check for both
 * rising- and falling- edge interrupts.  Returns a non-zero value if the
 * interrupt is pending; zero if NOT pending.
 *
 * @param[in]  g  The name of the specific GPIO to check
 * @param[in]  e  The edge (_RISING/_FALLING) of interest
 * @param[in]  v  The current value of GPIO interrupt register (passed-in to
 *                avoid unnecessary re-reading).
 */
#define PENDING_IO_BANK1(g,e,v)                                               \
    FLD_TEST_DRF(_CPWR_PMU, _GPIO_1_INTR##e, g, _PENDING, (v))

/*!
 * Utility macro used to determine if an interrupt is pending for a specific
 * GPIO in the second GPIO bank. This macro may be used to check for high- and
 * low- level interrupts, rising- and falling- edge interrupts. Returns a
 * non-zero value if the interrupt is pending; zero if NOT pending.
 *
 * @param[in]  g  The name of the specific GPIO to check
 * @param[in]  e  The level(_HIGH/_LOW) or edge (_RISING/_FALLING) of interest
 * @param[in]  v  The current value of GPIO interrupt register (passed-in to
 *                avoid unnecessary re-reading).
 */
#define PENDING_IO_BANK2(g,e,v)                                               \
    FLD_TEST_DRF(_CPWR_PMU, _GPIO_2_INTR##e, g, _PENDING, (v))

/*!
 * @brief Utility macro used to determine if an interrupt is pending for
 * Lane Margining. Returns a non-zero value if the interrupt is pending;
 * zero if NOT pending.
 *
 * @param[in] v Current value of LM interrupt register (passed-in to
 *              avoid unnecessary re-reading)
 */
#define PENDING_LANE_MARGINING(v)                                             \
    FLD_TEST_DRF(_CPWR, _FALCON_IRQSTAT, _EXT_RSVD8, _TRUE, (v))

/*!
 * The PMU's physical queue ID used for incoming traffic from the FSP. Move this
 * to some FSP specific header file in the future.
 */
#define PMU_CMD_QUEUE_FSP_RPC_MESSAGE 3U

/*!
 * @brief   IC_CMD_INTREN_SETTINGS - a mask of enabled CMD queue interrupts
 *
 * HEAD_0 interrupt is always enabled
 *  - RM->PMU HPQ (prior to the FBQ per-task command buffer)
 *  - Only RM->PMU queue for the FBQ per-task command buffer
 *
 * HEAD_1 interrupt is enabled only prior to FBQ PTCB
 *  - RM->PMU LPQ (prior to the FBQ per-task command buffer)
 *
 * HEAD_2 interrupt is enabled only when FBFLCN is enabled
 */
#define IC_CMD_INTREN_HEAD_0 \
    (LWBIT32(RM_PMU_COMMAND_QUEUE_HPQ))
#define IC_CMD_INTREN_HEAD_1 \
    (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER) ? \
        0U : LWBIT32(RM_PMU_COMMAND_QUEUE_LPQ))
#define IC_CMD_INTREN_HEAD_2 \
    (PMUCFG_FEATURE_ENABLED(PMU_CLK_FBFLCN_SUPPORTED) ? \
        LWBIT32(PMU_CMD_QUEUE_IDX_FBFLCN) : 0U)
#define IC_CMD_INTREN_HEAD_3 \
    (PMUCFG_FEATURE_ENABLED(PMU_FSP_RPC) ? \
        LWBIT32(PMU_CMD_QUEUE_FSP_RPC_MESSAGE) : 0U)
#define IC_CMD_INTREN_SETTINGS \
    (IC_CMD_INTREN_HEAD_0 | \
     IC_CMD_INTREN_HEAD_1 | \
     IC_CMD_INTREN_HEAD_2 | \
     IC_CMD_INTREN_HEAD_3)

/* ------------------------ Function Prototypes ---------------------------- */

#endif // PMU_OBJIC_H
