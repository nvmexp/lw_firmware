/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation.
 * Any use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_PLLLOCKRESTRICTION_H
#define INCLUDED_PLLLOCKRESTRICTION_H

class PllLockRestrictionCheck
{
public:
    bool IsPllLockRestricted(int pllType)
    {
        return DoIsPllLockRestricted(pllType);
    }

private:
    virtual bool DoIsPllLockRestricted(int pllType) = 0;
};

#endif
