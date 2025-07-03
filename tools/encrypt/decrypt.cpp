/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or diss_Closure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "decrypt.h"
#include "aes.h"
#include "ccm.h"
#include "core/include/zlib.h"
#include "encryption.h"
#include "encryption_common.h"

namespace
{
    class ZeroOnScopeExit
    {
        public:
            explicit ZeroOnScopeExit(vector<UINT08>& v): m_Vec(v) { }
            ~ZeroOnScopeExit()
            {
                if (!m_Vec.empty())
                {
                    memset(&m_Vec[0], 0, m_Vec.size());
                }
            }

        private:
            vector<UINT08>& m_Vec;
    };

    // Base-128 encoding is used primarily for bound JS to avoid tripping obfuscation.
    // We cram 7 bits into every byte and we set the highest bit to 1, so that it
    // has no chance to match any letters or digits.
    bool DecodeBase128
    (
        vector<UINT08>* pOut,
        const UINT08*   data,
        size_t          dataSize
    )
    {
        LWDASSERT(pOut);
        LWDASSERT(data);

        // The header is copied without decoding (file tag and file size)
        const size_t hdrSize = 3 + 8;

        // The first three bytes are copied without decoding, because they
        // constitute the file type tag.  The next 8 bytes are also copied
        // without decoding because they contitute the file size
        const size_t newSize = ((dataSize - hdrSize) * 7) / 8 + hdrSize;

        pOut->resize(newSize);

        // Skip file type tag, it is not encoded.
        // We don't need to copy it because we've identified the file type already.
        data     += hdrSize;
        dataSize -= hdrSize;

        UINT08* pDest = &(*pOut)[hdrSize];
        UINT08* const pDestEnd = &(*pOut)[0] + pOut->size();

        UINT32 aclwm    = 0;
        int    needBits = 8;

        while (dataSize)
        {
            const UINT08 value = *(data++);
            --dataSize;

            if (!(value & 0x80U))
            {
                LwDiagUtils::Printf(LwDiagUtils::PriError, "Detected corruption\n");
                return false;
            }

            aclwm = (aclwm << 7) | (value & 0x7FU);
            needBits -= 7;

            if (needBits <= 0)
            {
                const UINT08 out = static_cast<UINT08>(aclwm >> -needBits);
                *(pDest++) = out;
                needBits += 8;
            }
        }

        // MASSERT won't work because we rarely build EUD in debug mode
        if (pDest != pDestEnd)
        {
            LwDiagUtils::Printf(LwDiagUtils::PriError, "Looks like a bug\n");
            return false;
        }

        return true;
    }
}

namespace Decryptor
{

    static LwDiagUtils::EC DecryptDataArray
    (
        const UINT08*   data,
        size_t          dataSize,
        vector<UINT08>* pDecryptBuffer,
        bool            traceFile
    )
    {
        const auto encryptType = LwDiagUtils::GetDataArrayEncryption(data, dataSize);

        if (encryptType == LwDiagUtils::NOT_ENCRYPTED)
        {
            // If not decrypting in place copy the data
            if (data != pDecryptBuffer->data())
            {
                pDecryptBuffer->resize(dataSize);
                memcpy(&(*pDecryptBuffer)[0], data, dataSize);
            }
            return LwDiagUtils::OK;
        }

        // Header:
        // * 3 bytes of tag to recognize encryption type,
        // * 8 bytes of unencrypted data size,
        // * 16 bytes of initialization vector
        constexpr size_t hdrSize = 3 + 8 + 16;

        if ((encryptType != LwDiagUtils::ENCRYPTED_FILE_V3 &&
             encryptType != LwDiagUtils::ENCRYPTED_FILE_V3_BASE128)
            ||
            dataSize <= hdrSize)
        {
            LwDiagUtils::Printf(LwDiagUtils::PriError, "Unrecognized file format\n");
            return LwDiagUtils::ILWALID_FILE_FORMAT;
        }

        vector<UINT08> rebased;
        if (encryptType == LwDiagUtils::ENCRYPTED_FILE_V3_BASE128)
        {
            if (!DecodeBase128(&rebased, data, dataSize))
            {
                return LwDiagUtils::ILWALID_FILE_FORMAT;
            }
            data     = rebased.data();
            dataSize = rebased.size();
        }

        CryptoPP::SecByteBlock decrypted(0, dataSize - hdrSize);

        {
            CryptoPP::SecByteBlock iv(CryptoPP::AES::BLOCKSIZE);

            // initialization vector starts after the encryption type and unencrypted size
            memcpy(&iv[0], &data[3 + 8], iv.size());

            // Set the encryption key
            CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption dec;
            {
                CryptoPP::SecByteBlock key(0, CryptoPP::AES::DEFAULT_KEYLENGTH);
                static_assert(CryptoPP::AES::DEFAULT_KEYLENGTH == 16, "Unexpected key length");
                if (traceFile)
                {
                    GetAESTraceFileKey(&key[0]);
                }
                else
                {
                    GetAESFileKey(&key[0]);
                }
                dec.SetKeyWithIV(key, key.size(), iv);
            }

            // Decrypt the data
            dec.ProcessData(&decrypted[0],
                            (const CryptoPP::byte*)&data[hdrSize],
                            decrypted.size());
        }

        if (!pDecryptBuffer->empty())
        {
            memset(&(*pDecryptBuffer)[0], 0, pDecryptBuffer->size());
        }

        if (traceFile)
        {
            pDecryptBuffer->assign(decrypted.begin(), decrypted.end());
        }
        else
        {
            // Decompress decrypted data
            z_stream strm = { };
            if (inflateInit(&strm) != Z_OK)
            {
                LwDiagUtils::Printf(LwDiagUtils::PriError, "Failed to initialize decompression\n");
                return LwDiagUtils::FILE_NOMEM;
            }
            strm.next_in = &decrypted[0];
            strm.avail_in = decrypted.size();

            // Ensure there's 10x capacity to reduce reallocations
            size_t capacityMult = 10;
            if (decrypted.size() > 500_MB)
                capacityMult = 2;
            pDecryptBuffer->reserve(decrypted.size() * capacityMult);

            do
            {
                const auto oldSize = pDecryptBuffer->size();
                const size_t delta = oldSize ? 64 * 1024 : decrypted.size() * 2;

                // Make room for more data
                const auto newSize = oldSize + delta;
                if (pDecryptBuffer->capacity() < newSize)
                {
                    // Ensure we always zero memory before releasing it, so that we
                    // reduce the risk of leaking unencrypted contents
                    vector<UINT08> newBuf;
                    newBuf.reserve(max(pDecryptBuffer->capacity() * 2U, newSize));
                    newBuf.resize(oldSize);
                    memcpy(&newBuf[0], &(*pDecryptBuffer)[0], oldSize);
                    pDecryptBuffer->swap(newBuf);
                    memset(&newBuf[0], 0, newBuf.size());
                }
                pDecryptBuffer->resize(newSize);

                strm.next_out = &(*pDecryptBuffer)[oldSize];
                strm.avail_out = delta;

                const auto ret = inflate(&strm, Z_NO_FLUSH);
                if (ret != Z_OK && ret != Z_STREAM_END)
                {
                    LwDiagUtils::Printf(LwDiagUtils::PriError, "Decompression failed %d\n", ret);
                    inflateEnd(&strm);
                    return LwDiagUtils::ILWALID_FILE_FORMAT;
                }

                const auto have = delta - strm.avail_out;
                pDecryptBuffer->resize(oldSize + have);
            } while (strm.avail_out == 0);
        }

        return LwDiagUtils::OK;
    }

    static LwDiagUtils::EC DecryptFile
    (
        FILE*           pInFile,
        vector<UINT08>* pDecryptBuffer,
        bool            traceFile
    )
    {
        LwDiagUtils::EC ec = LwDiagUtils::OK;
        vector<UINT08> inputBuffer;

        // Don't leave the original contents of the file in memory
        ZeroOnScopeExit zero(inputBuffer);

        CHECK_EC(Encryption::LoadFile(pInFile, &inputBuffer));

        const auto encryptType = LwDiagUtils::GetDataArrayEncryption(&inputBuffer[0],
                                                                     inputBuffer.size());

        if (encryptType == LwDiagUtils::NOT_ENCRYPTED)
        {
            pDecryptBuffer->swap(inputBuffer);
            return LwDiagUtils::OK;
        }

        return DecryptDataArray(&inputBuffer[0], inputBuffer.size(), pDecryptBuffer, traceFile);
    }
}

LwDiagUtils::EC Decryptor::DecryptFile
(
    FILE*           pInFile,
    vector<UINT08>* pDecryptBuffer
)
{
    return DecryptFile(pInFile, pDecryptBuffer, false);
}

LwDiagUtils::EC Decryptor::DecryptTraceFile
(
    FILE*           pInFile,
    vector<UINT08>* pDecryptBuffer
)
{
    return DecryptFile(pInFile, pDecryptBuffer, true);
}

LwDiagUtils::EC Decryptor::DecryptDataArray
(
    const UINT08*   data,
    size_t          dataSize,
    vector<UINT08>* pDecryptBuffer
)
{
    return DecryptDataArray(data, dataSize, pDecryptBuffer, false);
}

LwDiagUtils::EC Decryptor::DecryptTraceDataArray
(
    const UINT08*   data,
    size_t          dataSize,
    vector<UINT08>* pDecryptBuffer
)
{
    return DecryptDataArray(data, dataSize, pDecryptBuffer, true);
}

LwDiagUtils::EC Decryptor::GetDecryptedSize
(
    const UINT08*   data,
    size_t          dataSize,
    size_t*         pDecryptedSize
)
{
    const auto encryptType = LwDiagUtils::GetDataArrayEncryption(data, dataSize);

    if (encryptType == LwDiagUtils::NOT_ENCRYPTED)
    {
        *pDecryptedSize = 0;
        return LwDiagUtils::OK;
    }

    // Header:
    // * 3 bytes of tag to recognize encryption type,
    // * 8 bytes of unencrypted data size,
    constexpr size_t hdrSize = 3 + 8;
    if ((encryptType != LwDiagUtils::ENCRYPTED_FILE_V3 &&
         encryptType != LwDiagUtils::ENCRYPTED_FILE_V3_BASE128)
        ||
        dataSize <= hdrSize)
    {
        LwDiagUtils::Printf(LwDiagUtils::PriError, "Unrecognized file format\n");
        return LwDiagUtils::ILWALID_FILE_FORMAT;
    }

    vector<UINT08> rebased;
    if (encryptType == LwDiagUtils::ENCRYPTED_FILE_V3_BASE128)
    {
        if (!DecodeBase128(&rebased, data, dataSize))
        {
            return LwDiagUtils::ILWALID_FILE_FORMAT;
        }
        data     = rebased.data();
        dataSize = rebased.size();
    }

    *pDecryptedSize = static_cast<size_t>(static_cast<UINT64>(data[3]) |
                                          (static_cast<UINT64>(data[4])  <<  8) |
                                          (static_cast<UINT64>(data[5])  << 16) |
                                          (static_cast<UINT64>(data[6])  << 24) |
                                          (static_cast<UINT64>(data[7])  << 32) |
                                          (static_cast<UINT64>(data[8])  << 40) |
                                          (static_cast<UINT64>(data[9])  << 48) |
                                          (static_cast<UINT64>(data[10]) << 56));
    return LwDiagUtils::OK;
}
