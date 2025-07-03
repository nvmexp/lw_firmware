/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010,2013-2017,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file lwdamemtest.h
 * @brief Base class for all memory tests based on LWCA.
 */

#ifndef INCLUDED_LWDAMEMTEST_H
#define INCLUDED_LWDAMEMTEST_H

#ifndef INCLUDED_GPUMTEST_H
#include "gpumtest.h"
#endif

#include <memory>

#include "gpu/tests/lwca/lwdawrap.h"

extern SObject LwdaMemTest_Object;

class TestDevice;
typedef shared_ptr<TestDevice> TestDevicePtr;

//! Base class for memory tests using LWCA.
class LwdaMemTest: public GpuMemTest
{
public:
    LwdaMemTest();
    bool IsSupported() override;
    bool IsSupportedVf() override { return true; }
    RC BindTestDevice(TestDevicePtr pTestDevice, JSContext *cx, JSObject *obj) override;
    RC Setup() override;
    RC Cleanup() override;
    bool IsTestType(TestType tt) override;

    //! Reports any errors logged during the test.
    virtual RC ReportErrors();
    //! Returns number of reported errors.
    UINT32 GetNumReportedErrors() const { return (UINT32)m_ReportedErrors.size(); }

    // JS properties
    SETGET_JSARRAY_PROP_LWSTOM(AllocatedBlocks);
    SETGET_PROP(UseBlockLinear, bool);

protected:
    //! Allocates all FB memory and maps it into LWCA context.
    RC AllocateEntireFramebuffer();
    //! Frees all FB memory and unmaps it from LWCA context.
    RC FreeEntireFramebuffer();
    //! Displays any chunk on the screen.
    RC DisplayAnyChunk() { return DisplayChunk(nullptr, 0); }
    //! Displays a chunk on the screen, preferably the one provided
    RC DisplayChunk(GpuUtility::MemoryChunkDesc* pPreferredChunk,
                    UINT64 preferredOffset);
    //! Returns LWCA instance (const).
    const Lwca::Instance& GetLwdaInstance() const { return m_Lwda; }
    //! Returns LWCA instance.
    Lwca::Instance& GetLwdaInstance() { return m_Lwda; }
    const Lwca::Instance* GetLwdaInstancePtr() const { return &m_Lwda; }
    //! Returns LWCA device.
    Lwca::Device GetBoundLwdaDevice() const;
    //! Returns the number of allocated memory chunks.
    UINT32 NumChunks() const { return (UINT32)GetChunks()->size(); }
    //! Returns description of a particular chunk.
    const GpuUtility::MemoryChunkDesc& GetChunkDesc(UINT32 i) const;
    GpuUtility::MemoryChunkDesc& GetChunkDesc(UINT32 i);
    //! Returns LWCA object of a particular chunk (const).
    const Lwca::ClientMemory& GetLwdaChunk(UINT32 i) const { return m_Chunks[i]; }
    //! Returns LWCA object of a particular chunk.
    Lwca::ClientMemory& GetLwdaChunk(UINT32 i) { return m_Chunks[i]; }
    //! Finds a memory chunk by virtual address (LWCA device pointer).
    UINT32 FindChunkByVirtualAddress(UINT64 virt) const;
    //! Vaddr to paddr translation that bypasses RM call
    UINT64 ContiguousVirtualToPhysical(UINT64 virt) const;
    //! Colwerts virtual FB address (device pointer) to a physical FB address.
    UINT64 VirtualToPhysical(UINT64 virt) const;
    //! Colwerts physical FB address to a virtual FB address (device pointer).
    UINT64 PhysicalToVirtual(UINT64 virt) const;
    //! Colwerts two physical FB addresses to virtual (device pointers) within the same chunk.
    RC PhysicalToVirtual(UINT64* begin, UINT64* end) const;
    //! Logs a memory error.
    virtual void LogError(UINT64 fbOffset, UINT32 actual, UINT32 expected,
            UINT32 reread1, UINT32 reread2, UINT64 iteration,
            const GpuUtility::MemoryChunkDesc& chunk);

    class ReportedError
    {
    public:
        virtual ~ReportedError() = default;
        UINT64 Address;
        UINT32 Actual;
        UINT32 Expected;
        UINT32 Reread1;
        UINT32 Reread2;
        UINT32 PteKind;
        UINT32 PageSizeKB;

        virtual bool operator<(const ReportedError& e) const
        {
            if (Address != e.Address) return Address < e.Address;
            if (Actual != e.Actual) return Actual < e.Actual;
            if (Expected != e.Expected) return Expected < e.Expected;
            if (Reread1 != e.Reread1) return Reread1 < e.Reread1;
            if (Reread2 != e.Reread2) return Reread2 < e.Reread2;
            if (PteKind != e.PteKind) return PteKind < e.PteKind;
            return PageSizeKB < e.PageSizeKB;
        }
    };

    typedef multiset<UINT64> TimeStamps;

private:
    typedef map<ReportedError,TimeStamps> ReportedErrors;
    ReportedErrors m_ReportedErrors;

    Lwca::Instance m_Lwda;
    Lwca::Device m_LwdaDevice;
    vector<Lwca::ClientMemory> m_Chunks;
    JsArray m_AllocatedBlocks;

    bool m_UseBlockLinear = false;
};

#endif // INCLUDED_LWDAMEMTEST_H
