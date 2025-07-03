/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   utility.cpp
 * @brief  Implementation of the Utility namespace (utility.h)
 *
 * Also contains JavaScript Utility class.
 */

// Utility functions.

#include "core/include/utility.h"

#include "core/include/cmdline.h"
#include "core/include/cnslmgr.h"
#include "core/include/console.h"
#include "core/include/crc.h"
#include "core/include/deprecat.h"
#include "core/include/dllhelper.h"
#include "core/include/errormap.h"
#include "core/include/fileholder.h"
#include "core/include/gpu.h"      // relies on stub in non-GPU builds
#include "core/include/jscript.h"
#include "core/include/js_uint64.h"
#include "core/include/jsonlog.h"
#include "core/include/log.h"
#include "core/include/massert.h"
#include "core/include/memcheck.h"
#include "core/include/lwrm.h"     // relies on stub in non-GPU builds
#include "core/include/platform.h"
#include "core/include/script.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/version.h"
#include "core/include/xp.h"
#include "core/include/zlib.h"

#include "core/utility/errloggr.h"

// From diag/utils
#include "random.h"
#include "lwdiagutils.h"

// From diag/encryption
#include "decrypt.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <ctype.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <fcntl.h>
#endif

#if defined(Yield)
 #undef Yield
#endif

#if defined(BitScanForward)
 #undef BitScanForward
#endif

#if defined(BitScanReverse)
 #undef BitScanReverse
#endif

#if defined(BitScanForward64)
 #undef BitScanForward64
#endif

#if defined(BitScanReverse64)
 #undef BitScanReverse64
#endif

#if defined(DecryptFile)
 #undef DecryptFile
#endif

#include "core/utility/coreutility.cpp"

/**
 * @param FileName Name of file to search for.
 * @param Directories List of directories to search.  Each entry
 *                    may or may not end in the platform-specific
 *                    path-separator character.
 *
 * @return The directory name if the file is found, or the empty
 *         string if it is not.
 *
 * @sa Xp::DoesFileExist
 */
string Utility::FindFile
(
    string FileName,
    const vector<string> & Directories
)
{
    return LwDiagUtils::FindFile(FileName, Directories);
}

void Utility::AppendElwSearchPaths(vector<string> * Paths,string elwVar)
{
    return LwDiagUtils::AppendElwSearchPaths(Paths, elwVar);
}

string Utility::DefaultFindFile(const string& FileName, bool ExtendedSearch)
{
    return LwDiagUtils::DefaultFindFile(FileName, ExtendedSearch);
}

RC Utility::LoadDLLFromPackage(const char* fileName,
                               void**      moduleHandle,
                               UINT32      loadDLLFlags,
                               string*     pFullPath)
{
    const string completeFileName = DllHelper::AppendMissingSuffix(fileName);

    string fullPath = DefaultFindFile(completeFileName, true);

    // On Linux, it is necessary to put ./ for shared libs to be loaded
    // from the current directory.
    if (fullPath == completeFileName)
    {
        fullPath = FixPathSeparators("./" + fullPath);
    }

    if (pFullPath != nullptr)
    {
        *pFullPath = fullPath;
    }

    return Xp::LoadDLL(fullPath, moduleHandle, loadDLLFlags);
}

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
string Utility::GetTimestampedFilename(const char *pTestName, const char * pExt)
{
    const time_t now = time(0);
    struct tm* lwrTime = localtime(&now);
    return StrPrintf("%s_%02d_%02d_%02d_%02d_%02d.%s", pTestName, lwrTime->tm_mon+1,
                     lwrTime->tm_mday, lwrTime->tm_hour, lwrTime->tm_min, lwrTime->tm_sec, pExt);
}

//------------------------------------------------------------------------------
//! Returns a filename based on fname_template that doesn't collide with
//! an existing file in the same directory.
//!
//! Does this by inserting a 4-digit decimal number between 0000 and 9999
//! in front of the last '.' in fname_template.
//!
//! For example, if fname_template is "./mods.jso" and the current directory
//! has files mods0000.jso and mods0001.jso, returns "./mods0002.jso".

string Utility::CreateUniqueFilename (const string & fname_template)
{
    // template             desired results
    // --------             ---------------
    // mods.jso             mods0000.jso
    // abcdef.jso           abcdef00.jso (DOS) or abcdef0000.jso
    // ../x.y/mods.jso      ../x.y/mods0000.jso
    // ../x.y/mods          ../x.y/mods0000
    // a.b.c                a.b0000.c  (broken on DOS, user's fault)
    // .jso                 0000.jso
    // (empty)              0000

    string base;
    string ext;
    string::size_type iLastDot = fname_template.rfind('.');
    if (string::npos == iLastDot)
    {
        base = fname_template;
        ext  = "";
    }
    else
    {
        // fname_template without the last '.' and the extension if any.
        base = fname_template.substr(0, iLastDot);

        // The extension not including the '.'
        ext = fname_template.substr(iLastDot+1);
    }

    // Use 4 decimal digits for the "uniqueness" field.
    int numDigits = 4;

    int maxi = 1;
    for (int pow = 0; pow < numDigits; pow++)
        maxi *= 10;

    string tryFname;
    for (int i = 0; i < maxi; i++)
    {
        tryFname = Utility::StrPrintf("%s%0*d.%s",
                base.c_str(),
                numDigits,
                i,
                ext.c_str());

        if (! Xp::DoesFileExist(tryFname))
            return tryFname;
    }
    Printf(Tee::PriHigh, "Unable to create unique filename for \"%s\"\n",
            tryFname.c_str());

    return fname_template;
}

//------------------------------------------------------------------------------
// Flush the input buffer.
//
RC Utility::FlushInputBuffer()
{
    ConsoleManager::ConsoleContext consoleCtx;
    Console *pConsole = consoleCtx.AcquireRealConsole(true);
    while(pConsole->KeyboardHit())
    {
        pConsole->GetKey();
    }
    return OK;
}

UINT32 Utility::Log2i(UINT32 num)
{
    MASSERT(num != 0);
    MASSERT((num & (num - 1)) == 0);  // num must have exactly one bit set
    if (num == 0 || (num & (num - 1)) != 0)
        return 0xffffffff;
    return static_cast<UINT32>(BitScanForward(num));
}

UINT32 Utility::FindNLowestSetBits(UINT32 Value, UINT32 n)
{
    UINT32 Tmp = Value;
    for (UINT32 ii = 0; ii < n; ++ii)
        Tmp &= (Tmp - 1);
    return Value & ~Tmp;
}

UINT32 Utility::PackBits(const vector<bool> &bits, UINT32 startIndex)
{
    UINT32 packedValue = 0;
    UINT32 endIndex = min(startIndex + 32, static_cast<UINT32>(bits.size()));
    for (UINT32 ii = startIndex; ii < endIndex; ++ii)
    {
        if (bits[ii])
            packedValue |= (1 << (ii - startIndex));
    }
    return packedValue;
}

void Utility::UnpackBits(vector<bool> *pDest, UINT32 src, UINT32 startIndex)
{
    MASSERT(pDest != nullptr);

    INT32 highSrcBit = BitScanReverse(src);
    if ((highSrcBit >= 0) && (pDest->size() < startIndex + highSrcBit + 1))
    {
        pDest->resize(startIndex + highSrcBit + 1, false);
    }

    UINT32 endIndex = min(startIndex + 32, static_cast<UINT32>(pDest->size()));
    for (UINT32 ii = startIndex; ii < endIndex; ++ii)
    {
        (*pDest)[ii] = (src & (1 << (ii - startIndex))) != 0;
    }
}

//-----------------------------------------------------------------------------
void Utility::ByteSwap(UINT08 *Buf, UINT64 Size, UINT32 WordSize)
{
    UINT08 Temp;
    UINT64 i;

    switch (WordSize)
    {
        case 1:
            // Don't need to do anything
            break;
        case 2:
            MASSERT(!(Size & 1));
            for (i = 0; i < Size; i += 2)
            {
                Temp     = Buf[i+0];
                Buf[i+0] = Buf[i+1];
                Buf[i+1] = Temp;
            }
            break;
        case 4:
            MASSERT(!(Size & 3));
            for (i = 0; i < Size; i += 4)
            {
                Temp     = Buf[i+0];
                Buf[i+0] = Buf[i+3];
                Buf[i+3] = Temp;

                Temp     = Buf[i+1];
                Buf[i+1] = Buf[i+2];
                Buf[i+2] = Temp;
            }
            break;
        case 8:
            MASSERT(!(Size & 7));
            for (i = 0; i < Size; i += 8)
            {
                Temp     = Buf[i+0];
                Buf[i+0] = Buf[i+7];
                Buf[i+7] = Temp;

                Temp     = Buf[i+1];
                Buf[i+1] = Buf[i+6];
                Buf[i+6] = Temp;

                Temp     = Buf[i+2];
                Buf[i+2] = Buf[i+5];
                Buf[i+5] = Temp;

                Temp     = Buf[i+3];
                Buf[i+3] = Buf[i+4];
                Buf[i+4] = Temp;
            }
            break;
        default:
            MASSERT(!"Unsupported word size");
    }
}

//-----------------------------------------------------------------------------
void Utility::GetRationalApproximation
(
    UINT64 NumIn,
    UINT64 DenIn,
    UINT32 Tolerance,
    UINT64 *pNumOut,
    UINT64 *pDenOut
)
{
    MASSERT(pNumOut != NULL);
    MASSERT(pDenOut != NULL);

    // Make sure that NumIn/DenIn is between 0.0 and 1.0, noninclusive
    //
    if (NumIn == 0 || DenIn == 0 || NumIn == DenIn)
    {
        *pNumOut = NumIn ? 1 : 0;
        *pDenOut = DenIn ? 1 : 0;
        return;
    }
    else if (NumIn > DenIn)
    {
        GetRationalApproximation(DenIn, NumIn, Tolerance, pDenOut, pNumOut);
        return;
    }
    else if (Tolerance < DenIn / (DenIn - NumIn))
    {
        *pNumOut = 1;
        *pDenOut = 1;
        return;
    }

    // Use the Farey sequence to find the rational approximation with
    // smallest denominator that is close enough.
    //
    UINT64 NumLo = 0;
    UINT64 DenLo = 1;
    UINT64 NumHi = 1;
    UINT64 DenHi = (DenIn - 1) / NumIn;

    for (;;)
    {
        UINT64 NumMid = NumLo + NumHi;
        UINT64 DenMid = DenLo + DenHi;
        MASSERT(DenMid <= DenIn);

        INT64 NumErr = NumIn * DenMid - NumMid * DenIn;
        UINT64 NumErrAbs = (NumErr >= 0) ? NumErr : -NumErr;
        UINT64 DenErr = NumIn * DenMid;
        if (NumErr == 0 || Tolerance < DenErr / NumErrAbs)
        {
            *pNumOut = NumMid;
            *pDenOut = DenMid;
            return;
        }
        else if (NumErr > 0)
        {
            NumLo = NumMid;
            DenLo = DenMid;
        }
        else
        {
            NumHi = NumMid;
            DenHi = DenMid;
        }
    }
}

//-----------------------------------------------------------------------------
void Utility::Delay
(
    UINT32 Microseconds
)
{
    UINT64 End = Xp::QueryPerformanceCounter() +
        (Microseconds * Xp::QueryPerformanceFrequency() / 1000000);

    while (Xp::QueryPerformanceCounter() < End)
    {
    }
}

//-----------------------------------------------------------------------------
void Utility::DelayNS
(
    UINT32 Nanoseconds
)
{
    UINT64 End = Xp::QueryPerformanceCounter() +
        (Nanoseconds * Xp::QueryPerformanceFrequency() / 1000000000);

    while (Xp::QueryPerformanceCounter() < End)
    {
    }
}

//------------------------------------------------------------------------------
void Utility::SleepUS(UINT32 MinMicrosecondsToSleep)
{
    UINT64 End = Xp::QueryPerformanceCounter() +
        (MinMicrosecondsToSleep * Xp::QueryPerformanceFrequency() / 1000000);
    do
    {
        // Guarantee at least one yield like Tasker::Sleep does
        Tasker::Yield();
    } while (Xp::QueryPerformanceCounter() < End);
}

//-----------------------------------------------------------------------------
UINT32 Utility::GetSystemTimeInSeconds()
{
    return (UINT32) time(NULL);
}

//-----------------------------------------------------------------------------
string Utility::ColwertEpochToGMT(UINT32 epoch)
{
    time_t epochSec = epoch;
    struct tm *dateGMT;
    const UINT08 dateLength = 32;
    char tmp[dateLength];

    dateGMT = gmtime(&epochSec);
    strftime(tmp, dateLength, "%Y-%m-%d, %H:%M:%S %Z", dateGMT);
    string s(tmp, dateLength);
    return s;
}

//-----------------------------------------------------------------------------
/*
 * Colwert a string to an unsigned long long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 * Taken taken from binutils
 */
# undef ULONG_LONG_MAX
#define ULONG_LONG_MAX   18446744073709551615ULL
UINT64 Utility::Strtoull (const char* nptr, char** endptr, int base)
{
    const char *s = nptr;
    UINT64 acc;
    int c;
    UINT64 cutoff;
    int neg = 0, any, lwtlim;

    /*
     * See strtol for comments as to the logic used.
     */
    do {
        c = *s++;
    } while (isspace(c));
    if (c == '-') {
        neg = 1;
        c = *s++;
    } else if (c == '+')
        c = *s++;
    if ((base == 0 || base == 16) &&
            c == '0' && (*s == 'x' || *s == 'X')) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0)
        base = c == '0' ? 8 : 10;
    cutoff = (UINT64)ULONG_LONG_MAX / (UINT64)base;
    lwtlim = int((UINT64)ULONG_LONG_MAX % (UINT64)base);
    for (acc = 0, any = 0;; c = *s++) {
        if (isdigit(c))
            c -= '0';
        else if (isalpha(c))
            c -= isupper(c) ? 'A' - 10 : 'a' - 10;
        else
            break;
        if (c >= base)
            break;
        if (any < 0 || acc > cutoff || (acc == cutoff && c > lwtlim))
            any = -1;
        else {
            any = 1;
            acc *= base;
            acc += c;
        }
    }
    if (any < 0) {
        acc = ULONG_LONG_MAX;
        errno = ERANGE;
    } else if (neg)
        acc = -(long long)acc;
    if (endptr != 0)
        *endptr = const_cast<char *> (any ? s - 1 : nptr);
    return (acc);
}

//! Duplicate a string, the caller must free the data.
char * Utility::StrDup(const char * str, UINT32 len)
{
    if (str == NULL)
        return NULL;

    if (len == 0)
        len = (UINT32)strlen(str);

    char *newStr = new char[len+1];
    if (!newStr)
    {
        Printf(Tee::PriHigh, "StrDup: alloc failed\n");
        return NULL;
    }

    strncpy(newStr, str, len);
    newStr[len] = '\0';
    return newStr;
}

static const char * s_NewLineToken = "\n";

bool  Utility::IsNewLineToken(const char *token)
{
    return (token == s_NewLineToken);
}

RC Utility::Tokenizer(const string &src,
                      const string &delimiter,
                      vector<string> *pTokens)
{
    if (delimiter.empty())
        return RC::BAD_PARAMETER;

    pTokens->clear();

    size_t beg = 0;
    size_t end = 0;
    while (string::npos != (beg = src.find_first_not_of(delimiter, end)))
    {
        end = src.find_first_of(delimiter, beg);
        if (string::npos == end)
            end = src.length();

        pTokens->push_back(src.substr(beg, end - beg));
    }
    return OK;
}

void Utility::TokenizeBuffer
(
    char                 *buffer,
    UINT32                bufferSize,
    vector<const char *> *pTokens,
    bool                  bInsertNewLines
)
{
    bool in_token = false;
    bool in_comment = false;

    char * p = buffer;
    char * pEnd = buffer + bufferSize;
    for (/**/; p < pEnd; p++)
    {
        if (isspace(*p))
        {
            if ('\n' == *p)
            {
                // Finish comment.
                in_comment = false;

                // Since we are overwriting all whitespace with '\0' to
                // terminate the real tokens, we push an external string to
                // represent the newline.
                // This also allows the parser to check for newline tokens with
                // just a pointer compare rather than having to use strcmp.
                if (bInsertNewLines && !pTokens->empty() &&
                    (pTokens->back() != s_NewLineToken))
                    pTokens->push_back(s_NewLineToken);
            }

            // Finish previous token.
            in_token = false;
            *p = '\0';
        }
        else if (!in_comment)
        {
            if ('#' == *p)
            {
                // Begin a comment, finish previous token.
                in_comment = true;
                in_token = false;
                *p = '\0';
            }
            else if (!in_token)
            {
                // Begin token.
                pTokens->push_back(p);
                in_token = true;
            }
            // Else continue current token.
        }
        // Else continue current comment.
    }

    // Add missing \n to last statement if necessary.
    if (bInsertNewLines && !pTokens->empty() &&
        (s_NewLineToken != pTokens->back()))
    {
        pTokens->push_back(s_NewLineToken);
    }
}

RC Utility::ParseIndices(const string &input, set<UINT32> *pOut)
{
    MASSERT(pOut);
    pOut->clear();

    if (input.empty())
    {
        return OK;
    }

    size_t pos = 0;
    while (pos != string::npos)
    {
        if (input[pos] == ',')
        {
            ++pos;
        }
        if (pos == input.size())
        {
            break;
        }

        const size_t begin = pos;
        pos = input.find(',', pos);

        const size_t len = pos == string::npos ? string::npos : pos - begin;
        const string range = input.substr(begin, len);
        if (range.empty())
        {
            continue;
        }

        // Verify contents and find a dash
        size_t dash = string::npos;
        for (size_t i=0; i < range.size(); i++)
        {
            const char c          = range[i];
            const bool good       = c == '-' || (c >= '0' && c <= '9');
            const bool secondDash = c == '-' && dash != string::npos;
            const bool termDash   = c == '-' && (i == 0 || i == range.size()-1);
            if (!good || secondDash || termDash)
            {
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            if (c == '-')
            {
                dash = i;
            }
        }

        // Parse range and extract frame indices
        if (dash == string::npos)
        {
            const UINT64 value = Utility::Strtoull(range.c_str(), nullptr, 10);
            pOut->insert(UNSIGNED_CAST(UINT32, value));
        }
        else
        {
            const UINT64 first64 = Utility::Strtoull(range.c_str(),  nullptr, 10);
            const UINT32 first = UNSIGNED_CAST(UINT32, first64);
            const UINT64 last64  = Utility::Strtoull(&range[dash+1], nullptr, 10);
            const UINT32 last = UNSIGNED_CAST(UINT32, last64);
            for (UINT32 i=first; i <= last; i++)
            {
                pOut->insert(i);
            }
        }
    }

    return OK;
}

RC Utility::StringToUINT32(const string& numStr, UINT32* pNumber, UINT32 base)
{
    MASSERT(pNumber);

    char  *endptr;
    errno = 0;

    const UINT64 value = Utility::Strtoull(numStr.c_str(), &endptr, base);
    if (errno == 0 && *endptr == 0)
    {
        *pNumber = UNSIGNED_CAST(UINT32, value);
        return RC::OK;
    }

    return RC::ILWALID_ARGUMENT;
}

RC Utility::StringToUINT32(const string& numStr, UINT32* pNumber)
{
    return StringToUINT32(numStr, pNumber, 16);
}

FLOAT32 Utility::FromLwTemp(LwTemp temp)
{
    return static_cast<FLOAT32>(temp / 256.0);
}

LwTemp Utility::ToLwTemp(FLOAT32 temp)
{
    return static_cast<LwTemp>(floor(temp * 256.0 + 0.5));
}

void Utility::AssertWithinBitWidth(UINT32 value, UINT32 bitWidth, const char* pValueName)
{
    MASSERT(pValueName);

    if (!IsWithinBitWidth(value, bitWidth))
    {
        Printf(Tee::PriError, "Value for %s does not fit in field:"
               "\n\tvalue: %X\n\tfield width: %u\n",
               pValueName, value, bitWidth);
        MASSERT(!"Value does not fit in field width");
    }
}

//------------------------------------------------------------------------------
// Script properties and methods.
//------------------------------------------------------------------------------

// using a function prototype fixes a codewarrior compiler warning.
JSBool Global_Emulation_Getter(JSContext * cx, JSObject * obj, jsval id, jsval *vp);
JSBool Global_Emulation_Getter(JSContext * cx, JSObject * obj, jsval id, jsval *vp)
{
    static Deprecation depr("Emulation", "3/30/2019",
                            "Use GpuSubdevice.IsEmOrSim instead\n");
    if (!depr.IsAllowed(cx))
        return JS_FALSE;

    Printf(Tee::PriNormal,
        "Warning: \"Emulation\" is true on simulation also.\n"
        "Recommend using GpuSubdevice.IsEmulation or .IsEmOrSim instead.\n");

    *vp = INT_TO_JSVAL(false);

    return JS_TRUE;
}

static SProperty Global_Emulation
(
    "Emulation",
    0,
    false,
    Global_Emulation_Getter,
    0,
    JSPROP_READONLY,
    "Running on an emulator or simulator."
);

JSBool Global_IsRemoteConsole_Getter(JSContext * cx, JSObject * obj, jsval id, jsval *vp);
JSBool Global_IsRemoteConsole_Getter(JSContext * cx, JSObject * obj, jsval id, jsval *vp)
{
    JavaScriptPtr()->ToJsval(Xp::IsRemoteConsole(), vp);
    return JS_TRUE;
}

static SProperty Global_IsRemoteConsole
(
    "IsRemoteConsole",
    0,
    false,
    Global_IsRemoteConsole_Getter,
    0,
    JSPROP_READONLY,
    "Using remote console, i.e. in PVS lab."
);

static SProperty Global_Changelist
(
    "Changelist",
    0,
    g_Changelist,          // from version.cpp
    0,
    0,
    JSPROP_READONLY,
    "Synced changelist for the build."
);

static SProperty Global_BuildDate
(
    "BuildDate",
    0,
    g_BuildDate,           // from version.cpp
    0,
    0,
    JSPROP_READONLY,
    "Date of the build."
);

P_(Global_Official_Getter);
static SProperty Global_Official
(
    "Official",
    0,
    true,
    Global_Official_Getter,
    0,
    JSPROP_READONLY,
    "Official operation (Deprecated)"
);

P_(Global_Official_Getter)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    const bool bOfficial = Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK;
    RC rc = JavaScriptPtr()->ToJsval(bOfficial, pValue);
    if (OK != rc)
    {
        JavaScriptPtr()->Throw(pContext, rc, "Failed to get Official or Qual.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// Qual build is gone and merged with official
static SProperty Global_Qual
(
    "Qual",
    0,
    true,
    Global_Official_Getter,
    0,
    JSPROP_READONLY,
    "Qual operation (Deprecated)"
);

static SProperty Global_FieldDiagMode
(
    "FieldDiagMode",
    0,
    static_cast<UINT32>(g_FieldDiagMode),          // from version.cpp
    0,
    0,
    JSPROP_READONLY,
    "Field diagnostic mode type"
);

#define DEFINE_FIELDDIAG_MODE(m) \
    GLOBAL_JS_PROP_CONST(FIELDDIAG_ ## m, static_cast<UINT32>(FieldDiagMode::FDM_ ## m), "");
        #include "core/include/fielddiagmodes.h"
#undef DEFINE_FIELDDIAG_MODE

P_(Global_Get_OperatingSystem);
static SProperty Global_OperatingSystem
(
    "OperatingSystem",
    0,
    Global_Get_OperatingSystem,
    0,
    0,
    "Operating system: Sim, Win, WinMfg, VistaRM, MacOSX, Linux, LinuxRM, Android."
);

P_(Global_Get_SimOS);
static SProperty Global_SimOS
(
    "SimOS",
    0,
    Global_Get_SimOS,
    0,
    0,
    "Simulation Operating system; Windows, Linux, or None (not a simulation)."
);

static SProperty Global_SpsNormal
(
    "SpsNormal",
    0,
    Tee::SPS_NORMAL,
    0,
    0,
    JSPROP_READONLY,
    "Constant: Normal print state"
);

static SProperty Global_SpsFail
(
    "SpsFail",
    0,
    Tee::SPS_FAIL,
    0,
    0,
    JSPROP_READONLY,
    "Constant: Fail print state"
);

static SProperty Global_SpsPass
(
    "SpsPass",
    0,
    Tee::SPS_PASS,
    0,
    0,
    JSPROP_READONLY,
    "Constant: Pass print state"
);

static SProperty Global_SpsWarning
(
    "SpsWarning",
    0,
    Tee::SPS_WARNING,
    0,
    0,
    JSPROP_READONLY,
    "Constant: Warning print state"
);

static SProperty Global_SpsHighlight
(
    "SpsHighlight",
    0,
    Tee::SPS_HIGHLIGHT,
    0,
    0,
    JSPROP_READONLY,
    "Constant: Highlight print state"
);

C_(Global_IntToBits);
static SMethod Global_IntToBits
(
    "IntToBits",
    C_Global_IntToBits,
    1,
    "Interpret the integer as a 32 bit pattern."
);

C_(Global_FloatToBits);
static SMethod Global_FloatToBits
(
    "FloatToBits",
    C_Global_FloatToBits,
    1,
    "Interpret the float as a 32 bit pattern."
);

C_(Global_Char);
static SMethod Global_Char
(
    "Char",
    C_Global_Char,
    1,
    "Colwert integer to a character (1 element string)."
);

C_(Global_SeedRandom);
static STest Global_SeedRandom
(
    "SeedRandom",
    C_Global_SeedRandom,
    1,
    "Seed the random number generator DEPRECATED."
);

C_(Global_GetRandom);
static SMethod Global_GetRandom
(
    "GetRandom",
    C_Global_GetRandom,
    2,
    "Return a random number between low and high inclusive DEPRECATED."
);

C_(Global_GetWeightedRandom);
static SMethod Global_GetWeightedRandom
(
    "GetWeightedRandom",
    C_Global_GetWeightedRandom,
    1,
    "Return a random number with weighted probability DEPRECATED."
);

C_(Global_Sleep);
static STest Global_Sleep
(
    "Sleep",
    C_Global_Sleep,
    1,
    "Pause the program for given amount of milliseconds."
);

C_(Global_SleepUS);
static STest Global_SleepUS
(
    "SleepUS",
    C_Global_SleepUS,
    1,
    "Pause the program for given amount of microseconds."
);

C_(Global_DelayUS);
static STest Global_DelayUS
(
    "DelayUS",
    C_Global_DelayUS,
    1,
    "Pause the program for given amount of microseconds."
);

C_(Global_GetWallTimeNS);
static SMethod Global_GetWallTimeNS
(
    "GetWallTimeNS",
    C_Global_GetWallTimeNS,
    0,
    "return the wall time in nanoseconds."
);

C_(Global_GetChar);
static SMethod Global_GetChar
(
    "GetChar",
    C_Global_GetChar,
    0,
    "Get a character from stdin."
);

C_(Global_KeyboardHit);
static SMethod Global_KeyboardHit
(
    "Global_KeyboardHit",
    C_Global_KeyboardHit,
    0,
    "Return true if a keyboard key was pressed."
);

C_(Global_FlushInputBuffer);
static STest Global_FlushInputBuffer
(
    "FlushInputBuffer",
    C_Global_FlushInputBuffer,
    0,
    "Flush the input buffer."
);

C_(Global_DisableUserInterface);
static STest Global_DisableUserInterface
(
    "DisableUserInterface",
    C_Global_DisableUserInterface,
    4,
    "Disable the user interface."
);

C_(Global_EnableUserInterface);
static STest Global_EnableUserInterface
(
    "EnableUserInterface",
    C_Global_EnableUserInterface,
    0,
    "Enable the user interface."
);

JSBool Global_IsUserInterfaceDisabled_Getter(JSContext * cx, JSObject * obj, jsval id, jsval *vp)
{
    JavaScriptPtr pJs;
    pJs->ToJsval(Platform::IsUserInterfaceDisabled(), vp);
    return JS_TRUE;
}

static SProperty Global_IsUserInterfaceDisabled
(
    "IsUserInterfaceDisabled",
    0,
    false,
    Global_IsUserInterfaceDisabled_Getter,
    0,
    JSPROP_READONLY,
    "UserInterface is lwrrently disabled."
);

C_(Global_StartBlinkLightsThread);
static STest Global_StartBlinkLightsThread
(
    "StartBlinkLightsThread",
    C_Global_StartBlinkLightsThread,
    0,
    "Start a background thread to blink lights (i.e.keyboard LEDs) to show mods isn't hung."
);

C_(Global_DoesFileExist);
static SMethod Global_DoesFileExist
(
    "DoesFileExist",
    C_Global_DoesFileExist,
    1,
    "Given a filename, tests for the existence of that file."
);

C_(Global_FindPkgFile);
static SMethod Global_FindPkgFile
(
    "FindPkgFile",
    C_Global_FindPkgFile,
    1,
    "Returns path to a file."
);

C_(Global_ColwertFloatToFXP);
static SMethod Global_ColwertFloatToFXP
(
    "ColwertFloatToFXP",
    C_Global_ColwertFloatToFXP,
    3,
    "Colwerts a floating-point number to fixed point"
);

C_(Global_ColwertFXPToFloat);
static SMethod Global_ColwertFXPToFloat
(
    "ColwertFXPToFloat",
    C_Global_ColwertFXPToFloat,
    3,
    "Colwerts a fixed-point number to floating-point"
);

C_(Global_ColwertFloatTo12_20);
static SMethod Global_ColwertFloatTo12_20
(
    "ColwertFloatTo12_20",
    C_Global_ColwertFloatTo12_20,
    1,
    "Colwerts a floating-point number to 12/20 fixed point"
);

C_(Global_Colwert12_20ToFloat);
static SMethod Global_Colwert12_20ToFloat
(
    "Colwert12_20ToFloat",
    C_Global_Colwert12_20ToFloat,
    1,
    "Colwerts a 12/20 fixed-point number to floating-point"
);

C_(Global_ColwertFloatTo12_4);
static SMethod Global_ColwertFloatTo124_
(
    "ColwertFloatTo12_4",
    C_Global_ColwertFloatTo12_4,
    1,
    "Colwerts a floating-point number to 12/4 fixed point"
);

C_(Global_Colwert12_4ToFloat);
static SMethod Global_Colwert12_4ToFloat
(
    "Colwert12_4ToFloat",
    C_Global_Colwert12_4ToFloat,
    1,
    "Colwerts a 12/4 fixed-point number to floating-point"
);

C_(Global_ColwertFloatTo8_8);
static SMethod Global_ColwertFloatTo8_8
(
    "ColwertFloatTo8_8",
    C_Global_ColwertFloatTo8_8,
    1,
    "Colwerts a floating-point number to 8/8 signed fixed point"
);

C_(Global_Colwert8_8ToFloat);
static SMethod Global_Colwert8_8ToFloat
(
    "Colwert8_8ToFloat",
    C_Global_Colwert8_8ToFloat,
    1,
    "Colwerts a 8/8 signed fixed-point number to floating-point"
);

C_(Global_GetElw);
static SMethod Global_GetElw
(
    "GetElw",
    C_Global_GetElw,
    1,
    "Get environment variable."
);

C_(Global_SetElw);
static STest Global_SetElw
(
    "SetElw",
    C_Global_SetElw,
    1,
    "Set environment variable."
);

C_(Global_SetScreenPrintState);
static SMethod Global_SetScreenPrintState
(
    "SetScreenPrintState",
    C_Global_SetScreenPrintState,
    1,
    "Set the screen print state to SpsNormal, SpsFail or SpsPass"
);

C_(Global_CountBits);
static SMethod Global_CountBits
(
    "CountBits",
    C_Global_CountBits,
    1,
    "Count the number of bits set in the passed in value."
);

C_(Global_BitScanForward);
static SMethod Global_BitScanForward
(
    "BitScanForward",
    C_Global_BitScanForward,
    2,
    "Find the index of the lowest set bit starting at or after the specified index."
);

C_(Global_BitScanReverse);
static SMethod Global_BitScanReverse
(
    "BitScanReverse",
    C_Global_BitScanReverse,
    2,
    "Find the index of the highest set bit starting at or before the specified index."
);

C_(Global_CopyBits);
static SMethod Global_CopyBits
(
    "CopyBits",
    C_Global_CopyBits,
    5,
    "Copy numBits from srcBitOffset of srcArray to dstBitOffset of dstArray."
);

C_(Global_GetSystemTimeInSeconds);
static SMethod Global_GetSystemTimeInSeconds
(
    "GetSystemTimeInSeconds",
    C_Global_GetSystemTimeInSeconds,
    0,
    "Get the number of seconds since the Epoch"
);

C_(Global_GetMODSStartTimeInSeconds);
static SMethod Global_GetMODSStartTimeInSeconds
(
    "GetMODSStartTimeInSeconds",
    C_Global_GetMODSStartTimeInSeconds,
    0,
    "Get MODS start time, in seconds since the Epoch"
);

//! \brief Creates and deletes a temporary file
//!
//! Creates and deletes a temporary file, useful for verifying write permission
//! \return Returns OK if success, otherwise an appropriate RC
C_(Global_CheckWritePerm);
static STest Global_CheckWritePerm
(
    "CheckWritePerm",
    C_Global_CheckWritePerm,
    1,
    "Create and delete a temporary file (useful to test permissions)."
);

C_(Global_CompareFiles);
static SMethod Global_CompareFiles
(
    "CompareFiles",
    C_Global_CompareFiles,
    2,
    "Binary-compare two files, returns if both are readable and match."
);

// Implementation

P_(Global_Get_OperatingSystem)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    string Os;
    UINT64 version = 0;

    switch (Xp::GetOperatingSystem())
    {
        case Xp::OS_WINDOWS:
            // Fetching the windows type: vista or NT
            if(Xp::GetOSVersion(&version)!=OK)
            {
                Printf(Tee::PriHigh,"Error in Xp::GetWindowsType\n");
                return(JS_FALSE);
            }
             // Fetching the Major version
            if(((UINT32)(version>>48))>= 6)
            {
                Os = "VistaRM";
            }
            else
            {
                Os = "Win";
            }
            break;

        case Xp::OS_MAC:
            Os = "Mac";
            break;

        case Xp::OS_MACRM:
            Os = "MacRM";
            break;

        case Xp::OS_LINUX:
            Os = "Linux";
            break;

        case Xp::OS_LINUXRM:
            Os = "LinuxRM";
            break;

        case Xp::OS_ANDROID:
            Os = "Android";
            break;

        case Xp::OS_WINSIM:
        case Xp::OS_LINUXSIM:
            Os = "Sim";
            break;

        case Xp::OS_WINMFG:
            Os = "WinMfg";
            break;

        case Xp::OS_WINDA:
        case Xp::OS_LINDA:
            Os = "DirectAmodel";
            break;

        case Xp::OS_NONE:
        default:
            // We should never reach this code.
            MASSERT(false);
            Os = "ERROR";
            break;
    }

    if (OK != JavaScriptPtr()->ToJsval(Os, pValue))
    {
        JS_ReportError(pContext, "Failed to get OperatingSystem.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

P_(Global_Get_SimOS)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    string SimOs;

    switch (Xp::GetOperatingSystem())
    {
        case Xp::OS_WINDA:
        case Xp::OS_WINSIM:
            SimOs = "Windows";
            break;

        case Xp::OS_LINDA:
        case Xp::OS_LINUXSIM:
            SimOs = "Linux";
            break;

        default:
            // We should never reach this code.
            SimOs = "None";
            break;
    }

    if (OK != JavaScriptPtr()->ToJsval(SimOs, pValue))
    {
        JS_ReportError(pContext, "Failed to get SimOS.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_IntToBits)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the argument.
    UINT32 Integer = 0;
    if
    (
         (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Integer))
    )
    {
        JS_ReportError(pContext, "Usage: IntToBits(integer)");
        return JS_FALSE;
    }

    if (OK != pJavaScript->ToJsval(
                  static_cast<FLOAT64>(Integer), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in IntToBits().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_FloatToBits)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the argument.
    FLOAT64 Double = 0.0;
    if
    (
         (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Double))
    )
    {
        JS_ReportError(pContext, "Usage: FloatToBits(float)");
        return JS_FALSE;
    }

    FLOAT32 Float   = static_cast<FLOAT32>(Double);
    UINT32  Integer = *reinterpret_cast<UINT32*>(&Float);

    if (OK != pJavaScript->ToJsval(
                  static_cast<FLOAT64>(Integer), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in FloatToBits().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_Char)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the argument.
    INT32 Integer = 0;
    if
    (
         (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Integer))
    )
    {
        JS_ReportError(pContext, "Usage: Char(integer)");
        return JS_FALSE;
    }

    char theChar[2] = { static_cast<char>(Integer), '\0' };
    if (pJavaScript->ToJsval(theChar, pReturlwalue) != RC::OK)
    {
        JS_ReportError(pContext, "Error oclwrred in Char().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

static Random s_JsUtilityRandomObject;

// STest
C_(Global_SeedRandom)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    static bool complainOneTime = false;
    if (!complainOneTime)
    {
        Printf(Tee::PriHigh,
            "Global random is unsafe, please use a new Random object.\n");
        complainOneTime = true;
    }

    // Check the argument.
    UINT32 Seed = 0;
    if
    (
         (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Seed))
    )
    {
        JS_ReportError(pContext, "Usage: SeedRandom(seed)");
        return JS_FALSE;
    }

    s_JsUtilityRandomObject.SeedRandom(Seed);

    RETURN_RC(OK);
}

// SMethod
C_(Global_GetRandom)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    static bool complainOneTime = false;
    if (!complainOneTime)
    {
        Printf(Tee::PriHigh,
            "Global random is unsafe, please use a new Random object.\n");
        complainOneTime = true;
    }

    // Check the arguments.
    UINT32 Low  = 0;
    UINT32 High = 0;
    if
    (
         (NumArguments != 2)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Low))
      || (OK != pJavaScript->FromJsval(pArguments[1], &High))
      || (Low > High)
    )
    {
        JS_ReportError(pContext, "Usage: GetRandom(low, high)");
        return JS_FALSE;
    }

    UINT32 RandomNumber = s_JsUtilityRandomObject.GetRandom(Low, High);

    if (OK != pJavaScript->ToJsval(RandomNumber, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in GetRandom().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_GetWeightedRandom)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    static bool complainOneTime = false;
    if (!complainOneTime)
    {
        Printf(Tee::PriHigh,
            "Global random is unsafe, please use a new Random object.\n");
        complainOneTime = true;
    }

    // Check the arguments.
    vector<Random::PickItem> Items;
    if
    (
         (NumArguments != 1)
      || (OK != Utility::PickItemsFromJsval(pArguments[0], &Items))
    )
    {
        JS_ReportError(pContext, "Usage: GetWeightedRandom([[prob, min, max],[prob,min,max]...)");
        return JS_FALSE;
    }

    UINT32 RandomNumber = s_JsUtilityRandomObject.Pick(&Items[0]);

    if (OK != pJavaScript->ToJsval(RandomNumber, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in GetWeightedRandom().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// STest
C_(Global_Sleep)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the argument.
    UINT32 Milliseconds = 0;
    if
    (
         (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Milliseconds))
    )
    {
        JS_ReportError(pContext, "Usage: Sleep(milliseconds)");
        return JS_FALSE;
    }

    Tasker::Sleep(Milliseconds);

    RETURN_RC(OK);
}

// STest
C_(Global_SleepUS)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the argument.
    UINT32 Microseconds = 0;
    if
    (
         (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Microseconds))
    )
    {
        JS_ReportError(pContext, "Usage: SleepUS(microseconds)");
        return JS_FALSE;
    }

    Utility::SleepUS(Microseconds);

    RETURN_RC(OK);
}

// STest
C_(Global_DelayUS)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    RC rc;
    JavaScriptPtr pJS;

    // Check the argument.
    UINT32 Microseconds = 0;
    if (NumArguments != 1) 
    {
        rc = RC::BAD_PARAMETER;
        pJS->Throw(pContext, rc, "Usage: DelayUS(microseconds)");
        return JS_FALSE;
    }
    if ((rc = pJS->FromJsval(pArguments[0], &Microseconds)) != RC::OK)
    {
        pJS->Throw(pContext, rc, "Error reading parameter.");
        return JS_FALSE;
    }
    Utility::Delay(Microseconds);

    RETURN_RC(RC::OK);
}

// SMethod
C_(Global_GetWallTimeNS)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    RC rc;
    JavaScriptPtr pJS;

    // Check this is a void method.
    if (NumArguments)
    {
        rc = RC::BAD_PARAMETER;
        pJS->Throw(pContext, rc, "Usage: GetWallTimeNS()");
        return JS_FALSE;
    }

    const UINT64 timeNs = Xp::GetWallTimeNS();
    JsUINT64* pJsUINT64 = new JsUINT64(timeNs);
    MASSERT(pJsUINT64);
    if ((rc = pJsUINT64->CreateJSObject(pContext)) != RC::OK ||
        (rc = pJS->ToJsval(pJsUINT64->GetJSObject(), pReturlwalue)) != RC::OK)
    {
        *pReturlwalue = JSVAL_NULL;
        pJS->Throw(pContext, rc, "Error oclwrred in GetWallTimeNs().");
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_GetChar)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check this is a void method.
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: GetChar()");
        return JS_FALSE;
    }

    ConsoleManager::ConsoleContext consoleCtx;
    string Character(1, static_cast<char>(consoleCtx.AcquireRealConsole(true)->GetKey()));

    if (OK != pJavaScript->ToJsval(Character, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in GetChar().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_KeyboardHit)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check this is a void method.
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: KeyboardHit()");
        return JS_FALSE;
    }

    ConsoleManager::ConsoleContext consoleCtx;
    if (OK != pJavaScript->ToJsval(consoleCtx.AcquireRealConsole(true)->KeyboardHit(), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in KeyboardHit().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// STest
C_(Global_FlushInputBuffer)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // Check this is a void method.
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: FlushInputBuffer()");
        return JS_FALSE;
    }

    RETURN_RC(Utility::FlushInputBuffer());
}

// STest
C_(Global_DisableUserInterface)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;

    // Check the arguments.
    if (NumArguments == 0)
    {
        RETURN_RC(Platform::DisableUserInterface());
    }

    UINT32 Width;
    UINT32 Height;
    UINT32 Depth;
    UINT32 RefreshRate;

    if
    (
         (NumArguments != 4)
      || (OK != pJs->FromJsval(pArguments[0], &Width))
      || (OK != pJs->FromJsval(pArguments[1], &Height))
      || (OK != pJs->FromJsval(pArguments[2], &Depth))
      || (OK != pJs->FromJsval(pArguments[3], &RefreshRate))
    )
    {
        JS_ReportError(pContext,
         "Usage: DisableUserInterface(width, height, depth, refresh rate) or DisableUserInterface()");
        return JS_FALSE;
    }

    RETURN_RC(Platform::DisableUserInterface(Width,
                                            Height,
                                            Depth,
                                            RefreshRate));
}

// STest
C_(Global_EnableUserInterface)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // Check this is a void method.
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: EnableUserInterface");
        return JS_FALSE;
    }

    RETURN_RC(Platform::EnableUserInterface());
    }

    // STest
    C_(Global_StartBlinkLightsThread)
    {
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // Check this is a void method.
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: StartBlinkLightsThread");
        return JS_FALSE;
    }

    RETURN_RC(Xp::StartBlinkLightsThread());
}

// SMethod
C_(Global_DoesFileExist)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Make sure we were given a filename argument.
    string filename;
    if
    (
         (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &filename))
    )
    {
        JS_ReportError(pContext, "Usage: DoesFileExist(filename)");
        return JS_FALSE;
    }

    if (OK != pJavaScript->ToJsval(Xp::DoesFileExist(filename), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in DoesFileExist().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_FindPkgFile)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Make sure we were given a filename argument.
    string filename;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &filename)))
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: FindPkgFile(filename)");
        return JS_FALSE;
    }

    string fullPath;
    const auto fileStatus = Utility::FindPkgFile(filename, &fullPath);

    if (fileStatus == Utility::NoSuchFile)
    {
        *pReturlwalue = JSVAL_NULL;
        return JS_TRUE;
    }

    const RC rc = pJavaScript->ToJsval(fullPath, pReturlwalue);
    if (rc != OK)
    {
        pJavaScript->Throw(pContext, rc, "Error oclwrred in FindPkgFile().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_ColwertFloatToFXP)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    FLOAT64 Input;
    UINT32 intBits;
    UINT32 fractionalBits;

    // Check this is a void method.
    if ( (NumArguments != 3)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Input))
      || (OK != pJavaScript->FromJsval(pArguments[1], &intBits))
      || (OK != pJavaScript->FromJsval(pArguments[2], &fractionalBits)))
    {
        JS_ReportError(pContext,
         "Usage: ColwertFloatToFXP(float, numIntBits, numfractionalBits)");
        return JS_FALSE;
    }

    INT32 Value = Utility::ColwertFloatToFXP( Input, intBits, fractionalBits);

    if (OK != pJavaScript->ToJsval(Value, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in ColwertFloatToFXP().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_ColwertFXPToFloat)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    INT32 Input;
    UINT32 intBits;
    UINT32 fractionalBits;

    // Check this is a void method.
    if ( (NumArguments != 3)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Input))
      || (OK != pJavaScript->FromJsval(pArguments[1], &intBits))
      || (OK != pJavaScript->FromJsval(pArguments[2], &fractionalBits)))
    {
        JS_ReportError(pContext,
         "Usage: ColwertFXPToFloat(fixed point num, numIntBits, numfractionalBits)");
        return JS_FALSE;
    }

    FLOAT64 Value = Utility::ColwertFXPToFloat( Input, intBits, fractionalBits);

    if (OK != pJavaScript->ToJsval(Value, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in ColwertFXPToFloat().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_ColwertFloatTo12_20)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    FLOAT64 Input;

    // Check this is a void method.
    if ( (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Input)) )
    {
        JS_ReportError(pContext, "Usage: ColwertFloatTo12_20(float)");
        return JS_FALSE;
    }

    UINT32 Value = Utility::ColwertFloatTo12_20( Input );

    if (OK != pJavaScript->ToJsval(Value, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in ColwertFloatTo12_20().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_Colwert12_20ToFloat)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    UINT32 Input;

    if ( (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Input)) )
    {
        JS_ReportError(pContext, "Usage: Colwert12_20ToFloat(fixed point num)");
        return JS_FALSE;
    }

    FLOAT64 Value = Utility::Colwert12_20ToFloat( Input );

    if (OK != pJavaScript->ToJsval(Value, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Colwert12_20ToFloat().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_ColwertFloatTo12_4)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    FLOAT64 Input;

    if ( (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Input)) )
    {
        JS_ReportError(pContext, "Usage: ColwertFloatTo12_4(float)");
        return JS_FALSE;
    }

    UINT32 Value = (UINT32)Utility::ColwertFloatTo12_4( Input );

    if (OK != pJavaScript->ToJsval(Value, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in ColwertFloatTo12_4().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_Colwert12_4ToFloat)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    UINT32 Input;

    if ( (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Input)) )
    {
        JS_ReportError(pContext, "Usage: Colwert12_4ToFloat(fixed point num)");
        return JS_FALSE;
    }

    FLOAT64 Value = Utility::Colwert12_4ToFloat( UINT16(Input & 0xFFFF) );

    if (OK != pJavaScript->ToJsval(Value, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Colwert12_4ToFloat().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_ColwertFloatTo8_8)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    FLOAT64 Input;

    if ( (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Input)) )
    {
        JS_ReportError(pContext, "Usage: ColwertFloatTo8_8(float)");
        return JS_FALSE;
    }

    UINT32 Value = (UINT32)Utility::ColwertFloatTo8_8( Input );

    if (OK != pJavaScript->ToJsval(Value, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in ColwertFloatTo8_8().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_Colwert8_8ToFloat)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    UINT32 Input;

    if ( (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Input)) )
    {
        JS_ReportError(pContext, "Usage: Colwert8_8ToFloat(fixed point num)");
        return JS_FALSE;
    }

    FLOAT64 Value = Utility::Colwert8_8ToFloat( UINT16(Input & 0xFFFF) );

    if (OK != pJavaScript->ToJsval(Value, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Colwert8_8ToFloat().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_GetElw)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    *pReturlwalue = JSVAL_NULL;

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    string Variable;
    if
    (
         (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Variable))
    )
    {
        JS_ReportError(pContext, "Usage: GetElw(\"environment variable\")");
        return JS_FALSE;
    }

    if (OK != pJavaScript->ToJsval(Xp::GetElw(Variable), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in GetElw()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// STest
C_(Global_SetElw)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    string Variable;
    string Value;
    if
    (
         (NumArguments != 2)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Variable))
      || (OK != pJavaScript->FromJsval(pArguments[1], &Value))
    )
    {
        JS_ReportError(pContext,
         "Usage: SetElw(\"environment variable\", \"value\")");
        return JS_FALSE;
    }

    RETURN_RC(Xp::SetElw(Variable, Value));
}

// SMethod
C_(Global_SetScreenPrintState)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    UINT32 Input;

    if ( (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Input)) )
    {
        JS_ReportError(pContext, "Usage: SetScreenPrintState(value)");
        return JS_FALSE;
    }

    if( Input >= Tee::SPS_END_LIST )
    {
        JS_ReportError(pContext, "Argument must be SpsNormal, SpsFail or SpsPass");
        return JS_FALSE;
    }

    Tee::SetScreenPrintState( (Tee::ScreenPrintState)Input );

    return JS_TRUE;
}

C_(Global_CompareFiles)
{
    JavaScriptPtr pJs;

    string fnameA;
    string fnameB;

    if ((NumArguments != 2) ||
       (OK != pJs->FromJsval(pArguments[0], &fnameA)) ||
       (OK != pJs->FromJsval(pArguments[1], &fnameB)))
    {
        JS_ReportError(pContext, "Useage: FilesMatch = CompareFiles(fnameA, fnameB)");
        return JS_FALSE;
    }

    bool filesMatch = false;
    RC rc = Utility::CompareFiles(fnameA.c_str(), fnameB.c_str(), &filesMatch);

    if(rc != OK)
    {
        Printf(Tee::PriNormal, "  file error: %d %s\n", INT32(rc), rc.Message());
        RETURN_RC(rc);
    }

    if(filesMatch)
    {
        RETURN_RC(OK);
    }
    else
    {
        Printf(Tee::PriHigh, "%s and %s did not match\n", fnameA.c_str(), fnameB.c_str());
        RETURN_RC(RC::GOLDEN_VALUE_MISCOMPARE);
    }
}

/**
 * @brief Colwerts a FLOAT64 input into a INT32 representing
 *        the number in FXP.
 *
 * @param Input - A FLOAT64 number to colwert
 *
 * @param intBits - A UINT32 number that represents the number of integer bits
 *
 * @param fractionalBits - A UINT32 number that represents the number of
 *        fractional bits
 *
 * @return INT32 - The integer portion of the input
 */

INT32 Utility::ColwertFloatToFXP(FLOAT64 Input, UINT32 intBits,
                        UINT32 fractionalBits)
{
    const UINT32 numBits = intBits + fractionalBits;
    MASSERT(numBits <= 32);
    const INT32 mask = (numBits == 32) ? -1 : ((1 << numBits)-1);
    const FLOAT64 unit = 1.0 / (1 << fractionalBits);
    const FLOAT64 round = (Input < 0) ? (-0.5 * unit) : (0.5 * unit);
    const INT32 fxp = static_cast<INT32>((Input+round) / unit);
    MASSERT(fabs(fxp*unit - Input) <= unit*0.5);   // Input is OO range or NaN
    return mask & fxp;
}

FLOAT64 Utility::ColwertFXPToFloat(INT32 Input, UINT32 intBits,
                          UINT32 fractionalBits)
{
    const UINT32 numBits = intBits + fractionalBits;
    MASSERT(numBits <= 32);
    const INT32 mask = (numBits == 32) ? -1 : ((1 << numBits)-1);
    const INT32 sInput = (Input & (1 << (numBits-1)))   // sign extend?
               ? (Input | (-1 & ~mask))
               : Input;
    const FLOAT64 unit = 1.0 / (1 << fractionalBits);
    return sInput * unit;
}

/**
 * @brief Colwerts a FLOAT64 input into a UINT32 representing
 *        the fraction portion of the number.
 *
 * @param Input - A FLOAT64 number to colwert
 *
 * @return UINT32 - The fraction portion of the input
 */

UINT32 Utility::ColwertFloatTo0_32(FLOAT64 Input)
{
    FLOAT64 Round       = floor(Input);
    UINT32  Remainder   = (UINT32) ((Input - Round) * (FLOAT64)(0xFFFFFFFF));

    return Remainder;
}

/**
 * @brief Colwerts a FLOAT64 input into a UINT32 representing
 *        the integer portion of the number.
 *
 * @param Input - A FLOAT64 number to colwert
 *
 * @return UINT32 - The integer portion of the input
 */

UINT32 Utility::ColwertFloatTo32_0(FLOAT64 Input)
{
    return (UINT32) floor(Input);
}

UINT32 Utility::ColwertFloatTo12_20( FLOAT64 Input )
{
    FLOAT64 Round     = floor(Input);
    UINT32  Integer   = (UINT32) Round;
    UINT32  Remainder = (UINT32) ((Input - Round) * (FLOAT64)(1 << 20));

    Integer   &= 0xFFF;
    Remainder &= 0xFFFFF;

    return (Integer << 20) | Remainder;
}

FLOAT64 Utility::Colwert12_20ToFloat( UINT32 Input )
{
    UINT32 Integer   = (Input >> 20) & 0xFFF;
    UINT32 Remainder = Input & 0xFFFFF;

    return (FLOAT64)Integer + ((FLOAT64)Remainder / (1 << 20));
}

UINT32 Utility::ColwertFloatTo2_24( FLOAT64 Input )
{
    FLOAT64 Round     = floor(Input);
    UINT32  Integer   = (UINT32) Round;
    UINT32  Remainder = (UINT32) ((Input - Round) * (FLOAT64)(1 << 24));

    Integer   &= 0x3;
    Remainder &= 0xFFFFFF;

    return (Integer << 24) | Remainder;
}

FLOAT64 Utility::Colwert2_24ToFloat( UINT32 Input )
{
    UINT32 Integer   = (Input >> 24) & 0x3;
    UINT32 Remainder = Input & 0xFFFFFF;

    return (FLOAT64)Integer + ((FLOAT64)Remainder / (1 << 24));
}

UINT16 Utility::ColwertFloatTo12_4( FLOAT64 Input )
{
    FLOAT64 Round     = floor(Input);
    UINT16  Integer   = (UINT16) Round;
    UINT16  Remainder = (UINT16) ((Input - Round) * (FLOAT64)(1 << 4));

    Integer   &= 0xFFF;
    Remainder &= 0xF;

    return (Integer << 4) | Remainder;
}

FLOAT64 Utility::Colwert12_4ToFloat( UINT16 Input )
{
    UINT16 Integer   = (Input >> 4) & 0xFFF;
    UINT16 Remainder = Input & 0xF;

    return (FLOAT64)Integer + ((FLOAT64)Remainder / (1 << 4));
}

UINT16 Utility::ColwertFloatTo8_8( FLOAT64 Input )
{
    FLOAT64 Round      = floor(Input);
    UINT16  Integer    = (UINT16) Round;
    UINT16  Remainder  = (UINT16) ((Input - Round) * (FLOAT64)(1 << 8));
    bool    IsPositive = (Input >= 0) ? true : false;

    Integer   &= 0x7F;
    Remainder &= 0xFF;

    if(IsPositive) return (Integer << 8) | Remainder;

    // if negative
    Integer |= 0x80;

    return (Integer << 8) | Remainder;
}

FLOAT64 Utility::Colwert8_8ToFloat( UINT16 Input )
{
    UINT16 Integer   = (Input >> 8) & 0x7F;
    UINT16 Remainder = Input & 0xFF;

    bool   IsPositive = (Input & 0x8000 ) ? false : true;

    if(IsPositive) return (FLOAT64)Integer + ((FLOAT64)Remainder / (1 << 8));

    Integer   ^= 0x7F;

    return -1.0 * ((FLOAT64)Integer + (1-((FLOAT64)Remainder / 256.0)));
}

UINT32 Utility::ColwertFloatTo16_16( FLOAT64 Input )
{
    FLOAT64 Round      = floor(Input);
    UINT16  Integer    = (UINT16) Round;
    UINT16  Remainder  = (UINT16) ((Input - Round) * (FLOAT64)(1 << 16));

    Integer   &= 0xFFFF;
    Remainder &= 0xFFFF;

    return (Integer << 16) | Remainder;
}

FLOAT64 Utility::Colwert16_16ToFloat( UINT32 Input )
{
    UINT16 Integer   = (Input >> 16) & 0xFFFF;
    UINT16 Remainder = Input & 0xFFFF;

    return (FLOAT64)Integer + ((FLOAT64)Remainder / 65536.0);
}

// Stolen from OpenGL/include/float16.h:
FLOAT32 Utility::Float16ToFloat32(UINT16 f16)
{
    UINT32 mantexp;
    UINT32 data;

    // Extract the mantissa and exponent.
    mantexp = f16 & 0x7FFF;

    if (mantexp < 0x0400)
    {
        // Exponent == 0, implies 0.0 or Denorm.
        if (mantexp == 0)
        {
            data = 0;
        }
        else
        {
            // Denorm -- shift the mantissa left until we find a leading one.
            // Each shift drops one off the final exponent.
            data = 0x38800000;  // 2^-14
            do
            {
                data -= 0x00800000; // multiply by 1/2
                mantexp *= 2;
            }
            while (!(mantexp & 0x0400));

            // Now shift the mantissa into the final location.
            data |= (mantexp & 0x3FF) << 13;
        }
    }
    else if (mantexp >= 0x7C00)
    {
        // Exponent = 31, implies INF or NaN.
        if (mantexp == 0x7C00)
        {
            data = 0x7F800000;  // INF
        }
        else
        {
            data = 0x7FFFFFFF;  // NaN
        }
    }
    else
    {
        // Normal float -- (1) shift over mantissa/exponent, (2) add bias to
        // exponent, and (3)
        data = (mantexp << 13);
        data += (112 << 23);
    }

    // Or in the sign bit and return the result.
    data |= (f16 & 0x8000) << 16;
    FLOAT32 * p = (FLOAT32 *)&data;
    return *p;
}

UINT16 Utility::Float32ToFloat16( FLOAT32 fin )
{
    UINT32 ui = *(UINT32 *)&fin;
    UINT32 data;
    UINT32 exp;
    UINT32 mant;

    // Extract the exponent and the 10 MSBs of the mantissa from the fp32
    // number.
    exp = (ui >> 23) & 0xFF;
    mant = (ui >> 13) & 0x3FF;

    // Round on type colwersion.  Check mantissa bit 11 in the 32-bit number.
    // If set, round the mantissa up.  If the mantissa overflows, bump the
    // exponent by 1 and clear the mantissa.
    if (ui & 0x00001000)
    {
        mant++;
        if (mant & 0x400)
        {
            mant = 0;
            exp++;
        }
    }

    if (exp < 113)
    {
        // |x| < 2^-14, implies 0.0 or Denorm

        // If |x| < 2^-25, we will flush to zero.  Otherwise, we will or in
        // the leading one and shift to the appropriate location.
        if (exp < 101)
        {
            data = 0;           // 0.0
        }
        else
        {
            data = (mant | 0x400) >> (113 - exp);
        }
    }
    else if (exp > 142)
    {
        // |x| > 2^15, implies overflow, an existing INF, or NaN.  NaN is any
        // non-zero mantissa with an exponent of +128 (255).  Note that our
        // rounding algorithm could have kicked the exponent up to 256.
        if (exp > 255 || (exp == 255 && mant))
        {
            data = 0x7FFF;
        }
        else
        {
            data = 0x7C00;
        }
    }
    else
    {
        exp -= 112;
        data = (exp << 10) | mant;
    }

    data |= ((ui >> 16) & 0x8000);
    return (UINT16) data;
}

FLOAT32 Utility::E8M7ToFloat32(UINT16 f16)
{
    UINT32 data = static_cast<UINT32>(f16) << 16;
    return *reinterpret_cast<FLOAT32*>(&data);
}

UINT16 Utility::Float32ToE8M7_RZ(FLOAT32 fin)
{
    UINT32 data = *reinterpret_cast<UINT32*>(&fin);

    // Round to Zero (trunucate)
    data >>= 16;

    // If NaN was truncated, set the LSB in the mantissa to 1
    if (isnan(fin) && !(data & 0x7F))
    {
        data |= 0x1;
    }
    return static_cast<UINT16>(data);
}

/**
 * @brief Colwerts a UINT32 input into a string
 *        treating the input as hex
 *        e.g. input -> 16 (hex -> 0xA)
 *             output -> 10
 *
 * @param Input - A UINT32 number to colwert
 *
 * @return string - Number represented in hex
 */
string Utility::ColwertHexToStr(UINT32 input)
{
    return Utility::StrPrintf("%x", input);
}

/**
 * @brief Colwerts a string input into a UINT32
 *        treating the input as hex
 *        e.g. input -> 0xA
 *             output -> 10
 *
 * @param Input - A hex string to colwert
 *
 * @return UINT32 - Number representation of hex value; 0 if string is not valid hex
 */
UINT32 Utility::ColwertStrToHex(const string& input)
{
    return UNSIGNED_CAST(UINT32, Strtoull(input.c_str(), nullptr, 16));
}

/**
 * Calls @a func many times, and reports the average time for
 * a single exelwtion of @a func.
 *
 * @param func Function to benchmark.
 * @param loops Number of times to call @a func (a higher number
 *              means a longer but more statistically sound
 *              benchmark).
 *
 * @return Average time, in seconds, to execute the instructions
 *         in func.  If there is an error, the function
 *         will return a negative number.  Likewise, if the
 *         Xp::QueryPerformanceFrequency and Xp::QueryPerformanceCounter
 *         functions are not implemented, this function will
 *         return a negative number.
 */
namespace Utility
{
    void NullFunc(void* arg) {}
    void NullFuncNoArg(void) {}

    class BmarkFuncWrapper
    {
    private:
        void (*m_pFunc)(void*);
        void* m_Arg;
    public:
        BmarkFuncWrapper()
            : m_pFunc(&NullFunc), m_Arg(NULL) {}
        BmarkFuncWrapper(void(*func)(void*), void* arg)
            : m_pFunc(func), m_Arg(arg) {}
        ~BmarkFuncWrapper() {}
        void Run() { (*m_pFunc)(m_Arg); }
    };

    class BmarkFuncWrapperNoArg
    {
    private:
        void (*m_pFunc)(void);
    public:
        BmarkFuncWrapperNoArg()
            : m_pFunc(&NullFuncNoArg) {}
        BmarkFuncWrapperNoArg(void(*func)(void))
            : m_pFunc(func) {}
        ~BmarkFuncWrapperNoArg() {}
        void Run() { (*m_pFunc)(); }
    };

    template<class T>
    FLOAT64 TBenchmark(T funcWrapper, UINT32 loops)
    {
        const UINT64 freq = Xp::QueryPerformanceFrequency();

        if (freq == 0 || loops == 0)
            return -1.0;

        UINT64 overheadStart, overheadStop;
        while (true)
        {
            T nullWrapper;
            overheadStart = Xp::QueryPerformanceCounter();

            for (UINT32 i = 0; i < loops; i++)
                nullWrapper.Run();

            overheadStop = Xp::QueryPerformanceCounter();
            if (overheadStop > overheadStart)
                break;
        }
        const UINT64 overheadDelta = overheadStop - overheadStart;

        UINT64 testStart, testStop;
        while (true)
        {
            testStart = Xp::QueryPerformanceCounter();

            for (UINT32 i = 0; i < loops; i++)
                funcWrapper.Run();

            testStop = Xp::QueryPerformanceCounter();
            if (testStop > testStart)
                break;
        }
        const UINT64 testDelta = testStop - testStart;

        MASSERT(testDelta > overheadDelta);
        const FLOAT64 adjustedTicks = FLOAT64(INT64(testDelta - overheadDelta));
        return adjustedTicks / FLOAT64(INT64(freq * loops));
    }
};  // anonymous namespace

FLOAT64 Utility::Benchmark(void (*func)(void), UINT32 loops)
{
    BmarkFuncWrapperNoArg w(func);
    return TBenchmark(w, loops);
}
FLOAT64 Utility::Benchmark(void (*func)(void*), void* arg, UINT32 loops)
{
    BmarkFuncWrapper w(func, arg);
    return TBenchmark(w, loops);
}

// This function always returns an error code, even if errno is zero.
RC Utility::InterpretFileError()
{
    switch(errno)
    {
        case E2BIG:        return RC::FILE_2BIG;
        case EACCES:       return RC::FILE_ACCES;
        case EAGAIN:       return RC::FILE_AGAIN;
        case EBADF:        return RC::FILE_BADF;
        case EBUSY:        return RC::FILE_BUSY;
        case ECHILD:       return RC::FILE_CHILD;
        case EDEADLK:      return RC::FILE_DEADLK;
        case EEXIST:       return RC::FILE_EXIST;
        case EFAULT:       return RC::FILE_FAULT;
        case EFBIG:        return RC::FILE_FBIG;
        case EINTR:        return RC::FILE_INTR;
        case EILWAL:       return RC::FILE_ILWAL;
        case EIO:          return RC::FILE_IO;
        case EISDIR:       return RC::FILE_ISDIR;
        case EMFILE:       return RC::FILE_MFILE;
        case EMLINK:       return RC::FILE_MLINK;
        case ENAMETOOLONG: return RC::FILE_NAMETOOLONG;
        case ENFILE:       return RC::FILE_NFILE;
        case ENODEV:       return RC::FILE_NODEV;
        case ENOENT:       return RC::FILE_DOES_NOT_EXIST;
        case ENOEXEC:      return RC::FILE_NOEXEC;
        case ENOLCK:       return RC::FILE_NOLCK;
        case ENOMEM:       return RC::FILE_NOMEM;
        case ENOSPC:       return RC::FILE_NOSPC;
        case ENOSYS:       return RC::FILE_NOSYS;
        case ENOTDIR:      return RC::FILE_NOTDIR;
        case ENOTEMPTY:    return RC::FILE_NOTEMPTY;
        case ENOTTY:       return RC::FILE_NOTTY;
        case ENXIO:        return RC::FILE_NXIO;
        case EPERM:        return RC::FILE_PERM;
        case EPIPE:        return RC::FILE_PIPE;
        case EROFS:        return RC::FILE_ROFS;
        case ESPIPE:       return RC::FILE_SPIPE;
        case ESRCH:        return RC::FILE_SRCH;
        case EXDEV:        return RC::FILE_XDEV;
        default:           return RC::FILE_UNKNOWN_ERROR;
    }
}

// It'd be nice to make this faster, for now compare one byte at a time.
RC Utility::CompareFiles(const char *FileOneName, const char *FileTwoName, bool *FilesMatch)
{
    MASSERT(FileOneName);
    MASSERT(FileTwoName);
    MASSERT(FilesMatch);

    RC rc;
    FileHolder fileOne;
    FileHolder fileTwo;

    long Len1, Len2;

    *FilesMatch = false;

    {
        CHECK_RC(fileOne.Open(FileOneName, "r"));
        CHECK_RC(fileTwo.Open(FileTwoName, "r"));

        fseek(fileOne.GetFile(), 0, SEEK_END);
        fseek(fileTwo.GetFile(), 0, SEEK_END);

        Len1 = ftell(fileOne.GetFile());
        Len2 = ftell(fileTwo.GetFile());

        Printf(Tee::PriLow, "Len1 is %ld, Len 2 is %ld\n", Len1, Len2);

        if(Len1 != Len2)
            return OK;

        fseek(fileOne.GetFile(), 0, SEEK_SET);
        fseek(fileTwo.GetFile(), 0, SEEK_SET);

        UINT08 c1, c2;
        size_t bytesRead;
        while(true)
        {
            bytesRead = fread(&c1, 1, 1, fileOne.GetFile());
            if(!bytesRead)
            {
                break;
            }

            if (1 != fread(&c2, 1, 1, fileTwo.GetFile()))
            {
                Printf(Tee::PriLow, "Files differ in size\n");
                return OK;
            }
            if(c1 != c2)
            {
                Printf(Tee::PriLow, "Mismatch at file offset %ld, %d <> %d\n",
                       ftell(fileOne.GetFile()), c1, c2);
                return OK;
            }
        }

        Printf(Tee::PriHigh, "File %s matches file %s\n", FileOneName, FileTwoName);
        *FilesMatch = true;
    }

    return rc;
}

RC Utility::GetLineAndColumnFromFileOffset
(
    const vector<char> & fileData,
    size_t offset,
    size_t * pLine,
    size_t * pColumn
)
{
    if (offset >= fileData.size())
    {
        Printf(Tee::PriError, "%s: Offset (%zu) is larger than the amount of file data (%zu)\n",
               MODS_FUNCTION, offset, fileData.size());
        return RC::FILE_READ_ERROR;
    }

    *pLine = 1;
    *pColumn = 0;

    for (size_t ii = 0; ii <= offset; ++ii)
    {
        if (fileData[ii] == '\n')
        {
            (*pLine)++;
            *pColumn = 0;
        }

        (*pColumn)++;
    }

    return RC::OK;
}

RC Utility::OpenFile(const char *FileName, FILE **fp, const char *Mode)
{
    MASSERT(FileName);
    MASSERT(fp);
    MASSERT(Mode);

    string TheFile(FileName);

    return OpenFile(TheFile, fp, Mode);
}

RC Utility::OpenFile(const string& FileName, FILE **fp, const char *Mode)
{
    return InterpretModsUtilErrorCode(LwDiagUtils::OpenFile(FileName, fp, Mode));
}

RC Utility::ReadTarFile(const char* tarfilename, const char* filename, UINT32* pSize, char* pData)
{
    MASSERT(tarfilename);
    MASSERT(filename);

    const string sArchive = DefaultFindFile(tarfilename, true);

    if (!Xp::DoesFileExist(sArchive))
    {
        return RC::FILE_DOES_NOT_EXIST;
    }

    TarArchive archive;
    if (!archive.ReadFromFile(sArchive, true))
    {
        return RC::FILE_DOES_NOT_EXIST;
    }

    TarFile *pFile = archive.Find(filename);
    if (pFile == nullptr)
    {
        return RC::FILE_DOES_NOT_EXIST;
    }

    const UINT32 fileSize = pFile->GetSize();

    if (pSize)
        *pSize = fileSize;

    if (pData && pFile->Read(pData, fileSize) != fileSize)
    {
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

RC Utility::FWrite(const void *ptr, size_t Size, size_t NumMembers, FILE *fp)
{
    RC rc = OK;

    MASSERT(ptr);
    MASSERT(fp);

    if (NumMembers != fwrite(ptr, Size, NumMembers, fp))
    {
        rc = InterpretFileError();
    }

    return rc;
}

size_t Utility::FRead(void *ptr, size_t Size, size_t NumMembers, FILE *fp)
{
    size_t numRead = 0;

    MASSERT(ptr);
    MASSERT(fp);

    numRead = fread(ptr, Size, NumMembers, fp);
    return numRead;
}

//! \brief Return the size of a file.  If you get back OK, the filesize is
//!        guaranteed to be valid
RC Utility::FileSize(FILE *fp, long *pFileSize)
{
    return InterpretModsUtilErrorCode(LwDiagUtils::FileSize(fp, pFileSize));
}

//! \brief Strips the filename from a path
//!
//! Strips the filename in dos/windows/linux paths, leaving just the
//! directory.  If no path is provided, then the empty string will be
//! returned (not ".")
string Utility::StripFilename(const char *filenameWithPath)
{
    return LwDiagUtils::StripFilename(filenameWithPath);
}

string Utility::ExpandStringWithTabs
(
    const string& str,
    UINT32 targetSize,
    UINT32 tabSize
)
{
    MASSERT(targetSize % tabSize == 0);
    string tabs;
    if (str.size() < targetSize)
    {
        tabs = string(targetSize/tabSize - str.size()/tabSize, '\t');
    }
    return str + tabs;
}

string Utility::EncodeBase64(const vector<UINT08> &inputData)
{
    const char *encodeTable =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string outputString;
    UINT32 bitsToEncode = 0;
    UINT32 numBitsToEncode = 0;

    // Encode most of the data, except for 0-5 bits at the end if
    // the number of bits in the input data isn't a multiple of 6.
    //
    outputString.reserve(inputData.size() * 65/48 + 4);
    for (UINT32 ii = 0; ii < inputData.size(); ++ii)
    {
        bitsToEncode = (bitsToEncode << 8) + (inputData[ii] & 0x00ff);
        numBitsToEncode += 8;
        while (numBitsToEncode >= 6)
        {
            numBitsToEncode -= 6;
            outputString.push_back(
                encodeTable[0x003f & (bitsToEncode >> numBitsToEncode)]);
        }
    }

    // Encode the last 0-5 bits, and pad with the required number of
    // '=' chars.
    //
    if (numBitsToEncode > 0)
    {
        MASSERT(numBitsToEncode < 6);
        outputString.push_back(
            encodeTable[0x003f & (bitsToEncode << (6 - numBitsToEncode))]);
        numBitsToEncode = 0;
    }
    while ((outputString.size() % 4) != 0)
    {
        outputString.push_back('=');
    }

    return outputString;
}

vector<UINT08> Utility::DecodeBase64(const string &inputString)
{
    vector<UINT08> outputData;
    UINT32 bitsToDecode = 0;
    UINT32 numBitsToDecode = 0;

    outputData.reserve(inputString.size() * 48 / 65);

    for (UINT32 ii = 0; ii < inputString.size(); ++ii)
    {
        const char inputChar = inputString[ii];

        // Decode the base64 character into the corresponding 6-bit
        // number, or 0xff if the input char isn't a valid base64
        // char.
        //
        UINT32 decodedChar = 0xff;
        if (inputChar >= 'A' && inputChar <= 'Z')
            decodedChar = inputChar - 'A';
        else if (inputChar >= 'a' && inputChar <= 'z')
            decodedChar = 26 + (inputChar - 'a');
        else if (inputChar >= '0' && inputChar <= '9')
            decodedChar = 52 + (inputChar - '0');
        else if (inputChar == '+')
            decodedChar = 62;
        else if (inputChar == '/')
            decodedChar = 63;

        // Append the decoded 6 bits to the end of the output data.
        //
        if (decodedChar != 0xff)
        {
            bitsToDecode = (bitsToDecode << 6) + decodedChar;
            numBitsToDecode += 6;
            if (numBitsToDecode >= 8)
            {
                numBitsToDecode -= 8;
                outputData.push_back(
                    (UINT08)(0x00ff & (bitsToDecode >> numBitsToDecode)));
            }
        }
    }

    // At this point, if the number of decoded bits wasn't a multiple
    // of 8, then the remaining bits should all be zero.  MASSERT if
    // this isn't the case.
    //
    MASSERT((bitsToDecode & ((1 << numBitsToDecode) - 1)) == 0);

    return outputData;
}

RC Utility::Deflate(vector<UINT08> *pData)
{
    MASSERT(pData);
    z_stream strm;
    vector<UINT08> outputData;
    UINT08 deflateBuffer[256];
    int status;

    memset(&strm, 0, sizeof(strm));
    Utility::CleanupOnReturnArg<void,int,z_streamp> Cleanup(deflateEnd, &strm);
    status = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (status != Z_OK)
        return RC::SOFTWARE_ERROR;

    outputData.reserve(pData->size());
    strm.next_in = const_cast<UINT08*>(&(*pData)[0]);
    strm.avail_in = (uInt)pData->size();
    while (status != Z_STREAM_END)
    {
        strm.next_out = &deflateBuffer[0];
        strm.avail_out = sizeof(deflateBuffer);
        status = deflate(&strm, Z_FINISH);
        if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR)
            return RC::SOFTWARE_ERROR;
        if (&deflateBuffer[0] == strm.next_out)
            return RC::SOFTWARE_ERROR;
        outputData.insert(outputData.end(), &deflateBuffer[0], strm.next_out);
    }
    pData->swap(outputData);
    return OK;
}

RC Utility::Inflate(vector<UINT08> *pData)
{
    MASSERT(pData);
    z_stream strm;
    vector<UINT08> outputData;
    UINT08 inflateBuffer[256];
    int status;

    memset(&strm, 0, sizeof(strm));
    Utility::CleanupOnReturnArg<void,int,z_streamp> Cleanup(inflateEnd, &strm);
    status = inflateInit(&strm);
    if (status != Z_OK)
        return RC::SOFTWARE_ERROR;

    outputData.reserve(pData->size());
    strm.next_in = const_cast<UINT08*>(&(*pData)[0]);
    strm.avail_in = (uInt)pData->size();
    while (status != Z_STREAM_END)
    {
        strm.next_out = &inflateBuffer[0];
        strm.avail_out = sizeof(inflateBuffer);
        status = inflate(&strm, Z_FINISH);
        if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR)
            return RC::SOFTWARE_ERROR;
        if (&inflateBuffer[0] == strm.next_out)
            return RC::SOFTWARE_ERROR;
        outputData.insert(outputData.end(), &inflateBuffer[0], strm.next_out);
    }
    pData->swap(outputData);
    return OK;
}

//------------------------------------------------------------------------------
RC Utility::GetOperatorYesNo(const char *prompt, const char *messageForNo, RC rcForNo)
{
    Console::VirtualKey k;

    ConsoleManager::ConsoleContext consoleCtx;
    Console *pConsole = consoleCtx.AcquireRealConsole(true);
    do
    {
        Printf(Tee::PriHigh, "%s (Y/N)\n", prompt);
        k = pConsole->GetKey();
    }
    while(!(k == Console::VKC_CAPITAL_N || k == Console::VKC_CAPITAL_Y ||
            k == Console::VKC_LOWER_N   || k == Console::VKC_LOWER_Y));

    switch(k)
    {
        case Console::VKC_CAPITAL_N:
        case Console::VKC_LOWER_N:
            Printf(Tee::PriHigh, "%s\n", messageForNo);
        return rcForNo;

    default:
        return OK;
    }
}

string Utility::CombinePaths(const char *PathLeft, const char *PathRight)
{
    MASSERT(PathLeft != NULL);
    MASSERT(PathRight != NULL);

    size_t PathLeftLength = strlen(PathLeft);
    bool PathLeftHasSlash = (PathLeftLength == 0) ?
                            false :
                            ((PathLeft[PathLeftLength-1] == '/') || (PathLeft[PathLeftLength-1] == '\\'));
    bool PathRightHasSlash = (PathRight[0] == '/') || (PathRight[0] == '\\');

    string Result = PathLeft;

    if (!PathLeftHasSlash && !PathRightHasSlash)
    {
        if (PathLeftLength == 0)
        {
            Result = PathRight;
        }
        else
        {
            Result += "/";
            Result += PathRight;
        }
    }
    else if (PathLeftHasSlash && PathRightHasSlash)
    {
        Result += &PathRight[1];
    }
    else
    {
        Result += PathRight;
    }

    return Result;
}

string Utility::ToUpperCase(string InputStr)
{
    for (UINT32 i = 0; i<InputStr.length(); i++)
    {
        InputStr[i] = toupper(InputStr[i]);
    }

    return InputStr;
}

string Utility::ToLowerCase(string InputStr)
{
    for (UINT32 i = 0; i<InputStr.length(); i++)
    {
        InputStr[i] = tolower(InputStr[i]);
    }

    return InputStr;
}

string Utility::StringReplace
(
    const string &str_in,
    const string &str_find,
    const string &str_replace
)
{
    string work(str_in);
    size_t flength = str_find.length(), rlength = str_replace.length(), pos = 0;

    while (string::npos != (pos = work.find(str_find, pos)))
    {
        work.replace(pos, flength, str_replace);
        pos += rlength;
    }

    return work;
}

string Utility::FixPathSeparators(const string& str)
{
    const char c1 = DIR_SEPARATOR_CHAR2;
    const char c2 = DIR_SEPARATOR_CHAR;

    // Replace all oclwrences
    string result = str;
    size_t pos = result.find(c1);
    while (pos != string::npos)
    {
        result[pos] = c2;
        pos = result.find(c1, pos+1);
    }

    // Replace duplicates, except leading //
    const char dup[] = { c2, c2, 0 };
    pos = result.find(dup, 1);
    while (pos != string::npos)
    {
        result.erase(pos, 1);
        pos = result.find(dup, pos);
    }

    return result;
}


void Utility::CreateArgv(const string &buffer, vector<string> *elements)
{
    Tokenizer(buffer, " \t\n\v\f\r", elements);
}

int Utility::strcasecmp(const char *s1, const char *s2)
{
     while (toupper(*s1) == toupper(*s2++))
    {
        if (*s1++ == '\0')
        {
           return(0);
        }
    }

    return(toupper(*s1) - toupper(*--s2));
}

void Utility::PrintMsgAlways(const char *msg)
{
    Printf(Tee::PriAlways, "%s", msg);
}

void Utility::PrintMsgHigh(const char *msg)
{
    Printf(Tee::PriHigh, "%s", msg);
}

// SMethod
C_(Global_CountBits)
{
    JavaScriptPtr pJs;

    *pReturlwalue = JSVAL_NULL;

    // Check the arguments.
    UINT32 Value;
    if
    (
        (NumArguments != 1)
        || (OK != pJs->FromJsval(pArguments[0], &Value))
    )
    {
        JS_ReportError(pContext, "Usage: CountBits(value)");
        return JS_FALSE;
    }

    if (OK != pJs->ToJsval(Utility::CountBits(Value), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in CountBits()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_BitScanForward)
{
    RC rc;
    JavaScriptPtr pJs;

    *pReturlwalue = JSVAL_NULL;

    // Check the arguments.
    UINT32 Value = 0;
    UINT32 Start = 0;
    switch (NumArguments)
    {
        case 2: if (OK != (rc = pJs->FromJsval(pArguments[1], &Start))) break;
        case 1: if (OK != (rc = pJs->FromJsval(pArguments[0], &Value))) break;
            break;

        default:
            rc = RC::BAD_PARAMETER;
            break;

    }
    if (rc != OK)
    {
        JS_ReportError(pContext, "Usage: BitsScanForward(value, start = 0");
        return JS_FALSE;
    }

    if (OK != pJs->ToJsval(Utility::BitScanForward(Value, Start), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in BitScanForward()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Global_BitScanReverse)
{
    RC rc;
    JavaScriptPtr pJs;

    *pReturlwalue = JSVAL_NULL;

    // Check the arguments.
    UINT32 Value = 0;
    UINT32 End = 31;
    switch (NumArguments)
    {
        case 2: if (OK != (rc = pJs->FromJsval(pArguments[1], &End))) break;
        case 1: if (OK != (rc = pJs->FromJsval(pArguments[0], &Value))) break;
            break;

        default:
            rc = RC::BAD_PARAMETER;
            break;
    }
    if (rc != OK)
    {
        JS_ReportError(pContext, "Usage: BitsScanReverse(value, end = 31");
        return JS_FALSE;
    }

    if (OK != pJs->ToJsval(Utility::BitScanReverse(Value, End), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in BitScanReverse()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// Helper class for JsArrayObject to manage UINT32 values
class JsArrayInt
{
public:
    JsArrayInt(JSObject *pJsObj, INT32 index);
    JsArrayInt& operator=(UINT32 val);
    operator UINT32();

private:
    INT32 m_Index;
    JSObject *m_pJsObj;
};

// Specialized version of a JSObject that stores 32 bit integers.
// This object is optimized to only retrieve & store specified indexed values
// of the JS array.
class JsArrayObject
{
public:
    typedef UINT32 value_type;
    JsArrayObject(JSObject *pJsObj) {m_pJsObj = pJsObj;}
    JsArrayInt operator[](int idx);
    UINT32 operator[](int idx) const;
    size_t size() const;
private:
    JSObject * m_pJsObj;
};

// SMethod
C_(Global_CopyBits)
{
    RC rc;
    JavaScriptPtr pJs;
    *pReturlwalue = JSVAL_NULL;

    // Check the arguments.
    UINT32 srcBitOffset = 0;
    UINT32 dstBitOffset = 0;
    UINT32 numBits = 0;
    JSObject *pSrcArray = nullptr;
    JSObject *pDstArray = nullptr;

    if (NumArguments != 5)
    {
        rc = RC::BAD_PARAMETER;
    }
    else if (
        (pJs->FromJsval(pArguments[0], &pSrcArray) != OK) ||
        (pJs->FromJsval(pArguments[1], &pDstArray) != OK) ||
        (pJs->FromJsval(pArguments[2], &srcBitOffset) != OK) ||
        (pJs->FromJsval(pArguments[3], &dstBitOffset) != OK) ||
        (pJs->FromJsval(pArguments[4], &numBits) != OK) )
    {
        rc = RC::BAD_PARAMETER;
    }

    if (OK != rc)
    {
        JS_ReportError(pContext, "Usage: CopyBits(srcArray, dstArray, srcBitOffset, dstBitOffset, numBits)");
        return JS_FALSE;

    }

    //Generic implementation that only updates the required indices of the array
    JsArrayObject  jsDstArray(pDstArray);
    JsArrayObject  jsSrcArray(pSrcArray);
    rc = Utility::CopyBits(jsSrcArray, jsDstArray, srcBitOffset, dstBitOffset, numBits);
    if (OK != rc)
    {
        JS_ReportError(pContext, "Error oclwrred in CopyBits()");
        return JS_FALSE;
    }

    return JS_TRUE;
}

JsArrayInt::JsArrayInt(JSObject *pJsObj, INT32 index)
{
    m_Index = index;
    m_pJsObj = pJsObj;
}

JsArrayInt& JsArrayInt::operator=(UINT32 val)
{
    JavaScriptPtr pJs;
    pJs->SetElement(m_pJsObj, m_Index,val);
    return *this;
}

JsArrayInt::operator UINT32()
{
    UINT32 tmp;
    JavaScriptPtr pJs;
    pJs->GetElement(m_pJsObj, m_Index,&tmp);
    return tmp;
}

JsArrayInt JsArrayObject::operator[](int idx)
{
    return JsArrayInt(m_pJsObj,idx);
}

// const version of [] will not be modified so we can just return the value.
UINT32 JsArrayObject::operator[](int idx) const
{
    JavaScriptPtr pJs;
    UINT32 tmp;
    pJs->GetElement(m_pJsObj, idx,&tmp);
    return tmp;
}

size_t JsArrayObject::size() const
{
    UINT32 size = 0;
    JavaScriptPtr pJs;
    pJs->GetProperty(m_pJsObj, "length", &size);
    return (size_t)size;
}

C_(Global_GetSystemTimeInSeconds)
{
    JavaScriptPtr pJs;

    const double timeSec = static_cast<double>(Xp::GetWallTimeNS()) / 1'000'000'000.0;

    if (pJs->ToJsval(timeSec, pReturlwalue) != RC::OK)
    {
        pJs->Throw(pContext, RC::CANNOT_COLWERT_FLOAT_TO_JSVAL, "Failed to get system time");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

C_(Global_GetMODSStartTimeInSeconds)
{
    JavaScriptPtr pJs;

    const UINT64 timeSec = static_cast<UINT64>(OneTimeInit::GetDOBUnixTime());

    if (pJs->ToJsval(timeSec, pReturlwalue) != RC::OK)
    {
        JS_ReportError(pContext, "Error in GetMODSStartTimeInSeconds()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

C_(Global_CheckWritePerm)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;

    string filename;

    if(NumArguments != 1
      || (OK != pJs->FromJsval(pArguments[0], &filename)))
    {
        JS_ReportError(pContext, "Usage: CheckWritePerm(filename)");
        return JS_FALSE;
    }

    RETURN_RC(Xp::CheckWritePerm(filename.c_str()));
}

//------------------------------------------------------------------------------
// Os JS class
//
JS_CLASS(Os);

static SObject Os_Object
(
    "Os",
    OsClass,
    0,
    0,
    "Os class"
    );

C_(Os_DeleteFile);
static STest Os_DeleteFile
(
    Os_Object,
    "DeleteFile",
    C_Os_DeleteFile,
    0,
    "Delete file"
);

C_(Os_DeleteFile)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // parse arguments
    JavaScriptPtr pJavaScript;
    string FileName;
    if ((NumArguments != 1) || (OK != pJavaScript->FromJsval(pArguments[0], &FileName)))
    {
        JS_ReportError(pContext, "Usage: Os.DeleteFile(\"FileName\")");
        return JS_FALSE;
    }

    RETURN_RC(Xp::EraseFile(FileName));
}

C_(Os_DoesFileExist);
static SMethod Os_DoesFileExist
(
    Os_Object,
    "DoesFileExist",
    C_Os_DoesFileExist,
    0,
    "Returns true if file exists"
);

C_(Os_DoesFileExist)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // parse arguments
    JavaScriptPtr pJavaScript;
    string FileName;
    if ((NumArguments != 1) || (OK != pJavaScript->FromJsval(pArguments[0], &FileName)))
    {
        JS_ReportError(pContext, "Usage: Os.DoesFileExist(\"FileName\")");
        return JS_FALSE;
    }

    bool FileExist = Xp::DoesFileExist(FileName);

    // store return value
    if (OK != pJavaScript->ToJsval(FileExist, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Os::DoesFileExist(\"FileName\")");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
bool Utility::MatchWildCard(const char* str, const char* pat)
{
    while (1)
    {
        char p = *pat;
        char c = *str;

        switch(p)
        {
            case '\0':
                return c == '\0';
            case '*':
                // Try to match rest of the pattern at each point in str.
                pat++;
                while (1)
                {
                    if (MatchWildCard(str, pat))
                        return true;
                    if (*str == '\0')
                        return false;
                    str++;
                }
            case '?':
                if (c == '\0')
                    return false;
                break;
            default:
                if (p != c)
                    return false;
                break;
        }
        str++;
        pat++;
        }
        // Can't get here.
}

//------------------------------------------------------------------------------
RC Utility::AllocateLoadFile
(
    const char *Filename,
    UINT08 *&Buffer,
    UINT32 *BufferSize,
    BufferDataFormat Base
)
{
    RC rc;
    FileHolder file;
    UINT32 FileSize = 0;

    CHECK_RC(file.Open(Filename, "rb"));

    if (fseek(file.GetFile(), 0, SEEK_END) != 0)
        return RC::FILE_UNKNOWN_ERROR;

    const long ret = ftell(file.GetFile());
    if (ret == -1)
        return RC::FILE_UNKNOWN_ERROR;
    FileSize = ret;

    UINT08 *tempBuf = (UINT08*)malloc(FileSize);

    if (tempBuf == NULL)
        return RC::CANNOT_ALLOCATE_MEMORY;

    if (fseek(file.GetFile(), 0, SEEK_SET) != 0)
    {
        free(tempBuf);
        return RC::FILE_UNKNOWN_ERROR;
    }

    if (fread(tempBuf, 1, (size_t)FileSize, file.GetFile()) != (size_t)FileSize)
    {
        free(tempBuf);
        return RC::FILE_UNKNOWN_ERROR;
    }

    switch(Base)
    {
        // buffer is a char buffer so Colwert it to HEX Dump
        case HEX:
            {
                Buffer = (UINT08*)malloc(FileSize);
                UINT32 DstIdx = 0;
                for (UINT32 SrcIdx = 0; SrcIdx < FileSize ; SrcIdx += 2)
                {
                    Buffer[DstIdx] = (UINT08)(RAWToHEX(tempBuf[SrcIdx])* 16 + RAWToHEX(tempBuf[SrcIdx + 1]));
                    DstIdx ++;
                }
                free(tempBuf);
                if(BufferSize)
                    *BufferSize = DstIdx;
            }
            break;
        case RAW:
             Buffer = tempBuf;
             if (BufferSize)
                 *BufferSize = FileSize;
             break;
        case BIN:
        case OCT:
        case DEC:
            {
                Printf(Tee::PriHigh, "AllocateLoadFile : Not yet implemented Buffer formats "
                                     " for BIN OCT DEC \n");
                free(tempBuf);
                return RC::SOFTWARE_ERROR;
            }
    }

    return OK;
}

//! \brief  Used to check if a file is encrypted (note that the file must
//! be opened first)
Utility::EncryptionType Utility::GetFileEncryption(FILE* pFile)
{
    LwDiagUtils::EncryptionType lwduType;

    lwduType = LwDiagUtils::GetFileEncryption(pFile);

    switch (lwduType)
    {
        case LwDiagUtils::ENCRYPTED_JS:  return ENCRYPTED_JS;
        case LwDiagUtils::ENCRYPTED_LOG: return ENCRYPTED_LOG;
        case LwDiagUtils::ENCRYPTED_JS_V2:  return ENCRYPTED_JS_V2;
        case LwDiagUtils::ENCRYPTED_LOG_V2: return ENCRYPTED_LOG_V2;
        case LwDiagUtils::ENCRYPTED_FILE_V3: return ENCRYPTED_FILE_V3;
        case LwDiagUtils::ENCRYPTED_LOG_V3: return ENCRYPTED_LOG_V3;
        case LwDiagUtils::NOT_ENCRYPTED: return NOT_ENCRYPTED;
        default:
            MASSERT(!"Unknown encryption type");
            return NOT_ENCRYPTED;
    }
}

//-----------------------------------------------------------------------------
Utility::FileExistence Utility::FindPkgFile(const string& fileName, string* pOutFullPath)
{
    int flags = NoSuchFile;

    const string unencryptedPath = Utility::DefaultFindFile(fileName, true);
    if (Xp::DoesFileExist(unencryptedPath))
    {
        flags |= FileUnencrypted;
    }

    const string baseName = LwDiagUtils::StripDirectory(fileName.c_str());
    string encFileName = fileName;

    // Truncate all file extensions to two characters and append an 'e'
    // except with JSON files to avoid collsion with JS files (.jse)
    const char json[] = ".json";
    const auto jsonLen = sizeof(json) - 1;
    if (baseName.size() <= jsonLen ||
        baseName.compare(baseName.size() - jsonLen, jsonLen, json) != 0)
    {
        const auto dotPos = baseName.rfind('.');
        const auto extLen = (dotPos != string::npos) ? (baseName.size() - dotPos - 1) : 0;
        if (extLen > 2)
        {
            encFileName.resize(encFileName.size() - (extLen - 2));
        }
    }

    encFileName += 'e';
    const string encryptedPath = Utility::DefaultFindFile(encFileName, true);
    if (Xp::DoesFileExist(encryptedPath))
    {
        flags |= FileEncrypted;
    }

    if (pOutFullPath != nullptr)
    {
        if (flags & FileUnencrypted)
        {
            *pOutFullPath = unencryptedPath;
        }
        else if (flags & FileEncrypted)
        {
            *pOutFullPath = encryptedPath;
        }
        else
        {
            *pOutFullPath = "";
        }
    }

    return static_cast<FileExistence>(flags);
}

//-----------------------------------------------------------------------------
//! \brief Find and read a possibly-encrypted file.
//!
//! This method automatically searches for the file along the search
//! path, and tries the encrypted suffix (e.g. .xml => .xme).  If it
//! finds the file anywhere, it reads the file into pBuffer.
//!
//! \param filename The file to read
//! \param pBuffer On exit, contains the decrypted file contents.
//! \param pIsEncrypted On exit, tells whether the file was encrypted.
//!     May be NULL.
//!
RC Utility::ReadPossiblyEncryptedFile
(
    const string &fileName,
    vector<char> *pBuffer,
    bool *pIsEncrypted
)
{
    MASSERT(pBuffer);
    string actualFileName;
    RC rc;

    Printf(Tee::PriLow, "Looking for %s\n", fileName.c_str());
    const auto fileStatus = FindPkgFile(fileName, &actualFileName);

    // If we can't find the file anywhere, return an error
    //
    if (fileStatus == NoSuchFile)
    {
        Printf(Tee::PriError, "File '%s' not found\n", fileName.c_str());
        return RC::CANNOT_OPEN_FILE;
    }

    // Determine whether the file is encrypted.
    //
    FileHolder fileHolder(actualFileName, "rb");
    const bool isEncrypted = LwDiagUtils::GetFileEncryption(fileHolder.GetFile())
                             != LwDiagUtils::NOT_ENCRYPTED;
    if ((fileStatus == FileEncrypted) && !isEncrypted)
    {
        Printf(Tee::PriError, "File '%s' should be encrypted\n",
               actualFileName.c_str());
        return RC::FILE_READ_ERROR;
    }

    // Read the file into pBuffer, decrypting if needed
    //
    FILE *pFile = fileHolder.GetFile();
    if (!pFile)
    {
        Printf(Tee::PriError, "Cannot open file %s\n", actualFileName.c_str());
        return RC::CANNOT_OPEN_FILE;
    }

    if (isEncrypted)
    {
        CHECK_RC(InterpretModsUtilErrorCode(
                Decryptor::DecryptFile(pFile, reinterpret_cast<vector<UINT08>*>(pBuffer))));
    }
    else
    {
        long fileSize = 0;
        CHECK_RC(Utility::FileSize(pFile, &fileSize));
        pBuffer->resize(fileSize);

        size_t bytesRead = fread(&(*pBuffer)[0], 1, fileSize, pFile);
        if (bytesRead != static_cast<size_t>(fileSize))
        {
            Printf(Tee::PriError, "File '%s' read error\n",
                   actualFileName.c_str());
            return RC::FILE_READ_ERROR;
        }
    }

    // Set *pIsEncrypted and return
    //
    if (pIsEncrypted)
    {
        *pIsEncrypted = isEncrypted;
    }
    return rc;
}

namespace
{
#ifndef ENABLE_FUTURE_CHIPS
    vector<pair<string,string>> s_NameTranslations;

    bool IsJsStringSanitary(const string& str)
    {
        // The strings shouldn't include anything which could be used to inject malicious code
        // into our JS files via the string replacement of the redaction process.
        const static string s_IllegalList[] = {"(", ")", "{", "}", "function", "return", ";"};

        for (const auto& s : s_IllegalList)
        {
            if (str.find(s) != string::npos)
                return false;
        }

        return true;
    }

    void NetFetchTranslationFile(vector<char>* pFileData)
    {
        MASSERT(pFileData);
        pFileData->clear();

        string platform = "";
        const string buildArch = g_BuildArch;
        if (buildArch == "x86" || buildArch == "amd64")
            platform = "Linux";
        else if (buildArch == "ppc64le")
            platform = "Linux_ppc64le";
        else if (buildArch == "aarch64")
            platform = "Linux_aarch64";
        else
            return;

        const string version = g_Version;
        if (version.empty())
            return;

        string branch = "";
        if (!isalpha(version[0]))
            branch += "R";
        size_t pos = string::npos;
        if ((pos = version.find(".")) != string::npos)
            branch += version.substr(0,pos);
        else
            return;

        const string getStr = Utility::StrPrintf("/modswebapi/GetLookupTable/%s/%s/%s",
                                                 platform.c_str(), branch.c_str(), version.c_str());

        vector<char> serverData;
        LwDiagUtils::EC ec = LwDiagUtils::ReadLwidiaServerFile(getStr.c_str(), &serverData);
        if (OK == Utility::InterpretModsUtilErrorCode(ec))
        {
            vector<char> fileData;
            DEFER
            {
                if (!fileData.empty())
                {
                    memset(&fileData[0], 0, fileData.size());
                }
            };
            ec = Decryptor::DecryptDataArray(reinterpret_cast<const UINT08*>(&serverData[0]),
                                             serverData.size(),
                                             reinterpret_cast<vector<UINT08>*>(&fileData));
            if (OK == Utility::InterpretModsUtilErrorCode(ec))
            {
                pFileData->assign(fileData.begin(), fileData.end());
            }
        }
    }

    void LoadTranslations()
    {
        static const string internalNames = "INTERNAL_names.dbe";
        static bool s_Loaded = false;

        if (s_Loaded)
            return;

        // Try fetching from local translation file if it exists
        vector<char> fileData;
        const bool loadedLocalNames =
            Xp::DoesFileExist(internalNames) &&
            OK == Utility::ReadPossiblyEncryptedFile(internalNames, &fileData, nullptr) &&
            fileData.size() > 0;

        // Otherwise fetch from net
        if (!loadedLocalNames && !CommandLine::NoNet())
        {
            NetFetchTranslationFile(&fileData);
        }

        // Don't leave it unencrypted in memory
        DEFER
        {
            if (!fileData.empty())
            {
                memset(&fileData[0], 0, fileData.size());
            }
        };

        // Load any translations we fetched
        if (fileData.size() > 0)
        {
            string name = "";
            string value = "";
            string* lwrr = &value;
            for (const char& c: fileData)
            {
                if (c == '\r' || c == '\n')
                {
                    if (lwrr == &value)
                    {
                        lwrr = &name;
                    }
                    else
                    {
                        if (!IsJsStringSanitary(name) || !IsJsStringSanitary(value))
                        {
                            MASSERT(false);
                        }
                        else
                        {
                            s_NameTranslations.push_back(pair<string,string>(name, value));
                        }
                        name = "";
                        value = "";
                        lwrr = &value;
                    }
                }
                else
                {
                    *lwrr += c;
                }
            }
        }
        s_Loaded = true;
    }
#endif //ifndef ENABLE_FUTURE_CHIPS
}

string Utility::RedactString(string name)
{
#ifndef ENABLE_FUTURE_CHIPS
    LoadTranslations();

    for (const auto& trans : s_NameTranslations)
    {
        string::size_type pos = 0;
        while ((pos = name.find(trans.first, pos)) != string::npos)
        {
            name.replace(pos, trans.first.length(), trans.second);
            pos += trans.second.length();
        }
    }
#endif

    return name;
}

string Utility::RestoreString(string name)
{
#ifndef ENABLE_FUTURE_CHIPS
    LoadTranslations();

    for (const auto& trans : s_NameTranslations)
    {
        string::size_type pos = 0;
        while ((pos = name.find(trans.second, pos)) != string::npos)
        {
            name.replace(pos, trans.second.length(), trans.first);
            pos += trans.first.length();
        }
    }
#endif

    return name;
}

C_(Global_RedactString);
static SMethod Global_RedactString
(
    "RedactString",
    C_Global_RedactString,
    1,
    "Redacts the string"
);

C_(Global_RedactString)
{
    STEST_HEADER(1, 1, "Usage: RedactString(string)");
    STEST_ARG(0, string, str);

    if (OK != pJavaScript->ToJsval(Utility::RedactString(str), pReturlwalue))
    {
        JS_ReportError(pContext, usage);
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(Global_RestoreString);
static SMethod Global_RestoreString
(
    "RestoreString",
    C_Global_RestoreString,
    1,
    "Restores the string"
);

C_(Global_RestoreString)
{
    STEST_HEADER(1, 1, "Usage: RestoreString(string)");
    STEST_ARG(0, string, str);

    if (OK != pJavaScript->ToJsval(Utility::RestoreString(str), pReturlwalue))
    {
        JS_ReportError(pContext, usage);
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}


string Utility::GetBuildType()
{
    string info = g_BuildOS;
    const string buildArch = g_BuildArch;
    const string buildOSSubtype = g_BuildOSSubtype;
    const string builCfg = g_BuildCfg;

    if (!buildOSSubtype.empty() &&
        ((buildOSSubtype != "x86_gcc4") || (buildArch != "amd64")))
    {
        if ((buildArch == "x86") && (buildOSSubtype == "x86_gcc4"))
            info += " " + buildOSSubtype;
        else if (buildArch == "arm")
            info += " " + buildArch;
        else
            info += " " + buildArch + " " + buildOSSubtype;
    }
    else if (!buildArch.empty())
    {
        info += " " + buildArch;
    }
    if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
    {
        info += " ";
        info += builCfg;
    }

    return info;
}

P_(Global_Get_BuildType);
static SProperty Global_BuildType
(
    "BuildType",
    0,
    Global_Get_BuildType,
    0,
    0,
    "Build type."
);

P_(Global_Get_BuildType)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    if (OK != JavaScriptPtr()->ToJsval(Utility::GetBuildType(), pValue))
    {
        JS_ReportError(pContext, "Failed to get BuildType.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

string Utility::GetBuiltinModules()
{
    string info;
    if (g_INCLUDE_GPU)      info += "gpu ";
    if (g_INCLUDE_OGL)      info += "gl ";
    if (g_INCLUDE_OGL_ES)   info += "gles ";
    if (g_INCLUDE_LWOGTEST) info += "lwogtest ";
    if (g_INCLUDE_LWDA)     info += "lwca ";
    if (g_INCLUDE_LWDART)   info += "lwdart ";
#   ifdef INCLUDE_GPUTEST
                            info += "gputest ";
#   endif
    if (g_INCLUDE_RMTEST)   info += "rmtest ";
    if (g_INCLUDE_MDIAG)    info += "mdiag ";
    if (g_INCLUDE_MDIAGUTL) info += "mdiagutl ";
    // Remove trailing space
    if (!info.empty())
        info.resize(info.size()-1);
    return info;
}

P_(Global_Get_BuiltinModules);
static SProperty Global_BuiltinModules
(
    "BuiltinModules",
    0,
    Global_Get_BuiltinModules,
    0,
    0,
    "Build type."
);

P_(Global_Get_BuiltinModules)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    if (OK != JavaScriptPtr()->ToJsval(Utility::GetBuiltinModules(), pValue))
    {
        JS_ReportError(pContext, "Failed to get BuiltinModules.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}
P_(Global_IsSelwrityBypassActive_Getter);
static SProperty Global_IsSelwrityBypassActive
(
    "IsSelwrityBypassActive",
    0,
    false,
    Global_IsSelwrityBypassActive_Getter,
    0,
    JSPROP_READONLY,
    "Security bypass is active."
);

P_(Global_IsSelwrityBypassActive_Getter)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    static Deprecation depr("IsSelwrityBypassActive", "3/1/2018",
                            "IsSelwrityBypassActive has been deprecated\n");
    Deprecation::Allow("IsSelwrityBypassActive");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    const bool bBypassActive = Utility::GetSelwrityUnlockLevel() >= Utility::SUL_BYPASS_UNEXPIRED;
    RC rc = JavaScriptPtr()->ToJsval(bBypassActive, pValue);
    if (OK != rc)
    {
        JavaScriptPtr()->Throw(pContext, rc, "Failed to get IsSelwrityBypassActive.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

P_(Global_SelwrityUnlockLevel_Getter);
static SProperty Global_SelwrityUnlockLevel
(
    "SelwrityUnlockLevel",
    0,
    false,
    Global_SelwrityUnlockLevel_Getter,
    0,
    JSPROP_READONLY,
    "Security unlock level"
);

P_(Global_SelwrityUnlockLevel_Getter)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJs;
    RC rc = pJs->ToJsval(Utility::GetSelwrityUnlockLevel(), pValue);

    if (OK != rc)
    {
        pJs->Throw(pContext, rc, "Error getting GetSelwrityUnlockLevel");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

static SProperty Global_Sul_Lwidia_Network_Bypass
(
    "SUL_LWIDIA_NETWORK_BYPASS",
    0,
    Utility::SUL_LWIDIA_NETWORK_BYPASS,
    0,
    0,
    JSPROP_READONLY,
    ""
);
static SProperty Global_Sul_Lwidia_Network
(
    "SUL_LWIDIA_NETWORK",
    0,
    Utility::SUL_LWIDIA_NETWORK,
    0,
    0,
    JSPROP_READONLY,
    ""
);
static SProperty Global_Sul_Bypass_Unexpired
(
    "SUL_BYPASS_UNEXPIRED",
    0,
    Utility::SUL_BYPASS_UNEXPIRED,
    0,
    0,
    JSPROP_READONLY,
    ""
);
static SProperty Global_Sul_Bypass_Expired
(
    "SUL_BYPASS_EXPIRED",
    0,
    Utility::SUL_BYPASS_EXPIRED,
    0,
    0,
    JSPROP_READONLY,
    ""
);
static SProperty Global_Sul_None
(
    "SUL_NONE",
    0,
    Utility::SUL_NONE,
    0,
    0,
    JSPROP_READONLY,
    ""
);

RC Utility::CallOptionalJsMethod(const char* FunctionName)
{
    JsArray Params;
    jsval RetValJs = JSVAL_NULL;
    UINT32 RetVal = RC::SOFTWARE_ERROR;
    JavaScriptPtr pJs;
    JSObject* pGlobObj = pJs->GetGlobalObject();

    // Determine whether the function exists
    JSFunction * pFunction = 0;
    if (OK != pJs->GetProperty(pGlobObj, FunctionName, &pFunction))
    {
        return OK;
    }

    // Ilwoke JS function
    RC rc;
    CHECK_RC(pJs->CallMethod(pGlobObj, FunctionName, Params, &RetValJs));

    // Handle return value
    CHECK_RC(pJs->FromJsval(RetValJs, &RetVal));
    if (RC::UNDEFINED_JS_METHOD == RetVal)
    {
        RetVal = OK;
    }

    // Return error code from the JS function
    return RetVal;
}

//-----------------------------------------------------------------------------
// Remove the whitespace at the head and tail of a string.
// Original str will be changed
char* Utility::RemoveHeadTailWhitespace(char* str)
{
    if ((str == 0) || (*str == 0))
        return str;

    char* begin = str;
    char* end = str + strlen(str) - 1;

    while (isspace(*begin) && (begin < end))
        begin++;

    while (isspace(*end) && (begin < end))
        end--;

    if ((begin == end) && isspace(*end))
    {
        *str = 0;
    }
    else
    {
        if (begin == str)
        {
            *(end+1) = 0;
        }
        else
        {
            char* tmp = str;
            while(begin <= end)
                *(tmp++) = *(begin++);

            *tmp = 0;
        }
    }

    return str;
}

//-----------------------------------------------------------------------------
// Remove whitespaces from string
string Utility::RemoveSpaces(string str)
{
    str.erase(remove(str.begin(), str.end(), ' '), str.end());
    return str;
}

// Exit status to return to shell on MODS failure. Usually 1, but set to 2 for
// RETEST case in field diag. See bug 1393389.
static INT32 s_ShellFailStatus = 1;

bool Utility::OnLwidiaIntranet()
{
    return Security::OnLwidiaIntranet();
}

INT32 Utility::GetShellFailStatus()
{
    return s_ShellFailStatus;
}

C_(Global_SetShellFailStatus);
static STest Global_SetShellFailStatus
(
    "SetShellFailStatus",
    C_Global_SetShellFailStatus,
    1,
    "Set exit status to return to shell on failure."
);

C_(Global_SetShellFailStatus)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    INT32 newStatus;

    if ( (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &newStatus)) )
    {
        JS_ReportError(pContext, "Usage: SetShellFailStatus(value)");
        return JS_FALSE;
    }

    s_ShellFailStatus = newStatus;
    return JS_TRUE;
}

int Utility::ExitMods(RC ExitCode, ExitMode Mode)
{
    int shellStatus;

    // Platform specific clean up code.
    if (Mode == ExitQuickAndDirty)
    {
        if(OK == Xp::OnQuickExit(ExitCode))
        {
            shellStatus = 0;
        }
        else
        {
            shellStatus = s_ShellFailStatus;
        }
        _exit(shellStatus);
    }

    if(OK == Xp::OnExit(ExitCode))
    {
        shellStatus = 0;
    }
    else
    {
        shellStatus = s_ShellFailStatus;
    }

    // see bug 631117
    int ElwRc = ExitCode.Get();
    // Set the MODS_EC environment variable to the first failure.
    string EC = StrPrintf("%08d", ElwRc);
    Xp::SetElw("MODS_EC", EC.c_str());

    // Store final RC in the json logfile (if enabled).
    JsonLogStream::WriteFinalRc(Log::FirstErrorStr());

    return shellStatus;
}

//-----------------------------------------------------------------------------
JSBool Global_UnhandledErrorCount_Getter(JSContext * cx, JSObject * obj, jsval id, jsval *vp);
JSBool Global_UnhandledErrorCount_Getter(JSContext * cx, JSObject * obj, jsval id, jsval *vp)
{
    *vp = INT_TO_JSVAL(ErrorLogger::UnhandledErrorCount());
    return JS_TRUE;
}

static SProperty Global_UnhandledErrorCount
(
    "UnhandledErrorCount",
    0,
    false,
    Global_UnhandledErrorCount_Getter,
    0,
    JSPROP_READONLY,
    "Number of unhandled LogError calls (unexpected gpu interrupts)."
);

UINT32 Utility::Gcf(UINT32 Num1, UINT32 Num2)
{
    // Euclid's algorithm
    while (Num2 != 0)
    {
        const UINT32 Tmp = Num1 % Num2;
        Num1 = Num2;
        Num2 = Tmp;
    }
    return Num1;
}

//----------------------------------------------------------------------------
RC Utility::DumpProgram
(
    const string & filenameTemplate,
    const void*  buffer,
    const size_t bytes
)
{
    RC rc;
    JavaScriptPtr pJs;
    bool doDump;
    rc = pJs->GetProperty(pJs->GetGlobalObject(), "g_DumpPrograms", &doDump);
    if (OK != rc)
    {
        // Ignore "invalid object property" error.
        return OK;
    }

    if (doDump)
    {
        // Dump .lwbin or GL_LW_gpu_program files as they are loaded.
        // We have a script that dumps these to SASS for evaluating which
        // gpu shader instructions are reached in a given mods run.

        const string temp = Utility::StrPrintf("t%d_%s",
                ErrorMap::Test(), filenameTemplate.c_str());
        const string fname = Utility::CreateUniqueFilename(temp);

        FileHolder f;
        rc = f.Open(fname, "w");
        if (OK == rc)
        {
            if (bytes != fwrite(buffer, 1, bytes, f.GetFile()))
                rc = Utility::InterpretFileError();
        }
    }
    return rc;
}

RC Utility::InterpretModsUtilErrorCode(LwDiagUtils::EC ec)
{
    switch (ec)
    {
        case LwDiagUtils::OK:                 return OK;
        case LwDiagUtils::FILE_2BIG:          return RC::FILE_2BIG;
        case LwDiagUtils::FILE_ACCES:         return RC::FILE_ACCES;
        case LwDiagUtils::FILE_AGAIN:         return RC::FILE_AGAIN;
        case LwDiagUtils::FILE_BADF:          return RC::FILE_BADF;
        case LwDiagUtils::FILE_BUSY:          return RC::FILE_BUSY;
        case LwDiagUtils::FILE_CHILD:         return RC::FILE_CHILD;
        case LwDiagUtils::FILE_DEADLK:        return RC::FILE_DEADLK;
        case LwDiagUtils::FILE_EXIST:         return RC::FILE_EXIST;
        case LwDiagUtils::FILE_FAULT:         return RC::FILE_FAULT;
        case LwDiagUtils::FILE_FBIG:          return RC::FILE_FBIG;
        case LwDiagUtils::FILE_INTR:          return RC::FILE_INTR;
        case LwDiagUtils::FILE_ILWAL:         return RC::FILE_ILWAL;
        case LwDiagUtils::FILE_IO:            return RC::FILE_IO;
        case LwDiagUtils::FILE_ISDIR:         return RC::FILE_ISDIR;
        case LwDiagUtils::FILE_MFILE:         return RC::FILE_MFILE;
        case LwDiagUtils::FILE_MLINK:         return RC::FILE_MLINK;
        case LwDiagUtils::FILE_NAMETOOLONG:   return RC::FILE_NAMETOOLONG;
        case LwDiagUtils::FILE_NFILE:         return RC::FILE_NFILE;
        case LwDiagUtils::FILE_NODEV:         return RC::FILE_NODEV;
        case LwDiagUtils::FILE_NOENT:         return RC::FILE_DOES_NOT_EXIST;
        case LwDiagUtils::FILE_NOEXEC:        return RC::FILE_NOEXEC;
        case LwDiagUtils::FILE_NOLCK:         return RC::FILE_NOLCK;
        case LwDiagUtils::FILE_NOMEM:         return RC::FILE_NOMEM;
        case LwDiagUtils::FILE_NOSPC:         return RC::FILE_NOSPC;
        case LwDiagUtils::FILE_NOSYS:         return RC::FILE_NOSYS;
        case LwDiagUtils::FILE_NOTDIR:        return RC::FILE_NOTDIR;
        case LwDiagUtils::FILE_NOTEMPTY:      return RC::FILE_NOTEMPTY;
        case LwDiagUtils::FILE_NOTTY:         return RC::FILE_NOTTY;
        case LwDiagUtils::FILE_NXIO:          return RC::FILE_NXIO;
        case LwDiagUtils::FILE_PERM:          return RC::FILE_PERM;
        case LwDiagUtils::FILE_PIPE:          return RC::FILE_PIPE;
        case LwDiagUtils::FILE_ROFS:          return RC::FILE_ROFS;
        case LwDiagUtils::FILE_SPIPE:         return RC::FILE_SPIPE;
        case LwDiagUtils::FILE_SRCH:          return RC::FILE_SRCH;
        case LwDiagUtils::FILE_XDEV:          return RC::FILE_XDEV;
        case LwDiagUtils::FILE_UNKNOWN_ERROR: return RC::FILE_UNKNOWN_ERROR;
        case LwDiagUtils::SOFTWARE_ERROR:     return RC::SOFTWARE_ERROR;
        case LwDiagUtils::ILWALID_FILE_FORMAT:return RC::ILWALID_FILE_FORMAT;
        case LwDiagUtils::CANNOT_ALLOCATE_MEMORY:return RC::CANNOT_ALLOCATE_MEMORY;
        case LwDiagUtils::FILE_DOES_NOT_EXIST:return RC::FILE_DOES_NOT_EXIST;
        case LwDiagUtils::PREPROCESS_ERROR:   return RC::PREPROCESS_ERROR;
        case LwDiagUtils::BAD_COMMAND_LINE_ARGUMENT:return RC::BAD_COMMAND_LINE_ARGUMENT;
        case LwDiagUtils::BAD_BOUND_JS_FILE:  return RC::SOFTWARE_ERROR;
        case LwDiagUtils::NETWORK_NOT_INITIALIZED: return RC::NETWORK_NOT_INITIALIZED;
        case LwDiagUtils::NETWORK_ALREADY_CONNECTED: return RC::NETWORK_ALREADY_CONNECTED;
        case LwDiagUtils::NETWORK_CANNOT_CREATE_SOCKET: return RC::NETWORK_CANNOT_CREATE_SOCKET;
        case LwDiagUtils::NETWORK_CANNOT_CONNECT: return RC::NETWORK_CANNOT_CONNECT;
        case LwDiagUtils::NETWORK_CANNOT_BIND: return RC::NETWORK_CANNOT_BIND;
        case LwDiagUtils::NETWORK_ERROR:       return RC::NETWORK_ERROR;
        case LwDiagUtils::NETWORK_WRITE_ERROR: return RC::NETWORK_WRITE_ERROR;
        case LwDiagUtils::NETWORK_READ_ERROR:  return RC::NETWORK_READ_ERROR;
        case LwDiagUtils::NETWORK_NOT_CONNECTED: return RC::NETWORK_NOT_CONNECTED;
        case LwDiagUtils::NETWORK_CANNOT_DETERMINE_ADDRESS: return RC::NETWORK_CANNOT_DETERMINE_ADDRESS;
        case LwDiagUtils::TIMEOUT_ERROR:        return RC::TIMEOUT_ERROR;
        case LwDiagUtils::UNSUPPORTED_FUNCTION: return RC::UNSUPPORTED_FUNCTION;
        case LwDiagUtils::BAD_PARAMETER:        return RC::BAD_PARAMETER;
        case LwDiagUtils::DLL_LOAD_FAILED:      return RC::DLL_LOAD_FAILED;
        case LwDiagUtils::DECRYPTION_ERROR:     return RC::ENCRYPT_DECRYPT_FAILED;
        default:
            MASSERT(!"Unknown EC from LwDiagUtils");
            return RC::SOFTWARE_ERROR;
    }
}

//------------------------------------------------------------------------------
FLOAT64 Utility::CallwlateConfidenceLevel
(
    FLOAT64 timeSec,
    FLOAT64 bitRateMbps,
    UINT32  errors,
    FLOAT64 desiredBER
)
{
    // BERs is always multipled by bitRate which is in units of 1e6
    desiredBER *= 1e6;
    const FLOAT64 bitsTimesBER = timeSec * bitRateMbps * desiredBER;
    if (errors == 0)
        return (1.0 - exp(-bitsTimesBER)) * 100.0;

    // Confidence level is callwlated as
    //
    // 1 - e^(-bitsTimesBER) * Sum(k:0->errors)((bitsTimesBER)^k / k!)
    //
    // Within the sum each term k can be expressed as
    //     f(k) = f(k - 1) * (bitsTimesBER / k)
    //
    // Perform the callwlation in the log space to provide the ability to account for large
    // numbers of errors and bits transmitted
    //
    // Start with the last term in the sum sequence that doesnt depend on the previous
    // entries in the sequence log(bitsTimesBER / errors), the loop below effectively
    // callwlates log(f(k - 1)) and iteratively adds it to the, which since the callwlation
    // is performed in the log scale effectively callwlates f(k).
    //
    // In addition, by adding in log(1 + exp(-logSum)), the loop also keeps around all
    // previous terms and generates the entire sum.
    //
    // At the end of the loop  logSum will contain:
    //
    // errors*log(bitsTimesBER) - log(errors) - log(errors - 1) - ... - log(1) + (summing term)
    //
    // Discounting the summing term exp(logSum) will be the last term of the sequence       :
    //
    //    ((bitsTimesBER)^errors)/errors!
    //
    // The expansion of the summing term is effectively:
    //
    //    1 + errors / ((bitsTimesBER) ^ 1)) + (errors * (errors - 1)) / ((bitsTimesBER) ^ 2) +
    //        ... errors! / ((bitsTimesBER) ^ errors))
    //
    // Multiplying this by the last term effectively callwlates the sum sequence
    FLOAT64 logSum = log(bitsTimesBER / errors);
    for (UINT32 k = 1; k < errors; k++)
    {
        logSum = log(bitsTimesBER) - log(errors - k) + logSum + log(1 + exp(-logSum));
    }

    logSum = logSum + log(1 + exp(-logSum));
    FLOAT64 cl = 1.0 - exp(-bitsTimesBER + logSum);
    return (cl < 1e-6) ? 0.0 : cl;
}

//------------------------------------------------------------------------------
// Colwert from Javascript data into C data.  Also runs PreparePickItems().
//------------------------------------------------------------------------------
RC Utility::PickItemsFromJsval
(
    jsval js,
    vector<Random::PickItem> *pItems
)
{
    MASSERT(pItems);

    JavaScriptPtr  pJavaScript;
    JsArray        OuterArray;
    INT32         rc;

    // OuterArray must be an array of 1 to NumItemsMax things.
    if(OK != (rc = pJavaScript->FromJsval(js, &OuterArray)))
    {
        return rc;
    }
    UINT32 NumItems = (UINT32)OuterArray.size();
    if(1 > NumItems)
    {
        return RC::BAD_PICK_ITEMS;
    }
    pItems->clear();
    pItems->resize(NumItems);

    // Each thing in the OuterArray must be an array of 2 or 3 numbers.
    for(UINT32 i = 0; i < NumItems; i++)
    {
        JsArray  InnerArray;

        if(OK != (rc = pJavaScript->FromJsval(OuterArray[i], &InnerArray)))
            return RC::BAD_PICK_ITEMS;

        UINT32 InnerSize = (UINT32)InnerArray.size();

        if((InnerSize < 2) || (InnerSize > 3))
            return RC::BAD_PICK_ITEMS;

        if(OK != (rc = pJavaScript->FromJsval(InnerArray[0], &((*pItems)[i].RelProb))))
            return RC::BAD_PICK_ITEMS;
        if(OK != (rc = pJavaScript->FromJsval(InnerArray[1], &((*pItems)[i].Min))))
            return RC::BAD_PICK_ITEMS;

        if(3 == InnerSize)
        {
         if(OK != (rc = pJavaScript->FromJsval(InnerArray[2], &((*pItems)[i].Max))))
            return RC::BAD_PICK_ITEMS;
        }
        else
        {
            (*pItems)[i].Max = (*pItems)[i].Min;
        }
        (*pItems)[i].ScaledProb = 0;
        (*pItems)[i].ScaledRange = 0.0;

        if((*pItems)[i].RelProb < 1)
            return RC::BAD_PICK_ITEMS;

    }
    // Preprocess the PickItems.
    // NOTE: We are assuming vector<> is implemented as an array.
    Random::PreparePickItems(NumItems, &(*pItems)[0]);

    return OK;
}

//------------------------------------------------------------------------------
// Dump a the text file to MODS output.
// Note: Substitutes non-printable characters with a period symbol.
//------------------------------------------------------------------------------
RC Utility::DumpTextFile
(
    INT32        Priority,
    const char * FileName
)
{
    FileHolder file;
    RC rc;
    char data[80+1];

    CHECK_RC(file.Open(FileName, "r"));

    while (1)
    {
        // One character reserved for the terminator
        size_t maxLen = sizeof(data) - 1;

        // Read a chunk of the file
        size_t len = fread(data, 1, maxLen, file.GetFile());

        if (len > 0)
        {
            // Replacing non-printable characters with a period symbol. In
            // consequence, we don't skip characters that follow the
            // terminating character '\000'.
            for (UINT32 i = 0; i < len; i++)
            {
                if (!(isprint(data[i]) || isspace(data[i])))
                {
                    data[i] = '.';
                }
            }

            // Terminate the data
            data[len] = 0;

            // Forward the chunk to MODS output
            Printf(Priority, "%s", data);
        }

        if (len < maxLen) break;
    }

    if (ferror(file.GetFile()))
    {
        return RC::FILE_IO;
    }

    return OK;
}

//------------------------------------------------------------------------------
bool Utility::IsInf(double x)
{
    // The normal C++ way to portably get a floating-point infinity
    // value is to use numeric_limits<double>::infinity().
    // But our GCC 2.95 compiler is missing the <limits> header.
    // So we resort to brute force here, using the IEEE float raw bits.
    union { UINT64 u; double d; } inf;

    // In IEEE double-precision floats,
    //   bit 63:63 sign      0 means positive
    //   bit 62:52 exponent  max value 0x7ff is reserved for INF, NAN.
    //   bit 51:0  mantissa  0 for +-infinity, nonzero for NAN
    inf.u = 0x7FF0000000000000ULL;
    return inf.d == fabs(x);
}

void Utility::InitSelwrityUnlockLevel(SelwrityUnlockLevel bypassUnlockLevel)
{
    return Security::InitSelwrityUnlockLevel(
        static_cast<Security::UnlockLevel>(bypassUnlockLevel));
}

Utility::SelwrityUnlockLevel Utility::GetSelwrityUnlockLevel()
{
    return static_cast<Utility::SelwrityUnlockLevel>(Security::GetUnlockLevel());
}

Utility::StopWatch::StopWatch(string name, Tee::Priority pri)
: m_Name(move(name))
, m_StartTime(Xp::GetWallTimeNS())
, m_Pri(pri)
{
}

UINT64 Utility::StopWatch::Stop()
{
    UINT64 elapsed = 0;
    if (m_StartTime)
    {
        elapsed = Xp::GetWallTimeNS() - m_StartTime;
        Printf(m_Pri, "%s: %f s\n",
               m_Name.c_str(), elapsed/1000000000.0);
        m_StartTime = 0;
    }
    return elapsed;
}

Utility::TotalStopWatch::TotalStopWatch(string name)
: m_Name(move(name))
, m_TotalTime(0)
{
}

Utility::TotalStopWatch::~TotalStopWatch()
{
    Printf(Tee::PriNormal, "%s: %f s\n", m_Name.c_str(), m_TotalTime/1000000000.0);
}

Utility::PartialStopWatch::PartialStopWatch(TotalStopWatch& timer)
: m_Timer(timer)
, m_StartTime(Xp::GetWallTimeNS())
{
}

Utility::PartialStopWatch::~PartialStopWatch()
{
    m_Timer.m_TotalTime += Xp::GetWallTimeNS() - m_StartTime;
}

struct AstcFileHeader
{
    UINT08 magic[4];
    UINT08 blockDimX;
    UINT08 blockDimY;
    UINT08 blockDimZ;
    UINT08 xSize[3];
    UINT08 ySize[3];
    UINT08 zSize[3];
};

static const UINT32 ASTC_HEADER_MAGIC = 0x5CA1AB13;

RC Utility::ReadAstcFile(const vector<char>& astcBuf, ASTCData* astcData)
{
    RC rc;

    const size_t hdrSize = sizeof(AstcFileHeader);
    if (astcBuf.size() <= hdrSize)
    {
        Printf(Tee::PriError, "Invalid ASTC header size\n");
        return RC::FILE_READ_ERROR;
    }
    const AstcFileHeader& hdr = *reinterpret_cast<const AstcFileHeader*>(&astcBuf[0]);

    const UINT32 hdrMagic = *reinterpret_cast<const UINT32*>(&hdr.magic[0]);
    if (hdrMagic != ASTC_HEADER_MAGIC)
    {
        Printf(Tee::PriError, "Invalid ASTC header magic value: 0x%x\n", hdrMagic);
        rc = RC::FILE_READ_ERROR;
        return rc;
    }

    astcData->imageWidth =  (hdr.xSize[2] << 16) + (hdr.xSize[1] << 8) + (hdr.xSize[0] << 0);
    astcData->imageHeight = (hdr.ySize[2] << 16) + (hdr.ySize[1] << 8) + (hdr.ySize[0] << 0);
    astcData->imageDepth =  (hdr.zSize[2] << 16) + (hdr.zSize[1] << 8) + (hdr.zSize[0] << 0);

    UINT08 blockX = hdr.blockDimX;
    UINT08 blockY = hdr.blockDimY;
    UINT08 blockZ = hdr.blockDimZ;

    // 3D ASTC files are not yet supported
    if (astcData->imageDepth != 1)
    {
        Printf(Tee::PriError,
            "3D Astc files are lwrrently unsupported! ImageDepth=%u\n",
            astcData->imageDepth);
        rc = RC::UNSUPPORTED_DEPTH;
        return rc;
    }

    // Validate that the file size based on the header properties matches actual
    // file size
    size_t imageFileSize = CEIL_DIV(astcData->imageWidth, blockX) *
                           CEIL_DIV(astcData->imageHeight, blockY) *
                           CEIL_DIV(astcData->imageDepth, blockZ) *
                           ASTC_BLOCK_SIZE;
    size_t expectedSize = imageFileSize + sizeof(AstcFileHeader);
    if (astcBuf.size() != expectedSize)
    {
        Printf(Tee::PriError,
               "Expected ASTC file to contain %u bytes, but it contains %u\n",
               static_cast<unsigned>(expectedSize), static_cast<unsigned>(astcBuf.size()));
        rc = RC::FILE_READ_ERROR;
        return rc;
    }

    astcData->imageData.resize(imageFileSize);
    memcpy(&astcData->imageData[0], &astcBuf[hdrSize], imageFileSize);

    return rc;
}

string Utility::FormatRawEcid(const vector<UINT32>& rawEcid)
{
    string chipIdStr = "";

    for (UINT32 i = 0; i < rawEcid.size(); i++)
    {
        chipIdStr += Utility::StrPrintf("%08x", rawEcid[i]);
    }

    // Dont print leading zeros to save column space in mods.log. However there
    // is a minimum size of 24 chars we have to adhere to. see B1873893
    auto pos = chipIdStr.find_first_not_of('0');
    if (pos != std::string::npos && pos != 0)
    {
        size_t extra0s = min(chipIdStr.length() - 24, pos); // we are putting one back below.
        if (extra0s > 0)
        {
            chipIdStr.erase(0, extra0s);
        }
    }
    return chipIdStr.insert(0, "0x");
}

void Utility::GetRandomBytes(UINT08 *bytes, UINT32 size)
{
#ifdef _WIN32
    HCRYPTPROV cryptProv;

    if (CryptAcquireContext(&cryptProv, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) 
    {
        if (CryptGenRandom(cryptProv, size, bytes))
        {
            CryptReleaseContext(cryptProv, 0);
            return;
        }
    }
#else
    const INT32 fd = open("/dev/urandom", O_RDONLY);

    if (fd != -1)
    {
        const ssize_t numRead = read(fd, bytes, size);
        close(fd);
        if ((UINT32)numRead == size)
            return;
    }
#endif

    // Fallback to use walltime
    const UINT32 multiplier = 0x8088405U;
    UINT32 state = ((UINT32)Xp::GetWallTimeNS() << 1) | 1U;
    UINT32 i;

    for (i = 0; i < 4; i++)
    {
        state = state * multiplier + 1;
    }

    for (i = 0; i < size; i++) 
    {
        state = state * multiplier + 1;
        bytes[i] = (UINT08)(state >> 23);
    }
}

string Utility::FormatBytesToString(const UINT08 *bytes, UINT32 size)
{
    MASSERT(size != 0);
    string bytesString;

    for (UINT32 i = 0; i < size; i++)
    {
        bytesString += Utility::StrPrintf("%02x-", bytes[i]);
    }
   
    // Remove trailing '-'
    bytesString.resize(bytesString.size() - 1);

    return bytesString;
}

static SProperty Global_INCLUDE_USB
(
    "g_IncludeUsb",
    0,
#ifdef INCLUDE_XUSB
    true,
#else
    false,
#endif
    0,
    0,
    JSPROP_READONLY,
    "Indicates if USB is included with the build"
);

static SProperty Global_INCLUDE_FSLIB
(
    "g_IncludeFslib",
    0,
#ifdef INCLUDE_FSLIB
    true,
#else
    false,
#endif
    0,
    0,
    JSPROP_READONLY,
    "Indicates if FsLib is included with the build"
);

C_(Global_CallwlateConfidenceLevel);
static SMethod Global_CallwlateConfidenceLevel
(
    "CallwlateConfidenceLevel",
    C_Global_CallwlateConfidenceLevel,
    4,
    "Callwlate the confidence level that a target bit error rate has been achieved."
);

C_(Global_CallwlateConfidenceLevel)
{
    STEST_HEADER(4, 4,
                 "Usage: CallwlateConfidenceLevel(timeSec, bitRateMbps, errors, desiredBER)");
    STEST_ARG(0, FLOAT64, timeSec);
    STEST_ARG(1, FLOAT64, bitRateMbps);
    STEST_ARG(2, UINT32,  errors);
    STEST_ARG(3, FLOAT64, desiredBER);

    const FLOAT64 confidenceLevel = Utility::CallwlateConfidenceLevel(timeSec,
                                                                      bitRateMbps,
                                                                      errors,
                                                                      desiredBER);
    RC rc = pJavaScript->ToJsval(confidenceLevel, pReturlwalue);
    if (rc != RC::OK)
    {
        pJavaScript->Throw(pContext, rc, "Error oclwrred in CallwlateConfidenceLevel()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

//------------------------------------------------------------------------------
void Utility::GetUniformDistribution
(
    UINT32 numBins,
    UINT32 numElements,
    vector<UINT32> *pDistribution
)
{
    MASSERT(pDistribution);

    UINT32 maxSubPerElement    = (numElements + numBins - 1) / numBins;
    UINT32 binsWithMaxElements = numElements % numBins;
    if (binsWithMaxElements == 0)
    {
        binsWithMaxElements = numBins;
    }

    for (UINT32 element = 0; element < numBins; element++)
    {
        UINT32 subElementsPerElement = 0;
        if (element < binsWithMaxElements)
        {
            subElementsPerElement = maxSubPerElement;
        }
        else
        {
            subElementsPerElement = maxSubPerElement - 1;
        }
        pDistribution->push_back(subElementsPerElement);
    }
}
