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
#ifndef _PARSE_H_
#define _PARSE_H_

class Lex;
class EvalElw;

class Parser
{
    Parser( void );
public:

    Parser( EvalElw &E );
    ScriptNode * Parse( void );

private:
    EvalElw &m_E;
    ScriptNode *m_programNode;
    Token m_lookAhead;

    void Match( Token::Type tokenType );
    bool Check( Token::Type tokenType );
    void Advance( void );
    void SyntaxError( string message );
    void SyntaxError( void );

    // check functions
    bool StatementPending( void );
    bool StatementBlockPending( void );
    bool ExpressionPending( void );
    bool TermPending( void );
    bool FactorPending( void );
    bool BoolUnitPending( void );
    bool CompareUnitPending( void );
    bool BoolOpPending( void );
    bool CompareOpPending( void );
    bool FunctionCallPending( void );
    bool UnaryPending( void );

    // nonterminal rule functions
    ScriptNode * Statement( void );
    ScriptNode * StatementList( void );
    string Identifier( void );
    ExprNode * Expression( void );
    ExprNode * Term( void );
    ExprNode * Factor( void );
    string Number( void );
    Token::Type BoolOp();
    Token::Type CompareOp();
    ExprNode * BoolUnit( void );
    ExprNode * CompareUnit( void );
    AssignmentNode * Assignment( void );
    ForLoopNode * ForLoop( void );
    ForEachLoopNode * ForEachLoop( void );
    IfNode * If( void );
    FuncCallNode * FunctionCall( void );
    ParamListNode * ParamList( void );
    ExprNode * Unary( void );
    ParameterNode * Parameter( void );
};

#endif // _PARSE_H_
