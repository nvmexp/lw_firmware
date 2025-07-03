/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   gralloc.cpp
 *
 * @brief  Implementation of the allocators for all graphics classes
 *
 */

#include "gpu/include/gralloc.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include <algorithm>

#include "class/cl502d.h" // LW50_TWOD
#include "class/cl5097.h" // LW50_TESLA
#include "class/cl50c0.h" // LW50_COMPUTE
#include "class/cl824d.h" // G82_EXTERNAL_VIDEO_DECODER
#include "class/cl8297.h" // G82_TESLA
#include "class/cl8397.h" // GT200_TESLA
#include "class/cl8597.h" // GT214_TESLA
#include "class/cl85b5sw.h" // LW85B5_ALLOCATION_PARAMETERS;
#include "class/cl8697.h" // GT21A_TESLA
#include "class/cl86b6.h" // LW86B6_VIDEO_COMPOSITOR
#include "class/clb0b6.h" // LWB0B6_VIDEO_COMPOSITOR
#include "class/clb1b6.h" // LWB1B6_VIDEO_COMPOSITOR
#include "class/clc5b6.h" // LWC5B6_VIDEO_COMPOSITOR
#include "class/cl902d.h" // FERMI_TWOD_A
#include "class/cl9097.h" // FERMI_A
#include "class/cl90b3.h" // GF100_PPP
#include "class/cl90b5.h" // GF100_DMA_COPY
#include "class/cl90b7.h" // LW90B7_VIDEO_ENCODER
#include "class/cla0b7.h" // LWA0B7_VIDEO_ENCODER
#include "class/cl90b8.h" // GF106_DMA_DECOMPRESS
#include "class/cl90c0.h" // FERMI_COMPUTE_A
#include "class/cl9171.h" // LW9171_DISP_SF_USER
#include "class/cl9197.h" // FERMI_B
#include "class/cl91c0.h" // FERMI_COMPUTE_B
#include "class/cl9271.h" // LW9271_DISP_SF_USER,
#include "class/cl9297.h" // FERMI_C
#include "class/cl9471.h" // LW9471_DISP_SF_USER,
#include "class/cl95a1.h" // LW95A1_TSEC
#include "class/cl95b1.h" // LW95B1_VIDEO_MSVLD
#include "class/cl95b2.h" // LW95B2_VIDEO_MSPDEC
#include "class/cla097.h" // KEPLER_A
#include "class/cla0b5.h" // KEPLER_DMA_COPY_A
#include "class/cla0b5sw.h" // LWA0B5_ALLOCATION_PARAMETERS_VERSION_1
#include "class/clb0b5.h" // MAXWELL_DMA_COPY_A
#include "class/clc0b5.h" // PASCAL_DMA_COPY_A
#include "class/clc1b5.h" // PASCAL_DMA_COPY_B
#include "class/clc3b5.h" // VOLTA_DMA_COPY_A
#include "class/clc5b5.h" // TURING_DMA_COPY_A
#include "class/clc6b5.h" // AMPERE_DMA_COPY_A
#include "class/clc7b5.h" // AMPERE_DMA_COPY_B
#include "class/clc8b5.h" // HOPPER_DMA_COPY_A
#include "class/clc9b5.h" // BLACKWELL_DMA_COPY_A
#include "class/cla0b0.h" // LWA0B0_VIDEO_DECODER
#include "class/clb0b0.h" // LWB0B0_VIDEO_DECODER
#include "class/clb6b0.h" // LWB6B0_VIDEO_DECODER
#include "class/clc0b0.h" // LWC0B0_VIDEO_DECODER
#include "class/clc1b0.h" // LWC1B0_VIDEO_DECODER
#include "class/clc2b0.h" // LWC2B0_VIDEO_DECODER
#include "class/clc3b0.h" // LWC3B0_VIDEO_DECODER
#include "class/clc4b0.h" // LWC4B0_VIDEO_DECODER
#include "class/clc6b0.h" // LWC6B0_VIDEO_DECODER
#include "class/clc7b0.h" // LWC7B0_VIDEO_DECODER
#include "class/clb8b0.h" // LWB8B0_VIDEO_DECODER
#include "class/cla0c0.h" // KEPLER_COMPUTE_A
#include "class/cla140.h" // KEPLER_INLINE_TO_MEMORY_B
#include "class/cla197.h" // KEPLER_B
#include "class/cla1c0.h" // KEPLER_COMPUTE_B
#include "class/cla297.h" // KEPLER_C
#include "class/clb097.h" // MAXWELL_A
#include "class/clb0c0.h" // MAXWELL_COMPUTE_A
#include "class/clb197.h" // MAXWELL_B
#include "class/clb1c0.h" // MAXWELL_COMPUTE_B
#include "class/clc097.h" // PASCAL_A
#include "class/clc0c0.h" // PASCAL_COMPUTE_A
#include "class/clc197.h" // PASCAL_B
#include "class/clc1c0.h" // PASCAL_COMPUTE_B
#include "class/clc397.h" // VOLTA_A
#include "class/clc3c0.h" // VOLTA_COMPUTE_A
#include "class/clc597.h" // TURING_A
#include "class/clc5c0.h" // TURING_COMPUTE_A
#include "class/clc697.h" // AMPERE_A
#include "class/clc6c0.h" // AMPERE_COMPUTE_A
#include "class/clc797.h" // AMPERE_B
#include "class/clc7c0.h" // AMPERE_COMPUTE_B
#include "class/clc997.h" // ADA_A
#include "class/clc9c0.h" // ADA_COMPUTE_A
#include "class/clcb97.h" // HOPPER_A
#include "class/clcbc0.h" // HOPPER_COMPUTE_A
#include "class/clcc97.h" // HOPPER_B
#include "class/clccc0.h" // HOPPER_COMPUTE_B
#include "class/clcd97.h" // BLACKWELL_A
#include "class/clcdc0.h" // BLACKWELL_COMPUTE_A
#include "class/clc0b7.h" // LWC0B7_VIDEO_ENCODER
#include "class/cld0b7.h" // LWD0B7_VIDEO_ENCODER
#include "class/clc1b7.h" // LWC1B7_VIDEO_ENCODER
#include "class/clc2b7.h" // LWC2B7_VIDEO_ENCODER
#include "class/clc3b7.h" // LWC3B7_VIDEO_ENCODER
#include "class/clc4b7.h" // LWC4B7_VIDEO_ENCODER
#include "class/clc5b7.h" // LWC5B7_VIDEO_ENCODER
#include "class/clb4b7.h" // LWB4B7_VIDEO_ENCODER
#include "class/clc7b7.h" // LWC7B7_VIDEO_ENCODER
#include "class/clc8b0.h" // LWC8B0_VIDEO_DECODER
#include "class/clc9b7.h" // LWC9B7_VIDEO_ENCODER
#include "class/clc9b0.h" // LWC9B0_VIDEO_DECODER
#include "class/cl83de.h" // GT200_DEBUGGER
#include "class/clc361.h" // VOLTA_USERMODE_A
#include "class/clc461.h" // TURING_USERMODE_A
#include "class/clc561.h" // AMPERE_USERMODE_A
#include "class/clc661.h" // HOPPER_USERMODE_A
#include "class/clc4d1.h" // LWC4D1_VIDEO_LWJPG
#include "class/clb8d1.h" // LWB8D1_VIDEO_LWJPG
#include "class/clc9d1.h" // LWC9D1_VIDEO_LWJPG
#include "class/clc6fa.h" // LWC6FA_VIDEO_OFA
#include "class/clc7fa.h" // LWC7FA_VIDEO_OFA
#include "class/clc8fa.h" // LWC8FA_VIDEO_OFA
#include "class/clb8fa.h" // LWB8FA_VIDEO_OFA
#include "class/clc9fa.h" // LWC9FA_VIDEO_OFA
#include "class/cle7d0.h" // LWE7D0_VIDEO_LWJPG

#include "lwos.h"

//-----------------------------------------------------------------------------
// GrClassAllocator Base Class
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/* virtual */ GrClassAllocator::~GrClassAllocator()
{
   /* Empty */
}

//-----------------------------------------------------------------------------
//! \brief Return the handle allocated by Alloc, or 0 if no object is allocated
LwRm::Handle GrClassAllocator::GetHandle() const
{
   return m_hLwObject;
}

//-----------------------------------------------------------------------------
//! \brief Return the class allocated by Alloc, or 0 if no object is allocated
UINT32 GrClassAllocator::GetClass() const
{
    return m_LwClass;
}

//------------------------------------------------------------------------------
//! \brief Override the usual allocation logic, to request one specific class.
//!
//! The requested class does not necessarily have to be in the class
//! list, but it's strongly recommended.
//!
void GrClassAllocator::SetClassOverride(UINT32 requestedClass)
{
    m_RequestedClass = requestedClass;
}

//------------------------------------------------------------------------------
//! \brief Set the oldest class in the class list that the caller supports
//!
void GrClassAllocator::SetOldestSupportedClass(UINT32 oldestSupportedClass)
{
    m_OldestSupportedClass = oldestSupportedClass;
}

//------------------------------------------------------------------------------
//! \brief Set the newest class in the class list that the caller supports
//!
void GrClassAllocator::SetNewestSupportedClass(UINT32 newestSupportedClass)
{
    m_NewestSupportedClass = newestSupportedClass;
}

//-----------------------------------------------------------------------------
//! \brief Return true if Alloc() is expected to succeed
//!
//! This method checks the class list that will be used by Alloc() to
//! see whether any of them are supported on this GpuDevice.
//!
bool GrClassAllocator::IsSupported(GpuDevice *pGpuDev) const
{
    // Find the list of classes
    //
    const UINT32 *pClassList;
    UINT32 numClasses;
    GetClassList(&pClassList, &numClasses);

    // Return true if any of them are supported on this GPU.
    //
    LwRm *pLwRm = m_pLwRm ? m_pLwRm : LwRmPtr(0).Get();
    UINT32 supportedClass;
    return (OK == pLwRm->GetFirstSupportedClass(numClasses, pClassList,
                                                &supportedClass, pGpuDev));
}

//-----------------------------------------------------------------------------
//! \brief Return the class that would be allocated by Alloc, or 0 if the
//!        allocator configuration would result in no class being supported
UINT32 GrClassAllocator::GetSupportedClass(GpuDevice *pGpuDev) const
{
    if (m_hLwObject != 0)
        return m_LwClass;

    MASSERT(pGpuDev);
    UINT32 lwClass = 0;
    FindSupportedClass(pGpuDev, &lwClass);
    return lwClass;
}

//-----------------------------------------------------------------------------
//! \brief Allocate the most recent supported version of the class
//!
//! This method searches the following classes to find a supported class.
//!  - If the caller called SetClassOverride(), then just try that
//!    one class.
//!  - Otherwise, if the caller called SetOldestSupportedClass() and/or
//!    SetNewestSupportedClass(), then try the subset of the class
//!    list that falls between those two classes, inclusive.
//!  - Otherwise, try the full class list, from newest to oldest.
//!
/* virtual */ RC GrClassAllocator::Alloc
(
    LwRm::Handle hParent,
    GpuDevice *pGpuDev,
    LwRm *pLwRm
)
{
    UINT32 LwClass;
    RC rc;

    SetRmClient(pLwRm);

    CHECK_RC(FindSupportedClass(pGpuDev, &LwClass));

    if (pGpuDev->GetSubdevice(0)->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID))
    {
        const vector<UINT32> classId = { LwClass };
        vector<UINT32> engines;
        CHECK_RC(pGpuDev->GetEnginesFilteredByClass(classId, &engines, pLwRm));

        if (engines.size() > 1)
        {
            const bool bRequireEngineId = GpuPtr()->GetRequireChannelEngineId();
            Printf(bRequireEngineId ? Tee::PriError : Tee::PriWarn,
                   "%s : Allocation without engine specification is not supported!\n",
                   __FUNCTION__);
            if (bRequireEngineId)
                return RC::UNSUPPORTED_FUNCTION;
        }
    }

    CHECK_RC(AllocProtected(hParent, pGpuDev, LwClass, NULL));
    return rc;
}

//-----------------------------------------------------------------------------
RC GrClassAllocator::AllocOnEngine
(
    LwRm::Handle  hCh,
    UINT32        Engine,
    GpuDevice   * pGpuDev,
    LwRm        * pLwRm
)
{
    // Only one engine for this object, just use the normal alloc
    return Alloc(hCh, pGpuDev, pLwRm);
}

//-----------------------------------------------------------------------------
//! \brief Allocator for classses that are parented by a subdevice
//!
RC GrClassAllocator::Alloc(GpuSubdevice *pGpuSubdevice, LwRm *pLwRm)
{
    return Alloc(pLwRm->GetSubdeviceHandle(pGpuSubdevice),
                 pGpuSubdevice->GetParentDevice(),
                 pLwRm);
}

//-----------------------------------------------------------------------------
//! \brief Check if the class supports 64Bit Semaphores
//!
/* virtual */ bool GrClassAllocator::Has64BitSemaphore(UINT32 classId)
{
    return false;
}

//-----------------------------------------------------------------------------
//! \brief Free the object allocated by Alloc()
//!
/* virtual */ void GrClassAllocator::Free()
{
    if ((m_hLwObject != 0) && (GpuPtr()->IsInitialized()))
    {
        MASSERT(m_pLwRm);
        m_pLwRm->Free(m_hLwObject);
    }
    m_hLwObject = 0;
    m_LwClass = 0;
}

//-----------------------------------------------------------------------------
//! \brief Return true if this class is in GetClassList().
//!
bool GrClassAllocator::HasClass(UINT32 classId) const
{
    const UINT32 *pClassList;
    UINT32 numClasses;
    GetClassList(&pClassList, &numClasses);

    for (UINT32 ii = 0; ii < numClasses; ++ii)
    {
        if (pClassList[ii] == classId)
            return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
/* virtual */ RC GrClassAllocator::GetDefaultEngineId
(
    GpuDevice * pGpuDev
   ,LwRm      * pLwRm
   ,UINT32    * pEngineId
) const
{
    RC rc;
    UINT32 lwClass;

    MASSERT(pEngineId);
    *pEngineId = LW2080_ENGINE_TYPE_NULL;

    CHECK_RC(FindSupportedClass(pGpuDev, &lwClass));

    const vector<UINT32> classId = { lwClass };
    vector<UINT32>       engines;
    CHECK_RC(pGpuDev->GetEnginesFilteredByClass(classId, &engines, pLwRm));

    // Non-engine class
    if (engines.empty())
        return OK;

    *pEngineId = engines[0];

    return rc;
}

//-----------------------------------------------------------------------------
RC GrClassAllocator::GetEngineId
(
    GpuDevice * pGpuDev
   ,LwRm      * pLwRm
   ,UINT32      instance
   ,UINT32    * pEngineId
) const
{
    RC rc;
    UINT32 lwClass;

    CHECK_RC(FindSupportedClass(pGpuDev, &lwClass));

    const vector<UINT32> classId = { lwClass };
    vector<UINT32>       engines;
    CHECK_RC(pGpuDev->GetEnginesFilteredByClass(classId, &engines, pLwRm));

    // Non-engine class
    if (engines.empty())
    {
        *pEngineId = LW2080_ENGINE_TYPE_NULL;
        return OK;
    }
    else if (engines.size() == 1)
    {
        const UINT32 engineInstance = EngineToInst(engines[0]);
        if (engineInstance != instance)
        {
            Printf(Tee::PriError, "%s : %s instance %u no matching engines found\n",
                   __FUNCTION__, m_Name, instance);
            return RC::DEVICE_NOT_FOUND;
        }
        *pEngineId = engines[0];
    }

    vector<UINT32> supportedEngines;
    for (auto const & lwrEng : engines)
    {
        if (EngineToInst(lwrEng) == instance)
            supportedEngines.push_back(lwrEng);
    }

    if (supportedEngines.empty())
    {
        Printf(Tee::PriError, "%s : %s instance %u no matching engines found\n",
               __FUNCTION__, m_Name, instance);
        return RC::DEVICE_NOT_FOUND;
    }
    else if (supportedEngines.size() > 1)
    {
        Printf(Tee::PriError, "%s : %s multiple matching engines found for instance %u\n",
               __FUNCTION__, m_Name, instance);
        return RC::DEVICE_NOT_FOUND;
    }

    *pEngineId = supportedEngines[0];

    return rc;
}

//-----------------------------------------------------------------------------
// Protected Methods
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
//! \param pClassList Array of alternate versions of this class
//!     from newest to oldest
//! \param numClasses The number of elements in pClassList
//! \param name The name of the class.  Used in error messages.
//!
GrClassAllocator::GrClassAllocator
(
    UINT32 * pClassList,
    size_t numClasses,
    const char *name
) :
    m_hLwObject(0),
    m_LwClass(0),
    m_RequestedClass(0),
    m_OldestSupportedClass(pClassList[numClasses - 1]),
    m_NewestSupportedClass(pClassList[0]),
    m_pClassList(pClassList),
    m_NumClasses(numClasses),
    m_Name(name),
    m_pLwRm(NULL)
{
    MASSERT(pClassList != NULL);
    MASSERT(numClasses > 0);
    MASSERT(name != NULL);
}

//-----------------------------------------------------------------------------
//! \brief Return the list of classes used by Alloc().
//!
//! The classes are listed in order from newest (i.e. most-preferred)
//! to oldest.
//!
//! \param[out] ppClassList On exit, contains the array of classes
//! \param[out] pClassList  On exit, contains the number of classes
//!
void GrClassAllocator::GetClassList
(
    const UINT32 **ppClassList,
    UINT32   *pNumClasses
) const
{
    MASSERT(ppClassList != NULL);
    MASSERT(pNumClasses != NULL);

    if (m_RequestedClass != 0)
    {
        // If the caller called SetClassOverride(), then return a
        // one-element list of the requested class.
        //
        *ppClassList = &m_RequestedClass;
        *pNumClasses = 1;
    }
    else
    {
        // Return all classes from OldestSupportedClass to
        // NewestSupportedClass
        //
        UINT32 *pClassList = m_pClassList;
        size_t  numClasses = m_NumClasses;

        while (0 < numClasses &&
               pClassList[numClasses - 1] != m_OldestSupportedClass)
        {
            --numClasses;
        }
        while (0 < numClasses &&
               pClassList[0] != m_NewestSupportedClass)
        {
            ++pClassList;
            --numClasses;
        }
        if (numClasses == 0)
        {
            // We should never get here unless the caller passed
            // illegal values to OldestSupportedClass and
            // NewestSupportedClass.
            //
            Printf(Tee::PriHigh,
                   "Error in %s: no classes between %X and %X:",
                   m_Name, m_NewestSupportedClass, m_OldestSupportedClass);
            for (size_t ii = 0; ii < m_NumClasses; ++ii)
                Printf(Tee::PriHigh, " %X\n", m_pClassList[ii]);
            Printf(Tee::PriHigh, "\n");
            MASSERT(!"Illegal class list in GrClassAllocator");
        }

        *ppClassList = pClassList;
        *pNumClasses = static_cast<UINT32>(numClasses);
    }
}

//-----------------------------------------------------------------------------
//! \brief Find the most recent supported version of the class.
//!
//! This method searches the classes found by GetClassList(), and
//! finds the most recent one supported by the GpuDevice.
//!
//! \param pGpuDev The GpuDevice on which the class will be allocated
//! \param[out] pLwClass On success, contains the supported class
//! \retval OK Successfully found a supported class
//! \retval LWRM_ILWALID_CLASS Failed to find the class specified by
//!     SetClassOverride()
//! \retval UNSUPPORTED_FUNCTION Failed to find a class from the class list.
//!
RC GrClassAllocator::FindSupportedClass
(
    GpuDevice *pGpuDev,
    UINT32 *pLwClass
) const
{
    LwRm *pLwRm = m_pLwRm ? m_pLwRm : LwRmPtr(0).Get();

    RC rc;

    // Find the class list
    //
    const UINT32 *pClassList;
    UINT32 numClasses;
    GetClassList(&pClassList, &numClasses);

    // Find the first class supported on this GPU.
    //
    rc = pLwRm->GetFirstSupportedClass(numClasses, pClassList,
                                       pLwClass, pGpuDev);

    // If the caller couldn't find a supported class, report an error
    //
    if (rc != OK)
    {
        *pLwClass = 0;

        if (m_RequestedClass != 0)
        {
            // The user specified this class explicitly, the user needs to know
            // they messed up.
            Printf(Tee::PriHigh, "Class 0x%X not supported on %s!\n",
                   m_RequestedClass,
                   Gpu::DeviceIdToString(pGpuDev->DeviceId()).c_str());
            return RC::LWRM_ILWALID_CLASS;
        }
        else
        {
            // Otherwise, this is a standard error, this GPU doesn't support this class.
            // Return a distinctive error code, so Random2d can ignore the error and
            // just not use this class.
            Printf(Tee::PriLow, "No supported %s class (", m_Name);
            for (UINT32 i = 0; i < m_NumClasses; i++)
            {
                Printf(Tee::PriLow, " %X", m_pClassList[i]);
            }
            Printf(Tee::PriLow, ") on %s.\n",
                   Gpu::DeviceIdToString(pGpuDev->DeviceId()).c_str());
            return RC::UNSUPPORTED_FUNCTION;
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Allocate the indicated class
//!
//! Called by Alloc(), and by subclass methods that replace Alloc().
//!
//! \param hParent The parent of the new object
//! \param pGpuDev The GpuDevice on which the class will be allocated
//! \param LwClass The class that was returned by FindSupportedClass()
//! \param pGrParms Parameter to pass to LwRm::Alloc()
//!
RC GrClassAllocator::AllocProtected
(
   LwRm::Handle hParent,
   GpuDevice *pGpuDev,
   UINT32 LwClass,
   void *pGrParms
)
{
   RC rc;
   LwRm::Handle h = 0;

   MASSERT(m_pLwRm);

   // Allocate the class we decided on.
   rc = m_pLwRm->Alloc(hParent, &h, LwClass, pGrParms);

   if (OK != rc)
   {
      Printf(Tee::PriHigh,
             "Allocation of %X %s object failed with error %d!\n",
             LwClass, m_Name, (INT32)rc);
      return rc;
   }
   else
   {
      Printf(Tee::PriLow, "Allocated %X %s\n", LwClass, m_Name);
   }
   m_hLwObject = h;
   m_LwClass = LwClass;

   return rc;
}

void GrClassAllocator::SetRmClient(LwRm *pLwRm)
{
    if (pLwRm)
        m_pLwRm = pLwRm;
    else
        m_pLwRm = LwRmPtr(0).Get();
}

//-----------------------------------------------------------------------------
// Derived Classes:
//-----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// DmaCopyAlloc
//------------------------------------------------------------------------------
UINT32 DmaCopyAlloc::s_ClassList[] =
{
    BLACKWELL_DMA_COPY_A,
    HOPPER_DMA_COPY_A,
    AMPERE_DMA_COPY_B,
    AMPERE_DMA_COPY_A,
    TURING_DMA_COPY_A,
    VOLTA_DMA_COPY_A,
    PASCAL_DMA_COPY_B,
    PASCAL_DMA_COPY_A,
    MAXWELL_DMA_COPY_A,
    KEPLER_DMA_COPY_A,
    GF100_DMA_COPY
};

DmaCopyAlloc::DmaCopyAlloc() :
    GrClassAllocator(s_ClassList, NUMELEMS(s_ClassList), "DMA_COPY")
{
}

UINT32 DmaCopyAlloc::EngineToInst(UINT32 engineId) const
{
    MASSERT(LW2080_ENGINE_TYPE_IS_COPY(engineId));
    return !LW2080_ENGINE_TYPE_IS_COPY(engineId) ? LW2080_ENGINE_TYPE_ALLENGINES :
                                                   LW2080_ENGINE_TYPE_COPY_IDX(engineId);
}

bool DmaCopyAlloc::Has64BitSemaphore(UINT32 classId)
{
     // 64bit semaphores are supported AMPERE_DMA_COPY_B onwards
    if ((classId == AMPERE_DMA_COPY_A) ||
        (classId == TURING_DMA_COPY_A) ||
        (classId == VOLTA_DMA_COPY_A) ||
        (classId == PASCAL_DMA_COPY_B) ||
        (classId == PASCAL_DMA_COPY_A) ||
        (classId == MAXWELL_DMA_COPY_A) ||
        (classId == KEPLER_DMA_COPY_A) ||
        (classId == GF100_DMA_COPY))
    {
        return false;
    }
    return true;
}

RC DmaCopyAlloc::AllocOnEngine
(
   LwRm::Handle hCh,
   UINT32       engineId,
   GpuDevice  * pGpuDev,
   LwRm       * pLwRm
)
{
    RC rc;
    UINT32 LwClass;
    LW85B5_ALLOCATION_PARAMETERS params = {0};

    if (!LW2080_ENGINE_TYPE_IS_COPY(engineId))
    {
        Printf(Tee::PriError, "Specified engine (%u) is not a copy engine!\n", engineId);
        return RC::SOFTWARE_ERROR;
    }

    MASSERT(pGpuDev);
    vector<UINT32> engineList;
    CHECK_RC(pGpuDev->GetEnginesFilteredByClass(vector<UINT32>(begin(s_ClassList), end(s_ClassList)),
                                                &engineList));

    if (std::find(engineList.begin(), engineList.end(), engineId) == engineList.end())
    {
        Printf(Tee::PriError, "Invalid Engine: %d.\n", engineId);
        return RC::SOFTWARE_ERROR;
    }

    SetRmClient(pLwRm);

    CHECK_RC(FindSupportedClass(pGpuDev, &LwClass));

    params.version = LWA0B5_ALLOCATION_PARAMETERS_VERSION_1;
    params.engineInstance = engineId;
    CHECK_RC(AllocProtected(hCh, pGpuDev, LwClass, &params));
    return rc;
}

// Function to get a list of enabled CopyEngines in the most recent
// version of the DMA graphics class
RC DmaCopyAlloc::GetEnabledPhysicalEnginesList(GpuDevice *pGpuDev, vector<UINT32> *engineList)
{
    MASSERT(pGpuDev);
    MASSERT(engineList);

    RC rc;
    vector<UINT32> classList(1, GetSupportedClass(pGpuDev));

    engineList->clear();
    CHECK_RC(pGpuDev->GetPhysicalEnginesFilteredByClass(classList, engineList));

    return rc;
}

//-----------------------------------------------------------------------------
RC DmaCopyAlloc::IsGrceEngine(UINT32 engine, GpuSubdevice *pSubdev, LwRm *pLwRm, bool *pbIsGrce)
{
    MASSERT(pSubdev);
    MASSERT(pLwRm);
    MASSERT(pbIsGrce);
    MASSERT((engine - LW2080_ENGINE_TYPE_COPY0) < LW2080_ENGINE_TYPE_COPY_SIZE);

    RC rc;

    LW2080_CTRL_CE_GET_CAPS_V2_PARAMS capsParams = {};
    capsParams.ceEngineType = engine;
    CHECK_RC(pLwRm->ControlBySubdevice(pSubdev,
                                       LW2080_CTRL_CMD_CE_GET_CAPS_V2,
                                       &capsParams, sizeof(capsParams)));

    *pbIsGrce = LW2080_CTRL_CE_GET_CAP(capsParams.capsTbl, LW2080_CTRL_CE_CAPS_CE_GRCE);

    return rc;
}

//-----------------------------------------------------------------------------
// ThreeDAlloc
//-----------------------------------------------------------------------------

UINT32 ThreeDAlloc::s_ClassList[] =
{
    BLACKWELL_A,
    HOPPER_B,
    HOPPER_A,
    ADA_A,
    AMPERE_B,
    AMPERE_A,
    TURING_A,
    VOLTA_A,
    PASCAL_B,
    PASCAL_A,
    MAXWELL_B,
    MAXWELL_A,
    KEPLER_C,
    KEPLER_B,
    KEPLER_A,
    FERMI_C,
    FERMI_B,
    FERMI_A,
    GT21A_TESLA,
    GT214_TESLA,
    GT200_TESLA,
    G82_TESLA,
    LW50_TESLA
};

ThreeDAlloc::ThreeDAlloc() :
    GrClassAllocator(s_ClassList, NUMELEMS(s_ClassList), "3D Class")
{
}

bool ThreeDAlloc::Has64BitSemaphore(UINT32 classId)
{
    // 64bit semaphores are supported AMPERE_B onwards
    if ((classId == AMPERE_A) ||
        (classId == TURING_A) ||
        (classId == VOLTA_A) ||
        (classId == PASCAL_B) ||
        (classId == PASCAL_A) ||
        (classId == MAXWELL_B) ||
        (classId == MAXWELL_A) ||
        (classId == KEPLER_C) ||
        (classId == KEPLER_B) ||
        (classId == KEPLER_A) ||
        (classId == FERMI_C) ||
        (classId == FERMI_B) ||
        (classId == FERMI_A) ||
        (classId == GT21A_TESLA) ||
        (classId == GT214_TESLA) ||
        (classId == GT200_TESLA) ||
        (classId == G82_TESLA) ||
        (classId == LW50_TESLA))
    {
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
// TwodAlloc
//-----------------------------------------------------------------------------

UINT32 TwodAlloc::s_ClassList[] =
{
    FERMI_TWOD_A,
    LW50_TWOD
};

TwodAlloc::TwodAlloc() :
    GrClassAllocator(s_ClassList, NUMELEMS(s_ClassList), "TWOD")
{
}

//-----------------------------------------------------------------------------
// LwdecAlloc
//-----------------------------------------------------------------------------

UINT32 LwdecAlloc::s_ClassList[] =
{
    LWC9B0_VIDEO_DECODER,
    LWB8B0_VIDEO_DECODER,
    LWC8B0_VIDEO_DECODER,
    LWC7B0_VIDEO_DECODER,
    LWC6B0_VIDEO_DECODER,
    LWC4B0_VIDEO_DECODER,
    LWC3B0_VIDEO_DECODER,
    LWC2B0_VIDEO_DECODER,
    LWC1B0_VIDEO_DECODER,
    LWC0B0_VIDEO_DECODER,
    LWB6B0_VIDEO_DECODER,
    LWB0B0_VIDEO_DECODER,
    LWA0B0_VIDEO_DECODER
};

LwdecAlloc::LwdecAlloc() :
    GrClassAllocator(s_ClassList, NUMELEMS(s_ClassList), "LWDEC")
{
}

UINT32 LwdecAlloc::EngineToInst(UINT32 engineId) const
{
    MASSERT(LW2080_ENGINE_TYPE_IS_LWDEC(engineId));
    return !LW2080_ENGINE_TYPE_IS_LWDEC(engineId) ? LW2080_ENGINE_TYPE_ALLENGINES :
                                                    LW2080_ENGINE_TYPE_LWDEC_IDX(engineId);
}

RC LwdecAlloc::AllocOnEngine
(
    LwRm::Handle hCh,
    UINT32       engineId,
    GpuDevice  * pGpuDev,
    LwRm       * pLwRm
)
{
    RC rc;
    UINT32 LwClass;
    LW_BSP_ALLOCATION_PARAMETERS params = {0};

    if (!LW2080_ENGINE_TYPE_IS_LWDEC(engineId))
    {
        Printf(Tee::PriError, "Specified engine (%u) is not a LWDEC engine!\n", engineId);
        return RC::SOFTWARE_ERROR;
    }

    MASSERT(pGpuDev);

    SetRmClient(pLwRm);

    CHECK_RC(FindSupportedClass(pGpuDev, &LwClass));

    params.size = sizeof(params);
    params.engineInstance = LW2080_ENGINE_TYPE_LWDEC_IDX(engineId);
    CHECK_RC(AllocProtected(hCh, pGpuDev, LwClass, &params));
    return rc;
}

bool LwdecAlloc::Has64BitSemaphore(UINT32 classId)
{
    // 64bit semaphores are supported LWC7B0_VIDEO_DECODER onwards
    if ((classId == LWC6B0_VIDEO_DECODER) ||
        (classId == LWC4B0_VIDEO_DECODER) ||
        (classId == LWC3B0_VIDEO_DECODER) ||
        (classId == LWC2B0_VIDEO_DECODER) ||
        (classId == LWC1B0_VIDEO_DECODER) ||
        (classId == LWC0B0_VIDEO_DECODER) ||
        (classId == LWB6B0_VIDEO_DECODER) ||
        (classId == LWB0B0_VIDEO_DECODER) ||
        (classId == LWA0B0_VIDEO_DECODER))
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
// EncoderAlloc
//-----------------------------------------------------------------------------

UINT32 EncoderAlloc::s_ClassList[] =
{
    LWC9B7_VIDEO_ENCODER,
    LWC7B7_VIDEO_ENCODER,
    LWC5B7_VIDEO_ENCODER,
    LWB4B7_VIDEO_ENCODER,
    LWC4B7_VIDEO_ENCODER,
    LWC3B7_VIDEO_ENCODER,
    LWC2B7_VIDEO_ENCODER,
    LWC1B7_VIDEO_ENCODER,
    LWD0B7_VIDEO_ENCODER,
    LWC0B7_VIDEO_ENCODER,
    LWA0B7_VIDEO_ENCODER,
    LW90B7_VIDEO_ENCODER
};

EncoderAlloc::EncoderAlloc() :
    GrClassAllocator(s_ClassList, NUMELEMS(s_ClassList), "ENCODER")
{
}

UINT32 EncoderAlloc::EngineToInst(UINT32 engineId) const
{
    MASSERT(LW2080_ENGINE_TYPE_IS_LWENC(engineId));
    return !LW2080_ENGINE_TYPE_IS_LWENC(engineId) ? LW2080_ENGINE_TYPE_ALLENGINES :
                                                    LW2080_ENGINE_TYPE_LWENC_IDX(engineId);
}

RC EncoderAlloc::AllocOnEngine
(
    LwRm::Handle hCh,
    UINT32       engineId,
    GpuDevice  * pGpuDev,
    LwRm       * pLwRm
)
{
    RC rc;
    UINT32 LwClass;
    LW_MSENC_ALLOCATION_PARAMETERS params = {0};

    if (!LW2080_ENGINE_TYPE_IS_LWENC(engineId))
    {
        Printf(Tee::PriError, "Specified engine (%u) is not a LWENC engine!\n", engineId);
        return RC::SOFTWARE_ERROR;
    }

    MASSERT(pGpuDev);

    SetRmClient(pLwRm);

    CHECK_RC(FindSupportedClass(pGpuDev, &LwClass));

    params.size = sizeof(params);
    params.engineInstance = LW2080_ENGINE_TYPE_LWENC_IDX(engineId);
    CHECK_RC(AllocProtected(hCh, pGpuDev, LwClass, &params));
    return rc;
}

bool EncoderAlloc::Has64BitSemaphore(UINT32 classId)
{
    // 64bit semaphores are supported LWC7B7_VIDEO_ENCODER onwards
    if ((classId == LWC5B7_VIDEO_ENCODER) ||
        (classId == LWB4B7_VIDEO_ENCODER) ||
        (classId == LWC4B7_VIDEO_ENCODER) ||
        (classId == LWC3B7_VIDEO_ENCODER) ||
        (classId == LWC2B7_VIDEO_ENCODER) ||
        (classId == LWC1B7_VIDEO_ENCODER) ||
        (classId == LWD0B7_VIDEO_ENCODER) ||
        (classId == LWC0B7_VIDEO_ENCODER) ||
        (classId == LWA0B7_VIDEO_ENCODER) ||
        (classId == LW90B7_VIDEO_ENCODER))
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
// SelwrityAlloc
//-----------------------------------------------------------------------------

UINT32 SelwrityAlloc::s_ClassList[] =
{
    LW95B1_VIDEO_MSVLD,
};

SelwrityAlloc::SelwrityAlloc() :
    GrClassAllocator(s_ClassList, NUMELEMS(s_ClassList), "SECURITY")
{
}

//-----------------------------------------------------------------------------
// TsecAlloc
//-----------------------------------------------------------------------------

UINT32 TsecAlloc::s_ClassList[] =
{
    LW95A1_TSEC
};

TsecAlloc::TsecAlloc() :
    GrClassAllocator(s_ClassList, NUMELEMS(s_ClassList), "SEC2")
{
}

//-----------------------------------------------------------------------------
// VideoCompositorAlloc
//-----------------------------------------------------------------------------

UINT32 VideoCompositorAlloc::s_ClassList[] =
{
    LWC5B6_VIDEO_COMPOSITOR,
    LWB1B6_VIDEO_COMPOSITOR,
    LWB0B6_VIDEO_COMPOSITOR,
    LW86B6_VIDEO_COMPOSITOR
};

VideoCompositorAlloc::VideoCompositorAlloc() :
    GrClassAllocator(s_ClassList, NUMELEMS(s_ClassList), "VIDEO_COMPOSITOR")
{
}

//-----------------------------------------------------------------------------
// ComputeAlloc
//-----------------------------------------------------------------------------

UINT32 ComputeAlloc::s_ClassList[] =
{
    BLACKWELL_COMPUTE_A,
    HOPPER_COMPUTE_B,
    HOPPER_COMPUTE_A,
    ADA_COMPUTE_A,
    AMPERE_COMPUTE_B,
    AMPERE_COMPUTE_A,
    TURING_COMPUTE_A,
    VOLTA_COMPUTE_A,
    PASCAL_COMPUTE_B,
    PASCAL_COMPUTE_A,
    MAXWELL_COMPUTE_B,
    MAXWELL_COMPUTE_A,
    KEPLER_COMPUTE_B,
    KEPLER_COMPUTE_A,
    FERMI_COMPUTE_B,
    FERMI_COMPUTE_A,
    LW50_COMPUTE
};

ComputeAlloc::ComputeAlloc() :
    GrClassAllocator(s_ClassList, NUMELEMS(s_ClassList), "COMPUTE")
{
}

UINT32 ComputeAlloc::EngineToInst(UINT32 engineId) const
{
    MASSERT(LW2080_ENGINE_TYPE_IS_GR(engineId));
    return !LW2080_ENGINE_TYPE_IS_GR(engineId) ? LW2080_ENGINE_TYPE_ALLENGINES :
                                                 LW2080_ENGINE_TYPE_GR_IDX(engineId);
}

bool ComputeAlloc::Has64BitSemaphore(UINT32 classId)
{
    // 64bit semaphores are supported AMPERE_COMPUTE_B onwards
    if ((classId == AMPERE_COMPUTE_A) ||
        (classId == TURING_COMPUTE_A) ||
        (classId == VOLTA_COMPUTE_A) ||
        (classId == PASCAL_COMPUTE_B) ||
        (classId == PASCAL_COMPUTE_A) ||
        (classId == MAXWELL_COMPUTE_B) ||
        (classId == MAXWELL_COMPUTE_A) ||
        (classId == KEPLER_COMPUTE_B) ||
        (classId == KEPLER_COMPUTE_A) ||
        (classId == FERMI_COMPUTE_B) ||
        (classId == FERMI_COMPUTE_A) ||
        (classId == LW50_COMPUTE))
    {
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
// Inline2MemoryAlloc
//-----------------------------------------------------------------------------

UINT32 Inline2MemoryAlloc::s_ClassList[] =
{
    KEPLER_INLINE_TO_MEMORY_B
};

Inline2MemoryAlloc::Inline2MemoryAlloc() :
    GrClassAllocator(s_ClassList, NUMELEMS(s_ClassList), "INLINE2MEMORY")
{
}

//-----------------------------------------------------------------------------
// DispSfUserAlloc
//-----------------------------------------------------------------------------

UINT32 DispSfUserAlloc::s_ClassList[] =
{
    LW9471_DISP_SF_USER,
    LW9271_DISP_SF_USER,
    LW9171_DISP_SF_USER
};

DispSfUserAlloc::DispSfUserAlloc() :
    GrClassAllocator(s_ClassList, NUMELEMS(s_ClassList), "DISP_SF_USER")
{
}

//-----------------------------------------------------------------------------
// UsermodeAlloc
//-----------------------------------------------------------------------------

UINT32 UsermodeAlloc::s_ClassList[] =
{
    HOPPER_USERMODE_A,
    AMPERE_USERMODE_A,
    TURING_USERMODE_A,
    VOLTA_USERMODE_A
};

UsermodeAlloc::UsermodeAlloc() :
    GrClassAllocator(s_ClassList, NUMELEMS(s_ClassList), "USERMODE")
{
}

RC UsermodeAlloc::Alloc(GpuSubdevice *pGpuSubdevice, LwRm *pLwRm, bool bar1Alloc)
{
    RC rc;
    UINT32 LwClass;
    GpuDevice* pGpuDev = pGpuSubdevice->GetParentDevice();
    SetRmClient(pLwRm);

    CHECK_RC(FindSupportedClass(pGpuDev, &LwClass));

    if (bar1Alloc)
    {
        if (LwClass < HOPPER_USERMODE_A)
        {
            Printf(Tee::PriError, 
                "BAR1 UserMode is not supported prior to Hopper Usermode class\n");
            return RC::SOFTWARE_ERROR;
        }
        else
        {
            Printf(Tee::PriDebug, "Allocating BAR1 UserMode\n");
            LW_HOPPER_USERMODE_A_PARAMS params = {};
            params.bBar1Mapping = true;
        
            CHECK_RC(AllocProtected(
                pLwRm->GetSubdeviceHandle(pGpuSubdevice), 
                pGpuDev, 
                LwClass, 
                &params));
        }
    }
    else
    {
        Printf(Tee::PriDebug, "Allocating legacy BAR0 UserMode\n");
        CHECK_RC(AllocProtected(
            pLwRm->GetSubdeviceHandle(pGpuSubdevice), 
            pGpuDev, 
            LwClass,
            nullptr));
    }

    return rc;
}

//-----------------------------------------------------------------------------
// LWJPG
//-----------------------------------------------------------------------------

UINT32 LwjpgAlloc::s_ClassList[] =
{
    LWC9D1_VIDEO_LWJPG,
    LWB8D1_VIDEO_LWJPG,
    LWC4D1_VIDEO_LWJPG,
    LWE7D0_VIDEO_LWJPG
};

LwjpgAlloc::LwjpgAlloc() :
    GrClassAllocator(s_ClassList, NUMELEMS(s_ClassList), "LWJPG")
{
}

UINT32 LwjpgAlloc::EngineToInst(UINT32 engineId) const
{
    MASSERT(LW2080_ENGINE_TYPE_IS_LWJPEG(engineId));
    return !LW2080_ENGINE_TYPE_IS_LWJPEG(engineId) ? LW2080_ENGINE_TYPE_ALLENGINES :
                                                    LW2080_ENGINE_TYPE_LWJPEG_IDX(engineId);
}

RC LwjpgAlloc::AllocOnEngine
(
    LwRm::Handle hCh,
    UINT32       engineId,
    GpuDevice  * pGpuDev,
    LwRm       * pLwRm
)
{
    RC rc;
    UINT32 LwClass;
    LW_BSP_ALLOCATION_PARAMETERS params = {0};

    if (!LW2080_ENGINE_TYPE_IS_LWJPEG(engineId))
    {
        Printf(Tee::PriError, "Specified engine (%u) is not a LWJPG engine!\n", engineId);
        return RC::SOFTWARE_ERROR;
    }

    MASSERT(pGpuDev);

    SetRmClient(pLwRm);

    CHECK_RC(FindSupportedClass(pGpuDev, &LwClass));

    params.size = sizeof(params);
    params.engineInstance = LW2080_ENGINE_TYPE_LWJPEG_IDX(engineId);
    CHECK_RC(AllocProtected(hCh, pGpuDev, LwClass, &params));
    return rc;
}

bool LwjpgAlloc::Has64BitSemaphore(UINT32 classId)
{
    // 64bit semaphores are supported LWB8D1_VIDEO_LWJPG onwards
    return (classId != LWC4D1_VIDEO_LWJPG);
}

//-----------------------------------------------------------------------------
// OFA
//-----------------------------------------------------------------------------

UINT32 OfaAlloc::s_ClassList[] =
{
    LWC9FA_VIDEO_OFA,
    LWB8FA_VIDEO_OFA,
    LWC8FA_VIDEO_OFA,
    LWC7FA_VIDEO_OFA,
    LWC6FA_VIDEO_OFA
};

OfaAlloc::OfaAlloc() :
    GrClassAllocator(s_ClassList, NUMELEMS(s_ClassList), "OFA")
{
}

bool OfaAlloc::Has64BitSemaphore(UINT32 classId)
{
    // 64bit semaphores are supported LWC7FA_VIDEO_OFA onwards
    return (classId != LWC6FA_VIDEO_OFA);
}
