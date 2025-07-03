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

#include "mucc.h"
#include "lwmisc.h"
#include <cstdlib>
using namespace LwDiagUtils;

namespace Mucc
{
    //! Name-to-value mapping of all litters supported by Mucc
    //!
    pair<AmapLitter, const char*> s_LitterNames[] =
    {
        { LITTER_GALIT1, "GALIT1" },
        { LITTER_GHLIT1, "GHLIT1" }
    };

    //----------------------------------------------------------------
    //! \brief Colwert a string to uppercase
    //!
    string ToUpper(const string_view& str)
    {
        string output(str.data(), str.size());
        for (char& ch: output)
        {
            ch = toupper(ch);
        }
        return output;
    }

    //----------------------------------------------------------------
    //! \brief Colwert a string to a quoted JSON string
    //!
    string ToJson(const string& str)
    {
        string output;
        output.reserve(str.size() + 2);
        output += '"';
        for (const char ch: str)
        {
            switch (ch)
            {
                case '"':
                    output += "\\\"";
                    break;
                case '\\':
                    output += "\\\\";
                    break;
                case '\n':
                    output += "\\n";
                    break;
                case '\r':
                    output += "\\r";
                    break;
                case '\t':
                    output += "\\t";
                    break;
                default:
                    if (isprint(ch))
                    {
                        output += ch;
                    }
                    else
                    {
                        char buffer[8];
                        sprintf(buffer, "\\u%04x", 0x00ff & ch);
                        output += buffer;
                    }
                    break;
            }
        }
        output += '"';
        return output;
    }

    //----------------------------------------------------------------
    //! \brief Colwert a case-insensitive litter name to an AmapLitter
    //!
    //! Return LITTER_ILWALID if the litter name is unsupported.
    //!
    AmapLitter NameToLitter(const string& litterName)
    {
        const string upperName = ToUpper(litterName);
        for (const auto& iter: s_LitterNames)
        {
            if (iter.second == upperName)
            {
                return iter.first;
            }
        }
        return LITTER_ILWALID;
    }

    //----------------------------------------------------------------
    //! \brief Colwert an AmapLitter to a string
    //!
    //! Return nullptr if the litter name is unsupported.
    //!
    const char* LitterToName(AmapLitter litter)
    {
        for (const auto& iter: s_LitterNames)
        {
            if (iter.first == litter)
            {
                return iter.second;
            }
        }
        return nullptr;
    }

    //----------------------------------------------------------------
    //! \brief Return all litter names supported by Mucc
    //!
    vector<string> GetLitterNames()
    {
        vector<string> names;
        for (const auto& iter: s_LitterNames)
        {
            names.push_back(iter.second);
        }
        return names;
    }

    //----------------------------------------------------------------
    //! \brief Test equality of data held in two Bits objects
    //!
    //! Two Bits objects are equal when:
    //! 1. They hold the same number of bits (GetSize() is equal)
    //! 2. The two objects are bitwise identical over the bits they contain
    //!
    bool Bits::operator==(const Bits& rhs) const
    {
        // 1. Check sizes
        if (GetSize() != rhs.GetSize())
        {
            return false;
        }

        // 2. Check that they're bitwise identical
        //
        // We do this by iterating over the data vectors and comparing
        // each UINT32, applying a mask to the last entry if the bit
        // vector doesn't use all the bits in the last UINT32.

        LWDASSERT(GetData().size() == rhs.GetData().size());

        const int numberOfEntries = static_cast<int>(GetData().size());
        for (int i = 0; i < numberOfEntries; ++i)
        {
            UINT32 leftEntry = GetData()[i];
            UINT32 rightEntry = rhs.GetData()[i];

            // If this is the last entry, apply mask
            if (i == numberOfEntries - 1)
            {
                const UINT32 hiBit = (GetSize() - 1) % 32;
                const UINT32 mask  = DRF_SHIFTMASK(hiBit:0);

                leftEntry  &= mask;
                rightEntry &= mask;
            }

            if (leftEntry != rightEntry)
            {
                return false;
            }
        }

        return true;
    }

    //----------------------------------------------------------------
    //! \brief Test inequality of data held in two Bits objects
    //!
    bool Bits::operator!=(const Bits& rhs) const
    {
        return !(*this == rhs);
    }

    //----------------------------------------------------------------
    //! \brief Set a single bit to the indicated value
    //!
    void Bits::SetBit(unsigned bitNum, bool value)
    {
        LWDASSERT(CheckBitNum(bitNum));
        const unsigned wordNum = bitNum / 32;
        const UINT32   bitMask = 1U << (bitNum % 32);
        if (value)
            m_Data[wordNum] |= bitMask;
        else
            m_Data[wordNum] &= ~bitMask;
        m_Mask[wordNum] |= bitMask;
    }

    //----------------------------------------------------------------
    //! \brief Set the bitfield hiBit:loBit to the indicated value
    //!
    //! This method asserts if the value would overflow.
    //!
    void Bits::SetBits(unsigned hiBit, unsigned loBit, INT32 value)
    {
        LWDASSERT(CheckBitRange32(hiBit, loBit));
        LWDASSERT(!IsOverflow(hiBit, loBit, value));

        const unsigned hiIdx = hiBit / 32;
        const unsigned loIdx = loBit / 32;
        const unsigned shift = loBit % 32;

        if (hiIdx == loIdx)
        {
            // If the data doesn't cross a DWORD boundary,
            // then update the DWORD
            const UINT32 mask = DRF_MASK((hiBit - loBit) : 0);
            m_Data[loIdx] &= ~(mask << shift);
            m_Data[loIdx] |= (value & mask) << shift;
            m_Mask[loIdx] |= mask << shift;
        }
        else
        {
            // If the data crosses a DWORD boundary, then cast two
            // conselwtive DWORDS to a UINT64 and operate on that.
            const UINT64   mask = DRF_MASK((hiBit - loBit) : 0);
            UINT64*        pData = reinterpret_cast<UINT64*>(&m_Data[loIdx]);
            UINT64*        pMask = reinterpret_cast<UINT64*>(&m_Mask[loIdx]);
            *pData &= ~(mask << shift);
            *pData |= (value & mask) << shift;
            *pMask |= mask << shift;
        }
    }

    //----------------------------------------------------------------
    //! \brief Set any bits that were overridden in rhs
    //!
    void Bits::SetBits(const Bits& rhs)
    {
        // If rhs is bigger, then append the data & mask bits from rhs
        //
        if (rhs.m_Size > m_Size)
        {
            if (rhs.m_Data.size() > m_Data.size())
            {
                const size_t mySize  = m_Data.size();
                m_Data.insert(m_Data.end(), rhs.m_Data.begin() + mySize,
                              rhs.m_Data.end());
                m_Mask.insert(m_Mask.end(), rhs.m_Mask.begin() + mySize,
                              rhs.m_Mask.end());
            }
            m_Size = rhs.m_Size;
        }

        // Set all masked data from rhs
        //
        for (unsigned ii = 0; ii < rhs.m_Data.size(); ++ii)
        {
            m_Data[ii] &= ~rhs.m_Mask[ii];
            m_Data[ii] |= (rhs.m_Mask[ii] & rhs.m_Data[ii]);
            m_Mask[ii] |= rhs.m_Mask[ii];
        }
    }

    //----------------------------------------------------------------
    //! \brief Set a single bit to a default value
    //!
    //! This is similar to SetBit(), except (1) it won't update
    //! m_Mask, and (2) it won't set the data if m_Mask indicates that
    //! the caller already used SetBit() or SetBits() to override the
    //! bit.
    //!
    void Bits::SetDefault(unsigned bitNum, bool value)
    {
        LWDASSERT(CheckBitNum(bitNum));
        const unsigned wordNum = bitNum / 32;
        const UINT32   bitMask = 1U << (bitNum % 32);
        if (value)
            m_Data[wordNum] |= bitMask;
        else
            m_Data[wordNum] &= ~bitMask;
    }

    //----------------------------------------------------------------
    //! \brief Set the bitfield hiBit:loBit to a default value
    //!
    //! This is similar to SetBits(), except (1) it won't update
    //! m_Mask, and (2) it won't set any data bits if m_Mask indicates
    //! that the caller already used SetBit() and/or SetBits() to
    //! override them.
    //!
    //! This method asserts if the value would overflow.
    //!
    void Bits::SetDefaults(unsigned hiBit, unsigned loBit, INT32 value)
    {
        LWDASSERT(CheckBitRange32(hiBit, loBit));
        LWDASSERT(!IsOverflow(hiBit, loBit, value));

        const unsigned hiIdx = hiBit / 32;
        const unsigned loIdx = loBit / 32;
        const unsigned shift = loBit % 32;

        if (hiIdx == loIdx)
        {
            // If the data doesn't cross a DWORD boundary,
            // then update the DWORD
            const UINT32 mask = DRF_MASK((hiBit - loBit) : 0);
            m_Data[loIdx] &= ~(mask << shift);
            m_Data[loIdx] |= (value & mask) << shift;
        }
        else
        {
            // If the data crosses a DWORD boundary, then cast two
            // conselwtive DWORDS to a UINT64 and operate on that.
            const UINT64   mask = DRF_MASK((hiBit - loBit) : 0);
            UINT64*        pData = reinterpret_cast<UINT64*>(&m_Data[loIdx]);
            *pData &= ~(mask << shift);
            *pData |= (value & mask) << shift;
        }
    }

    //----------------------------------------------------------------
    //! \brief Append a bit to the end
    //!
    void Bits::AppendBit(bool value)
    {
        const unsigned oldSize = m_Size;
        Resize(m_Size + 1);
        SetBit(oldSize, value);
    }

    //----------------------------------------------------------------
    //! \brief Append a bitfield to the end
    //!
    void Bits::AppendBits(unsigned numBits, INT32 value)
    {
        LWDASSERT(numBits > 0);
        const unsigned oldSize = m_Size;
        Resize(m_Size + numBits);
        SetBits(oldSize + numBits - 1, oldSize, value);
    }

    //----------------------------------------------------------------
    //! \brief Get a single data bit
    //!
    bool Bits::GetBit(unsigned bitNum) const
    {
        LWDASSERT(CheckBitNum(bitNum));
        return (m_Data[bitNum / 32] & (1U << (bitNum % 32))) != 0;
    }

    //----------------------------------------------------------------
    //! \brief Get a bitfield hiBit:loBit of data
    //!
    //! If the bitfield is less than 32 bits, then the returned INT32
    //! will be positive, even if a negative number was used to set
    //! the bitfield.
    //!
    INT32 Bits::GetBits(unsigned hiBit, unsigned loBit) const
    {
        LWDASSERT(CheckBitRange32(hiBit, loBit));

        const unsigned hiIdx = hiBit / 32;
        const unsigned loIdx = loBit / 32;
        const unsigned shift = loBit % 32;
        const INT32 mask = DRF_MASK((hiBit - loBit) : 0);

        if (hiIdx == loIdx)
        {
            return static_cast<INT32>(m_Data[loIdx] >> shift) & mask;
        }
        else
        {
            const INT64* pData = reinterpret_cast<const INT64*>(&m_Data[loIdx]);
            return static_cast<INT32>(*pData >> shift) & mask;
        }
    }

    //----------------------------------------------------------------
    //! \brief Return true if the bitfield hiBit:loBit equals the value
    //!
    //! If the high bit of the bitfield is set, then the "value"
    //! argument may be a positive number or a sign-extended negative
    //! number.  Either way, this method returns true as long as it
    //! aliases to the data in the bitfield.
    //!
    bool Bits::Equals(unsigned hiBit, unsigned loBit, INT32 value) const
    {
        if (IsOverflow(hiBit, loBit, value))
        {
            return false;
        }
        const INT32 mask = DRF_MASK((hiBit - loBit) : 0);
        return GetBits(hiBit, loBit) == (value & mask);
    }

    //----------------------------------------------------------------
    //! \brief Return true if the indicated bit was overridden
    //!
    bool Bits::IsBitSet(unsigned bitNum) const
    {
        LWDASSERT(CheckBitNum(bitNum));
        return (m_Mask[bitNum / 32] & (1U << (bitNum % 32))) != 0;
    }

    //----------------------------------------------------------------
    //! \brief Return true if any bit in hiBit:loBit was overridden
    //!
    //! Unlike most bitfield operations in this class, this one
    //! doesn't require hiBit:loBit to be 32 bits or less.
    //!
    bool Bits::AnyBitSet(unsigned hiBit, unsigned loBit) const
    {
        LWDASSERT(hiBit >= loBit);
        LWDASSERT(hiBit < m_Size);

        const unsigned hiIdx   = hiBit / 32;
        const unsigned loIdx   = loBit / 32;
        const unsigned hiBit32 = hiBit % 32;
        const unsigned loBit32 = loBit % 32;

        if (hiIdx == loIdx)
        {
            // If hiBit:loBit doesn't cross a DWORD boundary, then
            // just check one DWORD of the mask
            return (m_Mask[loIdx] & DRF_SHIFTMASK(hiBit32:loBit32)) != 0;
        }
        else
        {
            // If the data crosses a DWORD boundary, then check the
            // first and last DWORDS specially, and loop through the
            // rest between.
            if ((m_Mask[hiIdx] & DRF_SHIFTMASK(hiBit32:0))  != 0 ||
                (m_Mask[loIdx] & DRF_SHIFTMASK(31:loBit32)) != 0)
            {
                return true;
            }
            for (unsigned idx = loIdx + 1; idx < hiIdx; ++idx)
            {
                if (m_Mask[idx] != 0)
                {
                    return true;
                }
            }
            return false;
        }
    }

    //----------------------------------------------------------------
    //! \brief Resize the data & mask
    //!
    void Bits::Resize(unsigned size)
    {
        const unsigned numWords = (size + 31) / 32;
        m_Data.resize(numWords);
        m_Mask.resize(numWords);
        m_Size = size;
    }

    //----------------------------------------------------------------
    //! \brief Reserve space for the data & mask
    //!
    void Bits::Reserve(unsigned size)
    {
        const unsigned numWords = (size + 31) / 32;
        m_Data.reserve(numWords);
        m_Mask.reserve(numWords);
    }

    //----------------------------------------------------------------
    //! \brief Print the bits as a JSON array of 32-bit integers
    //!
    //! Print the bits in the following form:
    //!     least_significant_dword,
    //!     ...
    //!     most_significant_dword
    //! The caller must supply the surrounding square brackets.
    //!
    //! \param indent Indent the list by this many spaces.
    //!
    void Bits::PrintJson(FILE* fh, unsigned indent) const
    {
        const string indentStr(indent, ' ');
        const char* sep = "";
        for (UINT32 entry: m_Data)
        {
            fprintf(fh, "%s", sep);
            fprintf(fh, "%s%u", indentStr.c_str(), entry);
            sep = ",\n";
        }
        fprintf(fh, "\n");
    }

    //----------------------------------------------------------------
    //! \brief Return true if the value would overflow hiBit:loBit
    //!
    //! Note that this method allows any value that would fit into a
    //! signed or unsigned bitfield.  For example, a 4-bit bitfield
    //! can hold a signed number from -8 to +7, or an unsigned number
    //! from 0 to +15, so this method would allow any value from -8 to
    //! +15.
    //!
    /* static */ bool Bits::IsOverflow
    (
        unsigned hiBit,
        unsigned loBit,
        INT32    value
    )
    {
        unsigned numBits = 1 + hiBit - loBit;
        if (numBits >= 32)
        {
            return false;
        }
        else
        {
            const INT32 maxValue = DRF_MASK((hiBit - loBit) : 0);
            const INT32 milwalue = ~(maxValue >> 1);
            return value > maxValue || value < milwalue;
        }
    }

    //----------------------------------------------------------------
    //! \brief Return a string describing why IsOverflow() returned true
    //!
    //! Used for error messages.  Do not call this if IsOverflow()
    //! returned false.
    //!
    /* static */ string Bits::GetOverflow
    (
        unsigned hiBit,
        unsigned loBit,
        INT32    value
    )
    {
        LWDASSERT(IsOverflow(hiBit, loBit, value));
        const INT32  maxValue = DRF_MASK((hiBit - loBit) : 0);
        const INT32  milwalue = ~(maxValue >> 1);
        const size_t BUF_SIZE = 80;
        char buffer[BUF_SIZE];
        if (value > maxValue)
        {
            snprintf(buffer, BUF_SIZE, "too high; must be between %d and %d",
                     milwalue, maxValue);
        }
        else
        {
            LWDASSERT(value < milwalue);
            snprintf(buffer, BUF_SIZE, "too low; must be between %d and %d",
                     milwalue, maxValue);
        }
        return buffer;
    }

    //----------------------------------------------------------------
    //! \brief Return true if the 64-bit value would overflow hiBit:loBit
    //!
    //! Determine whether a 64-bit value would overflow a bitfield,
    //! using the same rules as Bits::IsOverflow().  A Bits object
    //! can't handle 64-bit fields, so this method is used to analyze
    //! other bitfields.
    //!
    /* static */ bool Bits::IsOverflow64
    (
        unsigned hiBit,
        unsigned loBit,
        INT64    value
    )
    {
        unsigned numBits = 1 + hiBit - loBit;
        if (numBits >= 64)
        {
            return false;
        }
        else
        {
            const INT64 maxValue = DRF_MASK64((hiBit - loBit) : 0);
            const INT64 milwalue = ~(maxValue >> 1);
            return value > maxValue || value < milwalue;
        }
    }

    //----------------------------------------------------------------
    //! \brief Return a string describing why IsOverflow64() returned true
    //!
    //! Used for error messages.  Do not call this if IsOverflow64()
    //! returned false.
    //!
    /* static */ string Bits::GetOverflow64
    (
        unsigned hiBit,
        unsigned loBit,
        INT64    value
    )
    {
        LWDASSERT(IsOverflow64(hiBit, loBit, value));
        const INT64  maxValue = DRF_MASK64((hiBit - loBit) : 0);
        const INT64  milwalue = ~(maxValue >> 1);
        const size_t BUF_SIZE = 80;
        char buffer[BUF_SIZE];
        if (value > maxValue)
        {
            snprintf(buffer, BUF_SIZE,
                     "too high; must be between %lld and %lld",
                     milwalue, maxValue);
        }
        else
        {
            LWDASSERT(value < milwalue);
            snprintf(buffer, BUF_SIZE,
                     "too low; must be between %lld and %lld",
                     milwalue, maxValue);
        }
        return buffer;
    }

    //----------------------------------------------------------------
    //! \brief Return false if bitNum is out of range; used for asserts
    //!
    bool Bits::CheckBitNum(unsigned bitNum) const
    {
        return bitNum < m_Size;
    }

    //----------------------------------------------------------------
    //! \brief Return false if hiBit:loBit is not a valid bitfield
    //!
    //! hiBit:loBit must be within range, and contain 1-32 bits.  Used
    //! for asserts.
    //!
    bool Bits::CheckBitRange32(unsigned hiBit, unsigned loBit) const
    {
        return (hiBit < m_Size &&
                hiBit >= loBit &&
                hiBit - loBit <= 31);
    }

    //----------------------------------------------------------------
    //! \brief Construct a Token object; used by Tokenizer class
    //!
    //! \param type The type of token, e.g. RESERVED_WORD
    //! \param value For reserved words and symbols, this is taken
    //!     from the maps passed to the Tokenizer constructor.  For
    //!     integers, this is the value of the integer.  Use 0 for
    //!     everything else.
    //! \param filename The name of the file, normally taken from the
    //!     last #line directive.
    //! \param line The line containing the token, without the
    //!     terminating '\n'.
    //! \param lineNumber The line number, normally callwlated from
    //!     the last #line directive plus the number of newlines
    //! \param linePos The position of the token within the line
    //! \param size The size of the token, in bytes.
    Token::Token
    (
        Type                   type,
        INT64                  value,
        const string_view&     filename,
        const string_view&     line,
        size_t                 lineNumber,
        string_view::size_type linePos,
        string_view::size_type size
    ) :
        m_Type(type),
        m_Value(value),
        m_Text(line.data() + linePos, size),
        m_Filename(filename),
        m_Line(line),
        m_LineNumber(lineNumber)
    {
        // Make sure the token lies entirely within the line.
        // Exception: The newline right after the line is allowed.
        LWDASSERT(linePos + size <= line.size() ||
                  (linePos == line.size() && size == 1 && m_Text[0] == '\n'));
    }

    //----------------------------------------------------------------
    //! \brief Less-than operator for Tokens
    //!
    //! Used to support Tokens as keys in map and set.  Two tokens are
    //! considered identical if they refer to the same underlying
    //! value (e.g. the same integer, the same reserved word, etc),
    //! even if the spelling is different.
    //!
    bool Token::operator<(const Token& rhs) const
    {
        if (m_Type != rhs.m_Type)
        {
            return m_Type < rhs.m_Type;
        }

        switch (m_Type)
        {
            case END_OF_FILE:
                return false;  // These tokens are equal, not less-than
            case RESERVED_WORD:
            case SYMBOL:
            case INTEGER:
                return m_Value < rhs.m_Value;
            case IDENTIFIER:
            case ILLEGAL:
                return m_Text < rhs.m_Text;
        };
        LWDASSERT(!"Unknown type");
        return false;
    }

    //----------------------------------------------------------------
    //! \brief Equality operator for Tokens
    //!
    //! Used to support Tokens as keys in unordered_map and
    //! unordered_set.  Two tokens are considered identical if they
    //! refer to the same underlying value (e.g. the same integer, the
    //! same reserved word, etc), even if the spelling is different.
    //!
    bool Token::operator==(const Token& rhs) const
    {
        if (m_Type != rhs.m_Type)
        {
            return false;
        }

        switch (m_Type)
        {
            case END_OF_FILE:
                return true;
            case RESERVED_WORD:
            case SYMBOL:
            case INTEGER:
                return m_Value == rhs.m_Value;
            case IDENTIFIER:
            case ILLEGAL:
                return m_Text == rhs.m_Text;
        };
        LWDASSERT(!"Unknown type");
        return false;
    }

    //----------------------------------------------------------------
    //! \brief Hash function for Tokens
    //!
    //! Used to support Tokens as keys in unordered_map and
    //! unordered_set.  Two tokens are considered identical if they
    //! refer to the same underlying value (e.g. the same integer, the
    //! same reserved word, etc), even if the spelling is different.
    //!
    size_t Token::GetHash() const noexcept
    {
        // Shift the type (RESERVED_WORD, etc) into the
        // most-significant byte of the hash.
        //
        static const unsigned MOST_SIG_BYTE = 8 * (sizeof(size_t) - 1);
        const size_t typeHash = static_cast<size_t>(m_Type) << MOST_SIG_BYTE;

        // XOR with a hash number that depends on m_Type.  Most token
        // types can be distinguished by a relatively small m_Value,
        // in which case this yields a unique hash per token.
        //
        switch (m_Type)
        {
            case END_OF_FILE:
                return typeHash;
            case RESERVED_WORD:
            case SYMBOL:
            case INTEGER:
                return typeHash ^ static_cast<size_t>(m_Value);
            case IDENTIFIER:
            case ILLEGAL:
                return typeHash ^ std::hash<string_view>()(m_Text);
        };
        LWDASSERT(!"Unknown type");
        return 0;
    }

    //----------------------------------------------------------------
    //! \brief Return all text from the start of this token to the end of "end"
    //!
    string_view Token::GetText(const Token& end) const
    {
        const char* pStart = m_Text.data();
        const char* pEnd   = end.m_Text.data() + end.m_Text.size();
        LWDASSERT(pEnd >= pStart);
        return string_view(pStart, pEnd - pStart);
    }

    //----------------------------------------------------------------
    //! \brief Return a string of the form filename[line_number]
    //!
    //! Used for warning and error messages
    //!
    string Token::GetFileLine() const
    {
        const size_t MAX_SIZE = 128;
        char output[MAX_SIZE];
        snprintf(output, MAX_SIZE, "%s[%zu]",
                 ToString(m_Filename).c_str(), m_LineNumber);
        return output;
    }

    //----------------------------------------------------------------
    //! \brief Print the line containing the token, with the token underlined
    //!
    //! For example, lets say the token is the word "R0" in the line
    //! "LD R0 50".  This method would print the following:
    //!
    //!         LD R0 50
    //!            ^^
    //!
    //! Used for warning and error messages.
    //!
    void Token::PrintUnderlined(INT32 pri) const
    {
        const size_t linePos = m_Text.data() - m_Line.data();
        Printf(pri, "%s\n", ToString(m_Line).c_str());
        Printf(pri, "%s%s\n",
               string(linePos, ' ').c_str(),
               string(m_Text.size(), '^').c_str());
    }

    //----------------------------------------------------------------
    //! \brief Print the line with multiple tokens underlined
    //!
    //! Used for warning and error messages.
    //!
    //! \param end A token on the same line as "this", appearing later
    //! in the line.  Underline "this", "end", and everything in
    //! between.
    //!
    void Token::PrintUnderlined(INT32 pri, const Token& end) const
    {
        LWDASSERT(m_Line == end.m_Line);
        LWDASSERT(m_Text.data() <= end.m_Text.data());
        const char*  pEnd     = end.m_Text.data() + end.m_Text.size();
        const size_t linePos  = m_Text.data() - m_Line.data();
        const size_t lineSize = pEnd - m_Text.data();
        Printf(pri, "%s\n", ToString(m_Line).c_str());
        Printf(pri, "%s%s\n",
               string(linePos, ' ').c_str(),
               string(lineSize, '^').c_str());
    }

    //----------------------------------------------------------------
    //! \brief Return true if two tokens are adjacent
    //!
    //! Return true if this token comes immediatly before nextToken,
    //! with no space in between.
    //!
    bool Token::IsAdjacent(const Token& nextToken) const
    {
        return m_Text.data() + m_Text.size() == nextToken.m_Text.data();
    }

    //----------------------------------------------------------------
    //! \brief Return the ReservedWord enum for a RESERVED_WORD token
    //!
    ReservedWord Token::GetReservedWord() const
    {
        LWDASSERT(m_Type == RESERVED_WORD);
        return static_cast<ReservedWord>(m_Value);
    }

    //----------------------------------------------------------------
    //! \brief Return the Symbol enum for a SYMBOL token
    //!
    Symbol Token::GetSymbol() const
    {
        LWDASSERT(m_Type == SYMBOL);
        return static_cast<Symbol>(m_Value);
    }

    //----------------------------------------------------------------
    //! \brief Return the integer value of an INTEGER token
    //!
    INT64 Token::GetInteger() const
    {
        LWDASSERT(m_Type == INTEGER);
        return m_Value;
    }

    //----------------------------------------------------------------
    //! \brief Return true if this token is the specified reserved word
    //!
    bool Token::Isa(ReservedWord value) const
    {
        return m_Type == RESERVED_WORD && m_Value == value;
    }

    //----------------------------------------------------------------
    //! \brief Return true if this token is the specified symbol
    //!
    bool Token::Isa(Symbol value) const
    {
        return m_Type == SYMBOL && m_Value == value;
    }

    //----------------------------------------------------------------
    //! \brief Change the type and value of a token
    //!
    //! This method is used by the Tokenizer class, so that it can
    //! change the token's type during parsing.
    //!
    void Token::ChangeType(Type type, INT64 value)
    {
        m_Type  = type;
        m_Value = value;
    }

    //----------------------------------------------------------------
    //! \brief Tokenizer constructor
    //!
    //! \param pProgram The Program object that's parsing this
    //!     program.  Mostly used by Tokenizer to find which warning
    //!     flags are enabled.
    //! \param pInput The program text.  Due to heavy usage of
    //!     string_views pointing into the input, the caller must not
    //!     delete or modify the input until Tokenizer and all of its
    //!     tokens are done.  Minor exception: a lot of code was
    //!     simpler if the input always ended in a newline, so if it
    //!     doesn't, then the Tokenizer constructor appends one.
    //! \param reservedWords A map containing case-insensitive
    //!     alphanumeric reserved words, and the corresponding enum
    //!     value they map to.  For example, if reservedWords["foo"] =
    //!     5, and the Tokenizer finds the word "Foo" in the input, it
    //!     creates a Token with token.GetType() = RESERVED_WORD and
    //!     token.GetReservedWord() = 5.
    //! \param symbols Similar to reservedWords, except that the keys
    //!     are non-alphanumeric symbols.  In case of ambiguity, the
    //!     longest-matching symbol is parsed first.  For example, if
    //!     "+" and "++" are both symbols, then the string "+++++"
    //!     will be parsed into "++", "++", and "+".  If one of the
    //!     symbols is "\n", then a newline will be a separate token,
    //!     otherwise it is treated as whitespace.
    //!
    Tokenizer::Tokenizer
    (
        Program*        pProgram,
        vector<char>*   pInput,
        const unordered_map<string, ReservedWord>& reservedWords,
        const unordered_map<string, Symbol>&       symbols
    ) :
        m_pProgram(pProgram),
        m_ReservedWords(ProcessReservedWordsArg(reservedWords)),
        m_Symbols(ProcessSymbolsArg(symbols)),
        m_NewlineIsToken(symbols.count("\n") != 0),
        m_NewlineSymbol(m_NewlineIsToken ? symbols.at("\n")
                                         : static_cast<Symbol>(0)),
        m_RemainingInput(ProcessInputArg(pInput))
    {
    }

    //----------------------------------------------------------------
    //! \brief Get one token from the Tokenizer
    //!
    //! GetToken(0) returns the next token, GetToken(1) gets the one
    //! after that, and so on.  This method returns a mutable
    //! reference, so that context-dependant tokens can be modified
    //! with Token::ChangeType().
    //!
    //! The reference returned by this method points into an internal
    //! deque.  It becomes invalid if the caller calls Pop() and/or
    //! uses GetToken() to read past the next newline.
    //!
    //! Once the end of file is reached, this method acts as if the
    //! input contained an infinite number of END_OF_FILE tokens.
    //!
    Token& Tokenizer::GetToken(size_t idx)
    {
        // Parse lines until m_Tokens has at least idx+1 tokens, or
        // until we reach EOF.
        //
        while (idx >= m_Tokens.size() && m_pEofToken == nullptr)
        {
            ParseLine();
        }

        // Return the token at index idx, or EOF
        //
        return (idx < m_Tokens.size()) ? m_Tokens[idx] : *m_pEofToken;
    }

    //----------------------------------------------------------------
    //! \brief Remove tokens from the start of the input
    //!
    //! Like GetToken(), this method acts as if the input ends with an
    //! infinite number of END_OF_FILE tokens, so it's OK to call
    //! Pop() even if we've reached EOF.
    //!
    void Tokenizer::Pop(size_t numTokens)
    {
        // Parse lines until m_Tokens has at least numTokens tokens,
        // or until we reach EOF.
        //
        while (m_Tokens.size() < numTokens && m_pEofToken == nullptr)
        {
            ParseLine();
        }

        // Remove tokens from m_Tokens until we remove numTokens
        // or reach EOF.
        //
        while (numTokens > 0 && !m_Tokens.empty())
        {
            m_Tokens.pop_front();
            --numTokens;
        }
    }

    //----------------------------------------------------------------
    //! \brief Process the pInput arg passed to the constructor
    //!
    //! The code was easier if the input always ends in a newline, so
    //! this method appends one if needed and then returns the input
    //! as a string_view.  The caller must not modify the input after
    //! this point, or it will corrupt the string_views.
    //!
    string_view Tokenizer::ProcessInputArg(vector<char>* pInput)
    {
        if (pInput->back() != '\n')
        {
            pInput->push_back('\n');
        }
        return string_view(pInput->data(), pInput->size());
    }

    //----------------------------------------------------------------
    //! \brief Process the reservedWords arg passed to the constructor
    //!
    //! This method verifies that all keys are alphanumeric
    //! identifiers with no duplication, and returns a map in which
    //! the keys have all been colwerted to uppercase.  The uppercase
    //! colwersion makes it easier to use case-insensitive keywords.
    //!
    unordered_map<string, ReservedWord> Tokenizer::ProcessReservedWordsArg
    (
        const unordered_map<string, ReservedWord>& reservedWords
    )
    {
        unordered_map<string, ReservedWord> output;
        for (const auto& iter: reservedWords)
        {
            const string word        = ToUpper(iter.first);
            const ReservedWord value = iter.second;

            // Sanity check to make sure word is a valid reserved word
            // with no duplicates
            LWDASSERT(output.count(word) == 0);
            LWDASSERT(word.size() > 0);
            LWDASSERT(isalpha(word[0]) || word[0] == '_');
#ifdef DEBUG
            for (char ch: word)
            {
                LWDASSERT(ch == '_' || isalnum(ch));
            }
#endif
            output[word] = value;
        }
        return output;
    }

    //----------------------------------------------------------------
    //! \brief Process the symbols arg passed to the constructor
    //!
    //! This method verifies that all keys consist of a string of
    //! punctuation characters with no duplication, and organizes them
    //! into a vector of "SizedSymbols", in which each "SizedSymbols"
    //! object contains a subset of the "symbols" map in which each
    //! key has the same size.  For example, if some symbols are 1
    //! character long and others are 2 characters long, the first
    //! SizedSymbol in the vector will contain all the 2-character
    //! symbols and the next will contain all the 1-character symbols.
    //!
    //! The SizedSymbols vector allows us to check for longer matching
    //! symbols before checking for shorter ones during parsing.  For
    //! example, if "+" and "++" are both symbols and we see "++" in
    //! the input, we'll parse it as a single "++" symbol rather than
    //! two "+" symbols because "++" is longer.
    //!
    vector<Tokenizer::SizedSymbols> Tokenizer::ProcessSymbolsArg
    (
        const unordered_map<string, Symbol>& symbols
    )
    {
        // Verify that each key is a valid symbol with no duplication,
        // and organize them into a map of SizedSymbols.  This is a
        // map<> instead of a unordered_map<> so that we can retrieve
        // them in sorted order later.
        //
        map<string::size_type, SizedSymbols> outputMap;
        for (const auto& iter: symbols)
        {
            const string& symbol = iter.first;
            const Symbol  value  = iter.second;
            auto& sizedSymbols   = outputMap[symbol.size()];

            // Sanity check to make sure symbol consists of valid
            // punctuation characters with no duplicates
            //
            LWDASSERT(sizedSymbols.symbols.count(symbol) == 0);
            LWDASSERT(symbol.size() > 0);
            LWDASSERT(symbol[0] != '_');
            if (symbol != "\n")
            {
#ifdef DEBUG
                for (char ch: symbol)
                {
                    LWDASSERT(ispunct(ch) && ch != '_');
                }
#endif
            }
            sizedSymbols.size            = symbol.size();
            sizedSymbols.symbols[symbol] = value;
        }

        // Move the SizedSymbols to a vector in reverse order by size
        //
        vector<SizedSymbols> output;
        output.reserve(outputMap.size());
        for (auto iter = outputMap.rbegin(); iter != outputMap.rend(); ++iter)
        {
            output.push_back(move(iter->second));
        }
        return output;
    }

    //----------------------------------------------------------------
    //! \brief Parse one line, and push the tokens onto m_Tokens
    //!
    //! Called by GetToken() and Pop() if there aren't enough tokens
    //! to satisfy the request.
    //!
    void Tokenizer::ParseLine()
    {
        // If we're out of input, then set m_pEofToken to an
        // END_OF_FILE token.  This tells GetToken() & Pop() not to
        // call us anymore, and gives GetToken() something to return
        // if the user requests a token past the end of file.
        //
        if (m_RemainingInput.empty())
        {
            if (m_pEofToken == nullptr)
            {
                m_pEofToken = make_unique<Token>(Token::END_OF_FILE, 0,
                                                 m_Filename, "",
                                                 m_LineNumber, 0, 0);

            }
            return;
        }

        // Find the next line, which is the portion of
        // m_RemainingInput up to (but not including) the next newline
        //
        const string_view line(m_RemainingInput.data(),
                               m_RemainingInput.find('\n'));

        if (line.substr(0, 6) == "#line ")
        {
            // If the line starts with "#line ", then parse the #line
            // directive to update m_Filename and m_LineNumber
            //
            ParseHashLine(line);
        }
        else
        {
            // Loop through the line, appending tokens to m_Tokens
            //
            for (string_view::size_type pos = 0; pos < line.size();)
            {
                const char ch = line[pos];
                if (isblank(ch))
                {
                    // Ignore whitespace
                    ++pos;
                }
                else if (isalpha(ch) || ch == '_')
                {
                    // Parse an IDENTIFIER or RESERVED_WORD
                    ParseIdentifier(line, pos);
                    pos += m_Tokens.back().GetText().size();
                }
                else if (isdigit(ch))
                {
                    // Parse an INTEGER
                    ParseInteger(line, pos);
                    pos += m_Tokens.back().GetText().size();
                }
                else
                {
                    // Parse a SYMBOL
                    ParseSymbol(line, pos);
                    pos += m_Tokens.back().GetText().size();
                }
            }

            // If a newline is treated as a token, then append a
            // newline token to m_Tokens
            //
            if (m_NewlineIsToken)
            {
                PushToken(Token::SYMBOL, m_NewlineSymbol,
                          line, line.size(), 1);
            }
        }

        // Remove the line (including terminating newline) from the
        // start of m_Remaining Input, and increment the line number
        //
        m_RemainingInput.remove_prefix(line.size() + 1);
        ++m_LineNumber;
    }

    //----------------------------------------------------------------
    //! \brief Parse a #line directive
    //!
    //! Parse a line of the form '#line <line_number> "<filename>"',
    //! and update the m_Filename and m_LineNumber.
    //!
    //! This method does not generate any new Tokens itself, unless
    //! it finds an error in the #line directive, in which case it
    //! adds the entire line as an ILLEGAL token.
    //!
    void Tokenizer::ParseHashLine(const string_view& line)
    {
        bool error = false;
        string_view::size_type pos = 6; // Current position in line, starting
                                        // after the initial "#line "

        // Parse the line number
        //
        char *endp;
        const unsigned long lineNumber = strtoul(line.data() + pos, &endp, 10);
        pos = endp - line.data();

        // Start the filename after the first double-quote
        //
        error |= (line.substr(pos, 2) != " \"");
        pos += 2;

        // End the filename at the second double-quote, ignoring any
        // escaped double-quotes.
        //
        string_view::size_type endPos = pos;
        while (endPos < line.size())
        {
            if (line[endPos] == '"')
                break;
            else if (line[endPos] == '\\')
                endPos += 2;
            else
                ++endPos;
        }
        if (endPos >= line.size())
        {
            error = true;
        }

        // Append an ILLEGAL token on error, or update m_Filename and
        // m_LineNumber on success
        //
        if (error)
        {
            PushToken(Token::ILLEGAL, 0, line, 0, line.size());
            Printf(PriError, "%s: Bad #line directive:\n",
                   m_Tokens.back().GetFileLine().c_str());
            Printf(PriError, "%s\n", ToString(line).c_str());
            Printf(PriError, "\n");
        }
        else
        {
            m_LineNumber = lineNumber - 1;
            m_Filename = line.substr(pos, endPos - pos);
        }
    }

    //----------------------------------------------------------------
    //! \brief Parse an IDENTIFIER or RESERVED_WORD
    //!
    //! \param line The line lwrrently being parsed, without terminating '\n'
    //! \param pos The position in "line" where the identifier starts
    //!
    void Tokenizer::ParseIdentifier
    (
        const string_view& line,
        string_view::size_type pos
    )
    {
        // Find where the identifier ends
        //
        string_view::size_type endPos = pos + 1;
        while (endPos < line.size() &&
               (isalnum(line[endPos]) || line[endPos] == '_'))
        {
            ++endPos;
        }

        // Check whether the identifier appears in the m_ReservedWords
        // map to determine whether it is an IDENTIFIER or
        // RESERVED_WORD.
        //
        const string word = ToString(line.substr(pos, endPos - pos));
        const auto pReservedWord = m_ReservedWords.find(ToUpper(word));
        if (pReservedWord == m_ReservedWords.end())
        {
            PushToken(Token::IDENTIFIER, 0, line, pos, endPos - pos);
        }
        else
        {
            PushToken(Token::RESERVED_WORD, pReservedWord->second,
                      line, pos, endPos - pos);
        }
    }

    //----------------------------------------------------------------
    //! \brief Parse an INTEGER
    //!
    //! Parse an integer literal.  An integer can start with an
    //! optional Python3 base specifier: 0b for binary, 0o for octal,
    //! and 0x for hexadecimal.  Integers are case-insensitive.  The
    //! tokenizer does not support signed integers; the caller must
    //! use a unary "+" or "-" for that.
    //!
    //! \param line The line lwrrently being parsed, without terminating '\n'
    //! \param pos The position in "line" where the integer starts
    //!
    void Tokenizer::ParseInteger
    (
        const string_view& line,
        string_view::size_type pos
    )
    {
        // Find where the integer ends, treating it as a series of
        // alphanumeric characters for now.
        //
        string_view::size_type endPos = pos;
        while (endPos < line.size() && isalnum(line[endPos]))
        {
            ++endPos;
        }

        // Add an ILLEGAL token for the integer.  The rationale for
        // adding an ILLEGAL token so early is that there are many
        // ways for an integer to be malformed.  By adding the ILLEGAL
        // token right now, we can simply return if we find an
        // problem.  If all goes well, we'll colwert it to an
        // INTEGER at the end of the method.
        //
        PushToken(Token::ILLEGAL, 0, line, pos, endPos - pos);
        Token* pToken = &m_Tokens.back();

        string_view text = pToken->GetText(); // Remaining chars in the integer

        // Process the base specifier (e.g. 0x), if any
        //
        unsigned base = 10;
        if (text.size() > 1 && text[0] == '0')
        {
            switch (text[1])
            {
                case 'b':
                case 'B':
                    base = 2;
                    text.remove_prefix(2);
                    break;
                case 'o':
                case 'O':
                    base = 8;
                    text.remove_prefix(2);
                    break;
                case 'x':
                case 'X':
                    base = 16;
                    text.remove_prefix(2);
                    break;
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                    if (m_pProgram->GetFlag(Flag::WARN_OCTAL))
                    {
                        Printf(PriWarn,
                               "%s: Use 0o to identify an octal number:\n",
                               pToken->GetFileLine().c_str());
                        pToken->PrintUnderlined(PriWarn);
                        if (m_pProgram->ReportWarning() != OK)
                        {
                            return;
                        }
                    }
                    break;
            }
        }

        // It's an ILLEGAL Token if there's nothing after the base
        // specifier
        //
        if (text.empty())
        {
            Printf(PriError, "%s: Incomplete integer:\n",
                   pToken->GetFileLine().c_str());
            pToken->PrintUnderlined(PriError);
            Printf(PriError, "\n");
            return;
        }

        // Process the digits
        //
        UINT64 num = 0;
        for (char ch: text)
        {
            const unsigned digit = (isdigit(ch) ? ch - '0' :
                                    islower(ch) ? ch - 'a' + 10 :
                                                  ch - 'A' + 10);
            if (digit >= base)
            {
                if (base == 10)
                {
                    Printf(PriError, "%s: Illegal character '%c' in integer:\n",
                           pToken->GetFileLine().c_str(), ch);
                }
                else
                {
                    Printf(PriError,
                           "%s: Illegal character '%c' in base-%d integer:\n",
                           pToken->GetFileLine().c_str(), ch, base);
                }
                pToken->PrintUnderlined(PriError);
                Printf(PriError, "\n");
                return;
            }
            const UINT64 newNum = num * base + digit;
            if (newNum < num)
            {
                Printf(PriError, "%s: Integer overflowed:\n",
                       pToken->GetFileLine().c_str());
                pToken->PrintUnderlined(PriError);
                Printf(PriError, "\n");
                return;
            }
            num = newNum;
        }

        // Colwert the Token to an INTEGER
        //
        pToken->ChangeType(Token::INTEGER, num);
    }

    //----------------------------------------------------------------
    //! \brief Parse a SYMBOL
    //!
    //! \param line The line lwrrently being parsed, without terminating '\n'
    //! \param pos The position in "line" where the integer starts
    //!
    void Tokenizer::ParseSymbol
    (
        const string_view& line,
        string_view::size_type pos
    )
    {
        // Search m_Symbols for a matching symbol, starting with the
        // longest ones.  If we find one, append the Token and return.
        //
        for (const auto& symbols: m_Symbols)
        {
            if (symbols.size <= line.size())
            {
                const auto pSymbol = symbols.symbols.find(
                        ToString(line.substr(pos, symbols.size)));
                if (pSymbol != symbols.symbols.end())
                {
                    PushToken(Token::SYMBOL, pSymbol->second,
                              line, pos, symbols.size);
                    return;
                }
            }
        }

        // If we get here, then we didn't find a match.  Combine all
        // punctuation characters into an ILLEGAL token.
        //
        string_view::size_type endPos = pos + 1;
        while (endPos < line.size() && ispunct(line[endPos]))
        {
            ++endPos;
        }
        PushToken(Token::ILLEGAL, 0, line, pos, endPos - pos);
        Printf(PriError, "%s: Unknown symbol \"%s\":\n",
               m_Tokens.back().GetFileLine().c_str(),
               ToString(m_Tokens.back().GetText()).c_str());
        m_Tokens.back().PrintUnderlined(PriError);
        Printf(PriError, "\n");
    }

    //----------------------------------------------------------------
    //! \brief Push a Token onto the end of m_Tokens
    //!
    void Tokenizer::PushToken
    (
        Token::Type            type,
        INT64                  value,
        const string_view&     line,
        string_view::size_type pos,
        string_view::size_type size
    )
    {
        m_Tokens.emplace_back(type, value, m_Filename,
                              line, m_LineNumber, pos, size);
    }
}
