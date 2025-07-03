/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GPUERRCOUNT_H
#define INCLUDED_GPUERRCOUNT_H

#ifndef INCLUDED_ERRCOUNT_H
#include "core/include/errcount.h"
#endif

class ModsTest;
class GpuSubdevice;
class TestDevice;

//--------------------------------------------------------------------
//! \brief ErrCounter subclass with an associated GpuSubdevice
//!
//! This ErrCounter subclass is for counters that have an associated
//! GpuSubdevice.  If a subclass is used to detect errors in mods
//! tests (coverage = MODS_TEST), then it automatically skips any
//! tests that are for a different GpuSubdevice.  This makes it
//! clearer which test caused the error in -conlwrrent_devices.
//!
class GpuErrCounter : public ErrCounter
{
public:
    GpuErrCounter(GpuSubdevice *pGpuSubdevice);
    GpuErrCounter(TestDevice *pGpuSubdevice);
    virtual bool WatchInStackFrame(ModsTest *pTest) const;
    GpuSubdevice *GetGpuSubdevice() const;
    TestDevice *GetTestDevice() const { return m_pTestDevice; }

protected:
    TestDevice *m_pTestDevice;
};

#endif // INCLUDED_GPUERRCOUNT_H
