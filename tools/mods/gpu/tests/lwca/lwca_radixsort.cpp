/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2014-2015,2017,2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

/*
* Brief Description:
*
* Lwca Radix Sort test is written to stress the GPU using a radix sort algorithm
* as implemented by Duane Merrill as a part of Back40Computing. This
* implementation was originally extracted from the Bonsai lwca application.
*/

#include "gpu/tests/lwdastst.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwca.h"
#include "core/include/utility.h"
#include "core/include/log.h"
#include "core/include/errormap.h"
#include "core/include/jscript.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/lwca/radixsort/lwda_types/vector_types.h"

#include <b40c/util/multi_buffer.lwh>
#include <b40c/radix_sort/enactor.lwh>

class LwdaRadixSortTest : public LwdaStreamTest
{
public:
    LwdaRadixSortTest();
    virtual ~LwdaRadixSortTest(){}
    virtual bool IsSupported() override;
    virtual RC InitFromJs() override;
    virtual void PrintJsProperties(Tee::Priority pri) override;
    virtual RC Setup() override;
    virtual RC Run() override;
    virtual RC Cleanup() override;
    bool IsSupportedVf() override { return true; }

    SETGET_PROP(Size, UINT64);
    SETGET_PROP(PercentOclwpancy, FLOAT32);
    SETGET_PROP(DirtyExitOnFail, bool);

private:
    UINT64 m_Size;
    FLOAT32 m_PercentOclwpancy;
    bool m_DirtyExitOnFail;

    vector<UINT32> m_HostKeys;
    vector<UINT32> m_HostValues;
    Lwca::DeviceMemory m_Keys;
    Lwca::DeviceMemory m_Values;
    Lwca::DeviceMemory m_AltKeys;
    Lwca::DeviceMemory m_AltValues;

    b40c::radix_sort::Kernels<UINT32, UINT32> m_Kernels;
    b40c::util::DoubleBuffer<UINT32, UINT32> m_DoubleBuffer;
    unique_ptr<b40c::radix_sort::Enactor> m_SortEnactor;

    RC SetupData(Lwca::DeviceMemory* device, vector<UINT32>* host, UINT32 chunkSize);
};

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
LwdaRadixSortTest::LwdaRadixSortTest()
: m_Size(0) //Will by default scale the size to PercentOclwpancy
, m_PercentOclwpancy(0.9f)
, m_DirtyExitOnFail(false)
{
    SetName("LwdaRadixSortTest");
}

//-----------------------------------------------------------------------------
//! \brief Check whether LwdaRadixSortTest is supported
//!
bool LwdaRadixSortTest::IsSupported()
{
    if (!LwdaStreamTest::IsSupported())
        return false;

    if (GetBoundGpuSubdevice()->IsSOC())
        return false;

    return true;
}

//-----------------------------------------------------------------------------
//! \brief Initialize JS related variables
//!
RC LwdaRadixSortTest::InitFromJs()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::InitFromJs());

    MASSERT(m_PercentOclwpancy > 0.0f && m_PercentOclwpancy < 1.0f);

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Print Properties
//!
void LwdaRadixSortTest::PrintJsProperties(Tee::Priority pri)
{
    LwdaStreamTest::PrintJsProperties(pri);

    Printf(pri, "\tSize: %llu\n", m_Size);
    Printf(pri, "\tPercentOclwpancy: %f\n", m_PercentOclwpancy);
    Printf(pri, "\tDirtyExitOnFail: %s\n", (m_DirtyExitOnFail)?"true":"false");
}

//-----------------------------------------------------------------------------
//! \brief SetupData
//!
RC LwdaRadixSortTest::SetupData(Lwca::DeviceMemory* device, vector<UINT32>* host,
                                UINT32 chunkSize)
{
    RC rc;
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(m_Size*sizeof(UINT32), device));
    host->resize(chunkSize);
    for (UINT64 offset = 0; offset < m_Size; offset += chunkSize)
    {
        UINT64 actualSize = ((offset + chunkSize) > m_Size) ? m_Size % chunkSize
                                                            : chunkSize;
        for (UINT64 i = 0; i < actualSize; i++)
        {
            host->at(i) = rand();
        }
        CHECK_RC(device->Set(&host->at(0),
                             sizeof(UINT32)*offset,
                             sizeof(UINT32)*actualSize));
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Setup
//!
RC LwdaRadixSortTest::Setup()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::Setup());

    m_SortEnactor = unique_ptr<b40c::radix_sort::Enactor>(
                        new b40c::radix_sort::Enactor(&GetLwdaInstance()));

    CHECK_RC(m_Kernels.Setup(GetLwdaInstance()));

    if (m_Size == 0)
    {
        //Scale the size to fit m_PercentOclwpancy
        size_t freeMem, totalMem;
        CHECK_RC(GetLwdaInstance().GetMemInfo(GetBoundLwdaDevice(), &freeMem, &totalMem));
        UINT32 numArrays = 5; //m_Keys, m_AltKeys, m_Values, m_AltValues, b40c::util::spine
        m_Size = static_cast<UINT64>(((freeMem * m_PercentOclwpancy)/numArrays)/sizeof(UINT32));
        if (m_Size > INT_MAX)
        {
            m_Size = INT_MAX;
            const FLOAT64 actualOclwpancy =
                static_cast<FLOAT64>((m_Size * sizeof(UINT32) * numArrays)) / freeMem;
            Printf(Tee::PriWarn,
                   "%s : Capping percent oclwpancy at %0.3f to prevent size overflow\n",
                   GetName().c_str(), actualOclwpancy);
        }
    }

    UINT32 chunkSize = 32000000 / sizeof(UINT32); //32MB

    CHECK_RC(SetupData(&m_Keys, &m_HostKeys, chunkSize));

    CHECK_RC(SetupData(&m_Values, &m_HostValues, chunkSize));

    CHECK_RC(GetLwdaInstance().AllocDeviceMem(m_Size*sizeof(UINT32), &m_AltKeys));
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(m_Size*sizeof(UINT32), &m_AltValues));

    m_DoubleBuffer.d_keys[0] = (UINT32*)m_Keys.GetDevicePtr();
    m_DoubleBuffer.d_keys[1] = (UINT32*)m_AltKeys.GetDevicePtr();
    m_DoubleBuffer.d_values[0] = (UINT32*)m_Values.GetDevicePtr();
    m_DoubleBuffer.d_values[1] = (UINT32*)m_AltValues.GetDevicePtr();

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Ilwoke Radix Sort
//!
RC LwdaRadixSortTest::Run()
{
    RC rc;

    if (m_Size > INT_MAX)
    {
        Printf(Tee::PriError,
            "LwdaRadixSortTest doesn't support Size %llu which is above %d.\n",
            m_Size, INT_MAX);
        return RC::SOFTWARE_ERROR;
    }

    int sortSize = static_cast<int>(m_Size);

    if (m_SortEnactor->Sort<30,0>(m_DoubleBuffer, sortSize, m_Kernels) != lwdaSuccess)
        return RC::LWDA_ERROR;

    UINT32 timeoutMs = UNSIGNED_CAST(UINT32,
        static_cast<UINT64>(GetTestConfiguration()->TimeoutMs()));
    if (GetLwdaInstance().SynchronizePoll(timeoutMs) != OK)
    {
        Printf(Tee::PriError, "The context failed to synchronize in time. "
                              "Continuing could result in a hang.\n");
        GpuHung(m_DirtyExitOnFail);
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Perform Cleanup
//!
RC LwdaRadixSortTest::Cleanup()
{
    m_Keys.Free();
    m_Values.Free();
    m_AltKeys.Free();
    m_AltValues.Free();

    m_SortEnactor.reset();

    m_Kernels.Free();

    return LwdaStreamTest::Cleanup();
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ LwdaRadixSortTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(LwdaRadixSortTest, LwdaStreamTest,
                 "Run the Lwca Radix Sort stress test");
CLASS_PROP_READWRITE(LwdaRadixSortTest, Size, UINT64,
                     "Number of random keys to sort. Overrides PercentOclwpancy.");
CLASS_PROP_READWRITE(LwdaRadixSortTest, PercentOclwpancy, FLOAT32,
                     "Target percentage of device memory to fill with sort data.");
CLASS_PROP_READWRITE(LwdaRadixSortTest, DirtyExitOnFail, bool,
                     "ExitQuickAndDirty from the test if it times out instead of "
                     "risking it hanging.");
