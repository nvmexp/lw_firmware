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

#include "core/include/security.h"

#include "core/include/cmdline.h"
#include "core/include/utility.h"

using Sul = Security::UnlockLevel;

namespace
{
    static Sul s_SelwrityUnlockLevel = Sul::UNKNOWN;
    static bool s_bOnLwidiaIntranet = false;
}

void Security::InitSelwrityUnlockLevel(Sul selwrityUnlockLevel)
{
    MASSERT(s_SelwrityUnlockLevel == Sul::UNKNOWN);
    DEFER
    {
        if (s_SelwrityUnlockLevel >= Sul::LWIDIA_NETWORK)
            Printf(Tee::PriLow, "Security unlock level = %d\n",
                   static_cast<INT32>(s_SelwrityUnlockLevel));
    };

#if defined(SIM_BUILD) || defined(DIRECTAMODEL)
    s_SelwrityUnlockLevel = Sul::LWIDIA_NETWORK;
#else
    s_SelwrityUnlockLevel = Sul::NONE;

    LwDiagUtils::EnableVerboseNetwork(selwrityUnlockLevel >= Sul::LWIDIA_NETWORK);

    s_bOnLwidiaIntranet = !CommandLine::NoNet() &&
                           LwDiagUtils::IsOnLwidiaIntranet();
    if (!s_bOnLwidiaIntranet || (selwrityUnlockLevel > Sul::LWIDIA_NETWORK))
        s_SelwrityUnlockLevel = selwrityUnlockLevel;
    else if (s_bOnLwidiaIntranet)
    {
        if (selwrityUnlockLevel == Sul::BYPASS_UNEXPIRED)
        {
            s_SelwrityUnlockLevel = Sul::LWIDIA_NETWORK_BYPASS;
        }
        else
        {
            s_SelwrityUnlockLevel = Sul::LWIDIA_NETWORK;
        }
    }
#endif
}

Sul Security::GetUnlockLevel()
{
    MASSERT(s_SelwrityUnlockLevel != Sul::UNKNOWN);
    return s_SelwrityUnlockLevel;
}

bool Security::OnLwidiaIntranet()
{
    return s_bOnLwidiaIntranet;
}

bool Security::CanDisplayInternalInfo()
{
    return Security::GetUnlockLevel() >= Sul::LWIDIA_NETWORK;
}
