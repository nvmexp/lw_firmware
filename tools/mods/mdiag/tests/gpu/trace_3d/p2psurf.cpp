/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2017,2019, 2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h"
// first.
#include "mdiag/tests/stdtest.h"

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "p2psurf.h"

#include "mdiag/utils/utils.h"

#include "core/include/fpicker.h"

#include "mdiag/utils/surfutil.h"

#include "gpu/include/gpudev.h"
#include "tracemod.h"
#include "teegroups.h"

#define MSGID() T3D_MSGID(P2P)
// //////////////////////////////////////////////////////////////////////////

P2pSurfaceMapper::P2pSurfaceMapper() :
    m_Params(0),
    m_P2pLocalSurfaces(0), m_P2pRemoteSurfaces(0), m_P2pSeed(0) {}

P2pSurfaceMapper::~P2pSurfaceMapper()
{
}

RC P2pSurfaceMapper::Setup()
{
    RC rc = OK;
    if (!m_GpuMap.empty())
    {
        return rc;
    }
    if (GetParams() == 0)
    {
        DebugPrintf(MSGID(),"%s: No P2p Params specified, hence skipping\n", __FUNCTION__);
        return rc;
    }
    // Assign the cmd line values or use the default values.
    m_P2pLocalSurfaces =
        m_Params->ParamUnsigned("-p2p_local_surfaces", 100);
    DebugPrintf(MSGID(),"%s: Setting m_P2pLocalSurfaces= %d\n",
        __FUNCTION__, m_P2pLocalSurfaces);

    m_P2pRemoteSurfaces =
        m_Params->ParamUnsigned("-p2p_remote_surfaces", 0);
    DebugPrintf(MSGID(),"%s: Setting m_P2pRemoteSurfaces= %d\n",
        __FUNCTION__, m_P2pRemoteSurfaces);

    if (m_P2pLocalSurfaces + m_P2pRemoteSurfaces == 0)
    {
        ErrPrintf("%s: ERROR -p2p_local_surfaces and "
            "-p2p_remote_surfaces cannot be both 0.\n",
            __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    m_P2pSeed = m_Params->ParamUnsigned("-p2p_seed", 0xdeadd1a8);
    DebugPrintf(MSGID(),"%s: Setting P2pSeed= %u\n", __FUNCTION__, m_P2pSeed);

    for (auto pLWGpuResource : LWGpuResource::GetAllResources())
    {
        GpuDevice* pGpuDev = pLWGpuResource->GetGpuDevice();
        //Need to check if device-ID is unique ?
        MASSERT(m_GpuMap.count(pGpuDev->GetDeviceInst()) == 0);
        DebugPrintf(MSGID(),"%s: Registering device %d for P2P \n", __FUNCTION__, pGpuDev->GetDeviceInst());
        m_GpuMap.insert(make_pair(pGpuDev->GetDeviceInst(),pGpuDev));
    }

    return rc;
}

GpuDevice* P2pSurfaceMapper::GetP2pDeviceMapping(GpuDevice* pGpuDev, GpuTrace::TraceFileType FileType, MdiagSurf* surface)
{
    DebugPrintf(MSGID(),"%s: Entering P2pSurfaceMapper\n", __FUNCTION__);
    MASSERT(pGpuDev);
    MASSERT(surface);
    GpuDevice* hostingGpuDevice = pGpuDev;
    int accessingGpuDevice = pGpuDev->GetDeviceInst();
    int numDevices = m_GpuMap.size();
    if (numDevices <= 1)
    {
        return hostingGpuDevice;
    }
    if (m_P2pLocalSurfaces == 100)
    {
        DebugPrintf(MSGID(),"%s: All surfaces are marked as local, nothing to do\n", __FUNCTION__);
        return hostingGpuDevice;
    }
    if (GpuTrace::FT_TEXTURE_HEADER == FileType)
    {
        DebugPrintf(MSGID(),"%s: Surface Filetype is Texture_Header, so marking surface as local\n", __FUNCTION__);
        return hostingGpuDevice;
    }
    MASSERT(m_GpuMap.count(accessingGpuDevice) == 1);
    DebugPrintf(MSGID(),"%s: P2P - randomizing location for surface=%s\n", __FUNCTION__, surface->GetName().c_str());

    //
    // The algorithm implemented here is:
    //
    // // normalize the distribution, if not already normalized
    // peer_distribution.normalize()
    // surface_location = sample(peer_distribution)
    // if (surface_location == "remote")
    //   Set mdiag location flag as remote
    //   // pick a random gpu to use as remote excluding current gpu
    //   return (rand(gpu_set - {gpu} ))
    // else
    //   Set mdiag location flag as local
    //   return (gpu)
    //

    // Define a fancy picker context.
    FancyPicker::FpContext fpCtx_local;
    fpCtx_local.Rand.SeedRandom(m_P2pSeed);
    fpCtx_local.RestartNum = 0;
    fpCtx_local.LoopNum = 0;
    fpCtx_local.pJSObj = NULL;
    // Define the local/remote picker.
    FancyPicker fp_local;
    fp_local.SetContext(&fpCtx_local);
    RC rc = fp_local.ConfigRandom();
    if (rc != OK)
    {
        ErrPrintf("%s: ConfigRandom failed", __FUNCTION__);
        return hostingGpuDevice;
    }
    enum { LOCAL = 0, REMOTE };
    fp_local.AddRandItem(m_P2pLocalSurfaces, LOCAL);
    fp_local.AddRandItem(m_P2pRemoteSurfaces, REMOTE);
    fp_local.CompileRandom();

    Random  random_gen;
    random_gen.SeedRandom(m_P2pSeed);
    vector<UINT32> fp_list(numDevices);
    if (surface->GetLocation() == Memory::Fb)
    {
        if (fp_local.Pick() == REMOTE)
        {
            CreateRandomList( &fp_list, &random_gen, numDevices );
            int hostingDeviceInst = fp_list[accessingGpuDevice];
            hostingGpuDevice = m_GpuMap[hostingDeviceInst];
            surface->SetSurfaceAsRemote(hostingGpuDevice);
            DebugPrintf(MSGID(),"%s: Allocating surface=%s of GpuDevice= %d remote on GpuDevice = %d\n",
                    __FUNCTION__, surface->GetName().c_str(), accessingGpuDevice, hostingDeviceInst);
        } else
        {
            DebugPrintf(MSGID(),"%s: Allocating surface=%s of GpuDevice= %d locally\n",
                    __FUNCTION__, surface->GetName().c_str(), accessingGpuDevice);
        }
    } else
    {
        DebugPrintf(MSGID(),"%s: surface=%s not in FB, so skipping. Loc=%d (%d), Address Model=%d (%d), Mem_handle=%d\n",
            __FUNCTION__, surface->GetName().c_str(),surface->GetLocation(),Memory::Fb,surface->GetAddressModel(),Memory::Paging,surface->GetMemHandle());
    }
    return hostingGpuDevice;
}

// CreateRandomList will generate a random list of number from 0..size-1, and
// guarantee that for any location i, the number is not i.
void P2pSurfaceMapper::CreateRandomList( vector<UINT32>* rlist,
    Random* radObj, UINT32 size) const
{
    MASSERT(size > 1);
    (*rlist).resize( size );

    if (size == 2)
    {
        // Only one choice
        (*rlist)[0] = 1;
        (*rlist)[1] = 0;
        return;
    }

    UINT32 i;

    // first fill the array with numbers
    for (i=0; i<size; ++i)
    {
        (*rlist)[i] = i;
    }

    // Now for each location, shuffle the data until this location
    // get a value differnet from the location number
    i = 0;
    while (i < (size - 1))
    {
        if ((*rlist)[i] == i)
        {
            radObj->Shuffle(size-i, &((*rlist)[i]));
        }
        else
        {
            ++i;
        }
    }

    if ((*rlist)[size-1] == (size-1))
    {
        // Special handling for last location
        (*rlist)[size-1] = (*rlist)[size-2];
        (*rlist)[size-2] = size-1;
    }
}
