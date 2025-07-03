/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file lwgsurf.cpp
//! \brief Implements the LwdaGoldenSurfaces class.
//! This class manages the CRCs / Checksums for a group of tiles of a given
//! surface. Each tile has its own width, height, and crc value.
#include "lwgoldensurfaces.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "lwos.h"

//------------------------------------------------------------------------------
LwdaGoldenSurfaces::~LwdaGoldenSurfaces()
{
}

//------------------------------------------------------------------------------
int LwdaGoldenSurfaces::NumSurfaces() const
{
    return (int)m_Surf.size();
}

//------------------------------------------------------------------------------
const string & LwdaGoldenSurfaces::Name (int SurfNum) const
{
    return m_Surf[SurfNum].name;
}

//------------------------------------------------------------------------------
RC LwdaGoldenSurfaces::CheckAndReportDmaErrors(UINT32 Subdev)
{
    // Re-read back the current cached surface and compare with the copy
    // already in m_pCacheMem.
    // We should use a different HW mechanism than the original one, i.e.
    // CPU reads of BAR1 vs. a DMA operation, if possible.
    // Return OK if surfaces match, RC::MEM_TO_MEM_RESULT_NOT_MATCH otherwise.

    // @@@ TBD
    return OK;
}

//------------------------------------------------------------------------------
void * LwdaGoldenSurfaces::GetCachedAddress
(
    int SurfNum,
    Goldelwalues::BufferFetchHint /*bufFetchHint*/,   //!< Not used
    UINT32 SubdevNum,
    vector<UINT08> *surfDumpBuffer
)
{
    MASSERT(SurfNum >=0);
    m_LwrSurfNum = SurfNum;

    if (surfDumpBuffer)
    {
        UINT08 *pSurf = static_cast<UINT08*>(m_Surf[SurfNum].memAddr);
        UINT64 surfSize = (static_cast<UINT64>(m_Surf[SurfNum].pitch) *
                           m_Surf[SurfNum].height);
        surfDumpBuffer->reserve(surfSize);
        surfDumpBuffer->assign(pSurf, pSurf + surfSize);
    }

    return (void*)m_Surf[SurfNum].memAddr;
}

void LwdaGoldenSurfaces::Ilwalidate()
{
    m_LwrSurfNum = -1;
}
//------------------------------------------------------------------------------
INT32 LwdaGoldenSurfaces::Pitch(int SurfNum) const
{
    return m_Surf[SurfNum].pitch;
}

//------------------------------------------------------------------------------
UINT32 LwdaGoldenSurfaces::Width(int SurfNum) const
{
    return m_Surf[SurfNum].width;
}

//------------------------------------------------------------------------------
UINT32 LwdaGoldenSurfaces::Height(int SurfNum) const
{
    return m_Surf[SurfNum].height;
}

//------------------------------------------------------------------------------
ColorUtils::Format LwdaGoldenSurfaces::Format(int SurfNum) const
{
    // we don't use color formats in lwca. so just return ARGB8
    return ColorUtils::A8R8G8B8;
}

//------------------------------------------------------------------------------
UINT32 LwdaGoldenSurfaces::Display(int SurfNum) const
{
    return 0;
}

//------------------------------------------------------------------------------
void LwdaGoldenSurfaces::DescribeSurface
(
    UINT32 Width,
    UINT32 Height,
    UINT32 Pitch,
    void*  MemAddr,
    int    SurfNum,
    const char * Name
)
{
    m_Surf[SurfNum].width = Width;
    m_Surf[SurfNum].height = Height;
    m_Surf[SurfNum].pitch = Pitch;
    m_Surf[SurfNum].memAddr = MemAddr;
    m_Surf[SurfNum].name = Name;
}

void LwdaGoldenSurfaces::SetNumSurfaces(int NumSurf)
{
    m_Surf.resize(NumSurf);
}

void LwdaGoldenSurfaces::SetRC(int SurfNum, RC rc)
{
    m_Surf[SurfNum].rc = rc;
}

RC LwdaGoldenSurfaces::GetRC(int SurfNum)
{
    return m_Surf[SurfNum].rc;
}

