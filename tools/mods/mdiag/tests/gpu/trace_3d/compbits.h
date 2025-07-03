/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _COMPBITS_H_
#define _COMPBITS_H_

#include "compbits_intf.h"
#include "compbits_lib.h"

class CompBitsTest
    : public CompBitsTestIF
{
public:
    // Typical testing flows:
    //   Setup test parameters;
    //   Setup test(s);
    //   Start test(s);
    //   Run DoPostRender();
    //   Cleanup test(s).
    // Making them virtual to allow derived tests to lwstomize their own steps.
    virtual RC SetupParam();
    virtual RC SetupTest();
    virtual RC StartTest();
    virtual RC DoPostRender();
    virtual void CleanupTest();

    void SetCBCLib(CompBits::CBCLibWrapper* pLibWrapper) { m_pCBCLib = pLibWrapper; }
    void SetTestID(const CompBits::TestID id) { m_ID = id; }

    MdiagSurf* GetSurfCA() { return m_pSurfCA; }
    // Save decompressed data
    RC SaveSurfaceDecompData(MdiagSurf *pSurf, vector<UINT08> *data) { return DoSaveSurfaceData(pSurf, data); }

    RC InitBackingStore();

protected: // Used by derived tests.
    CompBitsTest(TraceSubChannel* pSubChan);
    virtual ~CompBitsTest();

    virtual RC SetArgs(MdiagSurf* pSurfCA, GpuDevice *pGpuDev, TraceChannel *pTraceChan,
        const ArgReader *pParam, FLOAT64 timeoutMs);

    virtual const char* TestName() const = 0;

public: // Visible to CompBits SurfCompOperation, TestSurf, etc.
    CompBits::CBCLibWrapper* GetCBCLib() { return m_pCBCLib; }
    CompBits::Settings* GetSettings() { return &m_Settings; }

    // Backing store
    void* GetBackingStoreBar1Addr() { return m_BackingStoreCpuAddress; }

    // Utilities

    const ArgReader* GetArgReader() { return m_Params; }
    GpuDevice* GetGpuDev() { return m_BoundGpuDevice; }
    TraceChannel* GetTraceChan() { return m_Channel; }
    TraceSubChannel *GetTraceSubChan() { return m_pTraceSubch; }
    RC FlushCompTags();

    // Compr surfaces

    RC ReplicateSurface(MdiagSurf* pSrcSurf, MdiagSurf* pDstSurf,
        const char* pDstSurfName) const;

    // Have to specify which surface (source or dest) is the orignal dest
    // surface in a case we may exchange source and dest surface during a testing,
    // because pLwRm->VidHeapReleaseCompression()/VidHeapReacquireCompression()
    // may fail if re-ordered.
    // See also Pascal SurfCopyTest.
    RC CopySurfaceData(MdiagSurf *pSrcSurf,
        MdiagSurf *pDstSurf,
        MdiagSurf *pOrigDstSurf);

protected:
    void CleanUp();
    RC OnCheckpoint(size_t num, const char* pStr);

    /*
    Backing store

      Bar1Addr
       |
      AllocPA       StartPA = StoreBase
       |               |
       V---------------V-----------------------------------+
       |  Overhead     |                                   |
       +---------------+-----------------------------------+

    */

    RC DumpBackingStore(string dumpName);
    void CheckpointBackingStore(string name,
        const vector<size_t>* pLastChanges = 0);
    void ReadBackingStoreData(size_t offset, size_t size, vector<UINT08>* pData);
    void ReadBackingStoreData(vector<UINT08>* pData);
    RC WriteBackingStoreData(size_t offset, const vector<UINT08>& newData);
    RC WriteBackingStoreData(const vector<UINT08>& newData);
    RC InitInputSurfaces();
    RC CompareInitialBackingStore();
    void SetInitialBackingStore(const vector<UINT08>* pData = 0)
    {
        if (pData != 0)
        {
            m_InitialBackingStoreData = *pData;
        }
        else
        {
            m_InitialBackingStoreData = m_LwrrentBackingStoreData;
        }
    }

    // Utilities

    RC IlwalidateCompTags();
    RC WaitForIdle();
    RC GetComptagLines(MdiagSurf *surface, UINT32 *minLine, UINT32 *maxLine);

    // Compr surfaces

    RC SaveSurfaceData(MdiagSurf *surface, vector<UINT08> *data);
    RC RestoreSurfaceData(MdiagSurf *surface, vector<UINT08> *data);
    void CopySurfAttr(MdiagSurf* pSrcSurf, MdiagSurf* pDstSurf) const;
    RC RebuildSurface(MdiagSurf* pSurf);

    // Check Configurations
    int CheckCBCAddr() const;
    int CheckRegValsAndRMInfo() const;
    int CheckRegValsAndAmapInfo(const CompBits::Settings::AmapInfo& info) const;
    int CheckRMInfoAndAmapInfo(const CompBits::Settings::AmapInfo& info) const;

private:
    // Backing store

    size_t GetBackingStoreAddr(size_t offset) const;

    // Compr surfaces

    RC DoCopySurfaceData(MdiagSurf *sourceSurface, MdiagSurf *destSurface);
    RC DoSaveSurfaceData(MdiagSurf *pSurf, vector<UINT08> *data);

    // Private non-existing copy ctor to prevent from being copied.
    CompBitsTest(const CompBitsTest& inst);

protected:
    vector<size_t> m_MismatchOffsets;
    CompBits::Settings m_Settings;
    CompBits::CBCLibWrapper* m_pCBCLib;
    bool m_Initialized;

    // Utilities

    GpuDevice *m_BoundGpuDevice;
    TraceChannel *m_Channel;
    TraceSubChannel *m_pTraceSubch;
    const ArgReader *m_Params;
    FLOAT64 m_TimeoutMs;

    // Backing store

    unique_ptr<MdiagSurf> m_BackingStore;
    void* m_BackingStoreCpuAddress;
    vector<UINT08> m_InitialBackingStoreData;
    vector<UINT08> m_LwrrentBackingStoreData;

    // Compr surfaces

    // Each test has a default TestSurf.
    CompBits::TestSurf m_TestSurf;
    // Color surfaces.
    MdiagSurf* m_pSurfCA;

    CompBits::TestID m_ID;
};

////////////////////////////////////////////

namespace CompBits
{

class SingleTileTest1 : public CompBitsTest
{
public:
    SingleTileTest1(TraceSubChannel* pSubChan)
        : CompBitsTest(pSubChan)
    {
    }

private:
    virtual ~SingleTileTest1() {};

    virtual RC Init();
    virtual RC DoPostRender();
    virtual const char* TestName() const { return "SingleTileTest1"; }
};

class SingleTileTest2 : public CompBitsTest
{
public:
    SingleTileTest2(TraceSubChannel* pSubChan)
        : CompBitsTest(pSubChan)
    {
    }

private:
    virtual ~SingleTileTest2() {};

    virtual RC SetupParam();
    virtual RC DoPostRender();
    virtual const char* TestName() const { return "SingleTileTest2"; }

    vector<UINT08> m_Data;
};

// Surface Move Test, originally "SurfaceCopyTest" when for Maxwell.
class SurfMoveTest : public CompBitsTest
{
public:
    SurfMoveTest(TraceSubChannel* pSubChan)
        : CompBitsTest(pSubChan)
    {
    }

private:
    virtual ~SurfMoveTest() {};

    virtual RC DoPostRender();
    virtual const char* TestName() const { return "SurfaceMoveTest"; }
};

// Extending the SingleTileTest2 to check offsets.
class CheckOffsetTest: public CompBitsTest
{
public:
    CheckOffsetTest(TraceSubChannel* pSubChan)
        : CompBitsTest(pSubChan),
        m_Loops(0)
    {
        m_Data.resize(CompBits::Settings::SectorSize);
    }

private:
    // Entry =
    //   Surf Offset
    //     + BackingStore Offset
    //     + AMAPLib addressed PAOffset
    struct Entry
    {
        size_t m_SurfOff;
        size_t m_BsOff;
        size_t m_LibOff;

        bool Match() const;
    };

    virtual ~CheckOffsetTest() {};

    virtual RC SetupParam();
    virtual RC DoPostRender();
    virtual const char* TestName() const { return "CheckOffsetTest"; }

    RC Run();

    typedef list<Entry>   ListType;
    ListType m_Entries;
    vector<size_t> m_OffsetChanges;
    vector<UINT08> m_Data;
    size_t m_Loops;

};

class UnsupportedTest : public CompBitsTest
{
public:
    UnsupportedTest(TraceSubChannel* pSubChan)
        : CompBitsTest(pSubChan)
    {
    }

private:
    virtual ~UnsupportedTest() {};

    virtual RC SetArgs(MdiagSurf* pSurfCA, GpuDevice *pGpuDev, TraceChannel *pTraceChan,
        const ArgReader *pParam, FLOAT64 timeoutMs)
    {
        Alert();
        return RC::ILWALID_ARGUMENT;
    }
    virtual RC Init()
    {
        Alert();
        return RC::ILWALID_ARGUMENT;
    }
    virtual RC PostRender()
    {
        Alert();
        return RC::ILWALID_ARGUMENT;
    }
    virtual const char* TestName() const { return "UnsupportedTest"; }

    void Alert()
    {
        ErrPrintf("CBC: Unsupported TRC test type %u.\n", m_ID);
    }
};

//////////////////Test Creator///////////////////

//
// Test creating flow:
// 1. CreateInstance dispatchs to chip family specific creator such as
//   CreateMaxwellTest or CreatePascalTest, etc.
// 2. In each chip family specific creator, if no specific test found,
//   call CreateCommonTest.
// 3. The GetCommonTest or GetCommonUnitTest.
//
struct TestCreator
{
    // Step 1. the dispatcher.
    static CompBitsTest* Create(TraceSubChannel* pSubChan, const TestID id);

private:
    static const char* TestVersion() { return "2.34"; }

    // Step 2. chip family specific.
    static CompBitsTest* CreateMaxwellTest(TraceSubChannel* pSubChan, const TestID id);
    static CompBitsTest* CreatePascalTest(TraceSubChannel* pSubChan, const TestID id);

    // Step 3. Create a common test.
    static CompBitsTest* CreateCommonTest(TraceSubChannel* pSubChan, const TestID id);
    static CompBitsTest* GetCommonTest(TraceSubChannel* pSubChan, const TestID id);
    static CompBitsTest* GetCommonUnitTest(TraceSubChannel* pSubChan);

};

} //namespace CompBits

#endif

