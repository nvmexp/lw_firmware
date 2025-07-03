/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/golden.h"

class OnedGoldenSurfaces : public GoldenSurfaces
{
public:

    RC SetSurface
    (
        const string &name,
        void *surfaceData,
        UINT64 surfaceSize
    );

    int NumSurfaces() const override;
    const string & Name(int surfNum) const override;
    RC CheckAndReportDmaErrors(UINT32 subdevNum) override;
    void *GetCachedAddress
    (
        int surfNum,
        Goldelwalues::BufferFetchHint bufFetchHint,
        UINT32 subdevNum,
        vector<UINT08> *surfDumpBuffer
    ) override;
    void Ilwalidate() override;
    INT32 Pitch(int surfNum) const override;
    UINT32 Width(int surfNum) const override;
    UINT32 Height(int surfNum) const override;
    ColorUtils::Format Format(int surfNum) const override;
    UINT32 Display(int surfNum) const override;
    RC FetchAndCallwlate
    (
        int surfNum,
        UINT32 subdevNum,
        Goldelwalues::BufferFetchHint bufFetchHint,
        UINT32 numCodeBins,
        Goldelwalues::Code calcMethod,
        UINT32 * pRtnCodeValues,
        vector<UINT08> *surfDumpBuffer
    ) override;
    RC GetPitchAlignRequirement(UINT32 *pitch) override;

private:
    struct SurfaceDescription
    {
        string name;
        void *data;
        UINT64 dataSize;
    };

    vector<SurfaceDescription> m_Surfaces;
};
