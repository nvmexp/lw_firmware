/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _TRACE_AND_2D_H_
#define _TRACE_AND_2D_H_

#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"

#include "mdiag/utils/surfutil.h"
#include "mdiag/utils/utils.h"
#include "mdiag/tests/test_state_report.h"
#include "teegroups.h"

#include "mdiag/utils/profile.h"
#include <string.h>

template<class T> class TraceAnd2dTest : public T /*was: TraceCelsiusTest*/
{
public:
    TraceAnd2dTest(ArgReader *reader) : T /*TraceCelsiusTest*/ (reader)
    {
        if (reader->ParamPresent("-forever")) {
            forever = true;
            InfoPrintf("%s: trace_and_2d.cpp Forever option specified.\n", T::GetTestName());
        } else {
            forever = false;
        }

        if (T::params->ParamPresent("-nTimes")) {
            nTimes = T::params->ParamUnsigned("-nTimes");
        } else {
            nTimes = 1;
        }

        separate2D = T::params->ParamPresent("-separate") > 0;
        // make separate2D the default. lw34 need to avoid -doubleXor and -separate as the default
        separate2D = true;

        onlyUpdatePutPtrOnce = !!T::params->ParamPresent("-burst_write");
        buf = NULL;

        m_doV2d = T::params->ParamPresent( "-v2d" ) > 0;
        MASSERT(m_doV2d); // all trace_and_2d tests should be v2d-based now, not legacy 2D
        m_v2dScriptFileName = T::params->ParamStr( "-v2d" );
        if ( T::params->ParamPresent( "-v2dvars" ) > 0 )
            m_v2dVariableInitStr = T::params->ParamStr( "-v2dvars" );
        m_v2dDoCheckCrc = T::params->ParamPresent( "-v2dCheckCrc" ) > 0;
        m_v2dExpectedCrc = T::params->ParamStr( "-v2dCheckCrc", "0" );
    }

    virtual ~TraceAnd2dTest() {}

protected:
    int WriteMethod(int subchannel, UINT32 address, UINT32 data)
    {
        if (T::params->ParamPresent("-method_dump")) {
            InfoPrintf("Method dump: subchannel %01d, address %08x, data %08x\n", subchannel, address, data);
            return 1;
        } else {
            return T::ch->MethodWrite(subchannel, address, data);
        }
    }

    void FailRun()
    {
        T::SetStatus(Test::TEST_FAILED);
    }

    int separate2D;

    bool forever;
    int nTimes;
    bool onlyUpdatePutPtrOnce;
    Buf* buf;

    //where we are in the trace
    UINT32 ptr;
    bool m_doV2d;
    string m_v2dScriptFileName;
    string m_v2dVariableInitStr;
    bool m_v2dDoCheckCrc;
    string m_v2dExpectedCrc;
};

#endif
