/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "utlpython.h"
#include "utlevent.h"

/*
 * This file helps isolate PYBIND11_MAKE_OPAQUE(std::vector<xxx>) scope
 * Others event out of this scope will not be influenced
 * 
 */

class UtlVectorEvent : public BaseUtlEvent
{
public:
    static void Register(py::module module);
};

class UtlTestParseEvent :
    public UtlEvent<UtlTestParseEvent,
                    UtlCallbackFilter::Test<UtlTestParseEvent>>
{
public:
    UtlTestParseEvent(UtlTest* test, vector<char> *buf);
    static string GetClassName() { return "TestParseEvent"; }
    
    UtlTest* m_Test = nullptr;
    vector<char> * m_pBuf = nullptr;
};

