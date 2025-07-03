/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tempctrl/hwaccess/ipmitempctrl.h"

//! \file    ipmitempctrl_stub.cpp
//! \brief   IPMI temperature ctrl stub functions (IPMI reading is only
//!          supported on Linux (the linux implementation lives at
//!          mods/linux/ipmitempctrl_impl.cpp)

//-----------------------------------------------------------------------------
RC IpmiTempCtrl::GetTempCtrlValViaIpmi
(
    vector<UINT08> &requestVal,
    vector<UINT08> &responseVal
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC IpmiTempCtrl::SetTempCtrlValViaIpmi
(
    vector<UINT08> &requestVal,
    vector<UINT08> &responseVal,
    vector<UINT08> &writeVal
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC IpmiTempCtrl::InitIpmiDevice()
{
    return RC::UNSUPPORTED_FUNCTION;
}
