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

/**
 * @file    dispstub.h
 * @brief   Stub out display functions until I have time to do something better
 */

#ifndef INCLUDED_DISPLAY_STUB_H
#define INCLUDED_DISPLAY_STUB_H

#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif
#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_BASIC_STUB_H
#include "bascstub.h"
#endif

class Display
{
public:
    UINT32 Selected();

    enum ColorCompression
    {
        ColorCompressionNoChange = 0,
        ColorCompressionNone = 1,
        ColorCompression2x1 = 2,
        ColorCompression2x2 = 3,
    };

    enum FilterTaps
    {
        FilterTapsNoChange = 0,
        FilterTapsNone = 1,
        FilterTaps2x = 2,
        FilterTaps4x = 4,
        FilterTaps5x = 5,
        FilterTaps8x = 8,
    };

    RC SetMode
    (
        UINT32 Display,
        UINT32 Width,
        UINT32 Height,
        UINT32 Depth,
        UINT32 RefreshRate,
        FilterTaps FilterTaps,
        ColorCompression ColorComp,
        UINT32 DdScalerMode
    );

protected:
    Display();

private:
    friend class DisplayPtr;
    BasicStub m_Stub;
};

class DisplayPtr
{
public:
    explicit DisplayPtr() {}

    static Display s_DisplayStub;

    Display * Instance()   const { return &s_DisplayStub; }
    Display * operator->() const { return &s_DisplayStub; }
    Display & operator*()  const { return s_DisplayStub; }
};

#endif
