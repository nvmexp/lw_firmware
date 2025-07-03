/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/rppgimpl.h"
#include "core/include/lwrm.h"

RppgImpl::RppgImpl(GpuSubdevice* pSubdev)
: m_pSubdev(pSubdev)
, m_Type(GRRPPG)
{
}

RC RppgImpl::GetSupported(UINT32 pstateNum, bool* supported) const
{
    MASSERT(supported);
    *supported = false;
    RC rc;

    vector<FeatureParams> paramList;

    UINT32 supportVal = 0;
    FeatureParams supportParams = {LW2080_CTRL_LPWR_FEATURE_ID_RPPG,
                                   static_cast<UINT32>(m_Type),
                                   LW2080_CTRL_LPWR_PARAMETER_ID_RPPG_CTRL_SUPPORT,
                                   &supportVal};
    paramList.push_back(supportParams);

    UINT32 pstateSupportVal = 0;
    FeatureParams pstateSupportParams = {LW2080_CTRL_LPWR_FEATURE_ID_RPPG,
                                         static_cast<UINT32>(m_Type),
                                         LW2080_CTRL_LPWR_PARAMETER_ID_RPPG_CTRL_PSTATE_SUPPORT_MASK,
                                         &pstateSupportVal};
    paramList.push_back(pstateSupportParams);

    CHECK_RC(GetParams(paramList));

    *supported = supportVal && (pstateSupportVal & (1 << pstateNum));

    return OK;
}

RC RppgImpl::GetEntryCount(UINT32* count) const
{
    MASSERT(count);
    RC rc;

    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_RPPG,
                      m_Type,
                      LW2080_CTRL_LPWR_PARAMETER_ID_RPPG_CTRL_ENTRY_COUNT,
                      count));
    return OK;
}

RC RppgImpl::GetExitCount(UINT32* count) const
{
    MASSERT(count);
    RC rc;

    CHECK_RC(GetParam(LW2080_CTRL_LPWR_FEATURE_ID_RPPG, 
                      m_Type,
                      LW2080_CTRL_LPWR_PARAMETER_ID_RPPG_CTRL_EXIT_COUNT,
                      count));
    return OK;
}

RC RppgImpl::GetParams(const vector<FeatureParams>& paramList) const
{
    StickyRC rc;

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
        // The param.flag field is not always updated by RM.
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

RC RppgImpl::GetParam(UINT32 featureId, UINT32 subFeatureId, UINT32 paramId, UINT32* val) const
{
    MASSERT(val);
    RC rc;

    vector<FeatureParams> paramList;
    FeatureParams params = {featureId, subFeatureId, paramId, val}; //$
    paramList.push_back(params);
    CHECK_RC(GetParams(paramList));

    return OK;
}

