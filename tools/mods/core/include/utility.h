/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   utility.h
 * @brief  Contains the Utility namespace declaration.
 *
 * The Utility functions are miscellaneous functions that could be used
 * in a lot of areas.  Things like random number routines and type
 * colwersions go here.
 */

// Utility functions.

#pragma once

#ifndef INCLUDED_UTILITY_H
#define INCLUDED_UTILITY_H

#include "align.h"
#include "bitflags.h"
#include "random.h"
#include "massert.h"
#include "lwdiagutils.h"
#include "rc.h"
#include "jscript.h"
#include "jspubtd.h"
#include "tee.h"
#include "tar.h"
#include "core/include/coreutility.h"
#include "core/include/security.h"
#include "lwfixedtypes.h"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <iterator>
#include <limits>
#include <type_traits>
#include <set>
#include <vector>

// MIN,MAX,MINMAX,CEIL_DIV : Note that args are evaluated multiple times.
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
#ifndef CEIL_DIV
    #define CEIL_DIV(a,b) (((a)+(b)-1)/(b))
#endif

// Returns true if mask is a subset of mask_set
// Remember, 0 (the empty set) is a subset of every set. ie. make sure that
// mask is actually a mask, otherwise you will receive unexpected results
#define IS_MASK_SUBSET(mask, mask_set) (!((~(mask_set)) & (mask)))

#define __MODS_UTILITY_CATNAME_INTERNAL(a, b) a##b
#define __MODS_UTILITY_CATNAME(a, b) __MODS_UTILITY_CATNAME_INTERNAL(a, b)

//! Creates local variable which measures time between its construction and destruction.
//!
//! Usage:
//! @code{.cpp}
//! MODS_STOPWATCH("This function");
//! MODS_STOPWATCH(__FUNCTION__);
//! @endcode
#define MODS_STOPWATCH(name) Utility::StopWatch __MODS_UTILITY_CATNAME(__timer, __LINE__)(name)

namespace Utility
{
    enum CriticalEvents
    {
       CE_USER_ABORT  = 0x00000001,
       CE_FATAL_ERROR = 0x00000002
    };
}

namespace Utility
{
    using ::AlignUp;
    using ::AlignDown;
}


// Some other headers also define these macros
#ifdef ALIGN_DOWN
#undef ALIGN_DOWN
#endif
#ifdef ALIGN_UP
#undef ALIGN_UP
#endif

// macro to align a value down to nearest alignment point
#define ALIGN_DOWN(val, align) Utility::AlignDown(val, align)
// macro to align a value up to nearest alignment point
#define ALIGN_UP(val, align) Utility::AlignUp(val, align)

//!
//! \def UNSIGNED_CAST(type, value)
//!
//! \brief Perform a bounds checked cast from a numeric type to an integral
//! type.
//!
//! \deprecated Use #CHECKED_CAST instead. This macro was updated to support
//! arbitrary numeric types to arbitrary integral types making the
//! identifier \a UNSIGNED_CAST a misnomer.
//!
//! \param type Target type.
//! \param value Value to colwert.
//!
//! \see #CHECKED_CAST
//!
#if defined(DEBUG)
    #define UNSIGNED_CAST(type, value) \
    Utility::CheckedCast<type, decltype(value)>((value), #type, __LINE__, \
                                                 __FILE__, __func__, #value)
#else
    #define UNSIGNED_CAST(type, value) \
    Utility::CheckedCast<type, decltype(value)>((value), #type, __LINE__)
#endif

//!
//! \def CHECKED_CAST(type, value)
//!
//! \brief Perform a bounds checked cast from a numeric type to an integral
//! type.
//!
//! \param type Target type.
//! \param value Value to colwert.
//!
#define CHECKED_CAST(type, value) UNSIGNED_CAST(type, value)

namespace Utility
{
    namespace Details
    {
        struct AclwmulatorStop
        {
            template <
                typename IntegralType
              , typename Struct
              , typename Container
              >
            static
            IntegralType Accumulate(const Container& cont, IntegralType Struct::*member)
            {
                return cont.*member;
            }
        };
    }

    //! Aclwmulates `member` fields in a structure sored in any depth of nested containers
    template <std::size_t Depth>
    struct Aclwmulator
    {
        template <
              typename IntegralType
            , typename Struct
            , typename Container
            >
        static
        IntegralType Accumulate(const Container& cont, IntegralType Struct::*member)
        {
            IntegralType sum = 0;
            typename Container::const_iterator it;
            for (it = cont.begin(); cont.end() != it; ++it)
            {
                // `Depth` is just a counter that tells us when we have to stop
                // assuming that the members of the container are containers as
                // well. When `Depth` is equal 1, it means that the members of
                // the container are actual structures and we call
                // `Details::AclwmulatorStop` instead to accumulate the values
                // from a particular field. It also stops the relwrsion.
                typedef conditional_t<
                      (Depth > 1)
                    , Aclwmulator<Depth - 1>
                    , Details::AclwmulatorStop
                    > Next;
                sum += Next::template Accumulate<IntegralType, Struct>(*it, member);
            }
            return sum;
        }
    };

    //! Aclwmulates `member` fields in a vector of structures
    template <
        typename IntegralType
      , typename Struct
      >
    IntegralType Accumulate(const std::vector<Struct> &cont, IntegralType Struct::*member)
    {
        return Aclwmulator<1>::Accumulate(cont, member);
    }

    //! Aclwmulates `member` fields in a vector of vector of structures
    template <
        typename IntegralType
      , typename Struct
      >
    IntegralType Accumulate(
        const std::vector<std::vector<Struct> > &cont,
        IntegralType Struct::*member
    )
    {
        return Aclwmulator<2>::Accumulate(cont, member);
    }

    //! Aclwmulates `member` fields in a vector of vector of vector of structures
    template <
        typename IntegralType
      , typename Struct
      >
    IntegralType Accumulate(
        const std::vector<std::vector<std::vector<Struct> > > &cont,
        IntegralType Struct::*member
    )
    {
        return Aclwmulator<3>::Accumulate(cont, member);
    }

    // Bit flags
    enum FileExistence
    {
        NoSuchFile      = 0,
        FileUnencrypted = 1,
        FileEncrypted   = 2,
        FileDuplicate   = 3 // Both encrypted and unencrypted copy exist
    };

    //! Determines if a file from MODS package exists and whether it is encrypted
    //!
    //! Returns bit flags indicating whether the file is encrypted or not.  If the
    //! file does not exist, returns flags, which are equivalent of integer 0, so
    //! the return value can be used directly in if () statements if the knowledge
    //! of whether the file is encrypted is not required.
    //!
    //! @param fileName Name or path of the file to search for.  The name must not
    //!                 include the trailing 'e' used for encrypted files.
    //!
    //! @param pOutFillPath Optional argument, the full path to the file found may
    //!                     be returned through this argument.  If the file was
    //!                     found both encrypted and unencrypted, this function
    //!                     returns only the path to the encrypted copy.
    //!
    FileExistence FindPkgFile(const string& fileName, string* pOutFullPath = nullptr);
}

//! The cleanup base class.
class CleanupBase
{
public:
    CleanupBase() = default;
    CleanupBase(const CleanupBase&) = delete;
    CleanupBase& operator=(const CleanupBase&) = delete;

    void Activate()
    {
        m_Active = true;
    }

    void Cancel()
    {
        m_Active = false;
    }

    bool IsActive() const
    {
        return m_Active;
    }

private:
    bool m_Active = true;
};

//! The Utility namespace.
namespace Utility
{
    INT32 GetShellFailStatus();

    [[ deprecated("Use namespace Security") ]]
    bool OnLwidiaIntranet();

    //------------------------------------------------------------------------------
    //! Return a timestamp encoded filename based on the local time.
    //! pPrefixName : A string that will be prepended to the timestamp string.
    //! pExt : the file extention (doesn't include the '.')
    //!
    //! The timestamp string portion that is returned has the form of dd_hh_mm_ss where
    //! dd = day of the month (1-31)
    //! hh = hour of the day (0-23)
    //! mm = minute of the hours (0-59)
    //! ss = second of the minute (0-59)
    //! 
    string GetTimestampedFilename(const char *pPrefixName, const char * pExt);

    string FindFile(string FileName, const vector<string> & Directories);
    //!< Search for the file in the given directories.
    //!< If found, returns the directory name, else return an empty string.

    //! \brief Performs search for a file in standard directories
    //!
    //! Returns path plus filename.
    //! Extended search includes more directories vs only current working
    //! when ExtendedSearch is false
    string DefaultFindFile
    (
        const string& FileName, //!< Filename possibly also including path as provided by user
        bool ExtendedSearch     //!< Enables search in more directories (ProgramPath, ScriptFilePath)
    );

    //! Append all the paths specified by the elw variable "MODSP" to the list of
    //! directories to search.
    void AppendElwSearchPaths(vector<string> * Paths,string elwVar = "");

    //! Returns a filename based on fname_template that doesn't collide with
    //! an existing file in the same directory.
    //!
    //! Does this by inserting a 4-digit decimal number between 0000 and 9999
    //! in front of the last '.' in fname_template.
    //!
    //! For example, if fname_template is "./mods.jso" and the current directory
    //! has files mods0000.jso and mods0001.jso, returns "./mods0002.jso".
    string CreateUniqueFilename (const string & fname_template);

    RC FlushInputBuffer();
    //!< Flush the input buffer.

    INT32 ColwertFloatToFXP(  FLOAT64 Input, UINT32 intBits,
                              UINT32 fractionalBits);
    UINT32 ColwertFloatTo12_20( FLOAT64 Input );
    UINT32 ColwertFloatTo16_16( FLOAT64 Input );
    UINT16 ColwertFloatTo12_4(  FLOAT64 Input );
    UINT32 ColwertFloatTo2_24(  FLOAT64 Input );
    UINT16 ColwertFloatTo8_8(   FLOAT64 Input );
    UINT32 ColwertFloatTo0_32(  FLOAT64 Input );
    UINT32 ColwertFloatTo32_0(  FLOAT64 Input );

    FLOAT64 ColwertFXPToFloat( INT32 Input, UINT32 intBits,
                               UINT32 fractionalBits);
    FLOAT64 Colwert12_20ToFloat( UINT32 Input );
    FLOAT64 Colwert16_16ToFloat( UINT32 Input );
    FLOAT64 Colwert12_4ToFloat(  UINT16 Input );
    FLOAT64 Colwert2_24ToFloat(  UINT32 Input );
    FLOAT64 Colwert8_8ToFloat(   UINT16 Input );

    // Support for lw3x half-float formats:
    FLOAT32 Float16ToFloat32( UINT16 in);
    UINT16  Float32ToFloat16( FLOAT32 in);

    // Support for e8m7 float formats
    FLOAT32 E8M7ToFloat32(UINT16 f16);
    UINT16 Float32ToE8M7_RZ(FLOAT32 fin);

    string ColwertHexToStr(UINT32 input);
    UINT32 ColwertStrToHex(const string& input);

    //! General benchmarking functions.
    FLOAT64 Benchmark(void (*func)(void), UINT32 loops);
    FLOAT64 Benchmark(void (*func)(void*), void* arg, UINT32 loops);

    //! Look at the file error status and return an RC:: code
    //! This function always returns an error code, even if errno is zero.
    RC InterpretFileError();

    //! Open a file.  If you get back OK, your file pointer is guaranteed to
    //! be valid.  This function will search in multiple directories for
    //! the file.
    RC OpenFile(const char  *FileName, FILE **fp, const char *Mode);
    RC OpenFile(const string& FileName, FILE **fp, const char *Mode);

    //! Read data from a file within a tar file. Data can be null
    RC ReadTarFile(const char* tarfilename, const char* filename, UINT32* pSize, char* pData);

    //! Write to a file
    RC FWrite(const void *ptr, size_t Size, size_t NumMembers, FILE *fp);
    //! Read from a file
    size_t FRead(void *ptr, size_t Size, size_t NumMembers, FILE *fp);

    //! Return the size of a file.  If you get back OK, the filesize is guaranteed
    //! to be valid
    RC FileSize(FILE *fp, long *pFileSize);

    //! Compare two files byte by byte.  RC is for file errors, FilesMatch indicates
    //! the result of the comparison.
    RC CompareFiles(const char *FileOneName, const char *FileTwoName, bool *FilesMatch);

    //! Gets the line and column numbers in a file given an offset into the file
    RC GetLineAndColumnFromFileOffset
    (
        const vector<char> & fileData,
        size_t offset,
        size_t * pLine,
        size_t * pColumn
    );


    //! Find the integer log base 2.  MASSERT if num is not a power of 2.
    UINT32 Log2i(UINT32 num);

    //! Returns word with the high-order bits masked off,
    //! keeping the n low-order set bits.
    //! If Value has less set bits than n, return original Value.
    UINT32 FindNLowestSetBits(UINT32 Value, UINT32 n);

    //! Copy 32 conselwtive bits from a vector<bool> into a UINT32,
    //! starting at startIndex.  All bits past the end of the vector
    //! are assumed to be 0.
    UINT32 PackBits(const vector<bool> &bits, UINT32 startIndex = 0);

    //! Copy 32 bits from a UINT32 into a vector<bool>, starting at
    //! startIndex.  Other bits in the vector are unchanged.  The
    //! vector is auto-resized to hold the largest nonzero bit.
    void UnpackBits(vector<bool> *pDest, UINT32 src, UINT32 startIndex = 0);

    template<typename S, typename D>
    RC CopyBits(const S &src, D &dst, UINT32 srcBitOffset, UINT32 dstBitOffset, UINT32 numBits)
    {
        typedef typename S::value_type SrcValueType;
        typedef typename D::value_type DstValueType;

        const UINT32 srcBitSize = 8 * sizeof(SrcValueType);
        const UINT32 dstBitSize = 8 * sizeof(DstValueType);
        if (src.size() < ((srcBitOffset + numBits + srcBitSize -1) / srcBitSize))
        {
            Printf(Tee::PriNormal, "src array is too small to supply requested bits.\n");
            return RC::SOFTWARE_ERROR;
        }
        if (dst.size() < ((dstBitOffset + numBits + dstBitSize - 1) / dstBitSize))
        {
            Printf(Tee::PriNormal, "dst array is too small to hold requested bits.\n");
            return RC::SOFTWARE_ERROR;
        }
        // cal starting index for src & dst
        int srcIdx      = srcBitOffset / srcBitSize;
        int srcOffset   = srcBitOffset % srcBitSize;
        int dstIdx      = dstBitOffset / dstBitSize;
        int dstOffset   = dstBitOffset % dstBitSize;
        UINT32 bitsToCpy   = 0;
        while (numBits > 0)
        {
            // make sure we don't overrun src or dst boundries
            bitsToCpy = min(dstBitSize - dstOffset, srcBitSize - srcOffset);
            // don't copy more than we have.
            bitsToCpy = min(bitsToCpy, numBits);
            UINT64 srcData = (((SrcValueType)src[srcIdx]) >> srcOffset) &
                               (((UINT64)1 << bitsToCpy) - 1);
            srcData = srcData << dstOffset;

            DstValueType dstMask = ~((((UINT64)1 << bitsToCpy) - 1) << dstOffset);
            dst[dstIdx] = static_cast<DstValueType> (
                (((DstValueType)dst[dstIdx]) & dstMask) | srcData);

            // update for next copy
            srcIdx      += (bitsToCpy+srcOffset) / srcBitSize;
            srcOffset    = (bitsToCpy+srcOffset) % srcBitSize;
            dstIdx      += (bitsToCpy+dstOffset) / dstBitSize;
            dstOffset    = (bitsToCpy+dstOffset) % dstBitSize;
            numBits     -= bitsToCpy;
        }
        return OK;
    }

    //! Swap the bytes in a 16-bit or 32-bit word
    inline UINT16 ByteSwap16(UINT16 val)
    {
        return (val >> 8) | (val << 8);
    }
    inline UINT32 ByteSwap32(UINT32 val)
    {
        return (val >> 24) | (val << 24) |
               ((val & 0x00FF0000) >> 8) |
             ((val & 0x0000FF00) << 8);
    }
    inline UINT64 ByteSwap64(UINT64 val)
    {
        return (val >> 56) | (val << 56) |
               ((val & 0x00FF000000000000ULL) >> 40) |
               ((val & 0x0000FF0000000000ULL) >> 24) |
               ((val & 0x000000FF00000000ULL) >>  8) |
               ((val & 0x00000000FF000000ULL) <<  8) |
               ((val & 0x0000000000FF0000ULL) << 24) |
               ((val & 0x000000000000FF00ULL) << 40);
    }
    inline UINT32 WordSwap32(UINT32 val)
    {
        return (val >> 16) | (val << 16);
    }

    //! Byte-swap a block of memory according to a word size
    void ByteSwap(UINT08 *Buf, UINT64 Size, UINT32 WordSize);

    //! Given a fraction NumIn/DenIn, find a close approximation
    //! NumOut/DenOut such that the maximum error is
    //! (NumIn/DenIn)/Tolerance.  The goal is to find something that we
    //! can do integer math on without overflowing.
    void GetRationalApproximation
    (
        UINT64 NumIn,
        UINT64 DenIn,
        UINT32 Tolerance,
        UINT64 *pNumOut,
        UINT64 *pDenOut
    );

    //! Pause the thread for given amount of 'Microseconds'.
    //! This function DOES NOT yield the CPU to other threads.
    //! Warning, keep the delay as short as possible so the other threads
    //! do not starve.
    //! Also see Tasker::Sleep() for longer delay periods (milliseconds)
    //! Also see SleepUs for short delays that do yield the CPU
    void Delay(UINT32 Microseconds);
    void DelayNS(UINT32 Nanoseconds);

    //! This function only guarantees that it will sleep for at *least*
    //! MinMicrosecondsToSleep because yielding the processor could result
    //! in a delay longer than the time specified.
    void SleepUS(UINT32 MinMicrosecondsToSleep);

    //! Return the number of time in seconds since the Epoch.  Returning
    //! UINT32 rather than time_t so we don't confuse JavaScript
    UINT32 GetSystemTimeInSeconds();

    //Colwert Unix epoch time to
    // [YYYY-MM-DD, HH:MM:SS] format in GMT
    string ColwertEpochToGMT(UINT32 epoch);


    //! colwert a string to a 64-bit integer
    UINT64 Strtoull (const char* nptr, char** endptr, int base);

    //! Duplicate a string, the caller must free the data.
    char * StrDup(const char * str, UINT32 len = 0);

    //! Tokenize a string specified in 'src'. Break it down by characters
    //! specified in 'delimiter'. The tokens are pushed into 'pTokens'
    RC Tokenizer(const string &src,
                const string &delimiter,
                vector<string> *pTokens);

    //! Tokenize the provided buffer.  The buffer is altered in the process.
    //! Pointers to the first instance of non-whitespace characters are
    //! pushed into the provided vector and the first whitespace character
    //! after the token start is replaced with '\0'.  '#' is interpreted as
    //! a comment character and all data from '#' until '\n' is ignored.  If
    //! newline insertion is enabled, then the static new line token is
    //! inserted where a new line would have been.  Note that only a single
    //! newline will be inserted even if there are multiple conselwtive new
    //! lines in the provided buffer.
    void TokenizeBuffer(char                 *buffer,
                       UINT32                bufferSize,
                       vector<const char *> *pTokens,
                       bool                  bInsertNewLines);
    bool IsNewLineToken(const char *token);

    RC ParseIndices(const string &input, set<UINT32> *pOut);

    RC StringToUINT32(const string& numStr, UINT32* pNumber, UINT32 base);
    RC StringToUINT32(const string& numStr, UINT32* pNumber);

    // Prompt the user with 'prompt' until they hit Y/y/N/n.  If they hit N,
    // print 'messageForNo' and return rcForNo.  If they hit Y, return OK.
    RC GetOperatorYesNo(const char *prompt, const char *messageForNo, RC rcForNo);

    //! \brief Returns only path portion from path+filename string
    string StripFilename
    (
        const char *filenameWithPath //!< path plus filename input
    );

    //! \brief Appends tabs to a string to expand it to have an arbitrary size
    string ExpandStringWithTabs
    (
        const string& str,
        UINT32 targetSize,
        UINT32 tabSize
    );

    //! \brief Colwert binary data to a base64 ascii string
    string EncodeBase64(const vector<UINT08> &inputData);

    //! \brief Colwert a base64 ascii string to binary data
    vector<UINT08> DecodeBase64(const string &inputString);

    //! \brief Compress binary data using the zlib "deflate" algorithm
    //! \param pData Uncompressed data on entry, compressed data on exit.
    RC Deflate(vector<UINT08> *pData);

    //! \brief Uncompress binary data using the zlib "inflate" algorithm
    //! \param pData Compressed data on entry, uncompressed data on exit.
    RC Inflate(vector<UINT08> *pData);

    //! \brief Returns combined paths given in arguments
    //!
    //! Takes into account leading and ending slashes in arguments
    string CombinePaths
    (
        const char *PathLeft, //!< left portion
        const char *PathRight //!< right portion (possibly includes filename)
    );

    string ToUpperCase(string Buff);
    string ToLowerCase(string Buff);

    string StringReplace(const string &str_in,
                        const string &str_find,
                        const string &str_replace);

    string FixPathSeparators(const string& inPath);

    void CreateArgv(const string &buffer, vector<string> *elements);

    //strcasecmp is defined as a standard function under linux but is absent under windows.
    //This function is redefined to be cross-platform.
    int strcasecmp(const char *s1, const char *s2);

    void PrintMsgAlways(const char*);
    void PrintMsgHigh(const char*);

    //! Compares an argument string with a wildcard expression.
    //! Returns true if the strings match
    bool MatchWildCard(const char* str, const char* wildcard);

    enum BufferDataFormat
    {
        RAW = 0,
        BIN = 2,
        OCT = 8,
        DEC = 10,
        HEX = 16
    };

    inline INT32 RAWToHEX(char ch)
    {
        if (ch >= 'A' && ch <= 'F')
            return ch - 'A' + 10;
        if (ch >= 'a' && ch <= 'f')
            return ch - 'a' + 10;
        return ch - '0';
    }

    //! Used to open and read and load the buffer
    //! Returns RC as per the function exelwtion
    //! FileName: Name of the File from which to read the Data
    //! Buffer: Resulted Buffer output
    //! BufferSize : Resulted Buffer Size after the Read
    //! BufferDataFormat: BufferBaseFormat data can be read in  Hex or octal or Binary or decimal format
    //! Hex - 16 Decimal - 10 Binary -2 Ocatl - 8
    RC AllocateLoadFile
    (
        const char *Filename,
        UINT08 *&Buffer,
        UINT32 *BufferSize,
        BufferDataFormat Base = RAW
    );

    //! If the JS variable g_DumpPrograms is true, create a unique file
    //! (using CreateUniqueFilename) and write the program to it.
    //!
    //! If filenameTemplate is "abc.lwbin", and ErrorMap::Test() is 99, the
    //! resulting file will be named "t99_abc0000.lwbin" where the 0000 will
    //! increment to avoid overwriting any existing file.
    RC DumpProgram
    (
        const string & filenameTemplate,
        const void*  buffer,
        const size_t bytes
    );

    //! Dump a text file.
    RC DumpTextFile
    (
        INT32        Priority,
        const char  *FileName
    );

    // Types of file encryption that may be used
    enum EncryptionType
    {
        NOT_ENCRYPTED,
        ENCRYPTED_JS,
        ENCRYPTED_LOG,
        ENCRYPTED_JS_V2,
        ENCRYPTED_LOG_V2,
        ENCRYPTED_FILE_V3,
        ENCRYPTED_LOG_V3
    };

    //! Used to check if a file is encrypted (note that the file must be opened
    //! first)
    EncryptionType GetFileEncryption(FILE* pFile);

    // Find and read a possibly-encrypted file
    RC ReadPossiblyEncryptedFile(const string &fileName, vector<char> *pBuffer,
                                 bool *pIsEncrypted);

    // Redact string if necessary and capable
    string RedactString(string val);

    // Un-redact string if capable
    string RestoreString(string val);

    //! Returns information about the type of MODS build.
    string GetBuildType();

    //! Returns list of builtin modules
    string GetBuiltinModules();

    //! Ilwokes a JS function if it exists, otherwise returns cleanly.
    //! Useful for optional JS callbacks.
    //! The JS function is supposed to return an RC.
    RC CallOptionalJsMethod(const char* FunctionName);

    char* RemoveHeadTailWhitespace(char* str);
    string RemoveSpaces(string str);

    //! Finds greates common factor/denominator of two numbers.
    UINT32 Gcf(UINT32 Num1, UINT32 Num2);

    enum ExitMode {ExitNormally, ExitQuickAndDirty};
    int ExitMods(RC ExitCode, ExitMode Mode);

    //! Colwert from Javascript data into C data.  Also runs PreparePickItems().
    RC PickItemsFromJsval(jsval js, vector<Random::PickItem> *Items);

    RC InterpretModsUtilErrorCode(LwDiagUtils::EC ec);

    FLOAT64 CallwlateConfidenceLevel
    (
        FLOAT64 timeSec,
        FLOAT64 bitRateMbps,
        UINT32  errors,
        FLOAT64 desiredBER
    );

    //! Loads a shared library which came in MODS package.
    //!
    //! This function specifically avoid leading random libs from the system
    //! or from LD_LIBRARY_PATH.
    //!
    //! @param fileName     Base name of the library file.  It can be without the file
    //!                     file extension, which will be appended automagically.
    //! @param moduleHandle Pointer to module handle which is returned from the function.
    //! @param loadDLLFlags Load flags, passed to Xp::LoadDLL().
    //! @param pFullPath    Optional pointer to string which will contain full
    //!                     path to the library file being loaded.
    //!                     The path is set even if the function fails.
    //!
    RC LoadDLLFromPackage(const char* fileName,
                          void**      moduleHandle,
                          UINT32      loadDLLFlags, //!< enum LoadDLLFlags
                          string*     pFullName = nullptr);

    //! Temporarily assigns a value to a variable for the lifetime
    //! of TempValue object. Restores the original value upon destruction.
    template<typename T>
    class TempValue
    {
    public:
        TempValue(T& Object, const T& Value)
        : m_pObject(&Object), m_OldValue(Object)
        {
            *m_pObject = Value;
        }
        ~TempValue()
        {
            if (m_pObject != NULL)
            {
                *m_pObject = m_OldValue;
            }
        }
        void MakePermanent()
        {
            m_pObject = NULL;
        }

    private:
        T* m_pObject;
        const T m_OldValue;
    };

    //! Restores Value to Object upon destruction.
    template<typename T>
    class CleanupValue : public CleanupBase
    {
    public:
        CleanupValue(T& Object, const T& Value)
        :m_Object(&Object), m_Value(Value)
        {
        }

        ~CleanupValue()
        {
            if (IsActive())
            {
                MASSERT(m_Object != NULL);
                *m_Object = m_Value;
            }
        }

    private:
        T* m_Object;
        const T m_Value;
    };

    template<typename Func>
    class DeferredCleanup
    {
        public:
            explicit DeferredCleanup(Func f) : m_Func(f), m_Active(true) { }

            ~DeferredCleanup() noexcept(false)
            {
                if (m_Active)
                {
                    m_Func();
                }
            }

            DeferredCleanup(DeferredCleanup&& f)
            : m_Func(move(f.m_Func)), m_Active(true)
            {
                f.m_Active = false;
            }

            void Cancel()
            {
                m_Active = false;
            }

            DeferredCleanup(const DeferredCleanup&)            = delete;
            DeferredCleanup& operator=(const DeferredCleanup&) = delete;

        private:
            Func m_Func;
            bool m_Active;
    };

    class GenDeferObject
    {
        public:
            template<typename Func>
            DeferredCleanup<Func> operator+(Func f) const
            {
                return DeferredCleanup<Func>(f);
            }
    };

#define DEFERNAME(name) auto name = Utility::GenDeferObject() + [&]

    //! Runs a particular piece of code when the current scope ends.
    //!
    //! Example:
    //!
    //!     Surface2D surf;
    //!     /* ... */
    //!     CHECK_RC(surf.Alloc());
    //!     DEFER
    //!     {
    //!         surf.Free();
    //!     };                  // <-- NOTE MANDATORY SEMICOLON !!!

#define DEFER DEFERNAME(__MODS_UTILITY_CATNAME(_defer_, __LINE__))

    //! Upon destruction, ilwoke member function with specified argument
    template<typename T, typename Ret, typename U>
    class CleanupOnReturnArg : public CleanupBase
    {
    public:
        typedef Ret (T::*CleanupFunction)(U);
        CleanupOnReturnArg(T* pObject, CleanupFunction cleanupFunction, U arg)
            : m_pObject(pObject), m_CleanupFunction(cleanupFunction), m_arg(arg)
        {
        }

        ~CleanupOnReturnArg()
        {
            if (IsActive())
            {
                MASSERT(m_pObject != NULL);
                (m_pObject->*m_CleanupFunction)(m_arg);
            }
        }

    private:
        T* m_pObject;
        CleanupFunction m_CleanupFunction;
        U m_arg;
    };

    //! Upon destruction, ilwoke function with specified argument
    template<typename Ret, typename U>
    class CleanupOnReturnArg <void, Ret, U> : public CleanupBase
    {
    public:
        typedef Ret (*CleanupFunction)(U);
        CleanupOnReturnArg(CleanupFunction cleanupFunction, U arg)
            : m_CleanupFunction(cleanupFunction), m_arg(arg)
        {
        }
        ~CleanupOnReturnArg()
        {
            if (IsActive())
            {
                MASSERT(m_CleanupFunction != NULL);
                (*m_CleanupFunction)(m_arg);
            }
        }

    private:
        CleanupFunction m_CleanupFunction;
        U m_arg;
    };

    //! Upon destruction ilwokes specified member function on an object.
    template<typename T, typename Ret=void>
    class CleanupOnReturn : public CleanupBase
    {
    public:
        typedef Ret (T::*CleanupFunction)();
        CleanupOnReturn(T* pObject, CleanupFunction cleanupFunction)
        : m_pObject(pObject), m_CleanupFunction(cleanupFunction)
        {
        }
        ~CleanupOnReturn()
        {
            if (IsActive())
            {
                MASSERT(m_pObject != NULL);
                (m_pObject->*m_CleanupFunction)();
            }
        }

    private:
        T* m_pObject;
        CleanupFunction m_CleanupFunction;
    };

    //! Upon destruction ilwokes specified function.
    template<typename Ret>
    class CleanupOnReturn <void, Ret> : public CleanupBase
    {
    public:
        typedef Ret (*CleanupFunction)();
        explicit CleanupOnReturn(CleanupFunction cleanupFunction)
        : m_CleanupFunction(cleanupFunction)
        {
        }
        ~CleanupOnReturn()
        {
            if (IsActive())
            {
                MASSERT(m_CleanupFunction != NULL);
                (*m_CleanupFunction)();
            }
        }

    private:
        CleanupFunction m_CleanupFunction;
    };

    //! Set a pointer if it's non-null.  Mostly useful for optional
    //! pass-by-reference function args.
    template<typename T> inline void SetOptionalPtr(T *ptr, T value)
    {
        if (ptr != NULL)
        {
            *ptr = value;
        }
    }

    inline bool IsNan(double x)
    {
        // NAN "not a number" is returned by sqrt(-1.0) and similar operations.
        // Floating-point NAN values are != themselves.
        // All other float values == themselves.
        return x != x;
    }
    bool IsInf(double x);

    enum [[ deprecated("Use namespace Security") ]]
    SelwrityUnlockLevel
    {
         SUL_UNKNOWN               = static_cast<INT32>(Security::UnlockLevel::UNKNOWN)
        ,SUL_NONE                  = static_cast<INT32>(Security::UnlockLevel::NONE)
        ,SUL_BYPASS_EXPIRED        = static_cast<INT32>(Security::UnlockLevel::BYPASS_EXPIRED)
        ,SUL_BYPASS_UNEXPIRED      = static_cast<INT32>(Security::UnlockLevel::BYPASS_UNEXPIRED)
        ,SUL_LWIDIA_NETWORK        = static_cast<INT32>(Security::UnlockLevel::LWIDIA_NETWORK)
        ,SUL_LWIDIA_NETWORK_BYPASS = static_cast<INT32>(Security::UnlockLevel::LWIDIA_NETWORK_BYPASS)
    };
    [[ deprecated("Use namespace Security") ]]
    void InitSelwrityUnlockLevel(SelwrityUnlockLevel bypassUnlockLevel);
    [[ deprecated("Use namespace Security") ]]
    SelwrityUnlockLevel GetSelwrityUnlockLevel();

    //! Timer which measures how long a given portion of code takes.
    //!
    //! Objects of this class should be created on the stack.
    //! It is colwenient to use it with the MODS_STOPWATCH macro.
    //! The destructor prints time in ms how long it took since
    //! constructor was exelwted.
    //!
    //! @sa MODS_STOPWATCH
    class StopWatch
    {
        public:
            explicit StopWatch(string name, Tee::Priority pri = Tee::PriNormal);
            explicit StopWatch(const char* name, Tee::Priority pri = Tee::PriNormal)
                : StopWatch(string(name), pri) { }
            explicit StopWatch(int num, Tee::Priority pri = Tee::PriNormal)
                : StopWatch(StrPrintf("StopWatch %d", num), pri) { }
            ~StopWatch() { Stop(); }
            UINT64 Stop();

            StopWatch(const StopWatch&)            = delete;
            StopWatch& operator=(const StopWatch&) = delete;

        private:
            const string m_Name;
            UINT64       m_StartTime;
            Tee::Priority m_Pri;
    };

    //! Timer which measures total time spent inside a section of a loop over multiple iterations.
    //!
    //! Objects of this class should be created on the stack.
    //! The destructor prints time in ms how long each PartialStopWatch
    //! section took.
    //!
    //! Usage:
    //! @code{.cpp}
    //! Utility::TotalStopWatch tt("My fancy loop");
    //! for (/*...*/)
    //! {
    //!     // ... some code here ...
    //!     Utility::PartialStopWatch partial(tt);
    //! }
    //! @endcode
    class TotalStopWatch
    {
        public:
            explicit TotalStopWatch(string name);
            explicit TotalStopWatch(const char* name)
                : TotalStopWatch(string(name)) { }
            explicit TotalStopWatch(int num)
                : TotalStopWatch(StrPrintf("StopWatch %d", num)) { }
            ~TotalStopWatch();

            friend class PartialStopWatch;

            TotalStopWatch(const TotalStopWatch&)            = delete;
            TotalStopWatch& operator=(const TotalStopWatch&) = delete;

        private:
            const string m_Name;
            UINT64       m_TotalTime;
    };

    //! Partial timer for loops, used with TotalStopWatch
    //!
    //! Objects of this class should be created on the stack.
    //! The destructor adds the time took since constructor was exelwted
    //! to the total timer.
    //!
    //! See TotalStopWatch for example usage.
    //!
    //! @sa TotalStopWatch
    class PartialStopWatch
    {
        public:
            explicit PartialStopWatch(TotalStopWatch& timer);
            ~PartialStopWatch();

            PartialStopWatch(const PartialStopWatch&)            = delete;
            PartialStopWatch& operator=(const PartialStopWatch&) = delete;

        private:
            TotalStopWatch& m_Timer;
            const UINT64     m_StartTime;
    };

    //! Check if a value - of any type - fits within the bounds
    //! of an integral (signed or unsigned) type
    //!
    template<typename TypeUnderTest, typename InputType>
    constexpr bool IsWithinIntegralBounds(const InputType value)
    {
        static_assert(std::is_integral<TypeUnderTest>::value,
                      "Target type is not an integral type.");

        return ((value < 0) || (static_cast<UINT64>(value) <= static_cast<UINT64>(std::numeric_limits<TypeUnderTest>::max()))) && //$
               ((value > 0) || (static_cast<INT64>(value) >= static_cast< INT64>(std::numeric_limits<TypeUnderTest>::lowest()))); //$

        // When C++14 is fully working on all build systems, use this instead:
        // UINT64 maxLimit = static_cast<UINT64>(std::numeric_limits<TypeUnderTest>::max());
        // INT64  minLimit = static_cast< INT64>(std::numeric_limits<TypeUnderTest>::lowest());
        // // Check first against 0 because the second check colwerts both to UINT64,
        // // so if value is -1, it will lose its value
        // bool success = (value < 0) || (static_cast<UINT64>(value) <= maxLimit);
        // // Check first against 0 so that we can safely colwert to signed int64
        // // for the second check
        // success &=     (value > 0) || (static_cast<INT64>(value) >= minLimit);
        // return success;
    }

    //! Static cast to integral types with error reporting.
    //!
    //! The error reporting is done differently for debug and non-debug builds
    //! in order to control how much information is exposed
    //!
    template<typename Target, typename Source,
             typename = std::enable_if_t<std::is_integral<typename std::decay<Source>::type>::value ||      //$
                                         std::is_floating_point<typename std::decay<Source>::type>::value>, //$
             typename = std::enable_if_t<std::is_integral<Target>::value>>
    Target CheckedCast
    (
        const Source value,
        const char* typeName,
        const int   lineNumber
#if defined(DEBUG)
       ,const char* fileName,
        const char* functionName,
        const char* varName
#endif
    )
    {
        if (!IsWithinIntegralBounds<Target>(value))
        {
#if defined(DEBUG)
            Printf(Tee::PriError,
                   "Value of '%s' in function '%s' on line %d of file '%s' "
                   "is too large to fit in a %s\n",
                   varName,
                   functionName,
                   lineNumber,
                   fileName,
                   typeName);
            MASSERT(!"Variable overflow");
#else
            Printf(Tee::PriError,
                   "Cast to '%s' on line %d will cause variable overflow\n",
                   typeName,
                   lineNumber);
#endif
        }
        return static_cast<Target>(value);
    }

    static const UINT08 ASTC_BLOCK_SIZE = 16;
    struct ASTCData
    {
        UINT32 imageWidth;
        UINT32 imageHeight;
        UINT32 imageDepth;
        vector<UINT08> imageData;
    };

    /**
     * ReadAstcFile attempts to read image and meta data from a buffer containing
     * an ASTC file.  This function may return an error if the file is not
     * according to the ASTC file format specification, of if attempting to read
     * a 3D astc texture as this is lwrrently unsupported.  In case of error,
     * there is no guarantee on whether astcData will be modified or not.
     */
    RC ReadAstcFile(const vector<char>& astcBuf,
                    ASTCData*           astcData);

    /**
     * ExtractBitsFromU32Vector attempts to extract a range of bits from a vector of UINT32 values,
     * where regValues[0] has the lowest-significant bit.
     */
    template<class T>
    T ExtractBitsFromU32Vector(const vector<UINT32> &regValues, UINT16 high, UINT16 low)
    {
        // Check that the return value is big enough to store the range of bits
        MASSERT(static_cast<size_t>(high - low + 1) <= (sizeof(T) * 8));

        // Check that the high-bit is within the vector size
        MASSERT(high < (regValues.size() * sizeof(UINT32) * 8));
        MASSERT(low <= high);

        T retVal = 0;

        UINT16 nextBitToSet = 0;
        for (UINT16 i = 0; i < regValues.size(); i++)
        {
            // If we're on a DWORD that's beyond our high-bit, we can stop parsing
            if (high < i*32)
            {
                break;
            }

            // If we're on a DWORD that's below our low-bit, skip it
            if (low >= (i+1)*32)
            {
                continue;
            }

            // If we're on a DWORD that contains our low-bit, we'll want to consume only the bits
            // starting from the low-bit; otherwise we'll want all the bits in this DWORD
            UINT16 lwrLow = 0;
            if (low >= i*32)
            {
                lwrLow = low % 32;
            }

            // If we're on a DWORD that's before the high-bit, then we'll want all the bits up to
            // the end of this DWORD, otherwise we'll stop at the high-bit
            UINT16 lwrHigh = 31;
            if (high < (i+1)*32)
            {
                lwrHigh = high % 32;
            }

            // tempVal will hold the equivalent of REF_VAL()
            T tempMask = _UINT32_MAX >> (31 - lwrHigh + lwrLow);
            T tempVal = (regValues[i] >> lwrLow) & tempMask;

            // Shift the data from this DWORD to the appropriate location in the return val
            retVal |= tempVal << nextBitToSet;

            // Remember where we're going to set the next bit of the return value based on the
            // number of bits that have already been set
            nextBitToSet += lwrHigh - lwrLow + 1;
        }

        return retVal;
    }

    string FormatRawEcid(const vector<UINT32>& rawEcid);
    void GetRandomBytes(UINT08 *bytes, UINT32 size);
    string FormatBytesToString(const UINT08 *bytes, UINT32 size);
    //!
    //! \brief True if the given container contains the given value.
    //!
    //! \tparam Container Container type.
    //! \param container Container to check.
    //! \param val Search value.
    //!
    template<class Container>
    bool Contains(const Container& container, const typename Container::value_type& val)
    {
        return std::find(container.begin(), container.end(), val) != container.end();
    }

    //! Colwert LwTemp to Celsius
    //!
    FLOAT32 FromLwTemp(LwTemp temp);

    //! Colwert Celsius to LwTemp
    //!
    LwTemp ToLwTemp(FLOAT32 temp);

    //!
    //! \brief Return true if the given value is within the given bit width.
    //!
    //! Starts checking from the 0th bit of value.
    //!
    //! \param value Value to check.
    //! \param bitWidth Bit width expected to fit value.
    //!
    constexpr bool IsWithinBitWidth(UINT32 value, UINT32 bitWidth)
    {
        return (bitWidth < (sizeof(UINT32) * CHAR_BIT))
            ? !(value & ~((1U << bitWidth) - 1U)) : true;
    }

    //!
    //! \brief Asserts and prints an error if the given value is within the given
    //! bit width.
    //!
    //! Starts checking from the 0th bit of value.
    //!
    //! \param value Value to check.
    //! \param bitWidth Bit width expected to fit value.
    //! \param pValueName Short description or name of the value being checked.
    //!
    void AssertWithinBitWidth(UINT32 value, UINT32 bitWidth, const char* pValueName);

    //!
    //! \brief Get uniform distribution of elements across 'x' bins
    //!       (lower index bin populated before the higher index ones)
    //!        Eg. 9 elements in 3 bins = [3,3,3]
    //!            7 elements in 3 bins = [3,2,2]
    //!
    //! \param numBins            Number of bins
    //! \param numElements        Number of elements to distribute within the bins
    //! \param[out] pDistribution Final distribution of elements in the bins
    //!
    void GetUniformDistribution
    (
        UINT32 numBins,
        UINT32 numElements,
        vector<UINT32> *pDistribution
    );
} // namespace Utility

template<typename T>
class ObjectHolder
{
public:
    typedef void (*DtorType)(T);
    template<typename U>
    ObjectHolder(T handle, U (*dtor)(T))
      : m_handle(handle), m_dtor(reinterpret_cast<DtorType>(dtor)) {}
    ~ObjectHolder()
    {
        if(m_dtor != NULL)
        {
            m_dtor(m_handle);
        }
    }
    operator T() const { return m_handle; }
    void Cancel()
    {
        m_dtor = NULL;
    }

private:
    ObjectHolder(const ObjectHolder&);
    ObjectHolder& operator=(const ObjectHolder&);

    T m_handle;
    DtorType m_dtor;
};

#define INTERFACE_HEADER(class_type)                                                              \
template <typename...>                                                                            \
struct voider { using type = void; };                                                             \
template <typename... Ts>                                                                         \
using void_t = typename voider<Ts...>::type;                                                      \
                                                                                                  \
/* A default, not specialized version, of has_is_supported<A, void> returns */                    \
/* false */                                                                                       \
template <typename, typename = void_t<>>                                                          \
struct has_is_supported : false_type                                                              \
{};                                                                                               \
                                                                                                  \
/* If some T has T::IsSupported, then the below version                       */                  \
/* has_is_supported<T, void> will be chosen and                               */                  \
/* has_is_supported<T, void>::value will be equal true. Note that in both     */                  \
/* cases the second parameter of the template is void, since void_t<T>        */                  \
/* colwerts any type to void. This is important in order to generate a        */                  \
/* correct specialized template. If T doesn't have T::IsSupported, then the   */                  \
/* below expression doesn't make sense and the compiler will fall back to     */                  \
/* the non-specialized version above. The error is not generated because of   */                  \
/* SFINAE (Substitution Failure Is Not An Error).                             */                  \
template <typename T>                                                                             \
struct has_is_supported<                                                                          \
    T                                                                                             \
  , void_t<decltype(declval<T&>().IsSupported())>                                                 \
  > : true_type                                                                                   \
{};                                                                                               \
                                                                                                  \
template <typename T>                                                                             \
typename enable_if<!has_is_supported<T>::value, bool>::type                                       \
SupportsInterface() const                                                                         \
{                                                                                                 \
    auto intf = dynamic_cast<const typename decay<T>::type*>(const_cast<class_type*>(this));      \
    return nullptr != intf;                                                                       \
}                                                                                                 \
                                                                                                  \
template <typename T>                                                                             \
typename enable_if<has_is_supported<T>::value, bool>::type                                        \
SupportsInterface() const                                                                         \
{                                                                                                 \
    auto intf = dynamic_cast<const typename decay<T>::type*>(const_cast<class_type*>(this));      \
    return nullptr != intf && intf->IsSupported();                                                \
}                                                                                                 \
                                                                                                  \
template <typename T>                                                                             \
const typename decay<T>::type* GetInterface() const                                               \
{                                                                                                 \
    return dynamic_cast<const typename decay<T>::type*>(this);                                    \
}                                                                                                 \
                                                                                                  \
template <typename T>                                                                             \
typename decay<T>::type* GetInterface()                                                           \
{                                                                                                 \
    return dynamic_cast<typename decay<T>::type*>(this);                                          \
}

//!
//! \brief Retrieve the value from the specified property of the given
//! JavaScript object.
//!
//! \tparam T Type of resulting extraced value.
//! \param pJso JS object.
//! \param pPropertyName Property name.
//! \param[out] pValue Value of the property.
//!
template<typename T>
RC GetJsPropertyValue(JSObject* pJso, const char* pPropertyName, T* pValue, bool printErr = true)
{
    MASSERT(pJso);
    MASSERT(pPropertyName);
    RC rc;

    JavaScriptPtr pJs;
    jsval jsValue;
    rc = pJs->GetPropertyJsval(pJso, pPropertyName, &jsValue);
    if (rc == RC::OK)
    {
        rc = pJs->FromJsval(jsValue, pValue);
    }

    if (rc != RC::OK && printErr)
    {
        Printf(Tee::PriError, "Unable to get property '%s'\n", pPropertyName);
    }

    return rc;
}

//!
//! \brief Function object for performing equality checks. Ilwokes \a
//! operator==.
//!
template<class T>
struct CmpEqual
{
    bool operator()(const T& a, const T& b) { return a == b; }
};

//!
//! \brief Removes duplicates from a sorted container.
//!
//! \tparam Container Type of the container.
//! \tparam BinaryPred Equality comparison function object to initialize the
//! underlying functor.
//! \param pContainer Container to remove duplicates from.
//!
template<class Container, class BinaryPred = CmpEqual<typename Container::value_type>>
void RemoveDuplicatesFromSorted(Container* pContainer)
{
    MASSERT(pContainer);
    pContainer->erase(std::unique(pContainer->begin(), pContainer->end(),
                                  BinaryPred()),
                    pContainer->end());
}

#endif // ! UTILITY_H__
