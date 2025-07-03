/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwdiagutils.h"
#include "sslconnection.h"

#include <cstring>
#include <algorithm>

LwDiagUtils::EC LwDiagXp::LoadLibSsl()
{
    return SslConnection::InitializeOpenSsl();
}

void LwDiagXp::UnloadLibSsl()
{
    SslConnection::ShutdownOpenSsl();
}

//! \brief Return true if running on the lwpu intranet
//!
bool LwDiagXp::IsOnLwidiaIntranet(const string& host)
{
    // For security this function should print any errors if something fails in order
    // to avoid giving lwstomers clues that something with respect to SSL may affect
    // exelwtion

    if (!SslConnection::IsOpenSslInitialized())
        return false;

    SslConnection sslCon;

    if (LwDiagUtils::OK != sslCon.Connect(host))
        return false;

    return true;
}

// -----------------------------------------------------------------------------
LwDiagUtils::EC LwDiagXp::ReadLwidiaServerFile
(
    const string& host,
    const string& name,
    vector<char>* pData
)
{
    LWDASSERT(pData != nullptr);
    pData->clear();

    LwDiagUtils::EC ec = LwDiagUtils::OK;

    if (!SslConnection::IsOpenSslInitialized())
        return LwDiagUtils::NETWORK_READ_ERROR;

    SslConnection sslCon;
    CHECK_EC(sslCon.Connect(host));

    string putString = "GET " + name + " HTTP/1.1\r\n" +
                       "Host: " + host + "\r\n" +
                       "Connection: close\r\n\r\n";
    if (static_cast<INT32>(putString.size()) != sslCon.Write(putString.c_str()))
        return LwDiagUtils::NETWORK_WRITE_ERROR;

    vector<char> response;
    if (0 >= sslCon.Read(&response))
        return LwDiagUtils::NETWORK_READ_ERROR;

    const char* closeStr = "Connection: close\r\n\r\n";
    auto pos = std::search(response.begin(), response.end(), closeStr, closeStr+strlen(closeStr));
    const char* okStr = "HTTP/1.1 200 OK";
    if (response.size() < strlen(okStr) ||
        memcmp(&(response[0]), okStr, strlen(okStr)) != 0 ||
        pos == response.end())
    {
        LwDiagUtils::NetworkPrintf(host, LwDiagUtils::PriWarn,
                                   "Failed to read %s from host through OpenSSL\n", name.c_str());
        return LwDiagUtils::NETWORK_READ_ERROR;
    }

    LwDiagUtils::NetworkPrintf(host, LwDiagUtils::PriLow,
                               "Successfully read %s from host through OpenSSL\n", name.c_str());

    pData->assign(pos + strlen(closeStr), response.end());

    return LwDiagUtils::OK;
}
