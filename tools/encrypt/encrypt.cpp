/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <memory>
#include <string>
#include <vector>

#include "lwdiagutils.h"
#include "aes.h"
#include "align.h"
#include "ccm.h"
#include "core/include/zlib.h"
#include "decrypt.h"
#include "encrypt.h"
#include "encryption.h"
#include "encryption_common.h"
#include "random.h"

namespace Encryptor
{
    //! \brief Compress and encrypt a file
    //!
    //! Preprocess, compress and encrypt a file.
    //! \param fileName            : The filename to encrypt
    //! \param outFileName         : The filename to write to
    //! \param additionalPaths     : Additional search paths for include files
    //! \param numPaths            : Number of entries in the additional paths
    //! \param preprocDefs         : Preprocessor definitions
    //! \param numDefs             : Number of entries in the preprocessor definitions
    //! \param boundFile           : Generate bound JS header file
    //! \param preprocessFile      : Run preprocessor on the input file
    //! \sa JavaScript::RunEncryptedScript
    static LwDiagUtils::EC EncryptFile
    (
        const string& fileName,
        const string& outFileName,
        char        **additionalPaths,
        UINT32        numPaths,
        char        **preprocDefs,
        UINT32        numDefs,
        bool          boundFile,
        bool          preprocessFile,
        bool          traceFile
    )
    {
        LWDASSERT(fileName != "");
        LWDASSERT(!(preprocessFile && traceFile));
        LWDASSERT(!(boundFile && traceFile));

        string importPath;

        // This is the top level import file, store its path.
        string::size_type Pos = fileName.rfind('/');
        if (Pos != string::npos)
        {
            importPath = fileName.substr(0, Pos + 1);
        }
        else
        {
            importPath = "";
        }

        LwDiagUtils::EC ec = LwDiagUtils::OK;

        string fullFileName = fileName;
        // Check if the file exists.
        if (!LwDiagXp::DoesFileExist(fileName))
        {
            // Search for the file.
            vector<string> paths;
            paths.push_back(importPath);
            string path = LwDiagUtils::FindFile(fileName, paths);
            if ("" == path)
            {
                LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                                    "%s does not exist.\n",
                                    fileName.c_str());
                return LwDiagUtils::FILE_DOES_NOT_EXIST;
            }
            fullFileName = path + fullFileName;
        }

        vector<UINT08> buffer;

        {
            if (preprocessFile && !traceFile)
            {
                ec = Encryption::PreprocessFile(fullFileName.c_str(),
                                                &buffer,
                                                additionalPaths,
                                                numPaths,
                                                preprocDefs,
                                                numDefs);

                if (LwDiagUtils::OK != ec)
                {
                    LwDiagUtils::Printf(LwDiagUtils::PriError, "Failed to preprocess file %s\n",
                            fullFileName.c_str());
                    return ec;
                }
            }
            else
            {
                LwDiagUtils::FileHolder inFile;
                ec = inFile.Open(fullFileName.c_str(), "rb");

                if (LwDiagUtils::OK != ec)
                {
                    LwDiagUtils::Printf(LwDiagUtils::PriError, "Failed to open file %s\n",
                                        fullFileName.c_str());
                    return ec;
                }

                ec = Encryption::LoadFile(inFile.GetFile(), &buffer);
                if (LwDiagUtils::OK != ec)
                {
                    LwDiagUtils::Printf(LwDiagUtils::PriError, "Failed to read from file %s\n",
                                        fullFileName.c_str());
                    return ec;
                }
            }

            LwDiagUtils::Printf(LwDiagUtils::PriDebug,
                                "%s has %lu bytes.\n", fullFileName.c_str(),
                                (unsigned long)buffer.size());

            if (buffer.empty())
            {
                LwDiagUtils::Printf(LwDiagUtils::PriError,
                                    "Empty file cannot be encrypted.\n");
            }

            // Compress the file with zlib
            vector<UINT08> compressed;
            if (!traceFile)
            {
                z_stream strm = { };
                if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK)
                {
                    LwDiagUtils::Printf(LwDiagUtils::PriError,
                                        "Failed to initialize compression\n");
                    return LwDiagUtils::FILE_NOMEM;
                }

                strm.avail_in = buffer.size();
                strm.next_in = &buffer[0];

                compressed.resize(AlignUp(buffer.size(), static_cast<size_t>(64 * 1024)));

                strm.avail_out = compressed.size();
                strm.next_out = (UINT08*)&compressed[0];

                const auto ret = deflate(&strm, Z_FINISH);
                if (ret != Z_STREAM_END)
                {
                    LwDiagUtils::Printf(LwDiagUtils::PriError, "Compression failed\n");
                    deflateEnd(&strm);
                    return LwDiagUtils::FILE_NOMEM;
                }

                compressed.resize(compressed.size() - strm.avail_out);
                deflateEnd(&strm);
            }

            LwDiagUtils::FileHolder outFile;

            CHECK_EC(outFile.Open(outFileName.c_str(), "wb"));

            UINT64 wroteBytes = 0;

            // For bound JS we store 7 bits per output byte (hence n*8/7) and
            // it must be aligned up to account for the tail (hence +6)
            // Include 16 bytes for initialization vector which are also being
            // encoded.
            // Include the 3-byte file type tag, which is not encoded.
            UINT64 encodedSize = static_cast<UINT64>(
                    ((compressed.size() + 16) * 8 + 6) / 7 + 3 + 8);
            vector<UINT08> encoded;

            if (boundFile)
            {
                fprintf(outFile.GetFile(), "static const char BoundScriptName[] = \"%s\";\n",
                        fullFileName.c_str());
                fprintf(outFile.GetFile(), "static constexpr UINT32 BoundScriptSize = %llu;\n",
                        encodedSize);
                fprintf(outFile.GetFile(),
                        "static const UINT08 BoundScript[BoundScriptSize] = \n{\n" );
                encoded.reserve(encodedSize);
                encoded.push_back(0xf1U);
                encoded.push_back(0x1aU);
                encoded.push_back(0x82U);

                const UINT64 bufferSize = static_cast<UINT64>(buffer.size());
                encoded.push_back(static_cast<UINT08>(bufferSize & 0xFF));
                encoded.push_back(static_cast<UINT08>((bufferSize >>  8) & 0xFF));
                encoded.push_back(static_cast<UINT08>((bufferSize >> 16) & 0xFF));
                encoded.push_back(static_cast<UINT08>((bufferSize >> 24) & 0xFF));
                encoded.push_back(static_cast<UINT08>((bufferSize >> 32) & 0xFF));
                encoded.push_back(static_cast<UINT08>((bufferSize >> 40) & 0xFF));
                encoded.push_back(static_cast<UINT08>((bufferSize >> 48) & 0xFF));
                encoded.push_back(static_cast<UINT08>((bufferSize >> 56) & 0xFF));
            }
            else
            {
                // encryption flag
                fprintf(outFile.GetFile(), "%c%c%c", 0xf1, 0x1a, 0x80);
                UINT64 bufferSize = static_cast<UINT64>(buffer.size());
                if (fwrite(&bufferSize, 1, sizeof(UINT64), outFile.GetFile()) != sizeof(UINT64))
                {
                    ec = LwDiagUtils::InterpretFileError();
                    LwDiagUtils::Printf(LwDiagUtils::PriError, "Failed writing to file %s\n",
                                        outFileName.c_str());
                    return ec;
                }
                wroteBytes += 11;
            }

            // Generate random initialization vector for AES counter mode.
            //
            // The initialization vector must be different and unique for every
            // file encrypted with the same key.
            CryptoPP::SecByteBlock iv(CryptoPP::AES::BLOCKSIZE);
            static_assert(CryptoPP::AES::BLOCKSIZE == 16, "Unexpected block size");
            Random random;
            random.SeedRandom(time(0));
            for (size_t i = 0; i < iv.size(); i++)
            {
                iv[i] = static_cast<CryptoPP::byte>(random.GetRandom(0, 255));
            }

            {
                // Set the encryption key
                CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption enc;
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
                    enc.SetKeyWithIV(key, key.size(), iv);
                }

                // Encrypt the data
                if (traceFile)
                {
                    enc.ProcessData((CryptoPP::byte*)&buffer[0],
                                    (CryptoPP::byte*)&buffer[0],
                                    buffer.size());
                }
                else
                {
                    enc.ProcessData((CryptoPP::byte*)&compressed[0],
                                    (CryptoPP::byte*)&compressed[0],
                                    compressed.size());
                }
            }

            if (boundFile)
            {
                UINT32 aclwm       = 0;
                int    bitsInAclwm = 0;

                const auto Emit = [&](UINT08 value)
                {
                    aclwm = (aclwm << 8) + value;
                    bitsInAclwm += 8;
                    do
                    {
                        const UINT08 out = 0x80U | (aclwm >> (bitsInAclwm - 7));
                        encoded.push_back(out);
                        bitsInAclwm -= 7;
                    } while (bitsInAclwm >= 7);
                };

                // Write the initialization vector into the file
                for (const auto byte : iv)
                {
                    Emit(byte);
                }

                // Write encrypted data into the file
                for (const auto byte : compressed)
                {
                    Emit(byte);
                }

                // Push out remaining bits into the file
                if (bitsInAclwm)
                {
                    LWDASSERT(bitsInAclwm < 7);
                    const UINT08 out = 0x80U | (aclwm << (7 - bitsInAclwm));
                    encoded.push_back(out);
                }

                // The number of bytes emitted must match the declared array size
                if (encoded.size() != encodedSize)
                {
                    LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                                        "Invalid size emitted %zu, expected %llu\n",
                                        encoded.size(), encodedSize);
                    return LwDiagUtils::FILE_2BIG;
                }

                // Attempt to decode the data to ensure that it was encoded correctly
                vector<UINT08> decrypted;
                if (traceFile)
                {
                    CHECK_EC(Decryptor::DecryptTraceDataArray(&encoded[0],
                                                              encoded.size(),
                                                              &decrypted));
                }
                else
                {
                    CHECK_EC(Decryptor::DecryptDataArray(&encoded[0],
                                                         encoded.size(),
                                                         &decrypted));
                }
                if (decrypted.size() != buffer.size())
                {
                    LwDiagUtils::Printf(LwDiagUtils::PriNormal,
                                        "Invalid size %zu after decompression, expected %zu\n",
                                        decrypted.size(), buffer.size());
                    return LwDiagUtils::FILE_2BIG;
                }
                if (memcmp(&buffer[0], &decrypted[0], buffer.size()) != 0)
                {
                    LwDiagUtils::Printf(LwDiagUtils::PriNormal, "Decryption failed\n");
                    return LwDiagUtils::SOFTWARE_ERROR;
                }

                // Emit C code to the array in header file
                for (UINT32 i = 0; i < encodedSize; )
                {
                    for (UINT32 col = 0; col < 32 && i < encodedSize; col++, i++)
                    {
                        fprintf(outFile.GetFile(), "0x%x,", encoded[i]);
                    }
                    fprintf(outFile.GetFile(), "\n");
                }
                fprintf(outFile.GetFile(), "\n};\n");
                wroteBytes += encoded.size();
            }
            else
            {
                // Write the initialization vector into the file
                if (fwrite(&iv[0], 1, iv.size(), outFile.GetFile()) != iv.size())
                {
                    ec = LwDiagUtils::InterpretFileError();
                    LwDiagUtils::Printf(LwDiagUtils::PriError, "Failed writing to file %s\n",
                                        outFileName.c_str());
                    return ec;
                }
                wroteBytes += iv.size();

                // Write encrypted data into the file
                const auto size = traceFile ? buffer.size() : compressed.size();
                if (fwrite(traceFile ? &buffer[0] : &compressed[0], 1, size, outFile.GetFile()) != size)
                {
                    ec = LwDiagUtils::InterpretFileError();
                    LwDiagUtils::Printf(LwDiagUtils::PriError, "Failed writing to file %s\n",
                            outFileName.c_str());
                    return ec;
                }
                wroteBytes += size;
            }

            LwDiagUtils::Printf(LwDiagUtils::PriDebug, "Wrote %llu bytes to %s.\n",
                                wroteBytes, outFileName.c_str());
        }

        return ec;
    }
}
LwDiagUtils::EC Encryptor::EncryptFile
(
    const string& fileName,
    const string& outFileName,
    char        **additionalPaths,
    UINT32        numPaths,
    char        **preprocDefs,
    UINT32        numDefs,
    bool          boundFile,
    bool          preprocessFile
)
{
    return EncryptFile(fileName,
                       outFileName,
                       additionalPaths,
                       numPaths,
                       preprocDefs,
                       numDefs,
                       boundFile,
                       preprocessFile,
                       false);
}

LwDiagUtils::EC Encryptor::EncryptTraceFile(const string &fileName, const string &outFileName)
{
    return EncryptFile(fileName,
                       outFileName,
                       nullptr,
                       0,
                       nullptr,
                       0,
                       false,
                       false,
                       true);
}

