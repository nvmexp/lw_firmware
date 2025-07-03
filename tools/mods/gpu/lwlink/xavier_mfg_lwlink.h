/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "xavierlwlink.h"
#include "gpu/include/testdevice.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_libif.h"

//--------------------------------------------------------------------
//! \brief Xavier implementation used for T194 in Linuxmfg mods
//!
class XavierMfgLwLink
  : public XavierLwLink
{
public:
    XavierMfgLwLink() { }
    virtual ~XavierMfgLwLink() {}
private:
    LwLinkDevIf::LibIfPtr GetTegraLwLinkLibIf() override { return GetDevice()->GetLibIf(); }
    RC DoShutdown() override;
    RC GetRegPtr(void **ppRegs) override;
    bool DoHasNonPostedWrites() const override { return false; }

    void * m_pRegs = nullptr;
};
