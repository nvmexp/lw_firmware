/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// FrGpuPgmGen
//      Fragment program generator for GLRandom. This class strickly follows the
//      Bakus-Nuar form to specify the grammar.
//
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include "glrandom.h"  // declaration of our namespace
#include "pgmgen.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include <math.h>

using namespace GLRandom;

// This is the master table of fragment specific opcodes. The opcodes that
// actually get picked are determined by the RndGpuPrograms::SetDefaultPicker()
// and the ExtVer column.
const GpuPgmGen::OpData FrGpuPgmGen::s_FrOpData4[] =
{                                                                                                                          //  Modifiers //$
                                                                                                                           // F I C S H D  Out Inputs    Description
//     Name    OpInstType  ExtVer nIn inType1   inType2  inType3   outType    outMask opMod Types defType  IsTx  mul add   // - - - - - -  --- --------  --------------------------------
     { "DDX"   ,oitVector, vnafp2,1,  {atVector, atNone, atNone}  ,atVector,  omXYZW, smCS, dtFx,   dtF32, false,   5,  8} // X - X X X F  v   v         partial dericative relative to X
    ,{ "DDY"   ,oitVector, vnafp2,1,  {atVector, atNone, atNone}  ,atVector,  omXYZW, smCS, dtFx,   dtF32, false,   5,  8} // X - X X X F  v   v         partial derviative relative to Y
    ,{ "KIL"   ,oitSpecial,vnafp2,1,  {atVector, atNone, atNone}  ,atNone  ,  omNone, smNA, dtFSx,  dtF32, false,   5,  8} // X X - - X F  -   vc        kill fragment
    // LW_gpu_program4_1 opcodes
    ,{ "LOD"   ,oitTex,    gpu4_1,1,  {atVector, atNone, atNone}  ,atVector,  omXYZW, smCS, dtFx,   dtF32, true,    5,  8} // X - X X - F  v   v         Texture Level Of Detail (LOD)
    // LW_gpu_program5 opcodes
    ,{ "IPAC"  ,oitSpecial,gpu5,  1,  {atVector, atNone,   atNone} ,atVector, omXYZW, smCS, dtFx,   dtF32, false,   1,  1} // X - X X X F  v   v         Interpolate at Centroid
    ,{ "IPAO"  ,oitSpecial,gpu5,  2,  {atVector, atVector, atNone} ,atVector, omXYZW, smCS, dtFx,   dtF32, false,   1,  1} // X - X X X F  v   v         Interpolate with offset
    ,{ "IPAS"  ,oitSpecial,gpu5,  2,  {atVector, atScalar, atNone} ,atVector, omXYZW, smCS, dtFx,   dtF32, false,   1,  1} // X - X X X F  v   v         Interpolate at sample

    // These instructions are special only because they require a restricted implementation to prevent non-deterministict shaders.
    ,{ "ATOM"  ,oitSpecial,gpu5,  2,  {atVector, atScalar, atNone} ,atScalar, omX,    smNA, dtRun,  dtU32e,false,   1,  1} // - - X - - -  s   v,su      Atomic memory transaction
    ,{ "LOAD"  ,oitSpecial,gpu5,  1,  {atScalar, atNone  , atNone} ,atVector, omXYZW, smCS, dtFI6X4,dtF32e,false,   1,  1} // - - X X - F  v   v,su      Global load
    ,{ "STORE" ,oitSpecial,gpu5,  2,  {atVector, atScalar, atNone} ,atNone  , omXYZW, smNA, dtFI6X4,dtF32e,false,   1,  1} // - - - - - -  v   v,su      Global store
    ,{ "ATOMIM",oitSpecial,gpu5,  2,  {atVectNS, atVectNS, atNone} ,atVector, omX,    smNA, dtRun,  dtU32e,true,   1,  1}  // - - X - - -  s   v,vs,i    atomic image operation

};

const static char * RegNames[] =
{//   LW name
     "fragment.position"
    ,"fragment.color.primary"
    ,"fragment.color.secondary"
    ,"fragment.fogcoord"
    ,"fragment.texcoord[0]"
    ,"fragment.texcoord[1]"
    ,"fragment.texcoord[2]"
    ,"fragment.texcoord[3]"
    ,"fragment.texcoord[4]"
    ,"fragment.texcoord[5]"
    ,"fragment.texcoord[6]"
    ,"fragment.texcoord[7]"
    ,"fragment.attrib[6]"
    ,"fragment.clip[0]"
    ,"fragment.clip[1]"
    ,"fragment.clip[2]"
    ,"fragment.clip[3]"
    ,"fragment.clip[4]"
    ,"fragment.clip[5]"
    ,"fragment.facing"
    ,"fragment.layer"
    ,"fragment.viewport"
    ,"fragment.fullycovered"
    ,"fragment.barycoord"
    ,"fragment.barynopersp"
    ,"fragment.shadingrate"
    ,"result.color"
    ,"result.color[1]"
    ,"result.color[2]"
    ,"result.color[3]"
    ,"result.color[4]"
    ,"result.color[5]"
    ,"result.color[6]"
    ,"result.color[7]"
    ,"result.color[8]"
    ,"result.color[9]"
    ,"result.color[10]"
    ,"result.color[11]"
    ,"result.color[12]"
    ,"result.color[13]"
    ,"result.color[14]"
    ,"result.color[15]"
    ,"result.depth"
    ,"tmpR0"
    ,"tmpR1"
    ,"tmpR2"
    ,"tmpR3"
    ,"tmpR4"
    ,"tmpR5"
    ,"tmpR6"
    ,"tmpR7"
    ,"tmpR8"
    ,"tmpR9"
    ,"tmpR10"
    ,"tmpR11"
    ,"tmpR12"
    ,"tmpR13"
    ,"tmpR14"
    ,"tmpR15"
    ,"tmpA128"
    ,"pgmElw"                   // RND_GPU_PROG_FR_REG_Const
    ,"program.local"            // RND_GPU_PROG_FR_REG_LocalElw
    ,""                         // RND_GPU_PROG_FR_REG_ConstVect
    ,""                         // RND_GPU_PROG_FR_REG_Literal
    ,"fragment.smid"            // RND_GPU_PROG_FR_REG_SMID
    ,"sboAddr"                  // RND_GPU_PROG_FR_REG_SBO
    ,"garbage"                  // RND_GPU_PROG_FR_REG_Garbage
    ,"cbuf"                     // RND_GPU_PROG_FR_REG_PaboElw
    ,"tmpArray"                 // RND_GPU_PROG_FR_REG_TempArray
    ,"Subs"                     // RND_GPU_PROG_FR_REG_Subs
    ,"tmpA1"                    // RND_GPU_PROG_FR_REG_A1
    ,"tmpASubs"                 // RND_GPU_PROG_FR_REG_ASubs
    ,"tmpA3"                    // RND_GPU_PROG_FR_REG_A3
    ,"atom64"                   // RND_GPU_PROG_FR_REG_Atom64
};

// lookup table for registers that can be passed through this shader.
const static GpuPgmGen::PassthruRegs FrPassthruRegs[] =
{
//  Xfb prop                 Program prop  input                       output
     {RndGpuPrograms::Col0, ppPriColor,   RND_GPU_PROG_FR_REG_fCOL0,  RND_GPU_PROG_FR_REG_oCOL0}
    ,{0,                    ppIlwalid,    0                        ,  0                       }
};

static const int RegToProp[] = {
    ppPos,         //RND_GPU_PROG_FR_REG_fWPOS
    ppPriColor,    //RND_GPU_PROG_FR_REG_fCOL0
    ppSecColor,    //RND_GPU_PROG_FR_REG_fCOL1
    ppFogCoord,    //RND_GPU_PROG_FR_REG_fFOGC
    ppTxCd0,       //RND_GPU_PROG_FR_REG_fTEX0
    ppTxCd0+1,     //RND_GPU_PROG_FR_REG_fTEX1
    ppTxCd0+2,     //RND_GPU_PROG_FR_REG_fTEX2
    ppTxCd0+3,     //RND_GPU_PROG_FR_REG_fTEX3
    ppTxCd0+4,     //RND_GPU_PROG_FR_REG_fTEX4
    ppTxCd0+5,     //RND_GPU_PROG_FR_REG_fTEX5
    ppTxCd0+6,     //RND_GPU_PROG_FR_REG_fTEX6
    ppTxCd0+7,     //RND_GPU_PROG_FR_REG_fTEX7
    ppNrml,        //RND_GPU_PROG_FR_REG_fNRML
    ppClip0,       //RND_GPU_PROG_FR_REG_fCLP0
    ppClip0+1,     //RND_GPU_PROG_FR_REG_fCLP1
    ppClip0+2,     //RND_GPU_PROG_FR_REG_fCLP2
    ppClip0+3,     //RND_GPU_PROG_FR_REG_fCLP3
    ppClip0+4,     //RND_GPU_PROG_FR_REG_fCLP4
    ppClip0+5,     //RND_GPU_PROG_FR_REG_fCLP5
    ppFacing,      //RND_GPU_PROG_FR_REG_fFACE
    ppLayer,       //RND_GPU_PROG_FR_REG_fLAYR
    ppViewport,    //RND_GPU_PROG_FR_REG_fVPORT
    ppFullyCovered,//RND_GPU_PROG_FR_REG_fFULLYCOVERED
    ppBaryCoord,   //RND_GPU_PROG_FR_REG_fBARYCOORD
    ppBaryNoPersp, //RND_GPU_PROG_FR_REG_fBARYNOPERSP
    ppShadingRate, //RND_GPU_PROG_FR_REG_fSHADINGRATE
    ppClrBuf0,     //RND_GPU_PROG_FR_REG_oCOL0
    ppClrBuf0+1,   //RND_GPU_PROG_FR_REG_oCOL1
    ppClrBuf0+2,   //RND_GPU_PROG_FR_REG_oCOL2
    ppClrBuf0+3,   //RND_GPU_PROG_FR_REG_oCOL3
    ppClrBuf0+4,   //RND_GPU_PROG_FR_REG_oCOL4
    ppClrBuf0+5,   //RND_GPU_PROG_FR_REG_oCOL5
    ppClrBuf0+6,   //RND_GPU_PROG_FR_REG_oCOL6
    ppClrBuf0+7,   //RND_GPU_PROG_FR_REG_oCOL7
    ppClrBuf0+8,   //RND_GPU_PROG_FR_REG_oCOL8
    ppClrBuf0+9,   //RND_GPU_PROG_FR_REG_oCOL9
    ppClrBuf0+10,  //RND_GPU_PROG_FR_REG_oCOL10
    ppClrBuf0+11,  //RND_GPU_PROG_FR_REG_oCOL11
    ppClrBuf0+12,  //RND_GPU_PROG_FR_REG_oCOL12
    ppClrBuf0+13,  //RND_GPU_PROG_FR_REG_oCOL13
    ppClrBuf0+14,  //RND_GPU_PROG_FR_REG_oCOL14
    ppClrBuf0+15,  //RND_GPU_PROG_FR_REG_oCOL15
    ppDepth,       //RND_GPU_PROG_FR_REG_oDEP
    ppIlwalid,     //RND_GPU_PROG_FR_REG_R0
    ppIlwalid,     //RND_GPU_PROG_FR_REG_R1
    ppIlwalid,     //RND_GPU_PROG_FR_REG_R2
    ppIlwalid,     //RND_GPU_PROG_FR_REG_R3
    ppIlwalid,     //RND_GPU_PROG_FR_REG_R4
    ppIlwalid,     //RND_GPU_PROG_FR_REG_R5
    ppIlwalid,     //RND_GPU_PROG_FR_REG_R6
    ppIlwalid,     //RND_GPU_PROG_FR_REG_R7
    ppIlwalid,     //RND_GPU_PROG_FR_REG_R8
    ppIlwalid,     //RND_GPU_PROG_FR_REG_R9
    ppIlwalid,     //RND_GPU_PROG_FR_REG_R10
    ppIlwalid,     //RND_GPU_PROG_FR_REG_R11
    ppIlwalid,     //RND_GPU_PROG_FR_REG_R12
    ppIlwalid,     //RND_GPU_PROG_FR_REG_R13
    ppIlwalid,     //RND_GPU_PROG_FR_REG_R14
    ppIlwalid,     //RND_GPU_PROG_FR_REG_R15
    ppIlwalid,     //RND_GPU_PROG_FR_REG_A128
    ppIlwalid,     //RND_GPU_PROG_FR_REG_Const
    ppIlwalid,     //RND_GPU_PROG_FR_REG_LocalElw
    ppIlwalid,     //RND_GPU_PROG_FR_REG_ConstVect
    ppIlwalid,     //RND_GPU_PROG_FR_REG_Literal
    ppIlwalid,     //RND_GPU_PROG_FR_REG_SMID
    ppIlwalid,     //RND_GPU_PROG_FR_REG_SBO
    ppIlwalid,     //RND_GPU_PROG_FR_REG_Garbage
    ppIlwalid,     //RND_GPU_PROG_FR_REG_PaboElw
    ppIlwalid,     //RND_GPU_PROG_FR_REG_TempArray
    ppIlwalid,     //RND_GPU_PROG_FR_REG_Subs
    ppIlwalid,     //RND_GPU_PROG_FR_REG_A1
    ppIlwalid,     //RND_GPU_PROG_FR_REG_ASubs
    ppIlwalid,     //RND_GPU_PROG_FR_REG_A3
    ppIlwalid      //RND_GPU_PROG_FR_REG_Atom64
};

// This array stores the IO attributes for each ProgProperty (see glr_comm.h).
// Each property uses 2 bits, we lwrrently have 82 properties so we need 164
// bits.
const static UINT64 s_FrppIOAttribs[] =
{
    (
    (UINT64)ppIORd          << (ppPos*2) |
    (UINT64)ppIONone        << (ppNrml*2) |
    (UINT64)ppIORw          << (ppPriColor*2) |
    (UINT64)ppIORd          << (ppSecColor*2) |
    (UINT64)ppIONone        << (ppBkPriColor*2) |
    (UINT64)ppIONone        << (ppBkSecColor*2) |
    (UINT64)ppIORd          << (ppFogCoord*2) |
    (UINT64)ppIONone        << (ppPtSize*2) |
    (UINT64)ppIONone        << (ppVtxId*2) |
    (UINT64)ppIONone        << (ppInstance*2) |
    (UINT64)ppIORd          << (ppFacing*2) |
    (UINT64)ppIOWr          << (ppDepth*2) |
    (UINT64)ppIORd          << (ppPrimId*2) |
    (UINT64)ppIORd          << (ppLayer*2) |
    (UINT64)ppIORd          << (ppViewport*2) |
    (UINT64)ppIONone        << (ppPrimitive*2) |
    (UINT64)ppIORd          << (ppFullyCovered*2) |
    (UINT64)ppIORd          << (ppBaryCoord*2) |
    (UINT64)ppIORd          << (ppBaryNoPersp*2) |
    (UINT64)ppIORd          << (ppShadingRate*2) |
    (UINT64)ppIORd          << (ppClip0*2) |
    (UINT64)ppIORd          << ((ppClip0+1)*2) |
    (UINT64)ppIORd          << ((ppClip0+2)*2) |
    (UINT64)ppIORd          << ((ppClip0+3)*2) |
    (UINT64)ppIORd          << ((ppClip0+4)*2) |
    (UINT64)ppIORd          << (ppClip5*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+0)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+1)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+2)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+3)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+4)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+5)*2)
    ),
    (
    (UINT64)ppIORd          << ((ppGenAttr0+6 -32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+7 -32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+8 -32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+9 -32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+10-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+11-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+12-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+13-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+14-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+15-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+16-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+17-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+18-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+19-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+20-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+21-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+22-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+23-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+24-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+25-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+26-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+27-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+28-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+29-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr0+30-32)*2) |
    (UINT64)ppIORd          << ((ppGenAttr31  -32)*2) |
    (UINT64)ppIOWr          << ((ppClrBuf0+0  -32)*2) |
    (UINT64)ppIOWr          << ((ppClrBuf0+1  -32)*2) |
    (UINT64)ppIOWr          << ((ppClrBuf0+2  -32)*2) |
    (UINT64)ppIOWr          << ((ppClrBuf0+3  -32)*2) |
    (UINT64)ppIOWr          << ((ppClrBuf0+4  -32)*2) |
    (UINT64)ppIOWr          << ((ppClrBuf0+5  -32)*2)
    ),
    (
    (UINT64)ppIOWr          << ((ppClrBuf0+6  -64)*2) |
    (UINT64)ppIOWr          << ((ppClrBuf0+7  -64)*2) |
    (UINT64)ppIOWr          << ((ppClrBuf0+8  -64)*2) |
    (UINT64)ppIOWr          << ((ppClrBuf0+9  -64)*2) |
    (UINT64)ppIOWr          << ((ppClrBuf0+10 -64)*2) |
    (UINT64)ppIOWr          << ((ppClrBuf0+11 -64)*2) |
    (UINT64)ppIOWr          << ((ppClrBuf0+12 -64)*2) |
    (UINT64)ppIOWr          << ((ppClrBuf0+13 -64)*2) |
    (UINT64)ppIOWr          << ((ppClrBuf0+14 -64)*2) |
    (UINT64)ppIOWr          << ((ppClrBuf15   -64)*2) |
    (UINT64)ppIORd          << ((ppTxCd0+0    -64)*2) |
    (UINT64)ppIORd          << ((ppTxCd0+1    -64)*2) |
    (UINT64)ppIORd          << ((ppTxCd0+2    -64)*2) |
    (UINT64)ppIORd          << ((ppTxCd0+3    -64)*2) |
    (UINT64)ppIORd          << ((ppTxCd0+4    -64)*2) |
    (UINT64)ppIORd          << ((ppTxCd0+5    -64)*2) |
    (UINT64)ppIORd          << ((ppTxCd0+6    -64)*2) |
    (UINT64)ppIORd          << ((ppTxCd7      -64)*2)
    )
};

//------------------------------------------------------------------------------
// Return the ProgProperty's IO attributes.
int FrGpuPgmGen::GetPropIOAttr(ProgProperty prop) const
{
    if (prop >= ppPos && prop <= ppTxCd7)
    {
        // 32 entries per UINT64
        const int index = prop / 32;
        const int shift = prop % 32;
        return ((s_FrppIOAttribs[index] >> shift*2) & 0x3);
    }
    return ppIONone;
}
//------------------------------------------------------------------------------
// Fragment program specific generation routines
//
FrGpuPgmGen::FrGpuPgmGen
(
    GLRandomTest * pGLRandom,
    FancyPickerArray *pPicker
) : GpuPgmGen(pGLRandom, pPicker, pkFragment)
    ,m_ARB_draw_buffers(false)
    ,m_MaxIplOffset(1)
    ,m_MinIplOffset(0)
    ,m_SboSubIdx(-1)
    ,m_EndStrategy(RND_GPU_PROG_FR_END_STRATEGY_frc)
{
}

//------------------------------------------------------------------------------
FrGpuPgmGen::~FrGpuPgmGen()
{
}

//------------------------------------------------------------------------------
// Setup the BNF gramar and program header based on the highest supported opengl
// extension.
void FrGpuPgmGen::GetHighestSupportedExt()
{
    if (m_pGLRandom->HasExt(extGpu5))
    {
        m_PgmHeader = "!!LWfp5.0\n";
        m_BnfExt = extGpu5;
    }
    else if (m_pGLRandom->HasExt(extGpu4_1))
    {
        m_PgmHeader = "!!LWfp4.1\n";
        m_BnfExt = extGpu4_1;
    }
    else if (m_pGLRandom->HasExt(extGpu4))
    {
        m_PgmHeader = "!!LWfp4.0\n";
        m_BnfExt = extGpu4;
    }
    else if (m_pGLRandom->HasExt(extFp2))
    {
        m_PgmHeader = "!!ARBfp1.0\n";
        m_BnfExt = extFp2;
    }
    else
    {
        m_PgmHeader = "!!ARBfp1.0\n";
        m_BnfExt = extFp;
    }
}
//------------------------------------------------------------------------------
// Initialize the programming environment and program generation state variables.
// Note: Setup the program environment, register attributes, program target,
//       common data types, and interestingness scoring.
//
void FrGpuPgmGen::InitPgmElw()
{
    // Our register-names list must stay in sync with the list of macros in
    // the glr_comm.h file.
    MASSERT(sizeof(RegNames)/sizeof(RegNames[0]) == RND_GPU_PROG_FR_REG_NUM_REGS);

    // determine proper pgm header and BNF extension.
    GetHighestSupportedExt();
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_LW,
                     GL_MAX_PROGRAM_ELW_PARAMETERS_ARB,
                     &m_ElwSize);
    bool bAllowLiterals = true;
    if (m_BnfExt < extGpu4)
    {
        // we need to reserve some room for const scalars we can't avoid.
        m_ElwSize -= 4;

        // Fragment shaders utilize some of the constant buffer for true constant
        // data. ie. MOV R12,0; But we don't lwrrently count the number of const
        // vectors or scalars we use. So randomly decide to allow either const
        // vector/scalar usage or full pgmElw[] indexing.
        if (m_pFpCtx->Rand.GetRandom(0,1) == 1)
        {
            // allow space for 64 const vectors or 256 const scalars
            m_ElwSize -= 60;
            bAllowLiterals = true;
        }
        else // we are going to use full program indexing this time, no space reserved for literals
        {
            bAllowLiterals = false;
        }
    }

    m_SboSubIdx = -1;

    InitGenState(
            RND_GPU_PROG_FR_OPCODE_NUM_OPCODES,// total number of opcodes
            s_FrOpData4,                       // additional opcodes to append
            NULL,                              // optional program requirements
            FrPassthruRegs,                    // pointer to lookup table of passthru registers
            bAllowLiterals);
}

//------------------------------------------------------------------------------
void FrGpuPgmGen::InitMailwars
(
    Block * pMainBlock
)
{

    for (int reg = 0; reg < RND_GPU_PROG_FR_REG_NUM_REGS; reg++)
    {
        RegState v;
        v.Id            = reg;

        // Note: ReadableMask, WriteableMask, WrittenMask and Scores
        // are all 0 unless overridden below!
        //
        // InitReadOnlyReg also sets ReadableMask and WrittenMask.

        switch (reg)
        {
            case RND_GPU_PROG_FR_REG_fFOGC :
            case RND_GPU_PROG_FR_REG_fFULLYCOVERED :
                v.Attrs       |= regIn;
                v.InitReadOnlyReg(10, compX);
                break;

            case RND_GPU_PROG_FR_REG_fCLP0 :
            case RND_GPU_PROG_FR_REG_fCLP1 :
            case RND_GPU_PROG_FR_REG_fCLP2 :
            case RND_GPU_PROG_FR_REG_fCLP3 :
            case RND_GPU_PROG_FR_REG_fCLP4 :
            case RND_GPU_PROG_FR_REG_fCLP5 :
            case RND_GPU_PROG_FR_REG_fFACE :
            case RND_GPU_PROG_FR_REG_fLAYR :
            case RND_GPU_PROG_FR_REG_fVPORT:
                v.Attrs       |= regIn;
                // debugging, leave Score at 0 and Written clear (won't be used)
                break;

            case RND_GPU_PROG_FR_REG_fWPOS :
            case RND_GPU_PROG_FR_REG_fCOL1 :
            case RND_GPU_PROG_FR_REG_fCOL0 :
                v.Attrs       |= regIn;
                v.InitReadOnlyReg(15, compXYZW);
                break;

            case RND_GPU_PROG_FR_REG_fTEX0 :
            case RND_GPU_PROG_FR_REG_fTEX1 :
            case RND_GPU_PROG_FR_REG_fTEX2 :
            case RND_GPU_PROG_FR_REG_fTEX3 :
            case RND_GPU_PROG_FR_REG_fTEX4 :
            case RND_GPU_PROG_FR_REG_fTEX5 :
            case RND_GPU_PROG_FR_REG_fTEX6 :
            case RND_GPU_PROG_FR_REG_fTEX7 :
                v.Attrs       |= regIn;
                v.InitReadOnlyReg(20, compXYZW);
                break;

            // Avoid using fragment.attrib[6] for normals if we are using
            // per-vertex attributes (e.g. vertex[0].attrib[6])
            case RND_GPU_PROG_FR_REG_fNRML :
                if (m_BnfExt >= extGpu4 &&
                    m_EndStrategy != RND_GPU_PROG_FR_END_STRATEGY_per_vtx_attr)
                {
                    v.Attrs       |= regIn;
                    v.InitReadOnlyReg(20, compXYZ);
                }
                break;

            case RND_GPU_PROG_FR_REG_fBARYCOORD :
            case RND_GPU_PROG_FR_REG_fBARYNOPERSP :
                v.Attrs           |= regIn;
                v.InitReadOnlyReg(15, compXYZ);
                break;

            // This register is not that interesting unless you use the correct end strategy
            case RND_GPU_PROG_RF_REG_fSHADINGRATE:
                if (m_pGLRandom->HasExt(GLRandomTest::ExtGL_LW_shading_rate_image))
                {
                    v.Attrs           |= regIn;
                    v.InitReadOnlyReg(5, compXYZ);
                }
                break;

            case RND_GPU_PROG_FR_REG_oCOL0 :
            case RND_GPU_PROG_FR_REG_oCOL1:
            case RND_GPU_PROG_FR_REG_oCOL2:
            case RND_GPU_PROG_FR_REG_oCOL3:
            case RND_GPU_PROG_FR_REG_oCOL4:
            case RND_GPU_PROG_FR_REG_oCOL5:
            case RND_GPU_PROG_FR_REG_oCOL6:
            case RND_GPU_PROG_FR_REG_oCOL7:
            case RND_GPU_PROG_FR_REG_oCOL8:
            case RND_GPU_PROG_FR_REG_oCOL9:
            case RND_GPU_PROG_FR_REG_oCOL10:
            case RND_GPU_PROG_FR_REG_oCOL11:
            case RND_GPU_PROG_FR_REG_oCOL12:
            case RND_GPU_PROG_FR_REG_oCOL13:
            case RND_GPU_PROG_FR_REG_oCOL14:
            case RND_GPU_PROG_FR_REG_oCOL15:
                {
                    const UINT32 bufNum = v.Id - RND_GPU_PROG_FR_REG_oCOL0;

                    if ((bufNum == 0) ||
                        (m_ARB_draw_buffers &&
                         (bufNum < m_pGLRandom->NumColorSurfaces())))
                    {
                        // Note that we *don't* mark this reg as required!
                        // This is belwase MainSequenceEnd will overwrite it.
                        v.Attrs        |= (regOut);
                        v.WriteableMask = compXYZW;
                    }
                }
                break;

            case RND_GPU_PROG_FR_REG_oDEP  :
                v.Attrs        |= regOut;

                // If any path through the fragment program writes depth, all
                // must write depth.
                // I.e. depth must be either (not writable) or (required).
                if ((*m_pPickers)[RND_GPU_PROG_FR_DEPTH_RQD].Pick() != 0)
                {
                    v.Attrs |= regRqd;
                    v.WriteableMask = compZ;
                }
                break;

            case RND_GPU_PROG_FR_REG_R0    :
            case RND_GPU_PROG_FR_REG_R1    :
            case RND_GPU_PROG_FR_REG_R2    :
            case RND_GPU_PROG_FR_REG_R3    :
            case RND_GPU_PROG_FR_REG_R4    :
            case RND_GPU_PROG_FR_REG_R5    :
            case RND_GPU_PROG_FR_REG_R6    :
            case RND_GPU_PROG_FR_REG_R7    :
            case RND_GPU_PROG_FR_REG_R8    :
            case RND_GPU_PROG_FR_REG_R9    :
            case RND_GPU_PROG_FR_REG_R10   :
            case RND_GPU_PROG_FR_REG_R11   :
            case RND_GPU_PROG_FR_REG_R12   :
            case RND_GPU_PROG_FR_REG_R13   :
            case RND_GPU_PROG_FR_REG_R14   :
            case RND_GPU_PROG_FR_REG_R15   :
                v.Attrs |= regTemp;
                v.WriteableMask = compXYZW;
                v.ReadableMask  = compXYZW;
                // Leave WrittenMask and Scores at 0 initially.
                break;

            case RND_GPU_PROG_FR_REG_A128  :
            case RND_GPU_PROG_FR_REG_A3    :
            case RND_GPU_PROG_FR_REG_A1    :
            case RND_GPU_PROG_FR_REG_ASubs :
                v.InitIndexReg();
                break;

            case RND_GPU_PROG_FR_REG_Const :
                v.ArrayLen = m_ElwSize;
                v.InitReadOnlyReg(25, compXYZW);
                break;

            case RND_GPU_PROG_FR_REG_ConstVect: // 4-component literal
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

            case RND_GPU_PROG_FR_REG_LocalElw : // unused!
            case RND_GPU_PROG_FR_REG_PaboElw  : // cbuf[x]
                v.ArrayLen = m_ElwSize;
                v.InitReadOnlyReg(0, compXYZW);
                break;

            case RND_GPU_PROG_FR_REG_Literal:
                v.Attrs |= regLiteral;
                v.InitReadOnlyReg(0, compXYZW);
                break;

            case RND_GPU_PROG_FR_REG_SMID:
                v.InitReadOnlyReg(0, compX);
                break;

            // 64 bit register is used to support access to the Shader buffer objects.
            case RND_GPU_PROG_FR_REG_SBO:
                v.Attrs |= regTemp;
                v.WriteableMask = compXYZW;
                v.ReadableMask  = compXYZW;
                v.Attrs |= regSBO;
                v.Bits = 64;
                // Leave WrittenMask and Scores at 0 initially.
                break;

            // Special register to store garbage results that WILL NOT be read by subsequent
            // instructions. Created to support atomic instructions and sanitization code.
            // This register will not affect the scoring code or the random sequence.
            case RND_GPU_PROG_FR_REG_Garbage:
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

            //special register to pack the MSDW & LSDW from pgmElw[]
            case RND_GPU_PROG_FR_REG_Atom64:
                v.Attrs |= regTemp;
                v.WriteableMask = compXYZW;
                v.Bits = 64;
                break;

            case RND_GPU_PROG_FR_REG_TempArray:
                v.Attrs |= regTemp;
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

            case RND_GPU_PROG_FR_REG_Subs     :
                v.ArrayLen = MaxSubs;
                v.InitReadOnlyReg(0, compXYZW);
                break;

            default:
                MASSERT(!"Unknown fragment program register");
                break;
        }

        // Do not use if unsupported
        SkipRegisterIfUnsupported(&v);

        if (UsingMrtFloorsweepStrategy())
        {
            RestrictInputRegsForFloorsweep(&v);
        }
        pMainBlock->AddVar(v);
    }

    MASSERT(pMainBlock->NumVars() == RND_GPU_PROG_FR_REG_NUM_REGS);
}

//------------------------------------------------------------------------------
void FrGpuPgmGen::SkipRegisterIfUnsupported(RegState * pReg)
{
    switch (pReg->Id)
    {
        case RND_GPU_PROG_FR_REG_fFULLYCOVERED:
            if (m_pGLRandom->HasExt(GLRandomTest::ExtGL_LW_conservative_raster_underestimation))
            {
                return;
            }
            break;
        case RND_GPU_PROG_FR_REG_fBARYCOORD: // fallthrough
        case RND_GPU_PROG_FR_REG_fBARYNOPERSP:
            if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_fragment_shader_barycentric))
            {
                return;
            }
            break;
        default:
            return;
    }
    pReg->Score[0] = pReg->Score[1] = pReg->Score[2] = pReg->Score[3] = 0;
    pReg->ReadableMask = 0;
    pReg->WriteableMask = 0;
    pReg->WrittenMask = 0;
}

//------------------------------------------------------------------------------
// Allow reading only position, normal, txcoord[0..3] during TPC floorsweep,
// to keep input vertex requirements small.
// This allows a shorter vx program, so we can do the minimum shader work
// outside the fragment program itself.
void FrGpuPgmGen::RestrictInputRegsForFloorsweep(RegState * pReg)
{
    if (0 == (pReg->Attrs & regIn))
        return;

    switch (pReg->Id)
    {
        case RND_GPU_PROG_FR_REG_fWPOS:
        case RND_GPU_PROG_FR_REG_fTEX0:
        case RND_GPU_PROG_FR_REG_fTEX1:
        case RND_GPU_PROG_FR_REG_fTEX2:
        case RND_GPU_PROG_FR_REG_fTEX3:
        case RND_GPU_PROG_FR_REG_fNRML:
            return;
        default:
            break;
    }
    // Any other input reg is disabled.
    pReg->Score[0] = pReg->Score[1] = pReg->Score[2] = pReg->Score[3] = 0;
    pReg->ReadableMask = 0;
    pReg->WriteableMask = 0;
    pReg->WrittenMask = 0;
}

//------------------------------------------------------------------------------
// Grammar specific routines
//
int FrGpuPgmGen::PickOp() const
{
    bool validOp = false;
    int op;
    for (int i=0; (i<10) && !validOp; i++)
    {
        op = (*m_pPickers)[RND_GPU_PROG_FR_OPCODE].Pick();
        switch (op)
        {
            case RND_GPU_PROG_FR_OPCODE_atom:
            case RND_GPU_PROG_FR_OPCODE_load:
            case RND_GPU_PROG_FR_OPCODE_store:
                validOp = m_pGLRandom->GetUseSbos();
                break;
            default:
                validOp = true;
        }
    }

    // we must return a valid op so fall back to something safe
    if (!validOp)
        op = RND_GPU_PROG_OPCODE_tex;

    return op;
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::PickOpModifier() const
{
    int modifier = smNone;

    switch ((*m_pPickers)[RND_GPU_PROG_FR_WHICH_CC].Pick())
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

    switch ((*m_pPickers)[RND_GPU_PROG_FR_SATURATE].Pick())
    {
        case RND_GPU_PROG_OP_SAT_unsigned:  //clamp value to [0,1]
            modifier |= GpuPgmGen::smSS0;
            break;
        case RND_GPU_PROG_OP_SAT_signed:    // clamp value to [-1,1]
            modifier |= GpuPgmGen::smSS1;
            break;
        case RND_GPU_PROG_OP_SAT_none:
        default:
            break;
    }
    return modifier;
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::GetIndexReg(IndexRegType t) const
{
    switch (t)
    {
        default:        MASSERT(!"Unexpected IndexRegType");
        // fall through

        case tmpA128:   return RND_GPU_PROG_FR_REG_A128;
        case tmpA3:     return RND_GPU_PROG_FR_REG_A3;
        case tmpA1:     return RND_GPU_PROG_FR_REG_A1;
        case tmpASubs:  return RND_GPU_PROG_FR_REG_ASubs;
    }
}
//------------------------------------------------------------------------------
bool FrGpuPgmGen::IsIndexReg(int regId) const
{
    switch (regId)
    {
        case RND_GPU_PROG_FR_REG_A128:
        case RND_GPU_PROG_FR_REG_A3:
        case RND_GPU_PROG_FR_REG_A1:
        case RND_GPU_PROG_FR_REG_ASubs:
            return true;

        default:
            return false;
    }
}
//------------------------------------------------------------------------------
int FrGpuPgmGen::GetFirstTempReg() const
{
    return RND_GPU_PROG_FR_REG_R0;
}
//------------------------------------------------------------------------------
int FrGpuPgmGen::GetGarbageReg() const
{
    return RND_GPU_PROG_FR_REG_Garbage;
}
//------------------------------------------------------------------------------
int FrGpuPgmGen::GetLiteralReg() const
{
    return RND_GPU_PROG_FR_REG_Literal;
}
//------------------------------------------------------------------------------
int FrGpuPgmGen::GetFirstTxCdReg() const
{
    return RND_GPU_PROG_FR_REG_fTEX0;
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::GetLastTxCdReg() const
{
    return RND_GPU_PROG_FR_REG_fTEX7;
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::GetNumTempRegs() const
{
    return RND_GPU_PROG_FR_REG_R15 - RND_GPU_PROG_FR_REG_R0 + 1;
}
//------------------------------------------------------------------------------
int FrGpuPgmGen::GetMinOps() const
{
    return m_GS.MainBlock.NumRqdUnwritten();
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::GetMaxOps() const
{
    GLint maxOps;
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
                      GL_MAX_PROGRAM_INSTRUCTIONS_ARB,
                      &maxOps);
    if (mglGetRC() != OK)
        return 0;
    return maxOps;
}

//------------------------------------------------------------------------------
// return a texture coordinate for this register or <0 on error.
int FrGpuPgmGen::GetTxCd(int reg) const
{
    if ( reg >= RND_GPU_PROG_FR_REG_fTEX0 && reg <= RND_GPU_PROG_FR_REG_fTEX7)
        return(reg-RND_GPU_PROG_FR_REG_fTEX0);
    else
        return -1;
}

//------------------------------------------------------------------------------
string FrGpuPgmGen::GetRegName(int reg) const
{
    if (RND_GPU_PROG_FR_REG_oCOL0 == reg && m_ARB_draw_buffers)
        return "result.color[0]";

    if ( reg >= 0 && reg < int(NUMELEMS(RegNames)))
        return RegNames[reg];
    else if ( reg == RND_GPU_PROG_REG_none)
        return "";
    else
        return "Unknown";
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::GetConstReg() const
{
    return RND_GPU_PROG_FR_REG_Const;
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::GetPaboReg() const
{
    return RND_GPU_PROG_FR_REG_PaboElw;
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::GetSubsReg() const
{
    return RND_GPU_PROG_FR_REG_Subs;
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::GetLocalElwReg() const
{
    return RND_GPU_PROG_FR_REG_LocalElw;
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::GetConstVectReg() const
{
    return RND_GPU_PROG_FR_REG_ConstVect;
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::PickPgmTemplate() const
{
    // Pick a flow-control and randomization theme.
    UINT32 t = (*m_pPickers)[RND_GPU_PROG_FR_TEMPLATE].Pick();
    if (t >= RND_GPU_PROG_TEMPLATE_NUM_TEMPLATES)
    {
        t = RND_GPU_PROG_TEMPLATE_Simple;
    }

    if (UsingMrtFloorsweepStrategy() &&
        (t == RND_GPU_PROG_TEMPLATE_CallIndexed))
    {
        // The mrt_fs_cali end strategy uses CALI also, so we can't have the
        // body of the random shader use it as well.
        t = RND_GPU_PROG_TEMPLATE_Simple;
    }

    return(t);
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::PickNumOps() const
{
    return(*m_pPickers)[RND_GPU_PROG_FR_NUM_OPS].Pick();
}

int FrGpuPgmGen::PickCCtoRead() const
{
    int cc = (*m_pPickers)[RND_GPU_PROG_FR_WHICH_CC].Pick();
    if (m_GS.Blocks.top()->CCValidMask() & (0xf << (cc*4)))
        return cc;
    else
        return RND_GPU_PROG_CCnone;
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::PickCCTest(PgmInstruction *pOp) const
{
    // Special processing for the depth value.
    // If we are binding to the result.depth, then we must insure that we
    // write the register, otherwise the output will be undefined.
    // ie. we can't have multiple writes to result.depth with all of them
    // having a <ccTest> that yeilds a false condition.

    if (pOp->OutReg.Reg == RND_GPU_PROG_FR_REG_oDEP)
    {
        const RegState & v = GetVar(pOp->OutReg.Reg);
        if (!(v.Attrs & regRqd) &&     // not a required reg
            !v.WrittenMask             // not written
           )
        {
            return RND_GPU_PROG_OUT_TEST_none;
        }
    }
    int ccTest = (*m_pPickers)[RND_GPU_PROG_FR_OUT_TEST].Pick();
    if (m_BnfExt < extGpu4 && ccTest != RND_GPU_PROG_OUT_TEST_none)
    {
        // only EQ - FL supported
        ccTest = ccTest % RND_GPU_PROG_OUT_TEST_nan;
    }
    return ccTest;
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::PickSwizzleSuffix() const
{
    return (*m_pPickers)[RND_GPU_PROG_FR_SWIZZLE_SUFFIX].Pick();
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::PickSwizzleOrder() const
{
    return(*m_pPickers)[RND_GPU_PROG_FR_SWIZZLE_ORDER].Pick();
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::PickWriteMask() const
{
    return(*m_pPickers)[RND_GPU_PROG_FR_OUT_MASK].Pick();
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::PickNeg() const
{
    return (*m_pPickers)[RND_GPU_PROG_FR_IN_NEG].Pick();
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::PickAbs()const
{
    return (*m_pPickers)[RND_GPU_PROG_FR_IN_ABS].Pick();
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::PickRelAddr() const
{
    return (*m_pPickers)[RND_GPU_PROG_FR_RELADDR].Pick();
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::PickResultReg() const
{
    int reg = (*m_pPickers)[RND_GPU_PROG_FR_OUT_REG].Pick();
    if (reg > RND_GPU_PROG_FR_REG_oCOL0 &&
        reg <= RND_GPU_PROG_FR_REG_oCOL15)
    {
        if (!m_ARB_draw_buffers)
        {
            reg = RND_GPU_PROG_FR_REG_oCOL0;
        }
        else
        {
            reg = RND_GPU_PROG_FR_REG_oCOL0 +
            (reg - RND_GPU_PROG_FR_REG_oCOL0) % m_pGLRandom->NumColorSurfaces();
        }
    }
    return reg;
}

int FrGpuPgmGen::PickMultisample() const    //!< Use explicit_multisample extension?
{
    int value = (*m_pPickers)[RND_GPU_PROG_FR_MULTISAMPLE].Pick();
    if (!m_pGLRandom->HasExt(GLRandomTest::ExtLW_explicit_multisample))
        return 0;
    else
        return value;
}

//------------------------------------------------------------------------------
// Scoring:
//
// Callwlate the "interestingness" score for a given input register,
// based on the swizzle, the register type, and our rough knowlege of
// what has been stored in it so far (if it is a tmp reg).
//
// In VpgsPickOp (), fragment program instruction inputs are picked
// randomly, with probability proportionate to the returned values
// from this function.
//
// Of course, this entire scheme is a crude approximation.  To really do
// things right, we would have an actual simulator running here, and track
// actual opcode results, etc.
//
UINT32 FrGpuPgmGen::CalcScore(PgmInstruction *pOp, int reg, int argIdx)
{
    const UINT32 Score = GpuPgmGen::CalcScore(pOp, reg, argIdx);

    if ((reg >= RND_GPU_PROG_FR_REG_fTEX0) &&
        (reg <= RND_GPU_PROG_FR_REG_fTEX7) &&
        pOp->GetIsTx() &&
        (pOp->LwrInReg == 0))
    {
        // Boost the score for tx coord inputs for tx fetches.
        return Score * 2;
    }

    return Score;
}

//------------------------------------------------------------------------------
void FrGpuPgmGen::GenerateFixedPrograms
(
    Programs * pProgramStorage
)
{
    InitPgmElw();
    PgmAppend("!!ARBfp1.0\n"
              "OPTION LW_fragment_program2;\n"
              "MOV result.color, fragment.color;\n"
              "END\n");
    m_GS.Prog.PgmRqmt.Inputs[ppPriColor] = 0xf;

    pProgramStorage->AddProg(psFixed, m_GS.Prog);
}

//------------------------------------------------------------------------------
void FrGpuPgmGen::GenerateRandomProgram
(
    PgmRequirements *pRqmts,
    Programs * pProgramStorage,
    const string & pgmName
)
{
    // Shall we write separate result.color[n] for multiple color buffers?
    m_ARB_draw_buffers = ((*m_pPickers)[RND_GPU_PROG_FR_DRAW_BUFFERS].Pick() != 0);

    if (!(m_pGLRandom->HasExt(GLRandomTest::ExtARB_draw_buffers) &&
            m_pGLRandom->NumColorSurfaces() > 1))
    {
        m_ARB_draw_buffers = false;
    }

    m_EndStrategy = (*m_pPickers)[RND_GPU_PROG_FR_END_STRATEGY].Pick();

    switch (m_EndStrategy)
    {
        case RND_GPU_PROG_FR_END_STRATEGY_coarse:
            if (!m_pGLRandom->HasExt(GLRandomTest::ExtGL_LW_shading_rate_image))
            {
                m_EndStrategy = RND_GPU_PROG_FR_END_STRATEGY_frc;
            }
            break;
        case RND_GPU_PROG_FR_END_STRATEGY_per_vtx_attr:
            if (!m_pGLRandom->HasExt(GLRandomTest::ExtLW_fragment_shader_barycentric))
            {
                m_EndStrategy = RND_GPU_PROG_FR_END_STRATEGY_frc;
            }
            break;
        case RND_GPU_PROG_FR_END_STRATEGY_mod:
        case RND_GPU_PROG_FR_END_STRATEGY_mrt_fs_pred:
        case RND_GPU_PROG_FR_END_STRATEGY_mrt_fs_cali:
            if (m_BnfExt >= extGpu4) // MOD is not supported
            {
                break;
            }
            // else fall through because:
            //  MOD, fragment.smid, & CALI are not supported.
        default:
            m_EndStrategy = RND_GPU_PROG_FR_END_STRATEGY_frc;
            // fall through
        case RND_GPU_PROG_FR_END_STRATEGY_frc:
        case RND_GPU_PROG_FR_END_STRATEGY_mov:
            break;
    }

    InitPgmElw();
    m_GS.Prog.Name = pgmName;

    if (m_pGLRandom->HasExt(extGpu5))
    {
        glGetFloatv (GL_MIN_FRAGMENT_INTERPOLATION_OFFSET_LW, &m_MinIplOffset);
        glGetFloatv (GL_MAX_FRAGMENT_INTERPOLATION_OFFSET_LW, &m_MaxIplOffset);
    }

    ProgramSequence(m_PgmHeader.c_str());

    const bool TwoSided =
                (0 != (*m_pPickers)[RND_GPU_PROG_VX_TWOSIDE].Pick());
    const bool PointSize =
            (0 != (*m_pPickers)[RND_GPU_PROG_VX_POINTSZ].Pick());

    PgmRequirements * pReqs = &(m_GS.Prog.PgmRqmt);

    if (TwoSided)
    {
        // Two-sided fragment programs that read primary or secondary color
        // implicily also read the back-facing version of same.

        if (pReqs->Inputs.count(ppPriColor))
            pReqs->Inputs[ppBkPriColor] = pReqs->Inputs[ppPriColor];
        if (pReqs->Inputs.count(ppSecColor))
            pReqs->Inputs[ppBkSecColor] = pReqs->Inputs[ppSecColor];
    }

    if (PointSize && !UsingMrtFloorsweepStrategy())
    {
        // Point-size enabled fragment programs implicitly read point-size.
        pReqs->Inputs[ppPtSize] = omX;
    }

    // VTG programs must write layer/viewport if multiple layers are present
    if (m_pGLRandom->GetNumLayersInFBO() > 0)
    {
        pReqs->Inputs[ppLayer] = omX;
        pReqs->Inputs[ppViewport] = omX;
    }

    pProgramStorage->AddProg(psRandom, m_GS.Prog);
}

//------------------------------------------------------------------------------
void FrGpuPgmGen::OptionSequence()
{
    if (m_BnfExt == GLRandomTest::ExtLW_fragment_program2)
    {
        PgmOptionAppend("OPTION LW_fragment_program2;\n");
    }
    else if (m_BnfExt == GLRandomTest::ExtLW_fragment_program)
    {
        PgmOptionAppend("OPTION LW_fragment_program;\n");
    }

    if (m_ARB_draw_buffers)
    {
        PgmOptionAppend("OPTION ARB_draw_buffers;\n");
    }

    if (m_pGLRandom->GetNumLayersInFBO() > 0)
    {
        PgmOptionAppend("OPTION ARB_fragment_layer_viewport;\n");
    }
    if (m_GS.Prog.UsingGLFeature[Program::ConservativeRaster])
    {
        PgmOptionAppend("OPTION LW_conservative_raster_underestimation;\n");
    }

    // LW_internal is required for barycentrics & coarseShading because OGL team is not
    // publishing the GLASM portion of the extension
    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_fragment_shader_barycentric) ||
        m_pGLRandom->HasExt(GLRandomTest::ExtGL_LW_shading_rate_image))
    {
        PgmOptionAppend("OPTION LW_internal;\n");
    }

    GpuPgmGen::OptionSequence();
}

//------------------------------------------------------------------------------
void FrGpuPgmGen::DeclSequence()
{
    const bool disableCentroid = ((*m_pPickers)[RND_GPU_PROG_FR_DISABLE_CENTROID_SAMPLING].Pick() != 0);
    if (disableCentroid)
    {
        // GL treats pri/sec colors as centroid-interpolated by default.
        // GF100 has a HW bug when doing derivative operations (such as perspective
        // correct texture fetches) on a centroid-interpolated attribute, the
        // derivatives can be nondeterministic in 8+24VCAA mode.
        //
        // Force colors to use FLAT interpolation. Note: fragment.color.primary/secondary
        // will inherit these properties. So we don't actually have to reference these
        // variables in the code, just declare them.
        PgmAppend(" FLAT ATTRIB bug618730pri = fragment.color.primary;\n");
        PgmAppend(" FLAT ATTRIB bug618730sec = fragment.color.secondary;\n");
    }
    if (m_GS.Prog.UsingGLFeature[Program::Multisample] && m_pGLRandom->IsFsaa())
    {
        // TXFMS given an out-of-range W coord (sample number) will silently
        // fetch past the edge of the multisample texture renderbuffer and miscompare.
        // We treat TXFMS coords as pass-thru and limit the inputs to be in-range.
        //
        // But when multisampling, the fragment program can be working with a pixel
        // center address *outside* the parent triangle, meaning the W coord can be
        // extrapolated rather than interpolated.
        // This can produce out-of-range W coords and miscompares.
        //
        // So TXFMS coords in an FSAA glrandom test must be declared CENTROID
        // meaning they are clamped to the nearest edge of the triangle.
        //
        // The fragment.texcoord[] attribs will inherit the CENTROID interpolation
        // type from this attrib declaration, we don't have to use this declared name.

        for (int vi = GetFirstTxCdReg(); vi <= GetLastTxCdReg(); ++vi)
        {
            const RegState & v = GetVar(vi);
            if (v.Attrs & regCentroid)
            {
                PgmAppend(Utility::StrPrintf(
                              " CENTROID ATTRIB tc%d = fragment.texcoord[%d];\n",
                              vi - GetFirstTxCdReg(),
                              vi - GetFirstTxCdReg()));
            }
        }
    }
    if (m_EndStrategy == RND_GPU_PROG_FR_END_STRATEGY_mrt_fs_cali)
    {
        PgmAppend(
                "TEMP tmpC;\n"
                "SUBROUTINETYPE tSub { c0,c1,c2,c3,c4,c5,c6,c7 };\n"
                "SUBROUTINE tSub Subs[8] = { program.subroutine[0..7] };\n"
                "c0 SUBROUTINENUM(0):\n"
                " MOV result.color[0], tmpC;\n"
                " MOV result.color[1], 0;\n"
                " MOV result.color[2], 0;\n"
                " MOV result.color[3], 0;\n"
                " MOV result.color[4], 0;\n"
                " MOV result.color[5], 0;\n"
                " MOV result.color[6], 0;\n"
                " MOV result.color[7], 0;\n"
                " RET;\n"
                "c1 SUBROUTINENUM(1):\n"
                " MOV result.color[0], 0;\n"
                " MOV result.color[1], tmpC;\n"
                " MOV result.color[2], 0;\n"
                " MOV result.color[3], 0;\n"
                " MOV result.color[4], 0;\n"
                " MOV result.color[5], 0;\n"
                " MOV result.color[6], 0;\n"
                " MOV result.color[7], 0;\n"
                " RET;\n"
                "c2 SUBROUTINENUM(2):\n"
                " MOV result.color[0], 0;\n"
                " MOV result.color[1], 0;\n"
                " MOV result.color[2], tmpC;\n"
                " MOV result.color[3], 0;\n"
                " MOV result.color[4], 0;\n"
                " MOV result.color[5], 0;\n"
                " MOV result.color[6], 0;\n"
                " MOV result.color[7], 0;\n"
                " RET;\n"
                "c3 SUBROUTINENUM(3):\n"
                " MOV result.color[0], 0;\n"
                " MOV result.color[1], 0;\n"
                " MOV result.color[2], 0;\n"
                " MOV result.color[3], tmpC;\n"
                " MOV result.color[4], 0;\n"
                " MOV result.color[5], 0;\n"
                " MOV result.color[6], 0;\n"
                " MOV result.color[7], 0;\n"
                " RET;\n"
                "c4 SUBROUTINENUM(4):\n"
                " MOV result.color[0], 0;\n"
                " MOV result.color[1], 0;\n"
                " MOV result.color[2], 0;\n"
                " MOV result.color[3], 0;\n"
                " MOV result.color[4], tmpC;\n"
                " MOV result.color[5], 0;\n"
                " MOV result.color[6], 0;\n"
                " MOV result.color[7], 0;\n"
                " RET;\n"
                "c5 SUBROUTINENUM(5):\n"
                " MOV result.color[0], 0;\n"
                " MOV result.color[1], 0;\n"
                " MOV result.color[2], 0;\n"
                " MOV result.color[3], 0;\n"
                " MOV result.color[4], 0;\n"
                " MOV result.color[5], tmpC;\n"
                " MOV result.color[6], 0;\n"
                " MOV result.color[7], 0;\n"
                " RET;\n"
                "c6 SUBROUTINENUM(6):\n"
                " MOV result.color[0], 0;\n"
                " MOV result.color[1], 0;\n"
                " MOV result.color[2], 0;\n"
                " MOV result.color[3], 0;\n"
                " MOV result.color[4], 0;\n"
                " MOV result.color[5], 0;\n"
                " MOV result.color[6], tmpC;\n"
                " MOV result.color[7], 0;\n"
                " RET;\n"
                "c7 SUBROUTINENUM(7):\n"
                " MOV result.color[0], 0;\n"
                " MOV result.color[1], 0;\n"
                " MOV result.color[2], 0;\n"
                " MOV result.color[3], 0;\n"
                " MOV result.color[4], 0;\n"
                " MOV result.color[5], 0;\n"
                " MOV result.color[6], 0;\n"
                " MOV result.color[7], tmpC;\n"
                " RET;\n"
                );

    }
    else if (m_EndStrategy == RND_GPU_PROG_FR_END_STRATEGY_per_vtx_attr)
    {
        PgmAppend("PERVERTEX ATTRIB vtx_attr[] = { vertex.attrib[6..6] };\n");
    }
    GpuPgmGen::DeclSequence();
}

//------------------------------------------------------------------------------
void FrGpuPgmGen::MainSequenceEnd()
{
    // When writing to unsigned integer output color surfaces, ROP will limit
    // all outputs to the [0.0, 1.0] range.
    // Since most intermediate results are as signed floats, it's easy to write
    // results that are all black or all white.
    //
    // Instead, replace our result.color with something less boring:
    //
    //    FRC result.color.xyzw (LE0.xyzw), tmpR?.xyzw;
    //
    // The a input is chosen randomly, but with a different
    // skewing of the "interestingness" scores than in PickOp.
    //
    for (UINT32 mrtIdx = 0; mrtIdx < m_pGLRandom->NumColorSurfaces(); mrtIdx++)
    {
        if (mrtIdx > 0 && !m_ARB_draw_buffers)
        {
            // We are writing only one result.color, which will be copied to all
            // color buffers.
            break;
        }

        const int outReg = RND_GPU_PROG_FR_REG_oCOL0 + mrtIdx;
        const RegState & vOut = GetVar(outReg);

        // Pick a register to copy to ouput.

        const int numVars = m_GS.MainBlock.NumVars();
        m_Scores.resize(numVars);

        GLuint TotalScore = 0;

        int inReg;
        for (inReg = 0; inReg < numVars; inReg++)
        {
            const RegState & v = GetVar(inReg);

            GLuint score = 0;
            if ((v.WrittenMask & v.ReadableMask) == compXYZW)
            {
                for (int ci = 0; ci < 4; ci++)
                    score += v.Score[ci];
            }
            if (v.Attrs & regLiteral)
                score = 0;

            // watch for wraparound bug
            MASSERT(TotalScore <= (TotalScore + score));

            TotalScore += score;
            m_Scores[ inReg ] = TotalScore;
        }

        GLuint pick = m_pFpCtx->Rand.GetRandom(0, TotalScore-1);
        for (inReg = 0; inReg < numVars; inReg++)
        {
            if (pick < m_Scores[inReg])
            {
                break; // inReg is the new picked register
            }
        }
        const RegState & vIn = GetVar(inReg);

        // Now we've chosen a var to copy (probably a temp reg).

        // Choose a strategy for mapping the temp reg to the 0<>1 range to
        // help preserve the random data so that we get "interesting" output.

        if (UsingMrtFloorsweepStrategy())
        {
            ApplyMrtFloorsweepStrategy();
            return;
        }
        if (m_EndStrategy == RND_GPU_PROG_FR_END_STRATEGY_coarse)
        {
            ApplyCoarseShadingStrategy();
            return;
        }

        if (ColorUtils::IsFloat(
                mglTexFmt2Color(m_pGLRandom->ColorSurfaceFormat(mrtIdx))))
        {
            m_EndStrategy = RND_GPU_PROG_FR_END_STRATEGY_mov;
        }

        if ((m_EndStrategy == RND_GPU_PROG_FR_END_STRATEGY_mod) &&
            !(vIn.Attrs & regTemp))
        {
            // The MOD,I2F,MUL,MOV strategy assumes a temp input register.
            m_EndStrategy = RND_GPU_PROG_FR_END_STRATEGY_frc;
        }

        if (vIn.ArrayLen && (vIn.Attrs & regTemp))
        {
            // XOR all of tmpArray down to tmpArray[0]:
            //   XOR tmpArray[0], tmpArray[0], tmpArray[1]; // 0 = 0^1
            //   XOR tmpArray[2], tmpArray[2], tmpArray[3]; // 2 = 2^3
            //   XOR tmpArray[0], tmpArray[0], tmpArray[2]; // 0 = (0^1)^(2^3)
            //
            // This is the same #ops as just doing 0^1, 0^2, 0^3, but it might
            // have better scheduling in the hw.
            for (int step = 1; step <= TempArrayLen/2; step *= 2)
            {
                for (int dst = 0; dst < TempArrayLen; dst += 2*step)
                {
                    int src = dst + step;
                    PgmInstruction Op;
                    if (m_EndStrategy == RND_GPU_PROG_FR_END_STRATEGY_mod)
                        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_xor, vIn.Id);
                    else
                        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_add, vIn.Id);
                    UtilOpInReg(&Op, vIn.Id);
                    UtilOpInReg(&Op, vIn.Id);
                    Op.OutReg.IndexOffset = dst;
                    Op.InReg[0].IndexOffset = dst;
                    Op.InReg[1].IndexOffset = src;
                    if (vIn.Bits == 64)
                        Op.dstDT = dtU64;
                    UtilOpCommit(&Op);
                }
            }
        }

        AddPgmComment(EndStrategyToString(m_EndStrategy));
        m_GS.Prog.PgmRqmt.EndStrategy = m_EndStrategy;

        PgmInstruction Op;
        switch (m_EndStrategy)
        {
            default: // fall through

            case RND_GPU_PROG_FR_END_STRATEGY_mov:
                // MOV result.color[n], tmpRn;
                UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mov, outReg);
                UtilOpInReg(&Op, vIn.Id);
                break;

            case RND_GPU_PROG_FR_END_STRATEGY_frc:
                // FRC result.color[n], |tmpRn|;
                UtilOpDst(&Op, RND_GPU_PROG_OPCODE_frc, outReg);
                UtilOpInReg(&Op, vIn.Id);
                Op.InReg[0].Abs = GL_TRUE;
                break;

            case RND_GPU_PROG_FR_END_STRATEGY_mod:
            {
                // MOD.U tmpRn, tmpRn, 257;
                UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mod, vIn.Id);
                Op.dstDT = dtU32;
                UtilOpInReg(&Op, vIn.Id);
                UtilOpInConstU32(&Op, 257);
                UtilOpCommit(&Op);

                // I2F tmpRn, tmpRn;
                UtilOpDst(&Op, RND_GPU_PROG_OPCODE_i2f, vIn.Id);
                UtilOpInReg(&Op, vIn.Id);
                UtilOpCommit(&Op);

                // MUL tmpRn, tmpRn, 1/255;
                UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mul, vIn.Id);
                UtilOpInReg(&Op, vIn.Id);
                UtilOpInConstF32(&Op, float(1.0/255.0));
                UtilOpCommit(&Op);

                // MOV result.color[n], tmpRn;
                UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mov, outReg);
                UtilOpInReg(&Op, vIn.Id);
                break;
            }

            case RND_GPU_PROG_FR_END_STRATEGY_per_vtx_attr:
            {
                // Write the output color by interpolating vIn with the normals
                // of each of the three contributing vertices. This gives us
                // coverage for per-vertex attributes and attribute-forwarding.
                // In HW this uses the Beta-CBE and TRAM.
                AddPgmComment("END_STRATEGY_per_vtx_attr");

                // MOV tmpRX, 0.000
                const int tempReg = PickTempReg();
                UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mov, tempReg);
                UtilOpInConstF32(&Op, 0.0f);
                UtilOpCommit(&Op);

                const int randIdx =
                    vIn.ArrayLen ? m_pFpCtx->Rand.GetRandom(0, vIn.ArrayLen-1) : 0;

                static const char* compStr = "xyz";
                static constexpr int components[] = { compX, compY, compZ };
                for (UINT32 compIdx = 0; compIdx < 3; compIdx++)
                {
                    if (vIn.ReadableMask & components[compIdx])
                    {
                        // MUL.F32 tmpRX.x, vIn.x, vertex[0].attrib[6]
                        // MUL.F32 tmpRX.y, vIn.y, vertex[1].attrib[6]
                        // MUL.F32 tmpRX.z, vIn.z, vertex[2].attrib[6]
                        string mulStr = Utility::StrPrintf(
                            "MUL.F32 %s.%c, %s",
                            GetRegName(tempReg).c_str(),
                            compStr[compIdx],
                            GetRegName(vIn.Id).c_str());

                        // Handle input register array (e.g. pgmElw[123])
                        if (vIn.ArrayLen)
                        {
                            mulStr += Utility::StrPrintf("[%d]", randIdx);
                        }
                        mulStr += Utility::StrPrintf(
                            ".%c, vertex[%d].attrib[6]; # vertex[%d].normal",
                            compStr[compIdx], compIdx, compIdx);
                        m_GS.MainBlock.AddStatement(mulStr);
                    }
                }

                // FRC outReg, |tmpRX|
                UtilOpDst(&Op, RND_GPU_PROG_OPCODE_frc, outReg);
                UtilOpInReg(&Op, tempReg);
                Op.InReg[0].Abs = GL_TRUE;
                UtilOpCommit(&Op);

                // Update registers used and program requirements since we
                // hard-coded some MUL.F32 instructions above
                MarkRegUsed(vIn.Id);
                MarkRegUsed(tempReg);
                MarkRegUsed(RND_GPU_PROG_FR_REG_fNRML);
                PgmRequirements * pReqs = &(m_GS.Prog.PgmRqmt);
                pReqs->Inputs[ppNrml] = omXYZ;
                const auto inputProp = GetProp(vIn.Id);
                if (inputProp != ppIlwalid)
                {
                    pReqs->Inputs[inputProp] = omXYZ;
                }

                // Output color has been written
                continue;
            }
        }

        // OK now we can build the statement

        // If the the output has some already-written components, force our
        // output mask to skip them.
        Op.OutMask      = vOut.UnWrittenMask();

        // If the output is already fully written, use a random output mask and
        // a condition-test to partly overwrite.

        if (Op.OutMask == 0)
        {
            Op.OutMask = compXYZW &
                    (*m_pPickers)[RND_GPU_PROG_FR_OUT_MASK].Pick();
            const int ccToRead = PickCCtoRead();

            if (RND_GPU_PROG_CCnone != ccToRead)
            {
                Op.CCtoRead = ccToRead;
                Op.OutTest = RND_GPU_PROG_OUT_TEST_ge;

                // update the output mask to be compatable with the ccMask
                int validCCMask = m_GS.Blocks.top()->CCValidMask();
                if (Op.CCtoRead == RND_GPU_PROG_CC0)
                    validCCMask &= 0x0f;
                else
                    validCCMask >>= 4;

                Op.OutMask &= validCCMask;
            }
        }

        if (0 == Op.OutMask)
        {
            // This output already written, and no valid CCmask.
            // Abandon this re-write of it (don't commit the op).
        }
        else
        {
            UtilOpCommit(&Op);
        }
    }

    // Use layer/viewport values to assign color:
    // result.color.z = 1/(layer + viewport/2 + 1)
    //
    // We have to use "z" component so that the compiler is not optimizing out
    // previous "x" and/or "y" assignments in a shader like this:
    //  " FRC result.color, |fragment.texcoord[0]|;\n" +
    //  " MAD tmpR0.x, fragment.viewport.x, 0.500, 1.000;\n" +
    //  " ADD tmpR0.x, fragment.layer.x, tmpR0.x;\n" +
    //  " RCP tmpR0.y, tmpR0.x;\n" +
    //  " ADD result.color.x, tmpR0.y, fragment.color.primary.x;\n" +
    // When "x" or "y" is optimized out Point Sprites don't get expected
    // attributes when GL_POINT_SPRITE_R_MODE_LW is set to GL_S or GL_R and
    // amodel generates an exception like this:
    //  "[AModel] [ERROR] CHECK(primitive[i].isElementExistent(attributeOffset+kS)) failed"
    //  "[AModel] [ERROR] Message: 'POINTSPRITE_RMODE_FROM_S, but S is disabled'"
    // This is another oclwrrence of bug 2500992.

    if (m_pGLRandom->GetNumLayersInFBO() > 0)
    {
        const int tempReg1 = PickTempReg();
        MarkRegUsed(tempReg1);

        // MAD tmpR0.x, fragment.viewport.x, 0.5, 1.0;
        PgmInstruction Op;
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mad, tempReg1, compX, dtNA);
        UtilOpInReg(&Op, RND_GPU_PROG_FR_REG_fVPORT, scX);
        UtilOpInConstF32(&Op, 0.5f);
        UtilOpInConstF32(&Op, 1.0f);
        UtilOpCommit(&Op);

        // ADD tmpR0.x, fragment.layer.x, tmpR0.x;
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_add, tempReg1, compX, dtNA);
        UtilOpInReg(&Op, RND_GPU_PROG_FR_REG_fLAYR, scX);
        UtilOpInReg(&Op, tempReg1, scX);
        UtilOpCommit(&Op);

        // RCP tmpR0.y, tmpR0.x;
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_rcp, tempReg1, compY, dtNA);
        UtilOpInReg(&Op, tempReg1, scX);
        UtilOpCommit(&Op);

        // ADD result.color.z, tmpR0.y fragment.color.z;
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_add, RND_GPU_PROG_FR_REG_oCOL0, compZ, dtNA);
        UtilOpInReg(&Op, tempReg1, scY);
        UtilOpInReg(&Op, RND_GPU_PROG_FR_REG_fCOL0, scZ);
        UtilOpCommit(&Op);
    }

    #ifdef CREATE_INCONSISTENT_SHADER
    // create a inconsistent fragment program to test out error paths.
    if(m_GS.Prog.UsingGLFeature[Program::Multisample])
    {
        m_GS.MainBlock.AddStatement("TEMP badVAR;");
        m_GS.MainBlock.AddStatement("MOV badVAR, pgmElw[255];");  // pgmElw[255] will have different values for gputest vs gpugen
        m_GS.MainBlock.AddStatement("FRC result.color.xyzw, badVAR.xyzw;");
    }
    #endif
}

//-------------------------------------------------------------------------------------------------
// Create the instruction sequence to show how pixels are affected when coarse shading is enabled.
void FrGpuPgmGen::ApplyCoarseShadingStrategy()
{
    // The shadingrate.xy attributes correspond to the current shading that was programmed into
    // the ShadingRatePalette. This palette is initialized with values that correspond to 1x1,
    // 1x2, 2x1, 2x2, 4x2, 2x4, 4x4 coverage where .x holds the 1st dimension & .y holds the
    // second. So adding the .x & .y components should yield a value of 2, 3, 4, 6, or 8.
    // Set the fragment color as follows:
    // 8 = RED, 6 = Green, 4 = blue, 3 = white, 2 = cyan, any other value = black
    AddPgmComment(EndStrategyToString(m_EndStrategy));
    m_GS.Prog.PgmRqmt.EndStrategy = m_EndStrategy;
    // Start wth black (default)
    //" MOV result.color, { 0.0, 0.0, 0.0, 1.0 };\n"
    float vals[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    UtilCopyConstVect(RND_GPU_PROG_FR_REG_oCOL0, vals);

    // We need a single temp register to execute this strategy.
    const int numVars = m_GS.MainBlock.NumVars();
    int tmpReg;
    for (tmpReg = 0; tmpReg < numVars; tmpReg++)
    {
        const RegState & v = GetVar(tmpReg);
        if (v.Attrs & regTemp)
            break;
    }

    //" ADD.U tmpR1.xyzw, fragment.shadingrate.xxxx, fragment.shadingrate.yyyy;\n"
    PgmInstruction Op;
    UtilOpDst(&Op, RND_GPU_PROG_OPCODE_add, tmpReg);
    Op.dstDT = dtU32;
    UtilOpInReg(&Op, RND_GPU_PROG_RF_REG_fSHADINGRATE, scX, scX, scX, scX);
    UtilOpInReg(&Op, RND_GPU_PROG_RF_REG_fSHADINGRATE, scY, scY, scY, scY);
    UtilOpCommit(&Op, "combine .xy to get overall shading rate");

    //" SEQ.U.CC0 tmpR1.xyzw, tmpR1, { 8, 6, 4, 3 };\n"
    UtilOpDst(&Op, RND_GPU_PROG_OPCODE_seq, tmpReg, omXYZW, dtU32);
    Op.OpModifier = smCC0;
    UtilOpInReg(&Op, tmpReg);
    UtilOpInConstS32(&Op, 8, 6, 4, 3);
    UtilOpCommit(&Op, "set CC0 flags to correspond to the current shading rate.");

    //" MOV result.color.xyz(NE0.xyzw), { 1.0, 1.0, 1.0, 1.0 };\n"
    UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mov, RND_GPU_PROG_FR_REG_oCOL0, omXYZ, dtNone);
    UtilOpInConstF32(&Op, 1.0, 1.0, 1.0, 1.0);
    Op.CCtoRead = RND_GPU_PROG_CC0;
    Op.OutTest = RND_GPU_PROG_OUT_TEST_ne;
    Op.CCSwizCount = 4; //note CCSwiz[] is initialized to xyzw
    UtilOpCommit(&Op, "red = 4x4, green = 4x2 or 2x4, blue = 2x2");

    // Set the pixel to white if using 1x2 or 2x1 coverage
    //" MOV result.color.xyz(NE0.wwww), { 1.0, 1.0, 1.0, 1.0 };\n"
    UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mov, RND_GPU_PROG_FR_REG_oCOL0, omXYZ, dtNone);
    Op.CCtoRead = RND_GPU_PROG_CC0;
    Op.OutTest = RND_GPU_PROG_OUT_TEST_ne;
    Op.CCSwizCount = 4;
    Op.CCSwiz[0] = scW;
    Op.CCSwiz[1] = scW;
    Op.CCSwiz[2] = scW;
    Op.CCSwiz[3] = scW;
    UtilOpInConstF32(&Op, 1.0, 1.0, 1.0, 1.0);
    UtilOpCommit(&Op, "set to white if using 1x2 or 2x1 coverage");

    // Test 1x1 case
    //" SEQ.U.CC0 tmpR1.xyzw, tmpR1, { 2, 0, 0, 0 };\n"+
    UtilOpDst(&Op, RND_GPU_PROG_OPCODE_seq, tmpReg, omXYZW, dtU32);
    Op.OpModifier = smCC0;
    UtilOpInReg(&Op, tmpReg);
    UtilOpInConstS32(&Op, 2, 0, 0, 0);
    UtilOpCommit(&Op, "test for 1x1 coverage");

    // Set to cyan if using 1x1 coverage
    //" MOV result.color.xyzw(NE0.xxxx), { 0.0, 1.0, 1.0, 1.0 };\n"
    UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mov, RND_GPU_PROG_FR_REG_oCOL0, omXYZW, dtNone);
    Op.CCtoRead = RND_GPU_PROG_CC0;
    Op.OutTest = RND_GPU_PROG_OUT_TEST_ne;
    Op.CCSwizCount = 4;
    Op.CCSwiz[0] = scX;
    Op.CCSwiz[1] = scX;
    Op.CCSwiz[2] = scX;
    Op.CCSwiz[3] = scX;
    UtilOpInConstF32(&Op, 0.0, 1.0, 1.0, 1.0);
    UtilOpCommit(&Op, "set to cyan if using 1x1 coverage");

}
//------------------------------------------------------------------------------
void FrGpuPgmGen::ApplyMrtFloorsweepStrategy()
{
    // Pick an input reg with a strong preference for temp regs.
    AddPgmComment(EndStrategyToString(m_EndStrategy));
    m_GS.Prog.PgmRqmt.EndStrategy = m_EndStrategy;

    const int numVars = m_GS.MainBlock.NumVars();
    m_Scores.resize(numVars);

    GLuint TotalScore = 0;

    int inReg;
    for (inReg = 0; inReg < numVars; inReg++)
    {
        const RegState & v = GetVar(inReg);

        GLuint score = 0;
        if ((v.WrittenMask  == compXYZW) &&
            (v.ReadableMask == compXYZW))
        {
            for (int ci = 0; ci < 4; ci++)
                score += v.Score[ci];
            if (v.Attrs & regTemp)
                score *= 2;
            if (v.Attrs & regIndex)
                score = 0;
            if (v.ArrayLen)
                score = 0;
            if (v.Attrs & regLiteral)
                score = 0;
        }

        // watch for wraparound bug
        MASSERT(TotalScore <= (TotalScore + score));

        TotalScore += score;
        m_Scores[ inReg ] = TotalScore;
    }

    GLuint pick = m_pFpCtx->Rand.GetRandom(0, TotalScore-1);
    for (inReg = 0; inReg < numVars; inReg++)
    {
        if (pick < m_Scores[inReg])
        {
            break; // inReg is the new picked register
        }
    }

    // Need two temp regs other than inReg.

    int regScaled;
    int regUnread;
    if (inReg > RND_GPU_PROG_FR_REG_R1)
    {
        regScaled = RND_GPU_PROG_FR_REG_R0;
        regUnread = RND_GPU_PROG_FR_REG_R1;
    }
    else
    {
        regScaled = RND_GPU_PROG_FR_REG_R2;
        regUnread = RND_GPU_PROG_FR_REG_R3;
    }

    PgmInstruction Op;

    // Cast inReg to UINT32, get modulo 257 (prime).
    UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mod, regScaled);
    Op.dstDT = dtU32;
    UtilOpInReg(&Op, inReg);
    UtilOpInConstU32(&Op, 257);
    UtilOpCommit(&Op);

    // Colwert to FLOAT32 holding integer between 0.0 and 256.0.
    UtilOpDst(&Op, RND_GPU_PROG_OPCODE_i2f, regScaled);
    UtilOpInReg(&Op, regScaled);
    UtilOpCommit(&Op);

    // Multiply by 1/256 to scale to the 0.0 to 1.0 range.
    UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mul, regScaled);
    UtilOpInReg(&Op, regScaled);
    UtilOpInConstF32(&Op, 1.0/256.0);
    UtilOpCommit(&Op);

    if (RND_GPU_PROG_FR_END_STRATEGY_mrt_fs_cali == m_EndStrategy)
    {
        m_GS.Prog.SubArrayLen = 8;

        // Copy the scaled reg to "tmpC" as used by the color-writing funcs.
        m_GS.MainBlock.AddStatement(Utility::StrPrintf(
                "MOV tmpC, %s;", GetRegName(regScaled).c_str()));

        // MOV.U tmpA128.x, fragment.smid;
        const int idxReg = GetIndexReg(tmpA128);
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mov, idxReg);
        Op.dstDT = dtU32;
        Op.OutReg.Swiz[siX] = scX;
        Op.OutReg.SwizCount = 1;
        UtilOpInReg(&Op, RND_GPU_PROG_FR_REG_SMID);
        UtilOpCommit(&Op);

        // CALI ColorFuncs[tmpA128.x];
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_cali, idxReg);
        UtilOpInReg(&Op, GetSubsReg());
        Op.InReg[0].IndexReg = idxReg;
        Op.InReg[0].IndexRegComp = 0;
        Op.CCtoRead = RND_GPU_PROG_CCnone;
        Op.OutTest = RND_GPU_PROG_OUT_TEST_none;
        UtilOpCommit(&Op);
    }
    else
    {   // RND_GPU_PROG_FR_END_STRATEGY_mrt_fs_pred

        for (UINT32 surf = 0; surf < m_pGLRandom->NumColorSurfaces(); surf++)
        {
            // If this color-buffer matches the SMID (tpc number), write the
            // "interesting" result.  Else, write a black pixel.

            const int outReg = RND_GPU_PROG_FR_REG_oCOL0 + surf;

            // SUB.U.CC0 rUnused, fragment.smid.x, %d;
            UtilOpDst(&Op, RND_GPU_PROG_OPCODE_sub, regUnread);
            Op.dstDT = dtU32;
            Op.OpModifier |= smCC0;
            Op.CCtoRead = RND_GPU_PROG_CCnone;
            UtilOpInReg(&Op, RND_GPU_PROG_FR_REG_SMID);
            UtilOpInConstU32(&Op, surf);
            UtilOpCommit(&Op);

            // MOV result.color[%d](EQ0.xxxx), rScaled;
            UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mov, outReg);
            Op.CCtoRead = RND_GPU_PROG_CC0;
            Op.OutTest = RND_GPU_PROG_OUT_TEST_eq;
            Op.CCSwizCount = 4;
            Op.CCSwiz[siX] = scX;
            Op.CCSwiz[siY] = scX;
            Op.CCSwiz[siZ] = scX;
            Op.CCSwiz[siW] = scX;
            UtilOpInReg(&Op, regScaled);
            UtilOpCommit(&Op);

            // MOV result.color[%d](NE0.xxxx), {0,0,0,0};
            UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mov, outReg);
            Op.CCtoRead = RND_GPU_PROG_CC0;
            Op.OutTest = RND_GPU_PROG_OUT_TEST_ne;
            Op.CCSwizCount = 4;
            Op.CCSwiz[siX] = scX;
            Op.CCSwiz[siY] = scX;
            Op.CCSwiz[siZ] = scX;
            Op.CCSwiz[siW] = scX;
            UtilOpInConstF32(&Op, 0, 0, 0, 0);
            UtilOpCommit(&Op);
        }
    }

    // Mark the output registers written to avoid assertions.
    for (UINT32 surf = 0; surf < m_pGLRandom->NumColorSurfaces(); surf++)
    {
        const int outReg = RND_GPU_PROG_FR_REG_oCOL0 + surf;
        RegState vOut = GetVar(outReg);
        vOut.Attrs |= regUsed;
        vOut.WrittenMask = 0xf;
        vOut.Score[0] = vOut.Score[1] = vOut.Score[2] = vOut.Score[3] = 1;
        m_GS.Blocks.top()->UpdateVar(vOut);
    }
}

//------------------------------------------------------------------------------
// Process any special fragment shader instruction.
// Return true if CommitInstruction() was already called, false otherwise
bool FrGpuPgmGen::SpecialInstruction(PgmInstruction *pOp)
{
    // grammar spec.
    // <SpecialInstruction> ::= "KILL" <opModifiers> <killCond>
    //                    | "DDX" <opModifiers> <instResult> "," <instOperandV>
    //                    | "DDY" <opModifiers> <instResult> "," <instOperandV>
    //                    | "LOD" <opModifiers> <instResult> "," <instOperandV> "," <texAccess>
    // <killCond>  ::= <instOperandV>

    bool bHandled = true;
    switch ( pOp->OpId )
    {
        case RND_GPU_PROG_FR_OPCODE_kil:
            OpModifiers(pOp);
            InstOperandV(pOp);
            // KIL instr can prevent any further processing of the shader. So
            // mask out everything to prevent us from using anymore of the ATOM/
            // STORE instuctions.
            m_GS.ConstrainedOpsUsed = 0;
            break;

        case RND_GPU_PROG_FR_OPCODE_ipac:
            OpModifiers(pOp);
            InstResult(pOp);
            // We must pick a operand that has a fragment attribute binding.
            InstOperand(pOp,
                        RND_GPU_PROG_FR_REG_fWPOS,    // default
                        RND_GPU_PROG_FR_REG_fWPOS,    // min
                        RND_GPU_PROG_FR_REG_fCLP5);   // max
            break;

        case RND_GPU_PROG_FR_OPCODE_ipao:
            if(m_GS.AllowLiterals)
            {
                OpModifiers(pOp);
                InstResult(pOp);
                // We must pick a operand that has a fragment attribute binding.
                InstOperand(pOp,
                            RND_GPU_PROG_FR_REG_fWPOS,    // default
                            RND_GPU_PROG_FR_REG_fWPOS,    // min
                            RND_GPU_PROG_FR_REG_fCLP5);   // max
                InstLiteral( pOp,
                             GetConstVectReg(),
                             4,
                             m_pFpCtx->Rand.GetRandomFloat(m_MinIplOffset,m_MaxIplOffset),
                             m_pFpCtx->Rand.GetRandomFloat(m_MinIplOffset,m_MaxIplOffset),
                             0.0f,
                             0.0f);

            }
            else
                bHandled = false;
            break;
        case RND_GPU_PROG_FR_OPCODE_ipas:
            if(m_GS.AllowLiterals)
            {
                OpModifiers(pOp);
                InstResult(pOp);
                // We must pick a operand that has a fragment attribute binding.
                InstOperand(pOp,
                            RND_GPU_PROG_FR_REG_fWPOS,    // default
                            RND_GPU_PROG_FR_REG_fWPOS,    // min
                            RND_GPU_PROG_FR_REG_fCLP5);   // max
                InstLiteral( pOp,
                             GetLiteralReg(),
                             1,
                             (INT32)m_pFpCtx->Rand.GetRandom(0,m_pGLRandom->NumColorSamples()),
                             0,
                             0,
                             0);
            }
            else
                bHandled = false;
            break;

        // These ops must use sboAddr which contains the GPU address for the Shader buffer objects
        // We need to guard all instructions that access the sboAddr.xy components against invalid
        // values.
        case RND_GPU_PROG_FR_OPCODE_atom:
        {
            //There are two approaches to using the ATOM instruction, one for 32bit atomics and
            //one for 64 bit atomics. For 32 bit atomics we can pull the atomic mask directly
            //from the pgmElw[], however for 64 bit atomics we need to pack the contents of the
            //pgmElw[] into a local variable and then access that variable. The pgmElw[] has
            //dedicated 32/64 bit masks at different indices. sboAddr.y holds the Gpu addr for 32
            //bit operations, and sbo.Addr.z holds the Gpu addr for 64 bits operations.
            //
            //For 32bit operations we need our instructions to look like this:
            //ATOM.OR.U32 garbage.x, pgmElw[ 18].y, sboAddr.y;
            //
            //For 64 bit atomics we need a few extra instructions to pack the masks:
            //PK64.U atom64.xy, pgmElw[20];
            //PK64.U atom64.zw, pgmElw[21];
            //...
            //ATOM.OR.U64 garbage.x, atom64.y, sboAddr.z;

            // InstResult (w/o a CCMask, see bug 706277)
            pOp->OutReg.Reg = RND_GPU_PROG_FR_REG_Garbage;
            OptWriteMask(pOp);

            // Operand0 (atomicMask)
            // atomic ops can only access constant data setup in pgmElw[18..22]
            // For now default to 32bit implementation, we will do a fixup later.
            UtilOpInReg(pOp, RND_GPU_PROG_FR_REG_Const);

            // Operand1 (Gpu addr)
            // atomic ops can only access a single position specific address in the WriteSbo
            UtilOpInReg(pOp, RND_GPU_PROG_FR_REG_SBO);

            OpModifiers(pOp);
            if (GetDataTypeSize(pOp->dstDT) == 8)
                pOp->InReg[1].Swiz[siX] = scZ; // need the Sbo address for 64bit access
            else
                pOp->InReg[1].Swiz[siX] = scY; // need the Sbo address for 32bit access
            pOp->InReg[1].SwizCount = 1;

            // Its possible that we can't get a compatible atomic op.
            if ((pOp->OpModifier >> 16) != amNone)
            {
                // can't get the InReg[0] specifics until we have the op modifer.
                pOp->InReg[0].Reg = GetAtomicOperand0Reg(pOp);
                if (pOp->InReg[0].Reg == RND_GPU_PROG_FR_REG_Const)
                {
                    pOp->InReg[0].IndexOffset = GetAtomicIndex(pOp);
                }
                pOp->InReg[0].Swiz[siX] = GetAtomicSwiz(pOp);
                pOp->InReg[0].SwizCount = 1;

                // returns true if the SboCalOffset() was called.
                ProcessSboRequirements(pOp);
                m_GS.Prog.PgmRqmt.NeedSbos = AddGuardedSboInst(pOp);
            }

            bHandled = false; // return false because AddGuardedSboInst() will commit the instr.
            break;
        }
        case RND_GPU_PROG_FR_OPCODE_load:
        {
            ProcessSboRequirements(pOp);
            OpModifiers(pOp);
            InstResult(pOp);
            // oper0 must reference sboAddr.x (ReadSbo's GPU address)
            UtilOpInReg(pOp, RND_GPU_PROG_FR_REG_SBO);
            pOp->InReg[0].Swiz[siX] = scX;
            pOp->InReg[0].SwizCount = 1;

            m_GS.Prog.PgmRqmt.NeedSbos = AddGuardedSboInst(pOp);

            bHandled = false;
            break;
        }
        case RND_GPU_PROG_FR_OPCODE_store:
            if( s_OpConstraints[ocStore] & m_GS.ConstrainedOpsUsed)
            {
                m_GS.ConstrainedOpsUsed &= ~(1<<ocNone);
                m_GS.ConstrainedOpsUsed |= (1 << ocStore);

                ProcessSboRequirements(pOp);
                OpModifiers(pOp);   // there is no result with STORE
                // STORE op can only use constant data for the operand.
                UtilOpInReg(pOp, RND_GPU_PROG_FR_REG_Const);
                pOp->InReg[0].IndexOffset = m_pFpCtx->Rand.GetRandom(0, m_ElwSize);
                SwizzleSuffix(pOp,pOp->InReg[0].Swiz,&pOp->InReg[0].SwizCount,compXYZW);

                // oper1 must reference sboAddr.y (WriteSbo's GPU address)
                UtilOpInReg(pOp, RND_GPU_PROG_FR_REG_SBO);
                pOp->InReg[1].Swiz[siX] = scY;
                pOp->InReg[1].SwizCount = 1;

                m_GS.Prog.PgmRqmt.NeedSbos = AddGuardedSboInst(pOp);
            }

            bHandled = false;
            break;

        case RND_GPU_PROG_FR_OPCODE_atomim:
            if (IsBindlessTextures())
                return false;

            // InstResult (w/o a CCMask, see bug 706277)
            pOp->OutReg.Reg = RND_GPU_PROG_FR_REG_Garbage;
            OptWriteMask(pOp);

            // Operand0
            // We have to restrict our use of operand 0 to allow us to use
            // multiple ATOM/ATOMIM instructions in the same shader. So
            // pgmElw[18..22] have usable random data for this.
            UtilOpInReg(pOp, RND_GPU_PROG_FR_REG_Const);

            // We have to get the InstResult & Operand0 before we can determine
            // the storage modifier mask.
            OpModifiers(pOp);
            bHandled = ((pOp->OpModifier >> 16) != amNone);
            if (bHandled)
            {
                // Can't get InReg[0] specifics until we get the atomicOpModifier.
                pOp->InReg[0].IndexOffset = GetAtomicIndex(pOp);
                pOp->InReg[0].Swiz[siX] = GetAtomicSwiz(pOp);
                pOp->InReg[0].SwizCount = 1;
                // Use the picking logic in TexAccess() to get us a fetcher and
                // target for the implied TXQ instruction.
                bHandled = TexAccess(pOp,iuaReadWrite);
            }

            if (bHandled)
            {
                pOp->InReg[1].Reg = RND_GPU_PROG_FR_REG_fWPOS;
                pOp->InReg[1].SwizCount = 0;
                pOp->InReg[1].Neg = false;
                pOp->LwrInReg++;
                bHandled = PickImageUnit(pOp, iuaReadWrite);
            }

            if (bHandled)
            {
                if (CommitInstruction(pOp) == GpuPgmGen::instrValid)
                {
                    UtilScaleRegToImage(pOp);
                }
                else
                    bHandled = false;
            }
            break;

        default:
            return GpuPgmGen::SpecialInstruction(pOp);
    }

    return bHandled;
}

//------------------------------------------------------------------------------
// SboCalOffset() will initialize sboAddr with a valid address if the SBO is
// within range of the fragment's position.xy value. This set of instructions
// will test against a non-zero value in the sboAddr.x or sboAddr.y component
// and execute the new op if non-zero.
bool FrGpuPgmGen::AddGuardedSboInst( PgmInstruction * pOp)
{
    // Use a temporary Block object for command inside the IF statement:
    Block tmpIfBlock(m_GS.Blocks.top());
    m_GS.Blocks.push(&tmpIfBlock);

    const bool bUpdateVarMaybe = true;
    if (CommitInstruction(pOp, bUpdateVarMaybe) == GpuPgmGen::instrValid)
    {
        CommitInstText(pOp);
        m_GS.Blocks.pop();
        switch (pOp->OpId)
        {
            case RND_GPU_PROG_FR_OPCODE_load:
                m_GS.Blocks.top()->AddStatement("IF GT0.x;");
                break;
            case RND_GPU_PROG_FR_OPCODE_store:
            case RND_GPU_PROG_FR_OPCODE_atom:
                m_GS.Blocks.top()->AddStatement("IF GT0.y;");
                break;
            default:
                MASSERT(!"Unknown OpId for AddGuardedSboInst()");
                //Need to finish out the block regardless.
                m_GS.Blocks.top()->AddStatement("IF GT0.y;");
                break;
        }
        m_GS.Blocks.top()->AddStatements(tmpIfBlock);
        m_GS.Blocks.top()->AddStatement("ENDIF;");
        m_GS.Blocks.top()->IncorporateInnerBlock(tmpIfBlock, Block::CalledMaybe);
        return true;
    }
    else
    {
        m_GS.Blocks.pop();
        return false;
    }
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::PickTempReg() const
{
    return( m_pFpCtx->Rand.GetRandom(RND_GPU_PROG_FR_REG_R0,
                                     RND_GPU_PROG_FR_REG_R15));
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::PickTxOffset() const
{
    return (*m_pPickers)[RND_GPU_PROG_FR_TX_OFFSET].Pick();
}

//------------------------------------------------------------------------------
int FrGpuPgmGen::PickTxTarget() const
{
    int target = (*m_pPickers)[RND_GPU_PROG_FR_TX_DIM].Pick();
    return FilterTexTarget(target);
}

//-----------------------------------------------------------------------------
bool FrGpuPgmGen::IsTexTargetValid( int OpId, int TxFetcher, int TxAttr)
{
    // LW_gpu_program4_1 introduced the LOD instr and the ability to access
    // lwbemap arrays. So if this extension is not supported or we are
    // are trying to access a "RECT", "SHADOWRECT", or "RENDERBUFFER" the
    // target is not valid, Otherwise let the base class decide.
    if (OpId == RND_GPU_PROG_FR_OPCODE_lod)
    {
        if (m_pGLRandom->HasExt(extGpu4_1) && (TxAttr & ~Is2dMultisample))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
        return GpuPgmGen::IsTexTargetValid(OpId,TxFetcher,TxAttr);
}

//-----------------------------------------------------------------------------
GLRandom::ProgProperty FrGpuPgmGen::GetProp( int reg) const
{
    MASSERT(reg >= 0 && reg < RND_GPU_PROG_FR_REG_NUM_REGS);
    return (ProgProperty)RegToProp[reg];
}

//-----------------------------------------------------------------------------
// Returns true if the subroutine was actually called
//
// Design Considerations:
// Multiple ilwocations of fragment shaders will get ilwoked for the same texel.
// This usually happens when multisample is enabled or when multiple fragments
// touch the same texel. When this happens and we have a shader that has multiple
// ATOM instructions using different data types accessing the same memory location the
// potential for nondeterminism is increased. Consider a shader with the following
// instructions:
//     "!!LWfp5.0\n"+
//    "PARAM pgmElw[256] = { program.elw[0..255] };\n"+
//    "LONG TEMP sboAddr;\n"+
//    "LONG TEMP garbage;\n"+
//     ...
//        "Sub1:\n"+
//        "  CAL SboCalOffset;\n"+
//        "  IF GT0.y;\n"+
//        "   ATOM.ADD.U64 garbage.x, pgmElw[19].x, sboAddr.y;\n"+
//        "  ENDIF;\n"+
//        "  RET;\n"+
//        "main:\n"+
//        " CAL Sub1;\n"+
//        " SUB.U64.CC0 garbage.xy, sboAddr, {0,0,0,0};\n"+
//        " IF GT0.y;\n"+
//        "  ATOM.ADD.U32 garbage.x, pgmElw[ 19].x, sboAddr.y;\n"+
//        " ENDIF;\n"+
//        " FRC result.color, |fragment.texcoord[7]|;\n"+
//        "END\n"
// This shader has 2 ATOM.ADD instrs, one that accesses a 64bits and one that accesses
// 32 bits from the same memory location. If the following conditions are:
// - pgmElw[19].x = 0x1fb272cd
// - Initial contents of memory location at sbo.Addr.y = 0x80000000 0x80000000
// Then 3 threads ilwoking this shader running synchronously (ie. in different warps)
//   we could have the following sequence:      LSDW        MSDW
// tid 0: ATOM.ADD.U64 0x80000000 0x80000000 => 0x9fb272cd 0x80000000
// tid 0: ATOM.ADD.U32 0x9fb272cd 0x80000000 => 0xbf645e9a 0x80000000
// tid 1: ATOM.ADD.U64 0xbf645e9a 0x80000000 => 0xdf175867 0x80000000
// tid 1: ATOM.ADD.U32 0x9fb272cd 0x80000000 => 0xfec9cb34 0x80000000
// tid 2: ATOM.ADD.U64 0x80000000 0x80000000 => 0x1e7c3e01 0x80000001 <<--- carry applied
// tid 2: ATOM.ADD.U32 0x9fb272cd 0x80000000 => 0x3e2eb0ce 0x80000001
// - If the threads run simultaneously (ie in the same warp) we could have the
//   following sequence (32bit data wraps and we loose the carry).
// tid 0: ATOM.ADD.U64 0x80000000 0x80000000 => 0x9fb272cd 0x80000000
// tid 1: ATOM.ADD.U64 0x9fb272cd 0x80000000 => 0xbf645e9a 0x80000000
// tid 2: ATOM.ADD.U64 0xbf645e9a 0x80000000 => 0xdf175867 0x80000000
// tid 0: ATOM.ADD.U32 0x9fb272cd 0x80000000 => 0xfec9cb34 0x80000000
// tid 1: ATOM.ADD.U32 0x80000000 0x80000000 => 0x1e7c3e01 0x80000000 <<--- missing the carry
// tid 2: ATOM.ADD.U32 0x9fb272cd 0x80000000 => 0x3e2eb0ce 0x80000000 <<--- missing the carry
// This would most certainly cause a miscompare
// So we have to insure that 32 & 64 bits data access reference differnt
// memory locations in the SBO.
//-----------------------------------------------------------------------------
void FrGpuPgmGen::ProcessSboRequirements(PgmInstruction * pOp)
{
    if ( m_SboSubIdx < 0 )
    {
        // This subroutine will callwlate the sbo gpu address's based on the size
        // of the SBO and the fragment.position. If the r/w SBOs fall within the
        // fragment.position, sboAddr will be updated with a position specific gpu
        // address, if not sboAddr will be filled with zeros.
        // sboAddr.x = read-only SBO (for LOAD )
        // sboAddr.y = read-write SBO (for STORE, ATOM.?? 32bit access)
        // sboAddr.z = read-write SBO (for ATOM.?? 64bit access)
        // sboAddr.w = undefined
        // Note: Each element is 256 bits to support all possible data formats,ie F64X4
        m_SboSubIdx = (int)m_GS.Subroutines.size();
        Subroutine * pSub = new Subroutine(&m_GS.MainBlock, "SboCalOffset");
        pSub->m_Block.AddStatement("TEMP pos;");
        pSub->m_Block.AddStatement("TEMP offset;");
        pSub->m_Block.AddStatement(Utility::StrPrintf(
                                       "PARAM sboRwAddrs = program.local[%d];",
                                       SboRwAddrs));
        pSub->m_Block.AddStatement(Utility::StrPrintf(
                                        "PK64.U sboAddr.xy, sboRwAddrs;  "
                                        "#copy SBO gpu addr from local elw."));
        //CAUTION: The insertion of this instruction relies on the s_OpConstraints[]. We will
        // populate the atom64.xy fields with the appropriate masks base on the atomic modifier.
        if ((pOp->OpId == RND_GPU_PROG_FR_OPCODE_atom) &&
            (pOp->dstDT & dt64))
        {
            switch (pOp->OpModifier >> 16)
            {
                case amAnd:
                case amOr:
                case amXor:
                case amMin:
                case amMax:
                {
                    pSub->m_Block.AddStatement(Utility::StrPrintf(
                        "PK64.U atom64.xy, pgmElw[%d];  #copy 64bit And,Or mask to local var.",AtomicMask3));
                    pSub->m_Block.AddStatement(Utility::StrPrintf(
                        "PK64.U atom64.zw, pgmElw[%d];  #copy 64bit Xor, Min/Max mask to local var.",AtomicMask4));
                    RegState v = GetVar(RND_GPU_PROG_FR_REG_Atom64);
                    v.Attrs |= regUsed;
                    v.WrittenMask |= compXYZW;
                    m_GS.Blocks.top()->UpdateVar(v);
                    break;
                }

                case amAdd:
                case amExch:
                case amCSwap:
                {
                    pSub->m_Block.AddStatement(Utility::StrPrintf(
                        "PK64.U atom64.xy, pgmElw[%d];  "
                        "#copy 64bit Add,Exch/CSwap, mask to local var.",
                        AtomicMask5));
                    RegState v = GetVar(RND_GPU_PROG_FR_REG_Atom64);
                    v.Attrs |= regUsed;
                    v.WrittenMask |= compXY;
                    m_GS.Blocks.top()->UpdateVar(v);
                    break;
                }

                case amIwrap: //not supported with 64 bit instr.
                case amDwrap: //not supported with 64 bit instr.
                case amNone:
                    break;
            }
        }
        pSub->m_Block.AddStatement("CVT.U32.F32.FLR pos, fragment.position;  #colwert fragment position to integer");
        //Note: We write to .zw to insure the CC0 components are valid is all cases, specifically
        // the passing case.
        pSub->m_Block.AddStatement(Utility::StrPrintf("SUB.S.CC0 offset.xyzw, pos.xyxy, pgmElw[%d].zwzw; #determine if this fragment lies withing the SBO window", SboWindowRect)); //$
        pSub->m_Block.AddStatement("IF GE0.x;");
        pSub->m_Block.AddStatement(" IF GE0.y;");
        pSub->m_Block.AddStatement(Utility::StrPrintf("  SUB.S.CC0 garbage.xy, pgmElw[%d].xyxy,offset.xyxy;",SboWindowRect));
        pSub->m_Block.AddStatement("  IF GT0.x;");
        pSub->m_Block.AddStatement("   IF GT0.y;");
        pSub->m_Block.AddStatement(Utility::StrPrintf("    MAD.U32 offset.w, pgmElw[%d].x, offset.y, offset.x;",SboWindowRect));
        pSub->m_Block.AddStatement("    MUL.U32 offset.w, offset.w, {32,32,32,32};");
        pSub->m_Block.AddStatement("    ADD.U64.CC0 sboAddr.xy, sboAddr, offset.w;   #offset for ATOM 32bit access");
        pSub->m_Block.AddStatement("    ADD.U64 sboAddr.z, sboAddr.y, {8,8,8,8};     #offset for ATOM 64bit access");
        pSub->m_Block.AddStatement("    RET;");
        pSub->m_Block.AddStatement("   ENDIF;");
        pSub->m_Block.AddStatement("  ENDIF;");
        pSub->m_Block.AddStatement(" ENDIF;");
        pSub->m_Block.AddStatement("ENDIF;");
        pSub->m_Block.AddStatement("MOV.U64.CC0 sboAddr, {0,0,0,0};");
        pSub->m_Block.AddStatement("RET;");
        m_GS.Subroutines.push_back(pSub);

        // Update the global sboAttr & garbage register's requirements
        RegState v = GetVar(RND_GPU_PROG_FR_REG_SBO);
        v.Attrs |= regUsed;
        v.WrittenMask |= compXYZW;
        m_GS.Blocks.top()->UpdateVar(v);

        v = GetVar(RND_GPU_PROG_FR_REG_Garbage);
        v.Attrs |= regUsed;
        v.WrittenMask |= compXYZW;
        m_GS.Blocks.top()->UpdateVar(v);

        //After issuing all of the above instructions we can guarentee that all components
        //of CC0's mask are valid
        m_GS.Blocks.top()->UpdateCCValidMask(0x0f);

        // Call the subroutine to initialize the sboAddr register
        UtilBranchOp( m_SboSubIdx);
    }
    else
    {
        m_GS.Blocks.top()->AddStatement("SUB.U64.CC0 garbage.xy, sboAddr, {0,0,0,0};");
    }
}

bool FrGpuPgmGen::UsingMrtFloorsweepStrategy() const
{
    if ((RND_GPU_PROG_FR_END_STRATEGY_mrt_fs_pred == m_EndStrategy) ||
        (RND_GPU_PROG_FR_END_STRATEGY_mrt_fs_cali == m_EndStrategy))
    {
        return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
// Return the correct scaling method for the current texture coordinate register.
// Called by GpuPgmGen during UtilScaleRegToImage().
int FrGpuPgmGen::GetTxCdScalingMethod(PgmInstruction *pOp)
{
    int sm = smLinear;
    switch (pOp->OpId)
    {
        case RND_GPU_PROG_OPCODE_loadim:
            if (pOp->InReg[0].Reg == RND_GPU_PROG_FR_REG_fWPOS)
                sm = smWrap;
            break;
        case RND_GPU_PROG_OPCODE_storeim:
        case RND_GPU_PROG_FR_OPCODE_atomim:
            if (pOp->InReg[1].Reg == RND_GPU_PROG_FR_REG_fWPOS)
                sm = smWrap;
            break;
        default:
            break;
    }

    return sm;
}

