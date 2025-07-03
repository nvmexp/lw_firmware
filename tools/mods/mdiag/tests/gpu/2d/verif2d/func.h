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
#ifndef _FUNC_H_
#define _FUNC_H_

class Function
{
public:
    Function( void );
    void Call( EvalElw &E, FuncCallNode *fcn );
    void SetV2d( Verif2d *v2d ) { m_v2d = v2d; }

private:
    Verif2d *m_v2d;

    void Random(             EvalElw &E, FuncCallNode *fcn );
    void PrintVar(           EvalElw &E, FuncCallNode *fcn );
    void Object(             EvalElw &E, FuncCallNode *fcn );
    void Set(                EvalElw &E, FuncCallNode *fcn );
    void Launch(             EvalElw &E, FuncCallNode *fcn );
    void Surface(            EvalElw &E, FuncCallNode *fcn );
    void Clear(              EvalElw &E, FuncCallNode *fcn );
    void DumpTGA(            EvalElw &E, FuncCallNode *fcn );
    void DumpImage(          EvalElw &E, FuncCallNode *fcn );
    void LoadTGA(            EvalElw &E, FuncCallNode *fcn );
    void CheckCrc(           EvalElw &E, FuncCallNode *fcn );
    void BPP(                EvalElw &E, FuncCallNode *fcn );
    void SetSurfaces(        EvalElw &E, FuncCallNode *fcn );
    void SendInlineData(     EvalElw &E, FuncCallNode *fcn );
    void SendInlineDataArray(EvalElw &E, FuncCallNode *fcn );
    void SetRenderEnable(    EvalElw &E, FuncCallNode *fcn );
    void Defined(            EvalElw &E, FuncCallNode *fcn );
    void WaitForIdle(        EvalElw &E, FuncCallNode *fcn );
    void PM_Start(           EvalElw &E, FuncCallNode *fcn );
    void PM_End(             EvalElw &E, FuncCallNode *fcn );
    void ColwertColorFormat( EvalElw &E, FuncCallNode *fcn );
    void BPP_Format(         EvalElw &E, FuncCallNode *fcn );
    void Width(              EvalElw &E, FuncCallNode *fcn );
    void Height(             EvalElw &E, FuncCallNode *fcn );
    void XY_ToOffset(        EvalElw &E, FuncCallNode *fcn );
    void Format(             EvalElw &E, FuncCallNode *fcn );
    void Print(              EvalElw &E, FuncCallNode *fcn );
    void Ratio2FixedInt(     EvalElw &E, FuncCallNode *fcn );
    void Ratio2FixedFrac(    EvalElw &E, FuncCallNode *fcn );
    void OffsetLower(        EvalElw &E, FuncCallNode *fcn );
    void OffsetUpper(        EvalElw &E, FuncCallNode *fcn );

    // parameter list helper functions
    void WalkMethodParamList( EvalElw &E, FuncCallNode *fcn, V2dClassObj *obj );
    bool TaggedParameterPresent( EvalElw &E, const string &tag,
                                 list<ParameterNode*> &paramList );
    ParameterNode * GetTaggedParameter( EvalElw &E, const string &tag, list<ParameterNode*> &paramList );
    string GetTaggedParameterStringValue( EvalElw &E, const string &tag, list<ParameterNode*> &paramList );
    int GetTaggedParameterIntValue( EvalElw &E, const string &tag, list<ParameterNode*> &paramList );

};

#endif // #ifndef _FUNC_H_
