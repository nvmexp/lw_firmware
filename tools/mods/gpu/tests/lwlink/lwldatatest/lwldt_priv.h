/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_LWLDT_PRIV_H
#define INCLUDED_LWLDT_PRIV_H

#include "core/include/types.h"
#include <string>
#include <memory>

class Surface2D;

namespace LwLinkDataTestHelper
{
    //! Typdef used for sharing surface 2D pointers in lwlink BW stress
    typedef shared_ptr<Surface2D> Surface2DPtr;

    //! Enumeration specifying transfer direction of data relative to the DUT
    enum TransferDir
    {
        TD_IN_ONLY   = (1 << 0)
       ,TD_OUT_ONLY  = (1 << 1)
       ,TD_IN_OUT    = (1 << 2)
       ,TD_ALL       = (TD_IN_ONLY | TD_OUT_ONLY | TD_IN_OUT)
       ,TD_ILWALID   = 0
    };

    //! Enumeration specifying transfer type being performed
    enum TransferType
    {
        TT_SYSMEM           = (1 << 0)
       ,TT_P2P              = (1 << 1)
       ,TT_LOOPBACK         = (1 << 2)
       ,TT_SYSMEM_LOOPBACK  = (TT_SYSMEM | TT_LOOPBACK)
       ,TT_ALL       = (TT_SYSMEM | TT_P2P | TT_LOOPBACK)
       ,TT_ILWALID   = 0
    };

    enum
    {
        ALL_CES = ~0U
    };

    string TransferDirStr(TransferDir td);
    string TransferDirMaskStr(UINT32 tdMask);
    string TransferTypeStr(TransferType tt);
    string TransferTypeMaskStr(UINT32 ttMask);
};

#endif // INCLUDED_LWLDT_PRIV_H
