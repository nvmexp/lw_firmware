/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef RM_UTILITY_STRING_H_
#define RM_UTILITY_STRING_H_

#include <cerrno>
#include <string>
#include <list>
#include <map>
#include <sstream>
#include <iomanip>
#include <algorithm>

#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif

#include "ctype.h"
#include "lwmisc.h"

namespace rmt
{
    struct StringList;

    //! \brief      String
    class String: public std::string
    {
    public:

        //! \brief      List of whitespace characters
        static const char * const whitespace;

        //!
        //! \brief      List of ASCII alphanumeric characters
        //!
        //! \details    It is generally assumed that keywords and symbols
        //!             (including domain names) contain only characters from
        //!             this set.
        //!
        static const char * const alnum;

        //! \brief      Construct an empty string
        inline String():                             std::string()          { }

        //! \brief      Construct by promoting a standard string
        inline String(const std::string &s):         std::string(s)         { }

        //! \brief      Construct from a null-terminated character array
        inline String(const char *s):                std::string(s? s: "")  { }

        //! \brief      Construct from a stream
        inline String(const std::stringstream &oss): std::string(oss.str()) { }

        //! \brief      Construct from a stream
        inline String(const std::ostringstream &oss):std::string(oss.str()) { }

        //!
        //! \brief      Cast to a C-style string
        //!
        //! \warning    Pointer escape: Be sure the pointer is not used after this
        //!             string object is modified or destructed.
        //!
        inline operator const char *() const
        {
            return std::string::c_str();
        }

        //! \brief      Concatenation
        inline String operator+(const String &that) const
        {
            return std::operator+(*this, that);
        }

        //! \brief      Concatenation
        inline String operator+(const std::string &that) const
        {
            return std::operator+(*this, that);
        }

        //! \brief      Concatenation
        inline String operator+(const char *that) const
        {
            return std::operator+(*this, that);
        }

        //! \brief      Concatenation
        inline String operator+(char that) const
        {
            return std::operator+(*this, that);
        }

        //! \brief      Length
        inline size_t operator+() const
        {
            return std::string::length();
        }

        //! \brief      Emptiness
        inline bool operator!() const
        {
            return std::string::empty();
        }

        //! \brief      Trim whitespace from the end of the string
        inline void rtrim()
        {
            erase(find_last_not_of(whitespace) + 1);
        }

        //! \brief      Trim whitespace from the start of the string
        inline void ltrim()
        {
            erase(0, find_first_not_of(whitespace));
        }

        //! \brief      Trim whitespace from both the start and end of the string
        inline void trim()
        {
            rtrim();
            ltrim();
        }

        //! \brief      Trim whitespace from both the start and end of the string
        inline String trimq() const
        {
            String s = *this;
            s.rtrim();
            s.ltrim();
            return s;
        }

        //! \brief      String with the same character repeated
        static String repeat(size_t length, char c = ' ')
        {
            String s;
            while (length--)
                s = s + c;
            return s;
        }

        //! \brief      Add characters to the right
        inline String rpad(size_t length, char pad = ' ') const
        {
            return *this + (std::string::length() >= length? String():
                repeat(length - std::string::length(), pad));
        }

        //! \brief      Add characters to the left
        inline String lpad(size_t length, char pad = ' ') const
        {
            return (std::string::length() >= length? String():
                repeat(length - std::string::length(), pad)) +
                *this;
        }

        //! \brief      Is the character not an alphanumeric?
        static inline bool isnotalnum(int c)
        {
            return ! isalnum(c);
        }

        //! \brief      Colwert to lowercase
        inline void lcase()
        {
            transform(begin(), end(), begin(), ::tolower);
        }

        //! \brief      Does this start with the specified prefix?
        inline bool startsWith(String prefix)
        {
            return length() >= prefix.length()
                && substr(0, prefix.length()) == prefix;
        }

        //! \brief      Does this end with the specified suffix?
        inline bool endsWith(String suffix)
        {
            return length() >= suffix.length()
                && substr(length() - suffix.length()) == suffix;
        }

        //! \brief      Trim suffix if it exists on the end of the string
        inline void rtrim(String suffix)
        {
            if (length() >= suffix.length()
                && substr(length() - suffix.length()) == suffix)
            {
                erase(length() - suffix.length());
            }
        }

        //! \brief      Remove all spaces in a string.
        inline void removeSpace()
        {
            erase(remove_if(begin(), end(), ::isspace), end());
        }

        //! \brief      Remove anything that is not alphanumeric.
        inline void keepOnlyAlnum()
        {
            erase(remove_if(begin(), end(), isnotalnum), end());
        }

        //! \brief      Apply default if string is empty
        inline String defaultTo(const char *def) const
        {
            return empty()? (String) def: *this;
        }

        //!
        //! \brief      First character (if any)
        //!
        //! \retval     Null character if string is empty
        //!
        inline char firstChar() const
        {
            return empty()? '\0': at(0);
        }

        //!
        //! \brief      Last character (if any)
        //!
        //! \retval     Null character if string is empty
        //!
        inline char lastChar() const
        {
            return empty()? '\0': at(length() - 1);
        }

        //!
        //! \brief      Extract method name from __PRETTY_FUNCTION__
        //! \see        http://gcc.gnu.org/onlinedocs/gcc/Function-Names.html
        //!
        //! \details    If GCC's __PRETTY_FUNCTION__ is passed as an argument, the
        //!             resulting format is roughly namespace::class::function.
        //!             If the more standard __FUNCTION__ or __func__ is used, then
        //!             this function makes no change to the string content.
        //!
        static String function(const char *function);

        //!
        //! \brief      Get current working directory (pwd)
        //! \see        http://linux.die.net/man/3/getcwd
        //! \see        http://msdn.microsoft.com/en-us/library/sf98bd4y.aspx
        //!
        //! \details    This wrapper for the POSIX 'getcwd' or ISO C++ '_getcwd'
        //!             automatically manages memory allocation to avoid leaks
        //!             and buffer overruns.
        //!
        //! \return             Current (present) working directory
        //! \retval     empty   Unable to get the current working directory
        //!
        static String getcwd();

        //!
        //! \brief      Break string into shards by delimiter.
        //!
        //! \details    Given delimited string, this function returns a list
        //!             of shards which have been trimmed of leading and
        //!             trailing whitespace.
        //!
        void extractShards(StringList& shards, char delimiter = ',') const;

        //!
        //! \brief      Break string into shards by delimiter.
        //!
        //! \details    Given a delimited string, this function returns the next
        //!             shard which has been trimmed of leading and trailing
        //!             whitespace.  That shard and the delimiter are removed
        //!             from the string.
        //!
        String extractShard(char delimiter = ',');

        //!
        //! \brief      Break string into shards by character set.
        //!
        //! \details    Given a string, this function returns the next shard
        //!             composed only of the specified character set.  In effect,
        //!             the string is delimited by any character which is not
        //!             in the character set.  The shard and the prefixed
        //!             delimiters (if any) are removed from the string.
        //!
        //!             By default, this function returns alphanumeric shards,
        //!             which make is suitable for parsing a list of symbols
        //!             (e.g. clock domains).
        //!
        //!             Caller may want to call 'trim' before calling this
        //!             function if it is desirable to disallow whitespace.
        //!
        String extractShardOf(const char *shardCharSet = alnum);

        //!
        //! \brief      Extract next long integer shard
        //!
        //! \details    Assuming this string starts with a valid integer numeric
        //!             (as defined by 'strtol'), this function returns its
        //!             value and removes the text from this string.
        //!
        //!             If everything is proper with the long integer value,
        //!             'errno' is set to zero.  Under no cirlwmstances is
        //!             any prior value of 'errno' preserved.
        //!
        //!             If no such long integer numeric starts the string,
        //!             zero is returned and 'errno' is set to ERANGE.
        //!
        //!             If the numeric would cause an overflow, 'HUGE_VAL' is
        //!             returned and 'errno' is set to ERANGE.
        //!
        //!             Otherwise, the return value and 'errno' value are set
        //!             according to the rules defined by 'strtol'.
        //!
        inline long extractLong()
        {
            char *pEnd;
            const char *pBegin = c_str();

            // Clear any pending error.
            errno = 0;

            // Translate text to a long integer.
            long n = strtol(pBegin, &pEnd, 0);

            // If no text was eaten, then we have an error
            if (pEnd == pBegin)
                errno = ERANGE;

            // Otherwise eat the text that was used for this value.
            else
                assign(pEnd);

            // Done
            return n;
        }

        //!
        //! \brief      Extract next floating-point shard
        //!
        //! \details    Assuming this string starts with a valid floating-point
        //!             numeric (as defined by 'strtod'), this function returns
        //!             its value and removes the text from this string.
        //!
        //!             If everything is proper with the floating-point value,
        //!             'errno' is set to zero.  Under no cirlwmstances is
        //!             any prior value of 'errno' preserved.
        //!
        //!             If no such floating-point numeric starts the string,
        //!             zero is returned and 'errno' is set to ERANGE.
        //!
        //!             If the numeric would cause an overflow, 'HUGE_VAL' is
        //!             returned and 'errno' is set to ERANGE.
        //!
        //!             Otherwise, the return value and 'errno' value are set
        //!             according to the rules defined by 'strtod'.
        //!
        inline double extractDouble()
        {
            char *pEnd;
            const char *pBegin = c_str();

            // Clear any pending error
            errno = 0;

            // Translate text to double.
            double x = strtod(pBegin, &pEnd);

            // If no text was eaten, then we have an error
            if (pEnd == pBegin)
                errno = ERANGE;

            // Otherwise eat the text that was used for this value.
            else
                assign(pEnd);

            // Done
            return x;
        }

        //! \brief      Colwert to decimal string
        inline static String dec(unsigned long value)
        {
            std::ostringstream text;
            text << value;
            return String(text);
        }

        //! \brief      Colwert to decimal string
        inline static String decf(long double value)
        {
            std::ostringstream text;
            text << value;
            return String(text);
        }

        //! \brief      Colwert to decimal string
        static inline String decf(long double value, std::streamsize precision)
        {
            std::ostringstream text;
            text.setf( std::ios::fixed, std::ios::floatfield);
            text.precision(precision);
            text << value;
            return String(text);
        }

        //! \brief      Colwert to hexadecimal string
        inline static String hex(unsigned long value)
        {
            std::ostringstream text("0x");
            text << std::hex << value;
            return String(text);
        }

        //! \brief      Colwert to hexadecimal string with specified width
        inline static String hex(unsigned long value, unsigned short width)
        {
            std::ostringstream text("0x");
            text << std::hex << std::setw(width) << std::setfill('0') << value;
            return String(text);
        }
    };

    //! \brief      List of strings
    struct StringList: public std::list<String>
    {
        //!
        //! \brief      Concatenate into a single string
        //!
        //! \param[in]  delimiter   Optional delimiter to insert between strings
        //!
        inline String concatenate(const char *delimiter= ", ") const
        {
            return concatenate(NULL, delimiter, NULL);
        }

        //!
        //! \brief      Concatenate into a single string
        //!
        //! \param[in]  before      Optional delimiter to put before first string
        //! \param[in]  between     Optional delimiter to insert between strings
        //! \param[in]  after       Optional delimiter to put after last string
        //!
        String concatenate(const char *before, const char *between, const char *after) const;
    };

    //!
    //! \brief      Map from strings to anything.
    //!
    template <class T> struct StringMap: public std::map<String, T>
    {
        typedef typename std::map<String, T>::iterator iterator;

        typedef typename std::map<String, T>::const_iterator const_iterator;

        typedef typename std::map<String, T>::value_type value_type;

        //!
        //! \brief      Initializer of 'const' objects
        //!
        //! \details    This class enables initialization of a const StringMap
        //!             with syntax like this:
        //!
        //!             const StringMap mymap = StringMap::initialize(key,val)(key,val)...(key,val);
        //!
        struct initialize
        {
            //! \brief      Map being initialized,
            StringMap map;

            inline initialize(const String &key, const T &val)
            {
                map[key] = val;
            }

            inline initialize &operator()(const String& key, const T &val)
            {
                map[key] = val;
                return *this;
            }

            inline operator StringMap() const
            {
                return map;
            }
        };

        //! \brief      Insert into this map.
        inline std::pair<iterator,bool> insert(const String &key, const T &value)
        {
            value_type pair(key, value);
            return std::map<String, T>::insert(pair);
        }

        //!
        //! \brief      Find the associated object
        //!
        //! \details    If the key is not found, this function returns a
        //!             copy of the default value.
        //!
        //! \param[in]  key     The key for which to search
        //! \param[in]  def     Default value in case the key is not found.
        //!
        inline T find_else(const String &key, const T &def) const
        {
            const_iterator it = std::map<String, T>::find(key);
            if (it == std::map<String, T>::end())
                return def;
            return it->second;
        }

        //!
        //! \brief      Search for a match
        //!
        inline StringList search(const T &target) const
        {
            StringList list;
            const_iterator it;
            for (it = std::map<String, T>::begin(); it != std::map<String, T>::end(); ++it)
                if (it->second == target)
                    list.push_back(it->first);
            return list;
        }

        //!
        //! \brief      Search for a match
        //!
        inline String search_first(const T &target) const
        {
            const_iterator it;
            for (it = std::map<String, T>::begin(); it != std::map<String, T>::end(); ++it)
                if (it->second == target)
                    return it->first;
            return NULL;
        }
    };

    //!
    //! \brief      Map between bitmaps and strings
    //!
    struct StringBitmap32
    {
        //! \brief      Initializer of 'const' objects
        typedef StringMap<UINT32>::initialize initialize;

        //! \brief      Map from 32 bits to strings
        rmt::String map[32];

        //! \brief      Construction of empty map
        inline StringBitmap32();

        //!
        //! \brief      Construction using bitmapped map
        //!
        //! \details    This constructor can be used to initialize a 'const'
        //!             object from a map between strings and UINT32s.
        //!
        //!             Typically, the UINT32 would have exactly one bit set.
        //!             However, multiple bits are permitted and have the effect
        //!             of mapping several elements to the same string.
        //!             If the UINT32 is zero, then it has no effect.
        //!
        //! \warning    If there is a collision caused by the same on-bit in
        //!             more than one initializer entry, it goes undetected.
        //!
        //! \todo       We would not need this function if all compilers that we
        //!             use support the C99 standard, specifically, the tagged
        //!             array initializers.  The work-around is to use 'initialize'
        //!             to create a string map and let this constructor take it
        //!             from there,
        //!
        StringBitmap32(const initialize &map);

        //!
        //! \brief      Bit associated with this text
        //!
        //! \retval     zero        No match for this name
        //!
        UINT32 bit(const rmt::String &text) const;

        //!
        //! \brief      Text associated with the bitmap
        //!
        //! \param[in]  bitmap      Exactly one 'on' bit
        //!
        inline const rmt::String &text(UINT32 bit) const
        {
            return map[BIT_IDX_32(bit)];
        }

        //!
        //! \brief      Text associated with the bitmap
        //!
        //! \param[in]  bit         Exactly one 'on' bit
        //!
        inline rmt::String &text(UINT32 bit)
        {
            return map[BIT_IDX_32(bit)];
        }

        //!
        //! \brief      All text associated with the bitmap
        //!
        //! \details    This function generates a list that contains each string
        //!             from this map that corresponds to an 'on' bit in 'mask'.
        //!
        //! \param[in]  mask        Bitmap indicating which text to include
        //!
        inline rmt::StringList textList(UINT32 mask) const
        {
            return textListWithSign(mask, 0x00000000, NULL, NULL);
        }

        //!
        //! \brief      All names associated with the bitmap
        //!
        //! \details    This function generates a list that contains each string
        //!             from this map that corresponds to an 'on' bit in 'mask'.
        //!             Each string is prefixed with 'plus' if the corresponding
        //!             bit in 'value' is 'on', or 'minus' otherwise.
        //!
        //! \param[in]  mask        Bitmap indicating which text to include
        //! \param[in]  value       Bitmap indicating which prefix to use
        //! \param[in]  plus        Prefix to use when 'value' bit is 'on'
        //! \param[in]  minus       Prefix to use when 'value' bit is 'off'
        //!
        rmt::StringList textListWithSign(UINT32 mask, UINT32 value,
            const char *plus = "+", const char *minus = "-") const;
    };
};

#endif // RM_UTILITY_STRING_H_

