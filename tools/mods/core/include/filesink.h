/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2011,2013,2016-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// File sink.

#pragma once

#include "inc/bytestream.h"
#include "datasink.h"
#include "rc.h"
#include "random.h"
#include "encrypt_log.h"

/**
 * @class(FileSink)
 *
 * Print Tee's stream to a file.
 */

class FileSink : public DataSink
{
private:
    string m_FileName;
    string m_BaseFileName;
    UINT32 m_ModeMask;
    UINT32 m_SplitIndex;
    FILE * m_pFile;
    bool   m_AlwaysFlush;
    bool   m_FlushedSinceLastWrite;
    bool   m_Encrypt;
    bool   m_WriteBinary;

    Encryptor::LogSink m_EncryptedSink;

    bool   m_renameOnClose;
    string m_renameOnCloseName;

    string     m_TemporaryCache; // Prints cached before the log file is opened
    string     m_ModsHeaderText;
    ByteStream m_ModsHeaderBinary;
    bool       m_ModsHeaderRecordingActive;

    enum
    {
        NO_FILE_SIZE_LIMIT = 0,
        DEFAULT_FILS_SIZE_LIMIT_MB = 2048,
        PRINTS_PER_SIZE_CHECK = 100
    };

    UINT32     m_FileSizeLimitMb;
    UINT32     m_FileSizeSplitMb;
    UINT32     m_PrintsPerSizeCheck;
    UINT64     m_NumCharsInLwrrentSplit;

    void      *m_HashState;

    FileSink(const FileSink &);                  // not implemented
    FileSink & operator=(const FileSink &);      // not implemented

    void PrintEncrypted(const char* str, size_t size);
    void PrintEncryptedBinary(const UINT08* data, size_t size);
    void ResetRandom();
    void CloseIfLimitReached();
    void OpenNewFileOnSplit(size_t size);
    RC OpenSplit();
    RC CloseAndAcquireMutex(Tasker::MutexHolder* pMh);

    virtual void CacheOutput(const char* pStr, size_t size);
    void PrintCachedOutput();

public:
    // CREATORS
    FileSink();
    ~FileSink() override;

    // ACCESSORS
    string GetFileName() const;
    string GetFileNameOnly() const;
    bool   GetAlwaysFlush() const;
    bool   IsSinkOpen();

    // MANIPULATOR
    virtual void Flush();                     // flush to disk
    void SetAlwaysFlush(bool b);
    void SetEncrypt(bool e) { m_Encrypt = e; }
    void SetFileLimitMb(UINT32 FileSizeMb) {m_FileSizeLimitMb=FileSizeMb;};
    void SetFileSplitMb(UINT32 FileSizeMb) {m_FileSizeSplitMb=FileSizeMb;};

    void StartSigning();

    // TODO: Eventually remove this and replace with ModeMask
    enum Mode
    {
        Truncate,
        Append
    };

    enum ModeMask
    {
        MM_APPEND   = 0x1 << 0,
        MM_ENCRYPT  = 0x1 << 1,
        MM_MLE      = 0x1 << 2,
    };

    void SetRenameOnClose(const string& fileName);
    string GetRenameOnClose() const;

    // FileSink interface.
    RC Open(const string & fileName, Mode mode, bool encrypt);
    RC Open(const string & fileName, Mode mode);
    RC Open(UINT32 modeMask, const string & fileName);
    RC Close();
    RC Seek(long offset, int origin);
    RC Tell(long * pOffset);
    RC Read(vector<char> * pBuf, long numBytesToRead, long *pBytesRead);
    bool IsEncrypted() const { return m_Encrypt; }

    bool IsFileOpen() const;

    bool PrintMleHeader() override;

    void StartRecordingModsHeader();
    void StopRecordingModsHeader();
    void PrintModsHeader();
    RC SwitchFilename(const string &fileName);

protected:
    // DataSink override.
    void DoPrint(const char* str, size_t size, Tee::ScreenPrintState sps) override;
    bool DoPrintBinary(const UINT08* data, size_t size) override;
};
