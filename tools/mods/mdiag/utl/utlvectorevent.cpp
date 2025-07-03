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


#include "utlvectorevent.h"
#include "utlkwargsmgr.h"
PYBIND11_MAKE_OPAQUE(std::vector<char>);

void UtlVectorEvent::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    py::bind_vector<vector<char>>(module, "VectorStr");
    UTL_BIND_EVENT(TestParseEvent, "This class represents the event that triggers when test is parsing.");
    TestParseEvent.def_readonly("test", &UtlTestParseEvent::m_Test,
        "Test to be parsed");
    TestParseEvent.def_readwrite("data", &UtlTestParseEvent::m_pBuf,
        "Return the hdr content in list of char format", 
        py::return_value_policy::reference);
}

UtlTestParseEvent::UtlTestParseEvent
(
    UtlTest * pTest,
    vector<char> *buf
) :
    UtlEvent(),
    m_Test(pTest),
    m_pBuf(buf)
{
    MASSERT(m_Test != nullptr);
    MASSERT(m_pBuf != nullptr);
}
