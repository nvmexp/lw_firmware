/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// File sink.

#include <cstdio>

#include "core/include/filesink.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "core/include/cmdline.h"
#include "core/include/errormap.h"
#include "core/include/fileholder.h"
#include "core/include/log.h"
#include "core/include/tasker.h"
#include "core/signature/signature.h"
#include <cstring>

FileSink::FileSink() :
   m_ModeMask(0),
   m_SplitIndex(0),
   m_pFile(nullptr),
   m_AlwaysFlush(false),
   m_FlushedSinceLastWrite(true),
   m_WriteBinary(false),
   m_renameOnClose(false),
   m_ModsHeaderRecordingActive(false),
   m_FileSizeLimitMb(DEFAULT_FILS_SIZE_LIMIT_MB),
   m_FileSizeSplitMb(0),
   m_PrintsPerSizeCheck(PRINTS_PER_SIZE_CHECK),
   m_NumCharsInLwrrentSplit(0),
   m_HashState(nullptr)
{
    // reserve enough space so the move semantics don't kick in.
    m_FileName.reserve(256);
    m_FileName.insert(0, "NONE");
    m_Encrypt = CommandLine::GetEncryptFiles();
}

/* virtual */ FileSink::~FileSink()
{
   Close();
}

void FileSink::ResetRandom()
{
}

void FileSink::PrintEncrypted(const char* str, size_t strLen)
{
    m_EncryptedSink.Append(str, strLen);
}

void FileSink::PrintEncryptedBinary(const UINT08* data, size_t size)
{
    m_EncryptedSink.Append(reinterpret_cast<const char*>(data), size);
}

string FileSink::GetFileName() const
{
   return m_FileName;
}

string FileSink::GetFileNameOnly() const
{
    return LwDiagUtils::StripDirectory(m_FileName.c_str());
}

bool FileSink::IsSinkOpen()
{
    return m_pFile != nullptr;
}

void FileSink::CloseIfLimitReached()
{
    m_PrintsPerSizeCheck++;
    if (m_PrintsPerSizeCheck % PRINTS_PER_SIZE_CHECK)
        return;

    if (m_FileSizeLimitMb == NO_FILE_SIZE_LIMIT)
        return;

    long Offset;
    if (OK == Tell(&Offset))
    {
        Offset /= 1024*1024;  //round down to Mb
        if ((UINT32)Offset >= m_FileSizeLimitMb)
        {
            // setting this prevents relwrsion - when Printf is called again
            m_FileSizeLimitMb = NO_FILE_SIZE_LIMIT;
            static const char warning[] =
                "WARNING: File size limit reached - closing log file. Please change your"
                " -log_file_limit_mb arg usage to allow for a larger log \n";
            DoPrint(warning, sizeof(warning) - 1, Tee::SPS_NORMAL);
            Close();
        }
    }
}

void FileSink::OpenNewFileOnSplit(size_t size)
{
    m_NumCharsInLwrrentSplit += size;
    if (m_NumCharsInLwrrentSplit < m_FileSizeSplitMb * 1_MB)
        return;

    m_NumCharsInLwrrentSplit = 0;
    m_SplitIndex++;
    RC rc = OpenSplit();
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "Unable to create split %s at index %d\n",
            m_BaseFileName.c_str(), m_SplitIndex);
        Log::SetFirstError(ErrorMap(rc));
    }
}

/* virtual */ void FileSink::DoPrint
(
    const char*           str,
    size_t                strLen,
    Tee::ScreenPrintState state
)
{
   MASSERT(str != 0);

   if (m_pFile != 0)
   {
      if (m_Encrypt)
          PrintEncrypted(str, strLen);
      else
      {
          if (fwrite(str, 1, strLen, m_pFile) != strLen)
              return;
          if (m_HashState != nullptr)
          {
              size_t updatedOffset = 0;
#ifdef _WIN32
              const LwU8 endline[2] = {0xd, 0xa};
              for (size_t offset = 0; offset < strLen; offset++)
              {
                  if (str[offset] == '\n')
                  {
                      lw_sha256_update((lw_sha256_ctx*)m_HashState,
                          (const LwU8 *)&str[updatedOffset],
                          (LwU32)(offset - updatedOffset));
                      lw_sha256_update((lw_sha256_ctx*)m_HashState,
                          endline, 2);
                      updatedOffset = offset + 1;
                  }
              }
#endif
              lw_sha256_update((lw_sha256_ctx*)m_HashState,
                  (const LwU8 *)&str[updatedOffset],
                  (LwU32)(strLen - updatedOffset));
          }
      }

      m_FlushedSinceLastWrite = false;
      if (m_AlwaysFlush)
         Flush();
      else
         fflush(m_pFile);

      CloseIfLimitReached();
      if (m_FileSizeSplitMb)
      {
          OpenNewFileOnSplit(strLen);
      }
   }

   if (m_ModsHeaderRecordingActive)
   {
       m_ModsHeaderText.append(str, strLen);
   }
}

/* virtual */ bool FileSink::DoPrintBinary
(
    const UINT08* data,
    size_t        size
)
{
    if (m_pFile == 0)
    {
        return false;
    }

    bool ok = false;

    if (m_Encrypt)
    {
        PrintEncryptedBinary(data, size);
        ok = true;
    }
    else
    {
        const size_t wrote = fwrite(data, 1, size, m_pFile);

        ok = wrote == size;

        if (m_ModsHeaderRecordingActive)
        {
            m_ModsHeaderBinary.Push(data, size);
        }
    }

    m_FlushedSinceLastWrite = false;

    CloseIfLimitReached();
    if (m_FileSizeSplitMb)
    {
        OpenNewFileOnSplit(size);
    }

    return ok;
}

/* virtual */ void FileSink::Flush()
{
    if (m_pFile != 0 && !m_FlushedSinceLastWrite)
    {
        Xp::FlushFstream(m_pFile);
    }

    m_FlushedSinceLastWrite = true;
}

RC FileSink::Open
(
    const string &fileName,
    Mode   mode
)
{
    return Open(fileName, mode, CommandLine::GetEncryptFiles());
}

RC FileSink::Open
(
    const string &fileName,
    Mode   mode,
    bool   encrypt
)
{
    UINT32 modeMask = 0;
    if (encrypt)
    {
        modeMask |= MM_ENCRYPT;
    }

    if (mode == Append)
    {
        modeMask |= MM_APPEND;
    }
    return Open(modeMask, fileName);
}

RC FileSink::Open
(
    UINT32 modeMask,
    const string &fileName
)
{
    m_ModeMask = modeMask;
    m_BaseFileName = fileName;
    m_SplitIndex = 0;
    return OpenSplit();
}

RC FileSink::OpenSplit()
{
    RC rc;

    UINT32 modeMask = m_ModeMask;
    string fileName = m_BaseFileName;
    if (m_FileSizeSplitMb)
    {
        size_t pos = m_BaseFileName.find_last_of(".");
        if (pos == string::npos) 
        {
            pos = m_BaseFileName.size();
        }
        fileName.insert(pos, Utility::StrPrintf("_%06d", m_SplitIndex));
    }

    Tasker::MutexHolder mh;
    CHECK_RC(CloseAndAcquireMutex(&mh));

    m_Encrypt = (modeMask & MM_ENCRYPT) != 0;

    const bool append = (modeMask & MM_APPEND) && LwDiagXp::DoesFileExist(fileName);

    if (append)
    {
        // Use a file holder here instead of directly calling the APIs so
        // that when we go out of this scope the file will be closed
        FileHolder encryptedFile;

        // Open the file for read in binary mode (otherwise newlines will not
        // be counted as part of the filesize and the random seed will be off)
        // Ensure that we dont append to a file and create a mixed mode file
        CHECK_RC(encryptedFile.Open(fileName, "rb"));

        const auto encryptType = Utility::GetFileEncryption(encryptedFile.GetFile());

        // The file must be a non-JS type encrypted file to be able to
        // append to it
        if (m_Encrypt && (encryptType != Utility::ENCRYPTED_LOG_V3))
        {
            Printf(Tee::PriError,
                    "Unable to append encrypted data to non encrypted file %s\n",
                    fileName.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
        else if (!m_Encrypt && (encryptType != Utility::NOT_ENCRYPTED))
        {
            Printf(Tee::PriError,
                    "Unable to append unencrypted data to an encrypted file %s\n",
                    fileName.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    string openMode = append ? "r+" : "w+";

    if (m_Encrypt || (modeMask & MM_MLE))
    {
        openMode += "b";
    }

    CHECK_RC(Utility::OpenFile(fileName, &m_pFile, openMode.c_str()));

    // Initialize encryption
    if (m_Encrypt)
    {
        const auto ec = m_EncryptedSink.Initialize(
                    m_pFile,
                    append ? Encryptor::LogSink::APPEND
                           : Encryptor::LogSink::NEW_FILE);
        if (ec != LwDiagUtils::OK)
        {
            Printf(Tee::PriError, "Unable to open log file for writing\n");
            Close();
        }
        CHECK_RC(Utility::InterpretModsUtilErrorCode(ec));
    }

    if (append)
    {
        fseek(m_pFile, 0, SEEK_END);
    }

    // Note: we don't want the move sematics to be used or any new memory allocation to occur
    // because the leak checker will flag this as a memory leak.
    m_FileName.replace(0, fileName.size(), fileName.c_str());
    m_FileName.resize(fileName.size());

    if (m_HashState != nullptr)
    {
        DoPrint(Signature::VERSION_STRING, sizeof(Signature::VERSION_STRING) - 1, Tee::SPS_NORMAL);
        const char* lwr = Signature::LWRRENT_SIGNATURE.KeyName;
        DoPrint(lwr, strlen(lwr), Tee::SPS_NORMAL);
        DoPrint("\n", 1, Tee::SPS_NORMAL);
    }

    PrintCachedOutput();

    return RC::OK;
}

RC FileSink::Close()
{
    return CloseAndAcquireMutex(nullptr);
}

RC FileSink::CloseAndAcquireMutex(Tasker::MutexHolder* pMh)
{
    Tasker::MutexHolder mh;
    if (!pMh)
    {
        pMh = &mh;
    }

    if (nullptr != m_pFile)
    {
        if (m_HashState != nullptr)
        {
            LwU8 digest[LW_SHA256_DIGEST_SIZE];
            Signature::HmacSha256Outro((Signature::HmacSha256State*)m_HashState, digest);
            free(m_HashState);
            m_HashState = nullptr;
            char digestASCII[2*LW_SHA256_DIGEST_SIZE+1];
            digestASCII[2*LW_SHA256_DIGEST_SIZE] = 0;
            UINT32 shift = 4;
            for (UINT32 asciiIdx = 0; asciiIdx < 2*LW_SHA256_DIGEST_SIZE; asciiIdx++)
            {
                digestASCII[asciiIdx] =
                    Signature::HEX2ASCII[0xF & (digest[asciiIdx/2] >> shift)];
                shift ^= 4;
            }
            fputs(Signature::SIGNATURE_STRING, m_pFile);
            fputs(digestASCII, m_pFile);
        }

        // Block all writes until a file is opened.
        if (m_Mutex.get())
        {
            pMh->Acquire(m_Mutex.get());
        }

        Flush();
        fclose(m_pFile);
        m_pFile = nullptr;

        if (m_renameOnClose)
        {
            string dir = LwDiagUtils::StripFilename(m_FileName.c_str());
            string name = LwDiagUtils::StripDirectory(m_FileName.c_str());
            string newPath = LwDiagUtils::JoinPaths(dir, m_renameOnCloseName);

            if (rename(m_FileName.c_str(), newPath.c_str()) != 0)
            {
                return RC::FILE_WRITE_ERROR;
            }
        }
    }
    else
    {
        // Block all writes until a file is opened.
        if (m_Mutex.get())
        {
            pMh->Acquire(m_Mutex.get());
        }
    }

    m_FileName = "NONE";
    m_Encrypt = CommandLine::GetEncryptFiles();

    return RC::OK;
}

void FileSink::SetAlwaysFlush
(
   bool b
)
{
   m_AlwaysFlush = b;
}

bool FileSink::GetAlwaysFlush() const
{
   return m_AlwaysFlush;
}

RC FileSink::Seek(long offset, int origin)
{
    if (!m_pFile)
        return OK;

    if (m_Encrypt)
    {
        Printf(Tee::PriLow, "Error: Seeking is not supported inside encrypted files\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (fseek(m_pFile, offset, origin) != 0)
        return Utility::InterpretFileError();

    return OK;
}

RC FileSink::Tell(long * pOffset)
{
    MASSERT(pOffset);

    if (!m_pFile)
    {
        *pOffset = 0;
        return OK;
    }

    const long offset = ftell(m_pFile);

    if (offset == -1L)
        return Utility::InterpretFileError();

    *pOffset = offset;

    return OK;
}

RC FileSink::Read(vector<char> * pBuf, long numBytesToRead, long *pBytesRead)
{
    if (!m_pFile)
        return RC::FILE_READ_ERROR;

    if (m_Encrypt)
    {
        Printf(Tee::PriLow, "Error: Reading from encrypted files is not supported\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    MASSERT(pBuf);

    pBuf->resize(numBytesToRead);

    const size_t bytesRead = fread(&(*pBuf)[0], 1, numBytesToRead, m_pFile);

    if (pBytesRead)
        *pBytesRead = static_cast<long>(bytesRead);

    return OK;
}

void FileSink::StartSigning()
{
    if (m_HashState != nullptr)
    {
        MASSERT(!"Signature already enabled");
        return;
    }
    if (m_pFile != nullptr)
    {
        Printf(Tee::PriError,
            "File signature can be only started before the file is opened.\n");
        MASSERT(0);
        return;
    }
    if (Signature::LWRRENT_SIGNATURE.Method != Signature::METHOD_HMAC_SHA256)
    {
        Printf(Tee::PriError, "Signature method not implemented.\n");
        MASSERT(0);
        return;
    }

    m_HashState = malloc(sizeof(Signature::HmacSha256State));
    Signature::HmacSha256Intro((Signature::HmacSha256State*)m_HashState,
        Signature::LWRRENT_SIGNATURE.Prefix);
}

void FileSink::SetRenameOnClose(const string& fileName)
{
    m_renameOnClose = true;
    m_renameOnCloseName = fileName;
}

string FileSink::GetRenameOnClose() const
{
    if (m_renameOnClose)
    {
        return m_renameOnCloseName;
    }
    return string();
}

bool FileSink::IsFileOpen() const
{
    return (m_pFile != nullptr);
}

bool FileSink::PrintMleHeader()
{
    if (!IsFileOpen())
    {
        return false;
    }

    return DoPrintMleHeader();
}

void FileSink::CacheOutput(const char* pStr, size_t size)
{
    constexpr size_t maxCacheSize = 4096u;

    Tasker::MutexHolder mh;
    if (m_Mutex.get())
    {
        mh.Acquire(m_Mutex.get());
    }

    if (m_TemporaryCache.size() + size < maxCacheSize)
    {
        m_TemporaryCache.append(pStr, size);
    }
}

void FileSink::PrintCachedOutput()
{
    string cachedOutput;
    cachedOutput.swap(m_TemporaryCache);

    DoPrint(cachedOutput.data(), cachedOutput.size(), Tee::SPS_NORMAL);
}

void FileSink::StartRecordingModsHeader()
{
    m_ModsHeaderRecordingActive = true;
}

void FileSink::StopRecordingModsHeader()
{
    m_ModsHeaderRecordingActive = false;
}

void FileSink::PrintModsHeader()
{
    Print(m_ModsHeaderText.data(), m_ModsHeaderText.size());
}

RC FileSink::SwitchFilename(const string &fileName)
{
    RC rc;

    CHECK_RC(Open(m_ModeMask, fileName));

    if (m_ModeMask & MM_MLE)
    {
        PrintMleHeader();
    }

    return rc;
}
