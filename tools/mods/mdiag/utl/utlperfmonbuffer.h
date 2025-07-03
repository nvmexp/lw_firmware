/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLPERFMONBUFFER_H
#define INCLUDED_UTLPERFMONBUFFER_H

#include "mdiag/utils/perf_mon.h"
#include "utlpython.h"
#include "utlsurface.h"

// This class represents a Perfmon buffer from the point of view of a
// UTL script. 
// It is a wrapper for C++ class PerfmonBuffer in perf_mon.cpp and meanwhile 
// leverages the APIs of UtlSurface.
//
class UtlPerfmonBuffer : public UtlSurface
{
public:
    static void Register(py::module module);

    ~UtlPerfmonBuffer();

    // The following functions are called from the Python interpreter.
    static UtlPerfmonBuffer* Create(py::kwargs kwargs);
    void Bind();
    void Dump(py::kwargs kwargs);

    // Make sure the compiler doesn't implement default versions
    // of these constructors and assignment operators.  This is so we can
    // catch errors with incorrect references on the Python side.
    UtlPerfmonBuffer() = delete;
    UtlPerfmonBuffer(UtlPerfmonBuffer&) = delete;
    UtlPerfmonBuffer &operator=(UtlPerfmonBuffer&) = delete;
 
private:
    // UtlPerfmonBuffer objects should only be created by the static
    // Create function, so the constructor is private.
    UtlPerfmonBuffer(unique_ptr<PerfmonBuffer> pPerfmonBuffer, MdiagSurf* pMdiagSurf);
 
    unique_ptr<PerfmonBuffer> m_pPerfmonBuffer;
};

#endif
