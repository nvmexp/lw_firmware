/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "bascstub.h"
#include "core/include/massert.h"

//------------------------------------------------------------------------------
BasicStub::BasicStub() : m_DefaultBool(false), m_DefaultUINT32(0xFFFFFFFF)
{
    SetStubMode(AllFail);
}

// static member function (private)
//------------------------------------------------------------------------------
void BasicStub::LogHelper(Tee::Priority pri, const char *msg, StubMode m)
{
    Printf(pri, "%s is a stub\n", msg);
    if(m == AllFail)
    {
        MASSERT(!"Called a stub function in AllFail mode...did you mean to do this?");
    }
}

// static member function
//------------------------------------------------------------------------------
RC BasicStub::LogCallRC(Tee::Priority pri, const char *msg, RC toRet, StubMode m /* = AllPass */)
{
    BasicStub::LogHelper(pri, msg, m);
    return toRet;
}

// static member function
//------------------------------------------------------------------------------
bool BasicStub::LogCallBool(Tee::Priority pri, const char *msg, bool toRet, StubMode m /* = AllPass */)
{
    BasicStub::LogHelper(pri, msg, m);
    return toRet;
}

// static member function
//------------------------------------------------------------------------------
void BasicStub::LogCallVoid(Tee::Priority pri, const char *msg, StubMode m /* = AllPass */)
{
    BasicStub::LogHelper(pri, msg, m);
}

// static member function
//------------------------------------------------------------------------------
UINT32 BasicStub::LogCallUINT32(Tee::Priority pri, const char *msg, UINT32 toRet, StubMode m /* = AllPass */)
{
    BasicStub::LogHelper(pri, msg, m);
    return toRet;
}

//------------------------------------------------------------------------------
RC BasicStub::LogCallRC(const char *msg) const
{
    return BasicStub::LogCallRC(m_Pri, msg, m_DefaultRC, m_StubMode);
}

//------------------------------------------------------------------------------
bool BasicStub::LogCallBool(const char *msg) const
{
    return BasicStub::LogCallBool(m_Pri, msg, m_DefaultBool, m_StubMode);
}

//------------------------------------------------------------------------------
void BasicStub::LogCallVoid(const char *msg) const
{
    BasicStub::LogCallVoid(m_Pri, msg, m_StubMode);
}

//------------------------------------------------------------------------------
UINT32 BasicStub::LogCallUINT32(const char *msg) const
{
    return BasicStub::LogCallUINT32(m_Pri, msg, m_DefaultUINT32, m_StubMode);
}

//------------------------------------------------------------------------------
void BasicStub::SetStubMode(StubMode m)
{
    m_StubMode = m;

    switch(m)
    {
        case AllPass:
            m_Pri = Tee::PriSecret;
            m_DefaultRC = OK;
            break;
        case AllFail:
            m_Pri = Tee::PriHigh;
            m_DefaultRC = RC::UNSUPPORTED_FUNCTION;
            break;
        default:
            break;
    }
}

//------------------------------------------------------------------------------
void BasicStub::SetDefaultPrintPri(Tee::Priority pri)
{
    m_Pri = pri;
}

//------------------------------------------------------------------------------
void BasicStub::SetDefaultRC(RC rc)
{
    m_DefaultRC = rc;
}

//------------------------------------------------------------------------------
void BasicStub::SetDefaultBool(bool b)
{
    m_DefaultBool = b;
}

//------------------------------------------------------------------------------
void BasicStub::SetDefaultUINT32(UINT32 v)
{
    m_DefaultUINT32 = v;
}

//------------------------------------------------------------------------------
BasicStub::StubMode BasicStub::GetStubMode()
{
    return m_StubMode;
}

//------------------------------------------------------------------------------
Tee::Priority BasicStub::GetDefaultPrintPri()
{
    return m_Pri;
}

//------------------------------------------------------------------------------
RC BasicStub::GetDefaultRC()
{
    return m_DefaultRC;
}

//------------------------------------------------------------------------------
bool BasicStub::GetDefaultBool()
{
    return m_DefaultBool;
}

//------------------------------------------------------------------------------
UINT32 BasicStub::GetDefaultUINT32()
{
    return m_DefaultUINT32;
}

