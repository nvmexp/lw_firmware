
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

#ifndef INCLUDED_LPWRCTRLIMPL_H
#define INCLUDED_LPWRCTRLIMPL_H

#include "ctrl/ctrl2080/ctrl2080lpwr.h"

class GpuSubdevice;

/**
 * @brief Implement methods for Low Power Features
 *
 */
class LpwrCtrlImpl
{
public:

    LpwrCtrlImpl(GpuSubdevice* pSubdev);
    virtual ~LpwrCtrlImpl(){}

    RC GetLpwrCtrlEntryCount(UINT32 ctrlId, UINT32* count) const;
    RC GetLpwrCtrlExitCount(UINT32 ctrlId, UINT32* count) const;
    RC GetFeatureEngageStatus(UINT32 ctrlId, bool* pEngaged) const;
    RC GetLpwrCtrlIdleMask(UINT32 ctrlId, vector<UINT32> *pIdleMask) const;
    RC GetLpwrCtrlIdleStatus(UINT32 ctrlId, UINT32* idleStatus) const;
    RC GetLpwrCtrlSupported(UINT32 ctrlId, bool* pSupported) const;
    RC GetLpwrCtrlEnabled(UINT32 ctrlId, bool* pEnabled) const;
    RC SetLpwrCtrlEnabled(UINT32 ctrlId, bool  bEnabled) const;
    RC GetLpwrCtrlPreCondition
    (
        UINT32 ctrlId,
        bool *pPreConditionMet,
        vector<UINT32> *pIdleMask
    ) const;
    RC SetLpwrMinThreshold(UINT32 ctrlId) const;
    RC ResetLpwrMinThreshold(UINT32 ctrlId) const;
    RC GetLpwrPstateMask(UINT32 ctrlId, UINT32 *pPstateMask) const;
    RC GetLpwrModes(UINT32 *pModeMask) const;
    RC SetLpwrMode(UINT32 mode) const;
    RC ResetLpwrMode() const;

    RC GetSubFeatureEnableMask(UINT32 ctrlId, UINT32 *pMask) const;
    RC SetSubFeatureEnableMask(UINT32 ctrlId, UINT32 mask) const;
    RC GetSubfeatureSupportMask(UINT32 ctrlId, UINT32 *pMask) const;
    RC GetAbortCount(UINT32 ctrlId, UINT32 *pCount) const;
    RC GetAbortReasonMask(UINT32 ctrlId, UINT32 *pMask) const;
    RC GetDisableReasonMask(UINT32 ctrlId, vector<UINT32> *pReasonMask) const;

    RC GetRppgCtrlSupport(UINT32 ctrlId, bool* pSupported) const;
    RC GetRppgEntryCount(UINT32 ctrlId, UINT32 *pCount) const;
    RC GetRppgPstateMask(UINT32 ctrlId, UINT32 *pPstateMask) const;

private:
    GpuSubdevice* m_pSubdev;

    struct FeatureParams
    {
        UINT32 featureId;
        UINT32 subFeatureId;
        UINT32 paramId;
        UINT32* val;
    };

    // Below function is just used to get a vector of the featureId, 
    // subFeatureId, paramId and value requested and pass it to GetParams function
    RC GetParam(UINT32 featureId, UINT32 subFeatureId, UINT32 paramId, UINT32* val) const;

    // Below function makes the RmCtrl Call to get the requested value based
    // on the vector passed in by GetParam function
    RC GetParams(const vector<FeatureParams>& paramList) const;

    // Below function is just used to get a vector of the featureId, 
    // subFeatureId, paramId and value requested and pass it to SetParams function
    RC SetParam(UINT32 featureId, UINT32 subFeatureId, UINT32 paramId, UINT32* val) const;

    // Below function makes the RmCtrl Call to set the requested value based
    // on the vector passed in by SetParam function
    RC SetParams(const vector<FeatureParams>& paramList) const;

};
#endif
