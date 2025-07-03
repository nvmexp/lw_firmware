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

#include "sslconnection.h"
#include "sslimpl.h"
#include <vector>

namespace
{
    // Needs to be a base type (not a unique_ptr) specifically so that it doesnt
    // get destroyed by the static destruction process.  Shutdown is called at the
    // very end of static destruction and if the queue gets destroyed before the
    // flush in Shutdown (because it is a non POD type) the exelwtable will either
    // segfault or prints will be lost
    OpenSsl * g_OpenSsl = nullptr;
}

// -----------------------------------------------------------------------------
SslConnection::~SslConnection()
{
    if (g_OpenSsl && m_pConContext)
        g_OpenSsl->Disconnect(m_pConContext);
    m_pConContext = nullptr;
}

// -----------------------------------------------------------------------------
LwDiagUtils::EC SslConnection::Connect(string host)
{
    if (m_pConContext)
        return LwDiagUtils::NETWORK_ALREADY_CONNECTED;

    LWDASSERT(g_OpenSsl);
    return g_OpenSsl->Connect(host, &m_pConContext);
}

// -----------------------------------------------------------------------------
INT32 SslConnection::Read(vector<char> *pResponse)
{
    if (!m_pConContext)
    {
        LwDiagUtils::NetworkPrintf(LwDiagUtils::PriWarn, "OpenSSL connection not established\n");
        return -1;
    }
    return g_OpenSsl->Read(m_pConContext, pResponse);
}

// -----------------------------------------------------------------------------
INT32 SslConnection::Write(string str)
{
    if (!m_pConContext)
    {
        LwDiagUtils::NetworkPrintf(LwDiagUtils::PriWarn, "OpenSSL connection not established\n");
        return -1;
    }
    return g_OpenSsl->Write(m_pConContext, str);
}

// -----------------------------------------------------------------------------
/* static */ LwDiagUtils::EC SslConnection::InitializeOpenSsl()
{
    if (g_OpenSsl)
    {
        return LwDiagUtils::OK;
    }

    g_OpenSsl = CreateOpenSslv11();
    if (!g_OpenSsl)
    {
        g_OpenSsl = CreateOpenSslv10();
        if (!g_OpenSsl)
        {
            LwDiagUtils::NetworkPrintf(LwDiagUtils::PriWarn,
                                       "OpenSSL library is not installed\n");
            return LwDiagUtils::DLL_LOAD_FAILED;
        }
        else
        {
            LwDiagUtils::NetworkPrintf(LwDiagUtils::PriNormal,
                                       "Authenticating with OpenSSL v1.0\n");
        }
    }
    else
    {
        LwDiagUtils::NetworkPrintf(LwDiagUtils::PriNormal, "Authenticating with OpenSSL v1.1\n");
    }
    return LwDiagUtils::OK;
}

// -----------------------------------------------------------------------------
/* static */ void SslConnection::ShutdownOpenSsl()
{
    if (g_OpenSsl)
    {
        delete g_OpenSsl;
        g_OpenSsl = nullptr;
    }
}

// -----------------------------------------------------------------------------
/* static */ bool SslConnection::IsOpenSslInitialized()
{
    return g_OpenSsl != nullptr;
}
