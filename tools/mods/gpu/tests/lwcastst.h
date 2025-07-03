/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file lwdastst.h
 * @brief Base class for all LWCA (compute) tests
 */

#ifndef INCLUDED_LWDASTREAMTEST_H
#define INCLUDED_LWDASTREAMTEST_H

#ifndef INCLUDED_GPUTEST_H
#include "gputest.h"
#endif

#include "gpu/tests/lwca/lwdawrap.h"

#include <regex>
#include <memory>

class TestDevice;
typedef shared_ptr<TestDevice> TestDevicePtr;

extern SObject LwdaStreamTest_Object;

//! Base class for tests using LWCA. This is a new class which is
//! based on the new LWCA wrapper based on stream API. It is recommended
//! to use this class over LwdaTest since the LwdaTest class will
//! go away at some point.
class LwdaStreamTest: public GpuTest
{
public:
    LwdaStreamTest();
    virtual ~LwdaStreamTest();
    bool IsSupported() override;
    RC BindTestDevice(TestDevicePtr pTestDevice, JSContext *cx, JSObject *obj) override;
    void PropertyHelp(JSContext *pCtx, regex *pRegex) override;
    RC Setup() override;
    RC Cleanup() override;
    bool IsTestType(TestType tt) override;

protected:
    const Lwca::Instance& GetLwdaInstance() const { return m_Lwda; }
    Lwca::Instance& GetLwdaInstance() { return m_Lwda; }
    const Lwca::Instance* GetLwdaInstancePtr() const { return &m_Lwda; }
    Lwca::Device GetBoundLwdaDevice() const;
    virtual void PrintDeviceProperties(const Tee::Priority pri);
    virtual int NumSecondaryCtx() { return 0; }
    virtual int GetDefaultActiveContext() { return 0; }
    RC ReportFailingSmids(const map<UINT16, UINT64>& errorCountMap,
                          const UINT64 failureLimit);

private:
    Lwca::Instance m_Lwda;
    Lwca::Device m_LwdaDevice;
};

#endif // INCLUDED_LWDASTREAMTEST_H
