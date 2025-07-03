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
#include "gpu/utility/lwlink/lwlinkdevif/lwl_libif.h"

//--------------------------------------------------------------------
//! \brief Xavier implementation used for T194 in CheetAh MODS
//!
class XavierTegraLwLink
  : public XavierLwLink
{
public:
    XavierTegraLwLink() { }
    virtual ~XavierTegraLwLink() {}
private:
    bool DoIsSupported(LibDevHandles handles) const override;
    LwLinkDevIf::LibIfPtr GetTegraLwLinkLibIf() override;
    RC GetRegPtr(void **ppRegs) override;
    bool DoHasNonPostedWrites() const override { return true; }
};
