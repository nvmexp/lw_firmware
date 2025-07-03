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

#ifndef INCLUDED_RPPGIMPL_H
#define INCLUDED_RPPGIMPL_H

#include "gpusbdev.h"
#include "ctrl/ctrl2080/ctrl2080lpwr.h"

/**
 * @brief Implement methods for RPPG (RAM Periphery Power Gating)
 *
 */
class RppgImpl
{
public:
    enum RppgType
    {
        GRRPPG = LW2080_CTRL_LPWR_SUB_FEATURE_ID_RPPG_GR_COUPLED,   //00
        MSRPPG = LW2080_CTRL_LPWR_SUB_FEATURE_ID_RPPG_MS_COUPLED,   //01
        DIRPPG = LW2080_CTRL_LPWR_SUB_FEATURE_ID_RPPG_DI_COUPLED,   //02
    };

    RppgImpl(GpuSubdevice* pSubdev);
    virtual ~RppgImpl(){}

    RC GetSupported(UINT32 pstateNum, bool* supported) const;
    RC GetEntryCount(UINT32* count) const;
    RC GetExitCount(UINT32* count) const;

    SETGET_PROP(Type, RppgType);

private:
    GpuSubdevice* m_pSubdev;
    RppgType      m_Type;

    struct FeatureParams
    {
        UINT32 featureId;
        UINT32 subFeatureId;
        UINT32 paramId;
        UINT32* val;
    };
    RC GetParams(const vector<FeatureParams>& paramList) const;
    RC GetParam(UINT32 featureId, UINT32 subFeatureId, UINT32 paramId, UINT32* val) const;
};

#endif
