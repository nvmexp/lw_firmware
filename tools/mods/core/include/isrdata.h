/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_ISRDATA_H
#define INCLUDED_ISRDATA_H

#include "types.h"

class IsrData
{
public:
    IsrData(ISR isr, void* cookie)   : m_Isr(isr), m_Cookie(cookie) {}
    ~IsrData() {}

    bool operator==(const IsrData& rhs) const
    {
        return (m_Isr == rhs.m_Isr && m_Cookie == rhs.m_Cookie);
    }

    bool operator<(const IsrData& rhs) const
    {
        return (m_Isr < rhs.m_Isr && m_Cookie < rhs.m_Cookie);
    }

    long operator()() const
    {
        if (m_Isr)
            return m_Isr(m_Cookie);
        else
            return 0; // Equivalent to Xp::ISR_RESULT_NOT_SERVICED
    }

    ISR   GetIsr()    const { return m_Isr; }
    void* GetCookie() const { return m_Cookie; }

private:
    ISR           m_Isr;
    void* m_Cookie;
};

#endif // INCLUDED_ISRDATA_H
