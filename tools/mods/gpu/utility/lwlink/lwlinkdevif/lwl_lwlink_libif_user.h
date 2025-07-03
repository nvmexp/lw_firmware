/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_LWL_LWLINK_LIBIF_USER_H
#define INCLUDED_LWL_LWLINK_LIBIF_USER_H

#include "lwl_lwlink_libif.h"

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief LwLink core library interface definition for interacting with the
    //! lwlink core library when it is compiled directly into MODS
    class LwLinkLibIfUser : public LwLinkLibInterface
    {
    public:
        LwLinkLibIfUser() : m_bInitialized(false) {}
        virtual ~LwLinkLibIfUser() { }
    private:
        virtual RC Control
        (
            UINT32 controlId,
            void * pParams,
            UINT32 paramSize
        );
        virtual RC Initialize();
        virtual bool IsInitialized() { return m_bInitialized; }
        virtual RC Shutdown();

        bool m_bInitialized;
    };
};

#endif
