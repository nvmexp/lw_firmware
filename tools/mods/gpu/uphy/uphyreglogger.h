/* LWIDIA_COPYRIGHT_BEGIN                                                 */
/*                                                                        */
/* Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All  */
/* information contained herein is proprietary and confidential to LWPU */
/* Corporation.  Any use, reproduction, or disclosure without the written */
/* permission of LWPU Corporation is prohibited.                        */
/*                                                                        */
/* LWIDIA_COPYRIGHT_END                                                   */

#pragma once

#ifdef MATS_STANDALONE
    #include "fakemods.h"
#else
    #include "core/include/pci.h"
    #include "core/include/rc.h"
    #include "device/interface/lwlink.h"
    #include "gpu/include/testdevice.h"
#endif
#include <vector>

namespace UphyRegLogger
{
    enum class UphyInterface : UINT08
    {
        Pcie,
        LwLink,
        C2C,
        All
    };

    enum LogPoint
    {
        EOM              = (1U << 0),
        BandwidthTest    = (1U << 1),
        PreTest          = (1U << 2),
        PostTest         = (1U << 3),
        PostTestError    = (1U << 4),
        PostLinkTraining = (1U << 5),
        Manual           = (1U << 6),
        Background       = (1U << 7),
        All              = ((1U << 8) - 1)
    };

#ifndef MATS_STANDALONE
    typedef map<LwLink::FomMode, vector<vector<INT32>>> LwLinkEomData;
    typedef map<Pci::FomMode, vector<vector<INT32>>>    PciEomData;

    FLOAT64 GetBgLogPeriodMs();
    UINT08  GetLoggedInterface();
    UINT08  GetLoggedRegBlock();
    bool    GetLoggingEnabled();
    UINT32  GetLogPointMask();
    UINT08  GetLogType();
    string  GetReportSuffix();
    string  GetTestType();
    RC Initialize();
    RC LogRegs(UphyInterface interface, LogPoint logPoint, RC errorCode);
    RC LogRegs
    (
        TestDevicePtr pTestDevice,
        UphyInterface interface,
        LogPoint      logPoint,
        RC            errorCode
    );
    RC LogRegs
    (
        TestDevicePtr pTestDevice,
        UphyInterface interface,
        LogPoint      logPoint,
        RC            errorCode,
        const LwLinkEomData & eomData
    );
    RC LogRegs
    (
        TestDevicePtr pTestDevice,
        UphyInterface interface,
        LogPoint      logPoint,
        RC            errorCode,
        const PciEomData & eomData
    );

    RC ForceLogRegs
    (
        UphyInterface interface,
        LogPoint      logPoint,
        RC            errorCode,
        bool          bLogDevicePerfData
    );
    RC ForceLogRegs
    (
        TestDevicePtr pTestDevice,
        UINT08        uphyInterface,
        LogPoint      logPoint,
        RC            errorCode,
        bool          bLogDevicePerfData,
        UINT08        regBlock,
        UINT08        logType
    );

    RC SetBgLogPeriodMs(FLOAT64 logPeriodMs);
    RC SetC2CRegListToLog(const vector<UINT16> &regList);
    RC SetLoggedInterface(UINT08 uphyInterface);
    RC SetLoggedRegBlock(UINT08 regBlock);
    RC SetLoggingEnabled(bool bEnable);
    RC SetLogPointMask(UINT32 logPointMask);
    RC SetLogType(UINT08 logType);
    RC SetRegListToLog(UINT08 regBlock, const vector<UINT16> &regList);
    RC SetReportSuffix(string reportSuffix);
    RC SetTestType(string testType);
    RC Shutdown();
#endif
};
