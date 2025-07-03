/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pglogicovl.c
 * @brief  Handles Task2 overlay logic.
 *
 * OVERLAYS IN TASK2:
 * pg           : This overlay contains common functionalities handled by task 2.
 *                Its by default attached to task2 while creating the task.
 *
 * lpwrLoadin   : This overlay contains initialization and one time exelwtion
 *                functionalities of Task2.
 *
 * libPsi       : This overlay contains PSI (Phase State Indicator) related
 *                functionality. It contains entry and exit sequence of static
 *                and current aware PSI.
 *
 * libAp        : This overlay contains Adaptive Power related functionality.
 *                It contains Idle threshold prediction algorithm for Adaptive
 *                PWR.
 *
 * clkLibCntr & clkLibCntrSwSync
 *              : This overlay handles HW clock counters. Task2 has two
 *                uses cases for HW clock counters -
 *                1) Implement WAR #801614 for GR-ELPG
 *                2) Synchronize clock counters with MSCG
 *
 * libGr        : This overlay contains GR-ELPG entry and exit sequence.
 *
 * libFifo      : This overlay contains Channel and FIFO related functionality.
 *                libGr uses this overlay to do FIFO related stuff(Flush
 *                channel) in ELPG sequence.
 *
 * libMs        : This overlay contains complete MSCG entry and exit sequence.
 *
 * libDi        : This overlay contains PG_ENG DI entry and exit sequence.
 *
 * libSyncGpio  : This overlay contains SyncGpio related functions.
 *
 * OVERLAY LOADING POLICIES
 *
 * OVERLAY LOADING POLICIES FOR PG_CTRLS:
 * We have following policies to load GR, MS and DI overlays.
 *
 * 1) GR and MS mutual exclusion:
 *    Policy make sure that GR and MS overlays are never attached at same time.
 *    Refer @ pgLogicOvlPre() pgLogicOvlPost() for more details.
 *
 * 2) MS and DI at same time:
 *    This policy is meant for libDi. libDi follows same polices as that of
 *    libMs.
 *
 * OVERLAY LOADING POLICY: lpwrLoadin
 *    Detach GR and MS related overlays before loading lpwrLoadin.
 *
 *    We load lpwrLoadin overlay into IMEM as per the need during PMU
 *    initialization and destruction time. We don't need GR and MS overlays
 *    while exelwting ucode from lpwrLoadin overlay.Thus, To have efficient
 *    use of IMEM we unload GR and MS overlays before loading lpwrLoadin
 *    into IMEM.
 *
 * OVERLAY LOADING POLICY: FOREVER
 *    libLpwr, libAp and libRmOneshot overlays are permanently attached to
 *    LPWR task on lpwrPostInit()
 *
 * OVERLAY LOADING POLICY: PSI
 *    This policy is meant for libPsi. This library is used while engaging and
 *    disengaging PSI. 
 *    - This policy is aligned with its parent's policy (GR and MS).
 *
 * OVERLAY LOADING POLICY: Sync GPIO
 *    This policy is meant for libSyncGpio. This library is used by both DI and
 *    GC6 
 *    - For DI, we allign its policy with libDi (i.e. attach/detach it whenever
 *      we attach/detach libDi)
 *    - For GC6, We attach it before calling related functions and detach it
 *      aftwerwards
 *
 * OVERLAY LOADING POLICY: LCK counter overlays
 *    This policy is meant for clkLibCntr and clkLibCntrSwSync
 *    - clkLibCntrSwSync - It is aligned with both MS and GR overlay policy
 *    - clkLibCntr       - It is aligned with GR overlay policy
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#ifdef UPROC_RISCV
#include "syslib/syslib.h"
#endif
/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "dbgprintf.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_objms.h"
#include "pmu_objdi.h"
#include "pmu_objlpwr.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief Macros to manage overlay operations.
 */
#define PG_OVL_IS_ATTACHED(ovlIdMask)                                         \
            ((Pg.olvAttachedMask & (ovlIdMask)) != 0)
#define PG_OVL_ATTACH_MASK_SET(olvIdMask)                                     \
            Pg.olvAttachedMask |= (olvIdMask)
#define PG_OVL_ATTACH_MASK_CLEAR(olvIdMask)                                   \
            Pg.olvAttachedMask &= ~(olvIdMask)

/* ------------------------- Prototypes ------------------------------------- */
static LwBool s_pgOvlerayAttachLpwrLoadin(void);
static void   s_pgOvlerayDetachLpwrLoadin(void);
static LwBool s_pgOverlayAttachGr(void);
static void   s_pgOverlayDetachGr(void);
static LwBool s_pgOverlayAttachMs(void);
static void   s_pgOverlayDetachMs(void);
#ifdef UPROC_RISCV
static void   s_pgOverlayAttachMsRiscv(void);
static void   s_pgOverlayDetachMsRiscv(void);
#else
static void   s_pgOverlayAttachMsFalcon(void);
static void   s_pgOverlayDetachMsFalcon(void);
#endif
static LwBool s_pgOverlayAttachPsi(void);
static void   s_pgOverlayDetachPsi(void);
static LwBool s_pgOverlayAttachForever(void);
static LwBool s_pgOverlayLoadSanity(void);
static void   s_pgOverlaySyncGpioAttach(void);

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief PG Logic to overlay policies PRE
 *
 * It implements GR<->MS mutual exclusion policy. We support multiple GR parent
 * features as part of multi-FSM design. Feature follows these guidelines -
 * 1) Only one GR feature will be resident at given time
 * 2) Entry and exit sequence for different GR features will not overall
 * 3) MS entry is possible when any GR feature is engaged
 *
 * Sequence of events:
 * PG_ON(GR)        to POWERED_DOWN(GR): GR overlay required
 * POWERED_DOWN(GR) to POWERING_UP(GR) : GR overlay NOT required
 * POWERING_UP(GR)  to POWERED_UP(GR)  : GR overlay required
 * POWERED_UP(GR)   to PG_ON(GR)       : GR overlay NOT required
 *
 * PG_ON(MS)        to POWERED_DOWN(MS): MS overlay required
 * POWERED_DOWN(MS) to POWERING_UP(MS) : MS overlay NOT required
 * POWERING_UP(MS)  to POWERED_UP(MS)  : MS overlay required
 * POWERED_UP(MS)   to PG_ON(MS)       : MS overlay NOT required
 *
 * Overlay loading policy:
 *
 * - PG_ON(GR):
 *   GR ovl is loaded.
 *
 * - POWERED_DOWN(GR)
 *   GR olv is detached. MS ovl is loaded.
 *
 * - PG_ON(MS)
 *   Load MS ovl if its not already loaded. This will cover GR decoupled MS.
 *
 * - POWERED_DOWN(MS)
 * - POWERING_UP(MS)
 * - POWERED_UP(MS)
 *   No changes in overlay status
 *
 * - POWERING_UP(GR)
 *   MS ovl is detached. GR ovl is loaded.
 *
 * - POWERED_UP(GR)
 *   GR olv is detached.
 *
 * @param[in]   pPgLogicState   Pointer to PG Logic state
 */
void
pgLogicOvlPre
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    if (lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_GR, pPgLogicState->ctrlId))
    {
        switch (pPgLogicState->eventId)
        {
            case PMU_PG_EVENT_PG_ON:
            {
                pgOverlayAttachAndLoad(PG_OVL_ID_MASK_GR);
                break;
            }
            case PMU_PG_EVENT_POWERING_UP:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS))
                {
                    pgOverlayDetach(PG_OVL_ID_MASK_MS);
                }
                pgOverlayAttachAndLoad(PG_OVL_ID_MASK_GR);
                break;
            }
        }
    }

    if (lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_MS, pPgLogicState->ctrlId) &&
        (pPgLogicState->eventId == PMU_PG_EVENT_PG_ON))
    {
        pgOverlayAttachAndLoad(PG_OVL_ID_MASK_MS);
    }
}

/*!
 * @brief PG Logic to overlay policies POST
 *
 * It implements GR<->MS mutual exclusion policy. Refer at pgLogicOvlPost()
 * for more details.
 *
 * @param[in]   pPgLogicState   Pointer to PG Logic state
 */
void
pgLogicOvlPost
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    if (lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_GR, pPgLogicState->ctrlId))
    {
        switch (pPgLogicState->eventId)
        {
            case PMU_PG_EVENT_POWERED_DOWN:
            {
                pgOverlayDetach(PG_OVL_ID_MASK_GR);
                if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS))
                {
                    pgOverlayAttachAndLoad(PG_OVL_ID_MASK_MS);
                }
                break;
            }
            case PMU_PG_EVENT_POWERED_UP:
            {
                pgOverlayDetach(PG_OVL_ID_MASK_GR);
                break;
            }
        }
    }
}

/*!
 * @brief PG API to attach and load any overlay
 *
 * @param[in]   ovlIdMask   Overlay ID PG_OVL_ID_MASK_*
 */
void
pgOverlayAttachAndLoad(LwU32 ovlIdMask)
{
    if (pgOverlayAttach(ovlIdMask))
    {
        lwrtosYIELD();
    }
}

/*!
 * @brief API to attach any overlay to PG task
 *
 * @param[in]   ovlIdMask   Overlay ID PG_OVL_ID_MASK_*
 *
 * @return      LW_TRUE     Task Yield is required to load overlay
 *              LW_FALSE    Task Yield is not required as overlay is
 *                          not attached to task
 */
LwBool
pgOverlayAttach(LwU32 ovlIdMask)
{
    LwBool bAttached = LW_FALSE;

    if (PG_OVL_IS_ATTACHED(ovlIdMask))
    {
        return LW_FALSE;
    }

    PMU_HALT_COND(s_pgOverlayLoadSanity());

    if (ovlIdMask == PG_OVL_ID_MASK_FOREVER)
    {
        bAttached = s_pgOverlayAttachForever();
    }
    else if (ovlIdMask == PG_OVL_ID_MASK_LPWR_LOADIN)
    {
        bAttached = s_pgOvlerayAttachLpwrLoadin();
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_GR)) &&
             (ovlIdMask == PG_OVL_ID_MASK_GR))
    {
        bAttached = s_pgOverlayAttachGr();
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS)) &&
             (ovlIdMask == PG_OVL_ID_MASK_MS))
    {
        bAttached = s_pgOverlayAttachMs();
    }
    else
    {
        PMU_BREAKPOINT();
    }

    if (bAttached)
    {
        PG_OVL_ATTACH_MASK_SET(ovlIdMask);
    }

    return bAttached;
}

/*!
 * @brief API to detach any overlay from PG task
 *
 * @param[in]   ovlIdMask   Overlay ID - PG_OVL_ID_MASK_*
 */
void
pgOverlayDetach(LwU32 ovlIdMask)
{
    // Return early if overlay is already attached
    if (!PG_OVL_IS_ATTACHED(ovlIdMask))
    {
        return;
    }

    if (ovlIdMask == PG_OVL_ID_MASK_LPWR_LOADIN)
    {
        s_pgOvlerayDetachLpwrLoadin();
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_GR)) &&
             (ovlIdMask == PG_OVL_ID_MASK_GR))
    {
        s_pgOverlayDetachGr();
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS)) &&
             (ovlIdMask == PG_OVL_ID_MASK_MS))
    {
        s_pgOverlayDetachMs();
    }
    else
    {
        PMU_BREAKPOINT();
    }

    PG_OVL_ATTACH_MASK_CLEAR(ovlIdMask);
}

/*!
 * @brief Helper function to attach and load SYNCGPIO overlay
 */
void
pgOverlaySyncGpioAttachAndLoad()
{
    s_pgOverlaySyncGpioAttach();
    lwrtosYIELD();
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Helper function to permanently attach the overs to LPWR task
 *
 * @return  LW_TRUE     if overlay attached is to task
 *          LW_FALSE    otherwise
 */
static LwBool
s_pgOverlayAttachForever(void)
{
    // Attach libLpwr overlay
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_PMU_ONLY))
    {
        OSTASK_ATTACH_PINNED_IMEM_OVERLAY(OVL_INDEX_IMEM(libLpwr));
    }

    // Attach overlay required for Adaptive PWR
    if (PMUCFG_FEATURE_ENABLED(PMU_AP))
    {
        OSTASK_ATTACH_PINNED_IMEM_OVERLAY(OVL_INDEX_IMEM(libAp));
    }

    // Attach overlay required for OSM feature
    if (PMUCFG_FEATURE_ENABLED(PMU_MS_OSM))
    {
        OSTASK_ATTACH_PINNED_IMEM_OVERLAY(OVL_INDEX_IMEM(libRmOneshot));
    }

    return LW_TRUE;
}

/*!
 * @brief Helper function to attach lpwrLoadin overlay
 *
 * Follow these steps:
 * 1) Detach GR Overlays
 * 2) Detach MS overlays
 * 3) Attach lpwrLoadin overlay
 *
 * @return  LW_TRUE     if overlay attached is to task
 *          LW_FALSE    otherwise
 */
static LwBool
s_pgOvlerayAttachLpwrLoadin(void)
{
    // Step 1) Detach GR overlays
    if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_GR)) &&
        (PG_OVL_IS_ATTACHED(PG_OVL_ID_MASK_GR)))
    {
        s_pgOverlayDetachGr();
    }

    // Step 2) Detach MS overlays
    if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS)) &&
        PG_OVL_IS_ATTACHED(PG_OVL_ID_MASK_MS))
    {
        s_pgOverlayDetachMs();
    }

    // Step 3) Attach targeted overlay
    OSTASK_ATTACH_IMEM_OVERLAY(OVL_INDEX_IMEM(lpwrLoadin));

    return LW_TRUE;
}

/*!
 * @brief Helper function to detach lpwrLoadin overlay
 *
 * Follow the steps in reverse order of _pgOverlayAttachLpwrLoadin().
 */
static void
s_pgOvlerayDetachLpwrLoadin(void)
{
    // Step 1) Detach targeted overlay
     OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(lpwrLoadin));

     // Step 2) Attach MS overlays
    if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS)) &&
        PG_OVL_IS_ATTACHED(PG_OVL_ID_MASK_MS))
    {
        s_pgOverlayAttachMs();
    }

    // Step 3) Attach GR overlays
    if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_GR)) &&
        (PG_OVL_IS_ATTACHED(PG_OVL_ID_MASK_GR)))
    {
        s_pgOverlayAttachGr();
    }

    // Step 4) Yield the task to load all attached overlays into IMEM.
    lwrtosYIELD();
}

/*!
 * @brief Helper function to attach GR overlays
 *
 * @return  LW_TRUE     if overlay attached is to task
 *          LW_FALSE    otherwise
 */
static LwBool
s_pgOverlayAttachGr(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    // Do not load overlays if no GR feature is supported
    if (!lpwrGrpIsCtrlSupported(RM_PMU_LPWR_GRP_CTRL_ID_GR))
    {
        return LW_FALSE;
    }

    //
    // MS should not be attached to LPWR task as we support GR<->MS Exclusion
    // policy.
    //
    PMU_HALT_COND(!PG_OVL_IS_ATTACHED(PG_OVL_ID_MASK_MS));

    OSTASK_ATTACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libGr));

    // Attach PSI overlay if GR coupled PSI is supported
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_GR)      &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_GR_PG) &&
        LPWR_CTRL_IS_SF_SUPPORTED(pGrState, GR, PSI))
    {
        s_pgOverlayAttachPsi();
    }

    return LW_TRUE;
}

/*!
 * @brief Helper function to detach GR overlays
 */
static void
s_pgOverlayDetachGr(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libGr));

    // Detach PSI overlay if GR coupled PSI is supported
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_GR)      &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_GR_PG) &&
        LPWR_CTRL_IS_SF_SUPPORTED(pGrState, GR, PSI))
    {
        s_pgOverlayDetachPsi();
    }
}

/*!
 * @brief Helper function to attach MS overlays
 *
 * @return  LW_TRUE     if overlay attached is to task
 *          LW_FALSE    otherwise
 */
static LwBool
s_pgOverlayAttachMs(void)
{

    // Do not load overlays if no MS feature is supported
    if (!lpwrGrpIsCtrlSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS))
    {
        return LW_FALSE;
    }

#ifdef UPROC_RISCV
    s_pgOverlayAttachMsRiscv();
#else
    s_pgOverlayAttachMsFalcon();
#endif

    return LW_TRUE;
}

/*!
 * @brief Helper function to detach MS overlays
 */
static void
s_pgOverlayDetachMs(void)
{
    // Do not detach overlays if no MS feature is supported
    if (!lpwrGrpIsCtrlSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS))
    {
        return;
    }

#ifdef UPROC_RISCV
    s_pgOverlayDetachMsRiscv();
#else
    s_pgOverlayDetachMsFalcon();
#endif
}

#ifdef UPROC_RISCV

/*!
 * @brief Helper function to attach MS code section on Riscv
 *
 */
static void
s_pgOverlayAttachMsRiscv(void)
{
    FLCN_STATUS status = sysOdpPinSection(OVL_INDEX_IMEM(lpwrPinnedCode));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
}

/*!
 * @brief Helper function to detach MS code on Falcon
 */
static void
s_pgOverlayDetachMsRiscv(void)
{
    FLCN_STATUS status = sysOdpUnpinSection(OVL_INDEX_IMEM(lpwrPinnedCode));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
}
#else

/*!
 * @brief Helper function to attach MS code section on Falcon
 *
 */
static void
s_pgOverlayAttachMsFalcon(void)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    //
    // GR should not be attached to LPWR task as we support GR<->MS exclusion
    // policy.
    //
    PMU_HALT_COND(!PG_OVL_IS_ATTACHED(PG_OVL_ID_MASK_GR));

    OSTASK_ATTACH_PINNED_IMEM_OVERLAY(OVL_INDEX_IMEM(libMs));

    // Attach PSI overlay if MS or DI coupled PSI is supported
    if ((PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS)        &&
         pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG) &&
         LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, PSI))  ||
        (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_DI)        &&
         PG_IS_SF_SUPPORTED(Di, DI, PSI)))
    {
        s_pgOverlayAttachPsi();
    }

    //
    // Attach clkLibCntrSwSync overlay to sync clock counters.
    // Refer at clkCntrAccessSync()
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR))
    {
        OSTASK_ATTACH_PINNED_IMEM_OVERLAY(OVL_INDEX_IMEM(clkLibCntrSwSync));
    }

    //
    // Attach libClkVolt overlay to sync ADC counters.
    // Refer at clkAdcHwAccessSync()
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_VOLTAGE_SAMPLE_CALLBACK))
    {
        OSTASK_ATTACH_PINNED_IMEM_OVERLAY(OVL_INDEX_IMEM(libClkVolt));
    }
}

/*!
 * @brief Helper function to detach MS code on Falcon
 */
static void
s_pgOverlayDetachMsFalcon(void)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    OSTASK_DETACH_PINNED_IMEM_OVERLAY(OVL_INDEX_IMEM(libMs));

    // Detach PSI overlay if MS or DI coupled PSI is supported
    if ((PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS)        &&
         pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG) &&
         LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, PSI))  ||
        (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_DI)        &&
         PG_IS_SF_SUPPORTED(Di, DI, PSI)))
    {
        s_pgOverlayDetachPsi();
    }

    // Detach clkLibCntrSwSync overlay along with MS
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR))
    {
        OSTASK_DETACH_PINNED_IMEM_OVERLAY(OVL_INDEX_IMEM(clkLibCntrSwSync));
    }

    // Detach libClkVolt overlay along with MS
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_VOLTAGE_SAMPLE_CALLBACK))
    {
        OSTASK_DETACH_PINNED_IMEM_OVERLAY(OVL_INDEX_IMEM(libClkVolt));
    }
}
#endif
/*!
 * @brief Helper function to attach SYNCGPIO overlay
 */
static void
s_pgOverlaySyncGpioAttach(void)
{
    if (!PMUCFG_FEATURE_ENABLED(PMU_PGISLAND_SCI_PMGR_GPIO_SYNC))
    {
        return;
    }

    appTaskCriticalEnter();
    {
        if (!Pg.bSyncGpioAttached)
        {
            PMU_HALT_COND(s_pgOverlayLoadSanity());
            OSTASK_ATTACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSyncGpio));
            Pg.bSyncGpioAttached = LW_TRUE;
        }
    }
    appTaskCriticalExit();
}

/*!
 * @brief Helper function to detach SYNCGPIO overlay
 */
void
pgOverlaySyncGpioDetach(void)
{
    if (!PMUCFG_FEATURE_ENABLED(PMU_PGISLAND_SCI_PMGR_GPIO_SYNC))
    {
        return;
    }

    appTaskCriticalEnter();
    {
        if (Pg.bSyncGpioAttached)
        {
            OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSyncGpio));
            Pg.bSyncGpioAttached = LW_FALSE;
        }
    }
    appTaskCriticalExit();
}

/*!
 * @brief Helper function to attach PSI overlay
 *
 * @return  LW_TRUE     if overlay attached is to task
 *          LW_FALSE    otherwise
 */
static LwBool
s_pgOverlayAttachPsi(void)
{
    OSTASK_ATTACH_PINNED_IMEM_OVERLAY(OVL_INDEX_IMEM(libPsi));
    return LW_TRUE;
}

/*!
 * @brief Helper function to detach PSI overlay
 */
static void
s_pgOverlayDetachPsi(void)
{
    OSTASK_DETACH_PINNED_IMEM_OVERLAY(OVL_INDEX_IMEM(libPsi));
}

/*!
 * @brief Helper function to do the sanity check before loading overlay
 *
 * We can safely load the overlay only if MSCG is in Full Power mode.
 *
 * @return  LW_TRUE     Its safe to load the new overlay
 *          LW_FALSE    Otherwise
 */
static LwBool
s_pgOverlayLoadSanity(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS) &&
        lpwrGrpIsCtrlSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS))
    {
        return (!lpwrGrpCtrlIsEnaged(RM_PMU_LPWR_GRP_CTRL_ID_MS));
    }

    return LW_TRUE;
}

