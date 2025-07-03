/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "xavier_mfg_lwlink.h"
#include "core/include/platform.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_libif.h"
#include "t19x/t194/address_map_new.h"

//------------------------------------------------------------------------------
RC XavierMfgLwLink::DoShutdown()
{
    StickyRC rc = XavierLwLink::DoShutdown();

    if (m_pRegs != nullptr)
    {
        Platform::UnMapDeviceMemory(m_pRegs);
        m_pRegs = nullptr;
    }
    return rc;
}

//------------------------------------------------------------------------------
RC XavierMfgLwLink::GetRegPtr(void **ppRegs)
{
    RC rc;
    if (m_pRegs == nullptr)
    {
        CHECK_RC(Platform::MapDeviceMemory(
                    &m_pRegs,
                    LW_ADDRESS_MAP_LWLINK_CFG_BASE,
                    LW_ADDRESS_MAP_LWLINK_CFG_SIZE,
                    Memory::UC,
                    Memory::ReadWrite));
    }
    *ppRegs = m_pRegs;
    return (m_pRegs == nullptr) ? RC::COULD_NOT_MAP_PHYSICAL_ADDRESS : rc;
}
