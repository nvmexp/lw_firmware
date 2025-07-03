/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2010, 2013, 2017 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwdiagutils.h"
#include "sockosx.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
extern "C"
{
    #include <dlfcn.h>
}

//! \brief Returns the path delimiter for extracting paths from elw variables
//!
//! \return Path delimiter used in the environment
char LwDiagXp::GetElwPathDelimiter()
{
   return ':';
}

//! \brief Get an environment variable.
//!
//! \param Variable : Environment variable to get
//!
//! \return Value for the environment variable ("" if an error oclwred or the
//!         variable was not found
string LwDiagXp::GetElw
(
   string /* Variable */
)
{
   return "";
}

//! \brief Return true if file exists.
//!
//! \param strFilename : File to check for existence
//!
//! \return true if the file exists, false otherwise
bool LwDiagXp::DoesFileExist(string strFilename)
{
   bool bExists = false;
   struct stat statResult;
   int nResult = stat(strFilename.c_str(), &statResult);
   if (nResult == 0)
   {
      bExists = true;
   }
   return bExists;
}

//-----------------------------------------------------------------------------
//! \brief Open file.  Normally called by LwDiagUtils::OpenFile() wrapper.
//!
//! \param FileName : Filename to open
//! \param Mode     : Mode to open the file in
//!
//! \return Pointer to an open file (null if unsuccessful)
FILE *LwDiagXp::Fopen(const char *FileName, const char *Mode)
{
    return fopen(FileName, Mode);
}

//-----------------------------------------------------------------------------
//! \brief Create a new platform specific Socket.  The caller is responsible
//!        for deleting the socket
//!
//! \return Pointer to a platform specific Socket
Socket * LwDiagXp::CreateSocket()
{
   return static_cast<Socket *>(new SocketOsx);
}

//------------------------------------------------------------------------------
LwDiagUtils::EC LwDiagXp::LoadDynamicLibrary
(
    const string &fileName
   ,void **pModuleHandle
   ,UINT32 loadDLLFlags
)
{
#ifdef HOS
    MASSERT(!"Shared libraries are not supported on HOS!");
    return LwDiagUtils::UNSUPPORTED_FUNCTION;
#else
    *pModuleHandle = dlopen(fileName.c_str(), loadDLLFlags);
    if (*pModuleHandle == nullptr)
        return LwDiagUtils::DLL_LOAD_FAILED;
    return LwDiagUtils::OK;
#endif // !HOS
}

//------------------------------------------------------------------------------
LwDiagUtils::EC LwDiagXp::LoadDynamicLibrary
(
    const string &fileName
   ,void **pModuleHandle
)
{
    // Flags are unused on windows
    return LoadDynamicLibrary(fileName, pModuleHandle, RTLD_NOW | RTLD_GLOBAL);
}

//------------------------------------------------------------------------------
LwDiagUtils::EC LwDiagXp::UnloadDynamicLibrary(void * moduleHandle)
{
#ifdef HOS
    MASSERT(!"Shared libraries are not supported on HOS!");
    return LwDiagUtils::UNSUPPORTED_FUNCTION;
#else
    if (dlclose(moduleHandle) != 0)
        return LwDiagUtils::BAD_PARAMETER;
    return LwDiagUtils::OK;
#endif
}

//------------------------------------------------------------------------------
void * LwDiagXp::GetDynamicLibraryProc(void * moduleHandle, const char * funcName)
{
#ifdef HOS
    MASSERT(!"Shared libraries are not supported on HOS!");
    return nullptr;
#else
    return dlsym(moduleHandle, funcName);
#endif
}

//------------------------------------------------------------------------------
string LwDiagXp::GetDynamicLibrarySuffix()
{
    return ".so";
}

namespace LwDiagUtils
{
    namespace Path
    {
        const char separator = '/';

        bool IsSeparator(char c)
        {
            return separator == c;
        }

        string AppendSeparator(const string &path)
        {
            if (!path.empty() && !IsSeparator(*(path.end() - 1)))
            {
                return path + separator;
            }
            return path;
        }
    }
}
