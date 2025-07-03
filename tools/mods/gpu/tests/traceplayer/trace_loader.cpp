/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "trace_loader.h"
#include "trace_surface_accessor.h"
#include "core/include/fileholder.h"
#include "core/include/lz4.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "core/include/tasker.h"
#include "core/include/xp.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/surfrdwr.h"
#include "decrypt.h"
#include <algorithm>

namespace
{
    template<typename T>
    class ByteLoadHandler
    {
        public:
            explicit ByteLoadHandler(T* pData)
            : m_pData(pData)
            {
                MASSERT(pData);
            }
            void SetSize(size_t size)
            {
                m_pData->resize(size);
            }
            static bool OnlySize()
            {
                return false;
            }
            RC WriteBytes(const void* pBuffer, size_t offset, size_t size)
            {
                MASSERT(offset + size <= m_pData->size());
                memcpy(&(*m_pData)[offset], pBuffer, size);
                return RC::OK;
            }

        private:
            T* m_pData;
    };
    using StringLoadHandler = ByteLoadHandler<string>;
    using BinaryLoadHandler = ByteLoadHandler<vector<UINT08>>;

    class SizeLoadHandler
    {
        public:
            explicit SizeLoadHandler(size_t* pSize)
            : m_pSize(pSize)
            {
                MASSERT(pSize);
            }
            void SetSize(size_t size)
            {
                *m_pSize = size;
            }
            static bool OnlySize()
            {
                return true;
            }
            RC WriteBytes(const void* pBuffer, size_t offset, size_t size)
            {
                MASSERT(!"Size loader does not support reading surfaces");
                return RC::SOFTWARE_ERROR;
            }

        private:
            size_t* m_pSize;
    };

    class SurfaceLoadHandler
    {
        public:
            explicit SurfaceLoadHandler(Surface2D* pSurf)
            : m_Writer(*pSurf, 0), m_MaxSize(pSurf->GetArrayPitch())
            {
            }
            void SetSize(size_t size)
            {
                MASSERT(size == m_MaxSize);
            }
            static bool OnlySize()
            {
                return false;
            }
            RC WriteBytes(const void* pBuffer, size_t offset, size_t size)
            {
                MASSERT(offset + size <= m_MaxSize);
                return m_Writer.WriteBytes(offset, pBuffer, size);
            }

        private:
            SurfaceUtils::MappingSurfaceWriter m_Writer;
            size_t                             m_MaxSize;
    };

    class SurfaceAccessorHandler
    {
        public:
            explicit SurfaceAccessorHandler
            (
                Surface2D* pSurf,
                MfgTracePlayer::SurfaceAccessor *pSurfaceAccessor
            )
            : m_pSurfaceAccessor(pSurfaceAccessor), m_pSurface(pSurf)
            {
            }
            void SetSize(size_t size)
            {
                MASSERT(size == m_pSurface->GetArrayPitch());
            }
            static bool OnlySize()
            {
                return false;
            }
            RC WriteBytes(const void* pBuffer, size_t offset, size_t size)
            {
                MASSERT(offset + size <= m_pSurface->GetArrayPitch());
                return m_pSurfaceAccessor->WriteBytes(m_pSurface, offset, pBuffer, size);
            }

        private:
            MfgTracePlayer::SurfaceAccessor   *m_pSurfaceAccessor;
            Surface2D                         *m_pSurface;
    };

    bool FileExists(const string& filePath)
    {
        return Xp::DoesFileExist(Utility::DefaultFindFile(filePath, true));
    }

    // Used for reading files when the entire contents of the file is
    // loaded into memory
    class FullFileReader
    {
    public:
        FullFileReader(const string& filename, bool bEncrypted) :
            m_Filename(filename), m_bEncrypted(bEncrypted) { }
        FullFileReader() = delete;
        FullFileReader(const FullFileReader &)= delete;
        FullFileReader & operator=(const FullFileReader &)= delete;

        ~FullFileReader()
        {
            // Do not leave it decrypted in memory
            if (!m_FileData.empty())
            {
                memset(&m_FileData[0], 0, m_FileData.size());
            }
            m_FileData.clear();
        }

        RC Initialize
        (
            TarArchive * pArchive,
            const string & path,
            const string & traceBasename
        );
        size_t GetFileSize() { MASSERT(m_bInitialized); return m_FileSize; }
        RC GetTotalSize(size_t * pTotalSize);
        RC GetFileData(const UINT08 **ppFileData);
    private:
        const UINT08 * GetLwrrentFileData();

        bool            m_bInitialized  = false;
        string          m_Filename;
        bool            m_bEncrypted    = false;
        bool            m_bIsLz4        = false;
        const TarFile * m_pTarFile      = nullptr;
        size_t          m_FileSize      = 0;
        size_t          m_HeaderSize    = static_cast<size_t>(1_KB);
        size_t          m_DecryptedSize = 0;
        FileHolder      m_FileHolder;
        vector<UINT08>  m_FileData;
    };

    // Initialize the file reader data members and read in enough of the file so
    // that header information may be accessed
    RC FullFileReader::Initialize
    (
        TarArchive * pArchive,
        const string & path,
        const string & traceBasename
    )
    {
        RC rc;

        MASSERT(m_bEncrypted || pArchive);

        if (m_bEncrypted)
        {
            m_bIsLz4 = (m_Filename.size() > 5) ?
                               (m_Filename.compare(m_Filename.size() - 5, 5, ".lz4e") == 0) :
                               false;
        }
        else
        {
            m_bIsLz4 = (m_Filename.size() > 4) ?
                               (m_Filename.compare(m_Filename.size() - 4, 4, ".lz4") == 0) :
                               false;
        }

        const char * pTarFileData = nullptr;
        if (pArchive)
        {
            // Trace opens tar archives with "bigmem" which means the entire contents
            // of the archive is split into files and loaded into memory when the
            // archive is read
            m_pTarFile = pArchive->Find(traceBasename + "/" + m_Filename);
            if (!m_pTarFile)
            {
                return RC::FILE_DOES_NOT_EXIST;
            }

            m_pTarFile->Seek(0);
            if (m_pTarFile->Read(&pTarFileData) == 0 || !pTarFileData)
            {
                Printf(Tee::PriError,
                       "Failed to read file '%s' from trace, while looking"
                       " for the file header.\n",
                       m_Filename.c_str());
                return RC::FILE_READ_ERROR;
            }
            m_FileSize = m_pTarFile->GetSize();
            m_HeaderSize = min(m_FileSize, m_HeaderSize);
        }
        else
        {
            const string filePath = path.empty() ?
                m_Filename :
                Utility::CombinePaths(path.c_str(), m_Filename.c_str());

            // FileHolder::Open prints an error message if it fails to open the file.
            // To avoid polluting the output, we need to check whether the file
            // exists before trying to open it.
            if (!FileExists(filePath))
            {
                return RC::FILE_DOES_NOT_EXIST;
            }

            CHECK_RC(m_FileHolder.Open(filePath, "rb"));

            // Read the size of the file
            fseek(m_FileHolder.GetFile(), 0, SEEK_END);
            const auto size = ftell(m_FileHolder.GetFile());
            m_FileSize = UNSIGNED_CAST(size_t, size);
            m_HeaderSize = min(m_FileSize, m_HeaderSize);

            fseek(m_FileHolder.GetFile(), 0, SEEK_SET);

            // Read enough to determine the encrypted file size and, if the file is an
            // encrypted lz4 file, to read the decompressed size from the lz4 compression
            m_FileData.resize(m_HeaderSize);
            const auto numRead = fread(&(m_FileData[0]),
                                       1,
                                       m_HeaderSize,
                                       m_FileHolder.GetFile());

            if (numRead == 0)
            {
                Printf(Tee::PriError,
                       "Failed to read file '%s' from trace, while looking"
                       " for the file header.\n",
                       m_Filename.c_str());
                return RC::FILE_READ_ERROR;
            }
        }

        m_DecryptedSize = m_FileSize;
        if (m_bEncrypted)
        {
            CHECK_RC(Utility::InterpretModsUtilErrorCode(
                Decryptor::GetDecryptedSize(
                    m_pTarFile ? reinterpret_cast<const UINT08 *>(pTarFileData) : &(m_FileData[0]),
                    m_HeaderSize,
                    &m_DecryptedSize)));

            // File is not really encrypted
            if (m_DecryptedSize == 0)
            {
                m_DecryptedSize = m_FileSize;
            }
        }

        m_bInitialized = true;

        return RC::OK;
    }

    RC FullFileReader::GetTotalSize(size_t * pTotalSize)
    {
        MASSERT(m_bInitialized);

        const UINT08 * pFileData = GetLwrrentFileData();

        if (m_bIsLz4)
        {
            RC rc;

            vector<UINT08> tempBuffer;
            const UINT08 * pLz4Data = pFileData;
            if (m_bEncrypted)
            {
                // Handle the case where the file was lz4 compressed and then encrypted
                // Decrypt the header information into a temporary buffer so that the
                // full lz4 deompressed size can be queried
                rc = Utility::InterpretModsUtilErrorCode(
                    Decryptor::DecryptTraceDataArray(pFileData,
                                                     m_HeaderSize,
                                                     &tempBuffer));
                if (RC::OK != rc)
                {
                    Printf(Tee::PriError,
                           "Failed to read file '%s' from trace, when reading header\n",
                           m_Filename.c_str());
                    return RC::FILE_READ_ERROR;
                }
                pLz4Data = &tempBuffer[0];
            }

            Printf(Tee::PriLow, "Getting lz4 file size...\n");
            CHECK_RC(Lz4::GetDecompressedSize(pLz4Data, m_FileSize, pTotalSize));
            if (*pTotalSize == 0)
            {
                Printf(Tee::PriError,
                       "Failed to read size of lz4 file '%s' from trace. "
                       "Please make sure the file was compressed with "
                       "--content-size flag\n",
                       m_Filename.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (m_bEncrypted)
        {
            *pTotalSize = m_DecryptedSize;
        }
        else
        {
            *pTotalSize = m_FileSize;
        }
        return RC::OK;
    }

    RC FullFileReader::GetFileData(const UINT08 **ppFileData)
    {
        MASSERT(m_bInitialized);

        vector<UINT08> decryptedData;
        DEFER
        {
            if (!decryptedData.empty())
            {
                memset(&decryptedData[0], 0, decryptedData.size());
            }
        };

        m_FileData.clear();
        if (m_bEncrypted)
        {  
            // With LZ4 compression it is necessary to use a temporary buffer to
            // decrypt data into which will be decompressed (after decryption) into
            // m_FileData
            vector<UINT08> * pDecryptedData = m_bIsLz4 ? &decryptedData : &m_FileData;
            if (m_pTarFile)
            {
                // Tar files have already read the data into memory
                const RC rc = Utility::InterpretModsUtilErrorCode(
                    Decryptor::DecryptTraceDataArray(GetLwrrentFileData(),
                                                     m_FileSize,
                                                     pDecryptedData));
                if (RC::OK != rc)
                {
                    Printf(Tee::PriError,
                           "Failed to read file '%s' from trace\n",
                           m_Filename.c_str());
                    return RC::FILE_READ_ERROR;
                }
            }
            else
            {
                // Read file contents, at this point the file contents of the tar file are
                // already completely in memory because the trace opens the tar file with
                // bigmem (which fully loads all files in the archive into memory when the
                // archive is read)
                fseek(m_FileHolder.GetFile(), 0, SEEK_SET);

                m_FileData.clear();
                m_FileData.resize(m_FileSize);
                const size_t numRead = fread(&(m_FileData[0]), 1, m_FileSize, m_FileHolder.GetFile());

                if (numRead != m_FileSize)
                {
                    return RC::FILE_READ_ERROR;
                }

                const RC rc =  Utility::InterpretModsUtilErrorCode(
                    Decryptor::DecryptTraceDataArray(&m_FileData[0],
                                                     m_FileSize,
                                                     pDecryptedData));

                if (RC::OK != rc)
                {
                    Printf(Tee::PriError,
                           "Failed to read file '%s' from trace\n",
                           m_Filename.c_str());
                    return RC::FILE_READ_ERROR;
                }
            }
        }

        if (m_bIsLz4)
        {
            RC rc;
            m_FileData.clear();
            CHECK_RC(Lz4::Decompress(m_bEncrypted ? &decryptedData[0] : GetLwrrentFileData(),
                                     m_DecryptedSize,
                                     &m_FileData));

            *ppFileData = &m_FileData[0];
        }
        else if (m_bEncrypted)
        {
            *ppFileData = &m_FileData[0];
        }
        else if (m_pTarFile)
        {
            m_pTarFile->Seek(0);
            m_pTarFile->Read(reinterpret_cast<const char **>(ppFileData));
        }
        return RC::OK;
    }

    const UINT08 * FullFileReader::GetLwrrentFileData()
    {
        MASSERT(m_bInitialized);

        if (m_pTarFile)
        {
            const char * pData = nullptr;
            m_pTarFile->Seek(0);
            m_pTarFile->Read(&pData);
            return reinterpret_cast<const UINT08 *>(pData);
        }
        return &m_FileData[0];
    }
}

RC MfgTracePlayer::Loader::SetTraceLocation(const string& path)
{
    MASSERT(!m_bInitialized);

    // Strip test.hdr file from the path
    const string testHdr = "test.hdr";
    const size_t testHdrPos = path.size() - testHdr.size();
    if (path.size() >= testHdr.size() &&
        path.substr(testHdrPos) == testHdr &&
        (path.size() == testHdr.size() || path[testHdrPos-1] == '/' || path[testHdrPos-1] == '\\'))
    {
        if (path.size() > testHdr.size() + 1)
        {
            m_Path = path.substr(0, testHdrPos - 1);
        }
    }
    else
    {
        // Distinguish directory from file
        const string filePath = Utility::CombinePaths(path.c_str(), "test.hdr");
        const string encFilePath = Utility::CombinePaths(path.c_str(), "test.hde");
        if (FileExists(filePath) || FileExists(encFilePath))
        {
            m_Path = path;
        }
        // No trace header - treat as a tgz file
        else
        {
            m_Path = Utility::DefaultFindFile(path, true);
            m_bTar = true;
            bool loaded = false;
            {
                Tasker::DetachThread detach;
                static constexpr bool oneMemBlockPerFile = true;
                loaded = m_Archive.ReadFromFile(m_Path, oneMemBlockPerFile);
            }
            if (!loaded)
            {
                Printf(Tee::PriError, "Failed to read trace \"%s\"\n",
                       path.c_str());
                return RC::FILE_READ_ERROR;
            }

            // To support traces compressed with directory, determine trace's basename
            const size_t pathSepPos  = path.find_last_of("/\\");
            const size_t basenamePos = (pathSepPos == string::npos) ? 0 : (pathSepPos + 1);
            const size_t dotPos      = path.find_last_of('.');
            const bool   dotPosValid = (dotPos != string::npos) && (dotPos > basenamePos);
            const size_t endPos      = dotPosValid ? dotPos : path.size();
            m_TraceBasename          = path.substr(basenamePos, endPos - basenamePos);
        }
    }
    m_bInitialized = true;
    return OK;
}

RC MfgTracePlayer::Loader::GetFileSize(const string& filename, size_t* pSize)
{
    SizeLoadHandler handler(pSize);
    return LoadFile(filename, handler, false);
}

RC MfgTracePlayer::Loader::LoadFile(const string& filename, vector<UINT08>* pBuffer)
{
    BinaryLoadHandler handler(pBuffer);
    return LoadFile(filename, handler, false);
}

RC MfgTracePlayer::Loader::LoadFile(const string& filename, string* pBuffer)
{
    StringLoadHandler handler(pBuffer);
    return LoadFile(filename, handler, false);
}

RC MfgTracePlayer::Loader::LoadFile(const string& filename, Surface2D* pSurf)
{
    SurfaceLoadHandler handler(pSurf);
    return LoadFile(filename, handler, false);
}

RC MfgTracePlayer::Loader::LoadFile
(
    const string& filename,
    Surface2D* pSurf,
    SurfaceAccessor *pSuraceAccessor
)
{
    SurfaceAccessorHandler handler(pSurf, pSuraceAccessor);
    return LoadFile(filename, handler, false);
}

RC MfgTracePlayer::Loader::GetEncryptedFileSize(const string& filename, size_t* pSize)
{
    SizeLoadHandler handler(pSize);
    return LoadFile(filename, handler, true);
}

RC MfgTracePlayer::Loader::LoadEncryptedFile(const string& filename, vector<UINT08>* pBuffer)
{
    BinaryLoadHandler handler(pBuffer);
    return LoadFile(filename, handler, true);
}

RC MfgTracePlayer::Loader::LoadEncryptedFile(const string& filename, string* pBuffer)
{
    StringLoadHandler handler(pBuffer);
    return LoadFile(filename, handler, true);
}

RC MfgTracePlayer::Loader::LoadEncryptedFile(const string& filename, Surface2D* pSurf)
{
    SurfaceLoadHandler handler(pSurf);
    return LoadFile(filename, handler, true);
}

RC MfgTracePlayer::Loader::LoadEncryptedFile
(
    const string& filename,
    Surface2D* pSurf,
    SurfaceAccessor *pSuraceAccessor
)
{
    SurfaceAccessorHandler handler(pSurf, pSuraceAccessor);
    return LoadFile(filename, handler, true);
}

template<typename LoadHandler>
RC MfgTracePlayer::Loader::LoadFile(const string& filename, LoadHandler& handler, bool bEncrypted)
{
    MASSERT(m_bInitialized);
    RC rc;

    // Tar files are fully in memory because the archive is opened with "bigmem" and the
    // current decryptor does not support streaming decryption so it is necessary to load
    // the full file in memory to decrypt it
    const bool bFullFileInMemory = (m_bTar || bEncrypted);

    if (bFullFileInMemory)
    {
        FullFileReader fileReader(filename, bEncrypted);
        CHECK_RC(fileReader.Initialize(m_bTar ? &m_Archive : nullptr, m_Path, m_TraceBasename));

        size_t totalSize;
        CHECK_RC(fileReader.GetTotalSize(&totalSize));

        if (fileReader.GetFileSize() == 0 || totalSize == 0)
        {
            Printf(Tee::PriError, "File '%s' in trace is empty\n",
                   filename.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }

        handler.SetSize(totalSize);

        if (handler.OnlySize())
        {
            return RC::OK;
        }

        const UINT08 * pFileData = nullptr;
        CHECK_RC(fileReader.GetFileData(&pFileData));
        CHECK_RC(handler.WriteBytes(pFileData, 0, totalSize));
    }
    else
    {
        const bool isLz4 = (filename.size() > 4) ?
                           (filename.compare(filename.size() - 4, 4, ".lz4") == 0) :
                           false;

        static constexpr size_t pieceSize = 1024U*1024U;

        // Open the file
        const string filePath = m_Path.empty() ?
            filename :
            Utility::CombinePaths(m_Path.c_str(), filename.c_str());

        // FileHolder::Open prints an error message if it fails to open the file.
        // To avoid polluting the output, we need to check whether the file
        // exists before trying to open it.
        if (!FileExists(filePath))
        {
            return RC::FILE_DOES_NOT_EXIST;
        }

        FileHolder file;
        CHECK_RC(file.Open(filePath, "rb"));

        // Read the size of the file
        fseek(file.GetFile(), 0, SEEK_END);
        const auto size = ftell(file.GetFile());
        const size_t fileSize = UNSIGNED_CAST(size_t, size);
        size_t totalSize = fileSize;
        if (isLz4)
        {
            Printf(Tee::PriLow, "Getting lz4 file size...\n");
            // Get the decompressed size from the lz4 header
            const size_t maxHeaderSize = Lz4::MAX_HEADER_SIZE;
            string piece(maxHeaderSize, '\0');
            fseek(file.GetFile(), 0, SEEK_SET);
            const auto numRead = fread(&(piece[0]), 1, maxHeaderSize, file.GetFile());
            if (numRead == 0)
            {
                Printf(Tee::PriError,
                       "Failed to read lz4 file '%s' from trace, while looking"
                       " for the file header.\n",
                       filename.c_str());
                return RC::FILE_READ_ERROR;
            }
            CHECK_RC(Lz4::GetDecompressedSize(reinterpret_cast<const UINT08*>(piece.data()),
                                              fileSize, &totalSize));
            if (totalSize == 0)
            {
                Printf(Tee::PriError,
                       "Failed to read size of lz4 file '%s' from trace. "
                       "Please make sure the file was compressed with "
                       "--content-size flag\n",
                       filename.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }

        if (fileSize == 0 || totalSize == 0)
        {
            Printf(Tee::PriError, "File '%s' in trace is empty\n",
                   filename.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }

        handler.SetSize(totalSize);

        if (handler.OnlySize())
        {
            return RC::OK;
        }

        string piece(pieceSize, '\0');

        // Read file contents
        fseek(file.GetFile(), 0, SEEK_SET);
        size_t totalRead = 0;
        size_t totalWritten = 0;
        Lz4 lz4Decompressor;
        while (totalRead < static_cast<unsigned long>(fileSize))
        {
            const size_t numLeft  = fileSize - totalRead;
            const size_t readSize = min(numLeft, pieceSize);
            const size_t numRead = fread(&(piece[0]), 1, readSize, file.GetFile());

            if (numRead != readSize)
            {
                Printf(Tee::PriError,
                       "Failed to read contents of file '%s' from trace\n",
                       filename.c_str());
                return RC::FILE_READ_ERROR;
            }

            if (isLz4)
            {
                Printf(Tee::PriLow, "Decoding lz4 file...\n");

                std::vector<UINT08> decompressed;
                CHECK_RC(lz4Decompressor.StreamDecompress(
                                reinterpret_cast<const UINT08*>(piece.data()),
                                numRead, &decompressed));
                CHECK_RC(handler.WriteBytes(decompressed.data(),
                                            totalWritten,
                                            decompressed.size()));
                totalWritten += decompressed.size();
            }
            else
            {
                CHECK_RC(handler.WriteBytes(piece.data(), totalRead, numRead));
                totalWritten += numRead;
            }
            totalRead += numRead;
        }

        if (totalWritten != totalSize)
        {
            Printf(Tee::PriError,
                    "Mismatch between expected object size (%zu bytes) and "
                    "actual object size (%zu) retrieved from trace file '%s'\n",
                    totalSize,
                    totalWritten,
                    filename.c_str());
            return RC::FILE_READ_ERROR;
        }
    }
    return RC::OK;
}

RC MfgTracePlayer::Loader::Exists(const string& filename) const
{
    return (m_Archive.Find(filename) != nullptr);
}
