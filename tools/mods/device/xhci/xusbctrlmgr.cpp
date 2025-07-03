/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "xusbctrlmgr.h"
#include "cheetah/include/controllermgr.h"
#include "cheetah/include/devid.h"

DEFINE_CONTROLLER_MGR(XusbController, LW_DEVID_XUSB_DEV);

RC XusbControllerManager::PrivateValidateController(UINT32 Index)
{
    return OK;
}
