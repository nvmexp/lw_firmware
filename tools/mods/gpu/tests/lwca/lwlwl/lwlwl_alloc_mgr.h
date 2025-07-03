/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017,2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_LWLWL_ALLOC_MGR_H
#define INCLUDED_LWLWL_ALLOC_MGR_H

#include "core/include/memtypes.h"
#include "core/include/rc.h"
#include "core/include/tee.h"
#include "core/include/types.h"
#include "gpu/tests/lwlink/lwldatatest/lwldt_alloc_mgr.h"
#include "gpu/tests/lwlink/lwldatatest/lwldt_priv.h"
#include "gpu/tests/lwca/lwdawrap.h"

#include <vector>
#include <map>
#include <memory>

class GpuTestConfiguration;

namespace LwLinkDataTestHelper
{
    //--------------------------------------------------------------------------
    //! \brief Class which manages allocations for the Lwca LwLink
    //! test.  This is useful to avoid constant re-allocation of resources when
    //! test modes are changed since many resources can be reused in between
    //! test modes.  This manager takes a "lazy allocation" approach for
    //! such that when a resource is "acquired" if one is not available it
    //! creates one and then returns that resource.
    class AllocMgrLwda : public AllocMgr
    {
        public:
            struct LwdaFunction
            {
                Lwca::Stream   stream;
                Lwca::Function func;
                Lwca::Device   device;
            };

            class LwdaMemory
            {
                public:
                    LwdaMemory(Lwca::Device dev) : m_Device(dev), m_bLoopback(false) { }
                    ~LwdaMemory() { }

                    //! Similar to the Lwca::Global::Get function.  If the wrapped memory is FB
                    //! memory this will copy the memory into the host memory pointed to by the
                    //! provided parameter
                    RC     Get(LwdaMemory *pHost) const;
                    Lwca::Device GetDevice() const { return m_Device; }
                    UINT64 GetDevicePtr();
                    Memory::Location GetLocation() const;
                    void * GetPtr();
                    UINT64 GetSize() const;
                    RC     MapLoopback();
                    RC     UnmapLoopback();
                protected:
                    Lwca::Device       m_Device;
                    bool               m_bLoopback;
                    Lwca::HostMemory   m_HostMem;
                    Lwca::DeviceMemory m_DeviceMem;
            };

            AllocMgrLwda
            (
                const Lwca::Instance *pLwda,
                GpuTestConfiguration * pTestConfig,
                Tee::Priority pri,
                bool bFailRelease
            );
            virtual ~AllocMgrLwda() {}

            RC AcquireFunction
            (
                Lwca::Device dev
               ,const string &functionName
               ,LwdaFunction **ppFunction
            );
            RC AcquireMemory
            (
                Lwca::Device dev
               ,UINT64 size
               ,Memory::Location loc
               ,LwdaMemory **pMem
            );
            RC ReleaseFunction(const LwdaFunction * pLwdaFunction);
            RC ReleaseMemory(const LwdaMemory * pMem);
            virtual RC Shutdown();
        private:
            //! Derive a private class for Lwca memory that has the allocation and free routines
            //! so that pointers handed out to clients cannot allocate or free the underlying
            //! memory
            class LwdaMemoryPriv : public LwdaMemory
            {
                public:
                    LwdaMemoryPriv(Lwca::Device dev) : LwdaMemory(dev) { }
                    ~LwdaMemoryPriv();

                    RC Alloc(const Lwca::Instance *pLwda, Memory::Location loc, UINT64 size);
                    void Free();
            };

            //! Structure describing a channel allocation
            struct FunctionAlloc
            {
                bool                      bAcquired;      //!< true if allocation is used
                string                    functionName;   //!< name of the acquire function
                unique_ptr<LwdaFunction>  pFuncData;      //!< Pointer to the function
                FunctionAlloc(FunctionAlloc &&m) = default;
                FunctionAlloc & operator=(FunctionAlloc &&m) noexcept = default;
                FunctionAlloc(bool a, const string & f, unique_ptr<LwdaFunction> &&pF)
                    : bAcquired(a), functionName(f), pFuncData(move(pF))  {}
            };

            //! Structure describing a semaphore allocation
            struct LwdaMemoryAlloc
            {
                bool                       bAcquired;     //!< true if allocation is used
                unique_ptr<LwdaMemoryPriv> pLwdaMemory;
                LwdaMemoryAlloc(LwdaMemoryAlloc &&m) = default;
                LwdaMemoryAlloc & operator=(LwdaMemoryAlloc &&m) noexcept = default;
                LwdaMemoryAlloc(bool a, unique_ptr<LwdaMemoryPriv> &&pM)
                    : bAcquired(a), pLwdaMemory(move(pM)) {}
            };

            const Lwca::Instance *m_pLwda;
            map<Lwca::Device, unique_ptr<Lwca::Module>> m_Modules;
            map<Lwca::Device, vector<LwdaMemoryAlloc>> m_Memory;
            map<Lwca::Device, vector<FunctionAlloc>> m_Functions;
    };
};

#endif // INCLUDED_LWLWL_ALLOC_MGR_H
