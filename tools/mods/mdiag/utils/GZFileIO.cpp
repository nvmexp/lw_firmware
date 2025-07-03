/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015, 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "mdiag/sysspec.h"
#include "GZFileIO.h"
#include <assert.h>
#include "core/include/coreutility.h"

GZFileIO::GZFileIO()
{
    gz_fileHandle = Z_NULL;
}

GZFileIO::~GZFileIO()
{
    assert(gz_fileHandle == Z_NULL);
}

bool GZFileIO::FileOpen(const char* name, const char* mode)
{
    string str_mode(mode);
    if (str_mode.find('b') == string::npos)
    {
        str_mode += 'b'; // force to add 'b' since zip file only can be treated as binary
    }

    // zlib125 no more support "+" mode
    size_t plus_pos = str_mode.find('+');
    if (plus_pos != string::npos)
    {
        str_mode.erase(plus_pos);
    }

    DebugPrintf("Opening file '%s' in mode '%s'\n", name, str_mode.c_str());
    gz_fileHandle = gzopen(name, str_mode.c_str());
    if ( gz_fileHandle == Z_NULL || gz_fileHandle < 0) {
        return true;
    }
    return false;
}

void GZFileIO::FileClose()
{
    if ( gz_fileHandle )
        gzclose(gz_fileHandle);
    gz_fileHandle = Z_NULL;
}

UINT32 GZFileIO::FileWrite(const void* buffer, UINT32 size, UINT32 count)
{
    return gzwrite(gz_fileHandle, buffer, size*count);
}

bool GZFileIO::FileEOF()
{
    return (gzeof(gz_fileHandle) != 0);
}

char* GZFileIO::FileGetStr(char *string, int n)
{
    return gzgets(gz_fileHandle, string, n);
}

void GZFileIO::FilePrintf(const char *format, ...)
{
    va_list ap;
    int len = 0;
    const int buffsize = 1024;
    char buf[buffsize];

    va_start(ap, format);
    len = Utility::VsnPrintf(buf, buffsize, format, ap);
    va_end(ap);

    if (len > 0 && len <= buffsize)
    {
        gzwrite(gz_fileHandle, buf, len);
    }
    else
    {
        ErrPrintf("%s: print buffer overflow!!\n", __FUNCTION__);
        assert(0);
    }
}

int GZFileIO::FileGetLine( string& aline )
{
    // read in one line of text: keep reading until hit '\n' or eof
    char tmpbuf[1024];
    aline.erase( 0, string::npos );
    while( !FileEOF() )
    {
        tmpbuf[0] = '\0';
        FileGetStr( tmpbuf, 1024 );
        aline += tmpbuf;
        if( aline[aline.size()-1] == '\n' )
        {
            return aline.size();
        }
    }
    return aline.size();
}
