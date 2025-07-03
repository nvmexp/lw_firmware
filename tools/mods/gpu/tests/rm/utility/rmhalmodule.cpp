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
//! \file rmhalmodule.cpp
//! \brief Implementation file of base RM Hal
//!

#include "rmhalmodule.h"

RMTHalModule::RMTHalModule(LwU32 hClient, LwU32 hDev, LwU32 hSubDev)
:m_hClient(hClient),
 m_hDev(hDev),
 m_hSubDev(hSubDev)
{
}
