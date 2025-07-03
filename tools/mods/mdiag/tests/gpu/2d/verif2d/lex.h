/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _LEX_H_
#define _LEX_H_

#include <fstream>

class Token
{
public:

    enum Type
    {
        NUMBER,
        STRING,
        IDENTIFIER,
        OPEN_PAREN,
        CLOSE_PAREN,
        OPEN_BRACE,
        CLOSE_BRACE,
        SEMICOLON,
        EQUAL_SIGN,
        COMMA,
        PLUS,
        MINUS,
        MULTIPLY,
        DIVIDE,
        MOD,
        FOR,
        FOREACH,
        LESS,
        GREATER,
        LESSEQUAL,
        GREATEREQUAL,
        NOTEQUAL,
        EQUALEQUAL,
        ANDAND,
        OROR,
        BITAND,
        BITOR,
        BITXOR,
        LEFTSHIFT,
        RIGHTSHIFT,
        IF,
        ELSE,
        END_OF_INPUT,
        ILWALID_TOKEN
    };

    Type m_type;
    string m_value;
};

class Lex
{
public:
    Lex( void );
    void Init ( const string& fileName );
    void NextToken( Token *t );
    void PrintToken( Token *t );
    string TokenString( Token::Type type );    // return the string describing the token type

private:
    ifstream m_fin;     // input file stream to break into tokens
    int m_linenum;
    map<Token::Type, string> m_tokenStringMap;
    map<char, Token::Type> m_singleCharTokenMap;
    void SkipWhiteSpace( void );
    void StringLiteral( void );
    void NumericLiteral( void );
    void FindReservedWords( Token *t );
    void SingleCharToken( char c, Token *t );
    bool DoubleCharToken( char c, Token *t );
    bool IsHexDigit( char c );
    void HexadecimalNumber( void );

    string m_tokenString;
};

#endif // _LEX_H_
