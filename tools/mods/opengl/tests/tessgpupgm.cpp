/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2011, 2013-2014, 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// FragmentGpuProgramGrammar4_0
//      Fragment program generator for GLRandom. This class follows the
//      Bakus-Nuar form to specify the grammar.
//
// 45678901234567890123456789012345678901234567890123456789012345678901234567890
#include "glrandom.h"  // declaration of our namespace
#include "pgmgen.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include <math.h>

using namespace GLRandom;

// This array stores the IO attributes for each ProgProperty (see glr_comm.h).
// Each property uses 2 bits, we lwrrently have 81 properties so we need 162
// bits.
const static UINT64 s_TsppIOAttribs[] =
{
    (
    (UINT64)ppIORw          << (ppPos*2) |
    (UINT64)ppIONone        << (ppNrml*2) |
    (UINT64)ppIORw          << (ppPriColor*2) |
    (UINT64)ppIORw          << (ppSecColor*2) |
    (UINT64)ppIORw          << (ppBkPriColor*2) |
    (UINT64)ppIORw          << (ppBkSecColor*2) |
    (UINT64)ppIORw          << (ppFogCoord*2) |
    (UINT64)ppIORw          << (ppPtSize*2) |
    (UINT64)ppIORw          << (ppVtxId*2) |
    (UINT64)ppIONone        << (ppInstance*2) |
    (UINT64)ppIONone        << (ppFacing*2) |
    (UINT64)ppIONone        << (ppDepth*2) |
    (UINT64)ppIORw          << (ppPrimId*2) |
    (UINT64)ppIOWr          << (ppLayer*2) |
    (UINT64)ppIOWr          << (ppViewport*2) |
    (UINT64)ppIORw          << (ppPrimitive*2) |
    (UINT64)ppIONone        << (ppFullyCovered*2) |
    (UINT64)ppIONone        << (ppBaryCoord*2) |
    (UINT64)ppIONone        << (ppBaryNoPersp*2) |
    (UINT64)ppIONone        << (ppShadingRate*2) |
    (UINT64)ppIORw          << (ppClip0*2) |
    (UINT64)ppIORw          << ((ppClip0+1)*2) |
    (UINT64)ppIORw          << ((ppClip0+2)*2) |
    (UINT64)ppIORw          << ((ppClip0+3)*2) |
    (UINT64)ppIORw          << ((ppClip0+4)*2) |
    (UINT64)ppIORw          << (ppClip5*2) |
    (UINT64)ppIORw          << (ppGenAttr0*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+1)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+2)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+3)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+4)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+5)*2)
    ),
    (
    (UINT64)ppIORw          << ((ppGenAttr0+6 -32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+7 -32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+8 -32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+9 -32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+10-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+11-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+12-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+13-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+14-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+15-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+16-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+17-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+18-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+19-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+20-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+21-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+22-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+23-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+24-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+25-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+26-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+27-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+28-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+29-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr0+30-32)*2) |
    (UINT64)ppIORw          << ((ppGenAttr31  -32)*2) |
    (UINT64)ppIONone        << ((ppClrBuf0+0  -32)*2) |
    (UINT64)ppIONone        << ((ppClrBuf0+1  -32)*2) |
    (UINT64)ppIONone        << ((ppClrBuf0+2  -32)*2) |
    (UINT64)ppIONone        << ((ppClrBuf0+3  -32)*2) |
    (UINT64)ppIONone        << ((ppClrBuf0+4  -32)*2) |
    (UINT64)ppIONone        << ((ppClrBuf0+5  -32)*2)
    ),
    (
    (UINT64)ppIONone        << ((ppClrBuf0+6  -64)*2) |
    (UINT64)ppIONone        << ((ppClrBuf0+7  -64)*2) |
    (UINT64)ppIONone        << ((ppClrBuf0+8  -64)*2) |
    (UINT64)ppIONone        << ((ppClrBuf0+9  -64)*2) |
    (UINT64)ppIONone        << ((ppClrBuf0+10 -64)*2) |
    (UINT64)ppIONone        << ((ppClrBuf0+11 -64)*2) |
    (UINT64)ppIONone        << ((ppClrBuf0+12 -64)*2) |
    (UINT64)ppIONone        << ((ppClrBuf0+13 -64)*2) |
    (UINT64)ppIONone        << ((ppClrBuf0+14 -64)*2) |
    (UINT64)ppIONone        << ((ppClrBuf15   -64)*2) |
    (UINT64)ppIORw          << ((ppTxCd0+0    -64)*2) |
    (UINT64)ppIORw          << ((ppTxCd0+1    -64)*2) |
    (UINT64)ppIORw          << ((ppTxCd0+2    -64)*2) |
    (UINT64)ppIORw          << ((ppTxCd0+3    -64)*2) |
    (UINT64)ppIORw          << ((ppTxCd0+4    -64)*2) |
    (UINT64)ppIORw          << ((ppTxCd0+5    -64)*2) |
    (UINT64)ppIORw          << ((ppTxCd0+6    -64)*2) |
    (UINT64)ppIORw          << ((ppTxCd7      -64)*2)
    )
};

//------------------------------------------------------------------------------
TessGpuPgmGen::TessGpuPgmGen
(
    GLRandomTest * pGLRandom,
    FancyPickerArray *pPicker,
    pgmKind kind
)
 :  GpuPgmGen(pGLRandom, pPicker, kind)
{
}

//------------------------------------------------------------------------------
TessGpuPgmGen::~TessGpuPgmGen()
{
}

//------------------------------------------------------------------------------
// We don't support truly random tessellation programs, stub stuff out.
//------------------------------------------------------------------------------
void TessGpuPgmGen::InitMailwars
(
    Block * pMainBlock
)
{
    MASSERT(!"Random tessellation programs not supported");
}

void TessGpuPgmGen::InitPgmElw(GLRandom::PgmRequirements *pRqmts)
{
    MASSERT(!"Random tessellation programs not supported");
}

//------------------------------------------------------------------------------
//
int TessGpuPgmGen::GetPropIOAttr(ProgProperty prop)const
{
    if (prop >= ppPos && prop <= ppTxCd7)
    {
        // 32 entries per UINT64
        const int index = prop / 32;
        const int shift = prop % 32;
        return ((s_TsppIOAttribs[index] >> shift*2) & 0x3);
    }
    return ppIONone;
}

int TessGpuPgmGen::GetConstReg() const        { return 0; }
int TessGpuPgmGen::GetPaboReg() const         { return 0; }
int TessGpuPgmGen::GetSubsReg() const         { return 0; }
int TessGpuPgmGen::GetIndexReg(IndexRegType t) const        { return 0; }
bool TessGpuPgmGen::IsIndexReg(int id) const  { return false; }
int TessGpuPgmGen::GetFirstTempReg() const    { return 0; }
int TessGpuPgmGen::GetGarbageReg() const      { return 0; }
int TessGpuPgmGen::GetLiteralReg() const      { return 0; }
int TessGpuPgmGen::GetFirstTxCdReg() const    { return 0; }
int TessGpuPgmGen::GetLastTxCdReg() const     { return 0; }
int TessGpuPgmGen::GetLocalElwReg() const     { return 0; }
int TessGpuPgmGen::GetConstVectReg() const    { return 0; }
int TessGpuPgmGen::GetNumTempRegs() const     { return 0; }
int TessGpuPgmGen::GetMinOps()   const        { return 0; }
int TessGpuPgmGen::GetMaxOps()   const        { return 0; }

GLRandom::ProgProperty TessGpuPgmGen::GetProp(int reg) const
{
    return ppIlwalid;
}
string  TessGpuPgmGen::GetRegName(int reg) const
{
    return "Unknown";
}
int TessGpuPgmGen::GetTxCd(int reg) const     { return -1; }
int TessGpuPgmGen::PickAbs() const            { return 0; }
int TessGpuPgmGen::PickNeg() const            { return 0; }
int TessGpuPgmGen::PickMultisample() const    { return 0; }
int TessGpuPgmGen::PickOp() const             { return 0; }
int TessGpuPgmGen::PickPgmTemplate() const    { return 0; }
int TessGpuPgmGen::PickRelAddr() const        { return 0; }
int TessGpuPgmGen::PickNumOps() const         { return 0; }
int TessGpuPgmGen::PickResultReg() const      { return 0; }
int TessGpuPgmGen::PickTempReg() const        { return 0; }
int TessGpuPgmGen::PickTxOffset() const       { return 0; }
int TessGpuPgmGen::PickTxTarget() const       { return 0; }

//------------------------------------------------------------------------------
TcGpuPgmGen::TcGpuPgmGen (GLRandomTest * pGLRandom, FancyPickerArray *pPicker)
 : TessGpuPgmGen(pGLRandom, pPicker, pkTessCtrl)
{
}

//------------------------------------------------------------------------------
TcGpuPgmGen::~TcGpuPgmGen()
{
}

//------------------------------------------------------------------------------
void TcGpuPgmGen::GenerateRandomProgram
(
    GLRandom::PgmRequirements *pRqmts,
    Programs * pProgramStorage,
    const string & pgmName
)
{
    // Generate a program that passes through the requested properties to
    // the downstream tess-eval or geometry or fragment program.

    if (pRqmts->PrimitiveOut != GL_PATCHES &&
        pRqmts->PrimitiveOut != -1)
    {
        // Downstream program is a geometry program that doesn't expect patches.
        // We can't enable a tess-control program in this chain.
        GenerateDummyProgram(pProgramStorage,PgmGen::pkTargets[pkTessCtrl]);
        return;
    }

    if (pRqmts->VxPerPatchOut == -1)
    {
        // A value of -1 indicates a don't care condition. So we must be
        // configured without a downstream Tess-Eval or Geometry program.
        // In this case the patch will be treated as GL_POINTS.
        pRqmts->VxPerPatchIn = m_pFpCtx->Rand.GetRandom(2,32);
        pRqmts->VxPerPatchOut = pRqmts->VxPerPatchIn;
    }
    else if (pRqmts->VxPerPatchOut < 2 || pRqmts->VxPerPatchOut > 32)
    {
        pRqmts->VxPerPatchOut = 4;
    }

    Program prog;
    prog.Name = pgmName;
    GenerateProgram(pRqmts, &prog);

    pProgramStorage->AddProg(psRandom, prog);
}

//------------------------------------------------------------------------------
void TcGpuPgmGen::GenerateFixedPrograms
(
    Programs * pProgramStorage
)
{
    PgmRequirements reqs;
    reqs.Outputs[ppPos]           = omXYZW;
    reqs.Outputs[ppNrml]          = omXYZ;
    reqs.Outputs[ppPriColor]      = omXYZW;
    reqs.Outputs[ppSecColor]      = omXYZW;
    reqs.Outputs[ppBkPriColor]    = omXYZW;
    reqs.Outputs[ppBkSecColor]    = omXYZW;
    reqs.Outputs[ppFogCoord]      = omX;
    reqs.Outputs[ppPtSize]        = omX;
    for (int i=ppTxCd0; i<=ppTxCd7; i++)
    {
        reqs.Outputs[(ProgProperty)i] = omXYZW | omPassThru;
    }

    // Load simple passthru programs that pull tess-rates from pgm-elw,
    // for various patch sizes.
    //
    // These are just like our "random" programs, but they pass through a large
    // arbitrary list of vertex properties instead of the minimum required.

    for (int patchSize = 2; patchSize <= 8; patchSize++)
    {
        reqs.VxPerPatchOut = patchSize;

        Program prog;
        GenerateProgram(&reqs, &prog);

        pProgramStorage->AddProg(psFixed, prog);
    }
    MASSERT(pProgramStorage->NumProgs(psFixed) ==
            RND_GPU_PROG_TC_INDEX_NumFixedProgs);
}

//------------------------------------------------------------------------------
struct PassThruStrings
{
    int          prop;
    const char * sOut;
    const char * sIn;
};
void TcGpuPgmGen::GenerateProgram
(
    GLRandom::PgmRequirements *pRqmts,
    Program * pProgram
)
{
    // Generate a program that passes through the requested properties to
    // the downstream tess-eval or geometry or fragment program.

    pProgram->PgmRqmt = *pRqmts;
    pProgram->PgmRqmt.PrimitiveIn = GL_PATCHES;
    pProgram->PgmRqmt.PrimitiveOut = GL_PATCHES;
    pProgram->PgmRqmt.VxPerPatchIn = pProgram->PgmRqmt.VxPerPatchOut;
    pProgram->PgmRqmt.Outputs[ppPos] = omXYZW;

    string pgmStr = Utility::StrPrintf(
        "!!LWtcp5.0\n"
        "# passthru and set tess-rate from pgm-elw.\n"
        "VERTICES_OUT %d;\n"
        "PARAM pgmElw[256] = { program.elw[0..255] };\n"
        "ATTRIB pos = vertex.in.position;\n"
        "ATTRIB nrm = vertex.in.attrib[6];\n"
        "ATTRIB c0  = vertex.in.color.primary;\n"
        "ATTRIB c1  = vertex.in.color.secondary;\n"
        "ATTRIB c0b = vertex.in.color.back.primary;\n"
        "ATTRIB c1b = vertex.in.color.back.secondary;\n"
        "ATTRIB fog = vertex.in.fogcoord;\n"
        "ATTRIB psz = vertex.in.pointsize;\n"
        "ATTRIB txc[] = { vertex.in.texcoord[0..7] };\n"
        "ATTRIB clp[] = { vertex.in.clip[0..5] };\n"
        "TEMP i;\n"
        "TEMP t;\n"
        "main:\n"
        " MOV.U i.x, primitive.invocation;\n",
        pProgram->PgmRqmt.VxPerPatchOut);

    static const PassThruStrings passThruStrings [] =
    {
        {   ppPos,          "position",             "pos[i.x]" }
        ,{  ppNrml,         "attrib[6].xyz",        "nrm[i.x]" }
        ,{  ppPriColor,     "color.primary",        "c0[i.x]"  }
        ,{  ppSecColor,     "color.secondary",      "c1[i.x]"  }
        ,{  ppBkPriColor,   "color.back.primary",   "c0b[i.x]" }
        ,{  ppBkSecColor,   "color.back.secondary", "c1b[i.x]" }
        ,{  ppFogCoord,     "fogcoord.x",           "fog[i.x]" }
        ,{  ppPtSize,       "pointsize.x",          "psz[i.x]" }
        ,{  ppClip0,        "clip[0].x",            "clp[i.x][0]" }
        ,{  ppClip0+1,      "clip[1].x",            "clp[i.x][1]" }
        ,{  ppClip0+2,      "clip[2].x",            "clp[i.x][2]" }
        ,{  ppClip0+3,      "clip[3].x",            "clp[i.x][3]" }
        ,{  ppClip0+4,      "clip[4].x",            "clp[i.x][4]" }
        ,{  ppClip0+5,      "clip[5].x",            "clp[i.x][5]" }
        ,{  ppTxCd0,        "texcoord[0]",          "txc[i.x][0]" }
        ,{  ppTxCd0+1,      "texcoord[1]",          "txc[i.x][1]" }
        ,{  ppTxCd0+2,      "texcoord[2]",          "txc[i.x][2]" }
        ,{  ppTxCd0+3,      "texcoord[3]",          "txc[i.x][3]" }
        ,{  ppTxCd0+4,      "texcoord[4]",          "txc[i.x][4]" }
        ,{  ppTxCd0+5,      "texcoord[5]",          "txc[i.x][5]" }
        ,{  ppTxCd0+6,      "texcoord[6]",          "txc[i.x][6]" }
        ,{  ppTxCd0+7,      "texcoord[7]",          "txc[i.x][7]" }
    };
    constexpr int numPassThruStrings = static_cast<int>(NUMELEMS(passThruStrings));

    for (PgmRequirements::ConstPropMapItr iter = pProgram->PgmRqmt.Outputs.begin();
         iter != pProgram->PgmRqmt.Outputs.end();
             iter++)
    {
        const ProgProperty prop = iter->first;
        const int mask = iter->second;

        for (int i = 0; i < numPassThruStrings; i++)
        {
            if (passThruStrings[i].prop == prop)
            {
                pgmStr += Utility::StrPrintf(" MOV result.%s, %s;\n",
                        passThruStrings[i].sOut,
                        passThruStrings[i].sIn);
                pProgram->PgmRqmt.Inputs[prop] = mask;
                break;
            }
        }
    }

    pgmStr += Utility::StrPrintf(
        " SUB.CC0 t, i, 0;\n"
        " IF EQ0.x;\n"
        "  MOV result.patch.tessouter[0], pgmElw[%d].x;\n"
        "  MOV result.patch.tessouter[1], pgmElw[%d].y;\n"
        "  MOV result.patch.tessouter[2], pgmElw[%d].z;\n"
        "  MOV result.patch.tessouter[3], pgmElw[%d].w;\n"
        "  MOV result.patch.tessinner[0], pgmElw[%d].x;\n"
        "  MOV result.patch.tessinner[1], pgmElw[%d].y;\n"
        " ENDIF;\n"
        "END\n",
        TessOuterReg,
        TessOuterReg,
        TessOuterReg,
        TessOuterReg,
        TessInnerReg,
        TessInnerReg);

    pProgram->Pgm = pgmStr;
    pProgram->Target = PgmGen::pkTargets[pkTessCtrl];
    pProgram->CalcXfbAttributes();
}

//------------------------------------------------------------------------------
TeGpuPgmGen::TeGpuPgmGen (GLRandomTest * pGLRandom, FancyPickerArray *pPicker)
 : TessGpuPgmGen(pGLRandom, pPicker, pkTessEval)
{
}

//------------------------------------------------------------------------------
TeGpuPgmGen::~TeGpuPgmGen()
{
}

//------------------------------------------------------------------------------
void TeGpuPgmGen::GenerateFixedPrograms
(
    Programs * pProgramStorage
)
{
    PgmRequirements reqs;
    reqs.Outputs[ppPos]           = omXYZW;
    reqs.Outputs[ppPriColor]      = omXYZW;
    reqs.Outputs[ppSecColor]      = omXYZW;
    reqs.Outputs[ppBkPriColor]    = omXYZW;
    reqs.Outputs[ppBkSecColor]    = omXYZW;
    reqs.Outputs[ppFogCoord]      = omX;
    reqs.Outputs[ppPtSize]        = omX;
    for (int i=ppTxCd0; i<=ppTxCd7; i++)
    {
        reqs.Outputs[(ProgProperty)i] = omXYZW | omPassThru;
    }
    reqs.Outputs[ppNrml]          = omXYZ;

    {
        reqs.PrimitiveOut = GL_TRIANGLES;
        Program prog;
        GenerateProgram(&reqs, GL_QUADS, GL_EQUAL, false, &prog);
        prog.Name = "teFixQuadEqCcw";
        pProgramStorage->AddProg(psFixed, prog);
    }
    {
        reqs.PrimitiveOut = GL_TRIANGLES;
        Program prog;
        GenerateProgram(&reqs, GL_TRIANGLES, GL_FRACTIONAL_EVEN, true, &prog);
        prog.Name = "teFixTriEvenCw";
        pProgramStorage->AddProg(psFixed, prog);
    }
    {
        reqs.PrimitiveOut = GL_LINES;
        Program prog;
        GenerateProgram(&reqs, GL_ISOLINES, GL_EQUAL, true, &prog);
        prog.Name = "teFixLine";
        pProgramStorage->AddProg(psFixed, prog);
    }
    {
        reqs.PrimitiveOut = GL_POINTS;
        Program prog;
        GenerateProgram(&reqs, GL_TRIANGLES, GL_FRACTIONAL_ODD, true, &prog);
        prog.Name = "teFixTriPoint";
        pProgramStorage->AddProg(psFixed, prog);
    }
    MASSERT(pProgramStorage->NumProgs(psFixed) ==
            RND_GPU_PROG_TE_INDEX_NumFixedProgs);
}

//------------------------------------------------------------------------------
void TeGpuPgmGen::GenerateRandomProgram
(
    GLRandom::PgmRequirements *pRqmts,
    Programs * pProgramStorage,
    const string & pgmName
)
{
    // We can generate vertices from a 3-vx patch (tri) or a 4-vx patch (quad).
    // Tri mode works for triangles or points.
    // Quad mode works for triangles, lines, or points.
    GLenum tessMode    = (*m_pPickers)[RND_GPU_PROG_TE_TESS_MODE].Pick();
    GLenum tessSpacing = (*m_pPickers)[RND_GPU_PROG_TE_TESS_SPACING].Pick();
    bool   isClockwise = 0 != (*m_pPickers)[RND_GPU_PROG_TE_CW].Pick();
    GLenum randomPrim = GL_TRIANGLES;

    switch (tessMode)
    {
        default:
            tessMode = GL_TRIANGLES;
            // fall through, bad picker setup
        case GL_QUADS:
        case GL_TRIANGLES:
            break;

        case GL_ISOLINES:
            randomPrim = GL_LINES;
            break;
    }
    if ((*m_pPickers)[RND_GPU_PROG_TE_TESS_POINT_MODE].Pick())
        randomPrim = GL_POINTS;

    switch (pRqmts->PrimitiveOut)
    {
        case -1:
            // Downstream fragment program accepts any primitive.
            // Use the random one selected above.
            pRqmts->PrimitiveOut = randomPrim;
            break;

        case GL_LINES:
            tessMode = GL_ISOLINES;
            break;

        case GL_POINTS:
        case GL_TRIANGLES:
            if (tessMode == GL_ISOLINES)
                tessMode = GL_QUADS;
            // Downstream geometry program expects one of the primitive types
            // that tess-eval programs can generate.
            break;

        default:
            // Downstream geometry program expects a primitive type
            // that tess-eval programs cannot generate.
            // Disable tess-eval for this random chain.
            GenerateDummyProgram(pProgramStorage,PgmGen::pkTargets[pkTessEval]);
            return;
    }

    m_ThisProgWritesLayerVport = (0 != (*m_pPickers)[RND_GPU_PROG_TE_WRITES_TO_LAYR_VPORT].Pick());

    Program prog;
    prog.Name = pgmName;
    GenerateProgram(pRqmts, tessMode, tessSpacing, isClockwise, &prog);
    pProgramStorage->AddProg(psRandom, prog);
}

//------------------------------------------------------------------------------
struct TePassThruStrings
{
    int          prop;
    const char * sOut;
    const char * sIn;
};
static const TePassThruStrings s_TePassThruStrings[] =
{
     {  ppPriColor,     "color.primary",        "c0[%d]"  }
    ,{  ppSecColor,     "color.secondary",      "c1[%d]"  }
    ,{  ppBkPriColor,   "color.back.primary",   "c0b[%d]" }
    ,{  ppBkSecColor,   "color.back.secondary", "c1b[%d]" }
    ,{  ppFogCoord,     "fogcoord.x",           "fog[%d]" }
    ,{  ppClip0,        "clip[0].x",            "clp[%d][0]" }
    ,{  ppClip0+1,      "clip[1].x",            "clp[%d][1]" }
    ,{  ppClip0+2,      "clip[2].x",            "clp[%d][2]" }
    ,{  ppClip0+3,      "clip[3].x",            "clp[%d][3]" }
    ,{  ppClip0+4,      "clip[4].x",            "clp[%d][4]" }
    ,{  ppClip0+5,      "clip[5].x",            "clp[%d][5]" }
    ,{  ppTxCd0,        "texcoord[0]",          "txc[%d][0]" }
    ,{  ppTxCd0+1,      "texcoord[1]",          "txc[%d][1]" }
    ,{  ppTxCd0+2,      "texcoord[2]",          "txc[%d][2]" }
    ,{  ppTxCd0+3,      "texcoord[3]",          "txc[%d][3]" }
    ,{  ppTxCd0+4,      "texcoord[4]",          "txc[%d][4]" }
    ,{  ppTxCd0+5,      "texcoord[5]",          "txc[%d][5]" }
    ,{  ppTxCd0+6,      "texcoord[6]",          "txc[%d][6]" }
    ,{  ppTxCd0+7,      "texcoord[7]",          "txc[%d][7]" }
};
static constexpr int s_NumTePassThruStrings = static_cast<int>(NUMELEMS(s_TePassThruStrings));
// This static assert needs to be updated if we ever add elements to the array
// above.  We keep it here to avoid a compiler bug we hit in the past where the
// array size computed by NUMELEMS was 0.
static_assert(s_NumTePassThruStrings == 19, "Incorrect number of elements in s_TePassThruStrings");

//------------------------------------------------------------------------------
void TeGpuPgmGen::GenerateProgram
(
    GLRandom::PgmRequirements *pRqmts,
    GLenum    tessMode,
    GLenum    tessSpacing,
    bool      isClockwise,
    Program * pProgram
)
{
    // Generate a program that passes through the requested properties to
    // the downstream geometry or fragment program.
    pProgram->PgmRqmt = *pRqmts;
    pProgram->PgmRqmt.PrimitiveIn = GL_PATCHES;
    pProgram->PgmRqmt.Inputs[ppNrml] = omXYZ;
    pProgram->PgmRqmt.Inputs[ppPos] = omXYZW;
    pProgram->PgmRqmt.Outputs[ppPos] = omXYZW;

    string pgmStr = "!!LWtep5.0\n";

    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_viewport_array2) &&
        m_pGLRandom->GetNumLayersInFBO() > 0 &&
        m_ThisProgWritesLayerVport)
    {
        pgmStr += "OPTION LW_viewport_array2;\n";
    }

    switch (tessMode)
    {
        default:
            tessMode = GL_QUADS;
            // fall through
        case GL_QUADS:
            pgmStr += "TESS_MODE QUADS;\n";
            pProgram->PgmRqmt.VxPerPatchIn = 4;
            break;

        case GL_TRIANGLES:
            pgmStr += "TESS_MODE TRIANGLES;\n";
            pProgram->PgmRqmt.VxPerPatchIn = 3;
            break;

        case GL_ISOLINES:
            pgmStr += "TESS_MODE ISOLINES;\n";
            pProgram->PgmRqmt.VxPerPatchIn = 4;
            break;

        case GL_LINES:
            // GL requires ISOLINES with line primitives, should have been
            // forced before now.
            MASSERT(tessMode == GL_ISOLINES);
            break;
    }
    pProgram->TessMode = tessMode;

    switch (tessSpacing)
    {
        default:
            MASSERT(!"Unexpected tessSpacing");
            // fall through
        case GL_EQUAL:
            pgmStr += "TESS_SPACING EQUAL;\n";
            break;
        case GL_FRACTIONAL_ODD:
            pgmStr += "TESS_SPACING FRACTIONAL_ODD;\n";
            break;
        case GL_FRACTIONAL_EVEN:
            pgmStr += "TESS_SPACING FRACTIONAL_EVEN;\n";
            break;
    }
    if (pProgram->PgmRqmt.PrimitiveOut == GL_TRIANGLES)
    {
        pgmStr += isClockwise ? "TESS_VERTEX_ORDER CW;\n" :
            "TESS_VERTEX_ORDER CCW;\n";
    }
    else if (pProgram->PgmRqmt.PrimitiveOut == GL_POINTS)
    {
        pgmStr += "TESS_POINT_MODE;\n";
    }

    // Declare our inputs and temps.
    pgmStr +=
        "PARAM pgmElw[256] = { program.elw[0..255] };\n"
        "ATTRIB pos = vertex.in.position;\n"
        "ATTRIB nrm = vertex.in.attrib[6];\n"
        "ATTRIB c0  = vertex.in.color.primary;\n"
        "ATTRIB c1  = vertex.in.color.secondary;\n"
        "ATTRIB c0b = vertex.in.color.back.primary;\n"
        "ATTRIB c1b = vertex.in.color.back.secondary;\n"
        "ATTRIB fog = vertex.in.fogcoord;\n"
        "ATTRIB psz = vertex.in.pointsize;\n"
        "ATTRIB txc[] = { vertex.in.texcoord[0..7] };\n"
        "ATTRIB clp[] = { vertex.in.clip[0..5] };\n"
        "ATTRIB tc = vertex.tesscoord;\n"
        "TEMP n; # normal;\n"
        "TEMP p; # position;\n"
        "TEMP r;\n";

    if (tessMode == GL_TRIANGLES)
    {
        pgmStr += Utility::StrPrintf(
            "main:\n"
            " # Do barycentric interp for position and normal.\n"
            " MUL p, tc.x, pos[0];\n"
            " MAD p, tc.y, pos[1], p;\n"
            " MAD p, tc.z, pos[2], p;\n"
            " MUL n.xyz, tc.x, nrm[0];\n"
            " MAD n.xyz, tc.y, nrm[1], n;\n"
            " MAD n.xyz, tc.z, nrm[2], n;\n"
            " MOV n.w, 0;\n"
            " # Perturb position along normal with distance from v[0].\n"
            " MUL r.x, tc.x, pgmElw[%d].z; # Bulge factor\n"
            " MAD result.position, r.x, n, p;\n",
            TessBulge);
    }
    else  // QUADS and ISOLINES
    {
        pgmStr += Utility::StrPrintf(
            "TEMP hi; # interp along bottom edge\n"
            "TEMP lo; # interp along top edge\n"
            "main:\n"
            " # Do bilinear interpolation for position and normal.\n"
            " LRP lo, tc.x, pos[0], pos[1];\n"
            " LRP hi, tc.x, pos[2], pos[3];\n"
            " LRP p,  tc.y, lo, hi;\n"
            " LRP lo.xyz, tc.x, nrm[0], nrm[1];\n"
            " LRP hi.xyz, tc.x, nrm[1], nrm[2];\n"
            " LRP n.xyz,  tc.y, lo, hi;\n"
            " MOV n.w, 0;\n"
            " # Perturb position along normal with distance from v[0].\n"
            " ADD r.x, tc.x, tc.y;\n"
            " MUL r.x, r.x, pgmElw[%d].z; # Bulge factor\n"
            " MAD result.position, r.x, n, p;\n",
            TessBulge);
    }

    if (pProgram->PgmRqmt.Outputs.count(ppNrml))
    {
        pgmStr += " MOV result.attrib[6].xyz, n;\n";
    }
    if (pProgram->PgmRqmt.Outputs.count(ppPtSize))
    {
        if (tessMode == GL_TRIANGLES)
        {
            // tc.x,y,z are each 0..1, pgmElw[9].y is 4, so point size is 0..12.
            pgmStr += Utility::StrPrintf(
                " DP3 result.pointsize.x, tc, pgmElw[%d].yyyy;\n",
                GpuPgmGen::Constants1Reg);
        }
        else
        {
            // tc.x and y are 0..1, pgmElw[9].z is 5, so point size is 0..10.
            pgmStr += Utility::StrPrintf(
                " DP2 result.pointsize.x, tc, pgmElw[%d].zzzz;\n",
                GpuPgmGen::Constants1Reg);
        }
    }

    for (PgmRequirements::ConstPropMapItr iter = pProgram->PgmRqmt.Outputs.begin();
         iter != pProgram->PgmRqmt.Outputs.end();
             iter++)
    {
        const ProgProperty prop = iter->first;
        const int mask = iter->second;

        for (int i = 0; i < s_NumTePassThruStrings; i++)
        {
            if (s_TePassThruStrings[i].prop == prop)
            {
                if (tessMode == GL_TRIANGLES)
                {
                    string s = Utility::StrPrintf(
                        " MUL r, tc.x, %s;\n"
                        " MAD r, tc.y, %s, r;\n"
                        " MAD result.%s, tc.z, %s, r;\n",
                        s_TePassThruStrings[i].sIn,
                        s_TePassThruStrings[i].sIn,
                        s_TePassThruStrings[i].sOut,
                        s_TePassThruStrings[i].sIn);
                    pgmStr += Utility::StrPrintf( s.c_str(), 0,1,2);
                }
                else
                {
                    string s = Utility::StrPrintf(
                        " LRP lo, tc.x, %s, %s;\n"
                        " LRP hi, tc.x, %s, %s;\n"
                        " LRP result.%s, tc.y, lo, hi;\n",
                        s_TePassThruStrings[i].sIn,
                        s_TePassThruStrings[i].sIn,
                        s_TePassThruStrings[i].sIn,
                        s_TePassThruStrings[i].sIn,
                        s_TePassThruStrings[i].sOut);
                    pgmStr += Utility::StrPrintf( s.c_str(), 0,1,2,3);
                }
                pProgram->PgmRqmt.Inputs[prop] = mask;
                break;
            }
        }
    }

    // Write layer/vport info
    if (m_ThisProgWritesLayerVport &&
        m_pGLRandom->HasExt(GLRandomTest::ExtLW_viewport_array2) &&
        m_pGLRandom->GetNumLayersInFBO() > 0)
    {
        pgmStr += Utility::StrPrintf(" ROUND.U result.layer.x, pgmElw[%d].x;\n",
                                    GpuPgmGen::LayerAndVportIDReg);
        if (m_pGLRandom->m_GpuPrograms.GetIsLwrrVportIndexed())
            pgmStr += Utility::StrPrintf(
                                  " ROUND.U result.viewport.x, pgmElw[%d].y;\n",
                                  GpuPgmGen::LayerAndVportIDReg);
        else
            pgmStr += Utility::StrPrintf(
                           " ROUND.U result.viewportmask[0].x, pgmElw[%d].y;\n",
                           GpuPgmGen::LayerAndVportIDReg);
    }

    pgmStr += "END\n";
    pProgram->Pgm = pgmStr;
    pProgram->Target = PgmGen::pkTargets[pkTessEval];
    pProgram->CalcXfbAttributes();
}
