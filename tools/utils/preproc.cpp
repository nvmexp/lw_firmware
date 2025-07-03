/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*
 *  This is an implementation of the C preprocessing language.
 *
 *  It has been written generally following the GNU C preprocessor manual:
 *  https://gcc.gnu.org/onlinedocs/cpp
 *
 *  This preprocessor is used to preprocess JavaScript files.  As such, we do not
 *  need a fully compliant C preprocessor.  For this reason, several tradeoffs
 *  have been made and there are some features which are not implemented.
 *  Most unimplemented preprocessor features have been dolwmented in the comments.
 */

#include "preproc.h"
#include <algorithm>
#include <errno.h>
#include <iterator>
#include <string.h>

#ifdef _MSC_VER
#   define snprintf _snprintf
#endif

#if defined _MSC_VER && _MSC_VER < 1500
#   define SCAN64 "I64"
#else
#   define SCAN64 "ll"
#endif

// Uncomment this to get helpful debugging messages
//#define DEBUG_INFO 1

namespace LwDiagUtils
{
    EC InterpretFileError(int error);
}

namespace
{
    LwDiagUtils::EC NoDecrypt(FILE*, vector<UINT08>*)
    {
        LWDASSERT(!"Not implemented");
        return LwDiagUtils::ILWALID_FILE_FORMAT;
    }

    string IntToStr(int value)
    {
        char buf[16];
        buf[sizeof(buf)-1] = 0;
        snprintf(buf, sizeof(buf)-1, "%d", value);
        return buf;
    }

    const LwDiagUtils::Preprocessor::Punctuator* FindPunctuator(LwDiagUtils::StringView token, char c = '\0')
    {
        using PP = LwDiagUtils::Preprocessor;

        // Notes:
        // - No support for digraphs nor trigraphs.
        // - Mark "//" and "/*" as punctuators.  Don't worry - they are consumed by GetNextToken().
        // - The parser recognized JavaScript operators correctly, unlike regular C preprocessor.
        // - No love for JavaScript regular expression syntax though!
        // - The C preprocessor normally only supports a handful of operators.
        // - The precedence levels have been taken from http://en.cppreference.com/w/c/language/operator_precedence
        static const PP::Punctuator punctuators[] =
        {
            { "!",    PP::PT_LOGICAL_NOT,  2 },
            { "!=",   PP::PT_NE,           7 },
            { "!==",  PP::PT_ILLEGAL,      0 }, // JavaScript operator, not available in C
            { "#",    PP::PT_ILLEGAL,      0 },
            { "##",   PP::PT_ILLEGAL,      0 },
            { "%",    PP::PT_MOD,          3 },
            { "%=",   PP::PT_ILLEGAL,      0 },
            { "&",    PP::PT_AND,          8 },
            { "&&",   PP::PT_LOGICAL_AND, 11 },
            { "&=",   PP::PT_ILLEGAL,      0 },
            { "(",    PP::PT_PAREN_OPEN,   1 },
            { ")",    PP::PT_PAREN_CLOSE,  1 },
            { "*",    PP::PT_MUL,          3 },
            { "*=",   PP::PT_ILLEGAL,      0 },
            { "+",    PP::PT_ADD,          4 }, // 2 for unary plus, but we don't care about unary operator precedence
            { "++",   PP::PT_ILLEGAL,      0 },
            { "+=",   PP::PT_ILLEGAL,      0 },
            { ",",    PP::PT_ILLEGAL,      0 },
            { "-",    PP::PT_SUB,          4 }, // 2 for unary minus, but we don't care about unary operator precedence
            { "--",   PP::PT_ILLEGAL,      0 },
            { "-=",   PP::PT_ILLEGAL,      0 },
            { "->",   PP::PT_ILLEGAL,      0 },
            { "->*",  PP::PT_ILLEGAL,      0 },
            { ".",    PP::PT_ILLEGAL,      0 },
            { ".*",   PP::PT_ILLEGAL,      0 },
            { "...",  PP::PT_ILLEGAL,      0 },
            { "/",    PP::PT_DIV,          3 },
            { "/*",   PP::PT_ILLEGAL,      0 },
            { "//",   PP::PT_ILLEGAL,      0 },
            { "/=",   PP::PT_ILLEGAL,      0 },
            { ":",    PP::PT_COLON,       13 },
            { "::",   PP::PT_ILLEGAL,      0 },
            { ";",    PP::PT_ILLEGAL,      0 },
            { "<",    PP::PT_LT,           6 },
            { "<<",   PP::PT_SHL,          5 },
            { "<<=",  PP::PT_ILLEGAL,      0 },
            { "<=",   PP::PT_LE,           6 },
            { "=",    PP::PT_ILLEGAL,      0 },
            { "==",   PP::PT_EQ,           7 },
            { "===",  PP::PT_ILLEGAL,      0 }, // JavaScript operator, not available in C
            { ">",    PP::PT_GT,           6 },
            { ">=",   PP::PT_GE,           6 },
            { ">>",   PP::PT_SHR,          5 },
            { ">>=",  PP::PT_ILLEGAL,      0 },
            { ">>>",  PP::PT_ILLEGAL,      0 }, // JavaScript operator, not available in C
            { ">>>=", PP::PT_ILLEGAL,      0 }, // JavaScript operator, not available in C
            { "?",    PP::PT_TERNARY,     13 },
            { "[",    PP::PT_ILLEGAL,      0 },
            { "]",    PP::PT_ILLEGAL,      0 },
            { "^",    PP::PT_XOR,          9 },
            { "^=",   PP::PT_ILLEGAL,      0 },
            { "{",    PP::PT_ILLEGAL,      0 },
            { "|",    PP::PT_OR,          10 },
            { "|=",   PP::PT_ILLEGAL,      0 },
            { "||",   PP::PT_LOGICAL_OR,  12 },
            { "}",    PP::PT_ILLEGAL,      0 },
            { "~",    PP::PT_NOT,          2 }
        };
        const auto compare =
            [&](const PP::Punctuator& punc)->int
            {
                const char* tokStr    = token.begin();
                size_t      remaining = token.size();
                const char* puncStr   = punc.str;
                char        puncChar  = *puncStr;
                while (puncChar && remaining)
                {
                    const auto a = static_cast<unsigned char>(puncChar);
                    const auto b = static_cast<unsigned char>(*tokStr);
                    if (a < b)
                    {
                        return -1;
                    }
                    if (a > b)
                    {
                        return 1;
                    }
                    puncChar = *(++puncStr);
                    ++tokStr;
                    --remaining;
                }

                if (!puncChar)
                {
                    return remaining || c ? -1 : 0;
                }

                LWDASSERT(!remaining);

                return static_cast<int>(static_cast<unsigned char>(puncChar))
                     - static_cast<int>(static_cast<unsigned char>(c));
            };
        const auto endPunc = end(punctuators);
        const auto found = lower_bound(begin(punctuators), endPunc, 0,
                                       [&](const PP::Punctuator& punc, int)->bool
                                       { return compare(punc) < 0; });
        if (found == endPunc)
        {
            return nullptr;
        }
        return compare(*found) == 0 ? found : nullptr;
    }

    struct DirectiveItem
    {
        const char*                          identifierString;
        LwDiagUtils::Preprocessor::Directive id;
    };

    const DirectiveItem* FindDirective(LwDiagUtils::StringView token)
    {
        using PP = LwDiagUtils::Preprocessor;

        // No support for some directives, such as #warning or #line
        static const DirectiveItem directives[] =
        {
            { "define",  PP::D_DEFINE  },
            { "elif",    PP::D_ELIF    },
            { "else",    PP::D_ELSE    },
            { "endif",   PP::D_ENDIF   },
            { "error",   PP::D_ERROR   },
            { "if",      PP::D_IF      },
            { "ifdef",   PP::D_IFEQ    },
            { "ifndef",  PP::D_IFNEQ   },
            { "include", PP::D_INCLUDE },
            { "pragma",  PP::D_PRAGMA  },
            { "undef",   PP::D_UNDEF   }
        };
        const auto compare = [&](const DirectiveItem& dir)->int
            {
                return strncmp(dir.identifierString, token.begin(), token.size());
            };
        const auto endDir = end(directives);
        const auto found  = lower_bound(begin(directives), endDir, 0,
            [&](const DirectiveItem& dir, int)->bool
            {
                return compare(dir) < 0;
            });
        if (found == endDir)
        {
            return nullptr;
        }
        return compare(*found) == 0 ? found : nullptr;
    }

    int FindEol(const LwDiagUtils::StringView& str, int pos)
    {
        const auto begin = str.begin();
        auto       ptr   = begin + pos;
        const auto end   = str.end();

        LWDASSERT(ptr <= end);

        while (ptr < end)
        {
            const auto c = *ptr;
            if (c == '\r' || c == '\n')
            {
                return static_cast<int>(ptr - begin);
            }
            ++ptr;
        }

        return -1;
    }
#ifdef NDEBUG
#define Touch(x) do { } while (false)
#else
    void Touch(LwDiagUtils::StringViewBuilder svb)
    {
        const LwDiagUtils::StringView sv = svb;
        for (const auto& c : sv)
        {
            LWDASSERT(c);
        }
    }
#endif
}

const LwDiagUtils::Preprocessor::CharType LwDiagUtils::Preprocessor::m_CharTable[256] =
{
    // (0)
    CT_WHITESPACE,

    // (1)..(8)
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,

    // TAB(9),     LF(10), VT(11),        FF(12),        CR(13)
    CT_WHITESPACE, CT_EOL, CT_WHITESPACE, CT_WHITESPACE, CT_EOL,

    // (14)..(22)
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,

    // (23)..(31)
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,

    // SPACE(32),  !(33),         "(34)
    CT_WHITESPACE, CT_PUNCTUATOR, CT_STRING,

    // Treat hash as a punctuator here and worry about special treatment
    // of '#' and '##' later.

    // #(35)
    CT_PUNCTUATOR,

    // Treat $ as a letter.  This is not conforming to the standard, but
    // this is how GNU C preprocessor does it and we need this for JavaScript
    // as well.

    // $(36)
    CT_LETTER,

    // %(37),         &(38),         '(39)
    CT_PUNCTUATOR, CT_PUNCTUATOR, CT_STRING,

    // ((40),      )(41),         *(42),         +(43),         ,(44),         -(45),         .(46),         /(47)
    CT_PUNCTUATOR, CT_PUNCTUATOR, CT_PUNCTUATOR, CT_PUNCTUATOR, CT_PUNCTUATOR, CT_PUNCTUATOR, CT_PUNCTUATOR, CT_PUNCTUATOR,

    // 0(48)..9(57)
    CT_DIGIT, CT_DIGIT, CT_DIGIT, CT_DIGIT, CT_DIGIT, CT_DIGIT, CT_DIGIT, CT_DIGIT, CT_DIGIT, CT_DIGIT,

    // :(58),      ;(59),         <(60),         =(61),         >(62),         ?(63)          @(64)
    CT_PUNCTUATOR, CT_PUNCTUATOR, CT_PUNCTUATOR, CT_PUNCTUATOR, CT_PUNCTUATOR, CT_PUNCTUATOR, CT_OTHER,

    // A(65)..I(73)
    CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER,

    // J(74)..R(82)
    CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER,

    // S(83)..Z(90)
    CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER,

    // [(91)
    CT_PUNCTUATOR,

    // Treat a naked backslash as "other" (typically invalid).
    // Backslashes which are part of strings are consumed by strings automatically.
    // Backslashes which serve for line continuations are handled by
    // SkipLineContinuation().

    // \(92)
    CT_OTHER,

    // ](93),      ^(94),         _(95),     `(96)
    CT_PUNCTUATOR, CT_PUNCTUATOR, CT_LETTER, CT_OTHER,

    // a(97)..i(105)
    CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER,

    // j(106)..r(114)
    CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER,

    // s(115)..z(122)
    CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER,

    // {(123),     |(124),        }(125),        ~(126),        (127)
    CT_PUNCTUATOR, CT_PUNCTUATOR, CT_PUNCTUATOR, CT_PUNCTUATOR, CT_OTHER,

    // Treat all UTF-8 encodings as "other".
    // This will give us incorrect column numbers, but it's OK for JS purposes.

    // (128)..(255)
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER,
    CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER, CT_OTHER
};

int LwDiagUtils::StringView::Compare(char c) const
{
    if (m_Size > 0)
    {
        const int firstLetter = static_cast<int>(static_cast<unsigned char>(m_String[0]))
                              - static_cast<int>(static_cast<unsigned char>(c));

        if (firstLetter)
        {
            return firstLetter;
        }
    }

    return static_cast<int>(m_Size) - 1;
}

int LwDiagUtils::StringView::Compare(const StringView& other) const
{
    const size_t minSize = min(m_Size, other.m_Size);

    if (minSize)
    {
        const int binaryCompare = memcmp(m_String, other.m_String, minSize);
        if (binaryCompare)
        {
            return binaryCompare;
        }
    }

    return static_cast<int>(m_Size) - static_cast<int>(other.m_Size);
}

size_t LwDiagUtils::StringView::GetHash() const
{
    // djb2a algorithm
    size_t hash = 5381;

    for (char c : *this)
    {
        hash = (hash * 33U) ^ static_cast<size_t>(static_cast<unsigned char>(c));
    }

    return hash;
}

LwDiagUtils::StringViewBuilder::operator LwDiagUtils::StringView() const
{
    if (m_Size == 0)
    {
        return StringView();
    }

    LWDASSERT(!m_StoreBuffers->empty());
    const auto& buf = m_StoreBuffers->back();
    return StringView(&buf[buf.size() - m_Size], m_Size);
}

LwDiagUtils::StringViewBuilder& LwDiagUtils::StringViewBuilder::operator+=(char c)
{
    *MakeRoom(1) = c;
    m_Size++;
    Touch(*this);
    return *this;
}

LwDiagUtils::StringViewBuilder LwDiagUtils::StringViewBuilder::operator+(const char* s) const
{
    const size_t size = strlen(s);
    memcpy(MakeRoom(size), s, size);
    StringViewBuilder svb(m_StoreBuffers, m_Size + size);
    Touch(svb);
    return svb;
}

LwDiagUtils::StringViewBuilder LwDiagUtils::StringViewBuilder::operator+(const string& s) const
{
    const size_t size = s.size();
    memcpy(MakeRoom(size), &s[0], size);
    StringViewBuilder svb(m_StoreBuffers, m_Size + size);
    Touch(svb);
    return svb;
}

LwDiagUtils::StringViewBuilder& LwDiagUtils::StringViewBuilder::operator+=(const StringView& s)
{
    const size_t size = s.size();
    memcpy(MakeRoom(size), s.begin(), size);
    m_Size += size;
    Touch(*this);
    return *this;
}

char* LwDiagUtils::StringViewBuilder::MakeRoom(size_t size) const
{
    const bool wasEmpty = m_StoreBuffers->empty();
    if (wasEmpty || m_StoreBuffers->back().size() + size > m_StoreBuffers->back().capacity())
    {
        m_StoreBuffers->emplace_back();

        if (m_Size > 0U)
        {
            LWDASSERT(!wasEmpty);
            m_StoreBuffers->back().resize(m_Size);
            auto it = m_StoreBuffers->end();
            --it;
            --it;
            const auto& buf = *it;
            memcpy(&m_StoreBuffers->back()[0], &buf[buf.size() - m_Size], m_Size);
        }
    }

    auto& buf = m_StoreBuffers->back();
    LWDASSERT(size + m_Size <= buf.capacity());

    char* const ptr = &buf[buf.size()];

    buf.resize(buf.size() + size);
    LWDASSERT(ptr == &buf[buf.size() - size]);

#ifndef NDEBUG
    memset(ptr, 0, size);
#endif

    return ptr;
}

void LwDiagUtils::StringViewBuilder::Append(StringView* pStr, const char* buf, size_t len)
{
    if (pStr->size() == 0)
    {
        *pStr = StringView(buf, len);
        return;
    }

    const auto tokenEnd = pStr->end();

    if (tokenEnd == buf)
    {
        *pStr = StringView(pStr->begin(), pStr->size() + len);
        return;
    }

    if (!m_StoreBuffers->empty())
    {
        auto& genBuf = m_StoreBuffers->back();

        const auto genBufEnd = &genBuf[genBuf.size()];
        const auto remaining = genBuf.capacity() - genBuf.size();

        if (tokenEnd == genBufEnd && len <= remaining)
        {
            genBuf.resize(genBuf.size() + len);
            memcpy(genBufEnd, buf, len);
            *pStr = StringView(pStr->begin(), pStr->size() + len);
            return;
        }
    }

    *this += *pStr;
    *this += StringView(buf, len);
    *pStr = *this;
}

void LwDiagUtils::Preprocessor::StringStack::Push(StringView str)
{
    m_Stack.append(str.begin(), str.size());
    m_Stack.push_back('\0');
}

void LwDiagUtils::Preprocessor::StringStack::Pop()
{
    LWDASSERT(m_Stack.size() > 1);
    const auto zeroPos = m_Stack.rfind('\0', m_Stack.size() - 2);
    if (zeroPos == string::npos)
    {
        m_Stack.clear();
    }
    else
    {
        m_Stack.resize(zeroPos + 1);
    }
}

bool LwDiagUtils::Preprocessor::StringStack::HasString(StringView str) const
{
    string::size_type foundPos = 0;
    for (;;)
    {
        foundPos = m_Stack.find(str.begin(), foundPos, str.size());
        if (foundPos == string::npos)
        {
            return false;
        }

        LWDASSERT(foundPos + str.size() < m_Stack.size());
        if (m_Stack[foundPos + str.size()] == 0)
        {
            // The found string must be surrounded by 0s
            if (foundPos == 0 || m_Stack[foundPos - 1] == 0)
            {
                return true;
            }
        }

        ++foundPos;
    }
    return false;
}

LwDiagUtils::Preprocessor::Preprocessor()
: m_DecryptFile(&NoDecrypt)
{
    // Hand-picked for gputest.js.  Reserving enough space visibly reduces run time.
    // At the time of this writing, gputest.js defines 862 macros.
    m_Macros.reserve(1024U);
}

LwDiagUtils::Preprocessor::~Preprocessor()
{
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::SetBuffer(const char* buf, size_t size)
{
    LWDASSERT(m_Files.empty());

    const File desc =
    {
        "<<input>>",
        buf,
        buf + size,
        1,
        1
    };

    m_Files.push_back(desc);
    m_LineNumbers.clear();
    EmitLineNumber(StartFlag);

    return OK;
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::LoadFile(const string& file)
{
    LWDASSERT(m_Files.empty());
    m_LineNumbers.clear();
    return PushFile(file);
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::PushFile(const string& file)
{
    EC ec = OK;

    string fullPath = file;

    // Look in the path relative to the current file's path first
    if (!m_Files.empty())
    {
        const string& lastPath = m_Files.back().path;

        size_t slashPos = lastPath.rfind('/');
        if (slashPos == string::npos)
            slashPos = lastPath.rfind('\\');

        if (slashPos != string::npos)
        {
            fullPath = lastPath.substr(0, slashPos+1) + file;
        }
    }

    // Check if the file is there. If not, look in search paths.
    if (!LwDiagXp::DoesFileExist(fullPath))
    {
        if (m_SearchPaths.empty())
        {
            Printf(PriNormal, "File not found - %s\n", file.c_str());
            return PREPROCESS_ERROR;
        }

        string findFullPath = FindFile(file, m_SearchPaths);

        if (!findFullPath.empty())
        {
            fullPath = findFullPath + file;
        }
        else
        {
            // Look for encrypted version
            fullPath += "e";
            if (!LwDiagXp::DoesFileExist(fullPath))
            {
                findFullPath = FindFile(file + "e", m_SearchPaths);
                if (findFullPath.empty())
                {
                    Printf(PriNormal, "File not found - %s\n", file.c_str());
                    return PREPROCESS_ERROR;
                }

                fullPath = findFullPath + file + "e";
            }
        }
    }

    auto& buf = m_Buffers[fullPath];

    if (buf.empty())
    {
        FileHolder fh;
        CHECK_EC(fh.Open(fullPath.c_str(), "rb"));

        long size = 0;
        CHECK_EC(FileSize(fh.GetFile(), &size));

        const EncryptionType encType = GetFileEncryption(fh.GetFile());
        fseek(fh.GetFile(), 0, SEEK_SET);
        if (encType == NOT_ENCRYPTED)
        {
            buf.resize(size, '\0');

            if (fread(&buf[0], 1, size, fh.GetFile()) != static_cast<size_t>(size))
            {
                const int error = errno;
                Printf(PriNormal, "Failed to read from %s - %s\n",
                       file.c_str(), strerror(error));
                return InterpretFileError(error);
            }
        }
        else
        {
            CHECK_EC(m_DecryptFile(fh.GetFile(), &buf));
        }
    }

    const char* const data = reinterpret_cast<const char*>(&buf[0]);
    const File desc =
    {
        fullPath,
        data,
        data + buf.size(),
        1,
        1
    };

    m_Files.push_back(desc);
    EmitLineNumber(m_Files.size() == 1 ? StartFlag : PushFileFlag);

    return ec;
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::Process(vector<char>* pResult)
{
    LWDASSERT(m_Files.size() == 1);

    if (m_Files.size() != 1)
    {
        Printf(PriNormal, "Error: Invalid arguments to Preprocessor::Process()\n");
        return LwDiagUtils::SOFTWARE_ERROR;
    }

    LWDASSERT(pResult);
    pResult->clear();
    pResult->reserve(m_Files.back().end - m_Files.back().runningBuf);

    m_HashIfStack.clear();

    m_TokensStack.clear();

    m_DisabledCode   = false;
    m_NumOutputLines = 0;

    m_LwrFile = m_Files.back().path;

    m_Macros.clear();
    AddBuiltinMacro("defined",  BUILTIN_DEFINED, true);
    AddBuiltinMacro("__FILE__", BUILTIN_FILE);
    AddBuiltinMacro("__LINE__", BUILTIN_LINE);

    // Add user-defined macros
    for (auto& userMacro : m_UserMacros)
    {
        Macro& macro = m_Macros[GenerateStringView() + userMacro.name];
        macro.builtin = NOT_BUILTIN;
        macro.hasArgs = false;

        // Ensure the user macro is non-empty for parsing purposes
        string& macroContents = userMacro.contents;
        if (macroContents.empty())
        {
            macroContents += '\n';
        }

        const File desc =
        {
            "user macro " + userMacro.name,
            &macroContents[0],
            &macroContents[0] + macroContents.size(),
            1,
            1
        };

        m_Files.push_back(desc);

        EC ec;
        CHECK_EC(GetNextLine(&macro.tokens, nullptr, nullptr));

        // Remove trailing TT_EOL or TT_EOF
        if (!macro.tokens.empty())
        {
            LWDASSERT(macro.tokens.back().type == TT_EOL || macro.tokens.back().type == TT_EOF);
            macro.tokens.pop_back();
        }

        m_Files.pop_back();
    }

    OutputLineNumbers(pResult);

    Tokens tokens;

    EC ec;

    // Preprocess subsequent lines.
    for (;;)
    {
        // Retrieve next line.  Line continuations are collapsed automatically.
        const char* lineBegin = nullptr;
        const char* lineEnd   = nullptr;
        CHECK_EC(GetNextLine(&tokens, &lineBegin, &lineEnd));

        OutputLineNumbers(pResult);

        // No tokens means end of preprocessing.
        // It means that we've exhausted the stack of files.
        // FYI: empty lines have an EOL token.
        if (tokens.empty())
        {
            break;
        }

        int       numLinesIn      = tokens.back().lineNum - tokens.front().lineNum + 1;
        const int saveNumLinesOut = m_NumOutputLines;

        if (m_CollapseComments)
        {
            if (CollapseComments(tokens.begin(), tokens.end()))
            {
                lineEnd = lineBegin; // Avoid OutputLineFast
            }
        }

        m_LwrFile = m_Files.back().path;

        TokensIt lwr = find_if(tokens.begin(), tokens.end(), Token::IsNonWS);

        // Handle preprocessor directives
        if (lwr != tokens.end() && lwr->token == '#')
        {
            lwr = find_if(next(lwr), tokens.end(), Token::IsNonWS);

            if (!m_DisabledCode)
            {
                if (lwr == tokens.end())
                {
                    PrintError("Invalid preprocessor directive");
                    return PREPROCESS_ERROR;
                }
                if (lwr->type != TT_IDENTIFIER)
                {
                    PrintError(*lwr, "Invalid preprocessor directive");
                    return PREPROCESS_ERROR;
                }
            }

            Directive directive = D_ILWALID;

            if (lwr != tokens.end() && lwr->type == TT_IDENTIFIER)
            {
                const auto directiveIt = FindDirective(lwr->token);

                if (!directiveIt && ! m_DisabledCode)
                {
                    PrintError(*lwr, "Invalid preprocessor directive - " +
                                     string(lwr->token.begin(), lwr->token.size()));
                    return PREPROCESS_ERROR;
                }

                if (directiveIt)
                {
                    directive = directiveIt->id;
                    lwr       = find_if(next(lwr), tokens.end(), Token::IsNonWS);
                }
            }

            switch (directive)
            {
                case D_DEFINE:  CHECK_EC(DirectiveDefine  (lwr, tokens.end())); break;
                case D_ELIF:    CHECK_EC(DirectiveElif    (lwr, tokens.end())); break;
                case D_ELSE:    CHECK_EC(DirectiveElse    (lwr, tokens.end())); break;
                case D_ENDIF:   CHECK_EC(DirectiveEndif   (lwr, tokens.end())); break;
                case D_ERROR:   CHECK_EC(DirectiveError   (lwr, tokens.end())); break;
                case D_IF:      CHECK_EC(DirectiveIf      (lwr, tokens.end())); break;
                case D_IFEQ:    CHECK_EC(DirectiveIfDef   (lwr, tokens.end())); break;
                case D_IFNEQ:   CHECK_EC(DirectiveIfNDef  (lwr, tokens.end())); break;
                case D_INCLUDE: CHECK_EC(DirectiveInclude (lwr, tokens.end())); break;
                case D_PRAGMA:  CHECK_EC(DirectivePragma  (lwr, tokens.end())); break;
                case D_UNDEF:   CHECK_EC(DirectiveUndef   (lwr, tokens.end())); break;
                case D_ILWALID: LWDASSERT(m_DisabledCode);                      break;
                default:
                    LWDASSERT(0);
                    return SOFTWARE_ERROR;
            }

            OutputEmptyLine(tokens, pResult);
        }
        // Emit an empty line if the block of code was #ifdef'ed-out
        else if (m_DisabledCode)
        {
            OutputEmptyLine(tokens, pResult);
        }
        // Normal code, so expand macros
        else
        {
            LWDASSERT(!m_Files.empty());
            const string lwrFile = m_Files.back().path;
            const int    lwrLine = m_Files.back().lineNum;

            bool doneAnything = false;
            CHECK_EC(ExpandMacros(&tokens, &doneAnything, ExpandFull));

            // Prevent creation of new tokens due to macro expansion
            if (doneAnything)
            {
                PreventNewTokens(&tokens);
            }

            if (doneAnything || m_Files.empty() || lineEnd != m_Files.back().runningBuf)
            {
                Output(tokens, pResult);
            }
            else
            {
                OutputLineFast(lineBegin, lineEnd, pResult);
            }

            // Update number of lines read due to function-like macro expansion
            if (!m_Files.empty() && m_Files.back().path == lwrFile && m_Files.back().lineNum > lwrLine)
            {
                numLinesIn += m_Files.back().lineNum - lwrLine;
            }
        }

        // Catch up with line numbers if there were line continuations and we didn't emit line number
        if (!OutputLineNumbers(pResult))
        {
            const int numLinesOut = m_NumOutputLines - saveNumLinesOut;
            for (int i = numLinesIn - numLinesOut; i > 0; i--)
            {
                OutputEmptyLine(tokens, pResult);
            }
        }
    }

    LWDASSERT(m_Files.empty());

    if (!m_HashIfStack.empty())
    {
        PrintError("Missing #endif directive");
        return PREPROCESS_ERROR;
    }

#ifdef DEBUG_INFO
    size_t generatedSize = 0;
    for (const auto& buf : m_Generated)
    {
        generatedSize += buf.size();
    }
    printf("Size of generated tokens: %u bytes\n", static_cast<unsigned>(generatedSize));
#endif

    return OK;
}

void LwDiagUtils::Preprocessor::PrintError(const string& msg) const
{
    Printf(PriNormal, "%s:%d: %s\n",
            m_LwrFile.c_str(),
            m_LwrLineNum,
            msg.c_str());
}

void LwDiagUtils::Preprocessor::PrintError(const Token& token, const string& msg) const
{
    Printf(PriNormal, "%s:%d:%d: %s\n",
            m_LwrFile.c_str(),
            token.lineNum,
            token.column,
            msg.c_str());
}

void LwDiagUtils::Preprocessor::Output(const Tokens& tokens, vector<char>* pResult)
{
    for (const auto& tok : tokens)
    {
        m_NumOutputLines += CountEOLs(&tok.token[0], &tok.token[tok.token.size()]);
        pResult->insert(pResult->end(), tok.token.begin(), tok.token.end());
    }
}

void LwDiagUtils::Preprocessor::OutputLineFast(const char* begin, const char* end, vector<char>* pResult)
{
    pResult->insert(pResult->end(), begin, end);
    ++m_NumOutputLines;
}

void LwDiagUtils::Preprocessor::OutputEmptyLine(const Tokens& tokens, vector<char>* pResult)
{
    pResult->push_back('\n');
    ++m_NumOutputLines;
}

bool LwDiagUtils::Preprocessor::OutputLineNumbers(vector<char>* pResult)
{
    if (m_LineNumbers.empty())
    {
        return false;
    }

    m_NumOutputLines += CountEOLs(&m_LineNumbers[0], &m_LineNumbers[0]+m_LineNumbers.size());
    pResult->insert(pResult->end(), m_LineNumbers.begin(), m_LineNumbers.end());
    m_LineNumbers.clear();
    return true;
}

void LwDiagUtils::Preprocessor::ReduceWhitespaces(TokensIt tbegin, TokensIt tend, Tokens* pTokens)
{
    pTokens->clear();

    // Used for skipping conselwtive whitespace tokens,
    // also strips leading whitespaces.
    bool wasWhitespace = true;

    for ( ; tbegin != tend; ++tbegin)
    {
        if (!tbegin->IsWS())
        {
            wasWhitespace = false;
            pTokens->push_back(*tbegin);
        }
        else if (!wasWhitespace)
        {
            wasWhitespace = true;
            Token token = *tbegin; // preserve source location
            token.type  = TT_WHITESPACE;
            token.token = StringView(" ", 1);
            pTokens->push_back(token);
        }
    }

    // Strip trailing whitespace
    while (!pTokens->empty() && pTokens->back().IsWS())
    {
        pTokens->resize(pTokens->size() - 1);
    }
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::Concatenate
(
    Tokens*      pTokens,
    TokensIt*    pTok
) const
{
    TokensIt   tok = *pTok;
    TokenType  tt1 = TT_IDENTIFIER;
    TokenType  tt2 = TT_IDENTIFIER;
    StringView tok1;
    StringView tok2;

    // Find second arg to ## operator, skipping whitespaces
    TokensIt arg2 = tok;
    do
    {
        ++arg2;
    }
    while (arg2 != pTokens->end() && arg2->IsWS());
    const bool haveArg2 = arg2 != pTokens->end() && ! arg2->IsWS();
    if (haveArg2)
    {
        tt2  = arg2->type;
        tok2 = arg2->token;
    }
    else
    {
        arg2 = next(tok);
    }

    // Find first arg to ## operator, skipping whitespaces
    TokensIt arg1 = tok;
    if (arg1 != pTokens->begin())
    {
        --arg1;
    }
    while (arg1 != pTokens->begin() && arg1->IsWS())
    {
        --arg1;
    }
    const bool haveArg1 = arg1 != tok && ! arg1->IsWS();
    if (haveArg1)
    {
        tt1  = arg1->type;
        tok1 = arg1->token;
    }
    else
    {
        tt1 = tt2;
    }

    // Check if the token types are correct for concatenation
    if (tt1 != TT_IDENTIFIER && tt1 != TT_IDENTIFIER_EXPANDED && tt1 != TT_NUMBER && tt1 != TT_PUNCTUATOR)
    {
        PrintError(*arg1, "Invalid token for concatenation");
        return PREPROCESS_ERROR;
    }
    if (tt2 != TT_IDENTIFIER && tt2 != TT_IDENTIFIER_EXPANDED && tt2 != TT_NUMBER && tt2 != TT_PUNCTUATOR)
    {
        PrintError(*arg2, "Invalid token for concatenation");
        return PREPROCESS_ERROR;
    }

    // Note: We don't check if a valid token was formed.

    const Token newToken =
    {
        GenerateStringView() + tok1 + tok2,
        tt1,
        arg1->lineNum,
        arg1->column
    };

    // Point to the next token after the second arg - for deletion
    if (haveArg2)
    {
        ++arg2;
    }

    const size_t pos = arg1 - pTokens->begin();
    pTokens->erase(arg1, arg2);
    pTokens->insert(pTokens->begin() + pos, newToken);

    *pTok = pTokens->begin() + pos;
    return OK;
}

bool LwDiagUtils::Preprocessor::CollapseComments(TokensIt tbegin, TokensIt tend) const
{
    bool collapsed = false;
    for ( ; tbegin != tend; ++tbegin)
    {
        if (tbegin->type == TT_COMMENT)
        {
            const StringView oldToken = tbegin->token;
            auto             newToken = GenerateStringView();
            bool             hadSpace = false;

            for (const char c : oldToken)
            {
                switch (c)
                {
                    case '\r':
                    case '\n':
                        newToken += c;
                        hadSpace =  false;
                        break;

                    default:
                        if (!hadSpace)
                        {
                            newToken += ' ';
                            hadSpace =  true;
                        }
                        break;
                }
            }

            tbegin->token = newToken;
            collapsed     = true;
        }
    }
    return collapsed;
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::ExpandMacros
(
    Tokens*    pTokens,
    bool*      pDoneAnything,
    ExpandType expandType
)
{
    StringStack expandedMacros;
    return ExpandMacros(pTokens, pDoneAnything, expandType, expandedMacros);
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::ExpandMacros
(
    Tokens*      pTokens,
    bool*        pDoneAnything,
    ExpandType   expandType,
    StringStack& expandedMacros
)
{
    for (TokensIt tok = pTokens->begin(); tok != pTokens->end(); )
    {
        bool expanded = false;
        EC ec;
        CHECK_EC(ExpandOne(pTokens, &tok, &expanded, expandType, expandedMacros));

        if (expanded)
        {
            *pDoneAnything = true;
        }
        else
        {
            ++tok;
        }
    }
    return OK;
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::ExpandOne
(
    Tokens*      pTokens,
    TokensIt*    pTok,
    bool*        pExpanded,
    ExpandType   expandType,
    StringStack& expandedMacros
)
{
    TokensIt tok = *pTok;

    LWDASSERT(tok >= pTokens->begin());
    LWDASSERT(tok <  pTokens->end());

    *pExpanded = false;

    // Pitfall: we are doing concatenation even if no macros are being expanded.
    // This is not correct, but the ## operator is meaningless in the output JS
    // code anyway and would cause an error.
    if (tok->token == StringView("##", 2))
    {
        EC ec;
        CHECK_EC(Concatenate(pTokens, pTok));
        tok = *pTok;
    }

    if (tok->type != TT_IDENTIFIER)
    {
        return OK;
    }

    const StringView tokenStr = tok->token;

    // Note: We expand identifiers immediately following a string.
    // In C, these identifiers are treated as string literal suffixes
    // and are not expanded.

    auto macro = m_Macros.find(tok->token);
    if (macro == m_Macros.end())
    {
        return OK;
    }
    const BuiltinMacro builtin = macro->second.builtin;

    // Don't expand the first arg to the ## operator
    {
        TokensIt next = tok;
        do
        {
            ++next;
        }
        while (next != pTokens->end() && next->IsWS());
        if (next != pTokens->end() && next->token == StringView("##", 2))
        {
            return OK;
        }
    }

    // Only expand "defined" macro when handling #if
    if (builtin == BUILTIN_DEFINED && expandType != ExpandIf)
    {
        return OK;
    }

    // Prevent relwrsive expansion of the same macro
    if (expandedMacros.HasString(tok->token))
    {
        return OK;
    }

    Tokens   expanded = macro->second.tokens;
    TokensIt endToken = next(tok);

    PropagateLineNumbers(**pTok, expanded.begin(), expanded.end());

    // Function-like macro
    if (macro->second.hasArgs)
    {
        vector<Tokens> args;
        EC ec;
        bool ilwoked = false;
        CHECK_EC(FetchMacroArgs(pTokens, pTok, &endToken, expandType, &args, &ilwoked));
        tok = *pTok; // Update in case more lines were read

        // Don't expand the macro if it is not followed by arguments
        if (!ilwoked)
        {
            return OK;
        }

        if (args.empty() && macro->second.args.size() == 1)
        {
            args.resize(1);
        }

        if (args.size() != macro->second.args.size())
        {
            PrintError(*tok, "Incorrect number of arguments to a function-like macro");
            return PREPROCESS_ERROR;
        }

        // Pre-expand arguments
        for (const auto& macroArg : macro->second.args)
        {
            bool argIsExpanded = false;
            auto& arg = args[macroArg.second.index];
            if (macroArg.second.expand)
            {
                // When expanding args, behave like during #if expansion - don't allow
                // function-like macros to continue on next line.
                const ExpandType type = (expandType == ExpandFull) ? ExpandOneLine : expandType;
                CHECK_EC(ExpandMacros(&arg, &argIsExpanded, type, expandedMacros));
            }

            Tokens reduced;
            ReduceWhitespaces(arg.begin(), arg.end(), &reduced);
            arg = reduced;

            // Prevent re-expansion of the arguments
            if (argIsExpanded)
            {
                for (auto& argTok : arg)
                {
                    if (argTok.type == TT_IDENTIFIER)
                    {
                        argTok.type = TT_IDENTIFIER_EXPANDED;
                    }
                }
            }
        }

        // Now put the arguments in the macro expansion

        if (builtin == BUILTIN_DEFINED)
        {
            if (args[0].size() == 0)
            {
                PrintError(*tok, "Missing arguments to defined() macro");
                return PREPROCESS_ERROR;
            }
            if (args[0].size() > 1)
            {
                PrintError(*tok, "Too many arguments to defined() macro");
                return PREPROCESS_ERROR;
            }

            const Token& arg = args[0][0];

            const bool ok = (arg.type == TT_IDENTIFIER || arg.type == TT_IDENTIFIER_EXPANDED) &&
                m_Macros.find(arg.token) != m_Macros.end();

            const Token token =
            {
                StringView(ok ? "1" : "0", 1),
                TT_NUMBER,
                m_LwrLineNum,
                0
            };
            expanded.clear();
            expanded.push_back(token);
        }
        else
        {
            bool wasHash = false;
            TokensIt lwrTokIt = expanded.begin();
            while (lwrTokIt != expanded.end())
            {
                // Detect the # stringizing operator
                if (lwrTokIt->token == '#')
                {
                    wasHash = true;
                    ++lwrTokIt;
                    continue;
                }
                if (lwrTokIt->IsWS())
                {
                    ++lwrTokIt;
                    continue;
                }
                const bool stringize = wasHash;
                wasHash = false;

                if (lwrTokIt->type != TT_IDENTIFIER)
                {
                    ++lwrTokIt;
                    continue;
                }

                const auto argIt = macro->second.args.find(lwrTokIt->token);
                if (argIt == macro->second.args.end())
                {
                    ++lwrTokIt;
                    continue;
                }

                Tokens argReplacement = args[argIt->second.index];

                // The # stringizing operator
                if (lwrTokIt != expanded.begin() && stringize)
                {
                    auto newToken = GenerateStringView() + '"';
                    for (const auto& argToken : argReplacement)
                    {
                        for (const auto c : argToken.token)
                        {
                            if (c == '"' || c == '\'' || c == '\\')
                            {
                                newToken += '\\';
                            }
                            newToken += c;
                        }
                    }
                    newToken += '\"';

                    argReplacement.clear();

                    const Token token =
                    {
                        newToken,
                        TT_STRING,
                        m_LwrLineNum,
                        0
                    };
                    argReplacement.push_back(token);

                    TokensIt hashIt = lwrTokIt;
                    do
                    {
                        --hashIt;
                    }
                    while (hashIt->token != '#');
                    const size_t pos = hashIt - expanded.begin();
                    expanded.erase(hashIt, lwrTokIt);
                    lwrTokIt = expanded.begin() + pos;
                }

                const size_t pos = lwrTokIt - expanded.begin();
                expanded.erase(lwrTokIt);
                expanded.insert(expanded.begin()+pos, argReplacement.begin(), argReplacement.end());
                lwrTokIt = expanded.begin() + pos + argReplacement.size();
            }
        }
    }
    // Built-in macro (note: defined() is handled above)
    else if (builtin != NOT_BUILTIN)
    {
        Token token =
        {
            StringView(),
            TT_OTHER,
            m_LwrLineNum,
            0
        };
        switch (builtin)
        {
            case BUILTIN_FILE:
                token.token = GenerateStringView() + '"' + m_LwrFile + '"';
                token.type  = TT_STRING;
                break;

            case BUILTIN_LINE:
                token.token = GenerateStringView() + IntToStr(m_LwrLineNum);
                token.type  = TT_NUMBER;
                break;

            default:
                LWDASSERT(0);
                PrintError("Software bug!");
                return SOFTWARE_ERROR;
        }
        expanded.clear();
        expanded.push_back(token);
    }

    // Prevent relwrsive expansion of the same macro
    expandedMacros.Push(tokenStr);

    // Remove the token being expanded
    {
        const size_t tokPos = tok - pTokens->begin();
        pTokens->erase(tok, endToken);
        *pTok = pTokens->begin() + tokPos;
        tok = pTokens->end();
    }

    // Push the current state of the tokens vector in which we are
    // expanding a macro for the purpose of feeding FetchMacroArgs()
    {
        const TokensStackItem item = { *pTok, pTokens };
        m_TokensStack.push_back(item);
    }

    // Expand the macro relwrsively
    EC ec;
    CHECK_EC(ExpandMacros(&expanded, pExpanded, expandType, expandedMacros));

    expandedMacros.Pop();

    // The current token may have been updated due to FetchMacroArgs()
    // needing to pull in more tokens for macro parameters
    *pTok = m_TokensStack.back().tok;
    m_TokensStack.pop_back();

    // Insert the tokens resulting from the macro expansion
    const size_t tokPos = *pTok - pTokens->begin();
    pTokens->insert(pTokens->begin()+tokPos, expanded.begin(), expanded.end());
    *pTok = pTokens->begin() + tokPos + expanded.size();

    *pExpanded = true;

    return OK;
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::FetchMacroArgs
(
    Tokens*         pTokens,
    TokensIt*       pTok,
    TokensIt*       pEndTok,
    ExpandType      expandType,
    vector<Tokens>* pArgs,
    bool*           pIlwoked
)
{
    int parenLevel = 0;

    static const char s_Defined[] = "defined";
    const bool defined = (*pTok)->token == StringView(s_Defined, sizeof(s_Defined) - 1);
    LWDASSERT(!defined || expandType == ExpandIf);

    TokensIt tok          = next(*pTok);
    TokensIt argBeginTok  = tok;
    bool     haveFirstTok = false;
    for (;;)
    {
        if (tok == pTokens->end())
        {
            Tokens moreTokens;

            // Return early if there are non-whitespacetokens in
            // the lwrrently expanded tokens or current line and
            // we are still looking for an opening parenthesis, but
            // the first non-whitespace token is not a parenthesis.
            // Otherwise we will jumble the order of incoming tokens.
            if (parenLevel == 0)
            {
                for (size_t level = m_TokensStack.size(); level > 0; --level)
                {
                    TokensStackItem& tokens = m_TokensStack[level-1];
                    for (TokensIt t = tokens.tok; t != tokens.pTokens->end(); ++t)
                    {
                        if (!t->IsWS())
                        {
                            // If it's not an open parenthesis, terminate early
                            if (t->token != '(')
                            {
                                return OK;
                            }

                            level = 1; // Exit the outer loop prematurely
                            break;
                        }
                    }
                }
            }

            // Fetch another token from the lwrrently expanded tokens or
            // current line
            bool needMoreLines = true;
            for (size_t level = m_TokensStack.size(); level > 0; --level)
            {
                TokensStackItem& tokens = m_TokensStack[level-1];
                Tokens* const pSrc = tokens.pTokens;
                if (tokens.tok < pSrc->end())
                {
                    moreTokens.push_back(*tokens.tok);
                    needMoreLines = false;

                    const size_t pos = tokens.tok - pSrc->begin();
                    pSrc->erase(pSrc->begin() + pos);
                    tokens.tok = pSrc->begin() + pos;
                    break;
                }
            }

            // No more tokens in the current line, fetch next line
            if (needMoreLines)
            {
                // When expanding function-like macros for a #if directive
                // or function-like macro arguments,
                // the function-like macro invocation must end on the same
                // line -- line breaks are not allowed.
                if (expandType != ExpandFull)
                {
                    if (parenLevel == 0)
                    {
                        // No arguments specified, will not expand
                        return OK;
                    }
                    else
                    {
                        PrintError(*tok, "Missing closing parenthesis for a function-like macro");
                        return PREPROCESS_ERROR;
                    }
                }

                // Note: This implementation does not support preprocessor directives
                // in the middle of macro arguments, such as:
                // #define FUN(x) ((x)+1)
                // FUN(
                //     #if SOME_SWITCH
                //         1
                //     #else
                //         2
                //     #endif
                // )

                EC ec;
                CHECK_EC(GetNextLine(&moreTokens, nullptr, nullptr));
            }

            // Closing parenthesis still not found
            if (moreTokens.empty())
            {
                PrintError(*tok, "Missing closing parenthesis for a function-like macro");
                return PREPROCESS_ERROR;
            }

            const size_t begin    = *pTok - pTokens->begin();
            const size_t argBegin = argBeginTok - pTokens->begin();
            const size_t lwr      = tok - pTokens->begin();

            pTokens->insert(pTokens->end(), moreTokens.begin(), moreTokens.end());

            *pTok       = pTokens->begin() + begin;
            argBeginTok = pTokens->begin() + argBegin;
            tok         = pTokens->begin() + lwr;
            continue; // avoid ++tok
        }
        else if (tok->token == '(')
        {
            if (!haveFirstTok && parenLevel > 0)
            {
                argBeginTok  = tok;
                haveFirstTok = true;
            }
            ++parenLevel;
        }
        else if (tok->token == ')' && parenLevel > 1)
        {
            --parenLevel;
        }
        else if (parenLevel == 1 && (tok->token == ')' || tok->token == ','))
        {
            if (haveFirstTok || !pArgs->empty() || tok->token == ',')
            {
                pArgs->push_back(Tokens());
                if (haveFirstTok)
                {
                    pArgs->back().insert(pArgs->back().end(), argBeginTok, tok);
                }
            }
            haveFirstTok = false;
            if (tok->token == ')')
            {
                break;
            }
        }
        else if (!tok->IsWS()) // Only use the first non-whitespace token
        {
            if (!haveFirstTok)
            {
                if (parenLevel == 0)
                {
                    // defined doesn't require parentheses for its argument
                    if (defined)
                    {
                        pArgs->push_back(Tokens());
                        pArgs->back().push_back(*tok);
                        *pEndTok  = next(tok);
                        *pIlwoked = true;
                        return OK;
                    }

                    // No arguments specified, will not expand
                    return OK;
                }
                argBeginTok  = tok;
                haveFirstTok = true;
            }
        }
        ++tok;
    }

    LWDASSERT(tok < pTokens->end());
    LWDASSERT(tok->token == ')');
    *pEndTok  = next(tok);
    *pIlwoked = true;

    return OK;
}

bool LwDiagUtils::Preprocessor::MarkNonExpand(Arguments* pArgs, const Token& token)
{
    if (token.type != TT_IDENTIFIER)
    {
        return false;
    }

    const auto argIt = pArgs->find(token.token);
    if (argIt == pArgs->end())
    {
        return false;
    }

    argIt->second.expand = false;
    return true;
}

void LwDiagUtils::Preprocessor::PreventNewTokens(Tokens* pTokens)
{
    for (size_t i=0; i < pTokens->size(); i++)
    {
        if ((*pTokens)[i].token.size() == 0)
        {
            pTokens->erase(pTokens->begin() + i);
            i--;
        }
    }

    for (size_t i=1; i < pTokens->size(); i++)
    {
        const Token& tok1 = (*pTokens)[i-1];
        const Token& tok2 = (*pTokens)[i];

        const TokenType t1 = tok1.type;
        const TokenType t2 = tok2.type;

        bool insertSpace = false;

        if ((t1 == TT_IDENTIFIER || t1 == TT_IDENTIFIER_EXPANDED || t1 == TT_NUMBER) &&
            (t2 == TT_IDENTIFIER || t2 == TT_IDENTIFIER_EXPANDED || t2 == TT_NUMBER))
        {
            insertSpace = true;
        }
        else if (tok1.token[tok1.token.size()-1] == '.' && t2 == TT_NUMBER)
        {
            insertSpace = true;
        }
        else if (t1 == TT_NUMBER && (tok2.token[0] == '+' || tok2.token[0] == '-' || tok2.token[0] == '.'))
        {
            insertSpace = true;
        }
        else if (tok1.token == '/' && (tok2.token[0] == '/' || tok2.token[0] == '*'))
        {
            insertSpace = true;
        }
        else if (t1 == TT_PUNCTUATOR && t2 == TT_PUNCTUATOR)
        {
            if (FindPunctuator(tok1.token, tok2.token[0]))
            {
                insertSpace = true;
            }
        }

        if (insertSpace)
        {
            Token token;
            token.type    = TT_WHITESPACE;
            token.token   = StringView(" ", 1);
            token.lineNum = -1;
            token.column  = 0;

            pTokens->insert(pTokens->begin() + i, token);
        }
    }
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::CheckIfAllSpaces(TokensIt tbegin, TokensIt tend) const
{
    for ( ; tbegin != tend; ++tbegin)
    {
        if (!tbegin->IsWS())
        {
            PrintError(*tbegin, "Unexpected trailing characters");
            return PREPROCESS_ERROR;
        }
    }

    return OK;
}

int LwDiagUtils::Preprocessor::CountEOLs(const char* begin, const char* end)
{
    int numEOLs = 0;
    for (const char* s = begin; s != end; s++)
    {
        switch (*s)
        {
            case '\r':
                if (next(s) != end && s[1] == '\n')
                {
                    ++s;
                }
                // fall-through

            case '\n':
                ++numEOLs;
                break;

            default:
                break;
        }
    }
    return numEOLs;
}

void LwDiagUtils::Preprocessor::AddBuiltinMacro(const char* name, BuiltinMacro builtin, bool hasArgs)
{
    Macro macro;
    macro.builtin = builtin;
    macro.hasArgs = hasArgs;

    if (hasArgs)
    {
        Argument arg;
        arg.index  = 0;
        arg.expand = false;

        macro.args[StringView("dummy", 5)] = arg; // Don't really need arg name here, use "dummy"
    }

    m_Macros[GenerateStringView() + name] = macro;
}

void LwDiagUtils::Preprocessor::EmitLineNumber(LineNumberFlags flags)
{
    if (m_LineCommandMode == LineCommandNone)
    {
        return;
    }

    LWDASSERT(!m_Files.empty());

    string lineCommand;
    switch (m_LineCommandMode)
    {
        case LineCommandHash:    lineCommand = "#line ";    break;
        case LineCommandComment: lineCommand = "//line ";   break;
        case LineCommandAt:      lineCommand = "//@line ";  break;
        default:                 LWDASSERT(0);
    }

    const string strLineNum = IntToStr(m_Files.back().lineNum);

    string strFlags;
    if (flags != StartFlag && m_LineCommandMode == LineCommandHash)
    {
        strFlags = " " + IntToStr(static_cast<int>(flags));
    }

    m_LineNumbers += lineCommand + strLineNum + " \"" + m_Files.back().path + "\"" + strFlags + "\n";
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::Eval(TokensIt tbegin, TokensIt tend, bool* pTruthy)
{
    // Expand macros
    Tokens tokens;
    tokens.assign(tbegin, tend);
    bool doneAnything = false;
    EC ec;
    CHECK_EC(ExpandMacros(&tokens, &doneAnything, ExpandIf));
    tbegin = tokens.begin();
    tend   = tokens.end();

    // Evaluation of expressions in the preprocessor is very simple:
    // - integer numbers
    // - character literals are treated as integers (the same as in C)
    // - all numbers are 64-bit signed
    // - unexpanded identifiers evaluate to 0
    // - the same operator precedence rules apply as in C
    // - everything else (strings, array operator, pointers) causes preprocessing errors

    INT64 value = 0;
    CHECK_EC(EvalSub(tbegin, tend, &value));

    *pTruthy = value != 0;

    return OK;
}

// Ideally we could put this structure inside EvalSub, but then we will get
// warnings: "template argument uses local type".
struct LwDiagUtils::Preprocessor::NODE
{
    TokensIt          token;
    INT64             value;
    const Punctuator* op;
    bool              unary;
};

LwDiagUtils::EC LwDiagUtils::Preprocessor::EvalSub
(
    TokensIt tbegin,
    TokensIt tend,
    INT64*   pValue
)
{
    // Local helper functions
    struct Local
    {
        static void AddNode(vector<NODE>* pNodes, TokensIt token, INT64 value)
        {
            const NODE node = { token, value, nullptr, false };
            pNodes->push_back(node);
        }

        static void AddNode(vector<NODE>* pNodes, TokensIt token, const Punctuator& op)
        {
            const bool unary = op.type == PT_NOT         ||
                               op.type == PT_LOGICAL_NOT ||
                               op.type == PT_PAREN_OPEN;
            const NODE node = { token, 0, &op, unary };
            pNodes->push_back(node);
        }

        static void EvalUnary(vector<NODE>& nstack)
        {
            while (nstack.size() > 1                    &&
                   nstack[nstack.size()-2].unary &&
                   nstack[nstack.size()-2].op->type != PT_PAREN_OPEN)
            {
                const NODE& op = nstack[nstack.size()-2];
                switch (op.op->type)
                {
                    case PT_ADD:                                                            break;
                    case PT_SUB:         nstack.back().value = -nstack.back().value;        break;
                    case PT_NOT:         nstack.back().value = ~nstack.back().value;        break;
                    case PT_LOGICAL_NOT: nstack.back().value = nstack.back().value ? 0 : 1; break;

                    default:
                        LWDASSERT(0);
                        break;
                }

                nstack.erase(nstack.begin() + nstack.size() - 2);
            }
        }

        static void EvalBinary(vector<NODE>& nstack, int maxPrecedence)
        {
            int precedence = 0;
            while (nstack.size() > 2 &&
                   (!nstack[nstack.size()-2].op ||
                    nstack[nstack.size()-2].op->type != PT_PAREN_OPEN))
            {
                NODE&       a  = nstack[nstack.size()-3];
                const NODE& op = nstack[nstack.size()-2];
                const NODE& b  = nstack.back();

                LWDASSERT(!a.op);
                LWDASSERT(op.op);
                LWDASSERT(!op.unary);
                LWDASSERT(!b.op);

                // Limit evaluation when adding a new operator.
                // For example: a && b + c == d
                // On the stack we have: [ a && b + c ]
                // When == is processed, we need to evaluate + but not &&
                // because && has a higher precedence number than ==.
                if (op.op->precedence > maxPrecedence)
                {
                    break;
                }

                // Stop collapsing/evaluating operators when we encounter
                // an operator with a the same precedence as the previous one.
                // This only happens for the ternary operator.
                // Other operators have already been collapsed.
                if (op.op->precedence == precedence && precedence == 13)
                {
                    break;
                }

                // Operators with a higher or equal precedence than the current one
                // have already been collapsed when pushing this operator onto
                // the stack.
                // Note: a conceptually "higher" precedence means an actually
                // lower precedence number!
                LWDASSERT(op.op->precedence >= precedence);

                precedence = op.op->precedence;

                switch (op.op->type)
                {
                    // Note: no overflow detection
                    case PT_ADD:         a.value +=  b.value;                    break;
                    case PT_SUB:         a.value -=  b.value;                    break;
                    case PT_MUL:         a.value *=  b.value;                    break;
                    case PT_DIV:         a.value /=  b.value;                    break;
                    case PT_MOD:         a.value %=  b.value;                    break;
                    case PT_AND:         a.value &=  b.value;                    break;
                    case PT_OR:          a.value |=  b.value;                    break;
                    case PT_XOR:         a.value ^=  b.value;                    break;
                    case PT_SHL:         a.value <<= b.value;                    break;
                    case PT_SHR:         a.value >>= b.value;                    break;
                    case PT_EQ:          a.value = (a.value == b.value) ? 1 : 0; break;
                    case PT_NE:          a.value = (a.value != b.value) ? 1 : 0; break;
                    case PT_LT:          a.value = (a.value <  b.value) ? 1 : 0; break;
                    case PT_LE:          a.value = (a.value <= b.value) ? 1 : 0; break;
                    case PT_GT:          a.value = (a.value >  b.value) ? 1 : 0; break;
                    case PT_GE:          a.value = (a.value >= b.value) ? 1 : 0; break;
                    case PT_LOGICAL_AND: a.value = (a.value && b.value) ? 1 : 0; break;
                    case PT_LOGICAL_OR:  a.value = (a.value || b.value) ? 1 : 0; break;

                    case PT_TERNARY:
                        // Don't execute '?' on its own, wait for ':'
                        return;

                    case PT_COLON:
                        // There are three possibilities for ternary operator oclwrence.
                        // The diagrams below represent the contents of the node stack.
                        // - single ?:
                        // -- [ a ? b : c ]
                        // - multiple ?: chained in one of two ways:
                        // -- [ a ? b ? c : d : e ]
                        // -- [ a ? b : c ? d : e ]
                        //
                        // Please note that all unary and binary operators have a lower
                        // precedence level than ?: so they have been collapsed already.
                        //
                        // A ? is never processed on its own.  The first : encountered
                        // will also be placed on the stack and not exelwted immediately,
                        // so that chaining can be processed correctly.
                        //
                        // 1) For a single ?: ternary operator, the ? will be skipped
                        //    and the : will be processed here.
                        //
                        // 2) For the sequence [ a ? b ? c : d : e ]
                        //    - The ?s will be skipped, so they will be on the stack.
                        //    - The first colon will be placed on the stack.
                        //    - The second colon will cause the first colon to be exelwted
                        //      before the second colon is placed on the stack.
                        //      This will collapse the second ? too, so the stack
                        //      will contain: [ a ? x : e ] where is is the collapsed
                        //      "b ? c : d".
                        //    - Eventually the second colon will be exelwted when
                        //      a closing parenthesis or end of expression (or another
                        //      colon) is encountered.
                        //
                        // 3) For the sequence [ a ? b : c ? d : e ]
                        //    - The first ? will just be placed on the stack.
                        //    - The first : will also just be placed on the stack,
                        //      because the ? is not processed on its own.
                        //    - The second ? will also be placed on the stack,
                        //      because we don't ilwoke EvalBinary() when we encounter
                        //      a ? (see the invocation of EvalBinary() for operators).
                        //    - The second : will also be placed on the stack,
                        //      because the invocation of EvalBinary() will exit
                        //      when it encounters the second (sole) ?.
                        //    - Finally, when a closing parenthesis or end of expression
                        //      is encountered, the colons will be collapsed from
                        //      right to left, iteratively.  Normally we don't collapse
                        //      subsequent operators with the same precedence level,
                        //      except for colons - see the --precedence line below.

                        if (nstack.size() < 5)
                        {
                            return;
                        }
                        {
                            NODE&       cond         = nstack[nstack.size()-5];
                            const NODE& questionMark = nstack[nstack.size()-4];
                            if (!questionMark.op || questionMark.op->type != PT_TERNARY)
                            {
                                return;
                            }
                            LWDASSERT(!cond.op);
                            cond.value = cond.value ? a.value : b.value;
                            nstack.resize(nstack.size() - 2);
                            --precedence; // allow handling a chained ?:
                        }
                        break;

                    default:
                        LWDASSERT(0);
                        break;
                }

                nstack.resize(nstack.size() - 2);
            }
        }
    };

    // Colwert all tokens to either numbers or operators
    vector<NODE> nodes;
    for ( ; tbegin != tend; ++tbegin)
    {
        switch (tbegin->type)
        {
            case TT_PUNCTUATOR:
                {
                    const auto op = FindPunctuator(tbegin->token);
                    LWDASSERT(op != nullptr);
                    if (op->type == PT_ILLEGAL)
                    {
                        PrintError(*tbegin, "Invalid preprocessor operator");
                        return PREPROCESS_ERROR;
                    }
                    Local::AddNode(&nodes, tbegin, *op);
                }
                break;

            case TT_IDENTIFIER:
            case TT_IDENTIFIER_EXPANDED:
                // Unexpanded identifiers evaluate to 0
                Local::AddNode(&nodes, tbegin, 0);
                break;

            case TT_STRING:
                {
                    const StringView& token = tbegin->token;

                    LWDASSERT(token.size() > 1);

                    if (token[0] != '\'')
                    {
                        PrintError(*tbegin, "Invalid literal in a preprocessor expression");
                        return PREPROCESS_ERROR;
                    }

                    // No support for escape sequences
                    if (token.size() != 3)
                    {
                        PrintError(*tbegin, "Unsupported character literal in a preprocessor expression");
                        return PREPROCESS_ERROR;
                    }

                    Local::AddNode(&nodes, tbegin, token[1]);
                }
                break;

            case TT_NUMBER:
                {
                    const StringView& token = tbegin->token;

                    // No support for octal
                    const bool hex = token.size() > 2 &&
                                     token[0] == '0'  &&
                                     (token[1] == 'x' || token[1] == 'X');

                    for (size_t i=hex?2:0; i < token.size(); i++)
                    {
                        const char c = token[i];
                        if (c >= '0' && c <= '9')
                            continue;
                        if (hex && c >= 'A' && c <= 'F')
                            continue;
                        if (hex && c >= 'a' && c <= 'f')
                            continue;
                        PrintError(*tbegin, "Invalid numeric literal in a preprocessor expression");
                        return PREPROCESS_ERROR;
                    }

                    INT64 value;
                    int   ret;

                    if (token.size() > 17)
                        ret = 0;
                    else
                    {
                        char buf[20];
                        memcpy(buf, token.begin(), token.size());
                        buf[token.size()] = 0;

                        if (hex)
                            ret = sscanf(buf, "%" SCAN64 "x", &value);
                        else
                            ret = sscanf(buf, "%" SCAN64 "d", &value);
                    }

                    if (ret != 1)
                    {
                        PrintError(*tbegin, "Invalid numeric literal in a preprocessor expression");
                        return PREPROCESS_ERROR;
                    }

                    Local::AddNode(&nodes, tbegin, value);
                }
                break;

            case TT_EOL:
            case TT_COMMENT:
            case TT_WHITESPACE:
                // Ignore whitespaces
                break;

            default:
                PrintError(*tbegin, "Invalid literal in a preprocessor expression");
                return PREPROCESS_ERROR;
        }
    }

    const int evalAll = 1000; // precedence lower than all operators, causes all operators to evaluate

    // Evaluate the expression
    vector<NODE> nodeStack;
    int          parenLevel = 0;
    for (const auto& node : nodes)
    {
        const bool needOp = !nodeStack.empty() && !nodeStack.back().op;

        // Handle operators
        if (node.op)
        {
            if (needOp)
            {
                if (node.unary)
                {
                    PrintError(*node.token, "Operator expected");
                    return PREPROCESS_ERROR;
                }
                if (node.op->type == PT_PAREN_CLOSE)
                {
                    if (parenLevel == 0)
                    {
                        PrintError(*node.token, "Unexpected ')'");
                        return PREPROCESS_ERROR;
                    }
                    --parenLevel;

                    Local::EvalBinary(nodeStack, evalAll);

                    LWDASSERT(nodeStack.size() > 1);

                    const NODE& op = nodeStack[nodeStack.size()-2];
                    LWDASSERT(op.op);

                    if (op.op->type == PT_TERNARY || op.op->type == PT_COLON)
                    {
                        PrintError(*op.token, "Incomplete ternary operator");
                        return PREPROCESS_ERROR;
                    }
                    LWDASSERT(op.op->type == PT_PAREN_OPEN);

                    nodeStack.erase(nodeStack.begin() + nodeStack.size() - 2);

                    Local::EvalUnary(nodeStack);
                }
                else
                {
                    // If the previous operator has a higher or equal precedence,
                    // execute it first.
                    // Note: a conceptually "higher" precedence means an actually
                    // lower precedence number!
                    if (nodeStack.size() > 2)
                    {
                        const NODE& prevOp = nodeStack[nodeStack.size() - 2];
                        LWDASSERT(prevOp.op);
                        if (prevOp.op->precedence <= node.op->precedence &&
                            // Special handling for ternary operator
                            (node.op->precedence < 13   ||
                             prevOp.op->precedence < 13 ||
                             node.op->type == PT_COLON))
                        {
                            Local::EvalBinary(nodeStack, node.op->precedence);
                        }
                    }

                    nodeStack.push_back(node);
                }
            }
            else // Unary operator
            {
                if (!node.unary && node.op->type != PT_ADD && node.op->type != PT_SUB)
                {
                    PrintError(*node.token, "Value expected");
                    return PREPROCESS_ERROR;
                }
                nodeStack.push_back(node);
                nodeStack.back().unary = true; // Recognize + and - as unary
                if (node.op->type == PT_PAREN_OPEN)
                {
                    ++parenLevel;
                }
            }
        }
        // Handle values
        else
        {
            if (needOp)
            {
                PrintError(*node.token, "Operator expected");
                return PREPROCESS_ERROR;
            }

            nodeStack.push_back(node);

            Local::EvalUnary(nodeStack);
        }
    }

    Local::EvalBinary(nodeStack, evalAll);

    if (nodeStack.empty())
    {
        PrintError("Missing expression");
        return PREPROCESS_ERROR;
    }

    if (nodeStack.back().op)
    {
        PrintError(*nodeStack.back().token, "Value expected");
        return PREPROCESS_ERROR;
    }

    if (nodeStack.size() > 1)
    {
        const NODE& op = nodeStack[nodeStack.size()-2];
        LWDASSERT(op.op);
        if (op.op->type == PT_PAREN_OPEN)
        {
            PrintError(*op.token, "Unmatched opening parenthesis");
        }
        else if (op.op->type == PT_TERNARY || op.op->type == PT_COLON)
        {
            PrintError(*op.token, "Incomplete ternary operator");
        }
        else
        {
            LWDASSERT(0);
        }
        return PREPROCESS_ERROR;
    }

    *pValue = nodeStack.back().value;
    return OK;
}

void LwDiagUtils::Preprocessor::PropagateLineNumbers(const Token& token, TokensIt tbegin, TokensIt tend)
{
    for ( ; tbegin != tend; ++tbegin)
    {
        tbegin->lineNum = token.lineNum;
        tbegin->column  = token.column;
    }
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::DirectiveDefine(TokensIt tbegin, TokensIt tend)
{
    if (m_DisabledCode)
    {
        return OK;
    }

    if (tbegin == tend)
    {
        PrintError("Missing macro name");
        return PREPROCESS_ERROR;
    }
    if (tbegin->type != TT_IDENTIFIER)
    {
        PrintError(*tbegin, "Missing macro name");
        return PREPROCESS_ERROR;
    }

    const TokensIt macroToken = tbegin;

    // For built-in macros other than "defined" we could allow redefinition
    // and just issue a warning.
    // However we are treating redefinition of built-in macros as a hard error.
    auto existingMacro = m_Macros.find(macroToken->token);
    if (existingMacro != m_Macros.end() && existingMacro->second.builtin != NOT_BUILTIN)
    {
        PrintError(*tbegin, "Redefinition of built-in macro");
        return PREPROCESS_ERROR;
    }

    // Silently ignore macro redefinition, just overwrite the existing macro
    {
        Macro empty;
        empty.builtin = NOT_BUILTIN;
        empty.hasArgs = false;

        m_Macros[macroToken->token] = empty;
    }
    Macro& macro = m_Macros[macroToken->token];

    ++tbegin;

    // The '(' must immediately follow the macro name in order
    // for the macro to be treated as a function macro.
    macro.hasArgs = (tbegin != tend) && (tbegin->token == '(');
    int numArgs = 0;

    tbegin = find_if(tbegin, tend, Token::IsNonWS);

    if (tbegin != tend)
    {
        Tokens reduced;
        ReduceWhitespaces(tbegin, tend, &reduced);

        tbegin = reduced.begin();
        tend   = reduced.end();

        if (macro.hasArgs)
        {
            ++tbegin;
            while (tbegin != tend && tbegin->token != ')')
            {
                if (tbegin->IsWS())
                {
                    ++tbegin;
                    continue;
                }

                // Variadic macros are not supported
                if (tbegin->type != TT_IDENTIFIER)
                {
                    PrintError(*tbegin, "Invalid macro argument");
                    return PREPROCESS_ERROR;
                }

                if (macro.args.find(tbegin->token) != macro.args.end())
                {
                    PrintError(*tbegin, "Duplicate macro argument name");
                    return PREPROCESS_ERROR;
                }

                Argument arg;
                arg.index  = numArgs;
                arg.expand = true;

                macro.args[tbegin->token] = arg;
                ++numArgs;

                tbegin = find_if(next(tbegin), tend, Token::IsNonWS);

                if (tbegin == tend)
                {
                    PrintError("Incomplete macro definition");
                    return PREPROCESS_ERROR;
                }

                if (tbegin->token == ',')
                {
                    tbegin = find_if(next(tbegin), tend, Token::IsNonWS);
                }
                else if (tbegin->token != ')')
                {
                    PrintError(*tbegin, "Missing ')'");
                    return PREPROCESS_ERROR;
                }
            }

            if (tbegin == tend)
            {
                PrintError("Missing ')'");
                return PREPROCESS_ERROR;
            }

            ++tbegin;
        }

        // Replace remaining line breaks in strings with \r and \n
        for (TokensIt tok = tbegin; tok != tend; ++tok)
        {
            if (tok->type == TT_STRING)
            {
                int prevPos = 0;
                int pos     = FindEol(tok->token, prevPos);
                if (pos >= 0)
                {
                    auto genToken = GenerateStringView();
                    do
                    {
                        if (pos > prevPos)
                        {
                            genToken += StringView(tok->token.begin() + prevPos,
                                                   static_cast<size_t>(pos - prevPos));
                        }
                        genToken += '\\';
                        genToken += (tok->token[pos] == '\r') ? 'r' : 'n';
                        prevPos  = pos +1;
                        pos      = FindEol(tok->token, prevPos);
                    } while (pos >= 0);

                    pos = static_cast<int>(tok->token.size());
                    if (pos > prevPos)
                    {
                        genToken += StringView(tok->token.begin() + prevPos,
                                               static_cast<size_t>(pos - prevPos));
                    }

                    tok->token = genToken;
                }
            }
        }

        // Normally, macro parameters are pre-expanded before expanding the
        // macro.  However, macro parameters, which participate in
        // concatenation or stringification, are not pre-expanded.
        // Here, mark all such parameters, to prevent them from being pre-expanded.
        for (TokensIt tok = tbegin; tok != tend; ++tok)
        {
            if (tok->type != TT_PUNCTUATOR || tok->token[0] != '#')
            {
                continue;
            }

            if (tok->token == '#')
            {
                do
                {
                    ++tok;
                }
                while (tok != tend && tok->IsWS());
                if (tok == tend)
                {
                    PrintError("Missing macro argument for stringification");
                    return PREPROCESS_ERROR;
                }
                if (!MarkNonExpand(&macro.args, *tok))
                {
                    PrintError(*tok, "Macro argument required for stringification");
                    return PREPROCESS_ERROR;
                }
            }
            else if (tok->token == StringView("##", 2))
            {
                TokensIt next = tok;
                do
                {
                    ++next;
                }
                while (next != tend && next->IsWS());

                TokensIt prev = tok == tbegin ? tok : std::prev(tok);
                while (prev != tbegin && prev->IsWS()) --prev;
                if (prev->IsWS() || next == tend || next->token == StringView("##", 2))
                {
                    PrintError(*tok, "Invalid use of macro concatenation operator");
                    return PREPROCESS_ERROR;
                }
                MarkNonExpand(&macro.args, *prev);
                MarkNonExpand(&macro.args, *next);
            }
            else
            {
                LWDASSERT(0);
                return SOFTWARE_ERROR;
            }
        }

        tbegin = find_if(tbegin, tend, Token::IsNonWS);

        macro.tokens.insert(macro.tokens.end(), tbegin, tend);
    }

    return OK;
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::DirectiveElif(TokensIt tbegin, TokensIt tend)
{
    if (m_HashIfStack.empty())
    {
        PrintError("Unexpected #elif directive");
        return PREPROCESS_ERROR;
    }

    const IfDirectiveState state = m_HashIfStack.back();

    if (!state.enabled || state.active)
    {
        m_DisabledCode = true;
        return OK;
    }

    bool truthy = false;
    EC ec;
    CHECK_EC(Eval(tbegin, tend, &truthy));

    if (truthy)
    {
        m_HashIfStack.back().active = true;
        m_DisabledCode = false;
    }

    return OK;
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::DirectiveElse(TokensIt tbegin, TokensIt tend)
{
    if (m_HashIfStack.empty())
    {
        PrintError("Unexpected #else directive");
        return PREPROCESS_ERROR;
    }

    EC ec;
    CHECK_EC(CheckIfAllSpaces(tbegin, tend));

    const IfDirectiveState state = m_HashIfStack.back();

    if (state.enabled)
    {
        m_DisabledCode = state.active;
    }

    return OK;
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::DirectiveEndif(TokensIt tbegin, TokensIt tend)
{
    if (m_HashIfStack.empty())
    {
        PrintError("Unmatched #endif directive");
        return PREPROCESS_ERROR;
    }

    EC ec;
    CHECK_EC(CheckIfAllSpaces(tbegin, tend));

    const IfDirectiveState state = m_HashIfStack.back();
    m_HashIfStack.pop_back();

    m_DisabledCode = ! state.enabled;

    return OK;
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::DirectiveError(TokensIt tbegin, TokensIt tend)
{
    TokensIt msgPos = tbegin;
    string message = "#error ";
    for ( ; tbegin != tend; ++tbegin)
    {
        if (tbegin->type != TT_EOL)
        {
            message += string(tbegin->token.begin(), tbegin->token.size());
        }
    }
    PrintError(*msgPos, message);
    return PREPROCESS_ERROR;
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::DirectiveIf(TokensIt tbegin, TokensIt tend)
{
    if (m_DisabledCode)
    {
        IfDirectiveState state;
        state.enabled = false;
        state.active  = false;

        m_HashIfStack.push_back(state);
        return OK;
    }

    bool truthy = false;
    EC ec;
    CHECK_EC(Eval(tbegin, tend, &truthy));

    IfDirectiveState state;
    state.enabled = true;
    state.active  = truthy;

    m_HashIfStack.push_back(state);
    m_DisabledCode = ! truthy;

    return OK;
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::DirectiveIfDef(TokensIt tbegin, TokensIt tend)
{
    return DirectiveIfDef(tbegin, tend, true);
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::DirectiveIfNDef(TokensIt tbegin, TokensIt tend)
{
    return DirectiveIfDef(tbegin, tend, false);
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::DirectiveIfDef(TokensIt tbegin, TokensIt tend, bool defined)
{
    if (m_DisabledCode)
    {
        IfDirectiveState state;
        state.enabled = false;
        state.active  = false;

        m_HashIfStack.push_back(state);
        return OK;
    }

    if (tbegin == tend)
    {
        PrintError("Missing macro name");
        return PREPROCESS_ERROR;
    }
    if (tbegin->type != TT_IDENTIFIER)
    {
        PrintError(*tbegin, "Missing macro name");
    }

    const bool hasMacro = m_Macros.find(tbegin->token) != m_Macros.end();

    EC ec;
    CHECK_EC(CheckIfAllSpaces(next(tbegin), tend));

    const bool enable = hasMacro == defined;

    IfDirectiveState state;
    state.enabled = true;
    state.active  = enable;

    m_HashIfStack.push_back(state);
    m_DisabledCode = ! enable;

    return OK;
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::DirectiveInclude(TokensIt tbegin, TokensIt tend)
{
    if (m_DisabledCode)
    {
        return OK;
    }

    if (tbegin == tend)
    {
        PrintError("Missing include file name");
        return PREPROCESS_ERROR;
    }

    Tokens tokens;
    if (tbegin->type != TT_STRING && tbegin->token != '<')
    {
        tokens.assign(tbegin, tend);
        bool doneAnything = false;
        EC ec;
        CHECK_EC(ExpandMacros(&tokens, &doneAnything, ExpandOneLine));
        tbegin = tokens.begin();
        tend   = tokens.end();
    }

    string filename;

    if (tbegin->type == TT_STRING)
    {
        if (tbegin->token.size() >= 2)
        {
            filename = string(tbegin->token.begin() + 1, tbegin->token.size() - 2);
        }

        EC ec;
        CHECK_EC(CheckIfAllSpaces(next(tbegin), tend));
    }
    else if (tbegin->token == '<')
    {
        // Collapse everything until ">" to a string
        for (++tbegin; (tbegin != tend) && (tbegin->token != '>'); ++tbegin)
        {
            filename += string(tbegin->token.begin(), tbegin->token.size());
        }
        if (tbegin == tend)
        {
            PrintError("Invalid include file name");
        }
        else if (tbegin->token != '>')
        {
            PrintError(*tbegin, "Invalid include file name, '>' expected");
        }
        else
        {
            EC ec;
            CHECK_EC(CheckIfAllSpaces(next(tbegin), tend));
        }
    }
    else
    {
        PrintError("Missing include file name");
        return PREPROCESS_ERROR;
    }

    return PushFile(filename);
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::DirectivePragma(TokensIt tbegin, TokensIt tend)
{
    if (m_DisabledCode)
    {
        return OK;
    }

    if (tbegin == tend)
    {
        PrintError("Missing pragma type");
        return PREPROCESS_ERROR;
    }
    if (tbegin->type != TT_IDENTIFIER)
    {
        PrintError(*tbegin, "Missing pragma type");
        return PREPROCESS_ERROR;
    }

    // Silently ignore any pragma but "#pragma mods"
    if (tbegin->token != StringView("mods", 4))
        return OK;

    tbegin = find_if(next(tbegin), tend, Token::IsNonWS);
    if (tbegin == tend)
    {
        PrintError("Missing mods pragma type");
        return PREPROCESS_ERROR;
    }
    if (tbegin->type != TT_IDENTIFIER)
    {
        PrintError(*tbegin, "Missing mods pragma type");
        return PREPROCESS_ERROR;
    }
    if (tbegin->token != StringView("strict", 6))
    {
        PrintError(*tbegin, "Unknown mods pragma type");
        return PREPROCESS_ERROR;
    }

    tbegin = find_if(next(tbegin), tend, Token::IsNonWS);
    if (tbegin == tend)
    {
        PrintError("Missing strict mode");
        return PREPROCESS_ERROR;
    }
    if (tbegin->type != TT_IDENTIFIER)
    {
        PrintError(*tbegin, "Missing strict mode");
        return PREPROCESS_ERROR;
    }

    StrictMode strictMode;
    if (tbegin->token == StringView("off", 3))
        strictMode = StrictModeOff;
    else if (tbegin->token == StringView("on", 2))
        strictMode = StrictModeOn;
    else if (tbegin->token == StringView("engr", 4))
        strictMode = StrictModeEngr;
    else
    {
        PrintError(*tbegin, "Unknown strict mode");
        return PREPROCESS_ERROR;
    }

    if ((m_StrictMode != StrictModeUnspecified) &&
        (m_StrictMode != strictMode))
    {
        PrintError(*tbegin, "Conflicting strict modes found");
        return PREPROCESS_ERROR;
    }

    tbegin = find_if(next(tbegin), tend, Token::IsNonWS);

    if (tbegin != tend)
    {
        PrintError(*tbegin, "Unexpected tokens after #pragma mods strict");
        return PREPROCESS_ERROR;
    }

    m_StrictMode = strictMode;

    return OK;
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::DirectiveUndef(TokensIt tbegin, TokensIt tend)
{
    if (m_DisabledCode)
    {
        return OK;
    }

    if (tbegin == tend)
    {
        PrintError("Missing macro name");
        return PREPROCESS_ERROR;
    }
    if (tbegin->type != TT_IDENTIFIER)
    {
        PrintError(*tbegin, "Missing macro name");
        return PREPROCESS_ERROR;
    }

    auto macroIt = m_Macros.find(tbegin->token);

    // Silently ignore if the macro isn't defined

    if (macroIt != m_Macros.end())
    {
        if (macroIt->second.builtin != NOT_BUILTIN)
        {
            PrintError(*tbegin, "Unable to undefine built-in macro");
            return PREPROCESS_ERROR;
        }
        m_Macros.erase(macroIt);
    }

    tbegin = find_if(next(tbegin), tend, Token::IsNonWS);

    if (tbegin != tend)
    {
        PrintError(*tbegin, "Unexpected tokens after #undef");
        return PREPROCESS_ERROR;
    }

    return OK;
}

LwDiagUtils::EC LwDiagUtils::Preprocessor::GetNextLine
(
    Tokens*      pTokens,
    const char** pLineBegin,
    const char** pLineEnd
)
{
    LWDASSERT(!m_Files.empty());

    LWDASSERT(pTokens);
    {
        Tokens empty;
        pTokens->swap(empty);
        pTokens->reserve(32);
    }

    SkipLineContinuations();

    while (m_Files.back().Eof())
    {
        m_Files.pop_back();

        if (m_Files.empty())
            return OK;

        EmitLineNumber(PopFileFlag);

        SkipLineContinuations();
    }

    if (pLineBegin)
    {
        *pLineBegin = m_Files.back().runningBuf;
    }

    for (;;)
    {
        const TokenType type = GetNextToken(*pTokens);
#ifdef DEBUG_INFO
        static const char* const types[] = { "EOF   ", "EOL   ", "WHTESP", "IDN   ", "IDNEXP", "STRING", "NUMBER", "PUNCTR", "COMENT", "OTHER " };
        if (pTokens->size() == 1)
            printf("Line %d\n", pTokens->front().lineNum);
        printf("    %s %.*s\n", types[type], static_cast<int>(pTokens->back().token.size()), pTokens->back().token.begin());
#endif
        if (type == TT_EOL || type == TT_EOF)
        {
            break;
        }
    }

    if (pLineEnd)
    {
        *pLineEnd = m_Files.back().runningBuf;
    }

    if (!pTokens->empty())
    {
        m_LwrLineNum = pTokens->back().lineNum;
    }

    // TODO handle directives here, so that they can be used in-between
    //      function-like macro arguments

    return OK;
}

void LwDiagUtils::Preprocessor::GetNumber
(
    char         first,
    Token*       pToken,
    const char** pTokenBegin,
    const char** pBuf
)
{
    File& file            = m_Files.back();
    const char* const end = file.end;
    pToken->type          = TT_NUMBER;
    char last             = first;
    while (*pBuf < end)
    {
        char c = *((*pBuf)++);
        if (c != '\\')
        {
            const CharType type = GetCharType(c);
            if (type != CT_LETTER &&
                type != CT_DIGIT  &&
                c    != '.'       &&
                ! ((c    == '+' || c    == '-') &&
                   (last == 'e' || last == 'E'))
               )
            {
                --(*pBuf);
                break;
            }
            last = c;
        }
        else if (!SkipLineContinuation(pToken, pTokenBegin, pBuf))
        {
            --(*pBuf);
            break;
        }
    }
    file.column += static_cast<int>(*pBuf - *pTokenBegin);
}

namespace
{
    int Tab(int pos)
    {
        return ((pos + 8) & ~7) + 1;
    }
}

LwDiagUtils::Preprocessor::TokenType LwDiagUtils::Preprocessor::GetNextToken(Tokens& tokens)
{
    File& file = m_Files.back();

    tokens.emplace_back();
    Token& token = tokens.back();

    token.lineNum = file.lineNum;
    token.column  = file.column;

    const char* buf        = file.runningBuf;
    const char* const end  = file.end;
    const char* tokenBegin = buf;
    char        c;
    CharType    type;

    if (buf < end)
    {
        c    = *(buf++);
        type = GetCharType(c);
    }
    else
    {
        c    = 0;
        type = CT_EOF;
    }

    // Skip initial line continuation
    while (c == '\\' && type != CT_EOF)
    {
        if (SkipLineContinuation(nullptr, &tokenBegin, &buf))
        {
            token.lineNum = file.lineNum;
            token.column  = file.column;

            if (buf < end)
            {
                c    = *(buf++);
                type = GetCharType(c);
            }
            else
            {
                c    = 0;
                type = CT_EOF;
            }
        }
        else
        {
            break;
        }
    }

    switch (type)
    {
        case CT_EOF:
            token.type = TT_EOF;
            break;

        case CT_EOL:
            token.type = TT_EOL;
            if (buf < end && c == '\r' && *buf == '\n')
            {
                ++buf;
            }
            ++file.lineNum;
            file.column = 1;
            break;

        case CT_WHITESPACE:
        case CT_OTHER:
            token.type = (type == CT_WHITESPACE) ? TT_WHITESPACE : TT_OTHER;
            for (;;)
            {
                if (c == '\t')
                {
                    file.column = Tab(file.column);
                }
                else
                {
                    ++file.column;
                }

                if (buf >= end)
                {
                    break;
                }

                c = *(buf++);
                const CharType lwrType = GetCharType(c);
                if (lwrType != type)
                {
                    --buf;
                    break;
                }
            }
            break;

        case CT_LETTER:
            token.type = TT_IDENTIFIER;
            while (buf < end)
            {
                c = *(buf++);
                if (c != '\\')
                {
                    type = GetCharType(c);
                    if (type != CT_LETTER && type != CT_DIGIT)
                    {
                        --buf;
                        break;
                    }
                }
                else if (!SkipLineContinuation(&token, &tokenBegin, &buf))
                {
                    --buf;
                    break;
                }
            }
            file.column += static_cast<int>(buf - tokenBegin);
            break;

        case CT_DIGIT:
            GetNumber(c, &token, &tokenBegin, &buf);
            break;

        case CT_STRING:
            {
                token.type  = TT_STRING;
                bool escape = false;
                ++file.column;
                while (buf < end)
                {
                    char nextC = *(buf++);
                    switch (nextC)
                    {
                        case '\t':
                            file.column = Tab(file.column);
                            break;

                        case '\r':
                            if (buf < end && *buf == '\n')
                            {
                                ++buf;
                            }
                            // fall through

                        case '\n':
                            ++file.lineNum;
                            file.column = 1;
                            break;

                        case '\\':
                            if (buf < end)
                            {
                                type = GetCharType(*buf);
                                if (type == CT_WHITESPACE || type == CT_EOL)
                                {
                                    if (SkipLineContinuation(&token, &tokenBegin, &buf))
                                    {
                                        nextC = 0;
                                    }
                                }
                            }
                            break;

                        default:
                            ++file.column;
                            break;
                    }
                    if (nextC == c && ! escape)
                    {
                        break;
                    }
                    escape = (nextC == '\\') && ! escape;
                }
            }
            break;

        case CT_PUNCTUATOR:
            token.type = TT_PUNCTUATOR;
            tokenBegin = buf;
            ++file.column;
            GenerateStringView().Append(&token.token, buf - 1, 1);

            while (buf < end)
            {
                c = *(buf++);
                if (c != '\\')
                {
                    type = GetCharType(c);
                    if (type != CT_PUNCTUATOR)
                    {
                        --buf;
                        break;
                    }
                    if (!FindPunctuator(token.token, c))
                    {
                        --buf;
                        break;
                    }
                    GenerateStringView().Append(&token.token, buf - 1, 1);
                    tokenBegin  =  buf;
                    ++file.column;
                }
                else if (!SkipLineContinuation(&token, &tokenBegin, &buf))
                {
                    --buf;
                    break;
                }
            }

            if (token.token == StringView("//", 2))
            {
                token.type = TT_COMMENT;
                // We do not track column number here, so EOL or EOF token
                // following this comment will have an incorrect column number.
                while (buf < end)
                {
                    c = (*buf++);
                    if (c == '\\')
                    {
                        SkipLineContinuation(&token, &tokenBegin, &buf);
                    }
                    else if (c == '\r' || c == '\n')
                    {
                        --buf;
                        break;
                    }
                }
            }
            else if (token.token == StringView("/*", 2))
            {
                token.type = TT_COMMENT;
                char last  = '\0';
                while (buf < end)
                {
                    c = (*buf++);
                    if (c == '\t')
                    {
                        file.column = Tab(file.column);
                        last = c;
                    }
                    else if (c == '\r' || c == '\n')
                    {
                        if (c == '\r' && buf < end && *buf == '\n')
                        {
                            ++buf;
                        }
                        ++file.lineNum;
                        file.column = 1;
                        last = c;
                    }
                    else if (c != '\\')
                    {
                        if (last == '*' && c == '/')
                        {
                            break;
                        }
                        ++file.column;
                        last = c;
                    }
                    else if (!SkipLineContinuation(&token, &tokenBegin, &buf))
                    {
                        ++file.column;
                        last = c;
                    }
                }
            }
            else if (token.token == '.')
            {
                if (buf < end && GetCharType(*buf) == CT_DIGIT)
                {
                    GetNumber(c, &token, &tokenBegin, &buf);
                }
            }
            break;

        default:
            LWDASSERT(0);
            token.type = TT_EOF;
            break;
    }

    file.runningBuf  = buf;
    const size_t len = buf - tokenBegin;
    GenerateStringView().Append(&token.token, tokenBegin, len);

    return token.type;
}

LwDiagUtils::Preprocessor::CharType LwDiagUtils::Preprocessor::GetCharType(char c)
{
    return m_CharTable[static_cast<unsigned char>(c)];
}

bool LwDiagUtils::Preprocessor::SkipLineContinuation
(
    Token*       pToken,
    const char** pTokenBegin,
    const char** pBuf
)
{
    const char* buf  = *pBuf;
    bool        ok   = false;
    File&       file = m_Files.back();

    LWDASSERT(*(buf-1) == '\\');

    const char* const end = file.end;

    while (buf < end)
    {
        const char c = *(buf++);

        if (c == '\n')
        {
            ok = true;
            break;
        }
        else if (c == '\r')
        {
            ok = true;
            if (buf < end && *buf == '\n')
            {
                ++buf;
            }
            break;
        }
        // Ignore spaces between backslash and EOL.
        else if (c != ' ')
        {
            break;
        }
    }

    if (ok)
    {
        ++file.lineNum;
        file.column = 1;

        const size_t len = *pBuf - *pTokenBegin - 1;
        if (pToken)
        {
            GenerateStringView().Append(&pToken->token, *pTokenBegin, len);
        }

        *pTokenBegin = buf;
        *pBuf        = buf;
    }

    return ok;
}

void LwDiagUtils::Preprocessor::SkipLineContinuations()
{
    File& file = m_Files.back();
    if (file.Eof())
    {
        return;
    }

    const char* buf = file.runningBuf;

    while (*buf == '\\')
    {
        ++buf;
        const char* tokenBegin = buf;
        if (!SkipLineContinuation(nullptr, &tokenBegin, &buf))
        {
            --buf;
            break;
        }
    }

    file.runningBuf = buf;
}
