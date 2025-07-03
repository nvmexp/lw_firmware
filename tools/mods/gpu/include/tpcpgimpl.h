
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

#ifndef INCLUDED_TPCPGIMPL_H
#define INCLUDED_TPCPGIMPL_H

#include "gpusbdev.h"
#include "ctrl/ctrl2080/ctrl2080lpwr.h"

/**
 * @brief Implement methods for TPC-PG (TPC Power Gating)
 *
 */
class TpcPgImpl
{
public:
    struct AdaptivePgIdleThresholds
    {
        UINT32 min;
        UINT32 max;
        UINT32 current;
    };

    TpcPgImpl(GpuSubdevice* pSubdev);
    virtual ~TpcPgImpl(){}

    RC GetEntryCount(UINT32* count) const;
    //RC GetExitCount(UINT32* count) const;
    RC GetAdaptivePgSupported(bool* pSupported);
    RC GetPgSupported(bool * pSupported);

    RC GetAdaptivePgEnabled(bool*  pEnabled);
    RC SetAdaptivePgEnabled(bool bEnabled);

    RC GetPgEnabled(bool*  pEnabled);
    RC SetPgEnabled(bool  enabled);

    RC GetAdaptivePgIdleThresholdUs(AdaptivePgIdleThresholds* pPG);

    RC GetPgIdleThresholdUs(UINT32* pThresholdUs);
    RC SetPgIdleThresholdUs(UINT32 TimeUs);

private:
    GpuSubdevice* m_pSubdev;

    struct FeatureParams
    {
        UINT32 featureId;
        UINT32 subFeatureId;
        UINT32 paramId;
        UINT32* val;
    };
    RC GetParams(const vector<FeatureParams>& paramList) const;
    RC GetParam(UINT32 featureId, UINT32 subFeatureId, UINT32 paramId, UINT32* val) const;

    RC SetParams(const vector<FeatureParams>& paramList) const;
    RC SetParam(UINT32 featureId, UINT32 subFeatureId, UINT32 paramId, UINT32* val) const;
};

#endif
