/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or diss_Closure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "decrypt_log.h"
#include "aes.h"
#include "ccm.h"
#include "core/include/version.h"
#include "encryption.h"
#include "encryption_common.h"

LwDiagUtils::EC Decryptor::DecryptLog
(
    FILE*           pInFile,
    vector<UINT08>* pDecryptBuffer
)
{
    LwDiagUtils::EC ec = LwDiagUtils::OK;
    vector<UINT08> data;
    CHECK_EC(Encryption::LoadFile(pInFile, &data));

    const auto encryptType = LwDiagUtils::GetDataArrayEncryption(&data[0], data.size());

    if (encryptType == LwDiagUtils::NOT_ENCRYPTED)
    {
        pDecryptBuffer->swap(data);
        return LwDiagUtils::OK;
    }

    // Header:
    // * 3 bytes of tag to recognize encryption type,
    // * N bytes of version string (may change between versions)
    // * M bytes of arch string (may change between versions)
    // * 16 bytes of initialization vector
    constexpr size_t hdrSize = 3 + LOG_VERSION_LENGTH + LOG_ARCH_LENGTH + 16;

    if (encryptType != LwDiagUtils::ENCRYPTED_LOG_V3 ||
        data.size() <= hdrSize)
    {
        LwDiagUtils::Printf(LwDiagUtils::PriError, "Unrecognized file format\n");
        return LwDiagUtils::ILWALID_FILE_FORMAT;
    }

    const auto expectedVerLen = strnlen(g_Version, LOG_VERSION_LENGTH);
    const auto logVerLen      = strnlen(reinterpret_cast<const char*>(&data[3]),
                                        LOG_VERSION_LENGTH);
    const auto logArchLen     = strnlen(reinterpret_cast<const char*>(&data[3+LOG_VERSION_LENGTH]),
                                        LOG_ARCH_LENGTH);

    // Make sure there is a terminating NUL character at the end
    char logVer[LOG_VERSION_LENGTH + 1] = { };
    char logArch[LOG_ARCH_LENGTH + 1]   = { };
    memcpy(logVer, &data[3], logVerLen);
    memcpy(logArch, &data[3 + LOG_VERSION_LENGTH], logArchLen);

    {
        CryptoPP::SecByteBlock iv(CryptoPP::AES::BLOCKSIZE);
        memcpy(&iv[0], &data[3 + LOG_VERSION_LENGTH + LOG_ARCH_LENGTH], iv.size());

        // Set the encryption key
        CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption dec;
        {
            CryptoPP::SecByteBlock key(0, CryptoPP::AES::DEFAULT_KEYLENGTH);
            static_assert(CryptoPP::AES::DEFAULT_KEYLENGTH == 16, "Unexpected key length");
            GetAESLogKey(&key[0]);
            dec.SetKeyWithIV(key, key.size(), iv);
        }

        // Decrypt the verification string
        static const char verifString[] = LOG_ENCRYPTION_VERIF_STR;
        char decryptedVerifStr[sizeof(verifString) - 1];
        dec.ProcessData((CryptoPP::byte*)&decryptedVerifStr[0],
                        (const CryptoPP::byte*)&data[hdrSize],
                        sizeof(decryptedVerifStr));
        if (0 != memcmp(verifString, decryptedVerifStr, sizeof(decryptedVerifStr)))
        {
            LwDiagUtils::Printf(LwDiagUtils::PriError,
                                "The log did not decrypt correctly, "
                                "this decrypt tool is incorrect for this log\n");
            if ((logVerLen < expectedVerLen) ||
                (0 != memcmp(logVer + logVerLen - expectedVerLen, g_Version, expectedVerLen)))
            {
                LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                                    "This decrypt tool supports version %s, "
                                    "but the log was encrypted with MODS version %s on %s\n",
                                    g_Version, logVer, logArch);
            }
            else
            {
                LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                                    "The log was encrypted with MODS version %s on %s\n",
                                    logVer, logArch);
            }

            ec = LwDiagUtils::DECRYPTION_ERROR;
        }

        // Decrypt the data
        dec.ProcessData((CryptoPP::byte*)&data[0],
                        (const CryptoPP::byte*)&data[hdrSize + sizeof(decryptedVerifStr)],
                        data.size() - hdrSize - sizeof(decryptedVerifStr));

        data.resize(data.size() - hdrSize - sizeof(decryptedVerifStr));
    }

    pDecryptBuffer->swap(data);

    return ec;
}
