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

static EvalElw dummy(NULL, NULL);
Parser::Parser( void ) : m_E(dummy)
{
    V2D_THROW( "Parser default constructor called" );
}

Parser::Parser( EvalElw &E ) : m_E(E)
{
}

ScriptNode * Parser::Parse( void )
{
    ScriptNode *node = new ScriptNode;

    if ( m_E.m_v2d->GetScriptDir().size() > 0)
        node->SetSymbolValue( m_E, "ScriptDir",
            node->MakeTyped( m_E, ScriptNode::STRING,
                             m_E.m_v2d->GetScriptDir() ) );

    Advance();

    if ( StatementPending() )
        m_programNode = StatementList();
    else
        SyntaxError( "a v2d program must consist of a list of statements" );

    Match( Token::END_OF_INPUT );

    return m_programNode;
}

void Parser::SyntaxError( void )
{
    SyntaxError("");
}

void Parser::SyntaxError( string message )
{
    ErrPrintf( "v2d: a script syntax error oclwrred at the following token:\n" );
    m_E.m_lex->PrintToken( &m_lookAhead );
    V2D_THROW( message );
}

bool Parser::Check( Token::Type tokenType )
{
    return (m_lookAhead.m_type == tokenType);
}

void Parser::Advance( void )
{
    m_E.m_lex->NextToken( &m_lookAhead );
}

void Parser::Match( Token::Type tokenType )
{
    if ( tokenType != m_lookAhead.m_type )
    {
        stringstream errorMsg;
        errorMsg << "expecting token " << m_E.m_lex->TokenString( tokenType );
        SyntaxError( errorMsg.str() );
    }

    m_E.m_lex->NextToken( &m_lookAhead );
}

//
// <statement list> -> <statement> [ <statement> ]*
//

ScriptNode * Parser::StatementList( void )
{
    StatementListNode *sln = new StatementListNode;
    ScriptNode *node;

    node = Statement();
    sln->AppendStatement( node );

    while ( StatementPending() )
    {
        node = Statement();
        sln->AppendStatement( node );
    }

    return sln;
}

//
// Statement -> <for loop>
//            | <foreach loop>
//            | <identifier> = <expression> ;
//            | <statement block>
//            | <if statement>
//            | <function call> ;

bool Parser::StatementPending( void )
{
    return Check( Token::FOR ) || Check( Token::FOREACH ) || Check( Token::IDENTIFIER ) ||
        Check( Token::IF ) || StatementBlockPending() || FunctionCallPending();
}

//
// statement block -> { <statment list> }
//

bool Parser::StatementBlockPending( void )
{
    return Check( Token::OPEN_BRACE );
}

AssignmentNode * Parser::Assignment( void )
{
    // <identifier> = <expression>
    //
    string ident = Identifier();
    Match( Token::EQUAL_SIGN );
    ExprNode *rhs = Expression();

    AssignmentNode *an = new AssignmentNode;
    an->SetIdentifier( ident );
    an->SetRightHandSide( rhs );
    return an;
}

ScriptNode * Parser::Statement( void )
{
    ScriptNode *node = 0;

    if ( ! StatementPending() && !StatementBlockPending() )
        SyntaxError( "we're in Statement but there's no statement or statement block pending!" );

    if ( Check( Token::IDENTIFIER ) )
    {
        node = Assignment();
        Match( Token::SEMICOLON );
    }
    else if ( Check( Token::OPEN_BRACE ) )
    {
        // <statement block> -> { <statement list> }
        //
        Match( Token::OPEN_BRACE );
        node = StatementList();
        Match( Token::CLOSE_BRACE );
    }
    else if ( Check( Token::FOR ) )
        node = ForLoop();
    else if ( Check( Token::FOREACH ) )
        node = ForEachLoop();
    else if ( Check( Token::IF ) )
        node = If();
    else if ( FunctionCallPending() )
    {
        node = FunctionCall();
        Match( Token::SEMICOLON );
    }
    else
        SyntaxError( "looking for a Statement but no Statement Found" );

    return node;
}

//
// <for loop> -> for ( <assignment> ; <expression> ; <assignment> ) <statement>
//

ForLoopNode * Parser::ForLoop( void )
{
    Match( Token::FOR );
    Match( Token::OPEN_PAREN );
    AssignmentNode *initialAssignment = Assignment();
    Match( Token::SEMICOLON );
    ExprNode * loopTest = Expression();
    Match( Token::SEMICOLON );
    AssignmentNode *iterationAssignment = Assignment();
    Match( Token::CLOSE_PAREN );
    ScriptNode *loopBody = Statement();

    ForLoopNode *fln = new ForLoopNode;
    fln->SetInitialAssignment( initialAssignment );
    fln->SetLoopTest( loopTest );
    fln->SetIterationAssignment( iterationAssignment );
    fln->SetLoopBody( loopBody );

    return fln;
}

//
// <foreach loop> -> foreach <identifier> ( <expression> [, <expression> ]* ) <statement>
//
ForEachLoopNode * Parser::ForEachLoop( void )
{
    ForEachLoopNode *fel = new ForEachLoopNode;
    ExprNode *exprNode;

    Match( Token::FOREACH );
    string ident = Identifier();
    fel->SetIdentifier( ident );
    Match( Token::OPEN_PAREN );
    exprNode = Expression();
    fel->AppendExpression( exprNode );
    while ( Check( Token::COMMA ) )
    {
        Match( Token::COMMA );
        exprNode = Expression();
        fel->AppendExpression( exprNode );
    }
    Match( Token::CLOSE_PAREN );
    ScriptNode *loopBody = Statement();
    fel->SetLoopBody( loopBody );

    return fel;
}

//
// <if statement> -> if ( <expression> ) <statement> [ else <statement> ]
//
IfNode * Parser::If( void )
{
    Match( Token::IF );
    Match( Token::OPEN_PAREN );
    ExprNode *iftest = Expression();
    Match( Token::CLOSE_PAREN );
    ScriptNode *ifBody = Statement();

    IfNode *ifnode = new IfNode;
    ifnode->SetIfTest( iftest );
    ifnode->SetIfBody( ifBody );

    if ( Check( Token::ELSE ) )
    {
        Match( Token::ELSE );
        ScriptNode *elseBody = Statement();
        ifnode->SetElseBody( elseBody );
    }

    return ifnode;
}

string Parser::Identifier( void )
{
    string ident = m_lookAhead.m_value;

    Match( Token::IDENTIFIER );

    return ident;
}

//
// <expresion> -> <boolUnit> [ <boolOp> <boolUnit> ]*
//
// <boolUnit> -> <compareUnit> [ <compareOp> <compareUnit> ]
//
// compareUnit -> <term> [ +|- <term> ]*
//
// <term> -> <unary> [ *|/ <unary> ]*
//
// <unary> -> [ - ] <factor>
//
// <factor> -> ( < expression> )
//           | <identifier>
//           | <number>
//           | <string literal>      to allow string comparisons
//           | <function call>
//

bool Parser::BoolOpPending( void )
{
    return Check( Token::ANDAND ) || Check( Token::OROR ) || Check ( Token::BITAND) || Check(Token::BITOR) || Check(Token::BITXOR) || Check(Token::LEFTSHIFT)|| Check(Token::RIGHTSHIFT);
}

Token::Type Parser::BoolOp( void )
{
    if ( ! BoolOpPending() )
        SyntaxError( "in BoolOp but no BoolOp pending" );
    Token::Type op = m_lookAhead.m_type;
    Advance();
    return op;
}

//
// <expresion> -> <boolUnit> [ <boolOp> <boolUnit> ]*
//

bool Parser::ExpressionPending( void )
{
    return BoolUnitPending();
}

ExprNode * Parser::Expression ( void )
{
    ExprNode *nleft, *nright, *node;

    if ( ! ExpressionPending() )
        SyntaxError( "in Expression but no Expression is pending" );

    nleft = BoolUnit();
    node = nleft;
    while ( BoolOpPending() )
    {
        Token::Type op = BoolOp();
        nright = BoolUnit();
        node = new ExprNode;
        node->SetLeft( nleft );
        node->SetRight( nright );
        node->SetOp( op );
        nleft = node;
    }
    return node;
}

//
// <boolUnit> -> <compareUnit> [ <compareOp> <compareUnit> ]
//

bool Parser::BoolUnitPending( void )
{
    return CompareUnitPending();
}

bool Parser::CompareOpPending( void )
{
    return Check( Token::LESS ) || Check( Token::EQUALEQUAL ) || Check( Token::NOTEQUAL ) ||
        Check( Token::GREATER ) || Check( Token::LESSEQUAL ) || Check( Token::GREATEREQUAL );
}

Token::Type Parser::CompareOp( void )
{
    if ( ! CompareOpPending() )
        SyntaxError( "in CompareOp but no CompareOp pending" );
    Token::Type op = m_lookAhead.m_type;
    Advance();
    return op;
}

ExprNode * Parser::BoolUnit( void )
{
    ExprNode *nleft, *nright, *node;

    if ( ! BoolUnitPending() )
        SyntaxError( "in BoolUnit but no BoolUnit is pending" );

    nleft = CompareUnit();
    node = nleft;
    // only one compare allowed -- compares don't associate
    //
    if ( CompareOpPending() )
    {
        Token::Type op = CompareOp();
        nright = CompareUnit();
        node = new ExprNode;
        node->SetLeft( nleft );
        node->SetRight( nright );
        node->SetOp( op );
        nleft = node;
    }
    return node;
}

// compareUnit -> <term> [ +|- <term> ]*
//

bool Parser::CompareUnitPending( void )
{
    return TermPending();
}

ExprNode * Parser::CompareUnit ( void )
{
    ExprNode *nleft, *nright, *node;

    if ( ! CompareUnitPending() )
        SyntaxError( "in CompareUnit but no CompareUnit is pending" );

    nleft = Term();
    node = nleft;
    while ( Check( Token::PLUS ) || Check( Token::MINUS ) )
    {
        Token::Type op = m_lookAhead.m_type;
        Advance();
        nright = Term();
        node = new ExprNode;
        node->SetLeft( nleft );
        node->SetRight( nright );
        node->SetOp( op );
        nleft = node;
    }
    return node;
}

//
// <term> -> <unary> [ *|/ <unary> ]*
//

bool Parser::TermPending( void )
{
    return UnaryPending();
}

bool Parser::UnaryPending( void )
{
    return Check( Token::MINUS ) ||FactorPending();
}

ExprNode * Parser::Term( void )
{
    ExprNode *nleft, *nright, *node;

    if ( ! TermPending() )
        SyntaxError( "in Term but no Term is pending" );

    nleft = Unary();
    node = nleft;
    while ( Check( Token::MULTIPLY ) ||
            Check( Token::DIVIDE ) ||
            Check( Token::MOD ) )
    {
        Token::Type op = m_lookAhead.m_type;
        Advance();
        nright = Unary();
        node = new ExprNode;
        node->SetLeft( nleft );
        node->SetRight( nright );
        node->SetOp( op );
        nleft = node;
    }
    return node;
}

//
// <unary> -> [ -|++|--|~ ] <factor>
//

ExprNode * Parser::Unary( void )
{
    ExprNode *node;

    if ( Check ( Token::MINUS ) )
    {
        // we cheat here, no reason to create a unary minus operation, just create a "-" expression node with
        // a hard-coded 0 on the left and our parsed expression on the right
        //
        Match( Token::MINUS );
        ExprNode *zero = new ExprNode;
        zero->SetValue( zero->MakeTyped( m_E, ScriptNode::NUMBER, "0" ) );
        zero->SetOp( Token::NUMBER );

        ExprNode *n = new ExprNode;
        n->SetLeft( zero );
        n->SetOp( Token::MINUS );
        ExprNode *n2 = Factor();
        n->SetRight( n2 );
        node = n;
    }
    else
        node = Factor();

    return node;
}

// <factor> -> ( <expression> )
//           | <identifier>
//           | <number>
//           | <string>    HACK to allow string comparisons
//           | <function call>
//

bool Parser::FactorPending( void )
{
    return Check( Token::OPEN_PAREN ) || Check( Token::IDENTIFIER ) || Check( Token::NUMBER ) ||
        Check( Token::STRING ) || FunctionCallPending();
}

ExprNode * Parser::Factor( void )
{
    ExprNode *node = 0;

    if ( ! FactorPending() )
        SyntaxError( "in Factor but no Factor is pending" );

    if ( Check( Token::OPEN_PAREN ) )
    {
        Match( Token::OPEN_PAREN );
        node = Expression();
        Match( Token::CLOSE_PAREN );
    }
    else if ( Check( Token::IDENTIFIER ) )
    {
        node = new ExprNode;
        node->SetOp( Token::IDENTIFIER );
        node->SetValue( Identifier() );
    }
    else if ( Check( Token::NUMBER ) )
    {
        node = new ExprNode;
        node->SetOp( Token::NUMBER );
        node->SetValue( node->MakeTyped( m_E, ScriptNode::NUMBER, Number() ) );
    }
    else if ( Check( Token::STRING ) )
    {
        string stringVal = m_lookAhead.m_value;
        Match( Token::STRING );
        if ( Check( Token::OPEN_PAREN ) )
        {
            // function call
            // <function call> -> <string> ( <paramlist> )
            //
            FuncCallNode *fcn = new FuncCallNode;
            fcn->SetName( stringVal );
            Match( Token::OPEN_PAREN );
            ParamListNode *params = ParamList();
            fcn->SetParams( params );
            Match( Token::CLOSE_PAREN );
            node = fcn;
        }
        else
        {
            node = new ExprNode;
            node->SetOp( Token::STRING );
            node->SetValue( node->MakeTyped( m_E, ScriptNode::STRING, stringVal ) );
        }
    }
    else
        SyntaxError( "in Factor and nothing matched" );

    return node;
}

string Parser::Number( void )
{
    string number = m_lookAhead.m_value;

    Match( Token::NUMBER );

    return number;
}

//
// <function call> -> <string> ( <paramlist> )
//

bool Parser::FunctionCallPending( void )
{
    return Check( Token::STRING );
}

FuncCallNode * Parser::FunctionCall( void )
{
    string funcName = m_lookAhead.m_value;
    Match( Token::STRING );
    Match( Token::OPEN_PAREN );
    ParamListNode *params = ParamList();
    Match( Token::CLOSE_PAREN );

    FuncCallNode *node = new FuncCallNode;
    node->SetName( funcName );
    node->SetParams( params );

    return node;
}

//
// <paramlist> -> <parameter> [, <parameter> ]*
//
ParamListNode * Parser::ParamList( void )
{
    if ( Check( Token::CLOSE_PAREN ) )
        return 0;       // null paramlist

    if ( ! ExpressionPending() )
        SyntaxError( "illegal parameter list" );

    ParamListNode *pln = 0;
    pln = new ParamListNode;
    ParameterNode *pn = Parameter();
    pln->AppendParameter( pn );

    while ( Check( Token::COMMA ) )
    {
        Match( Token:: COMMA );
        pn = Parameter();
        pln->AppendParameter( pn );
    }

    return pln;
}

//
// <parameter> -> [ <expression> = ] <expression>
//

ParameterNode * Parser::Parameter( void )
{
    ExprNode *e1 = Expression();
    ParameterNode *pn = new ParameterNode;

    if ( Check( Token::EQUAL_SIGN ) )
    {
        // e1 is the tag expression
        Match( Token::EQUAL_SIGN );
        ExprNode *e2 = Expression();
        pn->SetTagExpr( e1 );
        pn->SetValueExpr( e2 );
    }
    else
    {
        // there is no tag, e1 is the value expression
        pn->SetValueExpr( e1 );
    }

    return pn;
}
