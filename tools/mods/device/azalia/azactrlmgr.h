/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_AZAMGR_H
#define INCLUDED_AZAMGR_H

#include "cheetah/include/controllermgr.h"
#include "azactrl.h"

DECLARE_CONTROLLER_MGR(AzaliaController);

namespace AzaliaControllerMgr
{
    extern bool s_IsIntelAzaliaAllowed;
}

#endif // INCLUDED_AZAMGR_H

