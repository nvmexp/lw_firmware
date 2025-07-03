/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_objdpu.c
 * @brief  General-purpose chip-specific routines which do not exactly fit
 *         within the bounds a particular engine in the GPU.
 *
 * @section _dpu_objdpu OBJDPU Notes
 * DPU object is created to serve general purpose chip specific routines that
 * can't be cleanly handled in task* codes without getting tangled up with
 * multiple includes of the same #defines.
 *
 * An engine specific code should still be handled in each engine hal layer,
 * but all other DPU features varying from chip to chip should all go in here.
 *
 * Ideally, we do not want to directly refer to chip specific #defines in
 * the main dpu app source codes.
 * @endsection
 */

/* ------------------------ System includes ------------------------------- */
#include "dpusw.h"

/* ------------------------ Application includes --------------------------- */
#include "lwostimer.h"
#include "dpu_objhal.h"
#include "dpu_objdpu.h"
#include "dpu_objic.h"
#include "dpu_task.h"
#include "dpu_mgmt.h"
#include "dispflcn_regmacros.h"
#include "lib_intfchdcp_cmn.h"

/* ------------------------ External definitions --------------------------- */
#ifndef GSPLITE_RTOS
extern LwU32 gHdcpOverlayMapping[HDCP_OVERLAYID_MAX];
void libIntfcHdcpAttachAndLoadOverlay(HDCP_OVERLAY_ID overlayId)
    GCC_ATTRIB_SECTION("imem_hdcpCmn", "libIntfcHdcpAttachAndLoadOverlay");
void libIntfcHdcpDetachOverlay(HDCP_OVERLAY_ID overlayId)
    GCC_ATTRIB_SECTION("imem_hdcpCmn", "libIntfcHdcpDetachOverlay");
#endif

/* ------------------------ Global variables ------------------------------- */
OBJDPU         Dpu;

//
// We need DPUJOB_RTTIMER_TEST to be defined in Features.pm so that the
// corresponding PDB_PROP_DPUJOB_RTTIMER_TEST can be generated for RM usage.
// But we also need this flag to be used in library, however DPUJOB_RTTIMER_TEST
// in Features.pm can't take effect in library scope. So another flag in
// dpu-profiiles.mk, "RTTIMER_TEST_LIB", is defined for library usage. Since we
// have two flags for same usage, so using below compile time check to ensure
// these two flags will be consistent.
//
// Besides, we also need to ensure  DPURTTIMER_FOR_OS_TICKS is not enabled when
// we want to enable DPUJOB_RTTIMER_TEST.
//
#if (defined(RTTIMER_TEST_LIB) && RTTIMER_TEST_LIB)
    ct_assert(DPUCFG_FEATURE_ENABLED(DPUJOB_RTTIMER_TEST));
    ct_assert(!DPUCFG_FEATURE_ENABLED(DPURTTIMER_FOR_OS_TICKS));
#else
    ct_assert(!DPUCFG_FEATURE_ENABLED(DPUJOB_RTTIMER_TEST));
#endif

/*!
 * Construct the DPU object.  This sets up the HAL interface used by the DPU
 * module.
 */
void
constructDpu(void)
{
    IfaceSetup->dpuHalIfacesSetupFn(&Dpu.hal);
}

/*!
 * Start the timer used for the OS scheduler
 *
 * Notes:
 *     - This function MUST be called PRIOR to starting the scheduler.
 *     - @ref icInit_HAL should be called before calling this function
 *       to set up the DPU interrupts for the timer.
 *
 * @return FLCN_OK  if succeed else error.
 */
FLCN_STATUS
dpuStartOsTicksTimer(void)
{
    FLCN_STATUS status = FLCN_OK;

#if (DPUCFG_FEATURE_ENABLED(DPUTIMER0_FOR_OS_TICKS))
    {
        icSetupTimer0Intr_HAL(&IcHal, LW_TRUE, LWRTOS_TICK_PERIOD_US, LW_TRUE,
                              LW_FALSE);
    }
#elif (DPUCFG_FEATURE_ENABLED(DPURTTIMER_FOR_OS_TICKS))
    {
        status = icSetupRttimerIntr_HAL(&IcHal, LW_TRUE, LWRTOS_TICK_PERIOD_US, LW_TRUE,
                                        LW_FALSE);
        if (status != FLCN_OK)
        {
            return status;
        }

    #if defined(GSPLITE_RTTIMER_WAR_ENABLED)
        //
        // Bug 200348935 that Volta RTTimer CG cause timer doesn't work correctly.
        // The WAR is to enable GPTimer together that avoid it enters low power state.
        //

        // General-purpose timer setup (used by the scheduler)
        DFLCN_REG_WR_NUM(GPTMRINT, _VAL, (DpuInitArgs.cpuFreqHz / LWRTOS_TICK_RATE_HZ - 1));

        DFLCN_REG_WR_NUM(GPTMRVAL, _VAL, (DpuInitArgs.cpuFreqHz / LWRTOS_TICK_RATE_HZ - 1));

        // Enable the general purpose timer
        DFLCN_REG_WR32(GPTMRCTL, DFLCN_DRF_SHIFTMASK(GPTMRCTL_GPTMREN));
    #endif
    }
#else
    {
        // General-purpose timer setup (used by the scheduler)
        DFLCN_REG_WR_NUM(GPTMRINT, _VAL, (DpuInitArgs.cpuFreqHz / LWRTOS_TICK_RATE_HZ - 1));

        DFLCN_REG_WR_NUM(GPTMRVAL, _VAL, (DpuInitArgs.cpuFreqHz / LWRTOS_TICK_RATE_HZ - 1));

        // Enable the general purpose timer
        DFLCN_REG_WR32(GPTMRCTL, DFLCN_DRF_SHIFTMASK(GPTMRCTL_GPTMREN));
    }
#endif

    return status;
}

#ifndef GSPLITE_RTOS
/*!
 * @brief This is a library interface function for attaching/loading overlay.
 *
 * @param[in]  overlayId  Overlay Id wants to attach and load.
 */
void
libIntfcHdcpAttachAndLoadOverlay
(
    HDCP_OVERLAY_ID overlayId
)
{
    if (overlayId < HDCP_OVERLAYID_MAX)
    {
        if (gHdcpOverlayMapping[overlayId] != ILWALID_OVERLAY_INDEX)
        {
            OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(gHdcpOverlayMapping[overlayId]);
        }
        else
        {
            //
            // Purposely discard ILWALID_OVERLAY_INDEX overlay attach, e.g. 
            // HDCP22WIRED_TIMER overlay.
            //
        }
    }
    else
    {
        // TODO: write error code to mailbox reg after thorough dislwssion.
        DPU_HALT();
    }
}

/*!
 * @brief This is a library interface function for detaching overlay.
 *
 * @param[in]  overlayId  Overlay Id wants to detach.
 */
void
libIntfcHdcpDetachOverlay
(
    HDCP_OVERLAY_ID overlayId
)
{
    if (overlayId < HDCP_OVERLAYID_MAX)
    {
        if (gHdcpOverlayMapping[overlayId] != ILWALID_OVERLAY_INDEX)
        {
            OSTASK_DETACH_IMEM_OVERLAY(gHdcpOverlayMapping[overlayId]);
        }
        else
        {
            //
            // Purposely discard ILWALID_OVERLAY_INDEX overlay detach, e.g. 
            // HDCP22WIRED_TIMER overlay.
            //
        }
    }
    else
    {
        // TODO: write error code to mailbox reg after thorough dislwssion.
        DPU_HALT();
    }
}
#endif

