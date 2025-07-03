/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  bascstub.h
 * @brief Some common stub functionality/utilities
 *
 *
 */

#ifndef INCLUDED_BASIC_STUB_H
#define INCLUDED_BASIC_STUB_H

#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

#ifndef INCLUDED_TEEBUF_H
#include "core/include/tee.h"
#endif

class BasicStub
{
public:
    enum StubMode
    {
        AllPass,
        AllFail,
        FrankenStub
    };

    BasicStub();

    // These lists will grow as I unify some code
    static RC LogCallRC(Tee::Priority pri, const char *msg, RC toRet, StubMode m = AllPass);
    static bool LogCallBool(Tee::Priority pri, const char *msg, bool toRet, StubMode m = AllPass);
    static void LogCallVoid(Tee::Priority pri, const char *msg, StubMode m = AllPass);
    static UINT32 LogCallUINT32(Tee::Priority pri, const char *msg, UINT32 toRet, StubMode m = AllPass);

    RC LogCallRC(const char *msg) const;
    bool LogCallBool(const char *msg) const;
    void LogCallVoid(const char *msg) const;
    UINT32 LogCallUINT32(const char *msg) const;

    // Maintain some internals
    void SetStubMode(StubMode m);
    void SetDefaultPrintPri(Tee::Priority pri);
    void SetDefaultRC(RC rc);
    void SetDefaultBool(bool b);
    void SetDefaultUINT32(UINT32 v);

    StubMode GetStubMode();
    Tee::Priority GetDefaultPrintPri();
    RC GetDefaultRC();
    bool GetDefaultBool();
    UINT32 GetDefaultUINT32();

private:
    static void LogHelper(Tee::Priority pri, const char *msg, StubMode m);

    StubMode m_StubMode;
    Tee::Priority m_Pri;
    RC m_DefaultRC;
    bool m_DefaultBool;
    UINT32 m_DefaultUINT32;
    // etc.
};

#endif // INCLUDED_BASIC_STUB_H

