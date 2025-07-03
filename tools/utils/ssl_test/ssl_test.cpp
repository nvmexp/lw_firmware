/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "../lwdiagutils.h"

#define COL_SUCCESS "\033[1;42m"      // Green bg
#define COL_WARN    "\033[1;43;0;30m" // Yellow bg, black fg
#define COL_ERR     "\033[1;41m"      // Red bg
#define COL_RESET   "\033[0m"         // Normal terminal color

int main(int argc, char**argv)
{
    LwDiagUtils::Initialize(nullptr, nullptr);

    LwDiagUtils::EnableVerboseNetwork(true);

    const bool bOnNetwork = LwDiagUtils::IsOnLwidiaIntranet();
    if (bOnNetwork)
        LwDiagUtils::Printf(LwDiagUtils::PriNormal, COL_SUCCESS); // Green bg
    else
        LwDiagUtils::Printf(LwDiagUtils::PriNormal, COL_WARN); // Yellow bg, black fg

    LwDiagUtils::Printf(LwDiagUtils::PriNormal,
        "Exelwting%s on the lwpu network!" COL_RESET "\n", bOnNetwork ? "" : " NOT");
    if (bOnNetwork)
    {
        vector<char> serverData;
        LwDiagUtils::EC ec =
            LwDiagUtils::ReadLwidiaServerFile("/modswebapi/GetLookupTable/Linux/R400/400.199",
                                              &serverData);

        if (ec == LwDiagUtils::OK)
        {
            LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                                COL_SUCCESS "Reading server file succeeded" COL_RESET "\n");
            LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                                COL_SUCCESS "Server file size %u" COL_RESET "\n",
                                static_cast<UINT32>(serverData.size()));
        }
        else
        {
            LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                                COL_ERR "Reading server file failed with ec = %u" COL_RESET "\n",
                                static_cast<UINT32>(ec));
        }
    }
    else
    {
        LwDiagUtils::Printf(LwDiagUtils::PriNormal,
            COL_WARN "Server file check skipped due to not exelwting on network" COL_RESET "\n");
    }
    LwDiagUtils::Shutdown();
    return 0;
}
