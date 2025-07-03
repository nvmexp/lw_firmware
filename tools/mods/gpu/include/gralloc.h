/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2013, 2015, 2017-2019, 2021 by LWPU Corporation.  All 
 * rights reserved. All information contained herein is proprietary and 
 * confidential to LWPU Corporation.  Any use, reproduction, or disclosure 
 * without the written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   gralloc.h
 *
 * @brief  Useful classes that abstract away the act of choosing the most
 *         recent version of a graphics class.
 *
 * Based heavily on Charlie's PickHelper (Random2d)
 */

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#ifndef INCLUDED_GRALLOC_H
#define INCLUDED_GRALLOC_H

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

/**
 * Abstract base class for graphics class allocators
 *
 * The idea with this class is to hide the code that allocates the most recent
 * version of a graphics class.  Notice that the derived classes all construct
 * an array of graphics classes from newest to oldest.
 *
 * Unless explicitly stated (ie grabbing a user specified class from JavaScript
 * and calling SetClassOverride) this class will allocate the first class
 * it can from the class list.
 */
//-----------------------------------------------------------------------------
class GrClassAllocator
{
public:
    virtual ~GrClassAllocator();

    // Non-virtual Getters and Setters
    LwRm::Handle GetHandle() const;
    UINT32       GetClass() const;
    void         SetClassOverride(UINT32 requestedClass);
    void         SetOldestSupportedClass(UINT32 oldestSupportedClass);
    void         SetNewestSupportedClass(UINT32 newestSupportedClass);
    void         GetClassList(const UINT32 **ppClassList,
                              UINT32 *pNumClasses) const;
    bool         HasClass(UINT32 classId) const;

    bool         IsSupported(GpuDevice *pGpuDev) const;

    //! Get the default engine ID to use when one is notspecified.  With newer GPUs
    //! (Ampere+), RM requires that the specific engine ID is provided when allocating
    //! classes that have multiple instances
    RC GetDefaultEngineId
    (
        GpuDevice * pGpuDev
       ,LwRm      * pLwRm
       ,UINT32    * pEngineId
    ) const;

    //! Get the engine ID associated with a particular instance of the GPU object
    //! The engine Id that is returned will be from sdk/lwpu/inc/class/cl2080.h
    RC GetEngineId
    (
        GpuDevice * pGpuDev
       ,LwRm      * pLwRm
       ,UINT32      instance
       ,UINT32    * pEngineId
    ) const;

    UINT32         GetSupportedClass(GpuDevice *pGpuDev) const;
    RC             Alloc(GpuSubdevice *pGpuSubdevice, LwRm *pLwRm);
    virtual RC     Alloc(LwRm::Handle hCh, GpuDevice *pGpuDev, LwRm *pLwRm = 0);
    virtual RC     AllocOnEngine
    (
        LwRm::Handle hCh
       ,UINT32       engineId
       ,GpuDevice  * pGpuDev
       ,LwRm       * pLwRm
    );
    virtual bool   Has64BitSemaphore(UINT32 classId);
    void           Free();
    const char *   GetName() { return m_Name; }

protected:
    // GrClassAllocator (and its subclasses) are multi client compliant and
    // should never use the LwRmPtr constructor without specifying which
    // client
    DISALLOW_LWRMPTR_IN_CLASS(GrClassAllocator);

    GrClassAllocator(UINT32 *pClassList, size_t numClasses, const char *name);
    RC   FindSupportedClass(GpuDevice *pGpuDev, UINT32 *pLwClass) const;
    RC   AllocProtected(LwRm::Handle hParent, GpuDevice *pGpudev,
                        UINT32 LwClass, void *pGrParms);
    void SetRmClient(LwRm *pLwRm);

    // This colwerts an engine ID to an engine instance.  The base behavior is
    // for engines that only have a single instance and therefore just return 0
    virtual UINT32 EngineToInst(UINT32 engineId) const { return 0; }

private:
    LwRm::Handle  m_hLwObject;
    UINT32        m_LwClass;
    UINT32        m_RequestedClass;
    UINT32        m_OldestSupportedClass;
    UINT32        m_NewestSupportedClass;
    UINT32      * m_pClassList;
    size_t        m_NumClasses;
    const char  * m_Name;
    LwRm        * m_pLwRm;
};

//-----------------------------------------------------------------------------
// Derived Classes
//-----------------------------------------------------------------------------

//------------------------------------------------------------------------------
class DmaCopyAlloc : public GrClassAllocator
{
public:
    DmaCopyAlloc();
    UINT32 EngineToInst(UINT32 engineId) const override;
    RC AllocOnEngine(LwRm::Handle hCh, UINT32 instance, GpuDevice *pGpuDev, LwRm *pLwRm) override;
    RC GetEnabledPhysicalEnginesList(GpuDevice *pGpuDev, vector<UINT32> *engineList);
    bool Has64BitSemaphore(UINT32 classId) override;
    RC IsGrceEngine(UINT32 engine, GpuSubdevice *pSubdev, LwRm *pLwRm, bool *pbIsGrce);
private:
    static UINT32 s_ClassList[];
};

//-----------------------------------------------------------------------------
class ThreeDAlloc : public GrClassAllocator
{
public:
    ThreeDAlloc();
    bool Has64BitSemaphore(UINT32 classId) override;
private:
    static UINT32 s_ClassList[];
};

//-----------------------------------------------------------------------------
class TwodAlloc : public GrClassAllocator
{
public:
    TwodAlloc();
    static const UINT32 MAX_COPY_HEIGHT = 131072;
private:
    static UINT32 s_ClassList[];
};

//-----------------------------------------------------------------------------
class LwdecAlloc : public GrClassAllocator
{
public:
    LwdecAlloc();
    UINT32 EngineToInst(UINT32 engineId) const override;
    RC AllocOnEngine(LwRm::Handle hCh, UINT32 engineId, GpuDevice *pGpuDev, LwRm *pLwRm) override;
    bool Has64BitSemaphore(UINT32 classId) override;
private:
    static UINT32 s_ClassList[];
};

//-----------------------------------------------------------------------------
class EncoderAlloc : public GrClassAllocator
{
public:
    EncoderAlloc();
    UINT32 EngineToInst(UINT32 engineId) const override;
    RC AllocOnEngine(LwRm::Handle hCh, UINT32 engineId, GpuDevice *pGpuDev, LwRm *pLwRm) override;
    bool Has64BitSemaphore(UINT32 classId) override;
private:
    static UINT32 s_ClassList[];
};

//-----------------------------------------------------------------------------
class SelwrityAlloc : public GrClassAllocator
{
public:
    SelwrityAlloc();
private:
    static UINT32 s_ClassList[];
};

//-----------------------------------------------------------------------------
class TsecAlloc : public GrClassAllocator
{
public:
    TsecAlloc();
private:
    static UINT32 s_ClassList[];
};

//-----------------------------------------------------------------------------
class VideoCompositorAlloc : public GrClassAllocator
{
public:
    VideoCompositorAlloc();
private:
    static UINT32 s_ClassList[];
};

//-----------------------------------------------------------------------------
class ComputeAlloc : public GrClassAllocator
{
public:
    ComputeAlloc();
    UINT32 EngineToInst(UINT32 engineId) const override;
    bool Has64BitSemaphore(UINT32 classId) override;
private:
    static UINT32 s_ClassList[];
};

//-----------------------------------------------------------------------------
class Inline2MemoryAlloc : public GrClassAllocator
{
public:
    Inline2MemoryAlloc();
private:
    static UINT32 s_ClassList[];
};

//-----------------------------------------------------------------------------
class DispSfUserAlloc : public GrClassAllocator
{
public:
    DispSfUserAlloc();
private:
    static UINT32 s_ClassList[];
};

//------------------------------------------------------------------------------
class UsermodeAlloc : public GrClassAllocator
{
public:
    UsermodeAlloc();
    RC Alloc(GpuSubdevice *pGpuSubdevice, LwRm *pLwRm, bool bar1Alloc);
private:
    static UINT32 s_ClassList[];
};

//------------------------------------------------------------------------------
class LwjpgAlloc : public GrClassAllocator
{
public:
    LwjpgAlloc();
    UINT32 EngineToInst(UINT32 engineId) const override;
    RC AllocOnEngine(LwRm::Handle hCh, UINT32 engineId, GpuDevice *pGpuDev, LwRm *pLwRm) override;
    bool Has64BitSemaphore(UINT32 classId) override;
private:
    static UINT32 s_ClassList[];
};

//------------------------------------------------------------------------------
class OfaAlloc : public GrClassAllocator
{
public:
    OfaAlloc();
    bool Has64BitSemaphore(UINT32 classId) override;
private:
    static UINT32 s_ClassList[];
};

#endif
