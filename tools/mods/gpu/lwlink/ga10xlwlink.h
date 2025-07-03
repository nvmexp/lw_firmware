/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "amperelwlink.h"

class GA10xLwLink : public AmpereLwLink
{
protected:
    GA10xLwLink() = default;
private:
    FLOAT64 DoGetDefaultErrorThreshold
    (
        LwLink::ErrorCounts::ErrId errId,
        bool bRateErrors
    ) const;
    Version DoGetVersion() const override { return UphyIf::Version::V32; }
    void DoGetEomDefaultSettings
    (
        UINT32 link,
        EomSettings* pSettings
    ) const override;
    bool DoSupportsFomMode(FomMode mode) const override;
    RC GetEomConfigValue
    (
        FomMode mode,
        UINT32 numErrors,
        UINT32 numBlocks,
        UINT32 *pConfigVal
    ) const override;
};
