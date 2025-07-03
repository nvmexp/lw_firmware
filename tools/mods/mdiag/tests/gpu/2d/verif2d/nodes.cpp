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

EvalElw::EvalElw( void )
{
    V2D_THROW( "EvalElw default constructor called" );
}

EvalElw::EvalElw( Verif2d *v2d, Lex *lex )
{
    m_v2d = v2d;
    m_lex = lex;

    m_typeToSymbolMap[ ScriptNode::NUMBER ] = '#';
    m_typeToSymbolMap[ ScriptNode::STRING ] = '$';
    m_typeToSymbolMap[ ScriptNode::OBJECT ] = '@';

    m_symbolToTypeMap[ '#' ] = ScriptNode::NUMBER;
    m_symbolToTypeMap[ '$' ] = ScriptNode::STRING;
    m_symbolToTypeMap[ '@' ] = ScriptNode::OBJECT;
}

EvalElw::~EvalElw()
{
}

//
// value strings can be "untyped" or "typed".   "untyped" means it contains no type symbol,
// "typed" means it contains a type symbol.    A "typed" value string consists of a single
// "type" character prepended onto the value string.  E.g., "#123" is the number 123,
// "$abc" is the string "abc"
//

bool ScriptNode::IsTyped( EvalElw &E, const string &s )
{
    if ( s.length() == 0 )
        V2D_THROW( "trying to check the type  of a zero-length value string" );

    map<char,ValueType>::iterator i = E.m_symbolToTypeMap.find( s[0] );
    if ( i == E. m_symbolToTypeMap.end() )
        return false;

    return true;
}

ScriptNode::ValueType ScriptNode::GetType( EvalElw &E, const string &value )
{
    if ( value.length() == 0 )
        V2D_THROW( "trying to find the value of a zero-length value string" );

    map<char,ValueType>::iterator i = E.m_symbolToTypeMap.find( value[0] );
    if ( i == E.m_symbolToTypeMap.end() )
        V2D_THROW( "tried to get the type of the untyped value string " << value );

    return (*i).second;
}

ScriptNode::ValueType ExprNode::GetEvalType( EvalElw &E )
{
    return GetType( E, m_evaluatiolwalue );
}

string ScriptNode::MakeTyped( EvalElw &E, ValueType type, string s )
{
    // check if the value is not already typed.  If it is already typed and the type matches
    // the desired type "type", then do nothing.  If it is already typed and the existing type
    // does NOT match the desired type, that's an illegal colwersion and we throw an exception
    //
    if ( IsTyped(E, s) )
    {
        if ( GetType(E, s) == type )
            return s;
        else
            V2D_THROW( "illegal type colwersion of value " << s << " into type " << type );
    }

    return E.m_typeToSymbolMap[ type ] + s;
}

string ScriptNode::RemoveType( EvalElw &E, string s )
{
    if ( ! IsTyped( E, s ) )
        V2D_THROW( "internal error: trying to remove the type of an untyped value string: " << s );

    int len = s.length();
    if ( len == 1 )
        V2D_THROW( "internal error: trying to remove the type of a length 1 value string:" << s );

    const char * result = s.c_str()+1;
    return result;
}

bool ScriptNode::ExistingSymbol( const EvalElw &E, const string &symbol )
{
    return E.m_symbolTable.find( symbol ) != E.m_symbolTable.end();
}

void ScriptNode::SetSymbolValue( EvalElw &E,
                                 const string &symbol, const string &value )
{
    bool exists = false;
    ValueType lwrrentType = NO_TYPE;
    ValueType newType = NO_TYPE;
    string lwrrentValue;

    if ( ExistingSymbol( E, symbol ) )
    {
        exists = true;
        lwrrentValue = GetSymbolValue( E, symbol );
        lwrrentType = GetType( E, lwrrentValue );
    }

    newType = GetType( E, value );

    if ( exists && (newType != lwrrentType) )
        V2D_THROW( "changing type of existing identifier in assignment: identifier = " << symbol <<
                   ", current value = " << lwrrentValue << ", new value = " << value );

    E.m_symbolTable[ symbol ] = value;
}

string ScriptNode::GetSymbolValue( EvalElw &E, const string &symbol )
{
    if ( E.m_symbolTable.find( symbol ) == E.m_symbolTable.end() )
        V2D_THROW( "GetSymbolValue: symbol " << symbol <<
                   " is not in the symbol table" );

    return E.m_symbolTable[symbol];
}

//
// for setting parameters on command line (without going through parser):
//
void ScriptNode::SetSymbolUntypedValue( EvalElw &E,
                                        const string &symbol, const string &value )
{
    string typedValue;

    if ( IsStringLiteral( value ) )
        typedValue += E.m_typeToSymbolMap[ ScriptNode::STRING ];
    else
        typedValue += E.m_typeToSymbolMap[ ScriptNode::NUMBER ];

    typedValue += value;

    SetSymbolValue( E, symbol, typedValue );
}

void StatementListNode::AppendStatement( ScriptNode *node )
{
    m_statements.push_back( node );
}

void StatementListNode::Evaluate( EvalElw &E )
{
    list<ScriptNode *>::iterator i;

    for ( i = m_statements.begin(); i != m_statements.end(); i++ )
        (*i)->Evaluate( E );
}

void AssignmentNode::Evaluate( EvalElw &E )
{
    m_rightHandSide->Evaluate( E );
    string newValue = m_rightHandSide->GetEvaluation();

    SetSymbolValue( E, m_identifier, newValue );
}

void ScriptNode::DumpSymbolTable( EvalElw &E )
{
    map<string,string>::iterator i;

    InfoPrintf( "--------Symbol Table Dump--------\n" );

    for ( i = E.m_symbolTable.begin(); i != E.m_symbolTable.end(); i++ )
        InfoPrintf( "Symbol %s has value %s\n", ((*i).first).c_str(), ((*i).second).c_str() );
}

bool ScriptNode::IsStringLiteral( const string &s )
{
    if ( s.length() == 0 )
        V2D_THROW( "ScriptNode::IsStringLiteral: string has zero length" );

    for(UINT32 i = 0; i < s.length(); i ++)
    {
        if(isalpha( s[i] ) || ( s[i] == '_' ))
            return true;
    }

    return false;
}

int ExprNode::GetIntEvaluation( EvalElw &E )
{
    // throw an exception if the evaluation string is not a number type
    //
    if ( GetEvalType(E) != ScriptNode::NUMBER )
        V2D_THROW( "trying to get integer value of non-integral value string: " << m_evaluatiolwalue );

    // form a new string from the evaluation string by removing the first character (the "type" string)
    //
    string intValString = RemoveType( E, m_evaluatiolwalue );

    int intVal = atoi( intValString.c_str() );
    return intVal;
}

UINT32 ExprNode::GetObjectEvaluation( EvalElw &E )
{
    // throw an exception if the evaluation string is not an object type
    //
    if ( GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "trying to get object value of non-object value string: " << m_evaluatiolwalue );

    // form a new string from the evaluation string by removing the first character (the "type" string)
    //
    string objValString = RemoveType( E, m_evaluatiolwalue );

    UINT32 objVal = strtoul( objValString.c_str(), NULL, 0 );
    return objVal;
}

string ExprNode::GetStringEvaluation( EvalElw &E )
{
    // throw an exception if the evaluation string is not a string type
    //
    if ( GetEvalType(E) != ScriptNode::STRING )
        V2D_THROW( "trying to get string value of non-stirng value string: " << m_evaluatiolwalue );

    // form a new string from the evaluation string by removing the first character (the "type" string)
    //
    string objValString = RemoveType( E, m_evaluatiolwalue );
    return objValString;
}

void ExprNode::Evaluate( EvalElw &E )
{
    if ( m_op == Token::IDENTIFIER )
    {
        string symbolValue = GetSymbolValue( E, m_value );
        m_evaluatiolwalue = symbolValue;
    }
    else if ( ( m_op == Token::NUMBER ) || ( m_op == Token::STRING ) )
    {
        if ( !IsTyped( E, m_value ) )
            V2D_THROW( "internal error: encountered untyped terminal number or string: " << m_value );

        m_evaluatiolwalue = m_value;
    }
    else
    {
        m_left->Evaluate( E );
        m_right->Evaluate( E );

        // operator + can work on strings too:
        //
        if ( m_op == Token::PLUS &&
                ( m_left->GetEvalType(E) == STRING ||
                  m_right->GetEvalType(E) == STRING ) )
        {
            if ( ( m_left->GetEvalType(E) == STRING ||
                   m_left->GetEvalType(E) == NUMBER ) &&
                 ( m_right->GetEvalType(E) == STRING ||
                   m_right->GetEvalType(E) == NUMBER ) )
            {
                m_evaluatiolwalue = string("")
                    + E.m_typeToSymbolMap[ ScriptNode::STRING ]
                    + m_left->GetEvaluation().substr(
                        1, m_left->GetEvaluation().size() - 1 )
                    + m_right->GetEvaluation().substr(
                        1, m_right->GetEvaluation().size() - 1 );
            }
            else
            {
                V2D_THROW( "illegal combination of types, operation + " <<
                           E.m_lex->TokenString(m_op) << ", left = " <<
                           m_left->GetEvaluation() << ", right = " <<
                           m_right->GetEvaluation() );
            }
        }

        // check that both left and right have equal types
        //
        else if ( m_left->GetEvalType(E) != m_right->GetEvalType(E) )
        {
            V2D_THROW( "illegal combination of types, operation = " <<
                       E.m_lex->TokenString(m_op) << ", left = " <<
                       m_left->GetEvaluation() << ", right = " <<
                       m_right->GetEvaluation() );
        }

        // check for operations allowed on both strings and integers:
        //
        else if ( m_op == Token::EQUALEQUAL )
        {
            m_evaluatiolwalue = IntToTypedValueString( E,
                m_left->GetEvaluation() == m_right->GetEvaluation() );
        }
        else if ( m_op == Token::NOTEQUAL )
        {
            m_evaluatiolwalue = IntToTypedValueString( E,
                m_left->GetEvaluation() != m_right->GetEvaluation() );
        }

        //
        // otherwise only numerical expressions are allowed
        //
        else
        {
            int leftIntVal = m_left->GetIntEvaluation(E);
            int rightIntVal = m_right->GetIntEvaluation(E);
            int intVal;

            switch ( m_op )
            {
             case Token::PLUS:
                intVal = leftIntVal + rightIntVal;
                break;
             case Token::MINUS:
                intVal = leftIntVal - rightIntVal;
                break;
             case Token::MULTIPLY:
                intVal = leftIntVal * rightIntVal;
                break;
             case Token::DIVIDE:
                intVal = leftIntVal / rightIntVal;
                if (rightIntVal == 0)
                    V2D_THROW( "divide by zero" );
                break;
             case Token::MOD:
                intVal = leftIntVal % rightIntVal;
                break;
             case Token::LESS:
                intVal = leftIntVal < rightIntVal;
                break;
             case Token::GREATER:
                intVal = leftIntVal > rightIntVal;
                break;
             case Token::LESSEQUAL:
                intVal = leftIntVal <= rightIntVal;
                break;
             case Token::GREATEREQUAL:
                intVal = leftIntVal >= rightIntVal;
                break;
             case Token::ANDAND:
                intVal = leftIntVal && rightIntVal;
                break;
             case Token::OROR:
                intVal = leftIntVal || rightIntVal;
                break;
             case Token::BITAND:
                intVal = leftIntVal & rightIntVal;
                break;
             case Token::BITOR:
                intVal = leftIntVal | rightIntVal;
                break;
             case Token::BITXOR:
                intVal = leftIntVal ^ rightIntVal;
                break;
             case Token::LEFTSHIFT:
                intVal = leftIntVal <<rightIntVal;
                break;
             case Token::RIGHTSHIFT:
                intVal = leftIntVal >> rightIntVal;
                break;

             case Token::EQUALEQUAL:
             case Token::NOTEQUAL:
                V2D_THROW( "internal error: == and != should have been handled in another place" );
                break;

             default:
                V2D_THROW( "illegal operation in ExprNode: " <<  m_op );
            }

            m_evaluatiolwalue = IntToTypedValueString( E, intVal );
        }
    }
}

void ForLoopNode::Evaluate( EvalElw &E )
{
    m_initial->Evaluate( E );
    m_test->Evaluate( E );
    int loopTest = m_test->GetIntEvaluation(E);
    while ( loopTest != 0 )
    {
        m_body->Evaluate( E );
        m_iteration->Evaluate( E );
        m_test->Evaluate( E );
        loopTest = m_test->GetIntEvaluation(E);
    }
}

void ForEachLoopNode::Evaluate( EvalElw &E )
{
    list<ExprNode *>::iterator i;
    string value;

    for ( i = m_expressions.begin(); i != m_expressions.end(); i++ )
    {
        (*i)->Evaluate( E );
        value = (*i)->GetEvaluation();
        SetSymbolValue( E, m_identifier, value );
        m_body->Evaluate( E );
    }
}

void IfNode::Evaluate( EvalElw &E )
{
    m_test->Evaluate( E );
    int intVal = m_test->GetIntEvaluation( E );
    if ( intVal )
        m_ifBody->Evaluate( E );
    else if ( m_elseBody != 0 )
        m_elseBody->Evaluate( E );
}

void FuncCallNode::Evaluate( EvalElw &E )
{
    if ( m_params )
        m_params->Evaluate( E );

    E.m_v2d->GetFunctionTranslator()->Call( E, this );
}

void ParameterNode::Evaluate( EvalElw &E )
{
    if ( m_tag )
        m_tag->Evaluate( E );

    m_value->Evaluate( E );
}

void ParamListNode::Evaluate( EvalElw &E )
{
    list<ParameterNode *>::iterator i;

    for ( i = m_paramlist.begin(); i != m_paramlist.end(); i++ )
        (*i)->Evaluate( E );
}

string ScriptNode::IntToTypedValueString( EvalElw &E, int i )
{
    stringstream ss;
    ss << i;
    return MakeTyped( E, ScriptNode::NUMBER, ss.str() );
}

string ScriptNode::ObjectToTypedValueString( EvalElw &E, UINT32 handle )
{
    stringstream ss;
    ss << handle;
    return MakeTyped( E, ScriptNode::OBJECT, ss.str() );
}
