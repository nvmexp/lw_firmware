/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   fileholder.h
 * @brief  RAII class for a file resource.
 *
 */
#ifndef INCLUDED_FILEHOLDER_H
#define INCLUDED_FILEHOLDER_H

#ifndef INCLUDED_LWDIAGUTILS_H
#include "lwdiagutils.h"
#endif
#ifndef INCLUDED_UTILITY_H
#include "utility.h"
#endif

//! LwDiagUtils::FileHolder is an RAII class to hold a file and ensure it
//! is closed when the object is destroyed.
//! Here we extend it to return RC instead of LwDiagUtils::EC.

class FileHolder : public LwDiagUtils::FileHolder
{
public:
    FileHolder()
        : LwDiagUtils::FileHolder()
    {
    }

    FileHolder(const string &FileName, const char* Attrib)
        : LwDiagUtils::FileHolder(FileName, Attrib)
    {
    }

    FileHolder(const char* FileName, const char* Attrib)
        : LwDiagUtils::FileHolder(FileName, Attrib)
    {
    }

    RC Open(const string &FileName, const char *Attrib)
    {
        return Utility::InterpretModsUtilErrorCode(
            LwDiagUtils::FileHolder::Open(FileName, Attrib));
    }

    RC Open(const char *FileName, const char *Attrib)
    {
        return Utility::InterpretModsUtilErrorCode(
            LwDiagUtils::FileHolder::Open(FileName, Attrib));
    }

    // Inherited methods (diag/utils/lwdiagutils.h)
    //   void Close()
    //   FILE* GetFile() const

private:
    // do not support assignment & copy
    FileHolder & operator=(const FileHolder&);
    FileHolder( const FileHolder &);
};

#endif // INCLUDED_FILEHOLDER_H
