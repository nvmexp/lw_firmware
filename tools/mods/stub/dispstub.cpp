/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "dispstub.h"

Display DisplayPtr::s_DisplayStub;

//------------------------------------------------------------------------------
Display::Display()
{
    m_Stub.SetStubMode(BasicStub::AllFail); // Sort of
    m_Stub.SetDefaultUINT32(0);
}

//------------------------------------------------------------------------------
UINT32 Display::Selected()
{
    return m_Stub.LogCallUINT32("Display::Selected");
}

//------------------------------------------------------------------------------
RC Display::SetMode
(
    UINT32 Display,
    UINT32 Width,
    UINT32 Height,
    UINT32 Depth,
    UINT32 RefreshRate,
    FilterTaps FilterTaps,
    ColorCompression ColorComp,
    UINT32 DdScalerMode
)
{
    return m_Stub.LogCallRC("Display::SetMode");
}
