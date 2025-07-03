/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2013, 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   lwdiagutils.h
 * @brief  Contains various utilities used in diag exelwtables.
 *
 */

#ifndef INCLUDED_LWDIAGUTILS_H
#define INCLUDED_LWDIAGUTILS_H

#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif
#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif
#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif
#include <cstdio>
#include <cstdarg>

using namespace std;

#ifdef DEBUG
   #define LWDASSERT(test) ((void)((test)||(LwDiagUtils::Assert(__FILE__,__LINE__,__FUNCTION__, #test),0)))
#else
   #define LWDASSERT(test) ((void)0)
#endif

#define CHECK_EC(f)                       \
    do {                                  \
        if (LwDiagUtils::OK != (ec = (f)))   \
            return ec;                    \
    } while (0)

#define FIRST_EC(f)                                 \
    do {                                            \
        const LwDiagUtils::EC tmpEcInFirstEc = (f); \
        if (ec == LwDiagUtils::OK)                  \
            ec = tmpEcInFirstEc;                    \
    } while (0)

// MIN,MAX,MINMAX : Note that args are evaluated multiple times.
// Beware of side-effects and perf issues with this.
// Inline functions might be better here.
#ifndef MIN
    #define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
    #define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MINMAX
    #define MINMAX(low, v, high) MAX(low, MIN(v,high))
#endif

class Socket;

//! The LwDiagUtils namespace.
namespace LwDiagUtils
{
    //! Types of file encryption that may be used
    enum EncryptionType
    {
        NOT_ENCRYPTED,
        ENCRYPTED_JS,
        ENCRYPTED_LOG,
        ENCRYPTED_JS_V2,
        ENCRYPTED_LOG_V2,
        ENCRYPTED_FILE_V3,
        ENCRYPTED_LOG_V3,
        ENCRYPTED_FILE_V3_BASE128,
    };

    //! Priority codes when using Printf
    //! Note that the priority scheme here should match that in baseline
    //! mods (core/include/tee.h) so that when MODS utilizes this library
    //! the prints generated here print correctly
    enum Priority
    {
        PriNone      = 0,
        PriDebug     = 1,
        PriLow       = 2,
        PriNormal    = 3,
        PriWarn      = 4,
        PriHigh      = 5,
        PriError     = 6,
        PriAlways    = 7,
        ScreenOnly   = 8,
        FileOnly     = 9,
        SerialOnly   = 10,
        CirlwlarOnly = 11,
        DebuggerOnly = 12,
        EthernetOnly = 13,
        MleOnly      = 14,
    };

    //! Enums necessary for printf's, note that these enums must match those
    //! in baseline mods (core/include/tee.h)
    enum
    {
        SPS_NORMAL = 0,
        ModuleNone = ~0
    };

    //! Error codes known by the utilities code
    enum EC
    {
        OK,
        FILE_2BIG,
        FILE_ACCES,
        FILE_AGAIN,
        FILE_BADF,
        FILE_BUSY,
        FILE_CHILD,
        FILE_DEADLK,
        FILE_EXIST,
        FILE_FAULT,
        FILE_FBIG,
        FILE_INTR,
        FILE_ILWAL,
        FILE_IO,
        FILE_ISDIR,
        FILE_MFILE,
        FILE_MLINK,
        FILE_NAMETOOLONG,
        FILE_NFILE,
        FILE_NODEV,
        FILE_NOENT,
        FILE_NOEXEC,
        FILE_NOLCK,
        FILE_NOMEM,
        FILE_NOSPC,
        FILE_NOSYS,
        FILE_NOTDIR,
        FILE_NOTEMPTY,
        FILE_NOTTY,
        FILE_NXIO,
        FILE_PERM,
        FILE_PIPE,
        FILE_ROFS,
        FILE_SPIPE,
        FILE_SRCH,
        FILE_XDEV,
        FILE_UNKNOWN_ERROR,
        SOFTWARE_ERROR,
        ILWALID_FILE_FORMAT,
        FILE_DOES_NOT_EXIST,
        CANNOT_ALLOCATE_MEMORY,
        PREPROCESS_ERROR,
        BAD_COMMAND_LINE_ARGUMENT,
        BAD_BOUND_JS_FILE,
        NETWORK_NOT_INITIALIZED,
        NETWORK_ALREADY_CONNECTED,
        NETWORK_CANNOT_CREATE_SOCKET,
        NETWORK_CANNOT_CONNECT,
        NETWORK_CANNOT_BIND,
        NETWORK_ERROR,
        NETWORK_WRITE_ERROR,
        NETWORK_READ_ERROR,
        NETWORK_NOT_CONNECTED,
        NETWORK_CANNOT_DETERMINE_ADDRESS,
        TIMEOUT_ERROR,
        UNSUPPORTED_FUNCTION,
        BAD_PARAMETER,
        DLL_LOAD_FAILED,
        DECRYPTION_ERROR,
        CANNOT_GET_ELEMENT
    };

    // Callback functions that applications linking with this library can
    // provide

    //! Typedef for callback function that should be called on an assertion
    typedef void (*UtilAssertFunc)(const char *, int, const char *, const char *);

    //! Typedef for callback function that should be called to print data
    typedef INT32 (*PrintfFunc)(INT32, UINT32, UINT32, const char *, va_list);

    //! Library initialization routine, sane defaults are provided if this is
    //! not called
    void Initialize(UtilAssertFunc assertFunc, PrintfFunc printfFunc);

    //! Free allocations and reset callbacks
    void Shutdown();

    //! Shutdown network interfaces (SSL)
    void NetShutdown();

    //! Colwert error from errno to lwdiagutils error code
    EC InterpretFileError(int error);

    //! Colwert errno to lwdiagutils error code
    //!
    //! Note: This variant is unsafe, functions called between the function which
    //! returned an error and InterpretFileError, such as Printf, can affect
    //! the value of errno!
    EC InterpretFileError();

    //! Append all the paths specified by the elw variable "MODSP" to the list of
    //! directories to search.
    void AppendElwSearchPaths(vector<string> * Paths, string elwVar = "");

    //! Add extended search paths used in DefaultFindFile.
    void AddExtendedSearchPath(string path);

    //! Strip the directory or filename from a fully qualified filename with
    //! path
    string StripDirectory(const char *FilenameWithPath);
    string StripFilename(const char *FilenameWithPath);

    //! Takes two file system paths and returns a path that is a proper
    //! platform dependent concatenation of the two
    string JoinPaths(const string &path1, const string &path2);

    //! Used to check if a file is encrypted (note that the file must be opened
    //! first)
    EncryptionType GetFileEncryption(FILE* pFile);

    //! Used to check if a data array is encrypted
    EncryptionType GetDataArrayEncryption(const UINT08* data, size_t dataSize);

    //! Search for the file in the given directories.
    //! If found, returns the directory name, else return an empty string.
    string FindFile(string FileName, const vector<string> & Directories);

    //! \brief Performes search for a file in standard directories
    //!
    //! Returns path plus filename.
    //! Extended search includes more directories vs only current working
    //! when ExtendedSearch is false
    string DefaultFindFile(const string& FileName, bool ExtendedSearch);

    //! Return the size of a file.  If you get back OK, the filesize is guaranteed
    //! to be valid
    EC FileSize(FILE *fp, long *pFileSize);

    //! Open a file.  If you get back OK, your file pointer is guaranteed to
    //! be valid.  This function will search in multiple directories for
    //! the file.
    EC OpenFile(const char *FileName, FILE **fp, const char *Mode);
    EC OpenFile(const string& FileName, FILE **fp, const char *Mode);

    //! Local version of assert.  Calls the provided assert callback
    void Assert(const char * file, int line, const char * function, const char * test);

    //! Local version of Printf.  Calls the provided Printf callback
    INT32 Printf(INT32 Priority, const char * Format, ... /* Arguments */)
#ifdef __GNUC__
       // GCC can type-check printf like 'Arguments' against the 'Format' string.
       __attribute__ ((format (printf, 2, 3)))
#endif
    ;

    //! Determine if we are on the LWPU intranet
    bool IsOnLwidiaIntranet();

    //! Read a file from the server
    EC ReadLwidiaServerFile(const string& name, vector<char>* pData);

    void EnableVerboseNetwork(bool bEnable);
    void NetworkPrintf
    (
        const string & host,
        INT32 Priority,
        const char * Format,
        ... /* Arguments */
    )
#ifdef __GNUC__
       // GCC can type-check printf like 'Arguments' against the 'Format' string.
       __attribute__ ((format (printf, 3, 4)))
#endif
    ;
    void NetworkPrintf
    (
        INT32 Priority,
        const char * Format,
        ... /* Arguments */
    )
#ifdef __GNUC__
       // GCC can type-check printf like 'Arguments' against the 'Format' string.
       __attribute__ ((format (printf, 2, 3)))
#endif
    ;

    //! Trigonometric functions for platform consistency
    FLOAT32 Sin(FLOAT32 rad);
    FLOAT32 Cos(FLOAT32 rad);
    
    namespace Path
    {
        bool IsSeparator(char c);
        string AppendSeparator(const string &path);
    }
}

//! The LwDiagXp namespace (platform specific functionality)
namespace LwDiagXp
{
    //! Get the path delimiter for extracting paths from elw variables
    char GetElwPathDelimiter();

    //! Get the specified environment variable value. If the variable does
    //! not exist return "".
    string GetElw(string Variable);

    //! Return true if file exists.  strFilename should include the path to
    //! the file.
    bool DoesFileExist(string strFilename);

    //! Drop-in replacement for fopen() in K&R, including setting errno.
    //! This replacement handles long filenames on some platforms.
    FILE *Fopen(const char *FileName, const char *Mode);

    //! Create a new platform specific Socket.
    Socket * CreateSocket();

    //! Dynamic library functionality
    LwDiagUtils::EC LoadDynamicLibrary
    (
        const string &fileName
       ,void **pModuleHandle
       ,UINT32 loadDLLFlags
    );
    LwDiagUtils::EC LoadDynamicLibrary
    (
        const string &fileName
       ,void **pModuleHandle
    );
    LwDiagUtils::EC UnloadDynamicLibrary(void * moduleHandle);
    void * GetDynamicLibraryProc(void * moduleHandle, const char * funcName);
    string GetDynamicLibrarySuffix();

    //! Load/Unload the SSL library used by IsOnLwidiaIntranet() and ReadLwidiaServerFile()
    LwDiagUtils::EC LoadLibSsl();
    void UnloadLibSsl();

    //! Determine if we are on the LWPU intranet
    bool IsOnLwidiaIntranet(const string& host);

    //! Read a file from the server
    LwDiagUtils::EC ReadLwidiaServerFile(const string& host, const string& name, vector<char>* pData);
}

namespace LwDiagUtils
{
    //! RAII class to hold a file and ensure it is closed when the object is
    //! destroyed
    class FileHolder
    {
    public:
    FileHolder()
        : m_pFile(NULL)
        {
        }

        FileHolder(const string &FileName, const char* Attrib)
        {
            LwDiagUtils::OpenFile(FileName, &m_pFile, Attrib);
        }

        FileHolder(const char* FileName, const char* Attrib)
        {
            LwDiagUtils::OpenFile(FileName, &m_pFile, Attrib);
        }

        LwDiagUtils::EC Open(const string &FileName, const char *Attrib)
        {
            Close();
            return LwDiagUtils::OpenFile(FileName, &m_pFile, Attrib);
        }

        LwDiagUtils::EC Open(const char *FileName, const char *Attrib)
        {
            Close();
            return LwDiagUtils::OpenFile(FileName, &m_pFile, Attrib);
        }

        void Close()
        {
            if (m_pFile)
            {
                fclose(m_pFile);
                m_pFile = NULL;
            }
        }

        ~FileHolder()
        {
            Close();
        }

        FILE* GetFile() const
        {
            return m_pFile;
        }

    private:
        FILE *m_pFile;
        // do not support assignment & copy
        FileHolder & operator=(const FileHolder&);
        FileHolder( const FileHolder &);
    };
}
#endif

