/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015,2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_devif.h"
#include "lwl_devif_fact.h"
#include "core/include/tee.h"

namespace LwLinkDevIf
{
    //! Tee module for printing within the LwLinkDevIf namespace
    TeeModule s_LwLinkDevIfModule("LwLinkDevIf");
#ifdef INCLUDE_LWLINK
    LwLinkLibIfPtr s_pLwLinkLibIf;
#endif
#ifdef LWCPU_AARCH64
    LibIfPtr s_pTegraLibIf;
#endif
};

//------------------------------------------------------------------------------
//! \brief Get a pointer to the tee module for printing within the LwLinkDevIf
//! namespace
//!
//! \return Pointer to tee module
TeeModule * LwLinkDevIf::GetTeeModule()
{
    return &s_LwLinkDevIfModule;
}

//------------------------------------------------------------------------------
LwLinkDevIf::LwLinkLibIfPtr LwLinkDevIf::GetLwLinkLibIf()
{
#ifdef INCLUDE_LWLINK
    return s_pLwLinkLibIf;
#else
    return nullptr;
#endif
}

//-----------------------------------------------------------------------------
LwLinkDevIf::LibIfPtr LwLinkDevIf::GetTegraLibIf()
{
#if defined(INCLUDE_LWLINK) && defined(LWCPU_AARCH64)
    return s_pTegraLibIf;
#else
    return nullptr;
#endif
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::Initialize()
{
#ifdef INCLUDE_LWLINK
    s_pLwLinkLibIf = Factory::CreateLwLinkLibInterface();

    RC rc;
    if (!s_pLwLinkLibIf)
    {
        Printf(Tee::PriLow, s_LwLinkDevIfModule.GetCode(),
               "%s : Unable to create lwlink core library interface!\n",
               __FUNCTION__);
    }
    else
    {
        CHECK_RC(s_pLwLinkLibIf->Initialize());
    }

#ifdef LWCPU_AARCH64
    s_pTegraLibIf = LibInterface::CreateTegraLibIf();
    if (!s_pTegraLibIf)
    {
        Printf(Tee::PriLow, s_LwLinkDevIfModule.GetCode(),
               "%s : Unable to create lwlink cheetah core library interface!\n",
               __FUNCTION__);
    }
    else
    {
        CHECK_RC(s_pTegraLibIf->Initialize());
    }
#endif
    return rc;
#else
    return OK;
#endif
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::Shutdown()
{
    StickyRC rc;

#ifdef INCLUDE_LWLINK
    if (s_pLwLinkLibIf)
    {
        rc = s_pLwLinkLibIf->Shutdown();
        s_pLwLinkLibIf.reset();
    }
#ifdef LWCPU_AARCH64
    if (s_pTegraLibIf && s_pTegraLibIf->IsInitialized())
    {
        rc = s_pTegraLibIf->Shutdown();
        s_pTegraLibIf.reset();
    }
#endif
#endif

    return rc;
}
