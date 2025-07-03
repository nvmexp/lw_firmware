/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/lwrm.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/lpwrctrlimpl.h"

LpwrCtrlImpl::LpwrCtrlImpl(GpuSubdevice* pSubdev)
: m_pSubdev(pSubdev)
{
}

RC LpwrCtrlImpl::GetLpwrCtrlEntryCount(UINT32 ctrlId, UINT32* count) const
{
    MASSERT(count);
    RC rc;

    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,                       //featureId
                      ctrlId,                                               //subFeatureId
                      LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_GATING_COUNT,   //paramId
                      count));
    return OK;
}

RC LpwrCtrlImpl::GetLpwrCtrlExitCount(UINT32 ctrlId, UINT32* count) const
{
    MASSERT(count);
    RC rc;

    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,                       //featureId
                      ctrlId,                                               //subFeatureId
                      LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_EXIT_COUNT,     //paramId
                      count));
    return OK;
}

RC LpwrCtrlImpl::GetLpwrCtrlSupported(UINT32 ctrlId, bool* pSupported) const
{
    MASSERT(pSupported);
    RC rc;
    UINT32 supported = 0;

    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,                       //featureId
                      ctrlId,                                               //subFeatureId
                      LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_FEATURE_SUPPORT,//paramId
                      &supported));
    *pSupported = (supported != 0x0) ? true : false;
    return rc;
}

RC LpwrCtrlImpl::GetLpwrCtrlEnabled(UINT32 ctrlId, bool* pEnabled) const
{
    MASSERT(pEnabled);
    RC rc;
    UINT32 enabled = 0;

    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,                       //featureId
                      ctrlId,                                               //subFeatureId
                      LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_ENABLE,         //paramId
                      &enabled));
    *pEnabled = (enabled != 0x0) ? true : false;
    return rc;
}

RC LpwrCtrlImpl::SetLpwrCtrlEnabled(UINT32 ctrlId, bool bEnabled) const
{
    RC rc;
    UINT32 enabled = bEnabled;

    CHECK_RC(SetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,                       //featureId
                      ctrlId,                                               //subFeatureId
                      LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_ENABLE,         //paramId
                      &enabled));
    return rc;
}

RC LpwrCtrlImpl::GetLpwrCtrlPreCondition
(
    UINT32 ctrlId,
    bool *pPreConditionMet,
    vector<UINT32> *pIdleMask
) const
{
    MASSERT(pPreConditionMet);
    RC rc;
    UINT32 idleStatus[3] = {0};
    UINT32 i = 0;

    for (i = 0; i < (*pIdleMask).size(); i++)
    {
        Printf(Tee::PriHigh, "The Idle Mask is : 0x%x \n", (*pIdleMask)[i]);
        CHECK_RC(GetLpwrCtrlIdleStatus(ctrlId, idleStatus));

        Printf(Tee::PriHigh, "The Idle Status is : 0x%x \n", idleStatus[i]);
        Printf(Tee::PriHigh,"***************************************\n");

        if (((*pIdleMask)[i] & idleStatus[i]) != (*pIdleMask)[i])
        {
            Printf(Tee::PriHigh, "\nPre condition not met. GPC-RG cannot be engaged yet.\n");
            *pPreConditionMet = false;
            break;
        }            
    }

    if (i == 3)
    {
        *pPreConditionMet = true;
    }

    return rc;
}

RC LpwrCtrlImpl::GetFeatureEngageStatus(UINT32 ctrlId, bool* pEngaged) const
{
    MASSERT(pEngaged);
    RC rc;
    UINT32 engaged = 0;

    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,                       //featureId
                      ctrlId,                                               //subFeatureId
                      LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_ENGAGE_STATUS,  //paramId
                      &engaged));
    *pEngaged = (engaged != 0x0) ? true : false;
    return rc;
}

RC LpwrCtrlImpl::GetLpwrCtrlIdleMask(UINT32 ctrlId, vector<UINT32> *pIdleMask) const
{
    MASSERT(pIdleMask);
    MASSERT((*pIdleMask).size() == 3);

    vector<FeatureParams> paramList;

    auto FillParam = [&](UINT32 paramId, UINT32 *val)
    {
        FeatureParams params = {LW2080_CTRL_LPWR_FEATURE_ID_PG, ctrlId, paramId, val};
        paramList.push_back(params);
    };

    FillParam(LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_IDLE_MASK_0, &(*pIdleMask)[0]);
    FillParam(LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_IDLE_MASK_1, &(*pIdleMask)[1]);
    FillParam(LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_IDLE_MASK_2, &(*pIdleMask)[2]);

    return GetParams(paramList);
}

//
// Idle status comes from IDLE bank in PMU HW. This idle bank is common 
// across all LPWR controllers. Thus ctrlId is not mandatory to get idle status.
//
RC LpwrCtrlImpl::GetLpwrCtrlIdleStatus(UINT32 ctrlId, UINT32* idleStatus) const
{
    MASSERT(idleStatus);
    RC rc;
    vector<FeatureParams> paramList;

    FeatureParams idleStatusParams0 = {LW2080_CTRL_LPWR_FEATURE_ID_GENERIC,
                                   ctrlId,
                                   LW2080_CTRL_LPWR_PARAMETER_ID_GENERIC_CTRL_IDLE_STATUS_0,
                                   &idleStatus[0]};
    paramList.push_back(idleStatusParams0);

    FeatureParams idleStatusParams1 = {LW2080_CTRL_LPWR_FEATURE_ID_GENERIC,
                                   ctrlId,
                                   LW2080_CTRL_LPWR_PARAMETER_ID_GENERIC_CTRL_IDLE_STATUS_1,
                                   &idleStatus[1]};
    paramList.push_back(idleStatusParams1);

    FeatureParams idleStatusParams2 = {LW2080_CTRL_LPWR_FEATURE_ID_GENERIC,
                                   ctrlId,
                                   LW2080_CTRL_LPWR_PARAMETER_ID_GENERIC_CTRL_IDLE_STATUS_2,
                                   &idleStatus[2]};
    paramList.push_back(idleStatusParams2);

    CHECK_RC(GetParams(paramList));

    return rc;
}

RC LpwrCtrlImpl::SetLpwrMinThreshold(UINT32 ctrlId) const
{
    UINT32 milwalue = LW2080_CTRL_LPWR_CTRL_IDLE_THRESHOLD_DEFAULT_MINIMUM_US;

    return SetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,
                    ctrlId,
                    LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_IDLE_THRESHOLD_US,
                    &milwalue);
}

RC LpwrCtrlImpl::ResetLpwrMinThreshold(UINT32 ctrlId) const
{
    UINT32 reset = 0;

    return SetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,
                    ctrlId,
                    LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_THRESHOLD_RESET,
                    &reset);
}

RC LpwrCtrlImpl::GetLpwrPstateMask(UINT32 ctrlId, UINT32 *pPstateMask) const
{
    MASSERT(pPstateMask);

    *pPstateMask = 0;

    return GetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,
                    ctrlId,
                    LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_PSTATE_SUPPORT_MASK,
                    pPstateMask);
}

RC LpwrCtrlImpl::GetLpwrModes(UINT32 *pModeMask) const
{
    MASSERT(pModeMask);

    *pModeMask = 0;

    return GetParam(LW2080_CTRL_LPWR_FEATURE_ID_GENERIC,
                    0,
                    LW2080_CTRL_LPWR_PARAMETER_ID_GENERIC_CTRL_LPWR_MODE_SUPPORT_MASK,
                    pModeMask);
}

RC LpwrCtrlImpl::SetLpwrMode(UINT32 mode) const
{
    return SetParam(LW2080_CTRL_LPWR_FEATURE_ID_GENERIC,
                    0,
                    LW2080_CTRL_LPWR_PARAMETER_ID_GENERIC_CTRL_LPWR_MODE,
                    &mode);
}

RC LpwrCtrlImpl::ResetLpwrMode() const
{
    UINT32 reset = 0;

    return SetParam(LW2080_CTRL_LPWR_FEATURE_ID_GENERIC,
                    0,
                    LW2080_CTRL_LPWR_PARAMETER_ID_GENERIC_CTRL_LPWR_MODE_RESET,
                    &reset);
}

RC LpwrCtrlImpl::GetRppgCtrlSupport(UINT32 ctrlId, bool* pSupported) const
{
    MASSERT(pSupported);
    RC rc;
    UINT32 supported = 0;

    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_RPPG,
                      ctrlId,
                      LW2080_CTRL_LPWR_PARAMETER_ID_RPPG_CTRL_SUPPORT,
                      &supported));
    *pSupported = (supported != 0);

    return rc;
}

RC LpwrCtrlImpl::GetRppgEntryCount(UINT32 ctrlId, UINT32* pCount) const
{
    MASSERT(pCount);

    return GetParam(LW2080_CTRL_LPWR_FEATURE_ID_RPPG,
                    ctrlId,
                    LW2080_CTRL_LPWR_PARAMETER_ID_RPPG_CTRL_ENTRY_COUNT,
                    pCount);
}

RC LpwrCtrlImpl::GetAbortCount(UINT32 ctrlId, UINT32 *pCount) const
{
    MASSERT(pCount);

    *pCount = 0;
    return GetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,
                    ctrlId,
                    LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_DENY_COUNT,
                    pCount);
}

RC LpwrCtrlImpl::GetAbortReasonMask(UINT32 ctrlId, UINT32 *pMask) const
{
    MASSERT(pMask);

    *pMask = 0;
    return GetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,
                    ctrlId,
                    LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_ABORT_REASON_MASK,
                    pMask);
}

RC LpwrCtrlImpl::GetRppgPstateMask(UINT32 ctrlId, UINT32 *pPstateMask) const
{
    MASSERT(pPstateMask);

    *pPstateMask = 0;

    return GetParam(LW2080_CTRL_LPWR_FEATURE_ID_RPPG,
                    ctrlId,
                    LW2080_CTRL_LPWR_PARAMETER_ID_RPPG_CTRL_PSTATE_SUPPORT_MASK,
                    pPstateMask);
}

RC LpwrCtrlImpl::SetSubFeatureEnableMask(UINT32 ctrlId, UINT32 mask) const
{
    return SetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,
                    ctrlId,
                    LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_SUB_FEATURE_ENABLED_MASK,
                    &mask);
}

RC LpwrCtrlImpl::GetSubFeatureEnableMask(UINT32 ctrlId, UINT32 *pMask) const
{
    MASSERT(pMask);

    *pMask = 0;

    return GetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,
                    ctrlId,
                    LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_SUB_FEATURE_ENABLED_MASK,
                    pMask);
}

RC LpwrCtrlImpl::GetSubfeatureSupportMask(UINT32 ctrlId, UINT32 *pMask) const
{
    MASSERT(pMask);

    *pMask = 0;
    return GetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,
                    ctrlId,
                    LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_SUB_FEATURE_SUPPORT_MASK,
                    pMask);
}

RC LpwrCtrlImpl::GetDisableReasonMask(UINT32 ctrlId, vector<UINT32> *pReasonMask) const
{
    MASSERT(pReasonMask);
    MASSERT((*pReasonMask).size() == 4);

    vector<FeatureParams> paramList;

    auto FillParam = [&](UINT32 paramId, UINT32 *val)
    {
        FeatureParams params = {LW2080_CTRL_LPWR_FEATURE_ID_PG, ctrlId, paramId, val};
        paramList.push_back(params);
    };

    FillParam(LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_SW_DISABLE_REASON_MASK_RM,  &(*pReasonMask)[0]);
    FillParam(LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_HW_DISABLE_REASON_MASK_RM,  &(*pReasonMask)[1]);
    FillParam(LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_SW_DISABLE_REASON_MASK_PMU, &(*pReasonMask)[2]);
    FillParam(LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_HW_DISABLE_REASON_MASK_PMU, &(*pReasonMask)[3]);

    return GetParams(paramList);
}

RC LpwrCtrlImpl::GetParams(const vector<FeatureParams>& paramList) const
{
    RC rc;

    if (paramList.size() == 0 || paramList.size() > LW2080_CTRL_LPWR_FEATURE_PARAMETER_LIST_MAX_SIZE)
        return RC::SOFTWARE_ERROR;

    LW2080_CTRL_LPWR_FEATURE_PARAMETER_GET_PARAMS params = {};
    params.listSize = static_cast<LwU32>(paramList.size());
    for (UINT32 i = 0; i < paramList.size(); i++)
    {
        params.list[i].feature.id    = paramList[i].featureId;
        params.list[i].feature.subId = paramList[i].subFeatureId;
        params.list[i].param.id      = paramList[i].paramId;
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdev,
        LW2080_CTRL_CMD_LPWR_FEATURE_PARAMETER_GET,
        &params,
        sizeof(params)));

    for (UINT32 i = 0; i < paramList.size(); i++)
    {
        if (FLD_TEST_DRF(2080_CTRL, _LPWR_FEATURE, _PARAMETER_FLAG_SUCCEED, _YES,
                          params.list[i].param.flag))
        {
            *paramList[i].val = params.list[i].param.val;
        }
        else
        {
            rc = RC::LWRM_ERROR;
        }
    }

    return rc;
}

RC LpwrCtrlImpl::GetParam(UINT32 featureId, UINT32 subFeatureId, UINT32 paramId, UINT32* val) const
{
    MASSERT(val);
    RC rc;

    vector<FeatureParams> paramList;
    FeatureParams params = {featureId, subFeatureId, paramId, val};
    paramList.push_back(params);
    CHECK_RC(GetParams(paramList));

    return rc;
}

RC LpwrCtrlImpl::SetParams(const vector<FeatureParams>& paramList) const
{
    RC rc;

    if (paramList.size() == 0 || paramList.size() > LW2080_CTRL_LPWR_FEATURE_PARAMETER_LIST_MAX_SIZE)
        return RC::SOFTWARE_ERROR;

    LW2080_CTRL_LPWR_FEATURE_PARAMETER_GET_PARAMS params = {};
    params.listSize = static_cast<LwU32>(paramList.size());
    for (UINT32 i = 0; i < paramList.size(); i++)
    {
        params.list[i].feature.id    = paramList[i].featureId;
        params.list[i].feature.subId = paramList[i].subFeatureId;
        params.list[i].param.id      = paramList[i].paramId;
        params.list[i].param.val     = *paramList[i].val;
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdev,
        LW2080_CTRL_CMD_LPWR_FEATURE_PARAMETER_SET,
        &params,
        sizeof(params)));

    for (UINT32 i = 0; i < paramList.size(); i++)
    {
        if (FLD_TEST_DRF(2080_CTRL, _LPWR_FEATURE, _PARAMETER_FLAG_SUCCEED, _YES,
                          params.list[i].param.flag))
        {
            continue;
        }
        else
        {
            rc = RC::LWRM_ERROR;
        }
    }

    return rc;
}

RC LpwrCtrlImpl::SetParam(UINT32 featureId, UINT32 subFeatureId, UINT32 paramId, UINT32* val) const
{
    MASSERT(val);
    RC rc;

    vector<FeatureParams> paramList;
    FeatureParams params = {featureId, subFeatureId, paramId, val};
    paramList.push_back(params);
    CHECK_RC(SetParams(paramList));

    return rc;
}
