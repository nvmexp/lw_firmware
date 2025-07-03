/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_LWL_LWLINK_LIBIF_H
#define INCLUDED_LWL_LWLINK_LIBIF_H

#include "core/include/types.h"
#include "core/include/rc.h"

#include <memory>

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief LwLink library interface definition for interacting with the
    //! lwlink core library.  There will be multiple implementations of this
    //! depending user vs. kernel mode
    class LwLinkLibInterface
    {
        public:
            virtual ~LwLinkLibInterface() {}
            virtual RC Control
            (
                UINT32 controlId,
                void * pParams,
                UINT32 paramSize
            ) = 0;
            virtual RC Initialize() = 0;
            virtual bool IsInitialized() = 0;
            virtual RC Shutdown() = 0;
    };

    typedef shared_ptr<LwLinkLibInterface> LwLinkLibIfPtr;
};

#endif
