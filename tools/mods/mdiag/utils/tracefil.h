/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2016, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   tracefil.h
 * @brief  class TraceFileManager
 *
 * Helper for class TracePlayer, for managing files and tar archives.
 */
#ifndef INCLUDED_TRACEFIL_H
#define INCLUDED_TRACEFIL_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif
#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif

struct TraceFileData;  // defined in tracefil.cpp
class Test;

//------------------------------------------------------------------------------
// TraceFileMgr -- does file searches in the trace directory and the (possible)
// compressed TAR file, and manages the lifespan of the open TAR file.
//
// The messy problem we're trying to hide from the rest of the trace player is
// that the .tgz file must be entirely decompressed into memory on open before
// we can search it.  This takes a lot of CPU time on open.
// So long as it is open, it consumes a very large amount of memory.
//
// A subdirectory full of individually-compressed files with a separate index
// file that listed name, path, and size would be a much more efficient way to
// store traces, as we wouldn't need to decompress anything until the last
// minute, and we wouldn't need to keep all that data in memory.
//
class TraceFileMgr
{
public:
    TraceFileMgr();
    ~TraceFileMgr();

    // Opaque handle used by our clients (actually a FILE* or TarFile*).
    typedef void* Handle;

    // Remember the directory part of the tracepath, try to open the .tgz file.
    RC Initialize(string tracepath, bool speed, Test* pTest);

    // Tell TraceFileMgr whether to search TarArchive then directory,
    // or directory then TarArchive.
    void SearchArchiveFirst(bool tf);

    // Close all open FILEs and free the TarArchive.
    void ShutDown();

    // Search the trace directory and the open TarArchive for the
    // file, return error code (OK on success) and the abstract handle.
    // Files are opened read-only, binary.
    RC Open(string filename, Handle* pRetHandle);

    // Return the size in bytes of the file.
    // If handle is not an open file, returns 0.
    size_t Size(Handle h);

    // Read the entire file, return error if there was a problem.
    // The buffer is assumed to be the right size.
    RC Read(void * pDest, Handle h);

    // Close the FILE, or do nothing if it is a TarFile.
    void Close(Handle h);

    // Quickly check that the file exists, and get size.
    RC Find(string filename, size_t * pSize);

private:
    TraceFileData * m_D;
    Test* m_pTest;
    map<Handle, string> m_MapHandleFileName;

    RC OpenArchive(string filename, Handle* pRetHandle);
    RC OpenDirectory(string filename, Handle* pRetHandle);
    RC FindDirectory(string filename, size_t * pSize);
    RC FindArchive(string filename, size_t * pSize);
    void TriggerTraceFileReadEvent(void * pDest, Handle h);
};

#endif // INCLUDED_TRACEFIL_H
