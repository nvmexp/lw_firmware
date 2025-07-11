/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/pmgrmutex.h"
#include "gpu/include/gpusbdev.h"
#include "mods_reg_hal.h"
#include "gpu/reghal/reghal.h"
#include "core/include/massert.h"
#include "core/include/platform.h"
#include "../../arch/lwalloc/common/inc/lw_mutex.h"
#include <vector>

// Declare a bunch of externs for the PMGR mutex tables generated by
// genreghalimpl.py
//
namespace PmgrMutexTableInfo
{
#define DEFINE_NEW_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, ...) \
    extern const Table g_ ## GpuId ## PmgrMutexIds;
#define DEFINE_DUP_GPU(...) // don't care
#define DEFINE_OBS_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_NEW_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_OBS_GPU
}

//------------------------------------------------------------------------------
PmgrMutexHolder::PmgrMutexHolder(GpuSubdevice *pSubdev, MutexId mid)
 : m_pSubdev(pSubdev)
{
    // Use a switch statement to find the compile-time HAL table
    //
    const PmgrMutexTableInfo::Table *pTable = nullptr;
    switch (pSubdev->DeviceId())
    {
#define DEFINE_NEW_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, ...)        \
        case Device::GpuId:                                             \
            pTable = &PmgrMutexTableInfo::g_ ## GpuId ## PmgrMutexIds;  \
            break;
#define DEFINE_DUP_GPU(...) // don't care
#define DEFINE_OBS_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_NEW_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_OBS_GPU
        default:
            Printf(Tee::PriError,
                   "No PMGR mutex table defined for %s\n",
                   m_pSubdev->DeviceShortName().c_str());
            m_ErrorRc = RC::SOFTWARE_ERROR;
            return;
    }

    INT32 logicalMid = LW_MUTEX_ID_ILWALID;
    switch (mid)
    {
        case MI_HBM_IEEE1500:
            logicalMid = LW_MUTEX_ID_HBM_IEEE1500;
            break;
        case MI_THERM_TSENSE_SNAPSHOT:
            logicalMid = LW_MUTEX_ID_THERM_GLOBAL_SNAPSHOT;
            break;
        default:
            Printf(Tee::PriError, "Unknown mutex ID %d\n", mid);
            m_ErrorRc = RC::SOFTWARE_ERROR;
            return;
    }
    for (size_t idx = 0; idx < pTable->size; ++idx)
    {
        if (pTable->pElements[idx] == logicalMid)
        {
            m_PhysMutexId = static_cast<UINT32>(idx);
            break;
        }
    }

    if (m_PhysMutexId == ~0U)
    {
        Printf(Tee::PriError,
               "Mutex %d not defined on %s\n",
               mid, m_pSubdev->DeviceShortName().c_str());
        m_ErrorRc = RC::UNSUPPORTED_HARDWARE_FEATURE;
        return;
    }

    const RegHal &regs = pSubdev->Regs();

    // Ensure all appropriate registers are supported
    if (!regs.IsSupported(MODS_PMGR_MUTEX_ID_ACQUIRE_VALUE) ||
        !regs.IsSupported(MODS_PMGR_MUTEX_ID_ACQUIRE_VALUE_INIT) ||
        !regs.IsSupported(MODS_PMGR_MUTEX_ID_ACQUIRE_VALUE_NOT_AVAIL) ||
        !regs.IsSupported(MODS_PMGR_MUTEX_ID_RELEASE_VALUE) ||
        !regs.IsSupported(MODS_PMGR_MUTEX_REG, m_PhysMutexId) ||
        !regs.IsSupported(MODS_PMGR_MUTEX_REG_VALUE))
    {
        Printf(Tee::PriError, "Required PMGR registers are not present\n");
        m_ErrorRc = RC::UNSUPPORTED_HARDWARE_FEATURE;
        return;
    }

    // Reading this register has a side effect of reserving one of 254 possible valid pmgr
    // mutex IDs for use.  This should always return a valid owner ID unless someone is
    // leaking IDs
    m_OwnerId = regs.Read32(MODS_PMGR_MUTEX_ID_ACQUIRE_VALUE);

    if (regs.TestField(m_OwnerId, MODS_PMGR_MUTEX_ID_ACQUIRE_VALUE_INIT) ||
        regs.TestField(m_OwnerId, MODS_PMGR_MUTEX_ID_ACQUIRE_VALUE_NOT_AVAIL))

    {
        Printf(Tee::PriError, "Failed to generate a PMGR mutex owner ID\n");
        m_OwnerId = ~0U;
        m_ErrorRc = RC::HW_ERROR;
        return;
    }
}

//------------------------------------------------------------------------------
PmgrMutexHolder::~PmgrMutexHolder()
{
    Release();
    if (m_OwnerId != ~0U)
        m_pSubdev->Regs().Write32(MODS_PMGR_MUTEX_ID_RELEASE_VALUE, m_OwnerId);
}

//------------------------------------------------------------------------------
RC PmgrMutexHolder::Acquire(FLOAT64 timeoutMs)
{
    RC rc;
    CHECK_RC(m_ErrorRc);
    CHECK_RC(Tasker::PollHw(timeoutMs, [&]() -> bool
    {
        m_pSubdev->Regs().Write32(MODS_PMGR_MUTEX_REG_VALUE, m_PhysMutexId, m_OwnerId);
        UINT32 ownerId = m_pSubdev->Regs().Read32(MODS_PMGR_MUTEX_REG_VALUE, m_PhysMutexId);
        if (ownerId == m_OwnerId)
        {
            Printf(Tee::PriDebug, "PMGR Mutex acquired, ID: %u\n", m_PhysMutexId);
            return true;
        }
        return false;
    }, __FUNCTION__));
    m_bAcquired = true;
    return rc;
}

//------------------------------------------------------------------------------
void PmgrMutexHolder::Release()
{
    if (m_bAcquired)
    {
        m_pSubdev->Regs().Write32(MODS_PMGR_MUTEX_REG_VALUE_INITIAL_LOCK, m_PhysMutexId);
        Printf(Tee::PriDebug, "PMGR Mutex released, ID: %u\n", m_PhysMutexId);
    }
    m_bAcquired = false;
}

