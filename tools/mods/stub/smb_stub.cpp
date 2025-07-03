/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011,2014,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/types.h"
#include "smb_stub.h"

RC SmbDevice::GetNumSubDev( UINT32 * pNumSubDev )
{
    *pNumSubDev = 0;
    return OK;
}

RC SmbManager::GetLwrrent(McpDev  **smbdev)
{
    *smbdev = nullptr;
    return OK;
}

namespace SmbMgr
{
    SmbManager *Mgr() { return NULL; }
}
