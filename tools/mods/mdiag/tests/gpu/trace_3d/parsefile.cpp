/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2016, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "parsefile.h"
#include "trace_3d.h"
#include "mdiag/utils/tracefil.h"
#include "stdlib.h"
#include "errno.h"
#include "mdiag/utl/utl.h"
const char * ParseFile::s_NewLine = "\n";

ParseFile::ParseFile
(
    Trace3DTest * test,
    const ArgReader * reader
) :
    m_Test(test)
{
    m_TraceFileMgr = m_Test->GetTraceFileMgr();
    params = reader;
}

ParseFile::~ParseFile()
{
    m_Tokens.clear();
    m_Buf.clear();
    m_InlineBuf.clear(); 
}

RC ParseFile::ReadHeader(const char * fileName, UINT32 timeoutMs)
{
    RC rc = OK;
    TraceFileMgr::Handle hFile = NULL;

    // Open the test.hdr file (prefer to find it in the directory).

    m_TraceFileMgr->SearchArchiveFirst(false);
    CHECK_RC( m_TraceFileMgr->Open(fileName, &hFile) );

    // From here on out, prefer to find other files in the tar archive,
    // if one is present.
    m_TraceFileMgr->SearchArchiveFirst(true);

    if (params->ParamPresent("-file_search_order"))
    {
        if (params->ParamUnsigned("-file_search_order") == 0)
        {
            m_TraceFileMgr->SearchArchiveFirst(true);
        }
        else
        {
            m_TraceFileMgr->SearchArchiveFirst(false);
        }
    }

    // Alloc a buffer to hold the test.hdr data, which will become the
    // storage for a zillion string "tokens".

    m_BufSize = m_TraceFileMgr->Size(hFile);
    m_Buf.resize(m_BufSize + 1);

    // This block ends at the Cleanup label (error handling).
    {
        // Read the file into a big char buffer.
        CHECK_RC_CLEANUP(m_TraceFileMgr->Read(&m_Buf[0], hFile));

        m_Buf[m_BufSize] = '\0'; // Null-terminate last token, in case of missing \n.
        
        if (Utl::HasInstance())
        {
            Utl::Instance()->TriggerTestParseEvent(m_Test, &m_Buf);
            // UTL make sure the last token is "\n" and append "\0" to unify the flow
            m_BufSize = m_Buf.size() - 1;
        }
        
        // UTL has a lib to help skip the comment. 
        // Details see /hw/lwgpu/diag/testgen/config/utl/lib/mods/hdrparser.py
        // Please update the script also when the Tokenize has some updates
        Tokenize();
    }
    Cleanup:

    // Don't need tokenizer stuff or the test.hdr anymore.
    if (hFile)
        m_TraceFileMgr->Close(hFile);

    return rc;
}

RC ParseFile::ReadCommandLine(const char * commands)
{
    RC rc;

    if (commands == nullptr)
        return RC::ILWALID_INPUT;

    // Prepare for parsing
    size_t newlen = strlen(commands);
    m_Buf.resize(newlen + 1);

    if (m_Buf.empty())
        return RC::BUFFER_ALLOCATION_ERROR;

    m_BufSize = newlen;
    m_Buf.assign(commands, commands + newlen);

    // Real parsing begins
    Tokenize();

    return rc;
}

void ParseFile::Tokenize()
{
    bool in_token = false;
    bool in_comment = false;
    bool in_inlineUTL = false;
    bool in_UTLBegin = false;
    bool in_quotes = false;
    bool in_inline_quotes = false;

    char * p = &m_Buf[0];
    char * pEnd = &m_Buf[0] + m_BufSize;
    for (/**/; p < pEnd; p++)
    {
        if (isspace(*p))
        {
            if ('\n' == *p)
            {
                // Finish comment.
                in_comment = false;
                *p = '\0';
                
                if ((!in_inlineUTL && !m_Tokens.empty() && 
                        strcmp(m_Tokens.back(), "UTL_RUN_BEGIN") == 0) ||
                        in_UTLBegin)
                {
                    in_inlineUTL = true;
                    in_UTLBegin = false;
                    in_token = true;
                    m_Tokens.push_back(s_NewLine);
                    m_InlineBuf.push_back(p + 1);
                }
                else if (in_inlineUTL &&
                    strcmp(m_InlineBuf.back(), "UTL_RUN_END") == 0)
                {
                    in_inlineUTL = false;
                    in_token = false;
                    m_InlineBuf.push_back(s_NewLine);
                }
                else
                {
                    if (in_inlineUTL)
                    {
                        m_InlineBuf.push_back(s_NewLine);
                        m_InlineBuf.push_back(p + 1);
                    }
                    else
                    {
                        // Since we are overwriting all whitespace with '\0' to
                        // terminate the real tokens, we push an external string to
                        // represent the newline.
                        // This also allows the parser to check for newline tokens with
                        // just a pointer compare rather than having to use strcmp.
                        m_Tokens.push_back(s_NewLine);
                        // Finish previous token.
                        in_token = false;
                    }
                }
            }
            else
            {
                // \t and space
                if (in_quotes)
                {
                    continue;
                }
                // Finish previous token
                *p = '\0';
                if (in_inlineUTL)
                {
                    m_InlineBuf.push_back(" ");
                    m_InlineBuf.push_back(p + 1);
                }
                else if (!m_Tokens.empty() && 
                        strcmp(m_Tokens.back(), "UTL_RUN_BEGIN") == 0)
                {
                    in_UTLBegin = true;
                    in_token = false;
                }
                else
                {
                    // Finish previous token.
                    in_token = false;
                }
            }
        }
        else if (!in_comment)
        {
            if ('#' == *p)
            {
                // Begin a comment, finish previous token.
                in_comment = true;
                in_token = false;
                *p = '\0';
            }
            else if ('"' == *p)
            {
                // distinguish two cases
                // 1.   quotes inside tokenUTL_SCRIPT surface(name="XXX") 
                // 2.   quotes around the token "UTL_SCRIPT surface(name = 'XXX')". 
                // case 1 is regared as inlinequotes which quotes need to be keep
                // case 2 which quotes need to be dropped
                if (in_quotes == false)
                {
                    if (in_token)
                    {
                        // The way to distingush inlinequotes is how first quotes comes from
                        // 1. comes from previous token finished(in_token = false) isn't inline quotes
                        // 2. comes from in_token is inline quotes
                        in_inline_quotes = true;
                    }
                    // fisrt quotes do nothing
                    // 1. inline quotes p still move to next
                    // 2. non-inline quotes skip this p and push back p next
                }
                else
                {   
                    if(in_inline_quotes)
                    {
                        // 2nd quotes for inline do nothing, still move to next
                        // recover to false
                        in_inline_quotes = false;
                    }
                    else
                    {
                        // 2nd quotes for non-inline quotes
                        // 1. need to finished previous
                        // 2. make the p next have the ability to be pushed back
                        *p = '\0';
                        in_token = false;
                    }
                }

                in_quotes = !in_quotes;
            }
            else if (!in_token)
            {
                if (!in_inlineUTL)
                {
                    // Begin token.
                    in_token = true;
                    m_Tokens.push_back(p);
                }
            }
            
            // Else continue current token.
        }
        // Else continue current comment.
    }

    // Add missing \n to last statement if necessary.
    if (s_NewLine != m_Tokens.back())
        m_Tokens.push_back(s_NewLine);
}
