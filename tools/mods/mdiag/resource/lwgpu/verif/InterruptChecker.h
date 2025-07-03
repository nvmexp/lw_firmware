/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_CRCINTERRUPTCHECKER_H
#define INCLUDED_CRCINTERRUPTCHECKER_H

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#include "GpuVerif.h"

class InterruptChecker: public IVerifChecker
{
public:
    TestEnums::TEST_STATUS Check(ICheckInfo* info);
    TestEnums::TEST_STATUS CheckIntrPassFail();

    virtual void Setup(GpuVerif* verifier);
    virtual void Cleanup();

    void StandaloneSetup(const ArgReader* params, ICrcProfile* crcProfile);

private:
    const string GetIntrFileName() const;

    GpuVerif* m_Verifier = nullptr;
    CrcEnums::CRC_MODE m_CrcMode = CrcEnums::CRC_CHECK;
    const ArgReader* m_Params;
    ICrcProfile* m_CrcProfile;
};

#endif /* INCLUDED_CRCINTERRUPTCHECKER_H */
