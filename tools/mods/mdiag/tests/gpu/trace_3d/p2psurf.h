/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2017, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _P2PSURF_H_
#define _P2PSURF_H_

#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "random.h"
#include "tracemod.h"

class MdiagSurf;

// P2pSurfaceMapper (P2pSM):
// - P2pSM sits between trace_3d code and surfaces that have to be allocated on remote gpu.
// - During allocation time, trace_3d calls P2pSM and passes params.
// - P2pSM decides on a random basis if a surface needs to be mapped on remote device or locally.
// - It a surface is to be remote, it sets a flag in the MdiagSurface and sets the accessing, host devices.
// - MdiagSurface would then allocate the surface using the host_device handle and do MapPeer to allow current device to access it
// - MdiagSurface would also return either local or peer CtxDmaHandle/Offset, based on this flag.
// The life-cycle of P2pSurfaceMapper is:
// - SetParams: Pass the pointer to the params from command line;
// - Setup: Reads the P2p-related params and identifies the different gpu devices
//       does not depend on the surfaces;
// - GetP2pDeviceMapping:
//     Decides if surface needs to be local or Remote. If Remote, it sets the field in mdiagSurface to enable Remote allocation
//
// For P2p testing with 2+Gpus (non-sli mode), surfaces of a gpu would randomly be allocated in either of the gpus. Consider
// gpu0: surfaceA, surfaceB
// gpu1: surfaceY, surfaceZ
// Based on the p2p local/remote args, consider the random allocation
// gpu0: surfaceA(peer), surfaceB(local)
// gpu1: surfaceY(local), surfaceZ(local)
// In this case during allocation of surface, if P2pManager returns peer, the allocation happens on the remote device(surfaceA on gpu1 instead of gpu0)
// gpu0 would have a peer VA to this surface, no local VA (would lead to fault/weird errors if used)
// gpu1 would have a local VA, but it would never be used as the trace running on gpu1 wouldn't be aware of this surface
// if P2p mode is not enabled, then it will return the current device
class P2pSurfaceMapper
{
public:
    // Ctors and = operator.
    P2pSurfaceMapper();

    ~P2pSurfaceMapper();

    // Accessors and state info.
    // ---------------------------------------------------------------------
    void SetParams(const ArgReader* params) { m_Params = params; }
    const ArgReader* GetParams() const { return m_Params; }

    RC Setup();

    // Actions.
    // ---------------------------------------------------------------------
    //! Distribute the surface over the devices using user's probability
    //! distribution.
    GpuDevice* GetP2pDeviceMapping(GpuDevice* pGpuDev, GpuTrace::TraceFileType FileType, MdiagSurf* surface);

private:
    // Explicitly disable copy and assignment
    P2pSurfaceMapper(const P2pSurfaceMapper&);
    P2pSurfaceMapper* operator=(const P2pSurfaceMapper&);

    map<UINT32, GpuDevice*> m_GpuMap;

    void CreateRandomList( vector<UINT32>* rlist,
        Random* radObj, UINT32 size) const;

    //! Params.
    const ArgReader* m_Params;
    UINT32 m_P2pLocalSurfaces;
    UINT32 m_P2pRemoteSurfaces;
    UINT32 m_P2pSeed;
};

#endif
