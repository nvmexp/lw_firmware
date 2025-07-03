/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_grp.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "dbgprintf.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions  ------------------------------ */

/*!
 * @brief Initializes all LPWR_GRPs
 */
FLCN_STATUS
lpwrGrpPostInit(void)
{
    LwU32          lpwrGrpId;
    LPWR_GRP_CTRL *pLpwrGrpCtrl;

    // Initialize Group OwnerId. Its used by mutual exclusion policy.
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MUTUAL_EXCLUSION))
    {
        for (lpwrGrpId = 0; lpwrGrpId < RM_PMU_LPWR_GRP_CTRL_ID__COUNT; lpwrGrpId++)
        {
            pLpwrGrpCtrl            = LPWR_GET_GRP_CTRL(lpwrGrpId);
            pLpwrGrpCtrl->ownerMask = 0;
        }
    }

    return FLCN_OK;
}

/*!
 * @brief If the given Lpwr group Ctrl is supported
 *
 * @param[in]  lpwrGrpId   Lpwr group id
 *
 * @return LW_TRUE      if lpwrGrpId is supported
 * @return LW_FALSE     Otherwise
 */
LwBool
lpwrGrpIsCtrlSupported
(
    LwU32 lpwrGrpId
)
{
    LPWR_GRP_CTRL *pLpwrGrpCtrl = LPWR_GET_GRP_CTRL(lpwrGrpId);

    if (pLpwrGrpCtrl->lpwrCtrlMask != 0)
    {
        return LW_TRUE;
    }
    else
    {
        return LW_FALSE;
    }
}

/*!
 * @brief If the given Lpwr Ctrl is member of specified group
 *
 * @param[in]  lpwrGrpId   Lpwr group id
 * @param[in]  lpwrCtrlId  Lpwr ctrl id
 *
 * @return LW_TRUE      if lpwrCtrlId is member of lpwrGrpId group
 * @return LW_FALSE     Otherwise
 */
LwBool
lpwrGrpIsLpwrCtrlMember
(
    LwU32 lpwrGrpId,
    LwU32 lpwrCtrlId
)
{
    LPWR_GRP_CTRL *pLpwrGrpCtrl = LPWR_GET_GRP_CTRL(lpwrGrpId);

    if ((pLpwrGrpCtrl->lpwrCtrlMask & BIT(lpwrCtrlId)) != 0)
    {
        return LW_TRUE;
    }
    else
    {
        return LW_FALSE;
    }
}

/*!
 * @brief Disallow specified Lpwr group for external clients
 *
 * @param[in]  lpwrGrpId   Lpwr group id
 */
void
lpwrGrpDisallowExt
(
    LwU32 lpwrGrpId
)
{
    LPWR_GRP_CTRL *pLpwrGrpCtrl = LPWR_GET_GRP_CTRL(lpwrGrpId);
    LwU32          lpwrCtrlId   = 0;

    FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, pLpwrGrpCtrl->lpwrCtrlMask)
    {
        pgCtrlDisallowExt(lpwrCtrlId);
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Allow specified Lpwr group for external clients
 *
 * @param[in]  lpwrGrpId   Lpwr group id
 */
void
lpwrGrpAllowExt
(
    LwU32 lpwrGrpId
)
{
    LPWR_GRP_CTRL *pLpwrGrpCtrl = LPWR_GET_GRP_CTRL(lpwrGrpId);
    LwU32          lpwrCtrlId   = 0;

    FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, pLpwrGrpCtrl->lpwrCtrlMask)
    {
        pgCtrlAllowExt(lpwrCtrlId);
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Get the Lpwr Ctrl mask for the specified group
 *
 * @param[in]  lpwrGrpId   Lpwr group id
 *
 * @return     Lpwr Ctrl mask of lpwrGrpId group
 */
LwU32
lpwrGrpCtrlMaskGet
(
    LwU32 lpwrGrpId
)
{
    LPWR_GRP_CTRL *pLpwrGrpCtrl = LPWR_GET_GRP_CTRL(lpwrGrpId);

    return pLpwrGrpCtrl->lpwrCtrlMask;
}

/*!
 * @brief Get the mask of LPWR_CTRL that are disallowed by RM
 *
 * @param[in]  lpwrGrpId   Lpwr group id
 *
 * @return      Mask of LPWR_CTRL that are disallowed by RM
 */
LwU32
lpwrGrpCtrlRmDisallowMaskGet
(
    LwU32 lpwrGrpId
)
{
    LPWR_GRP_CTRL *pLpwrGrpCtrl = LPWR_GET_GRP_CTRL(lpwrGrpId);
    OBJPGSTATE    *pPgState     = NULL;
    LwU32          disallowMask = 0;
    LwU32          lpwrCtrlId   = 0;

    FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, pLpwrGrpCtrl->lpwrCtrlMask)
    {
        pPgState = GET_OBJPGSTATE(lpwrCtrlId);

        if (pPgState->bRmDisallowed)
        {
            disallowMask |= BIT(lpwrCtrlId);
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    return disallowMask;
}

/*!
 * @brief Check if the given sub-feature is supported for any Lpwr Ctrl
 *        associated with the given group
 *
 * @param[in]  lpwrGrpId    Lpwr group id
 * @param[in]  sfId         Sub-feature id
 *
 * @return LW_TRUE      if sfId is supported for any Lpwr Ctrl in the group
 * @return LW_FALSE     Otherwise
 */
LwBool
lpwrGrpCtrlIsSfSupported
(
    LwU32    lpwrGrpId,
    LwU32    sfId
)
{
    LPWR_GRP_CTRL *pLpwrGrpCtrl = LPWR_GET_GRP_CTRL(lpwrGrpId);
    LwU32          lpwrCtrlId   = 0;
    LwBool         bSfSupported = LW_FALSE;

    FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, pLpwrGrpCtrl->lpwrCtrlMask)
    {
        // Check the sub-feature support
        if (pgCtrlIsSubFeatureSupported(lpwrCtrlId, sfId))
        {
            bSfSupported = LW_TRUE;
            break;
        }

    }
    FOR_EACH_INDEX_IN_MASK_END;

    return bSfSupported;
}

/*!
 * @brief Check if the all the Lpwr Ctrl
 *        associated with the given group are engaged or not
 *
 * @param[in]  lpwrGrpId    Lpwr group id
 *
 * @return LW_TRUE      Atleast one of the LPWR_CTRL is engaged
 * @return LW_FALSE     Otherwise
 */
LwBool
lpwrGrpCtrlIsEnaged
(
    LwU32    lpwrGrpId
)
{
    LPWR_GRP_CTRL *pLpwrGrpCtrl = LPWR_GET_GRP_CTRL(lpwrGrpId);
    LwU32          lpwrCtrlId   = 0;
    LwBool         bEngaged     = LW_FALSE;

    FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, pLpwrGrpCtrl->lpwrCtrlMask)
    {
        // Check if the Feature is enaged
        if (pgIsCtrlSupported(lpwrCtrlId) &&
            !PG_IS_FULL_POWER(lpwrCtrlId))
        {
            bEngaged = LW_TRUE;
            break;
        }

    }
    FOR_EACH_INDEX_IN_MASK_END;

    return bEngaged;;
}

/*!
 * @brief Check if the all the Lpwr Ctrl
 *        associated with the given group are in Transition or not
 *
 * @param[in]  lpwrGrpId    Lpwr group id
 *
 * @return LW_TRUE      Atleast one of the LPWR_CTRL is doing transition(entry/exit)
 * @return LW_FALSE     Otherwise
 */
LwBool
lpwrGrpCtrlIsInTransition
(
    LwU32    lpwrGrpId
)
{
    LPWR_GRP_CTRL *pLpwrGrpCtrl  = LPWR_GET_GRP_CTRL(lpwrGrpId);
    LwU32          lpwrCtrlId    = 0;
    LwBool         bInTransition = LW_FALSE;

    FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, pLpwrGrpCtrl->lpwrCtrlMask)
    {
        // Check if the Feature is enaged
        if ((pgIsCtrlSupported(lpwrCtrlId)) &&
            (LPWR_CTRL_IS_IN_TRANSITION(lpwrCtrlId)))
        {
            bInTransition = LW_TRUE;
            break;
        }

    }
    FOR_EACH_INDEX_IN_MASK_END;

    return bInTransition;
}

/*!
 * @brief LPWR_GRP mutual exclusion policy
 *
 * LPWR group is consist of multiple LPWR_CTRLs. Today we allow engaging only
 * on LPWR_CTRL from LPWR group. This is ensured by mutual exclusion policy of
 * LPWR group. Mutual exclusion policy finds a group owner based on enablement
 * state of LPWR_CTRL from clients. If multiple LPWR_CTRLs are enabled then
 * uses following priority order to select the group owner:
 * GR Group
 * 1) GPC-RG
 * 2) GR-RPG
 * 3) GR-PASSIVE
 *
 * MS Group
 * 1) DIFR_SW_ASR & DIFR_CG
 * 2) MSCG
 * 3) MS_LTC
 * 4) MS_PASSIVE
 *
 * To achieve the mutual exclusion, this API disables all LPWR_CTRLs except
 * the group owner.
 *
 * For now, we have implemented policy only for GR group, but design can be
 * extended to other LPWR_GRPs.
 *
 * @param[in]  grpCtrlId            Grp Ctrl Id
 * @param[in]  clientId             Client ID - LPWR_GRP_CTRL_CLIENT_ID_xyz
 * @param[in]  clientDisallowMask   Disallow mask requested by the client
 */
void
lpwrGrpCtrlMutualExclusion
(
    LwU32 grpCtrlId,
    LwU32 clientId,
    LwU32 clientDisallowMask
)
{
    LPWR_GRP_CTRL *pLpwrGrpCtrl         = LPWR_GET_GRP_CTRL(grpCtrlId);
    LwU32          lpwrCtrlDisallowMask = 0;
    LwU32          lpwrCtrlAllowMask;
    LwU32          newOwnerMask         = 0;

    // Filterout the unsupported LpwrCtrls
    clientDisallowMask &= Pg.supportedMask;

    // Update the client cache
    pLpwrGrpCtrl->clientDisallowMask[clientId] = clientDisallowMask;

    // Find the LPWR_CTRL disallow mask
    for (clientId = 0; clientId < LPWR_GRP_CTRL_CLIENT_ID__COUNT; clientId++)
    {
        lpwrCtrlDisallowMask |= pLpwrGrpCtrl->clientDisallowMask[clientId];
    }

    // Find the allow LPWR_CTRL based on disallow mask
    lpwrCtrlAllowMask = (~lpwrCtrlDisallowMask) & pLpwrGrpCtrl->lpwrCtrlMask;

    if (grpCtrlId == RM_PMU_LPWR_GRP_CTRL_ID_GR)
    {
        //
        // Select the owner based on following priority order. Top-most feature in
        // bellow list has highest priority
        // 1) GPC-RG
        // 2) GR-RPG
        // 3) GR-PASSIVE
        //
        if ((LWBIT(RM_PMU_LPWR_CTRL_ID_GR_RG) & lpwrCtrlAllowMask) != 0)
        {
            newOwnerMask = LWBIT(RM_PMU_LPWR_CTRL_ID_GR_RG);
        }
        else if ((LWBIT(RM_PMU_LPWR_CTRL_ID_GR_PG) & lpwrCtrlAllowMask) != 0)
        {
            newOwnerMask = LWBIT(RM_PMU_LPWR_CTRL_ID_GR_PG);
        }
        else if ((LWBIT(RM_PMU_LPWR_CTRL_ID_GR_PASSIVE) & lpwrCtrlAllowMask) != 0)
        {
            newOwnerMask = LWBIT(RM_PMU_LPWR_CTRL_ID_GR_PASSIVE);
        }
    }
    else if (grpCtrlId == RM_PMU_LPWR_GRP_CTRL_ID_MS)
    {
        //
        // Select the owner based on following priority order. Top-most feature in
        // bellow list has highest priority
        // 1) DIFR_SW_ASR & DIFR_SW_CG will get preference if both of them are enabled.
        // 2) MSCG
        // 3) MS_LTC
        // 4) MS_PASSIVE
        //
        if ((MS_DIFR_FSM_MASK & lpwrCtrlAllowMask) == MS_DIFR_FSM_MASK)
        {
            newOwnerMask = MS_DIFR_FSM_MASK;
        }
        else if ((LWBIT(RM_PMU_LPWR_CTRL_ID_MS_MSCG) & lpwrCtrlAllowMask) != 0)
        {
            newOwnerMask = LWBIT(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
        }
        else if ((LWBIT(RM_PMU_LPWR_CTRL_ID_MS_LTC) & lpwrCtrlAllowMask) != 0)
        {
            newOwnerMask = LWBIT(RM_PMU_LPWR_CTRL_ID_MS_LTC);
        }
        else if ((LWBIT(RM_PMU_LPWR_CTRL_ID_MS_PASSIVE) & lpwrCtrlAllowMask) != 0)
        {
            newOwnerMask = LWBIT(RM_PMU_LPWR_CTRL_ID_MS_PASSIVE);
        }
    }
    else
    {
        PMU_BREAKPOINT();
    }

    // Update the owner
    if (pLpwrGrpCtrl->ownerMask != newOwnerMask)
    {
        pLpwrGrpCtrl->ownerMask = newOwnerMask;
        lpwrGrpCtrlSeqOwnerChange(grpCtrlId);
    }
}

/*!
 * @brief Sequencer to update the owner
 *
 * Here is sequence to update owner
 * 1) Disallow all LPWR_CTRL from LPWR group
 *    - Disable one LPWR_CTRL at a time
 *    - Do not proceed further until disablement is completed
 * 2) Enable the group owner
 */
void
lpwrGrpCtrlSeqOwnerChange
(
    LwU8 grpCtrlId
)
{
    LPWR_GRP_CTRL *pLpwrGrpCtrl = LPWR_GET_GRP_CTRL(grpCtrlId);
    LwU32          lpwrCtrlId;

    // Disable all LPWR_CTRLs in group
    FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, pLpwrGrpCtrl->lpwrCtrlMask)
    {
        if (!pgCtrlDisallow(lpwrCtrlId, PG_CTRL_DISALLOW_REASON_MASK(LPWR_GRP)))
        {
            //
            // Immediate disablement is not possible. We need to wait until
            // disablement completes. Thus return from this function. We
            // re-trigger the sequencer after receiving the DISALLOW_ACK.
            //
            return;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Enable the current group owners
    FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, pLpwrGrpCtrl->ownerMask)
    {
        if (pgIsCtrlSupported(lpwrCtrlId))
        {
            pgCtrlAllow(lpwrCtrlId, PG_CTRL_ALLOW_REASON_MASK(LPWR_GRP));
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Manage mutual exclusion policy for RM client
 *
 * This helper find the mask of LPWR_CTRL disallowed by RM and feed that mask
 * to GR LPWR group mutual exclusion policy.
 */
void
lpwrGrpCtrlRmClientHandler
(
    LwU8 grpCtrlId
)
{
    LwU32 disallowMask;

    disallowMask = lpwrGrpCtrlRmDisallowMaskGet(grpCtrlId);

    lpwrGrpCtrlMutualExclusion(grpCtrlId, LPWR_GRP_CTRL_CLIENT_ID_RM,
                               disallowMask);
}

/* ------------------------ Private Functions ------------------------------- */
