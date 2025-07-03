/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2011, 2013-2014,2016-2019 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// VxGpuPgmGen
//      Vertex program generator for GLRandom. This class strickly follows the
//      Bakus-Nuar form to specify the grammar.
//
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include "glrandom.h"  // declaration of our namespace
#include "pgmgen.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include <math.h>

using namespace GLRandom;

const static char * RegNames[RND_GPU_PROG_VX_REG_NUM_REGS] =
{//   LW name
      "vertex.position"
    , "vertex.attrib[1]"
    , "vertex.normal"
    , "vertex.color.primary"
    , "vertex.color.secondary"
    , "vertex.fogcoord"
    , "vertex.attrib[6]"
    , "vertex.attrib[7]"
    , "vertex.texcoord[0]"
    , "vertex.texcoord[1]"
    , "vertex.texcoord[2]"
    , "vertex.texcoord[3]"
    , "vertex.texcoord[4]"
    , "vertex.texcoord[5]"
    , "vertex.texcoord[6]"
    , "vertex.texcoord[7]"
    , "vertex.instance"
    , "result.position"
    , "result.color.primary"
    , "result.color.secondary"
    , "result.color.back.primary"
    , "result.color.back.secondary"
    , "result.fogcoord"
    , "result.pointsize"
    , "result.clip[0]"
    , "result.clip[1]"
    , "result.clip[2]"
    , "result.clip[3]"
    , "result.clip[4]"
    , "result.clip[5]"
    , "result.texcoord[0]"
    , "result.texcoord[1]"
    , "result.texcoord[2]"
    , "result.texcoord[3]"
    , "result.texcoord[4]"
    , "result.texcoord[5]"
    , "result.texcoord[6]"
    , "result.texcoord[7]"
    , "result.attrib[6]"        // RND_GPU_PROG_VX_REG_oNRML
    , "result.id"
    , "result.layer"            //RND_GPU_PROG_VX_REG_oLAYR
    , "result.viewport"         //RND_GPU_PROG_VX_REG_oVPORTIDX
    , "result.viewportmask[0]"  //RND_GPU_PROG_VX_REG_oVPORTMASK
    , "tmpR0"
    , "tmpR1"
    , "tmpR2"
    , "tmpR3"
    , "tmpR4"
    , "tmpR5"
    , "tmpR6"
    , "tmpR7"
    , "tmpR8"
    , "tmpR9"
    , "tmpR10"
    , "tmpR11"
    , "tmpR12"
    , "tmpR13"
    , "tmpR14"
    , "tmpR15"
    , "tmpA128"             // RND_GPU_PROG_VX_REG_A128
    , "pgmElw"              // RND_GPU_PROG_VX_REG_Const
    , "program.local"       // RND_GPU_PROG_VX_REG_LocalElw
    , ""                    // RND_GPU_PROG_VX_REG_ConstVect
    , ""                    // RND_GPU_PROG_VX_REG_Literal
    , "cbuf"                // RND_GPU_PROG_VX_REG_PaboElw
    , "tmpArray"            // RND_GPU_PROG_VX_REG_TempArray
    , "Subs"                // RND_GPU_PROG_VX_REG_Subs
    , "tmpA1"               // RND_GPU_PROG_VX_REG_A1
    , "tmpASubs"            // RND_GPU_PROG_VX_REG_ASubs
    , "tmpA3"               // RND_GPU_PROG_VX_REG_A3
    , "garbage"             // RND_GPU_PROG_VX_REG_Garbage
};
// lookup table for registers that can be passed through this shader.
const static GpuPgmGen::PassthruRegs VxPassthruRegs[] =
{
    // Xfb prop             property     input                       output
     {RndGpuPrograms::Pos,  ppPos,        RND_GPU_PROG_VX_REG_vOPOS,  RND_GPU_PROG_VX_REG_oHPOS}
    ,{RndGpuPrograms::Col0, ppPriColor,   RND_GPU_PROG_VX_REG_vCOL0,  RND_GPU_PROG_VX_REG_oCOL0}
    ,{RndGpuPrograms::Col1, ppSecColor,   RND_GPU_PROG_VX_REG_vCOL1,  RND_GPU_PROG_VX_REG_oCOL1}
    ,{RndGpuPrograms::Fog,  ppFogCoord,   RND_GPU_PROG_VX_REG_vFOGC,  RND_GPU_PROG_VX_REG_oFOGC}
    ,{RndGpuPrograms::Tex0, ppTxCd0+0,    RND_GPU_PROG_VX_REG_vTEX0,  RND_GPU_PROG_VX_REG_oTEX0}
    ,{RndGpuPrograms::Tex1, ppTxCd0+1,    RND_GPU_PROG_VX_REG_vTEX1,  RND_GPU_PROG_VX_REG_oTEX1}
    ,{RndGpuPrograms::Tex2, ppTxCd0+2,    RND_GPU_PROG_VX_REG_vTEX2,  RND_GPU_PROG_VX_REG_oTEX2}
    ,{RndGpuPrograms::Tex3, ppTxCd0+3,    RND_GPU_PROG_VX_REG_vTEX3,  RND_GPU_PROG_VX_REG_oTEX3}
    ,{RndGpuPrograms::Tex4, ppTxCd0+4,    RND_GPU_PROG_VX_REG_vTEX4,  RND_GPU_PROG_VX_REG_oTEX4}
    ,{RndGpuPrograms::Tex5, ppTxCd0+5,    RND_GPU_PROG_VX_REG_vTEX5,  RND_GPU_PROG_VX_REG_oTEX5}
    ,{RndGpuPrograms::Tex6, ppTxCd0+6,    RND_GPU_PROG_VX_REG_vTEX6,  RND_GPU_PROG_VX_REG_oTEX6}
    ,{RndGpuPrograms::Tex7, ppTxCd0+7,    RND_GPU_PROG_VX_REG_vTEX7,  RND_GPU_PROG_VX_REG_oTEX7}
    ,{RndGpuPrograms::Nrml, ppNrml,       RND_GPU_PROG_VX_REG_vNRML,  RND_GPU_PROG_VX_REG_oNRML}
    ,{RndGpuPrograms::Usr6, ppGenAttr0+6, RND_GPU_PROG_VX_REG_v6,     RND_GPU_PROG_VX_REG_oNRML}
    ,{0,                    ppIlwalid,    0                        ,  0                       }
};

// lookup table to colwert a register to a ProgProperty
const static int RegToProp[RND_GPU_PROG_VX_REG_NUM_REGS] = {
    ppPos,         //RND_GPU_PROG_VX_REG_vOPOS
    ppGenAttr0+1,  //RND_GPU_PROG_VX_REG_vWGHT
    ppNrml,        //RND_GPU_PROG_VX_REG_vNRML
    ppPriColor,    //RND_GPU_PROG_VX_REG_vCOL0
    ppSecColor,    //RND_GPU_PROG_VX_REG_vCOL1
    ppFogCoord,    //RND_GPU_PROG_VX_REG_vFOGC
    ppGenAttr0+6,  //RND_GPU_PROG_VX_REG_v6
    ppGenAttr0+7,  //RND_GPU_PROG_VX_REG_v7
    ppTxCd0,       //RND_GPU_PROG_VX_REG_vTEX0
    ppTxCd0+1,     //RND_GPU_PROG_VX_REG_vTEX1
    ppTxCd0+2,     //RND_GPU_PROG_VX_REG_vTEX2
    ppTxCd0+3,     //RND_GPU_PROG_VX_REG_vTEX3
    ppTxCd0+4,     //RND_GPU_PROG_VX_REG_vTEX4
    ppTxCd0+5,     //RND_GPU_PROG_VX_REG_vTEX5
    ppTxCd0+6,     //RND_GPU_PROG_VX_REG_vTEX6
    ppTxCd0+7,     //RND_GPU_PROG_VX_REG_vTEX7
    ppIlwalid,     //RND_GPU_PROG_VX_REG_vINST  (not writeable, always readable)
    ppPos,         //RND_GPU_PROG_VX_REG_oHPOS
    ppPriColor,    //RND_GPU_PROG_VX_REG_oCOL0
    ppSecColor,    //RND_GPU_PROG_VX_REG_oCOL1
    ppBkPriColor,  //RND_GPU_PROG_VX_REG_oBFC0
    ppBkSecColor,  //RND_GPU_PROG_VX_REG_oBFC1
    ppFogCoord,    //RND_GPU_PROG_VX_REG_oFOGC
    ppPtSize,      //RND_GPU_PROG_VX_REG_oPSIZ
    ppClip0,       //RND_GPU_PROG_VX_REG_oCLP0
    ppClip0+1,     //RND_GPU_PROG_VX_REG_oCLP1
    ppClip0+2,     //RND_GPU_PROG_VX_REG_oCLP2
    ppClip0+3,     //RND_GPU_PROG_VX_REG_oCLP3
    ppClip0+4,     //RND_GPU_PROG_VX_REG_oCLP4
    ppClip0+5,     //RND_GPU_PROG_VX_REG_oCLP5
    ppTxCd0,       //RND_GPU_PROG_VX_REG_oTEX0
    ppTxCd0+1,     //RND_GPU_PROG_VX_REG_oTEX1
    ppTxCd0+2,     //RND_GPU_PROG_VX_REG_oTEX2
    ppTxCd0+3,     //RND_GPU_PROG_VX_REG_oTEX3
    ppTxCd0+4,     //RND_GPU_PROG_VX_REG_oTEX4
    ppTxCd0+5,     //RND_GPU_PROG_VX_REG_oTEX5
    ppTxCd0+6,     //RND_GPU_PROG_VX_REG_oTEX6
    ppTxCd0+7,     //RND_GPU_PROG_VX_REG_oTEX7
    ppNrml,        //RND_GPU_PROG_VX_REG_oNRML
    ppVtxId,       //RND_GPU_PROG_VX_REG_oID
    ppLayer,       //RND_GPU_PROG_VX_REG_oLAYR
    ppViewport,    //RND_GPU_PROG_VX_REG_oVPORTIDX
    ppViewport,    //RND_GPU_PROG_VX_REG_oVPORTMASK
    ppIlwalid,     //RND_GPU_PROG_VX_REG_R0
    ppIlwalid,     //RND_GPU_PROG_VX_REG_R1
    ppIlwalid,     //RND_GPU_PROG_VX_REG_R2
    ppIlwalid,     //RND_GPU_PROG_VX_REG_R3
    ppIlwalid,     //RND_GPU_PROG_VX_REG_R4
    ppIlwalid,     //RND_GPU_PROG_VX_REG_R5
    ppIlwalid,     //RND_GPU_PROG_VX_REG_R6
    ppIlwalid,     //RND_GPU_PROG_VX_REG_R7
    ppIlwalid,     //RND_GPU_PROG_VX_REG_R8
    ppIlwalid,     //RND_GPU_PROG_VX_REG_R9
    ppIlwalid,     //RND_GPU_PROG_VX_REG_R10
    ppIlwalid,     //RND_GPU_PROG_VX_REG_R11
    ppIlwalid,     //RND_GPU_PROG_VX_REG_R12
    ppIlwalid,     //RND_GPU_PROG_VX_REG_R13
    ppIlwalid,     //RND_GPU_PROG_VX_REG_R14
    ppIlwalid,     //RND_GPU_PROG_VX_REG_R15
    ppIlwalid,     //RND_GPU_PROG_VX_REG_A128
    ppIlwalid,     //RND_GPU_PROG_VX_REG_Const
    ppIlwalid,     //RND_GPU_PROG_VX_REG_LocalElw
    ppIlwalid,     //RND_GPU_PROG_VX_REG_ConstVect
    ppIlwalid,     //RND_GPU_PROG_VX_REG_Literal
    ppIlwalid,     //RND_GPU_PROG_VX_REG_PaboElw
    ppIlwalid,     //RND_GPU_PROG_VX_REG_TempArray
    ppIlwalid,     //RND_GPU_PROG_VX_REG_Subs
    ppIlwalid,     //RND_GPU_PROG_VX_REG_A1
    ppIlwalid,     //RND_GPU_PROG_VX_REG_ASubs
    ppIlwalid,     //RND_GPU_PROG_VX_REG_A3
    ppIlwalid      //RND_GPU_PROG_VX_REG_Garbage
};

// This array stores the IO attributes for each ProgProperty (see glr_comm.h).
// Each property uses 2 bits, we lwrrently have 81 properties so we need 162
// bits.
const static UINT64 s_VxppIOAttribs[] =
{
    (
    (UINT64)ppIORw          << (ppPos*2) |
    (UINT64)ppIORd          << (ppNrml*2) |
    (UINT64)ppIORw          << (ppPriColor*2) |
    (UINT64)ppIORw          << (ppSecColor*2) |
    (UINT64)ppIOWr          << (ppBkPriColor*2) |
    (UINT64)ppIOWr          << (ppBkSecColor*2) |
    (UINT64)ppIORw          << (ppFogCoord*2) |
    (UINT64)ppIOWr          << (ppPtSize*2) |
    (UINT64)ppIORw          << (ppVtxId*2) |
    (UINT64)ppIORd          << (ppInstance*2) |
    (UINT64)ppIONone        << (ppFacing*2) |
    (UINT64)ppIONone        << (ppDepth*2) |
    (UINT64)ppIONone        << (ppPrimId*2) |
    (UINT64)ppIOWr          << (ppLayer*2) |
    (UINT64)ppIOWr          << (ppViewport*2) |
    (UINT64)ppIONone        << (ppPrimitive*2) |
    (UINT64)ppIONone        << (ppFullyCovered*2) |
    (UINT64)ppIONone        << (ppBaryCoord*2) |
    (UINT64)ppIONone        << (ppBaryNoPersp*2) |
    (UINT64)ppIONone        << (ppShadingRate*2) |
    (UINT64)ppIOWr          << (ppClip0*2) |
    (UINT64)ppIOWr          << ((ppClip0+1)*2) |
    (UINT64)ppIOWr          << ((ppClip0+2)*2) |
    (UINT64)ppIOWr          << ((ppClip0+3)*2) |
    (UINT64)ppIOWr          << ((ppClip0+4)*2) |
    (UINT64)ppIOWr          << (ppClip5*2) |
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
VxGpuPgmGen::VxGpuPgmGen
(
    GLRandomTest * pGLRandom
   ,FancyPickerArray *pPicker
)
:   GpuPgmGen(pGLRandom, pPicker, pkVertex)
{
}

//------------------------------------------------------------------------------
VxGpuPgmGen::~VxGpuPgmGen()
{
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::GetNumTxFetchers()
{
    return m_pGLRandom->NumVtxTexFetchers();
}

//------------------------------------------------------------------------------
// Determine the highest supported OpenGL extension and update the program
// header with the appropriate statement to support that extension.
void VxGpuPgmGen::GetHighestSupportedExt()
{
    if (m_pGLRandom->HasExt(extGpu5))
    {
        m_PgmHeader = "!!LWvp5.0\n";
        m_BnfExt = extGpu5;
    }
    else if (m_pGLRandom->HasExt(extGpu4_1))
    {
        m_PgmHeader = "!!LWvp4.1\n";
        m_BnfExt = extGpu4_1;
    }
    else if (m_pGLRandom->HasExt(extGpu4))
    {
        m_PgmHeader = "!!LWvp4.0\n";
        m_BnfExt = extGpu4;
    }
    else if (m_pGLRandom->HasExt(extVp3))
    {
        m_PgmHeader = "!!ARBvp1.0\n";
        m_BnfExt = extVp3;
    }
    else
    {
        m_PgmHeader = "!!ARBvp1.0\n";
        m_BnfExt = extVp2;
    }
}

//------------------------------------------------------------------------------
void VxGpuPgmGen::OptionSequence()
{
    if (m_BnfExt == GLRandomTest::ExtLW_vertex_program3)
    {
        PgmOptionAppend("OPTION LW_vertex_program3;\n");
    }
    else if (m_BnfExt == GLRandomTest::ExtLW_vertex_program2)
    {
        PgmOptionAppend("OPTION LW_vertex_program2;\n");
    }
    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_viewport_array2) &&
        m_pGLRandom->GetNumLayersInFBO() > 0 &&
        m_ThisProgWritesLayerVport)
    {
        PgmOptionAppend("OPTION LW_viewport_array2;\n");
    }
    GpuPgmGen::OptionSequence();
}

//------------------------------------------------------------------------------
// Initialize the programming environment & program generation state variables.
// Note: Setup the program environment, register attributes, program target,
//       common data types, and interestingness scoring.
//
void VxGpuPgmGen::InitPgmElw
(
    PgmRequirements *pRqmts
)
{
    // determine proper pgm header and BNF extension.
    GetHighestSupportedExt();
    glGetProgramivARB(GL_VERTEX_PROGRAM_LW,
                     GL_MAX_PROGRAM_ELW_PARAMETERS_ARB,
                     &m_ElwSize);
    bool bFullPgmIndexing = true;
    if (m_BnfExt < extGpu4)
    {
        // we need to reserve some room for const scalars we can't avoid.
        m_ElwSize -= 4;

        // Vertex shaders utilize some of the constant buffer for true constant
        // data. ie. MOV R12,0; But we don't lwrrently count the number of const
        // vectors or scalars we use. So randomly decide to allow either const
        // vector/scalar usage or full pgmElw[] indexing.
        if (m_pFpCtx->Rand.GetRandom(0,1) == 1)
        {
            // allow space for 64 const vectors or 256 const scalars
            m_ElwSize -= 60;
            bFullPgmIndexing = false;
        }
    }

    InitGenState(
            RND_GPU_PROG_VX_OPCODE_NUM_OPCODES,// total number of opcodes
            NULL,                              // additional opcodes to append
            pRqmts,                            // optional program requirements
            VxPassthruRegs,                    // pointer to passthru lookup table
            (bFullPgmIndexing == false));

    VxAttribute col0Alias = att_COL0;
    GLuint al = (*m_pPickers)[RND_GPU_PROG_VX_COL0_ALIAS].Pick();
    switch (al)
    {
        default:
            col0Alias = att_COL0;
            break;

        case att_1:
        case att_NRML:
        case att_FOGC:
        case att_6   :
        case att_7   :
            col0Alias = (VxAttribute)al;
            break;

        case att_TEX2:
        case att_TEX3:
        case att_TEX4:
        case att_TEX5:
        case att_TEX6:
        case att_TEX7:
            if ((al - att_TEX0) >= (GLuint)m_pGLRandom->NumTxCoords())
                col0Alias = (VxAttribute)al;
            else
                col0Alias = att_COL0;
            break;
    }

    SetPgmCol0Alias(col0Alias);
}

//------------------------------------------------------------------------------
void VxGpuPgmGen::InitMailwars
(
    Block * pMainBlock
)
{

    // Initialize our registers array:
    for (int reg = 0; reg < RND_GPU_PROG_VX_REG_NUM_REGS; reg++)
    {
        // Sometimes we pass in primary color in the "wrong" vertex attribute,
        // just to make sure all inputs get tested.
        int aliasedReg = reg;
        const int col0Alias = GetPgmCol0Alias();

        if (col0Alias != att_COL0)
        {
            if (col0Alias == reg)                       // configure the aliased register with COL0 settings.
                aliasedReg = RND_GPU_PROG_VX_REG_vCOL0;
            if (reg == RND_GPU_PROG_VX_REG_vCOL0)       // configure the COL0 register with v6 settings.
                aliasedReg = RND_GPU_PROG_VX_REG_v6;
        }

        RegState v;
        v.Id            = aliasedReg;

        // Note: ReadableMask, WriteableMask, WrittenMask and Scores
        // are all 0 unless overridden below!
        //
        // InitReadOnlyReg also sets ReadableMask and WrittenMask.

        switch (aliasedReg)
        {
            case RND_GPU_PROG_VX_REG_vOPOS:
            case RND_GPU_PROG_VX_REG_vCOL0:
                v.Attrs |= regIn;
                v.InitReadOnlyReg(50, compXYZW);
                break;

            case RND_GPU_PROG_VX_REG_vFOGC:
                v.Attrs |= regIn;
                v.InitReadOnlyReg(50, compX);
                break;

            case RND_GPU_PROG_VX_REG_vNRML:
            case RND_GPU_PROG_VX_REG_vCOL1:
                v.Attrs |= regIn;
                v.InitReadOnlyReg(50, compXYZ);
                break;

            case RND_GPU_PROG_VX_REG_vWGHT:
            case RND_GPU_PROG_VX_REG_v6   :
            case RND_GPU_PROG_VX_REG_v7   :
                // Since Written and WriteableMasks are 0, won't be used.
                v.Attrs |= regIn;
                break;

            case RND_GPU_PROG_VX_REG_vTEX0:
            case RND_GPU_PROG_VX_REG_vTEX1:
            case RND_GPU_PROG_VX_REG_vTEX2:
            case RND_GPU_PROG_VX_REG_vTEX3:
            case RND_GPU_PROG_VX_REG_vTEX4:
            case RND_GPU_PROG_VX_REG_vTEX5:
            case RND_GPU_PROG_VX_REG_vTEX6:
            case RND_GPU_PROG_VX_REG_vTEX7:
                if (aliasedReg - RND_GPU_PROG_VX_REG_vTEX0 < m_pGLRandom->NumTxCoords())
                {
                    v.Attrs |= regIn;
                    v.InitReadOnlyReg(40, compXYZW);
                    v.Score[1]    = 30;
                    v.Score[2]    = 20;
                    v.Score[3]    = 10;
                }
                break;

            case RND_GPU_PROG_VX_REG_vINST:
                if (m_pGLRandom->HasExt(GLRandomTest::ExtARB_draw_instanced))
                {
                    v.InitReadOnlyReg(25, compX);
                }
                break;

            case RND_GPU_PROG_VX_REG_oHPOS:
            case RND_GPU_PROG_VX_REG_oCOL0:
            case RND_GPU_PROG_VX_REG_oCOL1:
            case RND_GPU_PROG_VX_REG_oBFC0:
            case RND_GPU_PROG_VX_REG_oBFC1:
                v.Attrs        |= (regOut | regRqd);
                v.WriteableMask = compXYZW;
                break;

            case RND_GPU_PROG_VX_REG_oFOGC:
            case RND_GPU_PROG_VX_REG_oPSIZ:
                v.Attrs        |= (regOut | regRqd);
                v.WriteableMask = compX;
                break;

            case RND_GPU_PROG_VX_REG_oTEX0:
            case RND_GPU_PROG_VX_REG_oTEX1:
            case RND_GPU_PROG_VX_REG_oTEX2:
            case RND_GPU_PROG_VX_REG_oTEX3:
            case RND_GPU_PROG_VX_REG_oTEX4:
            case RND_GPU_PROG_VX_REG_oTEX5:
            case RND_GPU_PROG_VX_REG_oTEX6:
            case RND_GPU_PROG_VX_REG_oTEX7:
                if (aliasedReg - RND_GPU_PROG_VX_REG_oTEX0 < m_pGLRandom->NumTxCoords())
                {
                    v.Attrs        |= (regOut | regRqd);
                    v.WriteableMask = compXYZW;
                }
                break;

            case RND_GPU_PROG_VX_REG_oNRML:
                if (m_BnfExt >= extGpu4)
                {
                    v.Attrs        |= (regOut);
                    v.WriteableMask = compXYZ;
                }
                break;

            case RND_GPU_PROG_VX_REG_oID:
            case RND_GPU_PROG_VX_REG_oCLP0:
            case RND_GPU_PROG_VX_REG_oCLP1:
            case RND_GPU_PROG_VX_REG_oCLP2:
            case RND_GPU_PROG_VX_REG_oCLP3:
            case RND_GPU_PROG_VX_REG_oCLP4:
            case RND_GPU_PROG_VX_REG_oCLP5:
            case RND_GPU_PROG_VX_REG_oLAYR:
            case RND_GPU_PROG_VX_REG_oVPORTIDX:
            case RND_GPU_PROG_VX_REG_oVPORTMASK:
                v.Attrs        |= regOut;
                v.WriteableMask = compX;
                break;

            case RND_GPU_PROG_VX_REG_R0   :
            case RND_GPU_PROG_VX_REG_R1   :
            case RND_GPU_PROG_VX_REG_R2   :
            case RND_GPU_PROG_VX_REG_R3   :
            case RND_GPU_PROG_VX_REG_R4   :
            case RND_GPU_PROG_VX_REG_R5   :
            case RND_GPU_PROG_VX_REG_R6   :
            case RND_GPU_PROG_VX_REG_R7   :
            case RND_GPU_PROG_VX_REG_R8   :
            case RND_GPU_PROG_VX_REG_R9   :
            case RND_GPU_PROG_VX_REG_R10  :
            case RND_GPU_PROG_VX_REG_R11  :
            case RND_GPU_PROG_VX_REG_R12  :
            case RND_GPU_PROG_VX_REG_R13  :
            case RND_GPU_PROG_VX_REG_R14  :
            case RND_GPU_PROG_VX_REG_R15  :
                v.Attrs        |= regTemp;
                v.WriteableMask = compXYZW;
                v.ReadableMask  = compXYZW;
                // Leave WrittenMask and Scores at 0 initially.
                break;

            // Special register to store garbage results that WILL NOT be read by subsequent
            // instructions. Created to support atomic instructions and sanitization code.
            // This register will not affect the scoring code or the random sequence.
            case RND_GPU_PROG_VX_REG_Garbage:
                v.Attrs |= regGarbage;
                v.WriteableMask = compXYZW;
                if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_shader_atomic_float64) ||
                    m_pGLRandom->HasExt(GLRandomTest::ExtLW_shader_atomic_int64))
                {
                    v.Bits = 64;
                }
                else
                                {
                    v.Bits = 32;
                }
                break;

            case RND_GPU_PROG_VX_REG_A128:
            case RND_GPU_PROG_VX_REG_A3:
            case RND_GPU_PROG_VX_REG_A1:
            case RND_GPU_PROG_VX_REG_ASubs:
                v.InitIndexReg();
                break;

            case RND_GPU_PROG_VX_REG_Const:
                v.ArrayLen = m_ElwSize;
                v.InitReadOnlyReg(25, compXYZW);
                break;

            case RND_GPU_PROG_VX_REG_ConstVect: // 4-component literal
                v.Attrs |= regLiteral;
                if(m_GS.AllowLiterals)
                {
                    v.InitReadOnlyReg(10, compXYZW);
                }
                else
                {
                    v.InitReadOnlyReg(0, compXYZW);
                }
                break;

            case RND_GPU_PROG_VX_REG_LocalElw: // unused!
            case RND_GPU_PROG_VX_REG_PaboElw:
                v.ArrayLen = m_ElwSize;
                v.InitReadOnlyReg(0, compXYZW);
                break;

            case RND_GPU_PROG_VX_REG_Literal:
                v.Attrs |= regLiteral;
                v.InitReadOnlyReg(0, compXYZW);
                break;

            case RND_GPU_PROG_VX_REG_TempArray:
                v.Attrs        |= regTemp;
                v.ArrayLen = TempArrayLen;
                if (m_pGLRandom->HasExt(extGpu5))
                {
                    v.WriteableMask = compXYZW;
                    v.ReadableMask  = compXYZW;
                    // Because of the special handling in ArrayVarInitSequence,
                    // we can assume this reg is pre-written.
                    v.WrittenMask = compXYZW;
                    v.Score[0] = 10;
                    v.Score[1] = 10;
                    v.Score[2] = 10;
                    v.Score[3] = 10;
                }
                break;

            case RND_GPU_PROG_VX_REG_Subs     :
                v.ArrayLen = MaxSubs;
                v.InitReadOnlyReg(0, compXYZW);
                break;

            default:
                MASSERT(!"Unknown vertex program register");
                break;
        }
        pMainBlock->AddVar(v);
    }

    MASSERT(pMainBlock->NumVars() == RND_GPU_PROG_VX_REG_NUM_REGS);
}

//------------------------------------------------------------------------------
// Return the ProgProperty's IO attributes.
int VxGpuPgmGen::GetPropIOAttr(ProgProperty prop) const
{
    if (prop >= ppPos && prop <= ppTxCd7)
    {
        // 32 entries per UINT64
        const int index = prop / 32;
        const int shift = prop % 32;
        return ((s_VxppIOAttribs[index] >> shift*2) & 0x3);
    }
    return ppIONone;
}

//------------------------------------------------------------------------------
// Grammar specific routines
//
int VxGpuPgmGen::PickOp() const
{
    return (*m_pPickers)[RND_GPU_PROG_VX_OPCODE].Pick();
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::PickOpModifier() const
{
    int modifier = smNone;

    switch ((*m_pPickers)[RND_GPU_PROG_VX_WHICH_CC].Pick())
    {
        case RND_GPU_PROG_CC1:
            if (m_BnfExt >= extGpu4)
            {
                modifier |= GpuPgmGen::smCC1;
                break;
            }// fallthrough to CC0
        case RND_GPU_PROG_CC0:
            modifier |= GpuPgmGen::smCC0;
            break;
        case RND_GPU_PROG_CCnone:
        default:
            break;
    }

    switch ((*m_pPickers)[RND_GPU_PROG_VX_SATURATE].Pick())
    {
        case RND_GPU_PROG_OP_SAT_unsigned:  // clamp value to [0,1]
            modifier |= GpuPgmGen::smSS0;
            break;
        case RND_GPU_PROG_OP_SAT_signed:    // clamp value to [-1,1]
            if (m_BnfExt >= extGpu4)
            {
                modifier |= GpuPgmGen::smSS1;
            }
            break;
        case RND_GPU_PROG_OP_SAT_none:
        default:
            break;
    }
    return modifier;
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::PickMultisample() const    //!< Use explicit_multisample extension?
{
    int value = (*m_pPickers)[RND_GPU_PROG_VX_MULTISAMPLE].Pick();
    if (!m_pGLRandom->HasExt(GLRandomTest::ExtLW_explicit_multisample))
        return 0;
    else
        return value;
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::GetIndexReg(IndexRegType t) const
{
    switch (t)
    {
        default:        MASSERT(!"Unexpected IndexRegType");
        // fall through

        case tmpA128:   return RND_GPU_PROG_VX_REG_A128;
        case tmpA3:     return RND_GPU_PROG_VX_REG_A3;
        case tmpA1:     return RND_GPU_PROG_VX_REG_A1;
        case tmpASubs:  return RND_GPU_PROG_VX_REG_ASubs;
    }
}

//------------------------------------------------------------------------------
bool VxGpuPgmGen::IsIndexReg(int regId) const
{
    switch (regId)
    {
        case RND_GPU_PROG_VX_REG_A128:
        case RND_GPU_PROG_VX_REG_A3:
        case RND_GPU_PROG_VX_REG_A1:
        case RND_GPU_PROG_VX_REG_ASubs:
            return true;

        default:
            return false;
    }
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::GetMinOps() const
{
    // MainSequenceStart uses 4 ops to write one required output.
    return m_GS.MainBlock.NumRqdUnwritten() + 3;
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::GetMaxOps() const
{
    GLint maxOps;
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB,
                      GL_MAX_PROGRAM_INSTRUCTIONS_ARB,
                      &maxOps);
    if (mglGetRC() != OK)
        return 0;
    // In practice the usuable number of ops is much lower than the system limit. i.e.
    // if we exceed 300 ops per program the test time is unacceptable.
    return maxOps;
}

//------------------------------------------------------------------------------
// return a texture coordinate for this register or <0 on error.
int VxGpuPgmGen::GetTxCd(int reg) const
{
    if ( reg >= RND_GPU_PROG_VX_REG_vTEX0 && reg <= RND_GPU_PROG_VX_REG_vTEX7)
        return(reg-RND_GPU_PROG_VX_REG_vTEX0);
    else
        return -1;
}

//------------------------------------------------------------------------------
string VxGpuPgmGen::GetRegName(int reg) const
{
    if ( reg >= RND_GPU_PROG_VX_REG_vOPOS && reg < int(NUMELEMS(RegNames)))
        return RegNames[reg];
    else if ( reg == RND_GPU_PROG_REG_none)
        return "";
    else
        return "Unknown";
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::GetConstReg() const
{
    return RND_GPU_PROG_VX_REG_Const;
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::GetPaboReg() const
{
    return RND_GPU_PROG_VX_REG_PaboElw;
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::GetSubsReg() const
{
    return RND_GPU_PROG_VX_REG_Subs;
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::GetLocalElwReg() const
{
    return RND_GPU_PROG_VX_REG_LocalElw;
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::GetConstVectReg() const
{
    return RND_GPU_PROG_VX_REG_ConstVect;
}
//------------------------------------------------------------------------------
int VxGpuPgmGen::GetFirstTxCdReg() const
{
    return RND_GPU_PROG_VX_REG_vTEX0;
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::GetLastTxCdReg() const
{
    return RND_GPU_PROG_VX_REG_vTEX7;
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::GetFirstTempReg() const
{
    return RND_GPU_PROG_VX_REG_R0;
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::GetGarbageReg() const
{
    return RND_GPU_PROG_VX_REG_Garbage;
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::GetLiteralReg() const
{
    return RND_GPU_PROG_VX_REG_Literal;
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::GetNumTempRegs() const
{
    return RND_GPU_PROG_VX_REG_R15 - RND_GPU_PROG_VX_REG_R0 + 1;
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::PickPgmTemplate() const
{
    // Pick a flow-control and randomization theme.
    UINT32 t = (*m_pPickers)[RND_GPU_PROG_VX_TEMPLATE].Pick();
    if (t >= RND_GPU_PROG_TEMPLATE_NUM_TEMPLATES)
    {
        t = RND_GPU_PROG_TEMPLATE_Simple;
    }
    return(t);

}

//------------------------------------------------------------------------------
int VxGpuPgmGen::PickNumOps() const
{
    return(*m_pPickers)[RND_GPU_PROG_VX_NUM_OPS].Pick();
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::PickCCtoRead() const
{
    int cc = (*m_pPickers)[RND_GPU_PROG_VX_WHICH_CC].Pick();
    if ((cc != RND_GPU_PROG_CCnone) && !m_pGLRandom->HasExt(extGpu4))
    {
        cc = RND_GPU_PROG_CC0;
    }
    if ( m_GS.Blocks.top()->CCValidMask() & (0xf << (cc*4)))
        return cc;
    else
        return RND_GPU_PROG_CCnone;
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::PickCCTest(PgmInstruction *pOp) const
{
    int ccTest = (*m_pPickers)[RND_GPU_PROG_VX_OUT_TEST].Pick();
    if (m_BnfExt < extGpu4 && ccTest != RND_GPU_PROG_OUT_TEST_none)
    {
        // only EQ - FL supported
        ccTest = ccTest % RND_GPU_PROG_OUT_TEST_nan;
    }
    return ccTest;

}

//------------------------------------------------------------------------------
int VxGpuPgmGen::PickSwizzleSuffix() const
{
    return (*m_pPickers)[RND_GPU_PROG_VX_SWIZZLE_SUFFIX].Pick();
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::PickSwizzleOrder() const
{
    return(*m_pPickers)[RND_GPU_PROG_VX_SWIZZLE_ORDER].Pick();
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::PickWriteMask() const
{
    return(*m_pPickers)[RND_GPU_PROG_VX_OUT_MASK].Pick();
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::PickNeg() const
{
    return (*m_pPickers)[RND_GPU_PROG_VX_IN_NEG].Pick();
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::PickAbs()const
{
    return (*m_pPickers)[RND_GPU_PROG_VX_IN_ABS].Pick();
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::PickRelAddr() const
{
    return (*m_pPickers)[RND_GPU_PROG_VX_RELADDR].Pick();
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::PickResultReg() const
{
    return (*m_pPickers)[RND_GPU_PROG_VX_OUT_REG].Pick();
}

//------------------------------------------------------------------------------
struct GpuAttAliases
{
    const char *          Name;
    GLRandom::VxAttribute Att;
};
void VxGpuPgmGen::GenFixedAttrUse(Program * pProg, int whichAttr)
{
    static const GpuAttAliases gpuAttAliases[] =
        {  { "vpAtt1",    att_1    },
           { "vpAttNRML", att_NRML },
           { "vpAttCOL0", att_COL0 },
           { "vpAttFOGC", att_FOGC },
           { "vpAtt6",    att_6    },
           { "vpAtt7",    att_7    },
           { "vpAttTEX4", att_TEX4 },
           { "vpAttTEX5", att_TEX5 },
           { "vpAttTEX6", att_TEX6 },
           { "vpAttTEX7", att_TEX7 }
        };
    MASSERT(whichAttr < (int) NUMELEMS(gpuAttAliases));

    pProg->Name      = gpuAttAliases[whichAttr].Name;
    pProg->Col0Alias = gpuAttAliases[whichAttr].Att;
    const bool useVp3 = (m_BnfExt >= GLRandomTest::ExtLW_gpu_program4_0) ?
            false : true;
    const int elwSize = (useVp3) ? 192 : 256;
    string extString = (useVp3) ?
            "!!ARBvp1.0\nOPTION LW_vertex_program3;\n" : "!!LWvp4.0\n";
    const char * killOlwp3 = useVp3 ? "#" : " ";

    pProg->Pgm       = Utility::StrPrintf(
        "%s"
        "#Fixed Pgm TemplateAttrUse\n"
        " PARAM pgmElw[%d] = { program.elw[0..%d] };\n"
        " TEMP pos;\n"
        " TEMP instOffs;\n"
        " DP4 pos.x, pgmElw[%d], vertex.position;\n"
        " DP4 pos.y, pgmElw[%d], vertex.position;\n"
        " DP4 pos.z, pgmElw[%d], vertex.position;\n"
        " DP4 pos.w, pgmElw[%d], vertex.position;\n"
        "%sI2F instOffs.x, vertex.instance.x;\n"
        "%sMUL instOffs.x, instOffs.x, -0.05;\n"
        "%sMAD pos.xyz, instOffs.x, pos.x, pos;\n"
        " MOV result.position, pos;\n"
        " MOV result.color, vertex.attrib[%d];\n"
        "%sMOV result.attrib[6].xyz, vertex.normal;\n"
        " MOV result.color.secondary, vertex.color.secondary;\n"
        " MUL result.color.back, vertex.attrib[%d], pgmElw[%d].x;\n"
        " MUL result.color.back.secondary, vertex.color.secondary, pgmElw[%d].x;\n"
        " MOV result.pointsize.x, pgmElw[%d].z;\n"
        " MOV result.texcoord[0], vertex.texcoord[0];\n"
        " MOV result.texcoord[1], vertex.texcoord[1];\n"
        " MOV result.texcoord[2], vertex.texcoord[2];\n"
        " MOV result.texcoord[3], vertex.texcoord[3];\n"
        " MOV result.texcoord[4], vertex.texcoord[4];\n"
        " MOV result.texcoord[5], vertex.texcoord[5];\n"
        " MOV result.texcoord[6], vertex.texcoord[6];\n"
        " MOV result.texcoord[7], vertex.texcoord[7];\n"
        " MOV result.fogcoord.x, pgmElw[%d].w;\n"
        "END\n",
        extString.c_str(),
        elwSize,elwSize-1,
        ModelViewProjReg+0,
        ModelViewProjReg+1,
        ModelViewProjReg+2,
        ModelViewProjReg+3,
        killOlwp3, killOlwp3, killOlwp3,
        gpuAttAliases[whichAttr].Att,
        killOlwp3,
        gpuAttAliases[whichAttr].Att,
        Constants1Reg,Constants1Reg,
        LightIndexReg,
        Constants2Reg);

    pProg->XfbStdAttrib = RndGpuPrograms::AllStd;
    if (gpuAttAliases[whichAttr].Att == att_NRML)
    {
        // surface-normal is xyz, but in this case we are passing
        // primary color (rgba) and need all 4 components.
        pProg->PgmRqmt.Inputs[ppNrml] = omXYZW;
    }
    pProg->PgmRqmt.Inputs[ppSecColor]  = omXYZW;
    pProg->PgmRqmt.Inputs[(ProgProperty)(ppGenAttr0 + pProg->Col0Alias)] = omXYZW;
    pProg->PgmRqmt.Inputs.erase(ppPriColor);
    pProg->PgmRqmt.Inputs.erase(ppFogCoord);
}

//------------------------------------------------------------------------------
void VxGpuPgmGen::GenFixedIdxLight(Program * pProg, int firstReg)
{
    // This program does 2-light lighting, testing indexed access,
    // all const registers, and all temp registers.

    // Note that renaming the temp regs (R0,R1,R2 vs. R3,R4,R5) is a noop
    // because the compiler assigns HW registers as-needed anyway.
    // So we no longer bother, all GenFixedIdxLight progs are the same.

    //Note: To get an equivilent VP1.0 shader do:
    // 1.Replace ROUND with ARL
    // 2.Don't copy the nrml into the generic result.attrib[6] binding.
    const bool useVp3 = (m_BnfExt >= GLRandomTest::ExtLW_gpu_program4_0) ?
            false : true;
    const int elwSize = useVp3 ? 192:256;
    string extString = useVp3 ?
        "!!ARBvp1.0\nOPTION LW_vertex_program3;\n" :
        "!!LWvp4.0\n";
    string loadInstr = useVp3 ? "ARL " : "ROUND.U ";
    const char * killOlwp3 = useVp3 ? "#" : " ";

    pProg->Name = Utility::StrPrintf("vpUseRegs%d_%d", firstReg, firstReg+2);
    pProg->Pgm = Utility::StrPrintf(
        "%s"
        "#Fixed Pgm TemplateIndexedLights\n"
        " PARAM pgmElw[%d] = { program.elw[0..%d] };\n"
        " TEMP R0;\n"
        " TEMP R1;\n"
        " TEMP R2;\n"
        " TEMP pos;\n"
        " TEMP instOffs;\n"
        "%s" // "ADDRESS A0;\n" or "TEMP A0;\n"
        " DP4 pos.x, pgmElw[%d], vertex.position;\n"
        " DP4 pos.y, pgmElw[%d], vertex.position;\n"
        " DP4 pos.z, pgmElw[%d], vertex.position;\n"
        " DP4 pos.w, pgmElw[%d], vertex.position;\n"
        "%sI2F instOffs.x, vertex.instance.x;\n"
        "%sMUL instOffs.x, instOffs.x, 0.07;\n"
        "%sMAD pos.xyz, instOffs.x, pos.y, pos;\n"
        " MOV result.position, pos;\n"
        " DP3 R2.x, pgmElw[%d], vertex.normal;\n"
        " DP3 R2.y, pgmElw[%d], vertex.normal;\n"
        " DP3 R2.z, pgmElw[%d], vertex.normal;\n"
        " DP3 R2.w, R2, R2;\n"
        " RSQ R2.w, R2.w;\n"
        " MUL R2, R2, R2.w; # normalize eye-space normal\n"
        //" ROUND.U A0.x, pgmElw[%d].x; # load index register (light 0)\n"
        " %s A0.x, pgmElw[%d].x; # load index register (light 0)\n"
        " DP3 R0.x, pgmElw[A0.x + 0], R2; # // R0.x = Ldir DOT n'\n"
        " DP3 R0.y, pgmElw[A0.x + 1], R2; # R0.y = hHat DOT n'\n"
        " MOV R0.w, pgmElw[A0.x + 3].x;   # R0.w = spelwlar power\n"
        " LIT R0, R0; # R0 = lighting values for first light\n"
        //" ROUND.U A0.x, pgmElw[%d].y;     # load index register (which light)\n"
        " %s A0.x, pgmElw[%d].y;     # load index register (which light)\n"
        " DP3 R1.x, pgmElw[A0.x + 0], R2; # R1.x = Ldir DOT n'\n"
        " DP3 R1.y, pgmElw[A0.x + 1], R2; # R1.y = hHat DOT n'\n"
        " MOV R1.w, pgmElw[A0.x + 3].x;   # R1.w = spelwlar power\n"
        " LIT R1, R1;                     # R1 = lighting values for second light\n"
        " MUL R2, R1.y, pgmElw[A0.x + 2]; # R2 =  light1 diffuse * light1 color\n"
        //" ROUND.U A0.x, pgmElw[%d].x;     # load index register (light 0)\n"
        " %s A0.x, pgmElw[%d].x;     # load index register (light 0)\n"
        " MAD R2, R0.y, pgmElw[A0.x + 2], R2; #  R2 += light0 diffuse * light0 color\n"
        " MAD result.color, R2, vertex.color, pgmElw[%d]; # col0 = R2 * material color + global ambient\n"
        " MUL R2, R0.z, pgmElw[A0.x + 2]; # R2 = light0 spelwlar * light0 color\n"
        //" ROUND.U A0.x, pgmElw[%d].y;     # load index register (light 1)\n"
        " %s A0.x, pgmElw[%d].y;     # load index register (light 1)\n"
        "%sMOV result.attrib[6].xyz, vertex.normal;\n"
        " MAD result.color.secondary, R1.z, pgmElw[A0.x + 2], R2; # col1 = light1 spelwlar * light1 color + R2\n"
        " MOV result.color.back, vertex.color; # color(backface) passthru\n"
        " MOV result.color.back.secondary, vertex.color.secondary; # color1(backface) passthru\n"
        " MOV result.fogcoord.x, vertex.fogcoord.x;\n"
        " MOV result.pointsize.x, pgmElw[%d].z;\n"
        " MOV result.texcoord[0], vertex.texcoord[0];\n"
        " MOV result.texcoord[1], vertex.texcoord[1];\n"
        " MOV result.texcoord[2], vertex.texcoord[2];\n"
        " MOV result.texcoord[3], vertex.texcoord[3];\n"
        " MOV result.texcoord[4], vertex.texcoord[4];\n"
        " MOV result.texcoord[5], vertex.texcoord[5];\n"
        " MOV result.texcoord[6], vertex.texcoord[6];\n"
        " MOV result.texcoord[7], vertex.texcoord[7];\n"
        "END\n",
        extString.c_str(),
        elwSize,elwSize-1,
        useVp3 ? " ADDRESS A0;\n" : " TEMP A0;\n",
        ModelViewProjReg+0,
        ModelViewProjReg+1,
        ModelViewProjReg+2,
        ModelViewProjReg+3,
        killOlwp3, killOlwp3, killOlwp3,
        ModelViewIlwReg+0,
        ModelViewIlwReg+1,
        ModelViewIlwReg+2,
        loadInstr.c_str(), LightIndexReg,
        loadInstr.c_str(), LightIndexReg,
        loadInstr.c_str(), LightIndexReg,
        AmbientColorReg,
        loadInstr.c_str(), LightIndexReg,
        killOlwp3,
        LightIndexReg);

    pProg->XfbStdAttrib = RndGpuPrograms::Pos  |
        RndGpuPrograms::Col0 |
        RndGpuPrograms::Col1 |
        RndGpuPrograms::Fog  |
        RndGpuPrograms::PtSz |
        RndGpuPrograms::AllTex;
}

//------------------------------------------------------------------------------
struct GpuUseOpFragments
{
    const char *   Name;
    const char *   PgmFragment;
};
void VxGpuPgmGen::GenFixedOpUse(Program * pProg, int whichOp)
{
    static const GpuUseOpFragments gpuUseOpFragments[] = {
        {
            "vpUseADD",
            " ADD result.texcoord[0], vertex.texcoord[0], peAmbient;\n"
            " ADD result.texcoord[1], vertex.texcoord[1], -peAmbient;\n"
            " ADD result.texcoord[2], vertex.texcoord[2], peAmbient.zyxw;\n"
            " ADD result.texcoord[3], vertex.texcoord[3], -peAmbient.zyxw;\n"
        },
        {
            "vpUseRCP",
            " RCP R4.x, peConsts1.x;\n"
            " RCP R4.y, peConsts1.y;\n"
            " RCP R4.z, peConsts1.z;\n"
            " RCP R4.w, peConsts1.w;\n"
            " MUL result.texcoord[0], vertex.texcoord[0], R4;\n"
            " MUL result.texcoord[1], vertex.texcoord[1], R4;\n"
            " MUL result.texcoord[2], vertex.texcoord[2], R4;\n"
            " MUL result.texcoord[3], vertex.texcoord[3], R4;\n"
        },
        {
            "vpUseDST",
            " DST R4, vertex.color, peAmbient;\n"
            " MUL result.texcoord[0], vertex.texcoord[0], R4.yzwx;\n"
            " MUL result.texcoord[1], vertex.texcoord[1], R4.yzwx;\n"
            " MUL result.texcoord[2], vertex.texcoord[2], R4.yzwx;\n"
            " MUL result.texcoord[3], vertex.texcoord[3], R4.yzwx;\n"
        },
        {  "vpUseMIN",
           " MIN result.texcoord[0], vertex.texcoord[0], peConsts2;\n"
           " MIN result.texcoord[1], vertex.texcoord[1], peConsts2;\n"
           " MIN result.texcoord[2], vertex.texcoord[2], peConsts2.zyxw;\n"
           " MIN result.texcoord[3], vertex.texcoord[3], peConsts2.zyxw;\n"
        },
        {
            "vpUseMAX",
            " MAX result.texcoord[0], vertex.texcoord[0], peConsts2;\n"
            " MAX result.texcoord[1], vertex.texcoord[1], peConsts2;\n"
            " MAX result.texcoord[2], vertex.texcoord[2], peConsts2.zyxw;\n"
            " MAX result.texcoord[3], vertex.texcoord[3], peConsts2.zyxw;\n"
        },
        {
            "vpUseSLT",
            " SLT R4, vertex.texcoord[0], peLightColor;\n"
            " MAD result.texcoord[0], vertex.texcoord[0], R4, vertex.texcoord[0];\n"
            " SLT R4, -vertex.texcoord[1], peLightColor;\n"
            " MAD result.texcoord[1], vertex.texcoord[1], R4, vertex.texcoord[1];\n"
            " SLT R4, vertex.texcoord[2], -peLightColor;\n"
            " MAD result.texcoord[2], vertex.texcoord[2], -R4, -vertex.texcoord[2];\n"
            " SLT R4, -vertex.texcoord[3], -peLightColor;\n"
            " MAD result.texcoord[3], vertex.texcoord[3], -R4, -vertex.texcoord[3];\n"
        },
        {
            "vpUseSGE",
            " SGE R4, vertex.texcoord[0], peLightColor;\n"
            " MAD result.texcoord[0], vertex.texcoord[0], R4, vertex.texcoord[0];\n"
            " SGE R4, -vertex.texcoord[1], peLightColor;\n"
            " MAD result.texcoord[1], vertex.texcoord[1], R4, vertex.texcoord[1];\n"
            " SGE R4, vertex.texcoord[2], -peLightColor;\n"
            " MAD result.texcoord[2], vertex.texcoord[2], -R4, -vertex.texcoord[2];\n"
            " SGE R4, -vertex.texcoord[3], -peLightColor;\n"
            " MAD result.texcoord[3], vertex.texcoord[3], -R4, -vertex.texcoord[3];\n"
        },
        {
            "vpUseEXP",
            " EX2 R4, vertex.texcoord[0].x;\n"
            " MAD result.texcoord[0], peAmbient, R4.zyxw, vertex.texcoord[0];\n"
            " EX2 R4, vertex.texcoord[1].x;\n"
            " MAD result.texcoord[1], peAmbient, R4.zyxw, vertex.texcoord[1];\n"
            " EX2 R4, vertex.texcoord[2].x;\n"
            " MAD result.texcoord[2], peAmbient, R4.zyxw, vertex.texcoord[2];\n"
            " EX2 R4, vertex.texcoord[3].x;\n"
            " MAD result.texcoord[3], peAmbient, R4.zyxw, vertex.texcoord[3];\n"
        },
        {
            "vpUseLOG",
            " DP3 R4, vertex.texcoord[0], peConsts1;\n"
            " LG2 R4, R4.x;\n"
            " MAD result.texcoord[0], vertex.texcoord[0], peConsts2.w, R4.zyxw;\n"
            " DP3 R4, vertex.texcoord[1], peConsts1;\n"
            " LG2 R4, R4.x;\n"
            " MAD result.texcoord[1], vertex.texcoord[1], peConsts2.w, R4.zyxw;\n"
            " DP3 R4, vertex.texcoord[2], peConsts1;\n"
            " LG2 R4, R4.x;\n"
            " MAD result.texcoord[2], vertex.texcoord[2], peConsts2.w, R4.zyxw;\n"
            " DP3 R4, vertex.texcoord[3], peConsts1;\n"
            " LG2 R4, R4.x;\n"
            " MAD result.texcoord[3], vertex.texcoord[3], peConsts2.w, R4.zyxw;\n"
        }
    };
    MASSERT(whichOp < (int) NUMELEMS(gpuUseOpFragments));

    pProg->Name = gpuUseOpFragments[whichOp].Name;
    const char * fragment = gpuUseOpFragments[whichOp].PgmFragment;

    const bool useVp3 = (m_BnfExt >= GLRandomTest::ExtLW_gpu_program4_0) ?
            false : true;
    const int elwSize = (useVp3) ? 192 : 256;
    string extString = (useVp3) ?
            "!!ARBvp1.0\nOPTION LW_vertex_program3;\n" : "!!LWvp4.0\n";
    const char * killOlwp3 = useVp3 ? "#" : " ";

    // Simple one-light lighting, and pass thru for everything but tex coords.
    // Tex coords 0,1,2,3 are munged by the fragment.
    pProg->Pgm = Utility::StrPrintf(
        "%s"
        "#Fixed Pgm TemplateUseOp\n"
        " PARAM pgmElw[%d] = { program.elw[0..%d] };\n"
        " PARAM peAmbient = program.elw[%d];\n"
        " PARAM peConsts1 = program.elw[%d];\n"
        " PARAM peConsts2 = program.elw[%d];\n"
        " PARAM peLightDir = program.elw[%d];\n"
        " PARAM peLightHat = program.elw[%d];\n"
        " PARAM peLightColor = program.elw[%d];\n"
        " PARAM peLightPow = program.elw[%d];\n"
        " TEMP R3;\n"
        " TEMP R4;\n"
        " TEMP pos;\n"
        " TEMP instOffs;\n"
        " DP4 pos.x, pgmElw[%d], vertex.position;\n"
        " DP4 pos.y, pgmElw[%d], vertex.position;\n"
        " DP4 pos.z, pgmElw[%d], vertex.position;\n"
        " DP4 pos.w, pgmElw[%d], vertex.position;\n"
        "%sI2F instOffs.x, vertex.instance.x;\n"
        "%sMUL instOffs.x, instOffs.x, -0.05;\n"
        "%sMAD pos.xyz, instOffs.x, pos.z, pos;\n"
        " MOV result.position, pos;\n"
        " DP3 R3.x, pgmElw[%d], vertex.normal;\n"
        " DP3 R3.y, pgmElw[%d], vertex.normal;\n"
        " DP3 R3.z, pgmElw[%d], vertex.normal;\n"
        " DP3 R3.w, R3, R3;\n"
        " RSQ R3.w, R3.w;\n"
        " MUL R3, R3, R3.w; #  normalize eye-space normal\n"
        " DP3 R4.x, peLightDir, R3; # R4.x = Ldir DOT n'\n"
        " DP3 R4.y, peLightHat, R3; # R4.y = hHat DOT n'\n"
        " MOV R4.w, peLightPow.x;   # R4.w = spelwlar power\n"
        " LIT R4, R4;               # R4 = light power results\n"
        " MUL R3, R4.y, peLightColor; # R3 =  light diffuse * light color\n"
        " MAD result.color, R3, vertex.color, peAmbient; # col0 = R3 * material color + global ambient\n"
        " MUL result.color.secondary, R4.z, peLightColor; # col1 = light spelwlar * light color\n"
        " MOV result.color.back, vertex.color; # color(backface) passthru\n"
        " MOV result.color.back.secondary, vertex.color.secondary; # color1(backface) passthru\n"
        " MOV result.fogcoord.x, vertex.fogcoord.x;\n"
        " MOV result.pointsize.x, pgmElw[%d].z;\n"
        "%sMOV result.attrib[6].xyz, vertex.normal;\n"
        " %s"
        " MOV result.texcoord[4], vertex.texcoord[4];\n"
        " MOV result.texcoord[5], vertex.texcoord[5];\n"
        " MOV result.texcoord[6], vertex.texcoord[6];\n"
        " MOV result.texcoord[7], vertex.texcoord[7];\n"
        "END\n",
        extString.c_str(),
        elwSize,elwSize-1,
        AmbientColorReg,
        Constants1Reg,
        Constants2Reg,
        FirstLightReg + whichOp*4 + 0,
        FirstLightReg + whichOp*4 + 1,
        FirstLightReg + whichOp*4 + 2,
        FirstLightReg + whichOp*4 + 3,
        ModelViewProjReg+0,
        ModelViewProjReg+1,
        ModelViewProjReg+2,
        ModelViewProjReg+3,
        killOlwp3, killOlwp3, killOlwp3,
        ModelViewIlwReg+0,
        ModelViewIlwReg+1,
        ModelViewIlwReg+2,
        LightIndexReg,
        killOlwp3,
        fragment);

    pProg->XfbStdAttrib = RndGpuPrograms::Pos  |
        RndGpuPrograms::Col0 |
        RndGpuPrograms::Col1 |
        RndGpuPrograms::Fog  |
        RndGpuPrograms::PtSz |
        RndGpuPrograms::AllTex |
        RndGpuPrograms::Usr0;

    // mask off the pass-thru bit
    for (ProgProperty i = ppTxCd0; i <= ppTxCd0+3; i++)
        pProg->PgmRqmt.Outputs[i] &= ~omPassThru;
}

//------------------------------------------------------------------------------
struct GpuUseOpFragments11
{
    const char *   Name;
    const char *   PgmFragment;
};
void VxGpuPgmGen::GenFixedOpUse11(Program * pProg, int whichOp)
{
    static const GpuUseOpFragments11 gpuUseOpFragments11[] = {
        {
            "vpUseDPH",
            " MOV R7, -vertex.texcoord[0].zyxw;\n"
            " DPH R7.w, R7, -vertex.texcoord[0].yxzw;\n"
            " MUL result.texcoord[0], R7.w, vertex.texcoord[0];\n"
            " MOV R7, -vertex.texcoord[1].zyxw;\n"
            " DPH R7.w, R7, -vertex.texcoord[1].yxzw;\n"
            " MUL result.texcoord[1], R7.w, vertex.texcoord[1];\n"
            " MOV R7, -vertex.texcoord[2].zyxw;\n"
            " DPH R7.w, R7, -vertex.texcoord[2].yxzw;\n"
            " MUL result.texcoord[2], R7.w, vertex.texcoord[2];\n"
            " MOV R7, -vertex.texcoord[3].zyxw;\n"
            " DPH R7.w, R7, -vertex.texcoord[3].yxzw;\n"
            " MUL result.texcoord[3], R7.w, vertex.texcoord[3];\n"
        },
        {
            "vpUseRCC",
            " RCC R4.x, peAmbient.x;\n"
            " RCC R4.y, peAmbient.y;\n"
            " RCC R4.z, peAmbient.z;\n"
            " RCC R4.w, peAmbient.w;\n"
            " MUL result.texcoord[0], vertex.texcoord[0], R4;\n"
            " MUL result.texcoord[1], vertex.texcoord[1], R4;\n"
            " MUL result.texcoord[2], vertex.texcoord[2], R4;\n"
            " MUL result.texcoord[3], vertex.texcoord[3], R4;\n"
        }
    };
    MASSERT(whichOp < (int) NUMELEMS(gpuUseOpFragments11));

    // GenFixedOpUse exercises the first 9 sets of "light" constants.
    // Here we use the next 2 sets.
    const int startLight = RND_GPU_PROG_VX_INDEX_UseLOG -
        RND_GPU_PROG_VX_INDEX_UseADD + 1 + whichOp;

    pProg->Name = gpuUseOpFragments11[whichOp].Name;
    const char * fragment = gpuUseOpFragments11[whichOp].PgmFragment;

    const bool useVp3 = (m_BnfExt >= GLRandomTest::ExtLW_gpu_program4_0) ?
            false : true;
    const int elwSize = (useVp3) ? 192 : 256;
    string extString = (useVp3) ?
            "!!ARBvp1.0\nOPTION LW_vertex_program3;\n" : "!!LWvp4.0\n";
    const char * killOlwp3 = useVp3 ? "#" : " ";

    // Simple one-light lighting, and pass thru for everything but tex coords.
    // Tex coords 0,1,2,3 are munged by the "fragment".
    pProg->Pgm = Utility::StrPrintf(
        "%s"
        "#Fixed Pgm TemplateUseOp1_1\n"
        " PARAM pgmElw[%d] = { program.elw[0..%d] };\n"
        " PARAM peAmbient = program.elw[%d];\n"
        " PARAM peConsts1 = program.elw[%d];\n"
        " PARAM peConsts2 = program.elw[%d];\n"
        " PARAM peLightDir = program.elw[%d];\n"
        " PARAM peLightHat = program.elw[%d];\n"
        " PARAM peLightColor = program.elw[%d];\n"
        " PARAM peLightPow = program.elw[%d];\n"
        " TEMP R3;\n"
        " TEMP R4;\n"
        " TEMP R7;\n"
        " TEMP R9;\n"
        " TEMP pos;\n"
        " TEMP instOffs;\n"
        " DP4 pos.x, pgmElw[%d], vertex.position;\n"
        " DP4 pos.y, pgmElw[%d], vertex.position;\n"
        " DP4 pos.z, pgmElw[%d], vertex.position;\n"
        " DP4 pos.w, pgmElw[%d], vertex.position;\n"
        "%sI2F instOffs.x, vertex.instance.x;\n"
        "%sMUL instOffs.x, instOffs.x, -0.05;\n"
        "%sMAD pos.xyz, instOffs.x, pos.w, pos;\n"
        " MOV result.position, pos;\n"
        " DP3  R3.x, pgmElw[%d], vertex.normal;\n"
        " DP3  R3.y, pgmElw[%d], vertex.normal;\n"
        " DP3  R3.z, pgmElw[%d], vertex.normal;\n"
        " DP3  R3.w, R3, R3;\n"
        " RSQ  R3.w, R3.w;\n"
        " MUL  R3, R3, R3.w;          # normalize eye-space normal\n"
        " DP3  R4.x, peLightDir, R3;  # R4.x = Ldir DOT n'\n"
        " DP3  R4.y, peLightHat, R3;  # R4.y = hHat DOT n'\n"
        " MOV  R4.w, peLightPow.x;    # R4.w = spelwlar power\n"
        " LIT  R4, R4;                # R4 = light power results\n"
        " MUL  R3, R4.y, peLightColor; # R3 =  light diffuse * light color\n"
        " MAD  result.color, R3, vertex.color, peAmbient; # col0 = R3 * material color + global ambient\n"
        " MUL  result.color.secondary, R4.z, peLightColor; # col1 = light spelwlar * light color\n"
        " MOV  result.color.back, vertex.color; # color(backface) passthru\n"
        " MOV  result.color.back.secondary, vertex.color.secondary; # color1(backface) passthru\n"
        " MOV  result.fogcoord.x, vertex.fogcoord.x;\n"
        "%sMOV  result.attrib[6].xyz, vertex.normal;\n"
        " MOV  R9, vertex.position;\n"
        " ADD  R9.x, R9.x, -R9.y;\n"
        " ADD  R9.y, R9.x, R9.y;\n"
        " RCP  R9.w, R9.y;\n"
        " MUL  R9.x, R9.x, R9.w;\n"
        " LG2  R9.x, R9.x;\n"
        " ABS  result.pointsize.x, R9.x;\n"
        " %s\n"                       // texture coord operations
        " MOV result.texcoord[4], vertex.texcoord[4];\n"
        " MOV result.texcoord[5], vertex.texcoord[5];\n"
        " MOV result.texcoord[6], vertex.texcoord[6];\n"
        " MOV result.texcoord[7], vertex.texcoord[7];\n"
        "END\n",
        extString.c_str(),
        elwSize,elwSize-1,
        AmbientColorReg,
        Constants1Reg,
        Constants2Reg,
        FirstLightReg + startLight*4 + 0,
        FirstLightReg + startLight*4 + 1,
        FirstLightReg + startLight*4 + 2,
        FirstLightReg + startLight*4 + 3,
        ModelViewProjReg+0,
        ModelViewProjReg+1,
        ModelViewProjReg+2,
        ModelViewProjReg+3,
        killOlwp3, killOlwp3, killOlwp3,
        ModelViewIlwReg+0,
        ModelViewIlwReg+1,
        ModelViewIlwReg+2,
        killOlwp3,
        fragment);

    pProg->XfbStdAttrib = RndGpuPrograms::Pos  |
        RndGpuPrograms::Col0 |
        RndGpuPrograms::Col1 |
        RndGpuPrograms::Fog  |
        RndGpuPrograms::PtSz |
        RndGpuPrograms::AllTex;

    // mask off the pass-thru bit
    for (ProgProperty i = ppTxCd0; i <= ppTxCd0+3; i++)
    {
        pProg->PgmRqmt.Outputs[i] &= ~omPassThru;
        pProg->PgmRqmt.Inputs[i] &= ~omPassThru;
    }
}

//------------------------------------------------------------------------------
void VxGpuPgmGen::GenFixedPosIlwar(Program * pProg)
{
    // GenFixedOpUse exercises the first 9 sets of "light" constants.
    // GenFixedOpUse11 gets 2 more.
    // Here we use the next 2 sets.
    const int lightA = RND_GPU_PROG_VX_INDEX_UseRCC -
        RND_GPU_PROG_VX_INDEX_UseADD + 1;
    const int lightB = lightA+1;

    pProg->Name = "vpPosIlwar";

    const bool useVp3 = (m_BnfExt >= GLRandomTest::ExtLW_gpu_program4_0) ?
            false : true;
    const int elwSize = (useVp3) ? 192 : 256;
    string extString = (useVp3) ?
            "!!ARBvp1.0\nOPTION LW_vertex_program3;\n" : "!!LWvp4.0\n";

    pProg->Pgm = Utility::StrPrintf(
        "%s"
        "#Fixed Pgm TemplatePosIlwar\n"
        " OPTION ARB_position_ilwariant;\n"
        " PARAM pgmElw[%d] = { program.elw[0..%d] };\n"
        " PARAM peAmbient = program.elw[%d];\n"
        " PARAM peLightADir = program.elw[%d];\n"
        " PARAM peLightAHat = program.elw[%d];\n"
        " PARAM peLightAColor = program.elw[%d];\n"
        " PARAM peLightAPow = program.elw[%d];\n"
        " PARAM peLightBDir = program.elw[%d];\n"
        " PARAM peLightBHat = program.elw[%d];\n"
        " PARAM peLightBColor = program.elw[%d];\n"
        " PARAM peLightBPow = program.elw[%d];\n"
        " TEMP R0;\n"
        " TEMP R1;\n"
        " TEMP R2;\n"
        " DP3 R2.x, pgmElw[%d], vertex.normal;\n"
        " DP3 R2.y, pgmElw[%d], vertex.normal;\n"
        " DP3 R2.z, pgmElw[%d], vertex.normal; #  R2 = vtx normal xform into eye space\n"
        " DP3 R2.w, R2, R2;\n"
        " RSQ R2.w, R2.w;\n"
        " MUL R2, R2, R2.w;           #  normalize eye-space normal\n"
        " DP3 R0.x, peLightADir, R2;  # R0.x = Ldir DOT n'\n"
        " DP3 R0.y, peLightAHat, R2;  # R0.y = hHat DOT n'\n"
        " MOV R0.w, peLightAPow.x;    # R0.w = spelwlar power\n"
        " LIT R0, R0;                 # R0 = lighting values for first light\n"
        " DP3 R1.x, peLightBDir, R2;  # R1.x = Ldir DOT n'\n"
        " DP3 R1.y, peLightBHat, R2;  # R1.y = hHat DOT n'\n"
        " MOV R1.w, peLightBPow.x;    # R1.w = spelwlar power\n"
        " LIT R1, R1;                 # R1 = lighting values for second light\n"
        " MUL R2, R1.y, peLightBColor; # R2 =  light1 diffuse * light1 color\n"
        " MAD R2, R0.y, peLightAColor, R2; # R2 += light0 diffuse * light0 color\n"
        " MAD result.color, R2, vertex.color, peAmbient; # col0 = R2 * material color + global ambient\n"
        " MUL R2, R0.z, peLightAColor; # R2 = light0 spelwlar * light0 color\n"
        " MAD result.color.secondary, R1.z, peLightBColor, R2; # col1 = light1 spelwlar * light1 color + R2\n"
        " MOV result.fogcoord.x, vertex.fogcoord.x;\n"
        " MOV result.pointsize.x, pgmElw[%d].z;\n"
        "%sMOV result.attrib[6].xyz, vertex.normal;\n"
        " MOV  result.color.back, vertex.color;\n"
        " MOV  result.color.back.secondary, vertex.color.secondary;\n"
        " MOV result.texcoord[0], vertex.texcoord[0];\n"
        " MOV result.texcoord[1], vertex.texcoord[1];\n"
        " MOV result.texcoord[2], vertex.texcoord[2];\n"
        " MOV result.texcoord[3], vertex.texcoord[3];\n"
        " MOV result.texcoord[4], vertex.texcoord[4];\n"
        " MOV result.texcoord[5], vertex.texcoord[5];\n"
        " MOV result.texcoord[6], vertex.texcoord[6];\n"
        " MOV result.texcoord[7], vertex.texcoord[7];\n"
        "END\n",
        extString.c_str(),
        elwSize,elwSize-1,
        AmbientColorReg,
        FirstLightReg + lightA*4 + 0,
        FirstLightReg + lightA*4 + 1,
        FirstLightReg + lightA*4 + 2,
        FirstLightReg + lightA*4 + 3,
        FirstLightReg + lightB*4 + 0,
        FirstLightReg + lightB*4 + 1,
        FirstLightReg + lightB*4 + 2,
        FirstLightReg + lightB*4 + 3,
        ModelViewIlwReg+0,
        ModelViewIlwReg+1,
        ModelViewIlwReg+2,
        LightIndexReg,
        useVp3 ? "#" : " ");

    pProg->XfbStdAttrib = RndGpuPrograms::Col0 |
        RndGpuPrograms::Col1 |
        RndGpuPrograms::Fog  |
        RndGpuPrograms::PtSz |
        RndGpuPrograms::AllTex;
    pProg->PgmRqmt.Inputs.erase(ppPos);
    pProg->PgmRqmt.Outputs.erase(ppPos);
}

//------------------------------------------------------------------------------
// Generate a set of fixed programs.
//
void VxGpuPgmGen::GenerateFixedPrograms
(
    Programs * pProgramStorage
)
{
    InitPgmElw(NULL);
    if (m_pGLRandom->HasExt(extGpu5))
        m_BnfExt = extGpu5;
    else if (m_pGLRandom->HasExt(extGpu4_1))
        m_BnfExt = extGpu4_1;
    else if (m_pGLRandom->HasExt(extGpu4))
        m_BnfExt = extGpu4;
    else if (m_pGLRandom->HasExt(extVp3))
        m_BnfExt = extVp3;
    else
        m_BnfExt = extVp2;

    GLint pIdx;
    for (pIdx = 0; pIdx < RND_GPU_PROG_VX_INDEX_NumFixedProgs; pIdx++)
    {
        ProgProperty i;
        // Fill in common defaults for this Program struct:
        Program tmp;
        InitProg(&tmp, GL_VERTEX_PROGRAM_LW,NULL);

        // Setup the default input/output pgm requirements
        tmp.PgmRqmt.Inputs[ppPos]      = omXYZW;
        tmp.PgmRqmt.Inputs[ppNrml]     = omXYZ;
        tmp.PgmRqmt.Inputs[ppPriColor] = omXYZW;
        tmp.PgmRqmt.Inputs[ppFogCoord] = omX;
        for ( i=ppTxCd0; i<=ppTxCd7; i++)
            tmp.PgmRqmt.Inputs[i] = omXYZW | omPassThru;
        tmp.PgmRqmt.Outputs[ppPos]     = omXYZW;
        tmp.PgmRqmt.Outputs[ppPriColor]= omXYZW;
        tmp.PgmRqmt.Outputs[ppSecColor]= omXYZW;
        tmp.PgmRqmt.Outputs[ppBkPriColor]= omXYZW;
        tmp.PgmRqmt.Outputs[ppBkSecColor]= omXYZW;
        tmp.PgmRqmt.Outputs[ppPtSize]  = omX;
        tmp.PgmRqmt.Outputs[ppFogCoord]= omX;
        if(m_BnfExt >= extGpu4)
        {
            tmp.PgmRqmt.Outputs[ppNrml] = omXYZ;
        }

        for ( i=ppTxCd0; i<=ppTxCd7; i++)
            tmp.PgmRqmt.Outputs[i] = omXYZW | omPassThru ;

        switch (pIdx)
        {
        case RND_GPU_PROG_VX_INDEX_UseAttr1:
        case RND_GPU_PROG_VX_INDEX_UseAttrNRML:
        case RND_GPU_PROG_VX_INDEX_UseAttrCOL0:
        case RND_GPU_PROG_VX_INDEX_UseAttrFOGC:
        case RND_GPU_PROG_VX_INDEX_UseAttr6:
        case RND_GPU_PROG_VX_INDEX_UseAttr7:
        case RND_GPU_PROG_VX_INDEX_UseAttrTEX4:
        case RND_GPU_PROG_VX_INDEX_UseAttrTEX5:
        case RND_GPU_PROG_VX_INDEX_UseAttrTEX6:
        case RND_GPU_PROG_VX_INDEX_UseAttrTEX7:
            GenFixedAttrUse(&tmp, pIdx - RND_GPU_PROG_VX_INDEX_UseAttr1);
            break;

        case RND_GPU_PROG_VX_INDEX_IdxLight012:
        case RND_GPU_PROG_VX_INDEX_IdxLight345:
        case RND_GPU_PROG_VX_INDEX_IdxLight678:
        case RND_GPU_PROG_VX_INDEX_IdxLight91011:
            GenFixedIdxLight(&tmp, (pIdx - RND_GPU_PROG_VX_INDEX_IdxLight012) * 3);
            break;

        case RND_GPU_PROG_VX_INDEX_UseADD:
        case RND_GPU_PROG_VX_INDEX_UseRCP:
        case RND_GPU_PROG_VX_INDEX_UseDST:
        case RND_GPU_PROG_VX_INDEX_UseMIN:
        case RND_GPU_PROG_VX_INDEX_UseMAX:
        case RND_GPU_PROG_VX_INDEX_UseSLT:
        case RND_GPU_PROG_VX_INDEX_UseSGE:
        case RND_GPU_PROG_VX_INDEX_UseEXP:
        case RND_GPU_PROG_VX_INDEX_UseLOG:
            GenFixedOpUse(&tmp, pIdx - RND_GPU_PROG_VX_INDEX_UseADD);
            break;

        case RND_GPU_PROG_VX_INDEX_UseDPH:
        case RND_GPU_PROG_VX_INDEX_UseRCC:
            GenFixedOpUse11(&tmp, pIdx - RND_GPU_PROG_VX_INDEX_UseDPH);
            break;

        case RND_GPU_PROG_VX_INDEX_PosIlwar:
            GenFixedPosIlwar(&tmp);
            break;

        default:
            {
                MASSERT(!"Missing case in VxGpuPgmGen::GenerateFixedPrograms");
            }
        }
        pProgramStorage->AddProg(psFixed, tmp);
    }
}

//------------------------------------------------------------------------------
// Generate a new random program.
//
void VxGpuPgmGen::GenerateRandomProgram
(
    PgmRequirements *pRqmts,
    Programs * pProgramStorage,
    const string & pgmName
)
{
    m_ThisProgWritesLayerVport = (0 != (*m_pPickers)[RND_GPU_PROG_VX_WRITES_TO_LAYR_VPORT].Pick());

    InitPgmElw(pRqmts);
    m_GS.Prog.Name = pgmName;

    ProgramSequence(m_PgmHeader.c_str());

    m_GS.Prog.CalcXfbAttributes();

    pProgramStorage->AddProg(psRandom, m_GS.Prog);
}

//------------------------------------------------------------------------------
// Perform the Homogeneous position transformation required by all vertex pgms.
void VxGpuPgmGen::MainSequenceStart()
{
    const int posReg = PickTempReg();
    const int offReg = posReg > GetFirstTempReg() ? (posReg-1) : (posReg+1);

    // "DP4 pos.x, pgmElw[0], vertex.position;"
    PgmInstruction Op;
    UtilOpDst  (&Op, RND_GPU_PROG_OPCODE_dp4, posReg);
    UtilOpInReg(&Op, GetConstReg());
    UtilOpInReg(&Op, RND_GPU_PROG_VX_REG_vOPOS);
    Op.InReg[0].IndexOffset = ModelViewProjReg + 0;
    Op.OutMask = compX;
    UtilOpCommit(&Op);

    // "DP4 pos.y, pgmElw[1], vertex.position;"
    Op.InReg[0].IndexOffset = ModelViewProjReg + 1;
    Op.OutMask = compY;
    UtilOpCommit(&Op);

    // "DP4 pos.z, pgmElw[2], vertex.position;"
    Op.InReg[0].IndexOffset = ModelViewProjReg + 2;
    Op.OutMask = compZ;
    UtilOpCommit(&Op);

    // "DP4 pos.w, pgmElw[3], vertex.position;"
    Op.InReg[0].IndexOffset = ModelViewProjReg + 3;
    Op.OutMask = compW;
    UtilOpCommit(&Op);

    if (m_pGLRandom->HasExt(GLRandomTest::ExtARB_draw_instanced))
    {
        // "I2F off.x, vertex.instance.x;"
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_i2f, offReg, compX, dtNA);
        UtilOpInReg(&Op, RND_GPU_PROG_VX_REG_vINST, scX);
        UtilOpCommit(&Op);

        // "MUL off.x, off.x, -0.05;"
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mul, offReg, compX, dtNA);
        UtilOpInReg(&Op, offReg, scX);
        UtilOpInConstF32(&Op, -0.05F);
        UtilOpCommit(&Op);

        // "MAD pos.xyz, off.x, pos.x, pos;"
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mad, posReg, compXYZ, dtNA);
        UtilOpInReg(&Op, offReg, scX);
        UtilOpInReg(&Op, posReg, scX);
        UtilOpInReg(&Op, posReg);
        UtilOpCommit(&Op);
    }

    // "MOV result.position, pos;"
    UtilCopyReg(RND_GPU_PROG_VX_REG_oHPOS, posReg);

    if (m_GS.Prog.PgmRqmt.Outputs.count(ppPtSize))
    {
        // Produce a point size based on world-space Y (+- 7.5k):
        //  we get point size between 0 and 13 pixels.
        m_GS.MainBlock.AddStatement("LG2 result.pointsize.x, |vertex.position.y|;");
        UpdateRegScore(RND_GPU_PROG_VX_REG_oPSIZ, compX, 1);
    }

    // RegState housekeeping -- mark inputs as used, outputs as written.
    MarkRegUsed    (RND_GPU_PROG_VX_REG_vOPOS);
    UpdateRegScore (RND_GPU_PROG_VX_REG_oHPOS, compXYZW, 1);
}

//------------------------------------------------------------------------------
// Do any special processing at the end of the program.
void VxGpuPgmGen::MainSequenceEnd()
{
    UtilProcessPassthruRegs();

    if (m_ThisProgWritesLayerVport &&
        m_pGLRandom->HasExt(GLRandomTest::ExtLW_viewport_array2))
    {
        AddLayeredOutputRegs(RND_GPU_PROG_VX_REG_oLAYR,
                 RND_GPU_PROG_VX_REG_oVPORTIDX, RND_GPU_PROG_VX_REG_oVPORTMASK);
    }
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::PickTempReg() const
{
    return( m_pFpCtx->Rand.GetRandom(RND_GPU_PROG_VX_REG_R0,
                                     RND_GPU_PROG_VX_REG_R15));
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::PickTxTarget() const
{
    int target = (*m_pPickers)[RND_GPU_PROG_VX_TX_DIM].Pick();
    return FilterTexTarget(target);
}

//------------------------------------------------------------------------------
int VxGpuPgmGen::PickTxOffset() const
{
    return (*m_pPickers)[RND_GPU_PROG_VX_TX_OFFSET].Pick();
}

//------------------------------------------------------------------------------
GLRandom::ProgProperty VxGpuPgmGen::GetProp( int reg) const
{
    MASSERT(reg >= 0 && reg < RND_GPU_PROG_VX_REG_NUM_REGS);
    return (ProgProperty)RegToProp[reg];
}

RC VxGpuPgmGen::GenerateXfbPassthruProgram(Programs * pProgramStorage)
{
    GLRandom::Program pgm;
    string s = "!!LWvp4.0\n"
               "#Passthru Pgm \n";

    UINT32  StdAttr = 0;
    UINT32  UsrAttr = 0;
    UINT32  bit;

    pgm.Target = PgmGen::pkTargets[pkVertex];
    pgm.Name = "vxPassThru";

    m_pGLRandom->m_GpuPrograms.GetXfbAttrFlags( &StdAttr, &UsrAttr);
    MASSERT( StdAttr != 0 );

    for(bit = 0; bit < 32; bit++)
    {
        const UINT32 xfbProp = StdAttr & (1<<bit);
        if(xfbProp)
        {
            const UINT32 compMask = (xfbProp == RndGpuPrograms::Fog) ? omX : omXYZW;

            for ( unsigned i=0; i < NUMELEMS(VxPassthruRegs); i++)
            {
                if( xfbProp == VxPassthruRegs[i].XfbProp)
                {
                    // ie. MOV result.position.xyzw, fragment.position.xyzw
                    s += Utility::StrPrintf(" MOV %s%s, %s%s;\n",
                            GetRegName(VxPassthruRegs[i].OutReg).c_str(),
                            CompMaskToString(compMask),
                            GetRegName(VxPassthruRegs[i].InReg).c_str(),
                            CompMaskToString(compMask));
                    pgm.PgmRqmt.Inputs[(GLRandom::ProgProperty)VxPassthruRegs[i].Prop] = compMask;
                    pgm.PgmRqmt.Outputs[(GLRandom::ProgProperty)VxPassthruRegs[i].Prop] = compMask;
                    break;
                }
            }
        }
    }

    for(bit = 0; bit < 32; bit++)
    {
        const UINT32 xfbProp = UsrAttr & (1<<bit);
        if(xfbProp)
        {
            for ( unsigned i=0; i < NUMELEMS(VxPassthruRegs); i++)
            {
                if( xfbProp == VxPassthruRegs[i].XfbProp)
                {
                    s += Utility::StrPrintf(" MOV %s%s, %s%s;\n",
                        GetRegName(VxPassthruRegs[i].OutReg).c_str(),
                        CompMaskToString(omXYZW),
                        GetRegName(VxPassthruRegs[i].InReg).c_str(),
                        CompMaskToString(omXYZW));
                    pgm.PgmRqmt.Inputs[(GLRandom::ProgProperty)VxPassthruRegs[i].Prop] = omXYZW;
                    pgm.PgmRqmt.Outputs[(GLRandom::ProgProperty)VxPassthruRegs[i].Prop] = omXYZW;
                }
            }
        }
    }

    s += "END\n";
    pgm.Pgm = s;
    pProgramStorage->AddProg(psSpecial, pgm);
    return OK;
}

//------------------------------------------------------------------------------
// Random vertex programs tend to read every possible vertex attribute.
// We'd rather they sometimes left the input vertex size smaller.
//
// So prefer vertex attributes we've already read to ones not before read
// in this program.
//
UINT32 VxGpuPgmGen::CalcScore(PgmInstruction *pOp, int reg, int argIdx)
{
    const UINT32 Score = GpuPgmGen::CalcScore(pOp, reg, argIdx);

    const RegState & v = GetVar(reg);
    if ((v.Attrs & regIn) && !(v.Attrs & regUsed))
    {
        return Score / 2;
    }

    return Score;
}

