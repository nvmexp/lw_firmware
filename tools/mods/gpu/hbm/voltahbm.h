/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_VOLTAHBM_H
#define INCLUDED_VOLTAHBM_H

#include "gpu/include/hbmimpl.h"

//--------------------------------------------------------------------
//! \brief HBM related functionality for Volta
//!
class VoltaHBM : public HBMImpl
{
public:
    VoltaHBM(GpuSubdevice *pSubdev) : HBMImpl(pSubdev) { }
    virtual ~VoltaHBM() { }

    virtual RC ShutDown();
    virtual RC ResetSite(const UINT32 siteID) const;

    virtual FLOAT64 GetPollTimeoutMs() const
    {
        return 1.0f;
    }

    // Unimplemented pure-virtual functions
    virtual RC AssertFBStop();
    virtual RC DeAssertFBStop();
    virtual RC SelectChannel(const UINT32 siteID, const UINT32 channel) const;
    virtual RC AssertMask(const UINT32 siteID) const;
    virtual RC ToggleClock(const UINT32 siteID, const UINT32 numTimes) const;
    virtual RC ReadHostToJtagHBMReg(const UINT32 siteID, const UINT32 instructionID,
        vector<UINT32> *dataRead, const UINT32 dataReadLength) const;
    virtual RC WriteHostToJtagHBMReg(const UINT32 siteID, const UINT32 instructionID,
        const vector<UINT32>& writeData, const UINT32 dataWriteLength) const;

    virtual RC WriteHBMDataReg(const UINT32 siteID, const UINT32 instruction,
        const vector<UINT32>& writeData, const UINT32 writeDatSize,
        const bool useHostToJtag) const;
    virtual RC ReadHBMLaneRepairReg(const UINT32 siteID, const UINT32 instruction,
        const UINT32 channel, vector<UINT32>* readData, const UINT32 readSize,
        const bool useHostToJtag) const;
    virtual RC WriteBypassInstruction(const UINT32 siteID, const UINT32 channel) const;

    virtual RC GetVoltageUv(UINT32 *pVoltUv);
    virtual RC SetVoltageUv(UINT32 voltUv);
private:
    // Get HBM site info
    virtual RC GetHBMDevIdDataHtoJ(const UINT32 siteID, vector<UINT32> *pRegValues) const;
    virtual RC GetHBMDevIdDataFbPriv(const UINT32 siteID, vector<UINT32> *pRegValues) const;

    RC InitVoltageController();
    UINT32 m_OriginalVoltageReg           = 0;
    bool   m_bVoltageControllerPresent    = false;
    bool   m_bVoltageControllerLockOnExit = false;
    bool   m_bVoltageControllerInit       = false;
};

#endif
