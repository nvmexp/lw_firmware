/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "mdiag/tests/stdtest.h"
#include "verif2d.h"
#include "mdiag/utils/mdiag_xml.h"
#include "gpu/include/gpudev.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"

Function::Function( void )
{
    m_v2d = 0;
}

void Function::Call( EvalElw &E, FuncCallNode *fcn )
{
    if ( fcn->m_name == "random" )
        Random( E, fcn );
    else if ( fcn->m_name == "printvar" )
        PrintVar( E, fcn );
    else if ( fcn->m_name == "object" )
        Object( E, fcn );
    else if ( fcn->m_name == "set" )
        Set( E, fcn );
    else if ( fcn->m_name == "launch" )
        Launch( E, fcn );
    else if ( fcn->m_name == "surface" )
        Surface( E, fcn );
    else if ( fcn->m_name == "clear" )
        Clear( E, fcn );
    else if ( fcn->m_name == "dumpTGA" )
        DumpTGA( E, fcn );
    else if ( fcn->m_name == "dumpImage" )
        DumpImage( E, fcn );
    else if ( fcn->m_name == "loadTGA" )
        LoadTGA( E, fcn );
    else if ( fcn->m_name == "checkCrc" )
        CheckCrc( E, fcn );
    else if ( fcn->m_name == "bpp" )
        BPP( E, fcn );
    else if ( fcn->m_name == "setsurfaces" )
        SetSurfaces( E, fcn );
    else if ( fcn->m_name == "defined" )
        Defined( E, fcn );
    else if ( fcn->m_name == "waitforidle" )
        WaitForIdle( E, fcn );
    else if ( fcn->m_name == "pmstart" )
        PM_Start( E, fcn );
    else if ( fcn->m_name == "pmend" )
        PM_End( E, fcn );
    else if ( fcn->m_name  == "colwertColorFormat" )
        ColwertColorFormat( E, fcn );
    else if ( fcn->m_name  == "bppFormat" )
        BPP_Format( E, fcn );
    else if ( fcn->m_name == "width" )
        Width( E, fcn );
    else if ( fcn->m_name == "height" )
        Height( E, fcn );
    else if ( fcn->m_name == "xy_to_offset" )
        XY_ToOffset( E, fcn );
    else if ( fcn->m_name == "format" )
        Format( E, fcn );
    else if ( fcn->m_name == "print" )
        Print( E, fcn );
    else if ( fcn->m_name == "ratio2fixedInt" )
        Ratio2FixedInt( E, fcn );
    else if ( fcn->m_name == "ratio2fixedFrac" )
        Ratio2FixedFrac( E, fcn );
    else if ( fcn->m_name == "offset_lower" )
        OffsetLower( E, fcn );
    else if ( fcn->m_name == "offset_upper" )
        OffsetUpper( E, fcn );
    else if ( fcn->m_name == "inline_data" )
        SendInlineData( E, fcn );
    else if ( fcn->m_name == "inline_data_array" )
        SendInlineDataArray( E, fcn );
    else if ( fcn->m_name == "setrenderenable" )
        SetRenderEnable( E, fcn );
    else
        V2D_THROW( "no such function: " << fcn->m_name );
}

//
// random() has two forms: with no arguments, it returns a 32-bit random number
// with one integer argument, it returns a positive number between 0 and the parameter,
// exclusive.    (e.g., random(100) returns 0..99)
//

void Function::Random( EvalElw &E, FuncCallNode *fcn )
{
    int num;

    if ( fcn->m_params == 0 )
        num = (int) m_v2d->Random();
    else
    {
        if ( fcn->m_params->m_paramlist.size() > 1 )
            V2D_THROW( "function random accepts either no parameters or 1 parameter" );

        list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();
        ExprNode *e = (*i)->m_value;
        if ( e->GetEvalType(E) != ScriptNode::NUMBER )
            V2D_THROW( "argument to function random must evaluate to a number" );
        UINT32 randArg = (UINT32) e->GetIntEvaluation(E);

        num = (int) ( m_v2d->Random() & 0x7fffffff );
        num = num % randArg;
    }

    fcn->m_evaluatiolwalue = fcn->IntToTypedValueString( E, num );
}

//
// print out a list of script variables
//

void Function::PrintVar( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i;

    for ( i = fcn->m_params->m_paramlist.begin(); i != fcn->m_params->m_paramlist.end(); i++ )
    {
        ExprNode *ident = (*i)->m_value;
        if ( ident->m_op != Token::IDENTIFIER )
            V2D_THROW( "printident function only accepts a list of identifiers to print out, you passed it a: "
                       << E.m_lex->TokenString( ident->m_op ) );

        InfoPrintf( "Script identifier %s has value %s\n", (ident->m_value).c_str(),
                    (ident->GetSymbolValue(E, ident->m_value)).c_str() );
    }

    // no return value, if anybody uses it it's an error
    fcn->m_evaluatiolwalue = "";
}

void Function::Object( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() != 1 )
        V2D_THROW( "function object() takes exactly one string parameter (the class name) as an argument)" );

    ExprNode *e = (*i)->m_value;

    if ( e->GetEvalType(E) != ScriptNode::STRING )
        V2D_THROW( "the type of the paramter to object() must evaluate to type STRING" );

    // create the object
    //
    string className = e->RemoveType( E, e->GetEvaluation() );
    V2dClassObj *obj = m_v2d->NewClassObj( className );

    if (obj->GetClassId() == V2dClassObj::_FERMI_TWOD_A)
    {
        GpuDevice *pGpuDev = m_v2d->GetLWGpu()->GetGpuDevice();
        MASSERT(pGpuDev);
        pGpuDev->SetGobHeight(8);

        if (Platform::GetSimulationMode() == Platform::Amodel)
        {
            m_v2d->AddCrc("gf100a");
        }
        else
        {
            m_v2d->AddCrc("gf100");
        }
    }
    else
    {
        if (Platform::GetSimulationMode() != Platform::Amodel)
            m_v2d->AddCrc("lw50f");
        m_v2d->AddCrc("lw50a");
    }

    // return the object
    //
    UINT32 handle = obj->GetHandle();
    fcn->m_evaluatiolwalue = e->ObjectToTypedValueString( E, handle );

    // walk the method initialization parameter list
    //
    WalkMethodParamList( E, fcn, obj );
}

//
// Set() and Launch() are nearly identical, only Launch() calls obj->LaunchScript()
//

void Function::Set( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() == 0 )
        V2D_THROW( "set object() must take at least one parameter" );

    ExprNode *e = (*i)->m_value;
    if ( e->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of the first paramter to set() must evaluate to type OBJECT" );

    // get the object
    UINT32 handle = e->GetObjectEvaluation(E);
    V2dClassObj *obj = m_v2d->GetObjectFromHandle( handle );

    // process the method parameter list
    //
    WalkMethodParamList( E, fcn, obj );

    // no return value
    fcn->m_evaluatiolwalue = "";
}

void Function::Launch( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() == 0 )
        V2D_THROW( "launch object() must take at least one parameter" );

    ExprNode *e = (*i)->m_value;
    if ( e->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of the first paramter to launch() must evaluate to type OBJECT" );

    // get the object
    UINT32 handle = e->GetObjectEvaluation(E);
    V2dClassObj *obj = m_v2d->GetObjectFromHandle( handle );

    // process the method parameter list
    //
    WalkMethodParamList( E, fcn, obj );

    // write any pending dirty shadowed methods to the GPU
    //
    obj->LaunchScript();

    // no return value
    fcn->m_evaluatiolwalue = "";
}

//
// common code to Set(), Object(), and Launch()
//
void Function::WalkMethodParamList( EvalElw &E,
                                    FuncCallNode *fcn, V2dClassObj *obj )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // move on to the first method parameter, which is the 2nd parameter in the list
    //
    ++ i;

    //
    // process each parameter.  make sure each one has a tag.  For now we do no method type checking, we just
    // remove the type from the value expression and pass it to the named method call, colwerting
    // NUMBER and OBJECT types to integers, and keeping STRINGs as strings.  If methods had types this would
    // be easier because we'd just pass in the typed string and the colwersion would take place inside the
    // named method write code.
    //
    while ( i != fcn->m_params->m_paramlist.end() )
    {
        ExprNode *tag = (*i)->m_tag;
        if ( tag == 0 )
            V2D_THROW( "syntax error: parameter list must not be empty and evaluate to type STRING" );

        string methodName = tag->GetStringEvaluation(E);
        DebugPrintf("Processing command %s\n", methodName.c_str());
        ExprNode *value = (*i)->m_value;
        if ( value->GetEvalType(E) == ScriptNode::STRING )
        {
            string methodData = value->GetStringEvaluation(E);
            obj->NamedMethodOrFieldWriteShadow( methodName, methodData );
        }
        else if ( value->GetEvalType(E) == ScriptNode::NUMBER )
        {
            UINT32 methodData = (UINT32) value->GetIntEvaluation(E);
            obj->NamedMethodOrFieldWriteShadow( methodName, methodData );
        }
        else if ( value->GetEvalType(E) == ScriptNode::OBJECT )
        {
            UINT32 handle = value->GetObjectEvaluation(E);
            V2dSurface *surf = m_v2d->GetObjectFromHandleSurf(handle);
            obj->NamedMethodOrFieldWriteShadow( methodName, surf->GetCtxDmaHandle());
        }
        else
            V2D_THROW( "unknown type in parameter value expression: " << value->GetEvalType(E) );

        // go on to the next parameter
        //
        ++ i;
    }
}

static int GobStringToLog(const string &s) {
        if(s == "ONE_GOB")
            return 0;
        else if(s == "TWO_GOBS")
            return 1;
        else if(s == "FOUR_GOBS")
            return 2;
        else if(s == "EIGHT_GOBS")
            return 3;
        else  if(s == "SIXTEEN_GOBS")
            return 4;
        else if(s == "THIRTYTWO_GOBS")
            return 5;
        else
            V2D_THROW( "unknown block size " << s );
}

//
// $surfObj = surface( width=<int>, height=<int>, format=<string>, tiled=<int>, where=<string> )
//

void Function::Surface( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode*> &paramList = fcn->m_params->m_paramlist;

    // required parameters
    int width = GetTaggedParameterIntValue( E, "width", paramList );
    int height = GetTaggedParameterIntValue( E, "height", paramList );
    string format = GetTaggedParameterStringValue( E, "format", paramList );
    string where = GetTaggedParameterStringValue( E, "where", paramList );

    // optional parameters
    int depth = 1;
    if ( TaggedParameterPresent( E, "depth", paramList ) == true )
        depth = GetTaggedParameterIntValue( E, "depth", paramList );
    int tiled = 0;
    if ( TaggedParameterPresent( E, "tiled", paramList ) == true )
        tiled = GetTaggedParameterIntValue( E, "tiled", paramList );
    MASSERT(!tiled);
    int swizzled = 0;
    if ( TaggedParameterPresent( E, "swizzled", paramList ) == true )
        swizzled = GetTaggedParameterIntValue( E, "swizzled", paramList );
    MASSERT(!swizzled);
    int compressed = 0;
    if ( TaggedParameterPresent( E, "compressed", paramList ) == true )
        compressed = GetTaggedParameterIntValue( E, "compressed", paramList );

    int offset = 0;
    if ( TaggedParameterPresent( E, "offset", paramList ) == true )
        offset = GetTaggedParameterIntValue( E, "offset", paramList );

    int limit = -1;
    if ( TaggedParameterPresent( E, "limit", paramList ) == true )
        limit = GetTaggedParameterIntValue( E, "limit", paramList );

    int pitch = 0;
    if ( TaggedParameterPresent( E, "pitch", paramList ) == true )
        pitch = GetTaggedParameterIntValue( E, "pitch", paramList );

    int blocklinear = 0;
    if ( TaggedParameterPresent( E, "blocklinear", paramList ) == true )
        blocklinear = (GetTaggedParameterStringValue( E, "blocklinear", paramList ) == "true");
    int blockHeight = 0;
    if ( TaggedParameterPresent( E, "blockHeight", paramList ) == true )
        blockHeight = GobStringToLog(GetTaggedParameterStringValue( E, "blockHeight", paramList ));
    int blockDepth = 0;
    if ( TaggedParameterPresent( E, "blockDepth", paramList ) == true )
        blockDepth = GobStringToLog(GetTaggedParameterStringValue( E, "blockDepth", paramList ));

    V2dSurface::Label label = V2dSurface::NO_LABEL;

    if (TaggedParameterPresent(E, "label", paramList))
    {
        if (GetTaggedParameterStringValue(E, "label", paramList) == "A")
        {
            label = V2dSurface::LABEL_A;
        }
        else if (GetTaggedParameterStringValue(E, "label", paramList) == "B")
        {
            label = V2dSurface::LABEL_B;
        }
        else
        {
            V2D_THROW("label parameter must be set to \"A\" or \"B\"");
        }
    }

    // removed parameters:
    if ( TaggedParameterPresent( E, "clear", paramList ) ||
         TaggedParameterPresent( E, "clearpattern", paramList ) )
    {
        V2D_THROW( "surface() function no longer supports \"clear\" and "
                   "\"clearpattern\", use clear() function instead" );
    }

    if ( compressed && !blocklinear )
    {
        V2D_THROW( "cannot create a compressed PITCH surface");
    }

    if ( depth > 1 && !blocklinear )
    {
        V2D_THROW( "cannot create a PITCH surface with depth > 1");
    }

    if ( format == "Y1_8X8" )
    {
        if ( width % 8 != 0 || height % 8 != 0 )
            V2D_THROW( "Y1_8X8 surface dimensions must be multiples of 8" );

        // Y1_8X8 surfaces have a dual nature
        // verif2d, rop, and blocklinear addr calcs treat them as 64 bit/pixel
        // the class API, rstr2d, and texture treat them as 1 bit/pixel
        width /= 8;
        height /= 8;
    }

    // create the surface
    V2dSurface::Flags flags = m_v2d->m_dstFlags;
    flags = (V2dSurface::Flags) ((UINT32)flags | (tiled ? V2dSurface::TILED : 0));
    flags = (V2dSurface::Flags) ((UINT32)flags | (swizzled ? V2dSurface::SWIZZLED : 0));
    flags = (V2dSurface::Flags) ((UINT32)flags | (compressed ? V2dSurface::COMPRESSED : 0));
    V2dSurface *surf = 0;
    if(blocklinear)
        surf = new V2dSurface(m_v2d,
                              width, height, depth,
                              format, flags, offset, limit, where,
                              m_v2d->GetParams(),
                              blockHeight, blockDepth, label);
    else
        surf = new V2dSurface(m_v2d,
                              width, height,
                              format, flags, offset, limit, pitch, where,
                              m_v2d->GetParams());
    surf->Init();

    // keep a list of all allocated surfaces, so we can delete them later:
    E.m_v2d->m_surfaces.push_back(surf);
    surf->AddToTable(&E.m_v2d->m_buffInfo);

    // return the object handle
    UINT32 handle = surf->GetHandle();
    fcn->m_evaluatiolwalue = fcn->ObjectToTypedValueString( E, handle );
}

//
// send inline data for memfmt
// no return value;
//
// usage: inline_data( $memobj, $count, $data );
//

void Function::SendInlineData( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();
    int nParams = fcn->m_params->m_paramlist.size();
    string dataPattern;

    if ( nParams < 2 )
        V2D_THROW( "SendInlineData() requires at least two parameters" );

    ExprNode *e = (*i)->m_value;
    if ( e->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of the 1st paramter to SendInlineData() must evaluate to type OBJECT" );

    // get the object
    UINT32 handle = e->GetObjectEvaluation(E);
    V2dClassObj *obj = m_v2d->GetObjectFromHandle( handle );

    i++;
    map<string,UINT32> argMap;

    while ( i != fcn->m_params->m_paramlist.end() )
    {
        ExprNode *tag = (*i)->m_tag;
        if ( tag == 0 )
            V2D_THROW( "syntax error: must supply a tag string to the data parameter in inline_data" );
        if ( tag->GetEvalType(E) != ScriptNode::STRING )
            V2D_THROW( "syntax error: the tag expression in the inline_data parameter list must evaluate to type STRING" );

        ExprNode *val = (*i)->m_value;
        if ( val->GetEvalType(E) != ScriptNode::NUMBER )
            V2D_THROW( "the type of the value paramters to inline_data() must evaluate to type NUMBER" );

        // get the data
        UINT32 data = val->GetIntEvaluation(E);
        argMap[ tag->GetStringEvaluation(E) ] = data;
        ++ i;

    }

    obj->SendInlineData( argMap );
    // no return value
    fcn->m_evaluatiolwalue = "";
}

//
// send inline data for memfmt
// no return value;
//
// the same as SendInlineData, except that data hi method is used here
// Ref the bug 406173
//

void Function::SendInlineDataArray( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();
    int nParams = fcn->m_params->m_paramlist.size();
    string dataPattern;

    if ( nParams < 2 )
        V2D_THROW( "SendInlineData() requires at least two parameters" );

    ExprNode *e = (*i)->m_value;
    if ( e->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of the 1st paramter to SendInlineData() must evaluate to type OBJECT" );

    // get the object
    UINT32 handle = e->GetObjectEvaluation(E);
    V2dClassObj *obj = m_v2d->GetObjectFromHandle( handle );

    i++;
    map<string,UINT32> argMap;

    while ( i != fcn->m_params->m_paramlist.end() )
    {
        ExprNode *tag = (*i)->m_tag;
        if ( tag == 0 )
            V2D_THROW( "syntax error: must supply a tag string to the data parameter in inline_data" );
        if ( tag->GetEvalType(E) != ScriptNode::STRING )
            V2D_THROW( "syntax error: the tag expression in the inline_data parameter list must evaluate to type STRING" );

        ExprNode *val = (*i)->m_value;
        if ( val->GetEvalType(E) != ScriptNode::NUMBER )
            V2D_THROW( "the type of the value paramters to inline_data() must evaluate to type NUMBER" );

        // get the data
        UINT32 data = val->GetIntEvaluation(E);
        argMap[ tag->GetStringEvaluation(E) ] = data;
        ++ i;

    }

    obj->SendInlineDataArray( argMap );
    // no return value
    fcn->m_evaluatiolwalue = "";
}

static int RenderEnableStringToInt(const string &s) {
    if ( s == "FALSE" )
       return 0;
    else if ( s== "TRUE")
       return 1;
    else if ( s== "CONDITIONAL")
       return 2;
    else if ( s== "RENDER_IF_EQUAL")
       return 3;
    else if ( s== "RENDER_IF_NOT_EQUAL")
       return 4;
    else if ( s== "USE_RENDER_ENABLE")
       return 0;
    else if ( s== "ALWAYS_RENDER")
       return 1;
    else if ( s== "NEVER_RENDER")
       return 2;
    else
       V2D_THROW( "unknown string in setrenderenable " << s );

}
//
// set render enable
// no return value;
//
// usage: setrenderenable($Object, "<name>"=value );
//

void Function::SetRenderEnable( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();
    int nParams = fcn->m_params->m_paramlist.size();
    string dataPattern;

    if ( nParams < 2 )
        V2D_THROW( "SetRenderEnable() requires at least two parameters" );

    ExprNode *e = (*i)->m_value;
    if ( e->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of the 1st paramter to SetRenderEnable() must evaluate to type OBJECT" );

    // get the object
    UINT32 handle = e->GetObjectEvaluation(E);
    V2dClassObj *obj = m_v2d->GetObjectFromHandle( handle );

    i++;
    map<string,UINT32> argMap;

    while ( i != fcn->m_params->m_paramlist.end() )
    {
        ExprNode *tag = (*i)->m_tag;
        if ( tag == 0 )
            V2D_THROW( "syntax error: must supply a tag string to the data parameter in setrenderenable" );
        if ( tag->GetEvalType(E) != ScriptNode::STRING )
            V2D_THROW( "syntax error: the tag expression in the setrenderenable parameter list must evaluate to type STRING" );

        ExprNode *val = (*i)->m_value;
        UINT32 data ;
        if ( val->GetEvalType(E) == ScriptNode::STRING )
            data = RenderEnableStringToInt(val->GetStringEvaluation( E ) );
        else if ( val->GetEvalType(E) == ScriptNode::NUMBER )
            data = val->GetIntEvaluation(E);
        else
            V2D_THROW( "syntax error: bject value is not allowed in parameter value in setrenderenable" );

        // get the data
        argMap[ tag->GetStringEvaluation(E) ] = data;
        ++ i;

    }

    obj->SetRenderEnable( argMap );
    // no return value
    fcn->m_evaluatiolwalue = "";
}
//
// clears a surface
// no return value;
//
// usage: clear( $surfaceObject, #optional <string> );
// if the optional clear pattern string is given, the surface's clear pattern is changed to that new pattern
//

void Function::Clear( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();
    int nParams = fcn->m_params->m_paramlist.size();
    string clearPattern;

    if ( nParams < 2 )
        V2D_THROW( "clear() requires at least two parameters" );

    ExprNode *e = (*i)->m_value;
    if ( e->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of the 1st paramter to clear() must evaluate to type OBJECT" );

    // get the object
    UINT32 handle = e->GetObjectEvaluation(E);
    V2dSurface *obj = m_v2d->GetObjectFromHandleSurf( handle );

    i++;

    while ( i != fcn->m_params->m_paramlist.end() )
    {
        ExprNode *e2 = (*i)->m_value;
        if ( e2->GetEvalType(E) != ScriptNode::STRING )
            V2D_THROW( "in function clear(), expected clear pattern" );
        clearPattern = e2->GetStringEvaluation(E);

        vector<UINT32> clearData;

        i++;

        while ( i != fcn->m_params->m_paramlist.end() &&
                (*i)->m_value->GetEvalType(E) == ScriptNode::NUMBER )
        {
            clearData.push_back( (*i)->m_value->GetIntEvaluation(E) );
            i++;
        }

        obj->Clear( clearPattern, clearData );
    }

    // no return value
    fcn->m_evaluatiolwalue = "";
}

//
// dumps a TGA file
// no return value;
//
// usage: clear( $surfaceObject );
//

void Function::DumpTGA( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() < 2 || fcn->m_params->m_paramlist.size() > 3)
        V2D_THROW( "dumpTGA() takes either two or three parameters" );

    ExprNode *e1 = (*i)->m_value;
    if ( e1->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of paramter 1 to dumpTGA() must evaluate to type OBJECT" );

    ++i;
    ExprNode *e2 = (*i)->m_value;
    if ( e2->GetEvalType(E) != ScriptNode::STRING )
        V2D_THROW( "the type of paramter 2 to dumpTGA() must evaluate to type STRING" );

    UINT32 layer = 0;
    if(fcn->m_params->m_paramlist.size() == 3)
    {
        ++i;
        if ( (*i)->m_value->GetEvalType(E) != ScriptNode::NUMBER )
            V2D_THROW( "the type of paramter 3 to dumpTGA() must evaluate to type NUMBER" );
        layer = (*i)->m_value->GetIntEvaluation(E);
    }

    // get the object
    UINT32 handle = e1->GetObjectEvaluation(E);
    V2dSurface *obj = m_v2d->GetObjectFromHandleSurf( handle );

    string filename = e2->GetStringEvaluation(E) + ".tga";
    m_v2d->WaitForIdle();

    InfoPrintf("dumping layer %d to TGA file \"%s\"\n", layer, filename.c_str());

    obj->SaveToTGAfile( filename, layer);

    // no return value
    fcn->m_evaluatiolwalue = "";
}

//
// dumps an image file, using default or command-line format
// no return value;
//
// usage: dumpImage( $surfaceObject );
//

void Function::DumpImage( EvalElw &E, FuncCallNode *fcn )
{
    switch ( m_v2d->GetImageDumpFormat() )
    {
        case V2dSurface::NONE:
            break;
        case V2dSurface::TGA:
            DumpTGA( E, fcn );
            break;
        default:
            V2D_THROW( "unknown image dump format " <<
                       m_v2d->GetImageDumpFormat() <<
                       " in Function::DumpImage()" ) ;
    }
}

//
// loads a TGA file
// no return value;
//
// usage: loadTGA( $surfaceObject, $filename );
//

void Function::LoadTGA( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() != 2 )
        V2D_THROW( "loadTGA() takes exactly two parameters" );

    ExprNode *e1 = (*i)->m_value;
    if ( e1->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of paramter 1 to loadTGA() must evaluate to type OBJECT" );

    i++;
    ExprNode *e2 = (*i)->m_value;
    if ( e2->GetEvalType(E) != ScriptNode::STRING )
        V2D_THROW( "the type of paramter 2 to loadTGA() must evaluate to type STRING" );

    // get the object
    UINT32 handle = e1->GetObjectEvaluation(E);
    V2dSurface *obj = m_v2d->GetObjectFromHandleSurf( handle );

    string filename = e2->GetStringEvaluation(E) + ".tga";
    m_v2d->WaitForIdle();

    obj->LoadFromTGAfile( filename );

    InfoPrintf("loaded TGA file \"%s\"\n", filename.c_str());

    // no return value
    fcn->m_evaluatiolwalue = "";
}
//
// returns integer number of bytes per pixel for specified format
//
void Function::BPP_Format( EvalElw &E, FuncCallNode *fcn )
{
    string colorString;
    unsigned short int bpp;
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    if ( fcn->m_params->m_paramlist.size() != 1)
        V2D_THROW( "BPP_Format() takes exactly one parameter" );
    ExprNode *e1 = (*i)->m_value;
    if ( e1->GetEvalType(E) != ScriptNode::STRING )
        V2D_THROW( "the type of paramter 1 to BPP_Format() must evaluate to type STRING" );
    colorString = e1->GetStringEvaluation(E);

    if(colorString=="RF32_GF32_BF32_AF32"||colorString=="RF32_GF32_BF32_X32")
         bpp = 128;
    else if(colorString=="RF32_GF32"||colorString=="RF16_GF16_BF16_AF16"||colorString=="RF16_GF16_BF16_X16")
         bpp = 64;
    else if(colorString=="A8R8G8B8" ||colorString=="X8R8G8B8" ||colorString=="A2R10G10B10" ||colorString=="A2B10G10R10"||colorString=="O8R8G8B8" ||colorString=="Z8R8G8B8" ||colorString=="A8B8G8R8" ||colorString=="X8B8G8R8"|| colorString=="X16R5G6B5" ||colorString=="X17R5G5B5"||colorString=="Y32"||colorString=="X8RL8GL8BL8"||colorString=="X8BL8GL8RL8"||colorString=="A8RL8GL8BL8"||colorString=="A8BL8GL8RL8"||colorString=="RF32")
        bpp = 32;
    else if(colorString=="A1R5G5B5" ||colorString=="R5G6B5" ||colorString=="X1R5G5B5" ||colorString=="Z1R5G5B5"||colorString=="O1R5G5B5" ||colorString=="Z8R8G8B8" ||colorString=="A8B8G8R8" ||colorString=="Y16"||colorString=="RF16")
        bpp=16;
    else if(colorString=="Y8"||colorString =="AY8")
        bpp = 8;
    else
        V2D_THROW("In BPP_Format unrecognized color format string "<<colorString);

     fcn->m_evaluatiolwalue = fcn->IntToTypedValueString( E, bpp );

}
//
// returns integer value of color in desired format
//
void Function::ColwertColorFormat( EvalElw &E, FuncCallNode *fcn )
{
    unsigned int srcColor;
    string dstfmtString;
    unsigned int bits0,bits1,bits2,bits3;
    unsigned int srcA,srcR,srcG,srcB;
    unsigned int dst0 = 0, dst1 = 0, dst2 = 0, dst3 = 0;
    unsigned int dstColor = 0;
    unsigned int FitTo(unsigned int src, unsigned int srcBits, unsigned int dstBits);

    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() != 2 )
        V2D_THROW( "colwertColorFormat() takes exactly two parameters" );
    ExprNode *e1 = (*i)->m_value;
    if ( e1->GetEvalType(E) != ScriptNode::NUMBER )
        V2D_THROW( "the type of paramter 1 to colwertColorFormat() must evaluate to type NUMBER" );
    srcColor = (unsigned int) e1->GetIntEvaluation(E);

    i++;
    ExprNode *e2 = (*i)->m_value;
    if ( e2->GetEvalType(E) != ScriptNode::STRING )
        V2D_THROW( "the type of paramter 2 to colwertColorFormat() must evaluate to type STRING" );
    dstfmtString = e2->GetStringEvaluation(E);

    srcB = (srcColor & 0x000000ff);
    srcG = (srcColor & 0x0000ff00)>>8;
    srcR = (srcColor & 0x00ff0000)>>16;
    srcA = (srcColor) >>24;

    if(dstfmtString == "A8R8G8B8" || dstfmtString == "X8R8G8B8" || dstfmtString == "O8R8G8B8" || dstfmtString == "Z8R8G8B8" || dstfmtString == "A8RL8GL8BL8" || dstfmtString == "X8RL8GL8BL8" )
        {
        if(dstfmtString == "O8R8G8B8")
            srcA = 0xff;
        else if(dstfmtString == "Z8R8G8B8")
            srcA = 0x00;

        dst0 = srcA;
        dst1 = srcR;
        dst2 = srcG;
        dst3 = srcB;
        bits0 = bits1 = bits2 = bits3 = 8;
        }
    else  if(dstfmtString == "A8B8G8R8" ||dstfmtString == "X8B8G8R8"||dstfmtString == "A8BL8GL8RL8" ||dstfmtString == "X8BL8GL8RL8")
        {
        dst0 = srcA;
        dst1 = srcB;
        dst2 = srcG;
        dst3 = srcR;
        bits0 = bits1 = bits2 = bits3 = 8;

        }
    else if(dstfmtString == "A2R10G10B10" || dstfmtString == "A2B10G10R10"  )
        {
        bits0 = 2;
        bits1 = bits2 = bits3 = 10;

        dst0 = FitTo(srcA,8,bits0);

        if(dstfmtString=="A2R10G10B10")
            {
            dst1 =  FitTo(srcR,8,bits1);
            dst2 =  FitTo(srcG,8,bits2);
            dst3 =  FitTo(srcB,8,bits3);
            }
        else
            {
            dst1 =  FitTo(srcB,8,bits1);
            dst2 =  FitTo(srcG,8,bits2);
            dst3 =  FitTo(srcR,8,bits3);

            }

        }
   else if(dstfmtString == "A16R5G6B5")
        {
        bits0 = 16;
        bits1 = bits3 = 5;
        bits2 = 6;

        dst0 =  FitTo(srcA,8,bits0);
        dst1 =  FitTo(srcR,8,bits1);
        dst2 =  FitTo(srcG,8,bits2);
        dst3 =  FitTo(srcB,8,bits3);
        }
   else if(dstfmtString == "R5G6B5"||dstfmtString == "X16R5G6B5")
        {
        bits0 = 16;
        bits1 = bits3 = 5;
        bits2 = 6;

        dst0 = 0x0000;

        dst1 =  FitTo(srcR,8,bits1);
        dst2 =  FitTo(srcG,8,bits2);
        dst3 =  FitTo(srcB,8,bits3);
        }

    else if(dstfmtString == "A1R5G5B5" || dstfmtString == "X1R5G5B5" || dstfmtString == "O1R5G5B5" || dstfmtString == "Z1R5G5B5" ||dstfmtString == "X17R5G5B5" ||dstfmtString == "X16A1R5G5B5" )
        {
        bits0 = 17;
        bits1 = bits2 = bits3 = 5;

        if(dstfmtString == "Z1R5G5B5")
            srcA = 0x00;

        dst0 = FitTo(srcA,8,bits0);

        //set 1 bit alpha to top bit if A1R5G5B5
        if(dstfmtString == "A1R5G5B5"||dstfmtString == "X16A1R5G5B5")
             {
             if(srcA>>7)
                 dst0 = 0x1ffff;
             else
                 dst0 = 0;
             }

        dst1 = FitTo(srcR,8,bits1);
        dst2 = FitTo(srcG,8,bits2);
        dst3 = FitTo(srcB,8,bits3);

        if(dstfmtString == "O1R5G5B5")
            dst0 = 0x1ffff;

        }
    else if(dstfmtString == "Y32" || dstfmtString == "Y16" || dstfmtString == "Y8"||dstfmtString == "AY8" )
        {
        if(dstfmtString == "Y32")
            bits3 = 32;
        else if(dstfmtString == "Y16")
            bits3 = 16;
        else
            bits3 = 8;
        dst3 =  FitTo(srcB,8,bits3);
        bits0 = bits1 = bits2 = 0;

        }
    else
        {
        V2D_THROW("In func.cpp : colwertColorFormat. Can't colwert from A8R8G8B8 to "<<dstfmtString);

        }

    //repack components into dstColor
    dstColor = (dst0<< bits1 ) | dst1;
    dstColor = (dstColor<< bits2) | dst2;
    dstColor = (dstColor<< bits3) | dst3;

    //deal with upper 16 bits of 16 bit formats
    if(dstfmtString == "R5G6B5" || dstfmtString == "A1R5G5B5" || dstfmtString == "X1R5G5B5" || dstfmtString == "O1R5G5B5" || dstfmtString == "Z1R5G5B5" || dstfmtString == "Y16")
         {
         dstColor = (0x0000ffff & dstColor)<<16 | (0x0000ffff & dstColor);
         }

    //deal with upper 24 bits of 8 bit formats
    if(dstfmtString == "Y8"||dstfmtString =="AY8" )
         dstColor = (0x000000ff & dstColor)<<24 | (0x000000ff & dstColor) <<16 | (0x000000ff & dstColor)<<8 | (0x000000ff & dstColor);

    fcn->m_evaluatiolwalue = fcn->IntToTypedValueString( E, dstColor );
}

//
// does one of the following, depending on command-line option:
//     checks CRC against value in crcfile
//     saves CRC to crcfile
//     nothing
//
// no return value;
//
// usage: checkCrc( $surfaceObject, $key_string );
//

void Function::CheckCrc( EvalElw &E, FuncCallNode *fcn )
{
    if (m_v2d->GetCrcMode() == CRC_MODE_NOTHING)
    {
        // bug 902561: skip calc and check altogether
        InfoPrintf("Skipping CRC callwlation and check by user request!\n");
        return;
    }

    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() != 2 )
        V2D_THROW( "checkCrc() takes exactly two parameters" );

    ExprNode *e1 = (*i)->m_value;
    if ( e1->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of paramter 1 to checkCrc() must evaluate to type OBJECT" );

    i++;
    ExprNode *e2 = (*i)->m_value;
    if ( e2->GetEvalType(E) != ScriptNode::STRING )
        V2D_THROW( "the type of paramter 2 to checkCrc() must evaluate to type STRING" );

    // get the object
    UINT32 handle = e1->GetObjectEvaluation(E);
    V2dSurface *obj = m_v2d->GetObjectFromHandleSurf( handle );

    // get the key
    string key = e2->GetStringEvaluation(E);

    UINT32 crc = obj->CalcCRC();

    switch ( m_v2d->GetCrcMode() )
    {
        case CRC_MODE_NOTHING:
            InfoPrintf( "checkCrc(%s): 0x%08x (ignored)\n",
                        key.c_str(), crc );
            break;
        case CRC_MODE_CHECK:
            if ( m_v2d->GetCrcProfile()->
                    CheckCrc(m_v2d->GetCrcSection(), key.c_str(), crc, 0) != 1 )
            {
                m_v2d->GetTest()->SetStatus( Test::TEST_FAILED_CRC );
                m_v2d->GetTest()->getStateReport()->crcCheckFailed(
                    "command-line CRC check failed");
                V2D_THROW_NOUPDATE("CRC check failed\n");
            }
            break;
        case CRC_MODE_GEN:
            m_v2d->GetCrcProfile()->
                    CheckCrc(m_v2d->GetCrcSection(), key.c_str(), crc, 1);
            break;
    }

    // no return value
    fcn->m_evaluatiolwalue = "";
}

//
// returns an int with the bytes/pixel of the given surface objects
//
// usage: bpp( $surfaceObject );
//

void Function::BPP( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() != 1 )
        V2D_THROW( "bpp() takes exactly one parameter" );

    ExprNode *e1 = (*i)->m_value;
    if ( e1->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of paramter to bpp() must evaluate to type OBJECT" );

    // get the object
    UINT32 handle = e1->GetObjectEvaluation(E);
    V2dSurface *obj = m_v2d->GetObjectFromHandleSurf( handle );

    fcn->m_evaluatiolwalue = fcn->IntToTypedValueString( E, obj->GetBpp() );
}

void Function::Width( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() != 1 )
        V2D_THROW( "width() takes exactly one parameter" );

    ExprNode *e1 = (*i)->m_value;
    if ( e1->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of paramter to width() must evaluate to type OBJECT" );

    // get the object
    UINT32 handle = e1->GetObjectEvaluation(E);
    V2dSurface *obj = m_v2d->GetObjectFromHandleSurf( handle );

    fcn->m_evaluatiolwalue = fcn->IntToTypedValueString( E, obj->GetWidth() );
}

void Function::Height( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() != 1 )
        V2D_THROW( "height() takes exactly one parameter" );

    ExprNode *e1 = (*i)->m_value;
    if ( e1->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of paramter to height() must evaluate to type OBJECT" );

    // get the object
    UINT32 handle = e1->GetObjectEvaluation(E);
    V2dSurface *obj = m_v2d->GetObjectFromHandleSurf( handle );

    fcn->m_evaluatiolwalue = fcn->IntToTypedValueString( E, obj->GetHeight() );
}

void Function::XY_ToOffset( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() != 3 )
        V2D_THROW( "xy_to_address() takes exactly three parameters" );

    ExprNode *e1 = (*i)->m_value;
    if ( e1->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of paramter 1 to xy_to_address() must evaluate to type OBJECT" );

    // get the object
    UINT32 handle = e1->GetObjectEvaluation(E);
    V2dSurface *obj = m_v2d->GetObjectFromHandleSurf( handle );

    i++;
    int x=0, y=0, n=0;

    while ( i != fcn->m_params->m_paramlist.end() && (*i)->m_value->GetEvalType(E) == ScriptNode::NUMBER ) {
        if ( n==0 )
            x = (*i)->m_value->GetIntEvaluation(E) ;
        else if ( n==1 )
            y = (*i)->m_value->GetIntEvaluation(E) ;
        else
            V2D_THROW( "in function xy_to_address(), found more than three parameters" );
        n++;
        i++;
    }

    if ( n != 2 ) {
        V2D_THROW( "in function xy_to_address(), found less than three parameters" );
    }

    UINT64 the_address = obj->GetAddress(x,y,0);
    fcn->m_evaluatiolwalue = fcn->IntToTypedValueString( E, the_address );
}

void Function::OffsetLower( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() != 1 )
        V2D_THROW( "offset_lower() takes exactly three parameters" );

    ExprNode *e1 = (*i)->m_value;
    if ( e1->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of paramter 1 to offset_lower() must evaluate to type OBJECT" );

    // get the object
    UINT32 handle = e1->GetObjectEvaluation(E);
    V2dSurface *obj = m_v2d->GetObjectFromHandleSurf( handle );

    fcn->m_evaluatiolwalue = fcn->IntToTypedValueString( E, LwU64_LO32(obj->GetOffset()));
}

void Function::OffsetUpper( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() != 1 )
        V2D_THROW( "offset_upper() takes exactly three parameters" );

    ExprNode *e1 = (*i)->m_value;
    if ( e1->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of paramter 1 to offset_uppoer() must evaluate to type OBJECT" );

    // get the object
    UINT32 handle = e1->GetObjectEvaluation(E);
    V2dSurface *obj = m_v2d->GetObjectFromHandleSurf( handle );

    fcn->m_evaluatiolwalue = fcn->IntToTypedValueString( E, LwU64_HI32(obj->GetOffset()));
}

void Function::Format( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() != 1 )
        V2D_THROW( "format() takes exactly one parameters" );

    ExprNode *e1 = (*i)->m_value;
    if ( e1->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of paramter to format() must evaluate to type OBJECT" );

    // get the object
    UINT32 handle = e1->GetObjectEvaluation(E);
    V2dSurface *obj = m_v2d->GetObjectFromHandleSurf( handle );

    string fmt = obj->GetFormatString();
    fcn->m_evaluatiolwalue = fcn->MakeTyped( E, ScriptNode::STRING, fmt );
}

void Function::Print( EvalElw &E, FuncCallNode *fcn )
{
    int m=0;
    #define BUFSIZE 1024
    char buffer[BUFSIZE];
    snprintf( buffer, BUFSIZE, "v2d print:  ");
    m = strlen(buffer);

    for( list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin(); i != fcn->m_params->m_paramlist.end(); ++i ) {
        ExprNode *e = (*i)->m_value;
        switch( e->GetEvalType(E) ) {
        case ScriptNode::NUMBER: {
            int n = (*i)->m_value->GetIntEvaluation(E) ;
            snprintf(buffer+m, BUFSIZE-m, " 0x%x ", n );
            m = strlen(buffer);
            break;
        }
        case ScriptNode::STRING: {
            string sym = e->GetStringEvaluation(E);
            snprintf(buffer+m, BUFSIZE-m, " %s ", sym.c_str() );
            m = strlen(buffer);
            break;
        }
        case ScriptNode::OBJECT: {
            UINT32 handle = e->GetObjectEvaluation(E);
            snprintf(buffer+m, BUFSIZE-m, " (obj @0x%x) ", handle );
            m = strlen(buffer);
            break;
        }
        case ScriptNode::NO_TYPE: {
            V2D_THROW( "\lw2d print cannot handle untyped parameter\n");
        }
        }
    }
    InfoPrintf( "%s\n", buffer );
}

//
// usage: setsurfaces( $classobject, <string>=<surface object>, ...)
// build up a map of the tag/value pairs and send it to the object's SetSurfaces function
// no return value
//

void Function::SetSurfaces( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() < 2 )
        V2D_THROW( "launch object() must take at least two parameters, an object and at least one surface parameter" );

    ExprNode *e = (*i)->m_value;
    if ( e->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of the first paramter to launch() must evaluate to type OBJECT" );

    // get the object
    UINT32 handle = e->GetObjectEvaluation(E);
    V2dClassObj *obj = m_v2d->GetObjectFromHandle( handle );

    map<string,V2dSurface*> argMap;
    ++ i;
    while ( i != fcn->m_params->m_paramlist.end() )
    {
        ExprNode *tag = (*i)->m_tag;
        if ( tag == 0 )
            V2D_THROW( "syntax error: must supply a tag string to the surface parameter list of setsurfaces" );
        if ( tag->GetEvalType(E) != ScriptNode::STRING )
            V2D_THROW( "syntax error: the tag expression in the surface parameter list must evaluate to type STRING" );

        ExprNode *val = (*i)->m_value;
        if ( val->GetEvalType(E) != ScriptNode::OBJECT )
            V2D_THROW( "the type of the value paramters to setsurfaces() must evaluate to type OBJECT" );

        // get the object
        UINT32 handle = val->GetObjectEvaluation(E);
        V2dSurface *surfObj = m_v2d->GetObjectFromHandleSurf( handle );
        argMap[ tag->GetStringEvaluation(E) ] = surfObj;
        ++ i;
    }

    // call the object to set up its surfaces
    //
    obj->SetSurfaces( argMap );

    // no return value
    fcn->m_evaluatiolwalue = "";
}

// Defined() returns true if a string is in the symbol table
// this lets scripts give default values to command-line parameters
// that are not set by caller

void Function::Defined( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() != 1 )
        V2D_THROW( "defined() takes exactly one parameter" );

    ExprNode *e = (*i)->m_value;
    if ( e->GetEvalType(E) != ScriptNode::STRING )
        V2D_THROW( "the type of the paramter to defined() must evaluate to type STRING" );

    // get the symbol
    string sym = e->GetStringEvaluation(E);

    bool value = fcn->ExistingSymbol( E, sym );
    fcn->m_evaluatiolwalue = fcn->IntToTypedValueString( E, value );
}

// WaitForIdle() waits for idle
// no parameters, no return value

void Function::WaitForIdle( EvalElw &E, FuncCallNode *fcn )
{
    V2dClassObj *class_obj = 0;

    // check parameters
    //
    if ( fcn->m_params && fcn->m_params->m_paramlist.size() >=1 )
    {
        list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();
        ExprNode *e = (*i)->m_value;
        if ( e->GetEvalType(E) != ScriptNode::OBJECT )
        {
            V2D_THROW( "the type of the first paramter to waitforidle() must evaluate to type OBJECT" );
        }

        // get the object
        UINT32 handle = e->GetObjectEvaluation(E);
        class_obj = m_v2d->GetObjectFromHandle( handle );

        if (!class_obj)
        {
            V2D_THROW("Could not get object for wait_for_idle command");
        }

        class_obj->WaitForIdle();
    }
    else
    {
        // Still using the original approach
        m_v2d->WaitForIdle();
    }

    // no return value
    fcn->m_evaluatiolwalue = "";
}

// PM_Start() starts the performance monitor
// takes class object as parameter
// no return value

void Function::PM_Start( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params && fcn->m_params->m_paramlist.size() != 1 )
        V2D_THROW( "pmstart() takes 1 parameter: "
                   << "a class object with PM_TRIGGER method" );

    ExprNode *e = (*i)->m_value;
    if ( e->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of the first paramter to pmstart() must "
                   << "evaluate to type OBJECT" );

    // get the object
    UINT32 handle = e->GetObjectEvaluation(E);
    V2dClassObj *obj = m_v2d->GetObjectFromHandle( handle );

    if ( m_v2d->GetLWGpu()->PerfMon() ) {
        if( gXML )
        {
            XD->XMLStartElement("<PmTrigger");
            XD->outputAttribute("index",  0);
            XD->endAttributes();
        }

        m_v2d->GetLWGpu()->PerfMonStart(
                                m_v2d->GetChannel(),
                                0,
                                obj->GetClassId() );
    }
    // no return value
    fcn->m_evaluatiolwalue = "";
}

// PM_Ends() ends the performance monitor
// takes class object as parameter
// no return value

void Function::PM_End( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params && fcn->m_params->m_paramlist.size() != 1 )
        V2D_THROW( "pmend() takes 1 parameter: "
                   << "a class object with PM_TRIGGER method" );

    ExprNode *e = (*i)->m_value;
    if ( e->GetEvalType(E) != ScriptNode::OBJECT )
        V2D_THROW( "the type of the first paramter to pmend() must "
                   << "evaluate to type OBJECT" );

    // get the object
    UINT32 handle = e->GetObjectEvaluation(E);
    V2dClassObj *obj = m_v2d->GetObjectFromHandle( handle );

    if ( m_v2d->GetLWGpu()->PerfMon() ) {
        m_v2d->GetLWGpu()->PerfMonEnd(
                                m_v2d->GetChannel(),
                                0,
                                obj->GetClassId() );
        if( gXML )
            XD->XMLEndLwrrent();
    }

    // no return value
    fcn->m_evaluatiolwalue = "";
}

void Function::Ratio2FixedInt( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() != 2 )
        V2D_THROW( "ratio2fixedInt takes exactly two parameters" );

    ExprNode *e;

    e = (*i)->m_value;
    if ( e->GetEvalType(E) != ScriptNode::NUMBER )
        V2D_THROW( "the parameters to ratio2fixedInt must be numbers" );
    UINT32 a = e->GetIntEvaluation(E);

    i++;

    e = (*i)->m_value;
    if ( e->GetEvalType(E) != ScriptNode::NUMBER )
        V2D_THROW( "the parameters to ratio2fixedInt must be numbers" );
    UINT32 b = e->GetIntEvaluation(E);

    // return integer portion of a / b:
    fcn->m_evaluatiolwalue = fcn->IntToTypedValueString( E, a / b );
}

void Function::Ratio2FixedFrac( EvalElw &E, FuncCallNode *fcn )
{
    list<ParameterNode *>::iterator i = fcn->m_params->m_paramlist.begin();

    // check parameters
    //
    if ( fcn->m_params->m_paramlist.size() != 2 )
        V2D_THROW( "ratio2fixedInt takes exactly two parameters" );

    ExprNode *e;

    e = (*i)->m_value;
    if ( e->GetEvalType(E) != ScriptNode::NUMBER )
        V2D_THROW( "the parameters to ratio2fixedInt must be numbers" );
    UINT32 a = e->GetIntEvaluation(E);

    i++;

    e = (*i)->m_value;
    if ( e->GetEvalType(E) != ScriptNode::NUMBER )
        V2D_THROW( "the parameters to ratio2fixedInt must be numbers" );
    UINT32 b = e->GetIntEvaluation(E);

    // return fractional portion of a / b:
    UINT64 f = (a % b);
    fcn->m_evaluatiolwalue = fcn->IntToTypedValueString( E, UINT32((f<<32)/b) );
}

//
// requires that there be an oclwrence of <tag>=<int value> in the parameter list
// and returns the int value
//

int Function::GetTaggedParameterIntValue( EvalElw &E, const string &tag,
                                          list<ParameterNode*> &paramList )
{
    ParameterNode *p = GetTaggedParameter( E, tag, paramList );
    if ( p == 0 )
        V2D_THROW( "no oclwrence of tag " << tag << " in the parameter list" );
    if ( p->m_value->GetEvalType(E) != ScriptNode::NUMBER )
        V2D_THROW( "the value expr of tag " << tag <<
                   " is required to evaluate to NUMBER, instead it is: " <<
                   p->m_value->GetEvaluation() );

    return p->m_value->GetIntEvaluation(E);
}

string Function::GetTaggedParameterStringValue( EvalElw &E, const string &tag,
                                                list<ParameterNode*> &paramList )
{
    ParameterNode *p = GetTaggedParameter( E, tag, paramList );
    if ( p == 0 )
        V2D_THROW( "no oclwrence of tag " << tag << " in the parameter list" );
    if ( p->m_value->GetEvalType(E) != ScriptNode::STRING )
        V2D_THROW( "the value expr of tag " << tag <<
                   " is required to evaluate to STRING, instead it is: " <<
                   p->m_value->GetEvaluation() );

    return p->m_value->GetStringEvaluation(E);
}

ParameterNode * Function::GetTaggedParameter( EvalElw &E, const string &tag,
                                              list<ParameterNode*> &paramList )
{
    list<ParameterNode*>::iterator i;

    for ( i = paramList.begin(); i != paramList.end(); i++ )
    {
        ExprNode *tagExpr = (*i)->m_tag;
        if ( (tagExpr != 0) && (tagExpr->GetStringEvaluation(E) == tag) )
            return *i;
    }
    return 0;
}

//
// returns true IFF a tagged parameter is present in the paramlist
//

bool Function::TaggedParameterPresent( EvalElw &E, const string &tag,
                                       list<ParameterNode*> &paramList )
{
    ParameterNode *p = GetTaggedParameter( E, tag, paramList );
    return p != 0;
}

//packs src value into specified number of bits
unsigned int FitTo(unsigned int src, unsigned int srcBits, unsigned int dstBits)
{
    int diff = srcBits-dstBits;
    return( (diff>0) ? src>>diff : src<<(-diff));

}
