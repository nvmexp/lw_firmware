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

// Render a solid rectangle.

#ifndef INCLUDED_RECT_H
#define INCLUDED_RECT_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

class Notifier;

/**
 * @class(Rectangle)
 *
 * Render a solid rectangle.
 */

class Rectangle
{
public:
    // CREATORS
    Rectangle();
    ~Rectangle();

    // MANIPULATORS
    RC Draw
    (
        UINT32 Offset,
        UINT32 StartX,
        UINT32 StartY,
        UINT32 Color,
        UINT32 Width,
        UINT32 Height,
        UINT32 Depth,
        UINT32 Pitch
    );

private:
    Notifier * m_pNotifier;
};

#endif // ! INCLUDED_RECT_H
