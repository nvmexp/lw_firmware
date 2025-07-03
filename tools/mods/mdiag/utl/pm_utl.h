/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#ifndef INCLUDED_PM_UTL_H
#define INCLUDED_PM_UTL_H

#include "mdiag/advschd/pmtest.h"
#include "mdiag/utl/utlusertest.h"

// This class represents the interface between the Policy Manager and
// a UTL user test.
//
class PmTest_Utl : public PmTest
{
public:
    PmTest_Utl(PolicyManager* policyManager, UtlUserTest* utlTest) :
        PmTest(policyManager, utlTest->GetLwRmPtr()),
        m_UtlTest(utlTest)
    {
    }
    virtual ~PmTest_Utl() {}

    string GetName() const override { return m_UtlTest->GetTestName(); }
    string GetTypeName() const override { return "UtlUserTest"; }
    void* GetUniquePtr() const override { return m_UtlTest; }
    GpuDevice* GetGpuDevice() const override { return m_UtlTest->GetGpuDevice(); }
    RC Abort() override { m_UtlTest->AbortTest(); return OK; }
    bool IsAborting() const override { return m_UtlTest->AbortedTest(); }

    void SendTraceEvent
    (
        const char* eventName,
        const char* surfaceName
    ) const override {};

    PmSmcEngine* GetSmcEngine() override { return nullptr; }

private:
    UtlUserTest* m_UtlTest;
};

#endif
