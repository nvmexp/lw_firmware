/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwlthroughputcountersimpl.h"
#include "core/include/platform.h"
#include "core/include/coreutility.h"
#include "device/interface/lwlink.h"
#include "gpu/include/testdevice.h"

//------------------------------------------------------------------------------
bool LwLinkThroughputCountersImpl::DoIsSupported() const
{
    return (GetDevice()->SupportsInterface<LwLink>() &&
           (Platform::GetSimulationMode() == Platform::Hardware));
}

//------------------------------------------------------------------------------
//! \brief Check that the link mask is valid
//!
//! \param linkMask : Link mask to check
//! \param reason   : Reason string for failure
//!
//! \return RC::OK if successful, not RC::OK otherwise
RC LwLinkThroughputCountersImpl::DoCheckLinkMask(UINT64 linkMask, const char * reason) const
{
    StickyRC rc;
    INT32 lwrLink = Utility::BitScanForward(linkMask, 0);
    const LwLink * pLwLink = GetDevice()->GetInterface<LwLink>();
    while (lwrLink != -1)
    {
        if (!pLwLink->IsLinkValid(lwrLink))
        {
            Printf(Tee::PriError,
                   "Unknown linkId %u %s\n",
                   lwrLink, reason);
            rc = RC::BAD_PARAMETER;
        }

        if (!pLwLink->IsLinkActive(lwrLink))
        {
            Printf(Tee::PriError,
                   "Inactive linkId %u %s\n",
                   lwrLink, reason);
            rc = RC::SOFTWARE_ERROR;
        }
        lwrLink = Utility::BitScanForward(linkMask, lwrLink + 1);
    }
    return RC::OK;
}

