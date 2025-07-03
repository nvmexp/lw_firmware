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

//! \file turingppc.h -- concrete implementation for Port Policy Controller on Turing

#pragma once

#include "gpu/usb/portpolicyctrl/portpolicyctrlimpl.h"

class TuringPPC : public PortPolicyCtrlImpl
{
public:
    TuringPPC() = default;
    virtual ~TuringPPC() {}

protected:
    bool DoIsSupported() const override;

    RC DoGetPciDBDF
    (
        UINT32 *pDomain   = nullptr,
        UINT32 *pBus      = nullptr,
        UINT32 *pDevice   = nullptr,
        UINT32 *pFunction = nullptr
    ) const override;

    RC DoWaitForConfigToComplete() const override;
    RC DoIsOrientationFlipped(bool *pIsOrientationFlipped) const override;
};
