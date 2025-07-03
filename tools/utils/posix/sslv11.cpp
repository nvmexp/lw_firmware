/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "sslimpl.h"
#include "lwdiagutils.h"
#include "openssl/ssl.h"  // Headers from //sw/tools/mods/openssl-1.1.0f/include
#include "openssl/err.h"  // Headers from //sw/tools/mods/openssl-1.1.0f/include

#include <string>
#include <vector>
#include <memory>

// OpenSsl version 1.1 library interface
class OpenSslv11 : public OpenSsl
{
public:
    OpenSslv11() { }
    virtual ~OpenSslv11();

    LwDiagUtils::EC Connect(string host, void **ppvConContext) override;
    void Disconnect(void *pvConContext) override;
    LwDiagUtils::EC InitializeLibrary() override;
    INT32 Read(void *pvConContext, vector<char> * pResponse) override;
    INT32 Write(void *pvConContext, string str) override;
private:

    struct ConnectionContext
    {
        SSL_CTX* pCtx = nullptr;
        BIO*     pWeb = nullptr;
    };

    long          (*_BIO_ctrl)(BIO *, int, long, void *)                                   = nullptr; //$
    void          (*_BIO_free_all)(BIO *)                                                  = nullptr; //$
    BIO *         (*_BIO_new_ssl_connect)(SSL_CTX *)                                       = nullptr; //$
    int           (*_BIO_puts)(BIO *, const char *)                                        = nullptr; //$
    int           (*_BIO_read)(BIO *, void *, int)                                         = nullptr; //$
    int           (*_BIO_test_flags)(const BIO *, int)                                     = nullptr; //$

    int           (*_OPENSSL_init_ssl)(uint64_t, const OPENSSL_INIT_SETTINGS *)            = nullptr; //$

    long          (*_SSL_ctrl)(SSL *, int, long, void *)                                   = nullptr; //$
    long          (*_SSL_get_verify_result)(const SSL *)                                   = nullptr; //$
    int           (*_SSL_set_cipher_list)(SSL *, const char *)                             = nullptr; //$
    X509 *        (*_SSL_get_peer_certificate)(const SSL *)                                = nullptr; //$

    int           (*_SSL_CTX_load_verify_locations)(SSL_CTX *, const char *, const char *) = nullptr; //$
    SSL_CTX *     (*_SSL_CTX_new)(const SSL_METHOD *)                                      = nullptr; //$
    unsigned long (*_SSL_CTX_set_options)(SSL_CTX *, unsigned long)                        = nullptr; //$
    int           (*_SSL_CTX_set_default_verify_paths)(SSL_CTX *)                          = nullptr; //$
    void          (*_SSL_CTX_set_verify)(SSL_CTX *, int, SSL_verify_cb)                    = nullptr; //$
    void          (*_SSL_CTX_free)(SSL_CTX *)                                              = nullptr; //$

    SSL_METHOD *  (*_TLS_client_method)(void)                                              = nullptr; //$

    X509_NAME *   (*_X509_get_issuer_name)(const X509 *)                                   = nullptr; //$
    char *        (*_X509_NAME_oneline)(const X509_NAME *, char *, int)                    = nullptr; //$
    void          (*_X509_free)(X509 *)                                                    = nullptr; //$

    void          (*_CRYPTO_free)(void *, const char *, int)                               = nullptr; //$

    void * m_SslHandle           = nullptr;
};

// -----------------------------------------------------------------------------
OpenSslv11::~OpenSslv11()
{
    if (m_SslHandle)
    {
        LwDiagXp::UnloadDynamicLibrary(m_SslHandle);
        m_SslHandle = nullptr;
    }
}

// -----------------------------------------------------------------------------
// This implementation is loosely based on the TLS implementation from
// https://wiki.openssl.org/index.php/SSL/TLS_Client with some modifications
// required specific for lwpu use
LwDiagUtils::EC OpenSslv11::Connect(string host, void **ppvConContext)
{
    LWDASSERT(ppvConContext != nullptr);

    ConnectionContext * pConContext = new ConnectionContext;

    // Simple cleanup class to handle failing cases in connection
    class SslCleanup
    {
    public:
        SslCleanup(OpenSslv11 *pOpenSsl, ConnectionContext *pCon) :
            m_pOpenSsl(pOpenSsl), m_pCon(pCon) { }
        ~SslCleanup() { if (m_pCon) m_pOpenSsl->Disconnect(m_pCon); }
        void Cancel() { m_pCon = nullptr; }
    private:
        OpenSslv11 *        m_pOpenSsl = nullptr;
        ConnectionContext * m_pCon     = nullptr;
    } sslCleanup(this, pConContext);

    const SSL_METHOD * pMethod = _TLS_client_method();
    pConContext->pCtx = _SSL_CTX_new(pMethod);
    if (pConContext->pCtx == nullptr)
    {
        LwDiagUtils::NetworkPrintf(host, LwDiagUtils::PriWarn,
                                   "Failed to create SSL context\n");
        return LwDiagUtils::SOFTWARE_ERROR;
    }

    _SSL_CTX_set_options(pConContext->pCtx, SSL_OP_ALL);

    _SSL_CTX_set_verify(pConContext->pCtx, SSL_VERIFY_PEER, NULL);

    string certFile = LwDiagUtils::DefaultFindFile("HQLWCA121-CA.crt", true);
    if ((!_SSL_CTX_load_verify_locations(pConContext->pCtx, certFile.c_str(), nullptr)) ||
        (!_SSL_CTX_set_default_verify_paths(pConContext->pCtx)))
    {
        LwDiagUtils::NetworkPrintf(host, LwDiagUtils::PriWarn,
                               "Failed to setup SSL certificate, check that %s is present\n",
                               certFile.c_str());
        return LwDiagUtils::SOFTWARE_ERROR;
    }

    pConContext->pWeb = _BIO_new_ssl_connect(pConContext->pCtx);
    if (pConContext->pWeb == nullptr)
    {
        LwDiagUtils::NetworkPrintf(host, LwDiagUtils::PriWarn,
                                   "Failed to create SSL connection interface\n");
        return LwDiagUtils::SOFTWARE_ERROR;
    }

    string hostPort = host + ":443";

    CHECK_EC_SSL(_BIO_ctrl(pConContext->pWeb,
                           BIO_C_SET_CONNECT,
                           0,
                           static_cast<void *>(&hostPort[0])));

    SSL * pSsl = nullptr;
    CHECK_EC_SSL(_BIO_ctrl(pConContext->pWeb, BIO_C_GET_SSL, 0, (char *)&pSsl));
    if (pSsl == nullptr)
    {
        LwDiagUtils::NetworkPrintf(host, LwDiagUtils::PriWarn,
                                   "Failed to create SSL connection\n");
        return LwDiagUtils::SOFTWARE_ERROR;
    }

    CHECK_EC_SSL(_SSL_set_cipher_list(pSsl, "DEFAULT:@SECLEVEL=0"));

    CHECK_EC_SSL(_SSL_ctrl(pSsl,
                           SSL_CTRL_SET_TLSEXT_HOSTNAME,
                           TLSEXT_NAMETYPE_host_name,
                           static_cast<void*>(const_cast<char*>(host.c_str()))));

    // These 2 calls are not a mistake, it is required to call this twice in order
    // to establish a connection
    if ((_BIO_ctrl(pConContext->pWeb, BIO_C_DO_STATE_MACHINE, 0, NULL) != 1) ||
        (_BIO_ctrl(pConContext->pWeb, BIO_C_DO_STATE_MACHINE, 0, NULL) != 1))
    {
        LwDiagUtils::NetworkPrintf(host, LwDiagUtils::PriWarn,
                                   "Failed to establish SSL connection to %s\n",
                                   host.c_str());
        return LwDiagUtils::SOFTWARE_ERROR;
    }

    X509 * pX509 = _SSL_get_peer_certificate(pSsl);
    if (!pX509)
    {
        LwDiagUtils::NetworkPrintf(host, LwDiagUtils::PriWarn,
                                   "No server certificate received from %s\n",
                                   host.c_str());
        return LwDiagUtils::SOFTWARE_ERROR;
    }

    // This issuer pointer is malloc'd, it will be freed by the call to CRYPTO_free below
    char * issuer = _X509_NAME_oneline(_X509_get_issuer_name(pX509), NULL, 0);
    string issuerString = issuer;
    _CRYPTO_free(issuer, "", 0);
    if ((issuerString.find("DC=lwpu") == string::npos) ||
        (issuerString.find("CN=HQLWCA122-CA") == string::npos))
    {
        LwDiagUtils::NetworkPrintf(host, LwDiagUtils::PriWarn,
                                   "Invalid server issuer \"%s\"\n",
                                   issuerString.c_str());
        return LwDiagUtils::SOFTWARE_ERROR;
    }
    _X509_free(pX509);

    LwDiagUtils::NetworkPrintf(host, LwDiagUtils::PriLow,
                               "SSL connection established, verifiying certificate\n");

    if (X509_V_OK != _SSL_get_verify_result(pSsl))
    {
        LwDiagUtils::NetworkPrintf(host, LwDiagUtils::PriWarn,
                                   "Failed to validate X509 certificate\n");
        return LwDiagUtils::SOFTWARE_ERROR;
    }

    LwDiagUtils::NetworkPrintf(host, LwDiagUtils::PriLow,
                               "X509 certificate verified, exelwting on the lwpu network\n");

    // Cancel the cleanup, the context pointer created earlier is considered valid
    // and will be used as a handle for this connection
    sslCleanup.Cancel();

    *ppvConContext = static_cast<void *>(pConContext);
    return LwDiagUtils::OK;
}

// -----------------------------------------------------------------------------
LwDiagUtils::EC OpenSslv11::InitializeLibrary()
{
    static vector<string> versionSuffixes =
    {
        ".1.1",
    };

    for (auto const & lwrVerSuf : versionSuffixes)
    {
        const string libString = "libssl" + LwDiagXp::GetDynamicLibrarySuffix() + lwrVerSuf;
        if (LwDiagUtils::OK == LwDiagXp::LoadDynamicLibrary(libString, &m_SslHandle))
            break;
    }

    if (m_SslHandle == nullptr)
    {
        LwDiagUtils::NetworkPrintf(LwDiagUtils::PriWarn,
                                   "OpenSSL library v1.1 is not installed\n");
        return LwDiagUtils::DLL_LOAD_FAILED;
    }

    LwDiagUtils::EC ec = LwDiagUtils::OK;

    CHECK_EC(GetLibFunction(m_SslHandle, &_BIO_ctrl, "BIO_ctrl"));
    CHECK_EC(GetLibFunction(m_SslHandle, &_BIO_free_all, "BIO_free_all"));
    CHECK_EC(GetLibFunction(m_SslHandle, &_BIO_new_ssl_connect, "BIO_new_ssl_connect"));
    CHECK_EC(GetLibFunction(m_SslHandle, &_BIO_puts, "BIO_puts"));
    CHECK_EC(GetLibFunction(m_SslHandle, &_BIO_read, "BIO_read"));
    CHECK_EC(GetLibFunction(m_SslHandle, &_BIO_test_flags, "BIO_test_flags"));

    CHECK_EC(GetLibFunction(m_SslHandle, &_OPENSSL_init_ssl, "OPENSSL_init_ssl"));

    CHECK_EC(GetLibFunction(m_SslHandle, &_SSL_ctrl, "SSL_ctrl"));
    CHECK_EC(GetLibFunction(m_SslHandle, &_SSL_get_verify_result, "SSL_get_verify_result"));
    CHECK_EC(GetLibFunction(m_SslHandle, &_SSL_set_cipher_list, "SSL_set_cipher_list"));
    CHECK_EC(GetLibFunction(m_SslHandle, &_SSL_get_peer_certificate, "SSL_get_peer_certificate"));

    CHECK_EC(GetLibFunction(m_SslHandle, &_SSL_CTX_free, "SSL_CTX_free"));
    CHECK_EC(GetLibFunction(m_SslHandle, &_SSL_CTX_load_verify_locations, "SSL_CTX_load_verify_locations")); //$
    CHECK_EC(GetLibFunction(m_SslHandle, &_SSL_CTX_new, "SSL_CTX_new"));
    CHECK_EC(GetLibFunction(m_SslHandle, &_SSL_CTX_set_options, "SSL_CTX_set_options"));
    CHECK_EC(GetLibFunction(m_SslHandle, &_SSL_CTX_set_default_verify_paths, "SSL_CTX_set_default_verify_paths")); //$
    CHECK_EC(GetLibFunction(m_SslHandle, &_SSL_CTX_set_verify, "SSL_CTX_set_verify"));

    CHECK_EC(GetLibFunction(m_SslHandle, &_TLS_client_method, "TLS_client_method"));

    CHECK_EC(GetLibFunction(m_SslHandle, &_X509_get_issuer_name, "X509_get_issuer_name"));
    CHECK_EC(GetLibFunction(m_SslHandle, &_X509_NAME_oneline, "X509_NAME_oneline"));
    CHECK_EC(GetLibFunction(m_SslHandle, &_X509_free, "X509_free"));

    CHECK_EC(GetLibFunction(m_SslHandle, &_CRYPTO_free, "CRYPTO_free"));

    _OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
    _OPENSSL_init_ssl(0, nullptr);

    return ec;
}

// -----------------------------------------------------------------------------
INT32 OpenSslv11::Read(void *pvConContext, vector<char> * pResponse)
{
    LWDASSERT(pvConContext);
    ConnectionContext * pConContext = static_cast<ConnectionContext *>(pvConContext);

    pResponse->clear();

    int len = 0;
    do
    {
        char buff[512] = {};
        len = _BIO_read(pConContext->pWeb, buff, sizeof(buff));
        if (len > 0)
        {
            LWDASSERT(len <= static_cast<int>(sizeof(buff)));
            pResponse->insert(pResponse->end(), buff, buff + len);
        }
    } while (len > 0 || _BIO_test_flags(pConContext->pWeb, BIO_FLAGS_SHOULD_RETRY));

    return pResponse->empty() ? len : static_cast<INT32>(pResponse->size());
}

// -----------------------------------------------------------------------------
INT32 OpenSslv11::Write(void *pvConContext, string str)
{
    LWDASSERT(pvConContext);
    ConnectionContext * pConContext = static_cast<ConnectionContext *>(pvConContext);
    return _BIO_puts(pConContext->pWeb, str.c_str());
}

// -----------------------------------------------------------------------------
void OpenSslv11::Disconnect(void *pvConContext)
{
    LWDASSERT(pvConContext);
    ConnectionContext * pConContext = static_cast<ConnectionContext *>(pvConContext);

    if (pConContext->pWeb)
    {
        _BIO_free_all(pConContext->pWeb);
    }
    if (pConContext->pCtx)
    {
        _SSL_CTX_free(pConContext->pCtx);
    }

    delete pConContext;
}

// -----------------------------------------------------------------------------
OpenSsl * CreateOpenSslv11()
{
    OpenSsl * pOpenSslv11 = new OpenSslv11;

    if (LwDiagUtils::OK == pOpenSslv11->InitializeLibrary())
        return pOpenSslv11;
    delete pOpenSslv11;
    return nullptr;
}
