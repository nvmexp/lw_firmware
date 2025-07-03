/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// MODS remote (network) user interface implementation
//------------------------------------------------------------------------------
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include "core/include/remotui.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/tee.h"
#include "runmacro.h"
#include "core/include/datasink.h"
// DEBUG code: #include <windows.h>

// gExit defined in simpleui.cpp
extern bool gExit;

/**
 *------------------------------------------------------------------------------
 * @function(RemoteUserInterface::RemoteUserInterface)
 *
 * ctor
 *------------------------------------------------------------------------------
 */

RemoteUserInterface::RemoteUserInterface
(
    SocketMods * s,
    bool     useMessageInterface /* =false */
) :
    m_Socket(s),
    m_UseMessageInterface(useMessageInterface)
{
}

/**
 *------------------------------------------------------------------------------
 * @function(RemoteUserInterface::~RemoteUserInterface)
 *
 * dtor
 *------------------------------------------------------------------------------
 */

RemoteUserInterface::~RemoteUserInterface()
{
}

/**
 *------------------------------------------------------------------------------
 * @function(RemoteUserInterface::Run)
 *
 * Run the user interface.
 *------------------------------------------------------------------------------
 */
struct CdmMsgHeader
{
    UINT08 Magic0;
    UINT08 Magic1;
    UINT08 MsgLenLsb;
    UINT08 MsgLenMsb;
};

void RemoteUserInterface::Run()
{
    UINT32      cmdBufSize;
    const char* prompt      = nullptr;
    size_t      promptLen   = 0;
    DataSink*   pRemoteSink = Tee::GetScreenSink();

    // DEBUG code: DebugBreak();
    if (m_UseMessageInterface)
    {
        // New interface (ilwented for HP CDM remote win32 gui over named pipe):
        //  - Command messages may span multiple socket reads, and are of any size.
        //  - The remote controller is another program, (cdmlwidiadiag.dll)
        // and has to be told when we are ready for the next command.
        //
        // The structure of a command message is:
        //   byte 0: magic 0xd1
        //   byte 1: magic 0xa6
        //   byte 2: message length LSB (not counting 4-byte header)
        //   byte 3: message length MSB (not counting 4-byte header)
        //   byte 4 .. n+3: message itself

        cmdBufSize = 256;  // will grow as needed

        // The newline is required to flush RemoteDataSink!
        static const char ready[] = "\n&&Ready&&\n";
        prompt    = ready;
        promptLen = sizeof(ready) - 1;
    }
    else
    {
        // Old interface (ilwented for xbox mods with remote TCPIP gui in TCL):
        //  - Command messages must never span two socket reads.
        //  - All commands fit in 1024 bytes.
        //  - The user knows when the test is done, (they are watching the output)
        // so there is no need to let the remote end know when we are ready for
        // the next command.
        //
        // Text comes into the script with a 3-character prefix.
        // "S: " means the rest of the line should be in script mode,
        // "M: " means the rest should be processed as a macro.

        cmdBufSize = 1024;
    }

    // Initial buffer alloc.
    char * cmdBuf = new char [cmdBufSize];

    // Loop until Exit() is called:
    JavaScriptPtr pJavaScript;
    while (!gExit)
    {
        bool isScript = true;  // else "macro"
        string strCmd;

        // Tell the remote UI we are ready for the next command message.
        if (promptLen)
        {
            pRemoteSink->Print(prompt, promptLen);
        }

        if (m_UseMessageInterface)
        {
            // Read next command using CDM style known-length command messages.
            UINT32 totalCount = 0;
            UINT32 count;

            // Read message header.
            while (totalCount < 4)
            {
                if (m_Socket->Read(cmdBuf + totalCount, 4-totalCount, &count) != OK)
                   break; // pipe error

                totalCount += count;
            }

            // Check message header and make sure we have enough buffer space.
            CdmMsgHeader * pHdr = (CdmMsgHeader *)cmdBuf;
            if ((pHdr->Magic0 != 0xd1) || (pHdr->Magic1 != 0xa6))
                break;

            UINT32 cmdLen = pHdr->MsgLenLsb + 256 * pHdr->MsgLenMsb;
            if (cmdLen >= cmdBufSize)
            {
                delete [] cmdBuf;
                cmdBufSize = cmdLen + 1;
                cmdBuf = new char [cmdBufSize];
            }

            // Read message.
            totalCount = 0;
            while (totalCount < cmdLen)
            {
                if (m_Socket->Read(cmdBuf + totalCount, cmdLen-totalCount, &count) != OK)
                   break; // pipe error

                totalCount += count;
            }
            if (totalCount != cmdLen)
                break; // pipe error

            // Colwert to string.
            cmdBuf[cmdLen] = '\0';
            strCmd = cmdBuf;
        }
        else
        {
            // Read next command using xbox-mods style command messages.
            UINT32 cmdLen;
            if ((m_Socket->Read(cmdBuf, cmdBufSize, &cmdLen) != OK) || (cmdLen < 3))
                break;

            if (cmdBuf[0] == 'S')
                isScript = true;
            else
                isScript = false;

            cmdBuf[cmdLen] = '\0';
            strCmd = &cmdBuf[3];
        }

        // Run the command.
        if (isScript)
            pJavaScript->RunScript( strCmd );
        else
            RunMacro( strCmd );

    } // while (!gExit)

    delete [] cmdBuf;
}

