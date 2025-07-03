/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLMEMMAPSEGMENT_H
#define INCLUDED_UTLMEMMAPSEGMENT_H

#include "mdiag/utils/mdiagsurf.h"

#include "utlpython.h"
#include "utlmmu.h"

// This class represents a memory-mapped segment. Segments are lwrrently
// limited to a constant size of 4k bytes.
//
class UtlRawMemory
{
public:
    static void Register(py::module module);

    UtlRawMemory() = delete;
    UtlRawMemory(UtlRawMemory&) = delete;
    UtlRawMemory& operator=(UtlRawMemory&) = delete;

    UtlRawMemory(LwRm *pLwRm, GpuDevice* pGpuDev);
    void SetPhysAddress(PHYSADDR physAddress);
    RC Map();
    RC Unmap();

    void MapPy();
    void UnmapPy();
    UINT64 GetPhysAddress();
    vector<UINT08> ReadData(py::kwargs kwargs);
    void WriteData(py::kwargs kwargs);
    UtlMmu::APERTURE GetAperture() const;
    void SetAperture(UtlMmu::APERTURE aperture);

private:
    unique_ptr<MDiagUtils::RawMemory> m_pRawMemory;
    UtlMmu::APERTURE m_Aperture = UtlMmu::APERTURE::APERTURE_ILWALID;
};

#endif
