/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_ibmnpu_p8p_dev.h"

//------------------------------------------------------------------------------
//! \brief Initialize the device
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::IbmP8PDev::Initialize()
{
    RC rc;
    if (m_Initialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : LwLink device %s already initialized, skipping\n",
               __FUNCTION__, GetName().c_str());
        return OK;
    }

    CHECK_RC(LwLinkDevIf::IbmNpuDev::Initialize());

    CHECK_RC(Pcie::Initialize());

    vector<LwLinkDevIf::LibInterface::LibDeviceHandle> handles;
    GetHandles(&handles);
    CHECK_RC(LwLink::Initialize(handles));

    m_Initialized = true;

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Shutdown the device
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::IbmP8PDev::Shutdown()
{
    StickyRC rc;

    rc = LwLink::Shutdown();

    Printf(Tee::PriLow, GetTeeModule()->GetCode(),
           "%s : Shutdown LwLink device %s\n", __FUNCTION__, GetName().c_str());
    m_Initialized = false;

    return rc;
}

