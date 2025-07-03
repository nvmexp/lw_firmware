/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_PREPROC_H
#define INCLUDED_PREPROC_H

#include "lwdiagutils.h"

#include <array>
#include <list>
#include <map>
#include <unordered_map>

namespace LwDiagUtils
{
    /// Behaves like a std::string in certain aspects, but it is immutable
    /// and it only contains a pointer to the string storage and size.
    ///
    /// Does not care how or where the string is actually stored,
    /// the caller must guarantee that the storage exists and does not
    /// change as long as StringView is alive.
    class StringView
    {
        public:
            StringView() = default;
            StringView(const char* begin, size_t size)
                : m_String(begin), m_Size(size) { }

            size_t size() const {
                return m_Size;
            }
            bool operator==(char c) const {
                return Compare(c) == 0;
            }
            bool operator!=(char c) const {
                return Compare(c) != 0;
            }
            bool operator==(const StringView& other) const {
                return Compare(other) == 0;
            }
            bool operator!=(const StringView& other) const {
                return Compare(other) != 0;
            }
            bool operator<(const StringView& other) const {
                return Compare(other) < 0;
            }
            const char& operator[](int i) const {
                return m_String[i];
            }
            const char* begin() const {
                return &m_String[0];
            }
            const char* end() const {
                return &m_String[m_Size];
            }

            int Compare(const StringView& other) const;
            int Compare(char c) const;

            size_t GetHash() const;

        private:
            const char* m_String = nullptr;
            size_t      m_Size   = 0U;
    };

    /// StringView hash function for std::unordered_map
    struct StringViewHash
    {
        size_t operator()(const StringView& str) const
        {
            return str.GetHash();
        }
    };

    /// Provides a storage with a reliable capacity and with semantics similar to vector<char>.
    ///
    /// Unlike vector, it does not ilwalidate pointers to data on resize.
    ///
    /// It can be thought of as "allocator" for generated StringViews.
    class GenBuf
    {
        public:
            GenBuf()                         = default;
            ~GenBuf()                        = default;
            GenBuf(const GenBuf&)            = delete;
            GenBuf& operator=(const GenBuf&) = delete;

            const char& operator[](size_t i) const { return m_Buf[i]; }
            char& operator[](size_t i)             { return m_Buf[i]; }
            size_t size() const                    { return m_Size; }
            static size_t capacity()               { return m_Capacity; }
            void resize(size_t size)               { m_Size = size; }

        private:
            static constexpr size_t m_Capacity = 4096U;
            size_t                  m_Size     = 0U;
            char                    m_Buf[m_Capacity];
    };

    using GeneratedBuffers = list<GenBuf>;

    /// Constructs new StringView with backing storage in GeneratedBuffers.
    class StringViewBuilder
    {
        public:
            explicit StringViewBuilder(GeneratedBuffers* storeBuffers)
                : m_StoreBuffers(storeBuffers), m_Size(0) { }

            operator StringView() const;

            StringViewBuilder& operator+=(char c);
            StringViewBuilder& operator+=(const StringView& s);
            StringViewBuilder operator+(char c) const {
                StringViewBuilder b = *this;
                b += c;
                return b;
            }
            StringViewBuilder operator+(const char* s) const;
            StringViewBuilder operator+(const string& s) const;
            StringViewBuilder operator+(const StringView& s) const {
                StringViewBuilder b = *this;
                b += s;
                return b;
            }

            void Append(StringView* pStr, const char* buf, size_t len);

        private:
            StringViewBuilder(GeneratedBuffers* storeBuffers, size_t size)
                : m_StoreBuffers(storeBuffers), m_Size(size) { }
            char* MakeRoom(size_t size) const;

            GeneratedBuffers* m_StoreBuffers;
            size_t            m_Size;
    };

    /// Simplified C preprocessor for JS scripts used by MODS.
    class Preprocessor
    {
    public:
        Preprocessor();
        ~Preprocessor();

        /// Sets the buffer containing the source file for preprocessing.
        EC SetBuffer(const char* buf, size_t len);
        /// Loads the source for preprocessing from a file.
        EC LoadFile(const string& file);
        /// Adds a search path for #include.
        void AddSearchPath(const string& path) { m_SearchPaths.push_back(path); }
        /// Clears all search paths.
        void ClearSearchPaths() { m_SearchPaths.clear(); }
        /// Adds a predefined macro.
        void AddMacro(const char* macroName, const char* value) { m_UserMacros.emplace_back(macroName, value); }

        typedef EC (*DecryptFile)(FILE*, vector<UINT08>*);

        /// Sets function used for decrypting files.
        ///
        /// We do this instead of calling it directly in order to
        /// avoid cirlwlar dependencies between liblwdiag_utils and libencryption.
        ///
        /// As a side effect, this also makes encryption support optional
        /// if this function is not called.
        void SetDecryptFile(DecryptFile fun) { m_DecryptFile = fun; }

        enum LineCommandMode
        {
            LineCommandNone,
            LineCommandComment,
            LineCommandAt,
            LineCommandHash
        };

        /// Sets the way the preprocessor emits line numbers.
        void SetLineCommandMode(LineCommandMode mode) { m_LineCommandMode = mode; }

        /// Disables collapsing of comments into a single space.
        void DisableCommentCollapsing() { m_CollapseComments = false; }

        /// Runs the preprocessor.
        ///
        /// SetBuffer or LoadFile must be called prior to calling Process.
        ///
        /// @param pResult Pointer to buffer which is filled with the output,
        ///                i.e. the result of the preprocessing.
        EC Process(vector<char>* pResult);

        enum StrictMode
        {
            StrictModeOff
           ,StrictModeOn
           ,StrictModeEngr
           ,StrictModeUnspecified
        };
        StrictMode GetStrictMode() { return m_StrictMode; }

        // The punctuator type is used for evaluating expressions in #if directive
        enum PunctuatorType
        {
            PT_ILLEGAL,     // Not a legal operator in the preprocessor
            PT_ADD,         // Binary add or unary plus
            PT_SUB,         // Binary subtract or unary minus
            PT_MUL,         // Binary multiply
            PT_DIV,         // Binary divide
            PT_MOD,         // Binary modulo
            PT_AND,         // Binary bitwise and
            PT_OR,          // Binary bitwise or
            PT_XOR,         // Binary bitwise xor
            PT_NOT,         // Unary bitwise not
            PT_SHL,         // Binary bitwise shift left
            PT_SHR,         // Binary bitwise shift right
            PT_EQ,          // Binary equal
            PT_NE,          // Binary not equal
            PT_LT,          // Binary less than
            PT_LE,          // Binary less than or equal
            PT_GT,          // Binary greater than
            PT_GE,          // Binary greater than or equal
            PT_LOGICAL_AND, // Binary logical and
            PT_LOGICAL_OR,  // Binary logical or
            PT_LOGICAL_NOT, // Unary logical not
            PT_TERNARY,     // Ternary ?
            PT_COLON,       // Ternary :
            PT_PAREN_CLOSE, // Open parenthesis
            PT_PAREN_OPEN   // Close parenthesis
        };

        struct Punctuator
        {
            const char*    str;
            PunctuatorType type;       // Operator type for expressions in #if directive
            int            precedence; // Operator precedence as defined in C
        };

        enum Directive
        {
            D_ILWALID,
            D_DEFINE,
            D_ELIF,
            D_ELSE,
            D_ENDIF,
            D_IF,
            D_IFEQ,
            D_IFNEQ,
            D_INCLUDE,
            D_PRAGMA,
            D_UNDEF,
            D_ERROR
        };

    private:
        struct File
        {
            string      path;
            const char* runningBuf;
            const char* end;
            int         lineNum;
            int         column;

            bool Eof() const { return runningBuf >= end; }
        };

        enum CharType
        {
            CT_EOF,
            CT_EOL,
            CT_WHITESPACE,
            CT_LETTER,
            CT_STRING,
            CT_DIGIT,
            CT_PUNCTUATOR,
            CT_OTHER
        };

        enum TokenType
        {
            TT_EOF,
            TT_EOL,
            TT_WHITESPACE,
            TT_IDENTIFIER,
            TT_IDENTIFIER_EXPANDED, // this is only used to mark arguments which have been expanded
            TT_STRING,
            TT_NUMBER,
            TT_PUNCTUATOR,
            TT_COMMENT,
            TT_OTHER
        };

        struct Token
        {
            StringView token;
            TokenType  type;
            int        lineNum;
            int        column;

            bool IsWS() const
            {
                return type == TT_EOL        ||
                       type == TT_WHITESPACE ||
                       type == TT_COMMENT    ||
                       type == TT_EOF;
            }

            static bool IsNonWS(const Token& token)
            {
                return ! token.IsWS();
            }
        };

        typedef vector<Token>    Tokens;
        typedef Tokens::iterator TokensIt;

        struct Argument
        {
            int  index;
            bool expand;
        };

        using Arguments = map<StringView, Argument>;

        enum BuiltinMacro
        {
            NOT_BUILTIN,
            BUILTIN_DEFINED,
            BUILTIN_FILE,
            BUILTIN_LINE
        };

        struct Macro
        {
            BuiltinMacro builtin;
            bool         hasArgs;
            Arguments    args;
            Tokens       tokens;
        };

        using Macros = unordered_map<StringView, Macro, StringViewHash>;

        enum ExpandType
        {
            ExpandFull,    // Read in new lines when necessary when collection function-like macro args
            ExpandOneLine, // Don't read in new lines
            ExpandIf       // Don't read in new lines and allow defined() macro
        };

        struct IfDirectiveState
        {
            bool enabled; // The entire #if was not disabled by previous #if
            bool active;  // The current or any previous section of an #if-#elif-#else chain was enabled
        };

        enum LineNumberFlags
        {
            StartFlag    = 0,
            PushFileFlag = 1,
            PopFileFlag  = 2
        };

        // This is to enable FetchMacroArgs() to retrieve tokens from the current line
        struct TokensStackItem
        {
            TokensIt tok;
            Tokens*  pTokens;
        };
        typedef vector<TokensStackItem> TokensStack;

        class StringStack
        {
            public:
                void Push(StringView str);
                void Pop();
                bool HasString(StringView str) const;

            private:
                string m_Stack;
        };

        // Put this here just to avoid compiler warnings
        struct NODE;

        StringViewBuilder GenerateStringView() const { return StringViewBuilder(&m_Generated); }

        EC              PushFile(const string& file);
        void            PrintError(const string& msg) const;
        void            PrintError(const Token& token, const string& msg) const;
        EC              GetNextLine(Tokens* pTokens, const char** pLineBegin, const char** pLineEnd);
        void            GetNumber(char first, Token* pToken, const char** pTokenBegin, const char** pBuf);
        TokenType       GetNextToken(Tokens& tokens);
        void            SkipLineContinuations();
        bool            SkipLineContinuation(Token* pToken, const char** pTokenBegin, const char** pBuf);
        static CharType GetCharType(char c);
        void            Output(const Tokens& tokens, vector<char>* pResult);
        void            OutputLineFast(const char* begin, const char* end, vector<char>* pResult);
        void            OutputEmptyLine(const Tokens& tokens, vector<char>* pResult);
        bool            OutputLineNumbers(vector<char>* pResult);
        static void     ReduceWhitespaces(TokensIt tbegin, TokensIt tend, Tokens* pTokens);
        EC              Concatenate(Tokens* pTokens, TokensIt* pTok) const;
        bool            CollapseComments(TokensIt begin, TokensIt tend) const;
        EC              ExpandMacros(Tokens* pTokens, bool* pDoneAnything, ExpandType expandType);
        EC              ExpandMacros(Tokens* pTokens, bool* pDoneAnything, ExpandType expandType, StringStack& expandedMacros);
        EC              ExpandOne(Tokens* pTokens, TokensIt* pTok, bool* pExpanded, ExpandType expandType, StringStack& expandedMacros);
        EC              FetchMacroArgs(Tokens* pTokens, TokensIt* pTok, TokensIt* pEndTok, ExpandType expandType, vector<Tokens>* pArgs, bool* pIlwoked);
        static bool     MarkNonExpand(Arguments* pArgs, const Token& token);
        void            PreventNewTokens(Tokens* pTokens);
        EC              CheckIfAllSpaces(TokensIt tbegin, TokensIt tend) const;
        static int      CountEOLs(const char* begin, const char* end);
        void            AddBuiltinMacro(const char* name, BuiltinMacro builtin, bool hasArgs=false);
        void            EmitLineNumber(LineNumberFlags flags);
        EC              Eval(TokensIt tbegin, TokensIt tend, bool* pTruthy);
        EC              EvalSub(TokensIt tbegin, TokensIt tend, INT64* pValue);
        static void     PropagateLineNumbers(const Token& token, TokensIt tbegin, TokensIt tend);

        EC DirectiveDefine  (TokensIt tbegin, TokensIt tend);
        EC DirectiveElif    (TokensIt tbegin, TokensIt tend);
        EC DirectiveElse    (TokensIt tbegin, TokensIt tend);
        EC DirectiveEndif   (TokensIt tbegin, TokensIt tend);
        EC DirectiveIf      (TokensIt tbegin, TokensIt tend);
        EC DirectiveIfDef   (TokensIt tbegin, TokensIt tend);
        EC DirectiveIfNDef  (TokensIt tbegin, TokensIt tend);
        EC DirectiveIfDef   (TokensIt tbegin, TokensIt tend, bool defined);
        EC DirectiveInclude (TokensIt tbegin, TokensIt tend);
        EC DirectivePragma  (TokensIt tbegin, TokensIt tend);
        EC DirectiveUndef   (TokensIt tbegin, TokensIt tend);
        EC DirectiveError   (TokensIt tbegin, TokensIt tend);

        struct UserMacro
        {
            string name;
            string contents;

            UserMacro(string macName, string macContents)
                : name(move(macName)), contents(move(macContents)) { }
        };
        using UserMacros = vector<UserMacro>;

        LineCommandMode             m_LineCommandMode     = LineCommandHash;
        bool                        m_CollapseComments    = true;
        vector<string>              m_SearchPaths;
        DecryptFile                 m_DecryptFile;
        UserMacros                  m_UserMacros;

        static const CharType       m_CharTable[256];

        vector<File>                m_Files;
        map<string, vector<UINT08>> m_Buffers;
        mutable GeneratedBuffers    m_Generated; // Stores strings for generated tokens
        vector<IfDirectiveState>    m_HashIfStack;
        Macros                      m_Macros;
        TokensStack                 m_TokensStack;

        bool                        m_DisabledCode;

        string                      m_LwrFile;
        int                         m_LwrLineNum;

        string                      m_LineNumbers;
        int                         m_NumOutputLines;

        StrictMode                  m_StrictMode          = StrictModeUnspecified;
    };

}

#endif // INCLUDED_PREPROC_H
