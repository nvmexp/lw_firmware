/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmhalmodule.h
//! \brief Base class for rmtest hal module
//!

#include "gpu/tests/rmtest.h"
#include "core/include/tee.h"
#include "core/include/lwrm.h"

class RMTHalModule
{
protected:
    RMTHalModule(LwU32 hClient, LwU32 hDev, LwU32 hSubDev);
    LwRm::Handle m_hClient;
    LwRm::Handle m_hDev;
    LwRm::Handle m_hSubDev;
};
