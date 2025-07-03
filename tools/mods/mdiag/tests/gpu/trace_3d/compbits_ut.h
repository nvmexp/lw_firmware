/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _COMPBITS_UT_H_
#define _COMPBITS_UT_H_

// Dependency eliminated:
// This header file shall not be included by any files except compbits_ut.cpp

#include "compbits.h"

namespace CompBits
{

class UnitTest: public CompBitsTest
{
public:
    UnitTest(TraceSubChannel* pSubChan)
        : CompBitsTest(pSubChan),
        m_Errors(0)
    {
    }

private:
    virtual ~UnitTest() {}

    virtual RC SetupTest() { return OK; }
    virtual RC DoPostRender();
    virtual void CleanupTest() {}
    virtual const char* TestName() const { return "UnitTest"; }

private:
    RC InitSurfCopier();
    void CleanupSurfCopier();
    // @return true if rc is OK.
    bool CheckError(const RC& rc)
    {
        m_Errors += OK == rc ? 0 : 1;
        return OK == rc;
    }

    // RWTest, Per offset compbits read/write test.
    RC RWTestRun();
    // RW64KTest, Per 64K page compbits read/write test.
    RC RW64KRun();
    // SurfCopyTest2, Unlike SurfCopyTest, this SurfCopyTest2 doesn't call
    // AMAP lib swizzleCompbitsTRC().
    RC SurfCopy2Run();

    CompBits::SurfCopier m_SurfCopier;
    int m_Errors;
};

} //namespace CompBits

#endif

