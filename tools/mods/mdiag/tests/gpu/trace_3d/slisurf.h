/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _SLISURF_H_
#define _SLISURF_H_

#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "random.h"

class MdiagSurf;

// SliSurfaceMapper (SSM):
// - SSM sits between trace_3d code and surfaces that have to be peer-ed.
// - clients that want to use peer surfaces, query SSM to get CtxDmaOffset.
// - SSM is used at relocation time and when massaging the push buffer to
// get offset of surfaces, based on where the surfaces are allocated and which
// GpuDevice is accessing.
// - SSM can be easily extend to allocate surfaces on several GPU subdevice
// based on the "kind of surface" (color, Z, ...)
// - SSM understands that there are several GPU devices with several
// subdevices. So it shouldn't require a redesign in the case of multi SLI
// system (I've been told me that it is a realistic case for testing /
// manufacturing)
// - SSM talks to MdiagSurf and LWSurface using the peer interface that
// CharlieD designed for MdiagSurf and that inspired all the code here.
// - SSM implements the algorithm spec-ed out in bug 289645 by Arch and Sw.
//
// MapSliSurface can selectively choose which surfaces should be peered
// for finer control on the testing scenarios.
//
// The life-cycle of SliSurfaceMapper is:
// - SetParams: Pass the pointer to the params from command line;
// - Setup: Reads the SLI-related params and initializes the state which does
// not depend on the surfaces;
// - Add...Surface: Declares and makes a surface peer-able.
// - AllocateSurfaces:
//     . When all the surfaces have been declared, AllocateSurfaces shuffles
//     the surfaces over the subdevices and makes all the RM calls needed to
//     make the surfaces peer-ed.
//     . After AllocateSurfaces no surface should be added.
// - GetCtxDmaOffset: Gets the peer/local offset of the surface, depending
// on where the surface is from the accessing subdevice's point of view.
class SliSurfaceMapper
{
public:
    typedef map<UINT32, UINT32> Accessing2Hosting;
    typedef struct
    {
        UINT32 access;
        UINT32 host;
        MdiagSurf *buff;
    } BuffAccessInfo;
    typedef map<string, BuffAccessInfo> Name2Buff_type;

    // Ctors and = operator.
    SliSurfaceMapper();

    ~SliSurfaceMapper();

    // Accessors and state info.
    // ---------------------------------------------------------------------
    void SetParams(const ArgReader* params) { m_Params = params; }
    const ArgReader* GetParams() const { return m_Params; }

    RC Setup();

    // Return whether we are running on an SLI system or not.
    bool IsSliSystem(GpuDevice* pGpuDev) const;
    // Return true <-> all the surfaces are mirrored on the gpu subdevices.
    bool IsMilestone1Mode() const;
    // Return true <-> a surface in the Texture header might be peered.
    bool IsMilestone2Mode() const;
    // Return true <-> a surface pointed by the Push-Buffer might be peered.
    bool IsMilestone3Mode() const;

    // Methods to declare surfaces that have to be peer-ed.
    // ---------------------------------------------------------------------
    void AddLWSurface(GpuDevice* pGpuDev, MdiagSurf* pSurf);
    void AddDmaBuffer(GpuDevice* pGpuDev, MdiagSurf* pSurf, bool canBePartialRemote);
    void AddDmaBuffer(GpuDevice* pGpuDev, MdiagSurf* pSurf, const Accessing2Hosting& a2h,
        bool canBePartialRemote);

    // Actions.
    // ---------------------------------------------------------------------
    //! Distribute the surfaces over the subdevices using user's probability
    //! distribution.
    //! ILW: All the surfaces for all the GPU devices/subdevices have been
    //! registered at this point.
    RC AllocateSurfaces(GpuDevice* pGpuDev);

    //! Query accessing map to return the hosting subdev for a specific
    //! surface from some subdev
    UINT32 GetHostingSubdevice(MdiagSurf *surf,
                               GpuDevice *pGpuDev,
                               UINT32 accessingSubdevice) const;

    //! Query accessing map to return the accessing subdev for a specific
    //! surface from some subdev
    UINT32 GetAccessingSubdevice(MdiagSurf *surf,
                               GpuDevice *pGpuDev,
                               UINT32 hostingSubdevice) const;

    // Query methods
    // ---------------------------------------------------------------------
    //! Returns the CtxDmaOffset of the Surface "pLWSurface" as seen by the
    //! subdevice "accessingSubdevice" of the device "pGpuDev".
    //! If pGpuDev is 0, the current GpuDevice will be used.
    //! If subdevice is ~0, the current subdevice is used. It won't work in
    //! an SLI system with 2^32 boards.
    UINT64 GetCtxDmaOffset(MdiagSurf* pSurf,
                           GpuDevice* pGpuDev,
                           UINT32 accessingSubdevice) const;

    // Query methods
    // ---------------------------------------------------------------------
    int GetSurfaceNumber(GpuDevice* pGpuDev) const;
    //! Return the location info map to support local/remote printing
    Name2Buff_type GetBuffLocInfo() const { return m_Name2Buff; }
    //! Print the state of SSM with all the mappings for one GpuDev.
    void Print(GpuDevice* pGpuDev) const;
    //! Print the state of SSM with all the mappings for all the GpuDev.
    void Print() const;
private:
    // Explicitly disable copy and assignment
    SliSurfaceMapper(const SliSurfaceMapper&);
    SliSurfaceMapper* operator=(const SliSurfaceMapper&);

    void DumpMemory(void* ptr, UINT32 byteSize) const;
    void CreateRandomList( vector<UINT32>& rlist,
        Random& rad_obj, UINT32 size) const;

    //! Store info for a single GpuDevice with seveal subdevices.
    class PerGpuDeviceInfo
    {
    public:
        //! List of all registered surfaces.
        typedef set<MdiagSurf*> SurfaceSet;
        SurfaceSet m_DmaBufferSet;
        SurfaceSet m_RenderSurfaceSet;
        //! Store the map between surfaces and subdevices that host that
        //! surface.
        typedef SliSurfaceMapper::Accessing2Hosting Accessing2Hosting;
        typedef map<MdiagSurf*, Accessing2Hosting> SurfaceMap;
        SurfaceMap m_DmaBufferMap;
        SurfaceMap m_RenderSurfaceMap;
    public:
        PerGpuDeviceInfo(GpuDevice* pgd);
        //! Pretty printing of GPU surface mapping.
        void PrintMap(const Accessing2Hosting& m) const;
        //! Print the state of PerGpuDeviceInfo.
        void Print() const;
        //! Calls MapPeer on all the surfaces.
        RC MapPeer();
    private:
        GpuDevice* m_pGpuDev;
    };
    //! Stores the registered GpuDevices and the subdevices.
    typedef map<GpuDevice*, PerGpuDeviceInfo> GpuDeviceInfo_type;
    GpuDeviceInfo_type m_GpuDeviceInfo;
    //! Store the location info for each buffer
    Name2Buff_type m_Name2Buff;
    typedef map<MdiagSurf*, bool> PartialRemote_type;
    PartialRemote_type m_CanBePartialRemote;
    //! Params.
    const ArgReader* m_Params;
    UINT32 m_SliLocalSurfaces;
    UINT32 m_SliRemoteSurfaces;
    UINT32 m_SliSeed;
};

#endif
