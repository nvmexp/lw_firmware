/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#pragma once

#ifndef INCLUDED_LWSWITCHISM_H
#define INCLUDED_LWSWITCHISM_H

#include "core/include/ism.h"
#include "gpu/include/testdevice.h"

//------------------------------------------------------------------------------
// Derived class for implementing access to the In-Silicon Measurement(ISM)
// macros on a lwswitch.
//------------------------------------------------------------------------------
class LwSwitchIsm : public Ism
{
public:
    LwSwitchIsm
    (
        TestDevice* pTestDev,
        vector<Ism::IsmChain> *pTable
    );

    virtual ~LwSwitchIsm() {};

    static vector<IsmChain>* GetTable(Device::LwDeviceId devId);

protected:
    virtual RC ConfigureISMBits
    (
        vector<UINT64> &IsmBits
        , IsmInfo &info
        , IsmTypes ismType
        , UINT32 iddq            // 0 = powerUp, 1 = powerDown
        , UINT32 enable          // 0 = disable, 1 = enable
        , UINT32  durClkCycles   // How long to run the experiment
        , UINT32 flags           // Special handling flags
    );

    virtual UINT64 ExtractISMRawCount
    (
        UINT32 ismType,
        vector<UINT64>& ismBits
    );

    virtual UINT32 GetISMSize
    (
        IsmTypes ismType
    );

    virtual RC DumpISMBits
    (
        UINT32 ismType,
        vector<UINT64>& ismBits
    );

    virtual RC ReadHost2Jtag
    (
        UINT32 Chiplet,
        UINT32 Instruction,
        UINT32 ChainLength,
        vector<UINT32> *pResult
    );

    virtual RC WriteHost2Jtag
    (
        UINT32 Chiplet,
        UINT32 Instruction,
        UINT32 ChainLength,
        const vector<UINT32> &InputData
    );

    virtual RC GetISMClkFreqKhz
    (
        UINT32* clkSrcFreqKHz
    );

    virtual RC PollISMCtrlComplete
    (
        UINT32 *pComplete
    );

    virtual UINT32 IsExpComplete
    (
        UINT32 ismType, //Ism::IsmTypes
        INT32 offset,
        const vector<UINT32> &JtagArray
    );

    virtual RC TriggerISMExperiment();

    virtual RC ConfigureISMCtrl
    (
        const IsmChain *pChain          //The chain with the primary controller
        ,vector<UINT32>  &jtagArray     //vector of bits for the chain
        ,bool            bEnable        //enable/disable the controller
        ,UINT32          durClkCycles   // how long to run the experiment
        ,UINT32          delayClkCycles // how long to wait after the trigger
                                        // before taking samples.
        ,UINT32          trigger        // type of trigger to use.
        ,INT32           loopMode       // if true allows continuous looping of the experiments
        ,INT32           haltMode       // if true allows halting of a continuous experiments
        ,INT32           ctrlSrcSel     // 1 = jtag, 0 = priv access
    );

    virtual UINT32 GetISMDurClkCycles();
    virtual INT32 GetISMMode(PART_TYPES type);
    virtual INT32 GetISMOutputDivider(PART_TYPES partType);
    virtual INT32 GetISMRefClkSel(PART_TYPES partType);
    virtual INT32 GetOscIdx(PART_TYPES type);
    virtual INT32 GetISMOutDivForFreqCalc(PART_TYPES PartType, INT32 outDiv);

private:
    virtual bool AllowIsmTrailingBit() { return true; }
    virtual bool AllowIsmUnusedBits()  { return true; }

    TestDevice* m_pTestDev;
};

#endif
