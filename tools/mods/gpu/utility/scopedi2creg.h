/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "device/interface/i2c.h"

//! Reads the I2C register value at the specified offset, then restores the original
//! value when going out of scope. Can be treated as a UINT32 after creation.
class ScopedI2cRegister
{
public:
    ScopedI2cRegister(I2c::Dev *pI2cDev, UINT32 offset)
    : m_pI2cDev(pI2cDev), m_Offset(offset), m_SavedValue(0), m_Active(false)
    {}

    ~ScopedI2cRegister()
    {
        if (m_Active)
            m_pI2cDev->Write(m_Offset, m_SavedValue);
    }

    operator UINT32() const
    {
        return m_SavedValue;
    }

    RC Save()
    {
        RC rc;
        MASSERT(m_pI2cDev);
        CHECK_RC(m_pI2cDev->Read(m_Offset, &m_SavedValue));
        m_Active = true;
        return rc;
    }

private:
    //! Do not allow copying or assignment.
    ScopedI2cRegister(const ScopedI2cRegister &other);
    ScopedI2cRegister &operator=(ScopedI2cRegister &other);

    I2c::Dev *m_pI2cDev;
    const UINT32 m_Offset;
    UINT32 m_SavedValue;
    bool  m_Active;
};

