/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef PARSE_FILE_H
#define PARSE_FILE_H

#include <string.h>
#include <deque>
#include "mdiag/utils/types.h"
#include "core/include/massert.h"
#include "core/include/lwrm.h"

class Trace3DTest;
class ArgReader;
class TraceFileMgr;

//-----------------------------------------------------------------
//! \brief ParseFile works for parse test hdr file and PE configure
//! file. This class will read the file into the m_Buf and toknize
//! it via the space. Each line will append a s_NewLine as the end.
//!
class ParseFile
{
public:
    ParseFile(Trace3DTest * test, const ArgReader * reader);
    ~ParseFile();
    deque<const char *> & GetInlineBuf() { return m_InlineBuf; }
    RC ReadHeader(const char * fileName, UINT32 timeout);
    RC ReadCommandLine(const char * commands);
    RC ParseUINT32(UINT32 * pUINT32);
    RC ParseString(string * pstr);
    deque<const char *> & GetTokens() { return m_Tokens;}
private:
    void Tokenize();
    size_t m_BufSize;
    vector<char> m_Buf;
    deque<const char *> m_Tokens;
    const char * m_FileName;
    UINT32 m_TimeOutMs;
    Trace3DTest * m_Test;
    TraceFileMgr * m_TraceFileMgr;
    deque<const char *> m_InlineBuf;

    const ArgReader * params;
public:
    static const char * s_NewLine;
};
#endif
