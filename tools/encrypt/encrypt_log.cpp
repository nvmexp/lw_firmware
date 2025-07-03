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

#include "encrypt_log.h"
#include "aes.h"
#include "ccm.h"
#include "core/include/version.h"
#include "encryption_common.h"
#include "random.h"

struct Encryptor::LogSink::Impl
{
    CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption enc;
};

Encryptor::LogSink::LogSink()
{
}

Encryptor::LogSink::~LogSink()
{
}

LwDiagUtils::EC Encryptor::LogSink::Initialize(FILE* pFile, WhatFile what)
{
    m_pFile = pFile;
    m_pImpl = make_unique<Impl>();

    CryptoPP::SecByteBlock iv(CryptoPP::AES::BLOCKSIZE);
    static_assert(CryptoPP::AES::BLOCKSIZE == 16, "Unexpected block size");

    long pos = 0;

    if (what == APPEND)
    {
        if (fseek(pFile, 3, SEEK_SET) != 0)
        {
            return LwDiagUtils::InterpretFileError();
        }

        char logVersion[LOG_VERSION_LENGTH] = { };
        const auto numVersBytes = fread(&logVersion[0], 1, LOG_VERSION_LENGTH, pFile);
        if (numVersBytes != LOG_VERSION_LENGTH)
        {
            return LwDiagUtils::EC::ILWALID_FILE_FORMAT;
        }
        if (strncmp(logVersion, g_Version, LOG_VERSION_LENGTH))
        {
            return LwDiagUtils::EC::ILWALID_FILE_FORMAT;
        }

        char logArch[LOG_ARCH_LENGTH];
        const auto numArchBytes = fread(&logArch[0], 1, LOG_ARCH_LENGTH, pFile);
        if (numArchBytes != LOG_ARCH_LENGTH)
        {
            return LwDiagUtils::EC::ILWALID_FILE_FORMAT;
        }

        const auto numRead = fread(&iv[0], 1, iv.size(), pFile);
        if (numRead != iv.size())
        {
            return LwDiagUtils::EC::ILWALID_FILE_FORMAT;
        }

        if (fseek(pFile, 0, SEEK_END) != 0)
        {
            return LwDiagUtils::InterpretFileError();
        }
        pos = ftell(pFile) - (3 + LOG_VERSION_LENGTH + LOG_ARCH_LENGTH + 16);
    }
    else
    {
        LWDASSERT(what == NEW_FILE);
        if (fprintf(pFile, "%c%c%c", 0xf1, 0x1a, 0x81) != 3)
        {
            return ferror(pFile) ? LwDiagUtils::InterpretFileError()
                                 : LwDiagUtils::EC::FILE_UNKNOWN_ERROR;
        }

        char logVersion[LOG_VERSION_LENGTH] = { };
        strncpy(&logVersion[0], g_Version, LOG_VERSION_LENGTH);
        const auto numVersBytes = fwrite(&logVersion[0], 1, LOG_VERSION_LENGTH, pFile);
        if (numVersBytes != LOG_VERSION_LENGTH)
        {
            return ferror(pFile) ? LwDiagUtils::InterpretFileError()
                                 : LwDiagUtils::EC::FILE_UNKNOWN_ERROR;
        }

        char logArch[LOG_ARCH_LENGTH] = { };
        strncpy(&logArch[0], g_BuildArch, LOG_ARCH_LENGTH);
        const auto numArchBytes = fwrite(&logArch[0], 1, LOG_ARCH_LENGTH, pFile);
        if (numArchBytes != LOG_ARCH_LENGTH)
        {
            return ferror(pFile) ? LwDiagUtils::InterpretFileError()
                                 : LwDiagUtils::EC::FILE_UNKNOWN_ERROR;
        }

        Random random;
        random.SeedRandom(time(0)); // very poor, very little entropy
        for (UINT32 i = 0; i < iv.size(); i++)
        {
            iv[i] = static_cast<char>(random.GetRandom(0, 255));
        }

        const auto numWritten = fwrite(&iv[0], 1, iv.size(), pFile);
        if (numWritten != iv.size())
        {
            return ferror(pFile) ? LwDiagUtils::InterpretFileError()
                                 : LwDiagUtils::EC::FILE_UNKNOWN_ERROR;
        }
    }

    CryptoPP::SecByteBlock key(0, CryptoPP::AES::DEFAULT_KEYLENGTH);
    static_assert(CryptoPP::AES::DEFAULT_KEYLENGTH == 16, "Unexpected key length");
    GetAESLogKey(&key[0]);

    m_pImpl->enc.SetKeyWithIV(key, key.size(), iv);

    if (what == APPEND)
    {
        m_pImpl->enc.Seek(pos);
    }
    else
    {
        // Write verification string to the log, which is used by the decrypt
        // tool to verify whether the log will be decrypted correctly
        static const char verifString[] = LOG_ENCRYPTION_VERIF_STR;
        Append(verifString, sizeof(verifString) - 1);
    }

    return LwDiagUtils::EC::OK;
}

void Encryptor::LogSink::Append(const char* str, size_t size)
{
    LWDASSERT(m_pImpl.get());
    LWDASSERT(m_pFile);

    if (!m_pImpl.get() || !m_pFile || size == 0)
    {
        return;
    }

    char stackBuf[128];
    char* encrypted = &stackBuf[0];

    const bool newBuf = size > sizeof(stackBuf);
    unique_ptr<char[]> heapBuf;
    if (newBuf)
    {
        heapBuf = make_unique<char[]>(size);
        encrypted = heapBuf.get();
    }

    m_pImpl->enc.ProcessData((CryptoPP::byte*)encrypted, (const CryptoPP::byte*)str, size);

    fwrite(encrypted, 1, size, m_pFile);
}
