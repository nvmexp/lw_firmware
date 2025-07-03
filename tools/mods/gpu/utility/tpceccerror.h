/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"
#include "core/include/tasker.h"
#include "core/include/types.h"
#include "gpu/utility/eclwtil.h"
#include <set>
#include <string>

class GpuSubdevice;

// Header for TpcEccError class
//
//------------------------------------------------------------------------------
// How to use the TpcEccError object (similar to a MemError object)
//------------------------------------------------------------------------------
// 1.  Add one TpcEccError to your class for each GPU being tested
// 2.  Before initializing your JS properties, call SetupTest(GpuSubdevice).
//     Ideally this should be done in the Setup() portion of a ModsTest.
// 3.  At the end of the test, call ResolveResult().  On GPUs, this
//       should be done after you restore the VGA display
// 4.  After finishing with the TpcEccError class instance, call Cleanup() on
//     the TpcEccError object.  Ideally this should be done in the Cleanup()
//     portion of a ModsTest.

class TpcEccError
{
public:
    TpcEccError(GpuSubdevice *pSubdev, const string & testName);
    ~TpcEccError();
    void ClearErrors() { m_ErrCounts.eccCounts.clear(); }
    UINT64 GetErrorCount() const;
    GpuSubdevice * GetSubdevice() const { return m_pSubdevice; }
    void LogErrors(const Ecc::DetailedErrCounts &newErrors);
    RC   ResolveResult();
    void SetTestName(const string &testName) { m_TestName = testName; }

    static void Initialize();
    static void LogErrors(const GpuSubdevice * pSubdev, const Ecc::DetailedErrCounts &newErrors);
    static void Shutdown();

private:
    GpuSubdevice            *m_pSubdevice = nullptr;
    Tasker::Mutex            m_Mutex = Tasker::NoMutex();
    Ecc::DetailedErrCounts   m_ErrCounts;
    string                   m_TestName;

    static set<TpcEccError*> s_Registry;
    static Tasker::Mutex     s_RegistryMutex;

};
