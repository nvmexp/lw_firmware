/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2011,2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpuerrcount.h"
#include "gpu/tests/gputest.h"
#include "gpu/include/testdevice.h"

#if defined(INCLUDE_LWLINK) && defined(INCLUDE_GPUTEST)
#include "gpu/tests/lwlink/lwldatatest.h"
#endif

//--------------------------------------------------------------------
//! \brief constructor
//!
GpuErrCounter::GpuErrCounter(GpuSubdevice *pGpuSubdevice)
{
    MASSERT(pGpuSubdevice);
    m_pTestDevice = dynamic_cast<TestDevice*>(pGpuSubdevice);
}

//--------------------------------------------------------------------
//! \brief constructor
//!
GpuErrCounter::GpuErrCounter(TestDevice *pTestDevice) :
    m_pTestDevice(pTestDevice)
{
    MASSERT(pTestDevice);
}

//--------------------------------------------------------------------
//! Overrides the base-class function.  Returns false if the stack
//! frame is a GpuTest for a different GPU than the one passed to the
//! constructor, which tells the ErrCounter base class to ignore this
//! counter in that GpuTest.
//!
/* virtual */ bool GpuErrCounter::WatchInStackFrame(ModsTest *pTest) const
{
    if (!ErrCounter::WatchInStackFrame(pTest))
    {
        return false;
    }

    if (pTest && pTest->IsTestType(ModsTest::GPU_TEST))
    {
        GpuTest *pGpuTest = static_cast<GpuTest*>(pTest);
        if (pGpuTest->GetBoundTestDevice()->DevInst() == m_pTestDevice->DevInst())
            return true;

#if defined(INCLUDE_LWLINK) && defined(INCLUDE_GPUTEST)
        auto pDataTest = dynamic_cast<MfgTest::LwLinkDataTest*>(pTest);
        if (pDataTest &&
            (pDataTest->GetTestAllGpus() || m_pTestDevice->GetType() == TestDevice::TYPE_LWIDIA_LWSWITCH))
        {
            return true;
        }
#endif
        
        return false;
    }

    return true;
}

//-------------------------------------------------------------------
GpuSubdevice* GpuErrCounter::GetGpuSubdevice() const
{
    return m_pTestDevice->GetInterface<GpuSubdevice>();
}
