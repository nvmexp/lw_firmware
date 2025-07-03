/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2010,2012-2013,2015-2016, 2019 by LWPU Corporation.  All 
 * rights reserved.  All information contained herein is proprietary and 
 * confidential to LWPU Corporation.  Any use, reproduction, or disclosure 
 * without the written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   tracefil.cpp
 * @brief  class TraceFileManager
 *
 * Helper for class TracePlayer, for managing files and tar archives.
 */
#include "tracefil.h"
#include "core/include/tar.h"
#include "core/include/tario2.h"
#include "core/include/unzip.h"
#include "core/include/utility.h"
#include <stdio.h>
#include <set>
#include <map>
#include <sys/stat.h>
#include "mdiag/utl/utl.h"
#include "mdiag/sysspec.h"

//------------------------------------------------------------------------------

class ArchiveWrapper
{
public:

    typedef void* Handle;

    static ArchiveWrapper* CreateWrapper(bool speed);

    virtual ~ArchiveWrapper() { }
    virtual bool Initialize(const string& TarArchiveName) = 0;
    virtual RC OpenArchive(const string& filename, Handle* pRetHandle) = 0;
    virtual size_t Size(Handle h) = 0;
    virtual RC Read(void* pDest, Handle h) = 0;
    virtual RC FindArchive(const string& filename, size_t* pSize) = 0;

};

class InMemoryTarWrapper : public ArchiveWrapper
{
public:

    InMemoryTarWrapper() : m_pTarArchive(0) {}

    virtual ~InMemoryTarWrapper() { delete m_pTarArchive; }

    bool Initialize(const string& TarArchiveName)
    {
        m_pTarArchive = new TarArchive();
        if (!m_pTarArchive->ReadFromFile(TarArchiveName))
        {
            delete m_pTarArchive;
            m_pTarArchive = 0;
            return false;
        }
        DebugPrintf("Opened TarArchive %s.\n", TarArchiveName.c_str());
        return true;
    }

    RC OpenArchive(const string& filename, Handle* pRetHandle)
    {
        void* ptf = m_pTarArchive->Find(filename);
        if (ptf)
        {
            DebugPrintf("Found %s in TarArchive.\n", filename.c_str());
            *pRetHandle = (Handle)ptf;
            return OK;
        }
        else
        {
            return RC::FILE_EXIST;
        }
    }

    size_t Size(Handle h)
    {
        TarFile * ptf = (TarFile *)h;
        return ptf->GetSize();
    }

    RC Read(void* pDest, Handle h)
    {
        TarFile * ptf = (TarFile *)h;
        ptf->Seek(0); // move to beginning
        ptf->Read((char *)pDest, ptf->GetSize());
        return OK;
    }

    RC FindArchive(const string& filename, size_t* pSize)
    {
        TarFile* ptf = m_pTarArchive->Find(filename);
        if (ptf)
        {
            *pSize = ptf->GetSize();
            return OK;
        }
        return RC::FILE_EXIST;
    }

private:

    TarArchive* m_pTarArchive;

};

class LowMemoryTarWrapper : public ArchiveWrapper
{
public:

    LowMemoryTarWrapper() : m_pTapeArchive2(0) {}

    virtual ~LowMemoryTarWrapper() { delete m_pTapeArchive2; }

    bool Initialize(const string& TarArchiveName)
    {
        FILE* pf = fopen(TarArchiveName.c_str(), "rb");
        if (!pf)
        {
            return false;
        }
        else
        {
            fclose(pf);
            m_pTapeArchive2 = new TapeArchive2(TarArchiveName.c_str(), "r");
        }
        DebugPrintf("Opened TarArchive %s.\n", TarArchiveName.c_str());
        return true;
    }

    RC OpenArchive(const string& filename, Handle* pRetHandle)
    {
        void* ptf = m_pTapeArchive2->fopen(filename.c_str(), "r");
        if (ptf)
        {
            DebugPrintf("Found %s in TarArchive.\n", filename.c_str());
            *pRetHandle = (Handle)ptf;
            return OK;
        }
        else
        {
            return RC::FILE_EXIST;
        }
    }

    size_t Size(Handle h)
    {
        TarFile2 * ptf = (TarFile2 *)h;
        return ptf->get_size();
    }

    RC Read(void* pDest, Handle h)
    {
        TarFile2 * ptf = (TarFile2 *)h;
        ptf->fseek(0); // move to beginning
        Z_SIZE_T size = ptf->get_size();
        Z_SIZE_T readSize = 0;
        Z_SIZE_T offset = 0;
        while(size - offset > INT_MAX)
        {
            readSize = ptf->fread((char *)pDest + offset, INT_MAX);
            // Can't read the next part if failed
            if (readSize != INT_MAX)
            {
                ErrPrintf("Not fully read the file, aborted\n", size);
                return RC::FILE_READ_ERROR;
            }
            offset += readSize;
        }
        // Read the remaining part
        readSize = ptf->fread((char *)pDest + offset, size - offset);
        // Check file last part read, give error if not match
        if (readSize != size - offset)
        {
            ErrPrintf("File not fully read, may cause by parent tar file is closed or it's not open for reading!\n");
            return RC::FILE_READ_ERROR;
        }
        return OK;
    }

    RC FindArchive(const string& filename, size_t* pSize)
    {
        TarFile2* ptf = m_pTapeArchive2->fopen(filename.c_str(), "r");
        if (ptf)
        {
            *pSize = ptf->get_size();
            m_pTapeArchive2->fclose(ptf);
            return OK;
        }
        return RC::FILE_EXIST;
    }

private:

    TapeArchive2* m_pTapeArchive2;

};

ArchiveWrapper* ArchiveWrapper::CreateWrapper(bool speed)
{
    if (speed)
    {
        return new InMemoryTarWrapper;
    }
    else
    {
        return new LowMemoryTarWrapper;
    }
}

struct ZipFileData
{
    unz_file_pos* pos;
    size_t        size;
};

struct TraceFileData
{
    string       Path;
    string       TarArchiveName;
    ArchiveWrapper* pArchiveWrapper;
    bool         ArchiveFirst;
    set<FILE*>   OpenFiles;
    unzFile      uf;
    bool         IsZip;
    map<string, ZipFileData*>   ZipMap;
};

//------------------------------------------------------------------------------
TraceFileMgr::TraceFileMgr()
{
    m_D = new TraceFileData;
    m_D->pArchiveWrapper = 0;
    m_D->ArchiveFirst = false;
    m_D->IsZip = false;
    m_pTest = nullptr;
}

//------------------------------------------------------------------------------
TraceFileMgr::~TraceFileMgr()
{
    ShutDown();
    delete m_D;
}

//------------------------------------------------------------------------------
// Remember the directory part of the tracepath, try to open the .tgz file.
RC TraceFileMgr::Initialize(string tracepath, bool speed, Test* pTest)
{
    m_pTest = pTest;
    // Remember the directory part of the name of the /foo/bar/baz/test.hdr.
    size_t pos = tracepath.find_last_of("/\\");
    if (pos == string::npos)
        m_D->Path = "";
    else
        m_D->Path = tracepath.substr(0, pos+1);

    // Do we have a gzipped tar archive to search in addition to the directory?
    m_D->TarArchiveName = tracepath;
    if (m_D->TarArchiveName.rfind(".hdr") == string::npos)
    {
        ErrPrintf("%s : trace file is expected to have .hdr extension\n", tracepath.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    m_D->TarArchiveName.replace(m_D->TarArchiveName.rfind(".hdr"), 4, ".zip");

    m_D->uf = unzOpen(m_D->TarArchiveName.c_str());

    if (m_D->uf == NULL)
    {
        m_D->TarArchiveName.replace(m_D->TarArchiveName.rfind(".zip"), 4, ".tgz");

        m_D->pArchiveWrapper = ArchiveWrapper::CreateWrapper(speed);
        if (!m_D->pArchiveWrapper->Initialize(m_D->TarArchiveName))
        {
            delete m_D->pArchiveWrapper;
            m_D->pArchiveWrapper = 0;
        }
    }
    else
    {
        m_D->IsZip = true;
        DebugPrintf("Opened Zip %s.\n", m_D->TarArchiveName.c_str());
    }
    return OK;
}

//------------------------------------------------------------------------------
// Close all open FILEs and free the TapeArchive2.
void TraceFileMgr::ShutDown()
{
    DebugPrintf("TraceFileMgr::ShutDown() %u open FILE*, %u open TarArchives.\n",
                UINT32(m_D->OpenFiles.size()), UINT32(m_D->pArchiveWrapper? 1: 0));

    set<FILE*>::iterator i;
    for (i = m_D->OpenFiles.begin(); i != m_D->OpenFiles.end(); ++i)
    {
        fclose(*i);
    }
    m_D->OpenFiles.clear();

    if (m_D->pArchiveWrapper)
    {
        delete m_D->pArchiveWrapper;
        m_D->pArchiveWrapper = 0;
    }

    map<string, ZipFileData*>::iterator zipIt = m_D->ZipMap.begin();
    while (zipIt != m_D->ZipMap.end())
    {
        delete zipIt->second->pos;
        delete zipIt->second;
        zipIt++;
    }
    m_D->ZipMap.clear();
}

//------------------------------------------------------------------------------
// Tell TraceFileMgr whether to search TapeArchive2 then directory,
// or directory then TapeArchive2.
void TraceFileMgr::SearchArchiveFirst(bool tf)
{
    m_D->ArchiveFirst = tf;
}

//------------------------------------------------------------------------------
// Search the trace directory and the open TapeArchive2 for the
// file, return error code (OK on success) and the abstract handle.
// Files are opened read-only, binary.
RC TraceFileMgr::Open(string filename, Handle* pRetHandle)
{
    RC rc = OK;
    string baseFilename = filename;
    size_t pos = filename.find_last_of("/\\");
    if (pos != string::npos) {
        baseFilename.erase(0, pos + 1);
    }
    else
    {
        filename = m_D->Path + filename;
    }

    if (m_D->ArchiveFirst)
    {
        rc = OpenArchive(baseFilename, pRetHandle);
        if (rc != OK)
        {
            rc.Clear();
            rc = OpenDirectory(filename, pRetHandle);
        }
    }
    else
    {
        rc = OpenDirectory(filename, pRetHandle);
        if (rc != OK)
        {
            rc.Clear();
            rc = OpenArchive(baseFilename, pRetHandle);
        }
    }

    if (OK != rc)
    {
        ErrPrintf("Could not open %s or find it in %s.\n",
                  filename.c_str(), m_D->TarArchiveName.c_str());
    }
    else
    {
        m_MapHandleFileName[*pRetHandle] = baseFilename;
    }
    return rc;
}

//------------------------------------------------------------------------------
RC TraceFileMgr::OpenArchive(string filename, Handle* pRetHandle)
{
    if (m_D->IsZip)
    {
        map<string, ZipFileData*>::iterator zipIt =
            m_D->ZipMap.find(filename);
        if (zipIt != m_D->ZipMap.end())
        {
            *pRetHandle = (Handle)zipIt->second;
            return OK;
        }
        else if (UNZ_OK == unzLocateFile(m_D->uf, filename.c_str(), 1))
        {
            ZipFileData* zipFileData = new ZipFileData;
            zipFileData->pos = new unz_file_pos;
            unz_file_info file_info;
            if (UNZ_OK == unzGetLwrrentFileInfo(m_D->uf, &file_info,
                NULL, 0, NULL, 0, NULL, 0) &&
                UNZ_OK == unzGetFilePos(m_D->uf, zipFileData->pos))
            {
                const char* dummy = filename.c_str();
                DebugPrintf("Opened file %s.\n", dummy);
                zipFileData->size = file_info.uncompressed_size;
                m_D->ZipMap[filename] = zipFileData;
                *pRetHandle = (Handle)zipFileData;
                return OK;
            }
            else
            {
                delete zipFileData->pos;
                delete zipFileData;
            }
        }
    }
    else
    {
        if (m_D->pArchiveWrapper)
        {
            return m_D->pArchiveWrapper->OpenArchive(filename, pRetHandle);
        }
    }
    return RC::FILE_EXIST;
}

//------------------------------------------------------------------------------
RC TraceFileMgr::OpenDirectory(string filename, Handle* pRetHandle)
{
    FILE * pf = fopen(filename.c_str(), "rb");
    if (pf)
    {
        DebugPrintf("Opened %s from filesystem.\n", filename.c_str());
        m_D->OpenFiles.insert(pf);
        *pRetHandle = (Handle)pf;
        return OK;
    }
    return Utility::InterpretFileError();
}

//------------------------------------------------------------------------------
// Return the size in bytes of the file.
// If handle is not an open file, returns 0.
size_t TraceFileMgr::Size(Handle h)
{
    set<FILE*>::iterator i = m_D->OpenFiles.find((FILE*)h);
    if (i != m_D->OpenFiles.end())
    {
        fseek(*i, 0, SEEK_END);
        size_t sz = ftell(*i);
        fseek(*i, 0, SEEK_SET);
        return sz;
    }
    else
    {
        if (m_D->IsZip)
        {
            return ((ZipFileData*)h)->size;
        }
        else if (m_D->pArchiveWrapper)
        {
            return m_D->pArchiveWrapper->Size(h);
        }
    }
    return 0;
}

//------------------------------------------------------------------------------
// Trigger UTL event for loading a trace file
void TraceFileMgr::TriggerTraceFileReadEvent(void * pDest, Handle h)
{
    if (Utl::HasInstance())
    {
        if (m_MapHandleFileName.find(h) == m_MapHandleFileName.end())
        {
            MASSERT(!"File Handle not present in Open Handles list");
        }
        string fileName = m_MapHandleFileName[h];
        Utl::Instance()->TriggerTraceFileReadEvent(m_pTest, fileName, pDest, Size(h));
    }
}

//------------------------------------------------------------------------------
// Read the entire file, return error if there was a problem.
// The buffer is assumed to be the right size.
RC TraceFileMgr::Read(void * pDest, Handle h)
{
    set<FILE*>::iterator i = m_D->OpenFiles.find((FILE*)h);
    if (i != m_D->OpenFiles.end())
    {
        fseek(*i, 0, SEEK_END);
        size_t sz = ftell(*i);
        fseek(*i, 0, SEEK_SET);

        if (! fread(pDest, sz, 1, *i))
            return Utility::InterpretFileError();
        else
        {
            TriggerTraceFileReadEvent(pDest, h);
            return OK;
        }
    }
    else
    {
        if (m_D->IsZip)
        {
            ZipFileData* zipFileData = (ZipFileData*)h;
            if (UNZ_OK == unzGoToFilePos(m_D->uf, zipFileData->pos))
            {
                if (UNZ_OK == unzOpenLwrrentFile(m_D->uf))
                {
                    if ((int)zipFileData->size ==
                        unzReadLwrrentFile(m_D->uf,pDest,
                            UNSIGNED_CAST(unsigned int, zipFileData->size)))
                    {
                        TriggerTraceFileReadEvent(pDest, h);
                        return OK;
                    }
                }
            }
        }
        else if (m_D->pArchiveWrapper)
        {
            RC rc = OK;
            CHECK_RC(m_D->pArchiveWrapper->Read(pDest, h));
            TriggerTraceFileReadEvent(pDest, h);
            return OK;
        }
    }
    return RC::FILE_EXIST;
}

//------------------------------------------------------------------------------
// Close the FILE, or do nothing if it is a TarFile2.
void TraceFileMgr::Close(Handle h)
{
    set<FILE*>::iterator i = m_D->OpenFiles.find((FILE*)h);
    if (i != m_D->OpenFiles.end())
    {
        fclose(*i);
        m_D->OpenFiles.erase(i);
    }
}

//------------------------------------------------------------------------------
// Quickly check that the file exists, and get size.
RC TraceFileMgr::Find(string filename, size_t * pSize)
{
    RC rc = OK;
    string baseFilename = filename;
    size_t pos = filename.find_last_of("/\\");
    if (pos != string::npos)
    {
        baseFilename.erase(0, pos);
    }
    else
    {
        filename = m_D->Path + filename;
    }

    if (m_D->ArchiveFirst)
    {
        rc = FindArchive(baseFilename, pSize);
        if (rc != OK)
        {
            rc.Clear();
            rc = FindDirectory(filename, pSize);
        }
    }
    else
    {
        rc = FindDirectory(filename, pSize);
        if (rc != OK)
        {
            rc.Clear();
            rc = FindArchive(baseFilename, pSize);
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC TraceFileMgr::FindDirectory(string filename, size_t * pSize)
{
    struct stat statbuf;
    if (stat(filename.c_str(), &statbuf) >= 0)
    {
        *pSize = statbuf.st_size;
        return OK;
    }
    return Utility::InterpretFileError();
}

//------------------------------------------------------------------------------
RC TraceFileMgr::FindArchive(string filename, size_t * pSize)
{
    if (m_D->IsZip)
    {
        map<string, ZipFileData*>::iterator zipIt =
            m_D->ZipMap.find(filename);
        if (zipIt != m_D->ZipMap.end())
        {
            *pSize = zipIt->second->size;
            return OK;
        }
        else if (UNZ_OK == unzLocateFile(m_D->uf, filename.c_str(), 1))
        {
            ZipFileData* zipFileData = new ZipFileData;
            zipFileData->pos = new unz_file_pos;
            unz_file_info file_info;
            if (UNZ_OK == unzGetLwrrentFileInfo(m_D->uf, &file_info,
                NULL, 0, NULL, 0, NULL, 0) &&
                UNZ_OK == unzGetFilePos(m_D->uf, zipFileData->pos))
            {
                const char* dummy = filename.c_str();
                DebugPrintf("Found file %s.\n", dummy);
                zipFileData->size = file_info.uncompressed_size;
                m_D->ZipMap[filename] = zipFileData;
                *pSize = file_info.uncompressed_size;
                return OK;
            }
            else
            {
                delete zipFileData->pos;
                delete zipFileData;
            }
        }
    }
    else
    {
        if (m_D->pArchiveWrapper)
        {
            return m_D->pArchiveWrapper->FindArchive(filename, pSize);
        }
    }
    return RC::FILE_EXIST;
}

