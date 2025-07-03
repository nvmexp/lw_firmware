/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/onedgoldensurfaces.h"
#include "core/include/crc.h"
#include "core/include/mods_profile.h"

RC OnedGoldenSurfaces::SetSurface(const string &name, void *surfaceData, UINT64 surfaceSize)
{
    for (auto &surface : m_Surfaces)
    {
        if (surface.name == name)
        {
            surface.data = surfaceData;
            surface.dataSize = surfaceSize;
            return RC::OK;
        }
    }

    m_Surfaces.push_back({name, surfaceData, surfaceSize});

    return RC::OK;
}

int OnedGoldenSurfaces::NumSurfaces() const
{
    return static_cast<int>(m_Surfaces.size());
}

const string & OnedGoldenSurfaces::Name(int surfNum) const
{
    MASSERT(surfNum < NumSurfaces());

    return m_Surfaces[surfNum].name;
}

RC OnedGoldenSurfaces::CheckAndReportDmaErrors(UINT32 subdevNum)
{
    return RC::OK;
}

void *OnedGoldenSurfaces::GetCachedAddress
(
    int surfNum,
    Goldelwalues::BufferFetchHint bufFetchHint,
    UINT32 subdevNum,
    vector<UINT08> *surfDumpBuffer
)
{
    return nullptr;
}

void OnedGoldenSurfaces::Ilwalidate()
{
}

INT32 OnedGoldenSurfaces::Pitch(int surfNum) const
{
    return 0;
}

UINT32 OnedGoldenSurfaces::Width(int surfNum) const
{
    return 0;
}

UINT32 OnedGoldenSurfaces::Height(int surfNum) const
{
    return 0;
}

ColorUtils::Format OnedGoldenSurfaces::Format(int surfNum) const
{
    return ColorUtils::Y8;
}

UINT32 OnedGoldenSurfaces::Display(int surfNum) const
{
    return 0;
}

#if defined(__amd64) || defined(__i386)
__attribute__((target("sse4.2")))
#endif
RC OnedGoldenSurfaces::FetchAndCallwlate
(
    int surfNum,
    UINT32 subdevNum,
    Goldelwalues::BufferFetchHint bufFetchHint,
    UINT32 numCodeBins,
    Goldelwalues::Code calcMethod,
    UINT32 * pRtnCodeValues,
    vector<UINT08> *surfDumpBuffer
)
{
    if (numCodeBins != 1)
    {
        Printf(Tee::PriError, "Only one codebin is supported.\n");
        return RC::SOFTWARE_ERROR;
    }

    PROFILE(CRC);

    UINT32 crc = ~0U;
    UINT32 *data = reinterpret_cast<UINT32 *>(m_Surfaces[surfNum].data);
    const UINT64 dataSize = m_Surfaces[surfNum].dataSize;
    const UINT32 *dataEnd = data + dataSize/4;

    if (Crc32c::IsHwCrcSupported())
    {
        for (; data < dataEnd; data++)
        {
            crc = CRC32C4(crc, *data);
        }
        if (dataSize & 0x3)
        {
            const UINT32 numLastBytes = dataSize & 0x3;
            const UINT32 lastBytesMask = (1 << (8 * numLastBytes)) - 1;
            const UINT32 lastData = *data & lastBytesMask;
            crc = CRC32C4(crc, lastData);
        }
    }
    else
    {
        Crc32c::GenerateCrc32cTable();
        for (; data < dataEnd; data++)
        {
            crc = Crc32c::StepSw(crc, *data);
        }
        if (dataSize & 0x3)
        {
            const UINT32 numLastBytes = dataSize & 0x3;
            const UINT32 lastBytesMask = (1 << (8 * numLastBytes)) - 1;
            const UINT32 lastData = *data & lastBytesMask;
            crc = Crc32c::StepSw(crc, lastData);
        }
    }
    pRtnCodeValues[0] = crc;

    Printf(Tee::PriLow, "Surface %s crc = 0x%08x.\n",
        m_Surfaces[surfNum].name.c_str(), crc);

    return RC::OK;
}

RC OnedGoldenSurfaces::GetPitchAlignRequirement(UINT32 *pitch)
{
    *pitch = 0;
    return RC::OK;
}
