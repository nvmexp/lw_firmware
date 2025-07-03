/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file voltappc.h -- concrete implementation for Port Policy Controller on Volta

// Volta + Cypress CCG4 EvalKit will be used for testing USB-C Port Policy
// Controller Feature prior to Turing Bring-up

#pragma once

#include "gpu/usb/portpolicyctrl/portpolicyctrlimpl.h"

class VoltaPPC : public PortPolicyCtrlImpl
{
public:
    VoltaPPC() = default;
    virtual ~VoltaPPC() {}

protected:
    RC DoGetPciDBDF
    (
        UINT32 *pDomain   = nullptr,
        UINT32 *pBus      = nullptr,
        UINT32 *pDevice   = nullptr,
        UINT32 *pFunction = nullptr
    ) const override;

    RC DoWaitForConfigToComplete() const override;
};
