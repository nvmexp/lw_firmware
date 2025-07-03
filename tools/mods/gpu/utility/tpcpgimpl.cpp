/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/tpcpgimpl.h"
#include "core/include/lwrm.h"

TpcPgImpl::TpcPgImpl(GpuSubdevice* pSubdev)
: m_pSubdev(pSubdev)
{
}

RC TpcPgImpl::GetEntryCount(UINT32* count) const
{
    MASSERT(count);
    RC rc;
    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,                       //featureId
                      LW2080_CTRL_LPWR_SUB_FEATURE_ID_GRAPHICS,             //subFeatureId
                      LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_GATING_COUNT,   //paramId??
                      count));
    return rc;
}

RC TpcPgImpl::GetPgSupported(bool * pSupported)
{
    MASSERT(pSupported);
    RC rc;
    UINT32 supported = 0;
    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,                           //featureId
                      LW2080_CTRL_LPWR_SUB_FEATURE_ID_GRAPHICS,                 //subFeatureId
                      LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_FEATURE_SUPPORT,    //paramId
                      &supported));
    *pSupported = (supported & 0x1) ? true : false;
    return rc;
}

RC TpcPgImpl::GetPgEnabled(bool * pEnabled)
{
    MASSERT(pEnabled);
    RC rc;
    UINT32 enabled = 0;
    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,               //featureId
                      LW2080_CTRL_LPWR_SUB_FEATURE_ID_GRAPHICS,     //subFeatureId
                      LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_ENABLE, //paramId
                      &enabled));
    *pEnabled = (enabled & 0x1) ? true : false;
    return rc;
}

RC TpcPgImpl::SetPgEnabled(bool  bEnabled)
{
    RC rc;
    UINT32 enabled = bEnabled;
    CHECK_RC(SetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,               //featureId
                      LW2080_CTRL_LPWR_SUB_FEATURE_ID_GRAPHICS,     //subFeatureId
                      LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_ENABLE, //paramId
                      &enabled));
    return rc;
}

RC TpcPgImpl::GetAdaptivePgSupported(bool * pSupported)
{
    MASSERT(pSupported);
    RC rc;
    UINT32 supported = 0;
    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_AP,               //featureId
                      LW2080_CTRL_LPWR_SUB_FEATURE_ID_AP_GRAPHICS,  //subFeatureId
                      LW2080_CTRL_LPWR_PARAMETER_ID_AP_SUPPORT,     //paramId
                      &supported));
    *pSupported = (supported & 0x1) ? true : false;
    return rc;
}

RC TpcPgImpl::GetAdaptivePgEnabled(bool * pEnabled)
{
    MASSERT(pEnabled);
    RC rc;
    UINT32 enabled = 0;
    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_AP,               //featureId
                      LW2080_CTRL_LPWR_SUB_FEATURE_ID_AP_GRAPHICS,  //subFeatureId
                      LW2080_CTRL_LPWR_PARAMETER_ID_AP_ENABLE,      //paramId
                      &enabled));
    *pEnabled = (enabled & 0x1) ? true : false;
    return rc;
}

RC TpcPgImpl::SetAdaptivePgEnabled(bool bEnabled)
{
    RC rc;
    UINT32 enabled = bEnabled;
    CHECK_RC(SetParam(LW2080_CTRL_LPWR_FEATURE_ID_AP,               //featureId
                      LW2080_CTRL_LPWR_SUB_FEATURE_ID_AP_GRAPHICS,  //subFeatureId
                      LW2080_CTRL_LPWR_PARAMETER_ID_AP_ENABLE,      //paramId
                      &enabled));
    return rc;
}

RC TpcPgImpl::GetPgIdleThresholdUs(UINT32 *pThresholdUs)
{
    RC rc;
    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,                           //featureId
                      LW2080_CTRL_LPWR_SUB_FEATURE_ID_GRAPHICS,                 //subFeatureId
                      LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_IDLE_THRESHOLD_US,  //paramId
                      pThresholdUs));
    return rc;
}

RC TpcPgImpl::GetAdaptivePgIdleThresholdUs(AdaptivePgIdleThresholds *pPG)
{
    RC rc;
    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_AP,                           //featureId
                      LW2080_CTRL_LPWR_SUB_FEATURE_ID_AP_GRAPHICS,              //subFeatureId
                      LW2080_CTRL_LPWR_PARAMETER_ID_AP_LWRRENT_IDLE_THRESHOLD,  //paramId
                      &pPG->current));
    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_AP,
                      LW2080_CTRL_LPWR_SUB_FEATURE_ID_AP_GRAPHICS,
                      LW2080_CTRL_LPWR_PARAMETER_ID_AP_IDLE_THRESHOLD_MIN,
                      &pPG->min));
    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_AP,
                      LW2080_CTRL_LPWR_SUB_FEATURE_ID_AP_GRAPHICS,
                      LW2080_CTRL_LPWR_PARAMETER_ID_AP_IDLE_THRESHOLD_MAX,
                      &pPG->max));
    return rc;
}

RC TpcPgImpl::SetPgIdleThresholdUs(UINT32 TimeUs)
{
    RC rc;
    CHECK_RC(SetParam(LW2080_CTRL_LPWR_FEATURE_ID_PG,                           //featureId
                      LW2080_CTRL_LPWR_SUB_FEATURE_ID_GRAPHICS,                 //subFeatureId
                      LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_IDLE_THRESHOLD_US,  //paramId
                      &TimeUs));
    return rc;
}

RC TpcPgImpl::GetParams(const vector<FeatureParams>& paramList) const
{
    RC rc;

    if (paramList.size() == 0)
        return OK;

    if (paramList.size() > LW2080_CTRL_LPWR_FEATURE_PARAMETER_LIST_MAX_SIZE)
        return RC::SOFTWARE_ERROR;

    LW2080_CTRL_LPWR_FEATURE_PARAMETER_GET_PARAMS params = {};
    params.listSize = static_cast<LwU32>(paramList.size());
    for (UINT32 i = 0; i < paramList.size(); i++)
    {
        params.list[i].feature.id    = static_cast<UINT16>(paramList[i].featureId & 0xFFFF);
        params.list[i].feature.subId = static_cast<UINT16>(paramList[i].subFeatureId & 0xFFFF);
        params.list[i].param.id      = paramList[i].paramId;
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdev,
        LW2080_CTRL_CMD_LPWR_FEATURE_PARAMETER_GET,
        &params,
        sizeof(params)));

    for (UINT32 i = 0; i < paramList.size(); i++)
    {
        // The param.flag field is not always updated by RM
        if (m_pSubdev->HasBug(2225586))
        {
            *paramList[i].val = params.list[i].param.val;
        }
        else
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
    }

    return rc;
}

RC TpcPgImpl::GetParam(UINT32 featureId, UINT32 subFeatureId, UINT32 paramId, UINT32* val) const
{
    MASSERT(val);
    RC rc;

    vector<FeatureParams> paramList;
    FeatureParams params = {featureId, subFeatureId, paramId, val}; //$
    paramList.push_back(params);
    CHECK_RC(GetParams(paramList));

    return rc;
}

RC TpcPgImpl::SetParam(UINT32 featureId, UINT32 subFeatureId, UINT32 paramId, UINT32* val) const
{
    MASSERT(val);
    RC rc;

    vector<FeatureParams> paramList;
    FeatureParams params = {featureId, subFeatureId, paramId, val}; //$
    paramList.push_back(params);
    CHECK_RC(SetParams(paramList));

    return rc;
}

RC TpcPgImpl::SetParams(const vector<FeatureParams>& paramList) const
{
    RC rc;

    if (paramList.size() == 0)
        return OK;

    if (paramList.size() > LW2080_CTRL_LPWR_FEATURE_PARAMETER_LIST_MAX_SIZE)
        return RC::SOFTWARE_ERROR;

    LW2080_CTRL_LPWR_FEATURE_PARAMETER_SET_PARAMS params = {};
    params.listSize = static_cast<LwU32>(paramList.size());
    for (UINT32 i = 0; i < paramList.size(); i++)
    {
        params.list[i].feature.id    = static_cast<UINT16>(paramList[i].featureId & 0xFFFF);
        params.list[i].feature.subId = static_cast<UINT16>(paramList[i].subFeatureId & 0xFFFF);
        params.list[i].param.id      = paramList[i].paramId;
        params.list[i].param.val     = *paramList[i].val;
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdev,
        LW2080_CTRL_CMD_LPWR_FEATURE_PARAMETER_SET,
        &params,
        sizeof(params)));

    // The param.flag field is not always updated by RM.
    if (!m_pSubdev->HasBug(2225586))
    {
        //Make sure each parameter was set by RM
        for (UINT32 i = 0; i < paramList.size(); i++)
        {
            if (FLD_TEST_DRF(2080_CTRL, _LPWR_FEATURE, _PARAMETER_FLAG_SUCCEED, _YES,
                              params.list[i].param.flag))
            {
                continue;
            }
            rc = RC::LWRM_ERROR;
        }
    }

    return rc;
}
