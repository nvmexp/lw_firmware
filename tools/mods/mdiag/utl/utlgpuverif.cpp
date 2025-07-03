/*************************************************************************
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "utlgpuverif.h"
#include "mdiag/resource/lwgpu/verif/GpuVerif.h"
#include "mdiag/tests/gpu/trace_3d/trace_3d.h"
#include <regex>
#include "mdiag/smc/smcengine.h"
#include "gpu/include/gpudev.h"
#include "mdiag/resource/lwgpu/verif/SurfBufInfo.h"

using namespace std;

map<UtlGpuVerif::ResourceKey, unique_ptr<UtlGpuVerif> > UtlGpuVerif::s_GpuVerifs;

/*static*/UtlGpuVerif * UtlGpuVerif::GetGpuVerif
(
    Test * pTest, 
    LwRm::Handle hVaSpace,
    LWGpuChannel * pChannel
)
{
    if (pTest == nullptr)
    {
        MASSERT(!"Not support global surface. Need to enhance it.");
    }

    ResourceKey key(pTest, hVaSpace, pChannel);
    if (s_GpuVerifs.find(key) == s_GpuVerifs.end())
    {
        unique_ptr<UtlGpuVerif> utlGpuVerif(pTest->CreateGpuVerif(hVaSpace, pChannel));
        s_GpuVerifs[key] = std::move(utlGpuVerif);
    }

    return s_GpuVerifs[key].get();
}

UtlT3dGpuVerif::UtlT3dGpuVerif
(
    Trace3DTest * pTest,
    LwRm::Handle hVaSpace,
    LWGpuChannel * pChannel
) :
    m_pTest(pTest)
{
    MASSERT(pTest);

    LwRm * pLwRm = m_pTest->GetLwRmPtr();
    SmcEngine * pSmcEngine = m_pTest->GetSmcEngine(); 
    LWGpuResource * pGpuRes = m_pTest->GetGpuResource();
    const ArgReader * params = m_pTest->GetParam();

    IGpuSurfaceMgr * pGsm = pTest->GetSurfaceMgr();
    TraceChannel * pTraceChannel = m_pTest->GetChannel(pChannel->GetName());
    TraceSubChannel* pSub = pTraceChannel->GetGrSubChannel();
    if ( !pSub )
    {
        pSub = pTraceChannel->GetSubChannel("");
    }
    m_pGpuVerif = new GpuVerif(pLwRm, 
            pSmcEngine, pGpuRes, pGsm, params, 
            pSub->GetClass(), nullptr, nullptr,
            pTraceChannel, pTraceChannel->GetTraceFileMgr(),
            m_pTest->NeedDmaCheck());

    if (!m_pGpuVerif->Setup(pGsm->GetBufferConfig()))
    {
        MASSERT(!"SetUp gpu verif failed.");
    }

    GpuSubdevice* pSubdev = m_pTest->GetBoundGpuDevice()->GetSubdevice(0);
    m_pGpuVerif->SetCrcChainManager(params, pSubdev, pSub->GetClass());
}


void UtlT3dGpuVerif::AddSurfaceInfo(MdiagSurf * pSurf)
{
    regex cPattern("Color[A-H]");
    regex zPattern("Zeta");
    smatch results;
    string surfaceName = pSurf->GetName();
    if (regex_match(surfaceName, results, cPattern))
    {
        // No need to know the index since the search will compare the whole name
        m_pGpuVerif->AddSurfC(pSurf, 0,
                CrcRect());
    }
    else if (regex_match(surfaceName, results, zPattern))
    {
        m_pGpuVerif->AddSurfZ(pSurf, CrcRect());
    }
    else
    {
        // leave surface size as 0 so that size will be callwalted based on width/height for BL since data outside are not interested
        m_pGpuVerif->AddAllocSurface(pSurf, pSurf->GetName().c_str(), CRC_CHECK, pSurf->GetOffset(), 0, 0, false, CrcRect(), false, CrcChainManager::UNSPECIFIED_CLASSNUM);
    }
}

SurfaceInfo * UtlT3dGpuVerif::GetSurfInfo(const string & name)
{
    for (auto surfInfo : m_pGpuVerif->AllSurfaces())
    {
        auto surface = surfInfo->GetSurface();
        if (surface != nullptr)
        {
            if (surface->GetName() == name)
            {
                return surfInfo;
            }
        }
    }

    return nullptr;
}

GpuDevice * UtlT3dGpuVerif::GetGpuDevice()
{
    return  m_pTest->GetBoundGpuDevice();
}

GpuSubdevice * UtlT3dGpuVerif::GetSubDevice()
{
    GpuDevice * pGpuDev = GetGpuDevice();
    if (pGpuDev == nullptr)
    {
        MASSERT(!"Get gpu device failed.");
    }

    GpuSubdevice* pSubdev = pGpuDev->GetSubdevice(GetSubDevIndex());
    return pSubdev;
}

UINT32 UtlT3dGpuVerif::GetSubDevIndex()
{
    GpuDevice * pGpuDev = GetGpuDevice();
    if (pGpuDev == nullptr)
    {
        MASSERT(!"%s: Get gpu device failed.");
    }

    MASSERT(pGpuDev->GetNumSubdevices() == 1);

    return 0;
}

UINT32 UtlT3dGpuVerif::CallwlateCrc
(
    MdiagSurf * pSurf,
    UINT32 preCrcValue
)
{
    SurfaceInfo * pSurfInfo = GetSurfInfo(pSurf->GetName());

    if (pSurfInfo == nullptr)
    {
        AddSurfaceInfo(pSurf);
        pSurfInfo = GetSurfInfo(pSurf->GetName());
    }

    m_pGpuVerif->SetPreCrcValue(preCrcValue);
    return m_pGpuVerif->CallwlateCrc(pSurf->GetName(), GetSubDevIndex(), nullptr);
}

string UtlT3dGpuVerif::GetCrcKey
(
    MdiagSurf * pSurf 
)
{
    SurfaceInfo * pSurfInfo = GetSurfInfo(pSurf->GetName());

    if (pSurfInfo == nullptr)
    {
        AddSurfaceInfo(pSurf);
        pSurfInfo = GetSurfInfo(pSurf->GetName());
    }

    string checkProfileKey("");
    if (pSurfInfo->GetProfKey().empty())
    {
        // After we have the traceop context we can know the suffix
        string key = "0";
        if (!pSurfInfo->GenProfileKey(&key[0], true, GetSubDevIndex(), checkProfileKey))
        {
            MASSERT(!"Get SurfInfo failure.");
        }
    }

    return checkProfileKey;
}

RC UtlT3dGpuVerif::DumpImageToFile
(
    MdiagSurf * pSurf,
    const string & fileName
)
{
    SurfInfo2 * pSurfInfo = 
        dynamic_cast<SurfInfo2 *>(GetSurfInfo(pSurf->GetName()));

    if (pSurfInfo == nullptr)
    {
        AddSurfaceInfo(pSurf);
        pSurfInfo = dynamic_cast<SurfInfo2 *>(GetSurfInfo(pSurf->GetName()));
    }

    return pSurfInfo->DumpImageToFile(fileName, GetCrcKey(pSurf), GetSubDevIndex());

}

void UtlT3dGpuVerif::ReadSurface
(
    void * data,
    MdiagSurf * pSurface
)
{
   SurfaceReader * pSurfaceReader = m_pGpuVerif->GetDmaUtils()->GetSurfaceReader();
   if (pSurfaceReader)
   {
       pSurfaceReader->ReadSurface(static_cast< vector<UINT08> *>(data), pSurface, GetSubDevIndex(), true, g_NullRectangle);
   }
}
