/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _GZFileIO_H_
#define _GZFileIO_H_

#include "core/include/zlib.h"
#include "mdiag/sysspec.h"

class GZFileIO: public FileIO {
public:
    GZFileIO();

    virtual ~GZFileIO();

    virtual bool FileOpen(const char* name, const char* mode);
    virtual void FileClose();
    virtual UINT32 FileWrite(const void* buffer, UINT32 size, UINT32 count);
    virtual char *FileGetStr( char *string, int n);
    virtual int FileGetLine( string& line );
    virtual void FilePrintf(const char *format, ...);

private:
    gzFile gz_fileHandle;

    bool FileEOF();
};

#endif // _GZFileIO_H_
