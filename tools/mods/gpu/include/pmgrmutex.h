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
#pragma once

#ifdef MATS_STANDALONE
#include "fakemods.h"
#else
#include "core/include/rc.h"
#endif

class GpuSubdevice;

// Mutex holder for PMGR HW mutexes.  Implementation was taken from corresponding RM
// implementation in resman/kernel/pmgr/pascal/pmgrgp100.c
class PmgrMutexHolder
{
    public:
        enum MutexId
        {
            MI_HBM_IEEE1500
           ,MI_THERM_TSENSE_SNAPSHOT
           ,MI_ILWALID
        };

        PmgrMutexHolder() = delete;
        PmgrMutexHolder(const PmgrMutexHolder&) = delete;
        PmgrMutexHolder& operator=(const PmgrMutexHolder&) = delete;

        PmgrMutexHolder(GpuSubdevice *pSubdev, MutexId mid);
        ~PmgrMutexHolder();
        RC Acquire(FLOAT64 timeoutMs);
        void Release();
    private:
        RC            m_ErrorRc      = OK;
        bool          m_bAcquired    = false;
        GpuSubdevice *m_pSubdev      = nullptr;
        UINT32        m_PhysMutexId  = ~0U;
        UINT32        m_OwnerId      = ~0U;
};

namespace PmgrMutexTableInfo
{
    struct Table
    {
        const UINT08* pElements;
        size_t        size;
    };
}

// Used by genreghalimpl.py to create the compile-time PMGR mutex tables
#if defined(LW_PMGR_MUTEX_DEFINES)
    #define DECLARE_PMGR_MUTEX_IDS(GPU)                                  \
        static const UINT08 s_##GPU##Mutexes[] =                         \
        {                                                                \
            LW_PMGR_MUTEX_DEFINES                                        \
        };                                                               \
        namespace PmgrMutexTableInfo                                     \
        {                                                                \
            extern const Table g_##GPU##PmgrMutexIds;                    \
            const Table g_##GPU##PmgrMutexIds =                          \
            {                                                            \
                &s_##GPU##Mutexes[0],                                    \
                sizeof(s_##GPU##Mutexes)                                 \
            };                                                           \
        }
#else
    #define DECLARE_PMGR_MUTEX_IDS(GPU)                                  \
        namespace PmgrMutexTableInfo                                     \
        {                                                                \
            extern const Table g_##GPU##PmgrMutexIds;                    \
            const Table g_##GPU##PmgrMutexIds = { nullptr, 0 };          \
        }
#endif
