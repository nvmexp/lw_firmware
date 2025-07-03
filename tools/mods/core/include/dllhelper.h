/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

//--------------------------------------------------------------------
//! \brief Helper functions for implementing Xp::LoadDLL() et al
//!
namespace DllHelper
{
    RC Initialize();
    RC Shutdown();

    void *GetMutex();
    string AppendMissingSuffix(const string &fileName);
    RC RegisterModuleHandle(void *moduleHandle, bool unloadOnExit);
    RC UnregisterModuleHandle(void *moduleHandle);
}
