
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _ECOV_VERIFIER_H_
#define _ECOV_VERIFIER_H_

#include "event_cover.h"

#define  ENABLE_ECOV_DUMPING   1
#define  DISABLE_ECOV_DUMPING  0

class EcovVerifier
{
public:
    EcovVerifier(const ArgReader* tparams = NULL);

    ~EcovVerifier();

    enum TEST_STATUS
    {
        TEST_SUCCEEDED          = 0,
        TEST_FAILED_ECOV        = 1,
        TEST_ECOV_NON_EXISTANT  = 2,
    };

    EcovVerifier::TEST_STATUS VerifyECov();
    void AddCrcChain(const CrcChain* crcc);
    void SetGpuResourceParams(const ArgReader*);
    void SetJsParams(const ArgReader*);
    void SetTestName(const string& testName);
    void SetTestChipName(const string& testChipName);
    void SetupEcovFile(const string& ecovFileName);
    EventCover::Status ParseEcovFiles();
    EventCover::Status ParseEcovFile(const string& ecovFileName);
    void DumpEcovData(bool isCtxSwitching);

    const ArgReader* m_TestParams;
private:
    bool SetupSimEvent(const string& filename);
    void ClearEcovList();

    SimEventListType m_SimEvents;
    ECovListType     m_ECovList;
    CrcChain         m_CrcChain;

    const ArgReader* m_ResParams;
    const ArgReader* m_JsParams;
    const string m_DirSep;
    const string m_DirSep2;
    const string m_SpaceDelim;
    string m_TestName;
    string m_TestChipName;

    vector<string> m_EcovFileNames;
    vector<string> m_ClassNameIgnoreList;
};

#endif
