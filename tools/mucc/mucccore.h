/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once
#ifndef INCLUDED_MUCCCORE_H
#define INCLUDED_MUCCCORE_H

#include "lwdiagutils.h"
#include "stringview.h"
#include "amap_v1.h"
#include <deque>
#include <memory>
#include <unordered_map>

// Core classes and functions for the Mucc system
//
namespace Mucc
{
    using namespace LwDiagUtils;
    class Program;

    // Colwert a string to uppercase
    string ToUpper(const string_view& str);

    // Colwert a string to a quoted JSON string
    string ToJson(const string& str);

    // Colwert a litter to a string and back
    //
    AmapLitter  NameToLitter(const string& litterName);
    const char* LitterToName(AmapLitter litter);
    vector<string> GetLitterNames();

    //----------------------------------------------------------------
    //! \brief All of the build flags supported by Mucc
    //!
    enum class Flag
    {
        WARN_MIDLABEL, //!< Warn if a label is in the middle of an instruction
        WARN_NOP,      //!< Warn if "NOP" opcode seems to be misused
        WARN_OCTAL,    //!< Warn if octal constant seems to be malformed
        TREAT_WARNINGS_AS_ERRORS,

        // Must be last
        NUM_FLAGS
    };

    //----------------------------------------------------------------
    //! \brief Resizable array of bits, with a mask showing which were set
    //!
    //! This class is conceptually similar to vector<bool>, except
    //! that it includes a built-in mask so that we can tell which
    //! bits contain their default value and which were deliberately
    //! set.
    //!
    //! This comes up several times in Mucc.  For example, the default
    //! instruction words are all 0 except for the CKE bits, which are
    //! 1.  As we process the source file, we'll override those
    //! default bits, but we need to be able to tell which bits were
    //! overridden so that we can tell if the code is trying to set
    //! the same bit to two different values.
    //!
    //! This class is also used for build flags and pattern ram.  It's
    //! used in lots of vectors, so it's move-constructable for
    //! efficient resize() operations.
    //!
    //! Bitfields are accessed as INT32 values because all numeric
    //! constants in the Mucc assembler are INT32.  This class does
    //! not support bitfields with more than 32 bits.
    //!
    class Bits
    {
    public:
        Bits() {}
        explicit Bits(unsigned size) { Resize(size); }
        Bits(const Bits&)            = default;
        Bits(Bits&&) noexcept        = default;
        Bits& operator=(const Bits&) = default;
        bool operator==(const Bits&) const;
        bool operator!=(const Bits&) const;

        void  SetBit(unsigned bitNum, bool value);
        void  SetBits(unsigned hiBit, unsigned loBit, INT32 value);
        void  SetBits(const Bits& rhs);
        void  SetDefault(unsigned bitNum, bool value);
        void  SetDefaults(unsigned hiBit, unsigned loBit, INT32 value);
        void  AppendBit(bool value);
        void  AppendBits(unsigned numBits, INT32 value);
        bool  GetBit(unsigned bitNum) const;
        INT32 GetBits(unsigned hiBit, unsigned loBit) const;
        bool  Equals(unsigned hiBit, unsigned loBit, INT32 value) const;
        bool  IsBitSet(unsigned bitNum) const;
        bool  AnyBitSet(unsigned hiBit, unsigned loBit) const;
        void  Resize(unsigned size);
        void  Reserve(unsigned size);

        const vector<UINT32>& GetData() const { return m_Data; }
        unsigned              GetSize() const { return m_Size; }
        void                  PrintJson(FILE* fh, unsigned indent) const;
        static bool   IsOverflow(unsigned hiBit, unsigned loBit, INT32 value);
        static string GetOverflow(unsigned hiBit, unsigned loBit, INT32 value);
        static bool   IsOverflow64(unsigned hiBit, unsigned loBit, INT64 value);
        static string GetOverflow64(unsigned hiBit, unsigned loBit,
                                    INT64 value);

    private:
        bool CheckBitNum(unsigned bitNum)                    const;
        bool CheckBitRange32(unsigned hiBit, unsigned loBit) const;
        vector<UINT32> m_Data; //!< The data bits, both default and overridden
        vector<UINT32> m_Mask; //!< Which data bits were overridden
        unsigned       m_Size = 0; //!< Number of bits in m_Data & m_Mask
    };

    //----------------------------------------------------------------
    //! \brief Holds the build flags described by the Flag enum
    //!
    //! Flags are typically set in Launcher, and then copied to
    //! Program in case different litters need to set different
    //! default flags.
    //!
    class Flags
    {
    public:
        Flags() : m_Bits(Idx(Flag::NUM_FLAGS)) {}
        Flags(const Flags&)            = default;
        Flags& operator=(const Flags&) = default;
        void SetFlag(Flag fl, bool val)    { m_Bits.SetBit(Idx(fl), val); }
        void SetDefault(Flag  fl, bool val) { m_Bits.SetDefault(Idx(fl), val); }
        bool GetFlag(Flag fl)        const { return m_Bits.GetBit(Idx(fl)); }
        bool IsFlagSet(Flag fl)      const { return m_Bits.IsBitSet(Idx(fl)); }
    private:
        static unsigned Idx(Flag flag) { return static_cast<unsigned>(flag); }
        Bits m_Bits;
    };

    // Forward declaration of the ReservedWord and Symbol enums used
    // by the Token and Tokenizer classes.  These enums may be defined
    // differently in different litters, which is why all we have here
    // are forward declarations.
    //
    enum ReservedWord: unsigned;
    enum Symbol: unsigned;

    //----------------------------------------------------------------
    //! \brief Token created by the Tokenizer class
    //!
    //! Each token contains a "type" and "value".  The "type" says
    //! whether it's a reserved word, symbol, identifier, integer,
    //! etc.  The "value" is an enum that identifies the specific
    //! reserved word or symbol, or the decoded integer for integer
    //! types, or ignored for all other types.
    //!
    //! The tokens also contain other info that's useful for error
    //! messages: the filename, line number, the line (not including
    //! the terminating '\n'), and the text within the line that the
    //! token represents.  Most of those are strings, using the
    //! string_view class to keep the token small.
    //!
    //! Tokens can be used as the keys of of map, unordered_map, set,
    //! and unordered_set.  Two tokens are considered identical if
    //! they refer to the same underlying value (e.g. the same
    //! integer, the same reserved word, etc), even if the spelling is
    //! different (e.g. reserved words are case-insensitive).
    //!
    class Token
    {
    public:
        enum Type
        {
            RESERVED_WORD,  //!< Reserved word passed to Tokenizer constructor
            SYMBOL,         //!< Symbol passed to Tokenizer constructor
            IDENTIFIER,     //!< Alphabetic char followed by alphanumerics
            INTEGER,        //!< decimal, hex (0x), octal (0o), or binary (0b)
            ILLEGAL,        //!< Any other character(s)
            END_OF_FILE
        };

        Token(const Token&) = default;
        Token(Type type, INT64 value, const string_view& filename,
              const string_view& line, size_t lineNumber,
              string_view::size_type linePos, string_view::size_type size);
        Token& operator=(const Token&) = default;
        bool   operator<(const Token& rhs)  const;
        bool   operator==(const Token& rhs) const;
        size_t GetHash()                    const noexcept;

        Type               GetType()         const { return m_Type; }
        const string_view& GetText()         const { return m_Text; }
        string_view        GetText(const Token& end) const;
        const string_view& GetLine()         const { return m_Line; }
        string             GetFileLine()     const;
        void               PrintUnderlined(INT32 pri) const;
        void               PrintUnderlined(INT32 pri, const Token& end) const;
        bool               IsAdjacent(const Token& nextToken)           const;

        ReservedWord GetReservedWord()       const;
        Symbol       GetSymbol()             const;
        INT64        GetInteger()            const;
        bool         Isa(Type type)          const { return m_Type == type; }
        bool         Isa(ReservedWord value) const;
        bool         Isa(Symbol value)       const;
        void         ChangeType(Type type, INT64 value);

    private:
        friend class Tokenizer;

        // Token info
        Type        m_Type;
        INT64       m_Value;   //!< For RESERVED_WORD, SYMBOL, or INTEGER
        string_view m_Text;    //!< The text that this token represents

        // Debug info
        string_view m_Filename;   //!< The filename this token came from
        string_view m_Line;       //!< The line this token came from, sans '\n'
        size_t      m_LineNumber; //!< The line number this token came from
    };

    //----------------------------------------------------------------
    //! \brief Generic tokenizer for Mucc assembler and/or compiler
    //!
    //! This generic tokenizer takes a program text as input, and
    //! returns a series of Token objects.  If it finds a series of
    //! characters that isn't a valid token, it prints an error
    //! message and returns the bad characters as an ILLEGAL token.
    //! When it reaches the end of the input, it acts as if there are
    //! an infinite number of END_OF_FILE tokens at the end of the
    //! file.
    //!
    //! Tokenizer uses #line directives in the input to keep track of
    //! the current file & line number.  It does not generate any
    //! Tokens for #line directives; the #line directives merely
    //! affect the m_Filename and m_LineNumber info in subsequent
    //! Tokens.
    //!
    //! The Tokenizer and Tokens make heavy use of string_view objects
    //! pointing into the input, so the caller must not modify the
    //! input after passing it to the Tokenizer constructor.
    //!
    class Tokenizer
    {
    public:
        Tokenizer(Program*      pProgram,
                  vector<char>* pInput,
                  const unordered_map<string, ReservedWord>& reservedWords,
                  const unordered_map<string, Symbol>& symbols);
        Token& GetToken(size_t idx);
        void   Pop(size_t numTokens);

    private:
        // Holds the symbols map passed to the constructor, and
        // processed by ProcessSymbolsArg()
        //
        struct SizedSymbols
        {
            string::size_type   size;
            unordered_map<string, Symbol> symbols;
        };

        static string_view ProcessInputArg(vector<char>* pInput);
        static unordered_map<string, ReservedWord> ProcessReservedWordsArg(
                const unordered_map<string, ReservedWord>& reservedWords);
        static vector<SizedSymbols> ProcessSymbolsArg(
                const unordered_map<string, Symbol>& symbols);
        void ParseLine();
        void ParseHashLine(const string_view& line);
        void ParseIdentifier(const string_view& line,
                             string_view::size_type pos);
        void ParseInteger(const string_view& line, string_view::size_type pos);
        void ParseSymbol(const string_view& line, string_view::size_type pos);
        void PushToken(Token::Type type, INT64 value, const string_view& line,
                       string_view::size_type pos, string_view::size_type size);

        // Set by constructor
        Program*                   m_pProgram;
        const unordered_map<string, ReservedWord> m_ReservedWords;
        const vector<SizedSymbols> m_Symbols;
        const bool                 m_NewlineIsToken;
        const Symbol               m_NewlineSymbol;

        // Updated during parsing
        string_view       m_RemainingInput;  //!< Remainder of input code
        string_view       m_Filename   = "<stdin>";
        size_t            m_LineNumber = 1;
        deque<Token>      m_Tokens;          //!< Tokens read by GetToken()
        unique_ptr<Token> m_pEofToken;       //!< Set to non-null at EOF
    };
};

namespace std
{
    //----------------------------------------------------------------
    //! \brief Support for unordered_set & unordered_map of Tokens
    //!
    //! Two tokens are considered identical if they refer to the same
    //! underlying value (e.g. the same integer, the same reserved
    //! word, etc), even if the spelling is different.
    //!
    template<> struct hash<Mucc::Token>
    {
        size_t operator()(const Mucc::Token& token) const noexcept
        {
            return token.GetHash();
        }
    };
}

#endif // INCLUDED_MUCCCORE_H
