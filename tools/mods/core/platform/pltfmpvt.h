/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Private members of the platform namespace.  Platform implementations are
// the only things that care about this file.

#ifndef INCLUDED_PLATFORM_PRIVATE_H
#define INCLUDED_PLATFORM_PRIVATE_H

namespace Platform
{
    void CallAndClearCleanupFunctions();

    bool GetUse4ByteMemCopy ();
    RC SetUse4ByteMemCopy (bool b);

    void Hw4ByteMemCopy(volatile void *Dst, const volatile void *Src, size_t Count);

    bool GetSkipDisableUserInterface();
    RC SetSkipDisableUserInterface(bool b);

    UINT32 GetAllowedAddressBits(UINT32 AddressBits);
};

#endif

