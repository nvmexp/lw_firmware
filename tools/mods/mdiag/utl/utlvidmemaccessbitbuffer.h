/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLVIDMEMACCESSBITBUFFER_H
#define INCLUDED_UTLVIDMEMACCESSBITBUFFER_H

#include "utlpython.h"
#include "utlsurface.h"

// This class represents a Vidmem Access Bit buffer from the point of view of a
// UTL script.
//
class UtlVidmemAccessBitBuffer : public UtlSurface
{
public:
    static void Register(py::module module);

    // The following functions are called from the Python interpreter.
    static UtlVidmemAccessBitBuffer* Create(py::kwargs kwargs);
    void EnableLogging(py::kwargs kwargs);
    void DisableLogging();
    void Dump();
    UINT32 PutOffset();

    // Make sure the compiler doesn't implement default versions
    // of these constructors and assignment operators.  This is so we can
    // catch errors with incorrect references on the Python side.
    UtlVidmemAccessBitBuffer() = delete;
    UtlVidmemAccessBitBuffer(UtlVidmemAccessBitBuffer&) = delete;
    UtlVidmemAccessBitBuffer& operator=(UtlVidmemAccessBitBuffer&) = delete;
private:
    // UtlVidmemAccessBitBuffer objects should only be created by the static
    // Create function, so the constructor is private.
    UtlVidmemAccessBitBuffer(MdiagSurf* pMdiagSurf);

    virtual RC DoAllocate(MdiagSurf* pMdiagSurf, GpuDevice *pGpuDevice) override;
    virtual void DoSetSize(UINT64 size) override;
};

#endif
