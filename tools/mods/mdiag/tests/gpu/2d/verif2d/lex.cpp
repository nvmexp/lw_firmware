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
#include "mdiag/tests/stdtest.h"

#include "verif2d.h"

Lex::Lex ( void )
{
    m_linenum = 1;
}

void Lex::Init( const string& fileName )
{
    m_fin.open( fileName.c_str() );
    if ( m_fin.good() == false )
    {
        V2D_THROW( "couldn't open script file" << fileName << endl
            << "Failed reason: "
            << Utility::InterpretFileError().Message());
    }

    m_tokenStringMap[ Token::NUMBER ] = "NUMBER";
    m_tokenStringMap[ Token::STRING ] = "STRING";
    m_tokenStringMap[ Token::IDENTIFIER ] = "IDENTIFIER";
    m_tokenStringMap[ Token::OPEN_PAREN ] = "OPEN_PAREN";
    m_tokenStringMap[ Token::CLOSE_PAREN ] = "CLOSE_PAREN";
    m_tokenStringMap[ Token::OPEN_BRACE ] = "OPEN_BRACE";
    m_tokenStringMap[ Token::CLOSE_BRACE ] = "CLOSE_BRACE";
    m_tokenStringMap[ Token::SEMICOLON ] = "SEMICOLON";
    m_tokenStringMap[ Token::EQUAL_SIGN ] = "EQUAL_SIGN";
    m_tokenStringMap[ Token::COMMA ] = "COMMA";
    m_tokenStringMap[ Token::PLUS ] = "PLUS";
    m_tokenStringMap[ Token::MINUS ] = "MINUS";
    m_tokenStringMap[ Token::MULTIPLY ] = "MULTIPLY";
    m_tokenStringMap[ Token::DIVIDE ] = "DIVIDE";
    m_tokenStringMap[ Token::MOD ] = "MOD";
    m_tokenStringMap[ Token::FOR ] = "FOR";
    m_tokenStringMap[ Token::FOREACH ] = "FOREACH";
    m_tokenStringMap[ Token::LESS ] = "LESS";
    m_tokenStringMap[ Token::GREATER ] = "GREATER";
    m_tokenStringMap[ Token::EQUALEQUAL ] = "EQUALEQUAL";
    m_tokenStringMap[ Token::NOTEQUAL ] = "NOTEQUAL";
    m_tokenStringMap[ Token::LESSEQUAL ] = "LESSEQUAL";
    m_tokenStringMap[ Token::GREATEREQUAL ] = "GREATEREQUAL";
    m_tokenStringMap[ Token::ANDAND ] = "ANDAND";
    m_tokenStringMap[ Token::OROR ] = "OROR";
    m_tokenStringMap[ Token::BITAND ] = "BITAND";
    m_tokenStringMap[ Token::BITOR ] = "BITOR";
    m_tokenStringMap[ Token::BITXOR ] = "BITXOR";
    m_tokenStringMap[ Token::LEFTSHIFT] = "LEFTSHIFT";
    m_tokenStringMap[ Token::RIGHTSHIFT ] = "RIGHTSHIFT";

    m_tokenStringMap[ Token::IF ] = "IF";
    m_tokenStringMap[ Token::ELSE ] = "ELSE";
    m_tokenStringMap[ Token::END_OF_INPUT ] = "END_OF_INPUT";

    m_singleCharTokenMap[ '(' ] = Token::OPEN_PAREN;
    m_singleCharTokenMap[ ')' ] = Token::CLOSE_PAREN;
    m_singleCharTokenMap[ '{' ] = Token::OPEN_BRACE;
    m_singleCharTokenMap[ '}' ] = Token::CLOSE_BRACE;
    m_singleCharTokenMap[ ';' ] = Token::SEMICOLON;
    m_singleCharTokenMap[ '=' ] = Token::EQUAL_SIGN;
    m_singleCharTokenMap[ ',' ] = Token::COMMA;
    m_singleCharTokenMap[ '+' ] = Token::PLUS;
    m_singleCharTokenMap[ '-' ] = Token::MINUS;
    m_singleCharTokenMap[ '*' ] = Token::MULTIPLY;
    m_singleCharTokenMap[ '/' ] = Token::DIVIDE;
    m_singleCharTokenMap[ '%' ] = Token::MOD;
    m_singleCharTokenMap[ '<' ] = Token::LESS;
    m_singleCharTokenMap[ '>' ] = Token::GREATER;
    m_singleCharTokenMap[ '&' ] = Token::BITAND;
    m_singleCharTokenMap[ '|' ] = Token::BITOR;
    m_singleCharTokenMap[ '^' ] = Token::BITXOR;
}

string Lex::TokenString( Token::Type type )
{
    return m_tokenStringMap[ type ];
}

void Lex::PrintToken( Token *t )
{
    if ( (t->m_type == Token::IDENTIFIER) || (t->m_type == Token::NUMBER) ||
         (t->m_type == Token::STRING) )
    {
        InfoPrintf( "found token %s (%s) on line number %d\n", (m_tokenStringMap[ t->m_type ]).c_str(),
                    t->m_value.c_str(), m_linenum );
    }
    else
        InfoPrintf( "found token %s on line number %d\n", (m_tokenStringMap[ t->m_type ]).c_str(), m_linenum );
}

void Lex::NextToken( Token *t )
{
    int c;

    m_tokenString = "";
    t->m_value = "";

    while ( m_fin.good() == true )
    {
        SkipWhiteSpace();
        c = m_fin.get();
        if ( c == EOF )
            break;
        if ( c == '$' )
        {
            // identifier: $ <whitespace> stringliteral
            SkipWhiteSpace();
            StringLiteral();
            t->m_type = Token::IDENTIFIER;
            t->m_value = m_tokenString;
            return;
        }
        else if ( isalpha(c) || ( c == '_' ) )
        {
            m_fin.putback( c );
            StringLiteral();
            t->m_value = m_tokenString;
            FindReservedWords( t );
            return;
        }
        else if ( isdigit(c) )
        {
            m_tokenString += (char) c;
            NumericLiteral();
            t->m_value = m_tokenString;
            t->m_type = Token::NUMBER;
            return;
        }
        else if ( DoubleCharToken( c, t ) )
        {
            return;
        }
        else
        {
            // it must be a single-character symbolic token
            SingleCharToken( c, t );
            return;
        }
    }
    t->m_type = Token::END_OF_INPUT;
}

void Lex::SkipWhiteSpace( void )
{
    int c;

    while ( 1 )
    {
        c = m_fin.get();
        if ( c == ' ' || c == '\t' )
            continue;
        else if ( c == '\n' )
        {
            ++ m_linenum;
            continue;
        }
        else if ( c == '#' )
        {
            // comment, read until end of line or until end of input
            while ( (c != EOF) && (c != '\n') )
                c = m_fin.get();
            if ( c == EOF )
                return;
            else
                ++ m_linenum;
        }
        else if ( c == EOF )
            return;
        else
        {
            m_fin.putback( c );
            return;
        }
    }
}

// first character already stored in main NextToken routine
// first character must be a non-digit
// read until a non-alphanumeric is found and store them in the token string

void Lex::StringLiteral( void )
{
    int c;

    c = m_fin.get();
    if ( ! isalpha(c) && ( c != '_' ) )
        V2D_THROW( "syntax error: first character " << (char)c << " of identifier is not a letter, line "
                   << m_linenum );
    m_tokenString += (char) c;

    while ( 1 )
    {
        c = m_fin.get();
        if ( isalpha(c) || isdigit(c) || c == '_' )
            m_tokenString += (char) c;
        else
        {
            if ( c != EOF )
                m_fin.putback( c );
            return;
        }
    }
}

bool Lex::IsHexDigit( char c )
{
    return ( ((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F')) );
}

//
// read in hex number: there must be at least one hex digit, followed by optional
// more hex digits.  The number ends with the first non-legal-hex digit after the first one
// the hex number prefix "0x" has already been read
//
void Lex::HexadecimalNumber( void )
{
    string hexString = "";
    int c;

    c = m_fin.get();

    if ( ! IsHexDigit( (char) c ) )
        V2D_THROW( "syntax error: illegal character " << c << "  after hexadecimal prefix 0x on line " << m_linenum );

    hexString += (char)c;

    while ( 1 )
    {
        c = m_fin.get();
        if ( IsHexDigit( (char) c ) )
            hexString += (char) c;
        else
        {
            if ( c != EOF )
                m_fin.putback( c );
            break;
        }
    }

    // colwert hex string to decimal string
    int value = strtoul( hexString.c_str(), NULL, 16 );
    stringstream ss;
    ss << value;
    m_tokenString = ss.str();
}

// first digit already stored in main NextToken routine
// read up to the next non-numeral and put it in the token string
// check if it's a hexadecimal constant (prefixed by "0x") or a decimal constant
//

void Lex::NumericLiteral( void )
{
    int c;

    if ( m_tokenString[0] == '0' )
    {
        c = m_fin.get();
        if ( c == 'x' )
        {
            HexadecimalNumber();
            return;
        }
        // not a hex number, just a decimal number starting with '0'
        m_fin.putback( c );
    }

    while ( 1 )
    {
        c = m_fin.get();
        if ( isdigit(c) )
            m_tokenString += (char) c;
        else
        {
            if ( c != EOF )
                m_fin.putback( c );
            return;
        }
    }
}

void Lex::FindReservedWords( Token *t )
{
    if ( m_tokenString == "for" )
        t->m_type = Token::FOR;
    else if ( m_tokenString == "foreach" )
        t->m_type = Token::FOREACH;
    else if ( m_tokenString == "if" )
        t->m_type = Token::IF;
    else if ( m_tokenString == "else" )
        t->m_type = Token::ELSE;
    else
        t->m_type = Token::STRING;
}

void Lex::SingleCharToken( char c, Token *t )
{
    map<char, Token::Type>::iterator i;

    i = m_singleCharTokenMap.find( c );
    if ( i == m_singleCharTokenMap.end() )
        V2D_THROW( "syntax error, illegal character " << c << " at line " << m_linenum );

    t->m_type = (*i).second;
    t->m_value += c;
}

bool Lex::DoubleCharToken( char c, Token *t )
{
    int c2 = m_fin.get();

    if ( (c == '<') && (c2 == '=') )
    {
        t->m_type = Token::LESSEQUAL;
        t->m_value = "<=";
        return true;
    }
    else if ( (c == '&') && (c2 == '&') )
    {
        t->m_type = Token::ANDAND;
        t->m_value = "&&";
        return true;
    }
    else if ( (c == '|') && (c2 == '|') )
    {
        t->m_type = Token::OROR;
        t->m_value = "||";
        return true;
    }

    else if ( (c == '>') && (c2 == '=') )
    {
        t->m_type = Token::GREATEREQUAL;
        t->m_value = ">=";
        return true;
    }
    else if ( (c == '>') && (c2 == '>') )
    {
        t->m_type = Token::RIGHTSHIFT;
        t->m_value = ">>";
        return true;
    }
     else if ( (c == '<') && (c2 == '<') )
    {
        t->m_type = Token::LEFTSHIFT;
        t->m_value = "<<";
        return true;
    }

    else if ( (c == '=') && (c2 == '=') )
    {
        t->m_type = Token::EQUALEQUAL;
        t->m_value = "==";
        return true;
    }
    else if ( (c == '!') && (c2 == '=') )
    {
        t->m_type = Token::NOTEQUAL;
        t->m_value = "!=";
        return true;
    }

    if ( c2 != EOF )
        m_fin.putback( c2 );
    return false;
}
