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
#ifndef _NODES_H_
#define _NODES_H_

class EvalElw;

class ScriptNode
{
    friend class Function;
public:
    virtual void Evaluate( EvalElw &E )  { }
    bool ExistingSymbol( const EvalElw &E, const string &symbol );
    void SetSymbolValue( EvalElw &E,
                         const string &symbol, const string &value );
    void SetSymbolUntypedValue( EvalElw &E,
                                const string &symbol, const string &value );
    string GetSymbolValue( EvalElw &E, const string &symbol );
    void DumpSymbolTable( EvalElw &E );
    bool IsStringLiteral( const string &s );
    string MakeNumber( string s );
    string IntToTypedValueString( EvalElw &E, int i );
    string ObjectToTypedValueString( EvalElw &E, UINT32 handle );

    enum ValueType
    {
        NUMBER,
        STRING,
        OBJECT,
        NO_TYPE
    };

    virtual string MakeTyped( EvalElw &E, ValueType type, string value );
    virtual bool IsTyped( EvalElw &E, const string &s );
    virtual ValueType GetType( EvalElw &E, const string &value );
    virtual string RemoveType( EvalElw &E, string s );
};

class EvalElw
{
private:
    EvalElw();
public:
    EvalElw( Verif2d *v2d, Lex *lex );
    ~EvalElw();

    Verif2d            *m_v2d;
    Lex                *m_lex;
    map<string,string>  m_symbolTable;

    map<ScriptNode::ValueType,char> m_typeToSymbolMap;
    map<char,ScriptNode::ValueType> m_symbolToTypeMap;
};

class StatementListNode: public ScriptNode
{
public:
    StatementListNode( void ) { m_statements.clear(); }
    virtual ~StatementListNode( void ) { }
    virtual void Evaluate( EvalElw &E );
    virtual void AppendStatement( ScriptNode *node );

private:
    list<ScriptNode *> m_statements;
};

class ExprNode: public ScriptNode
{
    friend class Function;
public:
    ExprNode( void )
        {
            m_value = "";
            m_left = 0;
            m_right = 0;
            m_op = Token::ILWALID_TOKEN;
        }
    virtual ~ExprNode( void ) { }
    virtual string GetValue( void ) { return m_value; }
    virtual string GetEvaluation( void ) { return m_evaluatiolwalue; }
    virtual int GetIntEvaluation( EvalElw &E );
    virtual UINT32 GetObjectEvaluation( EvalElw &E );
    virtual string GetStringEvaluation( EvalElw &E );
    virtual void SetValue( string value ) { m_value = value; }
    virtual void Evaluate( EvalElw &E );
    virtual void SetLeft( ExprNode *left ) { m_left = left; }
    virtual void SetRight( ExprNode *right ) { m_right = right; }
    virtual void SetOp( Token::Type op ) { m_op = op; }
    virtual Token::Type GetOp( void ) { return m_op; }
    virtual ValueType GetEvalType( EvalElw &E );
protected:
    string m_value;
    string m_evaluatiolwalue;
    ExprNode *m_left;
    Token::Type m_op;
    ExprNode *m_right;
};

class AssignmentNode : public ScriptNode
{
public:
    AssignmentNode( void ): m_identifier(""), m_rightHandSide(0) { }
    virtual ~AssignmentNode( void ) { }
    virtual void Evaluate( EvalElw &E );
    virtual void SetIdentifier( string ident ) { m_identifier = ident; }
    virtual void SetRightHandSide( ExprNode *rhs ) { m_rightHandSide = rhs; }

private:
    string m_identifier;
    ExprNode *m_rightHandSide;
};

class ParameterNode: public ScriptNode
{
    friend class Function;
public:
    ParameterNode( void ) : m_tag(0), m_value(0) { }
    virtual ~ParameterNode( void ) { }
    virtual void Evaluate( EvalElw &E );
    virtual void SetTagExpr( ExprNode *e ) { m_tag = e; }
    virtual void SetValueExpr( ExprNode *e ) { m_value = e; }
private:
    ExprNode *m_tag;
    ExprNode *m_value;
};

class ParamListNode: public ScriptNode
{
    friend class Function;
public:
    ParamListNode( void ) { }
    virtual ~ParamListNode( void ) { }
    virtual void AppendParameter( ParameterNode *param ) { m_paramlist.push_back( param ); }
    virtual void Evaluate( EvalElw &E );
private:
    list<ParameterNode *> m_paramlist;
};

class FuncCallNode: public ExprNode
{
    friend class Function;
public:
    FuncCallNode( void ) { m_params = 0; }
    virtual ~FuncCallNode( void ) { }
    virtual void Evaluate( EvalElw &E );
    virtual void SetName( string name ) { m_name = name; }
    virtual void SetParams( ParamListNode *params ) { m_params = params; }
private:
    string m_name;
    ParamListNode *m_params;
};

class ForLoopNode : public ScriptNode
{
public:
    ForLoopNode( void ) { }
    virtual ~ForLoopNode( void ) { }
    virtual void Evaluate( EvalElw &E );
    virtual void SetInitialAssignment( AssignmentNode *initial ) { m_initial = initial; }
    virtual void SetLoopTest( ExprNode *test ) { m_test = test; }
    virtual void SetIterationAssignment( AssignmentNode *iterAssign ) { m_iteration = iterAssign; }
    virtual void SetLoopBody( ScriptNode *body ) { m_body = body; }

  private:
    AssignmentNode *m_initial;
    ExprNode *m_test;
    AssignmentNode *m_iteration;
    ScriptNode *m_body;
};

class ForEachLoopNode : public ScriptNode
{
public:
    ForEachLoopNode( void ) { }
    virtual ~ForEachLoopNode( void ) { }
    virtual void Evaluate( EvalElw &E );
    virtual void SetIdentifier( string identifier ) { m_identifier = identifier; }
    virtual void AppendExpression( ExprNode *n ) { m_expressions.push_back( n ); }
    virtual void SetLoopBody( ScriptNode *body ) { m_body = body; }

  private:
    string m_identifier;
    list<ExprNode *> m_expressions;
    ScriptNode *m_body;
};

class IfNode : public ScriptNode
{
public:
    IfNode( void ) { m_ifBody = 0; m_elseBody = 0; }
    virtual ~IfNode( void ) { }
    virtual void Evaluate( EvalElw &E );
    virtual void SetIfTest( ExprNode *iftest ) { m_test = iftest; }
    virtual void SetIfBody( ScriptNode *body ) { m_ifBody = body; }
    virtual void SetElseBody( ScriptNode *body ) { m_elseBody = body; }

  private:
    ExprNode *m_test;
    ScriptNode *m_ifBody;
    ScriptNode *m_elseBody;
};

#endif // #ifndef _NODES_H_
