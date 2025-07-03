/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/mods_gdm_client.h"


RC ModsGdmClient::InitModsGdmClient(string& gdmAddress)
{
    /*Stub function*/
    return RC::OK;
}

void ModsGdmClient::StopModsGdmClient()
{
    /*Stub function*/
}

ModsGdmClient::ModsGdmClient()
{
}

ModsGdmClient& ModsGdmClient::Instance()
{
    static ModsGdmClient modsGdmClientInstance;
    return modsGdmClientInstance;
}

void ModsGdmClient::SendMonitorHeartBeat()
{
    /* Stub function */
}
