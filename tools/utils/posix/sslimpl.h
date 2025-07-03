/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "lwdiagutils.h"
#include <string>
#include <vector>
#include <memory>

// Return software error because we don't want to give away that we're doing network stuff.
#define CHECK_EC_SSL(f)                                              \
    do {                                                             \
        if (1 != (f))                                                \
        {                                                            \
            LwDiagUtils::NetworkPrintf(host, LwDiagUtils::PriWarn,   \
                "OpenSSL call failed " #f "\n");                     \
            return LwDiagUtils::SOFTWARE_ERROR;                      \
        }                                                            \
    } while (0)

// Abstract OpenSsl class so that multiple versions of OpenSsl can be supported
//
// This interface should only available to the SslConnection class which manages the
// need for a context and enforces closing of connections
class OpenSsl
{
public:
    virtual ~OpenSsl() { }
    virtual LwDiagUtils::EC Connect(string host, void **ppvConContext) = 0;
    virtual void Disconnect(void *pvConContext) = 0;
    virtual LwDiagUtils::EC InitializeLibrary() = 0;
    virtual INT32 Read(void *pvConContext, vector<char> * pResponse) = 0;
    virtual INT32 Write(void *pvConContext, string str) = 0;

protected:
    template <typename RetType>
    static LwDiagUtils::EC GetLibFunction
    (
        void* libHandle,
        RetType** ppFunction,
        const char* functionName
    )
    {
        *ppFunction =
            reinterpret_cast<RetType*>(LwDiagXp::GetDynamicLibraryProc(libHandle, functionName));
        if (!*ppFunction)
        {
            LwDiagUtils::NetworkPrintf(LwDiagUtils::PriWarn,
                                       "Failed to load SSL library function %s, ensure OpenSSL "
                                       "v1.0 or v1.1 is installed\n",
                                       functionName);
            return LwDiagUtils::DLL_LOAD_FAILED;
        }
        return LwDiagUtils::OK;
    }
};

OpenSsl * CreateOpenSslv11();
OpenSsl * CreateOpenSslv10();
