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
#include <vector>

// Thin wrapper class around the OpenSSL implementation.  Ensures that connections
// only last for the lifetime of the object and abstracts the complexity of requiring
// a connection context from callers
class SslConnection
{
public:
    explicit SslConnection() { }
    ~SslConnection();
    LwDiagUtils::EC Connect(string host);
    INT32 Read(vector<char> *pResponse);
    INT32 Write(string str);

    static LwDiagUtils::EC InitializeOpenSsl();
    static void ShutdownOpenSsl();
    static bool IsOpenSslInitialized();
private:
    void *    m_pConContext = nullptr;
};

