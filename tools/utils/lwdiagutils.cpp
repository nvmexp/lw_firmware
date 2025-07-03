/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2011, 2013, 2017-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwdiagutils.h"
#include "socket.h"

#include <assert.h>
#include <cerrno>
#include <condition_variable>
#include <chrono>
#include <cstdarg>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <map>
#ifdef _WIN32
#   pragma warning(push)
#   pragma warning(disable: 4265) // class has virtual functions, but destructor is not virtual
#endif
#include <thread>
#ifdef _WIN32
#   pragma warning(pop)
#endif

//! Private LwDiagUtils functions
namespace LwDiagUtils
{
    bool s_bVerboseNetwork = false;

    struct NetworkPrintData
    {
        LwDiagUtils::Priority pri;
        string                printData;
    };
    using NetworkPrintQueue = vector<NetworkPrintData>;
    // Needs to be a base type (not a unique_ptr) specifically so that it doesnt
    // get destroyed by the static destruction process.  Shutdown is called at the
    // very end of static destruction and if the queue gets destroyed before the
    // flush in Shutdown (because it is a non POD type) the exelwtable will either
    // segfault or prints will be lost
    NetworkPrintQueue* s_pNetworkPrintQueue = nullptr;
    mutex s_NetworkPrintMutex;

    static void FlushNetworkPrintQueue()
    {
        NetworkPrintQueue printQueue;

        {
            // Extract the contents of the queue when holding the
            // lock, then release the lock, so that the printing is
            // done while the lock is released.  This prevents
            // a lock order ilwersion between this mutex and the
            // event mutex held by MODS, fiddled with during
            // ModsExterlwAPrintf.
            unique_lock<mutex> lock(s_NetworkPrintMutex);
            if (s_pNetworkPrintQueue)
                printQueue.swap(*s_pNetworkPrintQueue);
        }

        for (const auto& p : printQueue)
        {
            LwDiagUtils::Printf(p.pri, "%s", p.printData.c_str());
        }
    }

    class FlushNetworkPrintQueueOnExit
    {
        public:
            FlushNetworkPrintQueueOnExit() { }
            ~FlushNetworkPrintQueueOnExit() { FlushNetworkPrintQueue(); }
    };
}

//! Default assertion function if none is provided by the application
static INT32 LwDiagUtilsVAPrintf(INT32 Priority, UINT32 ModuleCode, UINT32 Sps,
                                 const char * Format, va_list RestOfArgs);

//! \brief Default assertion function if none is provided by the application
//!
//! \param file     : File name where the assert oclwred
//! \param line     : Line in the file where the assert oclwred
//! \param function : Function name where the assert oclwred
//! \param test     : Failing test as a string
static void LwDiagUtilsAssert
(
    const char * file,
    int          line,
    const char * function,
    const char * test
)
{
   printf("%s: MASSERT(%s) failed at %s line %d\n",
          function, test, file, line);
   fflush(stdout);

   Printf(LwDiagUtils::PriHigh,
          "%s: MASSERT(%s) failed at %s line %d\n",
           function, test, file, line);

   assert(0);
}

//! \brief Default printing function if none is provided by the application
//!
//! \param Priority   : Priority to print at (not used)
//! \param ModuleCode : Module doing the printing (not used)
//! \param Sps        : Screen print state when printing (not used)
//! \param Format     : Format string for the print
//! \param RestOfArgs : Argument list for values to print
//!
//! \return Number of characters written
static INT32 LwDiagUtilsVAPrintf
(
    INT32        Priority,
    UINT32       ModuleCode,
    UINT32       Sps,
    const char * Format,
    va_list      RestOfArgs
)
{
    INT32 charsWritten = 0;
    if (Priority == LwDiagUtils::PriNone)
        return charsWritten;
    if (Priority == LwDiagUtils::PriError)
        charsWritten = printf("ERROR: ");
    else if (Priority == LwDiagUtils::PriWarn)
        charsWritten = printf("WARNING: ");

    return vprintf(Format, RestOfArgs);
}


namespace
{
    //! Callback function variables
    LwDiagUtils::UtilAssertFunc s_pAssertFunc = LwDiagUtilsAssert;
    LwDiagUtils::PrintfFunc s_pPrintfFunc = LwDiagUtilsVAPrintf;

    //! Vector of extended search paths to use when calling DefaultFindFile
    //!
    //! Note that this must be a pointer.  Since this library is not (and
    //! cannot be) protected by the OneTimeInit code in MODS and the
    //! Shutdown() routine is called from MODS prior from ~OneTimeInit it
    //! is possible that a non-pointer vector would be destroyed prior to
    //! ~OneTimeInit resulting in the data from the vector being destroyed
    //! twice (once by static destruction and once by Shutdown) and causing
    //! crashes
    vector<string> * s_ExtendedSearchPaths = NULL;

    // Needs to be a base type (not a unique_ptr) specifically so that it doesnt
    // get destroyed by the static destruction process.  Shutdown is called at the
    // very end of static destruction and if the thread list gets destroyed before the
    // flush in Shutdown (because it is a non POD type) the exelwtable will either
    // segfault or prints will be lost
    vector<thread> * s_pRunningThreads;
}


//! \brief Initialize the library by setting the callback functions
//!
//! \param assertFunc : Callback function to call when an assert oclwrs
//! \param printfFunc : Callback function to call when to print
void LwDiagUtils::Initialize(UtilAssertFunc assertFunc, PrintfFunc printfFunc)
{
    if (assertFunc)
        s_pAssertFunc = assertFunc;
    if (printfFunc)
        s_pPrintfFunc = printfFunc;
    if (s_ExtendedSearchPaths == nullptr)
        s_ExtendedSearchPaths = new vector<string>;
    if (s_pRunningThreads == nullptr)
        s_pRunningThreads = new vector<thread>;
}

//! \brief Free allocations and reset callbacks
void LwDiagUtils::Shutdown()
{
    LwDiagUtils::NetShutdown();

    s_pAssertFunc = LwDiagUtilsAssert;
    s_pPrintfFunc = LwDiagUtilsVAPrintf;

    if (s_ExtendedSearchPaths != nullptr)
    {
        delete s_ExtendedSearchPaths;
        s_ExtendedSearchPaths = nullptr;
    }
}

//! Shutdown network interfaces (SSL)
void LwDiagUtils::NetShutdown()
{
    if (s_pRunningThreads != nullptr)
    {
        for (auto & lwrThread : *s_pRunningThreads)
        {
            if (lwrThread.joinable())
                lwrThread.join();
        }
        s_pRunningThreads->clear();
        delete s_pRunningThreads;
        s_pRunningThreads = nullptr;
    }
    LwDiagXp::UnloadLibSsl();

    if (s_pNetworkPrintQueue)
    {
        FlushNetworkPrintQueue();
        unique_lock<mutex> lock(s_NetworkPrintMutex);
        delete s_pNetworkPrintQueue;
        s_pNetworkPrintQueue = nullptr;
    }
}

//! \brief Append all the paths specified by the elw variable "MODSP" to the
//!        list of directories to search.
//!
//! \param Paths  : List of paths to add the elwronment paths to
//! \param elwVar : Environment variable to use for paths
void LwDiagUtils::AppendElwSearchPaths(vector<string> * Paths, string elwVar)
{
    string ElwSearchPaths;

    if(elwVar == "")
    {
        ElwSearchPaths = LwDiagXp::GetElw("MODSP");
    }
    else
    {
        ElwSearchPaths = LwDiagXp::GetElw(elwVar);
    }

    size_t length = ElwSearchPaths.size();

    if(length == 0)
        return;

    char *SearchPath = new char[length + 1];

    UINT32 j = 0;

    char delimiter = LwDiagXp::GetElwPathDelimiter();

    for(size_t i = 0; i < length; i ++)
    {

        if(ElwSearchPaths[i] == delimiter)
        {
            SearchPath[j] = '\0';
            string NewSearchPath(SearchPath);
            Paths->push_back(NewSearchPath);
            j = 0;
        }
        else
        {
           SearchPath[j] = ElwSearchPaths[i];

           if(SearchPath[j] == '\\')
               SearchPath[j] = '/';

           j++;
        }
    }

    SearchPath[j] = '\0';
    string NewSearchPath(SearchPath);
    Paths->push_back(NewSearchPath);
    delete [] SearchPath;
}

//! \brief Add extended search paths used in DefaultFindFile.
//!
//! \param path  : Path to add to the extended search path
void LwDiagUtils::AddExtendedSearchPath(string path)
{
    if (s_ExtendedSearchPaths == NULL)
    {
        Printf(LwDiagUtils::PriHigh,
               "LwDiagUtils not initialized.  %s not added to search path\n",
               path.c_str() );
        return;
    }

    s_ExtendedSearchPaths->push_back(path);
}

//! \brief Takes two file system paths and returns a path that is a proper
//!        platform dependent concatenation of the two
//!
//! \param path1 : First path to be concatenated
//! \param path2 : Second path to be concatenated
//!
//! \return Concatenated path
string LwDiagUtils::JoinPaths(const string &path1, const string &path2)
{
    if (path2.empty())
    {
        return path1;
    }
    if (!Path::IsSeparator(path2[0]))
    {
        return Path::AppendSeparator(path1) + path2;
    }
    return path1 + path2;
}

//! \brief Strips the directory in dos/windows/linux paths, leaving just the
//!        filename.
//!
//! \param FilenameWithPath : String containing the filename with path to
//!                           be stripped
//!
//! \return If no filename is provided, then the empty string will be returned
//!         If no backslash or slashes appear then the entire input will be
//!         returned. This will not return the last backslash or slash prior
//!         to the filename.
string LwDiagUtils::StripDirectory(const char *FilenameWithPath)
{
    LWDASSERT(FilenameWithPath != NULL);
    // This is very imperfect implementation
    // There are many network share related "//netapp22/..." cases where this
    // simple solution will break
    string Result = FilenameWithPath;
    string::size_type Pos = Result.find_last_of("/\\");
    if (Pos == string::npos)
    {
        // no path, return whole result
        return Result;
    }
    else if(Pos == Result.size()-1)
    {
        // no file, return empty string
        return "";
    }
    else
    {
        // return just the filename
        return Result.substr(Pos+1).c_str();
    }
}

//! \brief Strips the filename in dos/windows/linux paths, leaving just the
//!        directory.
//!
//! \param FilenameWithPath : String containing the fully qualified filename
//!                           to strip the filename from
//!
//! \return If no path is provided, then the empty string will be
//!         returned (not ".")
string LwDiagUtils::StripFilename(const char *FilenameWithPath)
{
    LWDASSERT(FilenameWithPath != NULL);
    // This is very imperfect implementation
    // There are many network share related "//netapp22/..." cases where this
    // simple solution will break
    string Result = FilenameWithPath;
    string::size_type Pos = Result.find_last_of("/\\");
    if (Pos == string::npos)
    {
        Result = "";
    }
    else
    {
        Result = Result.substr(0, Pos).c_str();
    }

    return Result;
}

namespace
{
    LwDiagUtils::EncryptionType GetEncryption(UINT08 a, UINT08 b, UINT08 c, UINT08 d)
    {
        if ((a == 0xbe) && (b == 0xef) && (c == 0x0d) && (d == 0xed))
        {
            return LwDiagUtils::ENCRYPTED_JS;
        }
        else if ((a == 0x0d) && (b == 0xed) && (c == 0xbe) && (d == 0xef))
        {
            return LwDiagUtils::ENCRYPTED_LOG;
        }
        else if ((a == 0xde) && (b == 0xad) && (c == 0x0d) && (d == 0x06))
        {
            return LwDiagUtils::ENCRYPTED_JS_V2;
        }
        else if ((a == 0x0d) && (b == 0x06) && (c == 0xde) && (d == 0xad))
        {
            return LwDiagUtils::ENCRYPTED_LOG_V2;
        }
        else if ((a == 0xf1) && (b == 0x1a) && (c == 0x80))
        {
            return LwDiagUtils::ENCRYPTED_FILE_V3;
        }
        else if ((a == 0xf1) && (b == 0x1a) && (c == 0x81))
        {
            return LwDiagUtils::ENCRYPTED_LOG_V3;
        }
        else if ((a == 0xf1) && (b == 0x1a) && (c == 0x82))
        {
            return LwDiagUtils::ENCRYPTED_FILE_V3_BASE128;
        }
        return LwDiagUtils::NOT_ENCRYPTED;
    }
};

//! \brief Used to check if a file is encrypted (note that the file must
//!        be opened first) - this API consumes the first 4 bytes in the file
//!
//! \param pFile : Pointer to open file descriptor to check for encryption
//!
//! \return Encryption type of the file
LwDiagUtils::EncryptionType LwDiagUtils::GetFileEncryption(FILE* pFile)
{
    UINT08 a[4] = { };

    const auto numRead = fread(a, 1, 4, pFile);

    fseek(pFile, 0, SEEK_SET);

    if (numRead < 3) // We need at least 3 bytes to identify
    {
        return LwDiagUtils::NOT_ENCRYPTED;
    }

    return GetEncryption(a[0], a[1], a[2], a[3]);
}

LwDiagUtils::EncryptionType LwDiagUtils::GetDataArrayEncryption
(
    const UINT08* data,
    size_t        dataSize
)
{
    if (dataSize < 3)
        return LwDiagUtils::NOT_ENCRYPTED;

    return GetEncryption(data[0], data[1], data[2], dataSize > 3 ? data[3] : 0);
}

//! \brief Search for the file in the given directories.
//!
//! \param FileName    : Name of file to search for.
//! \param Directories : List of directories to search.  Each entry
//!                      may or may not end in the platform-specific
//!                      path-separator character.
//!
//! \return The directory name if the file is found, or the empty
//!         string if it is not.
//!
//! \sa LwDiagXp::DoesFileExist
string LwDiagUtils::FindFile
(
   string                 FileName,
   const vector<string> & Directories
)
{
   LWDASSERT(FileName != "");
   LWDASSERT(Directories.size() > 0);
   vector<string> Paths;
   vector<string>::const_iterator it;

   for (it = Directories.begin(); it != Directories.end(); ++it)
       Paths.push_back(*it);

   AppendElwSearchPaths(&Paths);

   char PathSeparator[2];

   PathSeparator[0] = '/';
   PathSeparator[1] = '\0';

   for (it = Paths.begin(); it != Paths.end(); ++it)
   {
      // Append the last past separator to the path if necessary.
      string Path = *it;
      if
      (     (Path.size() > 0)
         && (Path[Path.size()-1] != PathSeparator[0])
      )
      {
         Path += PathSeparator;
      }

      if (LwDiagXp::DoesFileExist(Path + FileName))
         return Path;    // Return the path, appended with '/' if necessary
   }

   return "";
}

//! \brief Performes search for a file in standard directories
//!
//! \param FileName       : Name of file to search for.
//! \param ExtendedSearch : true to include extended directories in the search
//!                         (false to only search the cwd)
//!
//! \return Path plus filename.
//!
string LwDiagUtils::DefaultFindFile
(
    const string& FileName,
    bool          ExtendedSearch
)
{
    if (!ExtendedSearch || LwDiagXp::DoesFileExist(FileName))
        return FileName;

    vector<string> Paths;
    if (s_ExtendedSearchPaths != NULL)
    {
        // Search for the file in the program path and script path.
        Paths.assign(s_ExtendedSearchPaths->begin(),
                     s_ExtendedSearchPaths->end());
    }

    AppendElwSearchPaths(&Paths);

    if (Paths.empty())
        return FileName;

    string Path = LwDiagUtils::FindFile(FileName, Paths);
    return Path + FileName;
}

//! \brief Return the size of a file.
//!
//! \param fp        : Open file descriptor to get the filesize for.
//! \param pFileSize : Pointer to returned file size
//!
//! \return OK if the filesize is valid, not OK otherwise
//!
LwDiagUtils::EC LwDiagUtils::FileSize(FILE *fp, long *pFileSize)
{
    long fileSize;
    long lwrPos;

    LWDASSERT(fp);
    LWDASSERT(pFileSize);

    *pFileSize = 0;

    lwrPos = ftell(fp);
    if ((int)lwrPos == -1)
        return InterpretFileError(errno);

    if (fseek(fp, 0, SEEK_END) != 0)
        return InterpretFileError(errno);

    fileSize = ftell(fp);
    if ((int)fileSize == -1)
        return InterpretFileError(errno);

    if (fseek(fp, lwrPos, SEEK_SET) != 0)
        return InterpretFileError(errno);

    *pFileSize = fileSize;

    return OK;
}

//! \brief Open a file, multiple directories will be searched for the file
//!
//! \param FileName  : File name to open
//! \param fp        : Pointer to a returned open file descriptor pointer
//! \param Mode      : Mode to open the file in
//!
//! \return OK if the file pointer is valid, not OK otherwise
//!
LwDiagUtils::EC LwDiagUtils::OpenFile
(
    const char *FileName,
    FILE **     fp,
    const char *Mode
)
{
   LWDASSERT(FileName);
   LWDASSERT(fp);
   LWDASSERT(Mode);

   string TheFile(FileName);

   return OpenFile(TheFile, fp, Mode);
}

//! \brief Open a file, multiple directories will be searched for the file
//!
//! \param fileName  : File name to open
//! \param fp        : Pointer to a returned open file descriptor pointer
//! \param mode      : Mode to open the file in
//!
//! \return OK if the file pointer is valid, not OK otherwise
//!
LwDiagUtils::EC LwDiagUtils::OpenFile
(
    const string& fileName,
    FILE **       fp,
    const char *  mode
)
{
   LWDASSERT(fp);
   LWDASSERT(mode);

   // We only want to do a file search if the file doesn't exist in the
   // current directory AND we are doing a read operation.  Writes should
   // always occur to the default directory.
   const string fullPath = DefaultFindFile(fileName, strchr(mode, 'r'));

   *fp = LwDiagXp::Fopen(fullPath.c_str(), mode);

   if (0 == *fp)
   {
      const int error = errno;
      Printf(LwDiagUtils::PriNormal, "Failed to open file %s - %s\n",
             fullPath.c_str(), strerror(error));
      return InterpretFileError(error);
   }

   Printf(LwDiagUtils::PriLow, "Successfully opened %s\n", fullPath.c_str());

   return OK;
}

//! \brief Assertion function called by LWDASSERT (calls the assert
//!        function callback)
//!
//! \param file     : File name where the assert oclwred
//! \param line     : Line in the file where the assert oclwred
//! \param function : Function name where the assert oclwred
//! \param test     : Failing test as a string
void LwDiagUtils::Assert
(
    const char * file,
    int          line,
    const char * function,
    const char * test
)
{
    (*s_pAssertFunc)(file, line, function, test);
}

//! \brief LwDiagUtils printing function (calls the printf function callback)
//!
//! \param Priority : Priority to print at (not used)
//! \param Format   : Format string for the print
//! \param ...      : Argument list for values to print
//!
//! \return Number of characters written
INT32 LwDiagUtils::Printf
(
    INT32 Priority,
    const char * Format,
    ... /* Arguments */
)
{
    int CharactersWritten;

    va_list Arguments;
    va_start(Arguments, Format);

    CharactersWritten = (*s_pPrintfFunc)(Priority, ModuleNone, SPS_NORMAL,
                                         Format, Arguments);

    va_end(Arguments);

    return CharactersWritten;
}

namespace
{
    const char* const s_Hosts[] =
    {
        "hqlwmodsauth01.lwpu.com",
        "hqlwmodsauth02.lwpu.com",
        "hqlwmodsauth03.lwpu.com",
        "hqlwmodsauth04.lwpu.com",
        "rnlwmodsauth01.lwpu.com",
        "rnlwmodsauth02.lwpu.com",
        "rnlwmodsauth03.lwpu.com",
        "rnlwmodsauth04.lwpu.com"
    };
    bool s_onLwidiaIntranet = false;
};

//! \brief Return true if running on the lwpu intranet
//!
bool LwDiagUtils::IsOnLwidiaIntranet()
{
    static mutex s_Mutex;
    static condition_variable s_CV;
    static unsigned int s_FinishedThreads = 0;
    static bool s_initialized = false;

    if (s_initialized)
    {
        return s_onLwidiaIntranet;
    }

    // Preload SSL library so that all spawned threads can use it.
    // This cannot be done automagically in LwDiagXp::IsOnLwidiaIntranet(),
    // because that function is called simultaneously from multiple
    // threads and the function pointers from the SSL library are
    // globals on non-Windows platforms, resulting in a data race.
    if (LwDiagXp::LoadLibSsl() != LwDiagUtils::OK)
    {
        s_initialized = true;
        return s_onLwidiaIntranet;
    }

    FlushNetworkPrintQueueOnExit f;

    // Launch threads to query each host. Doing this in parallel should prevent mods
    // from slowing down when some of the servers are having issues.
    for (auto host : s_Hosts)
    {
        thread t([host] ()
        {
            const bool success = LwDiagXp::IsOnLwidiaIntranet(host);

            unique_lock<mutex> lock(s_Mutex);
            if (!s_initialized)
            {
                if (success)
                    s_onLwidiaIntranet = true;
                s_FinishedThreads++;

                s_CV.notify_all();
            }
        });
        s_pRunningThreads->push_back(move(t));
    }

    // Wait until the first success, they all fail, or timeout
    {
        const auto timeout = chrono::system_clock::now() + chrono::seconds(5);
        unique_lock<mutex> lock(s_Mutex);
        while (!s_onLwidiaIntranet &&
               (s_FinishedThreads < sizeof(s_Hosts) / sizeof(s_Hosts[0])))
        {
            if (s_CV.wait_until(lock, timeout) == cv_status::timeout)
                break;
        }
        s_initialized = true;
    }

    return s_onLwidiaIntranet;
}

LwDiagUtils::EC LwDiagUtils::ReadLwidiaServerFile(const string& name, vector<char>* pData)
{
    static mutex s_Mutex;
    static condition_variable s_CV;
    static map<string, vector<char>> s_DataCache;
    static map<string, unsigned int> s_FinishedThreads;

    LWDASSERT(pData != nullptr);
    pData->clear();

    FlushNetworkPrintQueueOnExit f;

    // Check if we are on the Lwpu intranet
    if (!IsOnLwidiaIntranet())
    {
        return NETWORK_ERROR;
    }

    // Check cached network fetches (including failures, which are stored as empty vectors)
    {
        unique_lock<mutex> lock(s_Mutex);
        if (s_DataCache.count(name))
        {
            pData->assign(s_DataCache[name].begin(), s_DataCache[name].end());
            return pData->empty() ? NETWORK_READ_ERROR : OK;
        }
    }
    // Launch threads to query each host. Doing this in parallel should prevent mods from slowing
    // down when some of the servers are having issues.
    for (auto host : s_Hosts)
    {
        thread t([host, name] ()
        {
            vector<char> fileData;
            EC ec = LwDiagXp::ReadLwidiaServerFile(host, name, &fileData);

            unique_lock<mutex> lock(s_Mutex);
            if (ec == OK && fileData.size() > 0 && !s_DataCache.count(name))
            {
                s_DataCache[name].assign(fileData.begin(), fileData.end());
            }
            s_FinishedThreads[name]++;
            s_CV.notify_all();
        });
        s_pRunningThreads->push_back(move(t));
    }

    // Wait until the first success, they all fail, or timeout
    {
        const auto timeout = chrono::system_clock::now() + chrono::seconds(5);
        unique_lock<mutex> lock(s_Mutex);

        while (!s_DataCache.count(name) &&
               (s_FinishedThreads[name] < sizeof(s_Hosts) / sizeof(s_Hosts[0])))
        {
            if (s_CV.wait_until(lock, timeout) == cv_status::timeout)
                break;
        }

        // Copy any fetched data to the output.
        // If the fetch failed this will add an empty vector to the data cache.
        pData->assign(s_DataCache[name].begin(), s_DataCache[name].end());
    }
 
    return pData->empty() ? NETWORK_READ_ERROR : OK;
}

// -----------------------------------------------------------------------------
void LwDiagUtils::EnableVerboseNetwork(bool bEnable)
{
    s_bVerboseNetwork = bEnable;
}

namespace LwDiagUtils
{
    static void NetworkVaPrintf(const string & host, INT32 Priority, const char* format, va_list args)
    {
        if (!s_bVerboseNetwork && !s_onLwidiaIntranet)
            return;

        string tmpFormat = format;
        if (host != "")
            tmpFormat = "[host=" + host + "] " + tmpFormat;

        va_list tmpArgs;
        va_copy(tmpArgs, args);
        const int printSize = vsnprintf(nullptr, 0, tmpFormat.c_str(), tmpArgs);
        if (printSize > 0)
        {
            NetworkPrintData printData;
            printData.pri = static_cast<LwDiagUtils::Priority>(Priority);
            printData.printData.resize(printSize + 1);
            vsprintf(&printData.printData[0], tmpFormat.c_str(), args);
            unique_lock<mutex> lock(s_NetworkPrintMutex);
            if (!s_pNetworkPrintQueue)
                s_pNetworkPrintQueue = new NetworkPrintQueue;
            s_pNetworkPrintQueue->push_back(printData);
        }
        va_end(tmpArgs);
    }
};

//------------------------------------------------------------------------------
void LwDiagUtils::NetworkPrintf(const string & host, INT32 Priority, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    NetworkVaPrintf(host, Priority, format, args);
    va_end(args);
}

//------------------------------------------------------------------------------
void LwDiagUtils::NetworkPrintf(INT32 Priority, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    NetworkVaPrintf("", Priority, format, args);
    va_end(args);
}

LwDiagUtils::EC LwDiagUtils::InterpretFileError()
{
    return InterpretFileError(errno);
}

//! \brief Private function to colwert file error (via errno) to an EC
//!
//! \return EC corresponding to the file error
LwDiagUtils::EC LwDiagUtils::InterpretFileError(int error)
{
   switch (error)
   {
      case E2BIG:        return FILE_2BIG;
      case EACCES:       return FILE_ACCES;
      case EAGAIN:       return FILE_AGAIN;
      case EBADF:        return FILE_BADF;
      case EBUSY:        return FILE_BUSY;
      case ECHILD:       return FILE_CHILD;
      case EDEADLK:      return FILE_DEADLK;
      case EEXIST:       return FILE_EXIST;
      case EFAULT:       return FILE_FAULT;
      case EFBIG:        return FILE_FBIG;
      case EINTR:        return FILE_INTR;
      case EILWAL:       return FILE_ILWAL;
      case EIO:          return FILE_IO;
      case EISDIR:       return FILE_ISDIR;
      case EMFILE:       return FILE_MFILE;
      case EMLINK:       return FILE_MLINK;
      case ENAMETOOLONG: return FILE_NAMETOOLONG;
      case ENFILE:       return FILE_NFILE;
      case ENODEV:       return FILE_NODEV;
      case ENOENT:       return FILE_NOENT;
      case ENOEXEC:      return FILE_NOEXEC;
      case ENOLCK:       return FILE_NOLCK;
      case ENOMEM:       return FILE_NOMEM;
      case ENOSPC:       return FILE_NOSPC;
      case ENOSYS:       return FILE_NOSYS;
      case ENOTDIR:      return FILE_NOTDIR;
      case ENOTEMPTY:    return FILE_NOTEMPTY;
      case ENOTTY:       return FILE_NOTTY;
      case ENXIO:        return FILE_NXIO;
      case EPERM:        return FILE_PERM;
      case EPIPE:        return FILE_PIPE;
      case EROFS:        return FILE_ROFS;
      case ESPIPE:       return FILE_SPIPE;
      case ESRCH:        return FILE_SRCH;
      case EXDEV:        return FILE_XDEV;
      default:           return FILE_UNKNOWN_ERROR;
   }
}

namespace
{
    // Generate a Sin table with 256 non-trivial data points between
    // 0 < a < pi/2, which creates an increment of 257 angles in that
    // spacing
    static const FLOAT64 s_Pi = 3.141592653589793238462643383279;
    static const FLOAT32 s_SinIncRad = s_Pi / 257.0 / 2.0;
    static const UINT32  s_IdxPerQuad = 257;
    const FLOAT32 s_SinData[] =
    {
        0.00611201, 0.01222379, 0.01833512, 0.02444576
       ,0.03055548, 0.03666407, 0.04277128, 0.04887690
       ,0.05498069, 0.06108243, 0.06718189, 0.07327883
       ,0.07937304, 0.08546429, 0.09155234, 0.09763697
       ,0.10371795, 0.10979506, 0.11586807, 0.12193675
       ,0.12800087, 0.13406022, 0.14011455, 0.14616364
       ,0.15220729, 0.15824524, 0.16427729, 0.17030320
       ,0.17632273, 0.18233569, 0.18834183, 0.19434094
       ,0.20033279, 0.20631714, 0.21229382, 0.21826252
       ,0.22422311, 0.23017532, 0.23611890, 0.24205369
       ,0.24797942, 0.25389588, 0.25980288, 0.26570016
       ,0.27158749, 0.27746472, 0.28333157, 0.28918782
       ,0.29503331, 0.30086771, 0.30669090, 0.31250262
       ,0.31830272, 0.32409090, 0.32986695, 0.33563071
       ,0.34138191, 0.34712034, 0.35284585, 0.35855815
       ,0.36425704, 0.36994234, 0.37561384, 0.38127127
       ,0.38691449, 0.39254326, 0.39815733, 0.40375653
       ,0.40934068, 0.41490951, 0.42046285, 0.42600048
       ,0.43152222, 0.43702781, 0.44251707, 0.44798982
       ,0.45344582, 0.45888489, 0.46430680, 0.46971139
       ,0.47509843, 0.48046771, 0.48581901, 0.49115220
       ,0.49646708, 0.50176334, 0.50704092, 0.51229948
       ,0.51753896, 0.52275908, 0.52795976, 0.53314060
       ,0.53830159, 0.54344243, 0.54856294, 0.55366302
       ,0.55874240, 0.56380093, 0.56883836, 0.57385457
       ,0.57884932, 0.58382243, 0.58877373, 0.59370309
       ,0.59861028, 0.60349506, 0.60835725, 0.61319679
       ,0.61801338, 0.62280691, 0.62757713, 0.63232398
       ,0.63704717, 0.64174658, 0.64642197, 0.65107322
       ,0.65570015, 0.66030264, 0.66488045, 0.66943336
       ,0.67396128, 0.67846406, 0.68294144, 0.68739337
       ,0.69181961, 0.69621998, 0.70059437, 0.70494252
       ,0.70926440, 0.71355975, 0.71782845, 0.72207040
       ,0.72628534, 0.73047310, 0.73463356, 0.73876667
       ,0.74287212, 0.74694979, 0.75099963, 0.75502139
       ,0.75901490, 0.76298010, 0.76691675, 0.77082479
       ,0.77470410, 0.77855438, 0.78237557, 0.78616756
       ,0.78993016, 0.79366326, 0.79736674, 0.80104047
       ,0.80468422, 0.80829787, 0.81188136, 0.81543452
       ,0.81895721, 0.82244933, 0.82591075, 0.82934123
       ,0.83274078, 0.83610922, 0.83944643, 0.84275228
       ,0.84602666, 0.84926939, 0.85248047, 0.85565960
       ,0.85880685, 0.86192203, 0.86500490, 0.86805558
       ,0.87107372, 0.87405944, 0.87701237, 0.87993264
       ,0.88282007, 0.88567442, 0.88849574, 0.89128381
       ,0.89403868, 0.89676011, 0.89944798, 0.90210235
       ,0.90472293, 0.90730977, 0.90986264, 0.91238159
       ,0.91486651, 0.91731715, 0.91973358, 0.92211556
       ,0.92446321, 0.92677623, 0.92905474, 0.93129849
       ,0.93350738, 0.93568146, 0.93782055, 0.93992466
       ,0.94199365, 0.94402742, 0.94602597, 0.94798911
       ,0.94991690, 0.95180917, 0.95366591, 0.95548695
       ,0.95727235, 0.95902205, 0.96073580, 0.96241373
       ,0.96405572, 0.96566170, 0.96723151, 0.96876532
       ,0.97026289, 0.97172415, 0.97314918, 0.97453785
       ,0.97589010, 0.97720587, 0.97848517, 0.97972792
       ,0.98093402, 0.98210353, 0.98323631, 0.98433238
       ,0.98539168, 0.98641419, 0.98739982, 0.98834860
       ,0.98926044, 0.99013531, 0.99097317, 0.99177408
       ,0.99253786, 0.99326462, 0.99395424, 0.99460673
       ,0.99522209, 0.99580026, 0.99634123, 0.99684501
       ,0.99731147, 0.99774075, 0.99813271, 0.99848741
       ,0.99880481, 0.99908489, 0.99932766, 0.99953306
       ,0.99970114, 0.99983191, 0.99992532, 0.99998134
    };
}

FLOAT32 LwDiagUtils::Sin(FLOAT32 rad)
{
    UINT32 index = static_cast<UINT32>((fmod(static_cast<double>(fabs(rad)),
                                             2.0 * s_Pi) / s_SinIncRad) + 0.5);

    // Callwlate the index within a full circle and return the trivial points
    index = index % (s_IdxPerQuad * 4);
    if ((index == 0) || (index == (s_IdxPerQuad * 2)))
        return 0.0;
    else if (index == s_IdxPerQuad)
        return (rad < 0.0) ? -1.0 : 1.0;
    else if (index == (s_IdxPerQuad * 3))
        return (rad < 0.0) ? 1.0 : -1.0;

    const UINT32  quadrant = index / s_IdxPerQuad;
    FLOAT32 signMult = quadrant > 1 ? -1.0 : 1.0;

    // sin(-x) = -sin(x)
    if (rad < 0.0)
        signMult = -signMult;

    index = index % s_IdxPerQuad;
    if ((quadrant == 1) || (quadrant == 3))
        index = s_IdxPerQuad - index;

    // Shift index by one to account for trivial points not being in the table
    return s_SinData[index - 1] * signMult;
}

FLOAT32 LwDiagUtils::Cos(FLOAT32 rad)
{
    // cos(-x) = cos(x)
    UINT32 index = static_cast<UINT32>((fmod(static_cast<double>(fabs(rad)),
                                             2.0 * s_Pi) / s_SinIncRad) + 0.5);

    // Callwlate the index within a full circle and return the trivial points
    index = index % (s_IdxPerQuad * 4);
    if (index == 0)
        return 1.0;
    else if (index == (s_IdxPerQuad * 2))
        return -1.0;
    else if ((index == s_IdxPerQuad) || (index == (s_IdxPerQuad * 3)))
        return 0.0;

    const UINT32  quadrant = index / s_IdxPerQuad;
    const FLOAT32 signMult = (quadrant == 0) || (quadrant == 3) ? 1.0 : -1.0;

    // Since the data was generated for sin, flip the index so we get cos
    index = s_IdxPerQuad - (index % s_IdxPerQuad);
    if ((quadrant == 1) || (quadrant == 3))
        index = s_IdxPerQuad - index;

    // Shift index by one to account for trivial points not being in the table
    return s_SinData[index - 1] * signMult;
}

