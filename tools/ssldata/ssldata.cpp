/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "lwdiagutils.h"
#include "win32/sockwin.h"

#include <memory>

#include <wincrypt.h>
#include <tchar.h>
#define SELWRITY_WIN32
#include <security.h>
#include <schnlsp.h>

#define AUTH_SERVER "cftt.lwpu.com"
#define AUTH_PORT   443
#define AUTH_SERVER_PORT AUTH_SERVER ":443"

namespace
{
    const DWORD s_SSPIFlags = ISC_REQ_SEQUENCE_DETECT |
                              ISC_REQ_REPLAY_DETECT |
                              ISC_REQ_CONFIDENTIALITY |
                              ISC_RET_EXTENDED_ERROR |
                              ISC_REQ_ALLOCATE_MEMORY |
                              ISC_REQ_STREAM |
                              ISC_REQ_MANUAL_CRED_VALIDATION;

    class SelwreConnectionOnExit
    {
        public:
            SelwreConnectionOnExit(unique_ptr<SocketWin32> &&sock) : m_pSock(move(sock)) { }
            ~SelwreConnectionOnExit()
            {
                if (m_phSelwrityContext)
                    DeleteSelwrityContext(m_phSelwrityContext);
                if (m_phCred)
                    FreeCredentialsHandle(m_phCred);
                if (m_pSock)
                    m_pSock->Close();
            }
            void SetCredHandle(PCredHandle phCred) { m_phCred = phCred; };
            void SetSelwrityContext(PCtxtHandle phSelwrityContext)
            {
                m_phSelwrityContext = phSelwrityContext;
            }
        private:
            unique_ptr<SocketWin32> m_pSock;
            PCredHandle             m_phCred            = nullptr;
            PCtxtHandle             m_phSelwrityContext = nullptr;
    };

    const UINT32 SOCKET_BUFFER_SIZE = 0x1000;
}

void PrintData(DWORD dataCount, BYTE *pbData)
{
    const string commaSpace = ", ";
    const string comma      = ",";
    const string empty      = "";

    LwDiagUtils::Printf(LwDiagUtils::PriNormal, "{");
    for (DWORD lwrIdx = 0; lwrIdx < dataCount; lwrIdx++)
    {
        if ((lwrIdx % 16) == 0)
            LwDiagUtils::Printf(LwDiagUtils::PriNormal, "\n    ");

        const string &sepStr = (lwrIdx == (dataCount - 1)) ?
            empty : (((lwrIdx % 16) != 15) ? commaSpace : comma);
        LwDiagUtils::Printf(LwDiagUtils::PriNormal, "0x%02x%s", pbData[lwrIdx], sepStr.c_str());
    }
    LwDiagUtils::Printf(LwDiagUtils::PriNormal, "\n}\n");
}

int main(int argc, char **argv)
{
    unique_ptr<SocketWin32> pNewSocket = make_unique<SocketWin32>();
    SocketWin32 *pCfttSocket = pNewSocket.get();

    // Create an RAII class for shutting things down when this exits
    SelwreConnectionOnExit secExit(move(pNewSocket));

    UINT32 addr = pCfttSocket->DoDnsLookup(AUTH_SERVER);
    if (addr == 0)
    {
        LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                            "Unable to resolve address for " AUTH_SERVER "\n");
        return -1;
    }
    if (LwDiagUtils::OK != pCfttSocket->Connect(addr, AUTH_PORT))
    {
        LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                            "Unable to connect to " AUTH_SERVER ":%u\n", AUTH_PORT);
        return -1;
    }

    CredHandle hCred = { };
    SCHANNEL_CRED schannelCred = { };
    schannelCred.dwVersion = SCHANNEL_CRED_VERSION;
    schannelCred.dwFlags   = SCH_CRED_NO_DEFAULT_CREDS |
                             SCH_CRED_NO_SYSTEM_MAPPER |
                             SCH_CRED_REVOCATION_CHECK_CHAIN;

    // Acquire a credentials handle to initiate the secure connection.  This uses
    // the windows credential store for the current user
    SELWRITY_STATUS ss = AcquireCredentialsHandle(0,
                                                  SCHANNEL_NAME,
                                                  SECPKG_CRED_OUTBOUND,
                                                  0,
                                                  &schannelCred,
                                                  0,
                                                  0,
                                                  &hCred,
                                                  0);

    if (SEC_E_OK != ss)
    {
        LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                            "Failed to acquire credentials handle\n");
        return -1;
    }

    // Successfully acquired a credentials handle, add it to the RAII class so
    // that it will be freed on exit
    secExit.SetCredHandle(&hCred);

    SecBufferDesc secBufferDescOut   = { };
    SecBufferDesc secBufferDescIn    = { };
    SecBuffer     secInputBuffers[2] = { };
    SecBuffer     secOutputBuffer    = { };
    vector<char>  socketBuffer(SOCKET_BUFFER_SIZE, 0);
    UINT32        lwrBufIdx = 0;

    CtxtHandle hCtx;
    bool bCtxInit = false;

    // Loop using InitializeSelwrityContext until success or complete failure
    ss = SEC_I_CONTINUE_NEEDED;
    for (;;)
    {
        // Immediately exit if the last return value is a faulure
        if ((ss != SEC_I_CONTINUE_NEEDED) &&
            (ss != SEC_E_INCOMPLETE_MESSAGE) &&
            (ss != SEC_I_INCOMPLETE_CREDENTIALS))
        {
            LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                                "Failed to negotiate connection\n");
            return -1;
        }

        secOutputBuffer.pvBuffer   = NULL;
        secOutputBuffer.BufferType = SECBUFFER_TOKEN;
        secOutputBuffer.cbBuffer   = 0;
        secBufferDescOut.cBuffers  = 1;
        secBufferDescOut.pBuffers  = &secOutputBuffer;
        secBufferDescOut.ulVersion = SECBUFFER_VERSION;

        // Once the context is initialized there are several subsequent calls for
        // negotiation between the client/server and the input buffer to the
        // server must be initialized and the message constructed in the socket
        // buffer
        if (bCtxInit)
        {
            UINT32 bytesRead = 0;
            if (LwDiagUtils::OK != pCfttSocket->Read(&socketBuffer[lwrBufIdx],
                                                     SOCKET_BUFFER_SIZE - lwrBufIdx,
                                                     &bytesRead))
            {
                LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                                    "Failed to read data from server\n");
                return -1;
            }
            lwrBufIdx += bytesRead;

            secInputBuffers[0].BufferType   = SECBUFFER_TOKEN;
            secInputBuffers[0].cbBuffer     = lwrBufIdx;
            secInputBuffers[0].pvBuffer     = &socketBuffer[0];
            secInputBuffers[1].BufferType   = SECBUFFER_EMPTY;
            secInputBuffers[1].cbBuffer     = 0;
            secInputBuffers[1].pvBuffer     = 0;
            secBufferDescIn.ulVersion       = SECBUFFER_VERSION;
            secBufferDescIn.pBuffers        = &secInputBuffers[0];
            secBufferDescIn.cBuffers        = 2;
        }

        DWORD sspiOutputFlags = 0;
        ss = InitializeSelwrityContext(&hCred,
                                       bCtxInit ? &hCtx : 0,
                                       _T(AUTH_SERVER_PORT),
                                       s_SSPIFlags,
                                       0,
                                       0,
                                       bCtxInit ? &secBufferDescIn : 0,
                                       0,
                                       bCtxInit ? 0 : &hCtx,
                                       &secBufferDescOut,
                                       &sspiOutputFlags,
                                       0);

        if (bCtxInit)
            secExit.SetSelwrityContext(&hCtx);

        if (ss == SEC_E_INCOMPLETE_MESSAGE)
            continue; // allow more

        lwrBufIdx = 0;

        if (FAILED(ss))
        {
            LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                                "Failed to negotiate connection\n");
            return -1;
        }

        if (!bCtxInit && ss != SEC_I_CONTINUE_NEEDED)
        {
            LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                                "Failed to negotiate connection\n");
            return -1;
        }

        // Write the output buffer back to the server and free the allocated data
        LwDiagUtils::EC ec = pCfttSocket->Write(static_cast<char *>(secOutputBuffer.pvBuffer),
                                                secOutputBuffer.cbBuffer);
        FreeContextBuffer(secOutputBuffer.pvBuffer);
        if (LwDiagUtils::OK != ec)
        {
            LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                                "Failed to write data to server\n");
            return -1;
        }

        // Wen we get to this point the first time the initial contex has been created
        if (!bCtxInit)
        {
            bCtxInit = true;
            continue;
        }

        if (ss == S_OK)
            break;
    }

    // Query and compare attributes to ensure that this is really the lwpu network
    PCCERT_CONTEXT  cliCert;
    if (QueryContextAttributes(&hCtx, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &cliCert) != S_OK)
    {
        LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                            "Failed to query the remote server context\n");
        return -1;
    }

    LwDiagUtils::Printf(LwDiagUtils::PriNormal, "BYTE s_SerialNumData[] =\n");
    PrintData(cliCert->pCertInfo->SerialNumber.cbData, cliCert->pCertInfo->SerialNumber.pbData);
    LwDiagUtils::Printf(LwDiagUtils::PriNormal,
        "CRYPT_INTEGER_BLOB s_SerialNum = { sizeof(s_SerialNumData), &s_SerialNumData[0] };\n");

    LwDiagUtils::Printf(LwDiagUtils::PriNormal, "BYTE s_IssuerData[] =\n");
    PrintData(cliCert->pCertInfo->Issuer.cbData, cliCert->pCertInfo->Issuer.pbData);
    LwDiagUtils::Printf(LwDiagUtils::PriNormal,
        "CRYPT_INTEGER_BLOB s_Issuer = { sizeof(s_IssuerData), &s_IssuerData[0] };\n");

    bool bAlgParams = false;
    if (cliCert->pCertInfo->SubjectPublicKeyInfo.Algorithm.Parameters.cbData)
    {
        bAlgParams = true;
        LwDiagUtils::Printf(LwDiagUtils::PriNormal, "BYTE s_AlgParamData[] =\n");
        PrintData(cliCert->pCertInfo->SubjectPublicKeyInfo.Algorithm.Parameters.cbData,
                  cliCert->pCertInfo->SubjectPublicKeyInfo.Algorithm.Parameters.pbData);
    }

    LwDiagUtils::Printf(LwDiagUtils::PriNormal, "BYTE s_ServerPubKeyData[] =\n");
    PrintData(cliCert->pCertInfo->SubjectPublicKeyInfo.PublicKey.cbData,
              cliCert->pCertInfo->SubjectPublicKeyInfo.PublicKey.pbData);

    LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                        "CERT_PUBLIC_KEY_INFO s_ServerPubKey =\n"
                        "{\n"
                        "    { \"%s\" { %s, %s }},\n"
                        "    { sizeof(s_ServerPubKeyData), &s_ServerPubKeyData[0], 0 }\n"
                        "};\n",
                        cliCert->pCertInfo->SubjectPublicKeyInfo.Algorithm.pszObjId,
                        bAlgParams ? "sizeof(s_AlgParamData)" : "0",
                        bAlgParams ? "&s_AlgParamData[0]" : "0");
    return 0;
}
