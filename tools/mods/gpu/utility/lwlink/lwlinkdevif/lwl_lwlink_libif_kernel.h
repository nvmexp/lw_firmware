/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "lwl_lwlink_libif.h"

extern "C"
{
    #include "lwlink_user_api.h"
}

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief LwLink core library interface definition for interacting with the
    //! lwlink core library when it is compiled directly into MODS
    class LwLinkLibIfKernel : public LwLinkLibInterface
    {
    public:
        LwLinkLibIfKernel() = default;
        virtual ~LwLinkLibIfKernel() = default;
    private:
        RC Control
        (
            UINT32 controlId,
            void * pParams,
            UINT32 paramSize
        ) override;
        RC Initialize() override;
        bool IsInitialized() override { return m_bInitialized; }
        RC Shutdown() override;

    private:
        bool m_bInitialized = false;
        lwlink_session* m_pSession = nullptr;
    };
};

