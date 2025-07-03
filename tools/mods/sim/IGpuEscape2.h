/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009,2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#ifndef _IGPUESCAPE2_H_
#define _IGPUESCAPE2_H_

#include "ITypes.h"
#include "IIface.h"

class IGpuEscape2 : public IIfaceObject
{
public:
    virtual LwErr EscapeWriteBuffer
    (
        LwU032      gpuId,
        const char* path,
        LwU032      index,
        size_t      size,
        const void* buf
    )
    {
        return LW_FAIL;
    }

    virtual LwErr EscapeReadBuffer
    (
        LwU032      gpuId,
        const char* path,
        LwU032      index,
        size_t      size,
        void*       buf
    )
    {
        return LW_FAIL;
    }

    virtual LwU032 GetGpuId
    (
        LwU032      bus,
        LwU032      dev,
        LwU032      fnc
    )
    {
        return 0;
    }
};

#endif
