/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mdiag/tests/test.h"

class ArgReader;

extern const ParamDecl rtapi_params[];

class RTApiTest : public Test
{
    public:
        RTApiTest(ArgReader *reader);
        virtual int Setup(void);
        virtual void PreRun(void) {};

        virtual void Run(void); 

        virtual void PostRun(void){};

        virtual void CleanUp(void);
        virtual void CleanUp(UINT32 stage);
        static Test* Factory(ArgDatabase *args);

    protected:
        void WaitForSocBrInitDone();
        void HandleSoArgs();

        const ArgReader *m_pParams;
        UINT64 m_TimeoutMs;

    private:
        void RunNitroCTest();
        void ParseSoArgs(vector<const char*>& soargs);
        void AssertSoArgs(bool assert);

        using pFnExecPhase = int (*)(int phase_id, int routine_idx, int processor_id);
        pFnExecPhase m_pExecPhase;

        void *m_testHandle;
};

class CpuModeTest : public RTApiTest
{
    public:
        CpuModeTest(ArgReader *reader);

        virtual int Setup(void);
        virtual void Run(void);
    private:
        void LoadAxf(const char* path);
        void PollTestResult();
        void HandleTestArgs();
};

#ifdef MAKE_TEST_LIST
        CREATE_TEST_LIST_ENTRY(rtapi, RTApiTest, "run RTAPI test on mods");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &rtapi_testentry
#endif
