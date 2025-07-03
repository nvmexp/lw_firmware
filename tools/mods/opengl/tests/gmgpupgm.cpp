/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007,2009-2014, 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// FragmentGpuProgramGrammar4_0
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

const static char * RegNames[RND_GPU_PROG_GM_REG_NUM_REGS] =
{//   LW name
      "vertex[%d].position"                 //RND_GPU_PROG_GM_REG_vOPOS
    , "vertex[%d].color.primary"            //RND_GPU_PROG_GM_REG_vCOL0
    , "vertex[%d].color.secondary"          //RND_GPU_PROG_GM_REG_vCOL1
    , "vertex[%d].color.back.primary"       //RND_GPU_PROG_GM_REG_vBFC0
    , "vertex[%d].color.back.secondary"     //RND_GPU_PROG_GM_REG_vBFC1
    , "vertex[%d].fogcoord"                 //RND_GPU_PROG_GM_REG_vFOGC
    , "vertex[%d].pointsize"                //RND_GPU_PROG_GM_REG_vPSIZ
    , "vertex[%d].texcoord[0]"              //RND_GPU_PROG_GM_REG_vTEX0
    , "vertex[%d].texcoord[1]"              //RND_GPU_PROG_GM_REG_vTEX1
    , "vertex[%d].texcoord[2]"              //RND_GPU_PROG_GM_REG_vTEX2
    , "vertex[%d].texcoord[3]"              //RND_GPU_PROG_GM_REG_vTEX3
    , "vertex[%d].texcoord[4]"              //RND_GPU_PROG_GM_REG_vTEX4
    , "vertex[%d].texcoord[5]"              //RND_GPU_PROG_GM_REG_vTEX5
    , "vertex[%d].texcoord[6]"              //RND_GPU_PROG_GM_REG_vTEX6
    , "vertex[%d].texcoord[7]"              //RND_GPU_PROG_GM_REG_vTEX7
    , "vertex[%d].attrib[6]"                //RND_GPU_PROG_GM_REG_vNRML
    , "vertex[%d].clip[0]"                  //RND_GPU_PROG_GM_REG_vCLP0
    , "vertex[%d].clip[1]"                  //RND_GPU_PROG_GM_REG_vCLP1
    , "vertex[%d].clip[2]"                  //RND_GPU_PROG_GM_REG_vCLP2
    , "vertex[%d].clip[3]"                  //RND_GPU_PROG_GM_REG_vCLP3
    , "vertex[%d].clip[4]"                  //RND_GPU_PROG_GM_REG_vCLP4
    , "vertex[%d].clip[5]"                  //RND_GPU_PROG_GM_REG_vCLP5
    , "vertex[%d].id"                       //RND_GPU_PROG_GM_REG_vID
    , "primitive.id"                        //RND_GPU_PROG_GM_REG_pID
    , "primitive.invocation"                //RND_GPU_PROG_GM_REG_pILWO
    , "result.position"                     //RND_GPU_PROG_GM_REG_oHPOS
    , "result.color.primary"                //RND_GPU_PROG_GM_REG_oCOL0
    , "result.color.secondary"              //RND_GPU_PROG_GM_REG_oCOL1
    , "result.color.back.primary"           //RND_GPU_PROG_GM_REG_oBFC0
    , "result.color.back.secondary"         //RND_GPU_PROG_GM_REG_oBFC1
    , "result.fogcoord"                     //RND_GPU_PROG_GM_REG_oFOGC
    , "result.pointsize"                    //RND_GPU_PROG_GM_REG_oPSIZ
    , "result.texcoord[0]"                  //RND_GPU_PROG_GM_REG_oTEX0
    , "result.texcoord[1]"                  //RND_GPU_PROG_GM_REG_oTEX1
    , "result.texcoord[2]"                  //RND_GPU_PROG_GM_REG_oTEX2
    , "result.texcoord[3]"                  //RND_GPU_PROG_GM_REG_oTEX3
    , "result.texcoord[4]"                  //RND_GPU_PROG_GM_REG_oTEX4
    , "result.texcoord[5]"                  //RND_GPU_PROG_GM_REG_oTEX5
    , "result.texcoord[6]"                  //RND_GPU_PROG_GM_REG_oTEX6
    , "result.texcoord[7]"                  //RND_GPU_PROG_GM_REG_oTEX7
    , "result.attrib[6]"                    //RND_GPU_PROG_GM_REG_oNRML
    , "result.clip[0]"                      //RND_GPU_PROG_GM_REG_oCLP0
    , "result.clip[1]"                      //RND_GPU_PROG_GM_REG_oCLP1
    , "result.clip[2]"                      //RND_GPU_PROG_GM_REG_oCLP2
    , "result.clip[3]"                      //RND_GPU_PROG_GM_REG_oCLP3
    , "result.clip[4]"                      //RND_GPU_PROG_GM_REG_oCLP4
    , "result.clip[5]"                      //RND_GPU_PROG_GM_REG_oCLP5
    , "result.primid"                       //RND_GPU_PROG_GM_REG_oID
    , "result.layer"                        //RND_GPU_PROG_GM_REG_oLAYR
    , "result.viewport"                     //RND_GPU_PROG_GM_REG_oVPORTIDX
    , "result.viewportmask[0]"              //RND_GPU_PROG_GM_REG_oVPORTMASK (reg [0] is sufficient as we are not intending to render to more than 32 viewports)
    , "tmpR0"                               //RND_GPU_PROG_GM_REG_R0
    , "tmpR1"                               //RND_GPU_PROG_GM_REG_R1
    , "tmpR2"                               //RND_GPU_PROG_GM_REG_R2
    , "tmpR3"                               //RND_GPU_PROG_GM_REG_R3
    , "tmpR4"                               //RND_GPU_PROG_GM_REG_R4
    , "tmpR5"                               //RND_GPU_PROG_GM_REG_R5
    , "tmpR6"                               //RND_GPU_PROG_GM_REG_R6
    , "tmpR7"                               //RND_GPU_PROG_GM_REG_R7
    , "tmpR8"                               //RND_GPU_PROG_GM_REG_R8
    , "tmpR9"                               //RND_GPU_PROG_GM_REG_R9
    , "tmpR10"                              //RND_GPU_PROG_GM_REG_R10
    , "tmpR11"                              //RND_GPU_PROG_GM_REG_R11
    , "tmpR12"                              //RND_GPU_PROG_GM_REG_R12
    , "tmpR13"                              //RND_GPU_PROG_GM_REG_R13
    , "tmpR14"                              //RND_GPU_PROG_GM_REG_R14
    , "tmpR15"                              //RND_GPU_PROG_GM_REG_R15
    , "tmpA128"                             //RND_GPU_PROG_GM_REG_A128
    , "pgmElw"                              //RND_GPU_PROG_GM_REG_Const
    , "program.local"                       //RND_GPU_PROG_GM_REG_LocalElw
    , ""                                    //RND_GPU_PROG_GM_REG_ConstVect
    , ""                                    //RND_GPU_PROG_GM_REG_Literal
    , "cbuf"                                //RND_GPU_PROG_GM_REG_PaboElw
    , "tmpArray"                            //RND_GPU_PROG_GM_REG_TempArray
    , "Subs"                                //RND_GPU_PROG_GM_REG_Subs
    , "tmpA1"                               //RND_GPU_PROG_GM_REG_A1
    , "tmpASubs"                            //RND_GPU_PROG_GM_REG_ASubs
    , "tmpA3"                               //RND_GPU_PROG_GM_REG_A3
    , "garbage"                             //RND_GPU_PROG_GM_REG_Garbage
    
};

// Each property uses 2 bits, we lwrrently have 82 properties so we need 164
// bits.
const static UINT64 s_GmppIOAttribs[] =
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
    (UINT64)ppIORd          << (ppVtxId*2) |
    (UINT64)ppIORd          << (ppInstance*2) |
    (UINT64)ppIONone        << (ppFacing*2) |
    (UINT64)ppIONone        << (ppDepth*2) |
    (UINT64)ppIORw          << (ppPrimId*2) |
    (UINT64)ppIOWr          << (ppLayer*2) |
    (UINT64)ppIOWr          << (ppViewport*2) |
    (UINT64)ppIORd          << (ppPrimitive*2) |
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

const static GpuPgmGen::PassthruRegs GmPassthruRegs[] =
{
    // Xfb prop                 Program prop  input                       output
     {RndGpuPrograms::Pos,      ppPos,        RND_GPU_PROG_GM_REG_vHPOS   ,RND_GPU_PROG_GM_REG_oHPOS  }
    ,{RndGpuPrograms::Col0,     ppPriColor,   RND_GPU_PROG_GM_REG_vCOL0   ,RND_GPU_PROG_GM_REG_oCOL0  }
    ,{RndGpuPrograms::Col1,     ppSecColor,   RND_GPU_PROG_GM_REG_vCOL1   ,RND_GPU_PROG_GM_REG_oCOL1  }
    ,{RndGpuPrograms::BkCol0,   ppBkPriColor, RND_GPU_PROG_GM_REG_vBFC0   ,RND_GPU_PROG_GM_REG_oBFC0  }
    ,{RndGpuPrograms::BkCol1,   ppBkSecColor, RND_GPU_PROG_GM_REG_vBFC1   ,RND_GPU_PROG_GM_REG_oBFC1  }
    ,{RndGpuPrograms::Fog,      ppFogCoord,   RND_GPU_PROG_GM_REG_vFOGC   ,RND_GPU_PROG_GM_REG_oFOGC  }
    ,{RndGpuPrograms::PtSz,     ppPtSize,     RND_GPU_PROG_GM_REG_vPSIZ   ,RND_GPU_PROG_GM_REG_oPSIZ  }
    ,{RndGpuPrograms::Tex0,     ppTxCd0+0,    RND_GPU_PROG_GM_REG_vTEX0   ,RND_GPU_PROG_GM_REG_oTEX0  }
    ,{RndGpuPrograms::Tex1,     ppTxCd0+1,    RND_GPU_PROG_GM_REG_vTEX1   ,RND_GPU_PROG_GM_REG_oTEX1  }
    ,{RndGpuPrograms::Tex2,     ppTxCd0+2,    RND_GPU_PROG_GM_REG_vTEX2   ,RND_GPU_PROG_GM_REG_oTEX2  }
    ,{RndGpuPrograms::Tex3,     ppTxCd0+3,    RND_GPU_PROG_GM_REG_vTEX3   ,RND_GPU_PROG_GM_REG_oTEX3  }
    ,{RndGpuPrograms::Tex4,     ppTxCd0+4,    RND_GPU_PROG_GM_REG_vTEX4   ,RND_GPU_PROG_GM_REG_oTEX4  }
    ,{RndGpuPrograms::Tex5,     ppTxCd0+5,    RND_GPU_PROG_GM_REG_vTEX5   ,RND_GPU_PROG_GM_REG_oTEX5  }
    ,{RndGpuPrograms::Tex6,     ppTxCd0+6,    RND_GPU_PROG_GM_REG_vTEX6   ,RND_GPU_PROG_GM_REG_oTEX6  }
    ,{RndGpuPrograms::Tex7,     ppTxCd0+7,    RND_GPU_PROG_GM_REG_vTEX7   ,RND_GPU_PROG_GM_REG_oTEX7  }
    ,{RndGpuPrograms::Clp0,     ppClip0+0,    RND_GPU_PROG_GM_REG_vCLP0   ,RND_GPU_PROG_GM_REG_oCLP0  }
    ,{RndGpuPrograms::Clp1,     ppClip0+1,    RND_GPU_PROG_GM_REG_vCLP1   ,RND_GPU_PROG_GM_REG_oCLP1  }
    ,{RndGpuPrograms::Clp2,     ppClip0+2,    RND_GPU_PROG_GM_REG_vCLP2   ,RND_GPU_PROG_GM_REG_oCLP2  }
    ,{RndGpuPrograms::Clp3,     ppClip0+3,    RND_GPU_PROG_GM_REG_vCLP3   ,RND_GPU_PROG_GM_REG_oCLP3  }
    ,{RndGpuPrograms::Clp4,     ppClip0+4,    RND_GPU_PROG_GM_REG_vCLP4   ,RND_GPU_PROG_GM_REG_oCLP4  }
    ,{RndGpuPrograms::Clp5,     ppClip0+5,    RND_GPU_PROG_GM_REG_vCLP5   ,RND_GPU_PROG_GM_REG_oCLP5  }
    ,{RndGpuPrograms::PrimId,   ppPrimitive,  RND_GPU_PROG_GM_REG_pID     ,RND_GPU_PROG_GM_REG_oPID   }
    ,{0,                        ppIlwalid,    0                           ,0                          }
};

// lookup table to colwert a register to a ProgProperty
const static int RegToProp[RND_GPU_PROG_GM_REG_NUM_REGS] = {
    ppPos,         //RND_GPU_PROG_GM_REG_vOPOS
    ppPriColor,    //RND_GPU_PROG_GM_REG_vCOL0
    ppSecColor,    //RND_GPU_PROG_GM_REG_vCOL1
    ppBkPriColor,  //RND_GPU_PROG_GM_REG_vBFC0
    ppBkSecColor,  //RND_GPU_PROG_GM_REG_vBFC1
    ppFogCoord,    //RND_GPU_PROG_GM_REG_vFOGC
    ppPtSize,      //RND_GPU_PROG_GM_REG_vPSIZ
    ppTxCd0,       //RND_GPU_PROG_GM_REG_vTEX0
    ppTxCd0+1,     //RND_GPU_PROG_GM_REG_vTEX1
    ppTxCd0+2,     //RND_GPU_PROG_GM_REG_vTEX2
    ppTxCd0+3,     //RND_GPU_PROG_GM_REG_vTEX3
    ppTxCd0+4,     //RND_GPU_PROG_GM_REG_vTEX4
    ppTxCd0+5,     //RND_GPU_PROG_GM_REG_vTEX5
    ppTxCd0+6,     //RND_GPU_PROG_GM_REG_vTEX6
    ppTxCd0+7,     //RND_GPU_PROG_GM_REG_vTEX7
    ppNrml,        //RND_GPU_PROG_GM_REG_vNRML
    ppClip0,       //RND_GPU_PROG_GM_REG_vCLP0
    ppClip0+1,     //RND_GPU_PROG_GM_REG_vCLP1
    ppClip0+2,     //RND_GPU_PROG_GM_REG_vCLP2
    ppClip0+3,     //RND_GPU_PROG_GM_REG_vCLP3
    ppClip0+4,     //RND_GPU_PROG_GM_REG_vCLP4
    ppClip0+5,     //RND_GPU_PROG_GM_REG_vCLP5
    ppVtxId,       //RND_GPU_PROG_GM_REG_vID
    ppPrimId,      //RND_GPU_PROG_GM_REG_pID
    ppIlwalid,     //RND_GPU_PROG_GM_REG_pILWO
    ppPos,         //RND_GPU_PROG_GM_REG_oHPOS
    ppPriColor,    //RND_GPU_PROG_GM_REG_oCOL0
    ppSecColor,    //RND_GPU_PROG_GM_REG_oCOL1
    ppBkPriColor,  //RND_GPU_PROG_GM_REG_oBFC0
    ppBkSecColor,  //RND_GPU_PROG_GM_REG_oBFC1
    ppFogCoord,    //RND_GPU_PROG_GM_REG_oFOGC
    ppPtSize,      //RND_GPU_PROG_GM_REG_oPSIZ
    ppTxCd0,       //RND_GPU_PROG_GM_REG_oTEX0
    ppTxCd0+1,     //RND_GPU_PROG_GM_REG_oTEX1
    ppTxCd0+2,     //RND_GPU_PROG_GM_REG_oTEX2
    ppTxCd0+3,     //RND_GPU_PROG_GM_REG_oTEX3
    ppTxCd0+4,     //RND_GPU_PROG_GM_REG_oTEX4
    ppTxCd0+5,     //RND_GPU_PROG_GM_REG_oTEX5
    ppTxCd0+6,     //RND_GPU_PROG_GM_REG_oTEX6
    ppTxCd0+7,     //RND_GPU_PROG_GM_REG_oTEX7
    ppNrml,        //RND_GPU_PROG_GM_REG_oNRML
    ppClip0,       //RND_GPU_PROG_GM_REG_oCLP0
    ppClip0+1,     //RND_GPU_PROG_GM_REG_oCLP1
    ppClip0+2,     //RND_GPU_PROG_GM_REG_oCLP2
    ppClip0+3,     //RND_GPU_PROG_GM_REG_oCLP3
    ppClip0+4,     //RND_GPU_PROG_GM_REG_oCLP4
    ppClip0+5,     //RND_GPU_PROG_GM_REG_oCLP5
    ppPrimId,      //RND_GPU_PROG_GM_REG_oID
    ppLayer,       //RND_GPU_PROG_GM_REG_oLAYR
    ppViewport,    //RND_GPU_PROG_GM_REG_oVPORTIDX
    ppViewport,    //RND_GPU_PROG_GM_REG_oVPORTMASK
    ppIlwalid,     //RND_GPU_PROG_GM_REG_R0
    ppIlwalid,     //RND_GPU_PROG_GM_REG_R1
    ppIlwalid,     //RND_GPU_PROG_GM_REG_R2
    ppIlwalid,     //RND_GPU_PROG_GM_REG_R3
    ppIlwalid,     //RND_GPU_PROG_GM_REG_R4
    ppIlwalid,     //RND_GPU_PROG_GM_REG_R5
    ppIlwalid,     //RND_GPU_PROG_GM_REG_R6
    ppIlwalid,     //RND_GPU_PROG_GM_REG_R7
    ppIlwalid,     //RND_GPU_PROG_GM_REG_R8
    ppIlwalid,     //RND_GPU_PROG_GM_REG_R9
    ppIlwalid,     //RND_GPU_PROG_GM_REG_R10
    ppIlwalid,     //RND_GPU_PROG_GM_REG_R11
    ppIlwalid,     //RND_GPU_PROG_GM_REG_R12
    ppIlwalid,     //RND_GPU_PROG_GM_REG_R13
    ppIlwalid,     //RND_GPU_PROG_GM_REG_R14
    ppIlwalid,     //RND_GPU_PROG_GM_REG_R15
    ppIlwalid,     //RND_GPU_PROG_GM_REG_A128
    ppIlwalid,     //RND_GPU_PROG_GM_REG_Const
    ppIlwalid,     //RND_GPU_PROG_GM_REG_LocalElw
    ppIlwalid,     //RND_GPU_PROG_GM_REG_ConstVect
    ppIlwalid,     //RND_GPU_PROG_GM_REG_Literal
    ppIlwalid,     //RND_GPU_PROG_GM_REG_PaboElw
    ppIlwalid,     //RND_GPU_PROG_GM_REG_TempArray
    ppIlwalid,     //RND_GPU_PROG_GM_REG_Subs
    ppIlwalid,     //RND_GPU_PROG_GM_REG_A1
    ppIlwalid,     //RND_GPU_PROG_GM_REG_ASubs
    ppIlwalid,     //RND_GPU_PROG_GM_REG_A3
    ppIlwalid      //RND_GPU_PROG_GM_REG_Garbage
};

const char * GpuPgmGmPassthruTri =
    "!!LWgp4.0\n"
    " # simple passthru program\n"
    " # set the GP enrironment parameters\n"
    " PRIMITIVE_IN TRIANGLES;\n"
    " PRIMITIVE_OUT TRIANGLE_STRIP;\n"
    " VERTICES_OUT 3;\n"
    " # emit the data\n"
    " MOV result.color, vertex[0].color;\n"
    " MOV result.color.secondary, vertex[0].color.secondary;\n"
    " MOV result.color.back, vertex[0].color.back;\n"
    " MOV result.color.back.secondary, vertex[0].color.back.secondary;\n"
    " MOV result.position, vertex[0].position;\n"
    " MOV result.fogcoord.x, vertex[0].fogcoord.x;\n"          // fog passthru
    " MOV result.pointsize.x, vertex[0].pointsize.x;\n"        // ptsize passthru
    " MOV result.texcoord[0], vertex[0].texcoord[0];\n"    // tex0 coord passthru
    " MOV result.texcoord[1], vertex[0].texcoord[1];\n"    // tex1 coord passthru
    " MOV result.texcoord[2], vertex[0].texcoord[2];\n"    // tex2 coord passthru
    " MOV result.texcoord[3], vertex[0].texcoord[3];\n"    // tex3 coord passthru
    " MOV result.texcoord[4], vertex[0].texcoord[4];\n"    // tex4 coord passthru
    " MOV result.texcoord[5], vertex[0].texcoord[5];\n"    // tex5 coord passthru
    " MOV result.texcoord[6], vertex[0].texcoord[6];\n"    // tex6 coord passthru
    " MOV result.texcoord[7], vertex[0].texcoord[7];\n"    // tex7 coord passthru
    " MOV result.attrib[6], vertex[0].attrib[6];\n"
    "EMIT;\n"
    "MOV result.color, vertex[1].color;\n"
    " MOV result.color.secondary, vertex[1].color.secondary;\n"
    " MOV result.color.back, vertex[1].color.back;\n"
    " MOV result.color.back.secondary, vertex[1].color.back.secondary;\n"
    " MOV result.position, vertex[1].position;\n"
    " MOV result.fogcoord.x, vertex[1].fogcoord.x;\n"          // fog passthru
    " MOV result.pointsize.x, vertex[1].pointsize.x;\n"        // ptsize passthru
    " MOV result.texcoord[0], vertex[1].texcoord[0];\n"    // tex0 coord passthru
    " MOV result.texcoord[1], vertex[1].texcoord[1];\n"    // tex1 coord passthru
    " MOV result.texcoord[2], vertex[1].texcoord[2];\n"    // tex2 coord passthru
    " MOV result.texcoord[3], vertex[1].texcoord[3];\n"    // tex3 coord passthru
    " MOV result.texcoord[4], vertex[1].texcoord[4];\n"    // tex4 coord passthru
    " MOV result.texcoord[5], vertex[1].texcoord[5];\n"    // tex5 coord passthru
    " MOV result.texcoord[6], vertex[1].texcoord[6];\n"    // tex6 coord passthru
    " MOV result.texcoord[7], vertex[1].texcoord[7];\n"    // tex7 coord passthru
    " MOV result.attrib[6], vertex[1].attrib[6];\n"
    "EMIT;\n"
    " MOV result.color, vertex[2].color;\n"
    " MOV result.color.secondary, vertex[2].color.secondary;\n"
    " MOV result.color.back, vertex[2].color.back;\n"
    " MOV result.color.back.secondary, vertex[2].color.back.secondary;\n"
    " MOV result.position, vertex[2].position;\n"
    " MOV result.fogcoord.x, vertex[2].fogcoord.x;\n"          // fog passthru
    " MOV result.pointsize.x, vertex[2].pointsize.x;\n"        // ptsize passthru
    " MOV result.texcoord[0], vertex[2].texcoord[0];\n"    // tex0 coord passthru
    " MOV result.texcoord[1], vertex[2].texcoord[1];\n"    // tex1 coord passthru
    " MOV result.texcoord[2], vertex[2].texcoord[2];\n"    // tex2 coord passthru
    " MOV result.texcoord[3], vertex[2].texcoord[3];\n"    // tex3 coord passthru
    " MOV result.texcoord[4], vertex[2].texcoord[4];\n"    // tex4 coord passthru
    " MOV result.texcoord[5], vertex[2].texcoord[5];\n"    // tex5 coord passthru
    " MOV result.texcoord[6], vertex[2].texcoord[6];\n"    // tex6 coord passthru
    " MOV result.texcoord[7], vertex[2].texcoord[7];\n"    // tex7 coord passthru
    " MOV result.attrib[6], vertex[2].attrib[6];\n"
    "EMIT;\n"
    "END\n" ;

const char * GpuPgmGmPassthruTriAdj =
    "!!LWgp4.0\n"
    " # simple passthru program\n"
    " # set the GP enrironment parameters\n"
    " PRIMITIVE_IN TRIANGLES_ADJACENCY;\n"
    " PRIMITIVE_OUT TRIANGLE_STRIP;\n"
    " VERTICES_OUT 3;\n"
    " # emit the data\n"
    " MOV result.color, vertex[0].color;\n"
    " MOV result.color.secondary, vertex[0].color.secondary;\n"
    " MOV result.color.back, vertex[0].color.back;\n"
    " MOV result.color.back.secondary, vertex[0].color.back.secondary;\n"
    " MOV result.position, vertex[0].position;\n"
    " MOV result.fogcoord.x, vertex[0].fogcoord.x;\n"          // fog passthru
    " MOV result.pointsize.x, vertex[0].pointsize.x;\n"        // ptsize passthru
    " MOV result.texcoord[0], vertex[0].texcoord[0];\n"    // tex0 coord passthru
    " MOV result.texcoord[1], vertex[0].texcoord[1];\n"    // tex1 coord passthru
    " MOV result.texcoord[2], vertex[0].texcoord[2];\n"    // tex2 coord passthru
    " MOV result.texcoord[3], vertex[0].texcoord[3];\n"    // tex3 coord passthru
    " MOV result.texcoord[4], vertex[0].texcoord[4];\n"    // tex4 coord passthru
    " MOV result.texcoord[5], vertex[0].texcoord[5];\n"    // tex5 coord passthru
    " MOV result.texcoord[6], vertex[0].texcoord[6];\n"    // tex6 coord passthru
    " MOV result.texcoord[7], vertex[0].texcoord[7];\n"    // tex7 coord passthru
    " MOV result.attrib[6], vertex[0].attrib[6];\n"
    " EMIT;\n"
    " MOV result.color, vertex[1].color;\n"
    " MOV result.color.secondary, vertex[1].color.secondary;\n"
    " MOV result.color.back, vertex[1].color.back;\n"
    " MOV result.color.back.secondary, vertex[1].color.back.secondary;\n"
    " MOV result.position, vertex[1].position;\n"
    " MOV result.fogcoord.x, vertex[1].fogcoord.x;\n"          // fog passthru
    " MOV result.pointsize.x, vertex[1].pointsize.x;\n"        // ptsize passthru
    " MOV result.texcoord[0], vertex[1].texcoord[0];\n"    // tex0 coord passthru
    " MOV result.texcoord[1], vertex[1].texcoord[1];\n"    // tex1 coord passthru
    " MOV result.texcoord[2], vertex[1].texcoord[2];\n"    // tex2 coord passthru
    " MOV result.texcoord[3], vertex[1].texcoord[3];\n"    // tex3 coord passthru
    " MOV result.texcoord[4], vertex[1].texcoord[4];\n"    // tex4 coord passthru
    " MOV result.texcoord[5], vertex[1].texcoord[5];\n"    // tex5 coord passthru
    " MOV result.texcoord[6], vertex[1].texcoord[6];\n"    // tex6 coord passthru
    " MOV result.texcoord[7], vertex[1].texcoord[7];\n"    // tex7 coord passthru
    " MOV result.attrib[6], vertex[1].attrib[6];\n"
    " EMIT;\n"
    " MOV result.color, vertex[2].color;\n"
    " MOV result.color.secondary, vertex[2].color.secondary;\n"
    " MOV result.color.back, vertex[2].color.back;\n"
    " MOV result.color.back.secondary, vertex[2].color.back.secondary;\n"
    " MOV result.position, vertex[2].position;\n"
    " MOV result.fogcoord.x, vertex[2].fogcoord.x;\n"          // fog passthru
    " MOV result.pointsize.x, vertex[2].pointsize.x;\n"        // ptsize passthru
    " MOV result.texcoord[0], vertex[2].texcoord[0];\n"    // tex0 coord passthru
    " MOV result.texcoord[1], vertex[2].texcoord[1];\n"    // tex1 coord passthru
    " MOV result.texcoord[2], vertex[2].texcoord[2];\n"    // tex2 coord passthru
    " MOV result.texcoord[3], vertex[2].texcoord[3];\n"    // tex3 coord passthru
    " MOV result.texcoord[4], vertex[2].texcoord[4];\n"    // tex4 coord passthru
    " MOV result.texcoord[5], vertex[2].texcoord[5];\n"    // tex5 coord passthru
    " MOV result.texcoord[6], vertex[2].texcoord[6];\n"    // tex6 coord passthru
    " MOV result.texcoord[7], vertex[2].texcoord[7];\n"    // tex7 coord passthru
    " MOV result.attrib[6], vertex[2].attrib[6];\n"
    " EMIT;\n"
    "END\n" ;

const char * GpuPgmGmPassthruLine =
    "!!LWgp4.0\n"
    " # simple passthru program\n"
    " # set the GP enrironment parameters\n"
    " PRIMITIVE_IN LINES;\n"
    " PRIMITIVE_OUT LINE_STRIP;\n"
    " VERTICES_OUT 2;\n"
    " # emit the data\n"
    " MOV result.color, vertex[0].color;\n"
    " MOV result.color.secondary, vertex[0].color.secondary;\n"
    " MOV result.color.back, vertex[0].color.back;\n"
    " MOV result.color.back.secondary, vertex[0].color.back.secondary;\n"
    " MOV result.position, vertex[0].position;\n"
    " MOV result.fogcoord.x, vertex[0].fogcoord.x;\n"          // fog passthru
    " MOV result.pointsize.x, vertex[0].pointsize.x;\n"        // ptsize passthru
    " MOV result.texcoord[0], vertex[0].texcoord[0];\n"    // tex0 coord passthru
    " MOV result.texcoord[1], vertex[0].texcoord[1];\n"    // tex1 coord passthru
    " MOV result.texcoord[2], vertex[0].texcoord[2];\n"    // tex2 coord passthru
    " MOV result.texcoord[3], vertex[0].texcoord[3];\n"    // tex3 coord passthru
    " MOV result.texcoord[4], vertex[0].texcoord[4];\n"    // tex4 coord passthru
    " MOV result.texcoord[5], vertex[0].texcoord[5];\n"    // tex5 coord passthru
    " MOV result.texcoord[6], vertex[0].texcoord[6];\n"    // tex6 coord passthru
    " MOV result.texcoord[7], vertex[0].texcoord[7];\n"    // tex7 coord passthru
    " MOV result.attrib[6], vertex[0].attrib[6];\n"
    " EMIT;\n"
    " MOV result.color, vertex[1].color;\n"
    " MOV result.color.secondary, vertex[1].color.secondary;\n"
    " MOV result.color.back, vertex[1].color.back;\n"
    " MOV result.color.back.secondary, vertex[1].color.back.secondary;\n"
    " MOV result.position, vertex[1].position;\n"
    " MOV result.fogcoord, vertex[1].fogcoord;\n"          // fog passthru
    " MOV result.pointsize.x, vertex[1].pointsize.x;\n"        // ptsize passthru
    " MOV result.texcoord[0], vertex[1].texcoord[0];\n"    // tex0 coord passthru
    " MOV result.texcoord[1], vertex[1].texcoord[1];\n"    // tex1 coord passthru
    " MOV result.texcoord[2], vertex[1].texcoord[2];\n"    // tex2 coord passthru
    " MOV result.texcoord[3], vertex[1].texcoord[3];\n"    // tex3 coord passthru
    " MOV result.texcoord[4], vertex[1].texcoord[4];\n"    // tex4 coord passthru
    " MOV result.texcoord[5], vertex[1].texcoord[5];\n"    // tex5 coord passthru
    " MOV result.texcoord[6], vertex[1].texcoord[6];\n"    // tex6 coord passthru
    " MOV result.texcoord[7], vertex[1].texcoord[7];\n"    // tex7 coord passthru
    " MOV result.attrib[6], vertex[1].attrib[6];\n"
    " EMIT;\n"
    "END\n" ;

const char * GpuPgmGmPassthruLineAdj =
    "!!LWgp4.0\n"
    " # simple passthru program, but don't pass the adjacency vertices\n"
    " # set the GP enrironment parameters\n"
    " PRIMITIVE_IN LINES_ADJACENCY;\n"
    " PRIMITIVE_OUT LINE_STRIP;\n"
    " VERTICES_OUT 2;\n"
    " # emit the data\n"
    " MOV result.color, vertex[1].color;\n"
    " MOV result.color.secondary, vertex[1].color.secondary;\n"
    " MOV result.color.back, vertex[1].color.back;\n"
    " MOV result.color.back.secondary, vertex[1].color.back.secondary;\n"
    " MOV result.position, vertex[1].position;\n"
    " MOV result.fogcoord.x, vertex[1].fogcoord.x;\n"          // fog passthru
    " MOV result.pointsize.x, vertex[1].pointsize.x;\n"        // ptsize passthru
    " MOV result.texcoord[0], vertex[1].texcoord[0];\n"    // tex0 coord passthru
    " MOV result.texcoord[1], vertex[1].texcoord[1];\n"    // tex1 coord passthru
    " MOV result.texcoord[2], vertex[1].texcoord[2];\n"    // tex2 coord passthru
    " MOV result.texcoord[3], vertex[1].texcoord[3];\n"    // tex3 coord passthru
    " MOV result.texcoord[4], vertex[1].texcoord[4];\n"    // tex4 coord passthru
    " MOV result.texcoord[5], vertex[1].texcoord[5];\n"    // tex5 coord passthru
    " MOV result.texcoord[6], vertex[1].texcoord[6];\n"    // tex6 coord passthru
    " MOV result.texcoord[7], vertex[1].texcoord[7];\n"    // tex7 coord passthru
    " MOV result.attrib[6], vertex[1].attrib[6];\n"
    " EMIT;\n"
    " MOV result.color, vertex[2].color;\n"
    " MOV result.color.secondary, vertex[2].color.secondary;\n"
    " MOV result.color.back, vertex[2].color.back;\n"
    " MOV result.color.back.secondary, vertex[2].color.back.secondary;\n"
    " MOV result.position, vertex[2].position;\n"
    " MOV result.fogcoord.x, vertex[2].fogcoord.x;\n"          // fog passthru
    " MOV result.pointsize.x, vertex[2].pointsize.x;\n"        // ptsize passthru
    " MOV result.texcoord[0], vertex[2].texcoord[0];\n"    // tex0 coord passthru
    " MOV result.texcoord[1], vertex[2].texcoord[1];\n"    // tex1 coord passthru
    " MOV result.texcoord[2], vertex[2].texcoord[2];\n"    // tex2 coord passthru
    " MOV result.texcoord[3], vertex[2].texcoord[3];\n"    // tex3 coord passthru
    " MOV result.texcoord[4], vertex[2].texcoord[4];\n"    // tex4 coord passthru
    " MOV result.texcoord[5], vertex[2].texcoord[5];\n"    // tex5 coord passthru
    " MOV result.texcoord[6], vertex[2].texcoord[6];\n"    // tex6 coord passthru
    " MOV result.texcoord[7], vertex[2].texcoord[7];\n"    // tex7 coord passthru
    " MOV result.attrib[6], vertex[2].attrib[6];\n"
    " EMIT;\n"
    "END\n" ;

const char * GpuPgmGmPassthruLineAdj2 =
    "!!LWgp4.0\n"
    " # simple passthru program that will pass the adjacency vertices\n"
    " # set the GP enrironment parameters\n"
    " PRIMITIVE_IN LINES_ADJACENCY;\n"
    " PRIMITIVE_OUT LINE_STRIP;\n"
    " VERTICES_OUT 4;\n"
    " # emit the data\n"
    " MOV result.color, vertex[0].color;\n"
    " MOV result.color.secondary, vertex[0].color.secondary;\n"
    " MOV result.color.back, vertex[0].color.back;\n"
    " MOV result.color.back.secondary, vertex[0].color.back.secondary;\n"
    " MOV result.position, vertex[0].position;\n"
    " MOV result.fogcoord.x, vertex[0].fogcoord.x;\n"          // fog passthru
    " MOV result.pointsize.x, vertex[0].pointsize.x;\n"        // ptsize passthru
    " MOV result.texcoord[0], vertex[0].texcoord[0];\n"    // tex0 coord passthru
    " MOV result.texcoord[1], vertex[0].texcoord[1];\n"    // tex1 coord passthru
    " MOV result.texcoord[2], vertex[0].texcoord[2];\n"    // tex2 coord passthru
    " MOV result.texcoord[3], vertex[0].texcoord[3];\n"    // tex3 coord passthru
    " MOV result.texcoord[4], vertex[0].texcoord[4];\n"    // tex4 coord passthru
    " MOV result.texcoord[5], vertex[0].texcoord[5];\n"    // tex5 coord passthru
    " MOV result.texcoord[6], vertex[0].texcoord[6];\n"    // tex6 coord passthru
    " MOV result.texcoord[7], vertex[0].texcoord[7];\n"    // tex7 coord passthru
    " MOV result.attrib[6], vertex[0].attrib[6];\n"
    " EMIT;\n"
    " MOV result.color, vertex[1].color;\n"
    " MOV result.color.secondary, vertex[1].color.secondary;\n"
    " MOV result.color.back, vertex[1].color.back;\n"
    " MOV result.color.back.secondary, vertex[1].color.back.secondary;\n"
    " MOV result.position, vertex[1].position;\n"
    " MOV result.fogcoord.x, vertex[1].fogcoord.x;\n"          // fog passthru
    " MOV result.pointsize.x, vertex[1].pointsize.x;\n"        // ptsize passthru
    " MOV result.texcoord[0], vertex[1].texcoord[0];\n"    // tex0 coord passthru
    " MOV result.texcoord[1], vertex[1].texcoord[1];\n"    // tex1 coord passthru
    " MOV result.texcoord[2], vertex[1].texcoord[2];\n"    // tex2 coord passthru
    " MOV result.texcoord[3], vertex[1].texcoord[3];\n"    // tex3 coord passthru
    " MOV result.texcoord[4], vertex[1].texcoord[4];\n"    // tex4 coord passthru
    " MOV result.texcoord[5], vertex[1].texcoord[5];\n"    // tex5 coord passthru
    " MOV result.texcoord[6], vertex[1].texcoord[6];\n"    // tex6 coord passthru
    " MOV result.texcoord[7], vertex[1].texcoord[7];\n"    // tex7 coord passthru
    " MOV result.attrib[6], vertex[1].attrib[6];\n"
    " EMIT;\n"
    " MOV result.color, vertex[2].color;\n"
    " MOV result.color.secondary, vertex[2].color.secondary;\n"
    " MOV result.color.back, vertex[2].color.back;\n"
    " MOV result.color.back.secondary, vertex[2].color.back.secondary;\n"
    " MOV result.position, vertex[2].position;\n"
    " MOV result.fogcoord.x, vertex[2].fogcoord.x;\n"          // fog passthru
    " MOV result.pointsize.x, vertex[2].pointsize.x;\n"        // ptsize passthru
    " MOV result.texcoord[0], vertex[2].texcoord[0];\n"    // tex0 coord passthru
    " MOV result.texcoord[1], vertex[2].texcoord[1];\n"    // tex1 coord passthru
    " MOV result.texcoord[2], vertex[2].texcoord[2];\n"    // tex2 coord passthru
    " MOV result.texcoord[3], vertex[2].texcoord[3];\n"    // tex3 coord passthru
    " MOV result.texcoord[4], vertex[2].texcoord[4];\n"    // tex4 coord passthru
    " MOV result.texcoord[5], vertex[2].texcoord[5];\n"    // tex5 coord passthru
    " MOV result.texcoord[6], vertex[2].texcoord[6];\n"    // tex6 coord passthru
    " MOV result.texcoord[7], vertex[2].texcoord[7];\n"    // tex7 coord passthru
    " MOV result.attrib[6], vertex[2].attrib[6];\n"
    " EMIT;\n"
    " MOV result.color, vertex[3].color;\n"
    " MOV result.color.secondary, vertex[3].color.secondary;\n"
    " MOV result.color.back, vertex[3].color.back;\n"
    " MOV result.color.back.secondary, vertex[3].color.back.secondary;\n"
    " MOV result.position, vertex[3].position;\n"
    " MOV result.fogcoord.x, vertex[3].fogcoord.x;\n"          // fog passthru
    " MOV result.pointsize.x, vertex[3].pointsize.x;\n"        // ptsize passthru
    " MOV result.texcoord[0], vertex[3].texcoord[0];\n"    // tex0 coord passthru
    " MOV result.texcoord[1], vertex[3].texcoord[1];\n"    // tex1 coord passthru
    " MOV result.texcoord[2], vertex[3].texcoord[2];\n"    // tex2 coord passthru
    " MOV result.texcoord[3], vertex[3].texcoord[3];\n"    // tex3 coord passthru
    " MOV result.texcoord[4], vertex[3].texcoord[4];\n"    // tex4 coord passthru
    " MOV result.texcoord[5], vertex[3].texcoord[5];\n"    // tex5 coord passthru
    " MOV result.texcoord[6], vertex[3].texcoord[6];\n"    // tex6 coord passthru
    " MOV result.texcoord[7], vertex[3].texcoord[7];\n"    // tex7 coord passthru
    " MOV result.attrib[6], vertex[3].attrib[6];\n"
    " EMIT;\n"
    "END\n" ; // can't have any chars after END

const char * GpuPgmGmPassthruPoints =
    "!!LWgp4.0\n"
    " # simple passthru program of points\n"
    " PRIMITIVE_IN POINTS;\n"
    " PRIMITIVE_OUT POINTS;\n"
    " VERTICES_OUT 1;\n"
    " MOV result.color, vertex[0].color;\n"
    " MOV result.color.secondary, vertex[0].color.secondary;\n"
    " MOV result.color.back, vertex[0].color.back;\n"
    " MOV result.color.back.secondary, vertex[0].color.back.secondary;\n"
    " MOV result.position, vertex[0].position;\n"
    " MOV result.fogcoord.x, vertex[0].fogcoord.x;\n"          // fog passthru
    " MOV result.pointsize.x, vertex[0].pointsize.x;\n"        // ptsize passthru
    " MOV result.texcoord[0], vertex[0].texcoord[0];\n"    // tex0 coord passthru
    " MOV result.texcoord[1], vertex[0].texcoord[1];\n"    // tex1 coord passthru
    " MOV result.texcoord[2], vertex[0].texcoord[2];\n"    // tex2 coord passthru
    " MOV result.texcoord[3], vertex[0].texcoord[3];\n"    // tex3 coord passthru
    " MOV result.texcoord[4], vertex[0].texcoord[4];\n"    // tex4 coord passthru
    " MOV result.texcoord[5], vertex[0].texcoord[5];\n"    // tex5 coord passthru
    " MOV result.texcoord[6], vertex[0].texcoord[6];\n"    // tex6 coord passthru
    " MOV result.texcoord[7], vertex[0].texcoord[7];\n"    // tex7 coord passthru
    " MOV result.attrib[6], vertex[0].attrib[6];\n"
    " EMIT;\n"
    "END\n" ; // can't have any chars after END

const char * GpuPgmGmPointSprite =
    "!!LWgp4.0\n"
    "# Colwert points to point sprites\n"
    "PRIMITIVE_IN POINTS;\n"
    "PRIMITIVE_OUT TRIANGLE_STRIP;\n"
    "VERTICES_OUT 4;\n"
    "PARAM size = { 0.1, 0.1, -0.1, -0.1 };\n"
    "TEMP v0, v1, v2, v3;\n"
    "MOV v0, vertex[0].position;\n"
    "MOV v1, vertex[0].position;\n"
    "MOV v2, vertex[0].position;\n"
    "MOV v3, vertex[0].position;\n"
    "ADD v0.xy, vertex[0].position, size.zwww;\n"
    "ADD v1.xy, vertex[0].position, size.xwww;\n"
    "ADD v2.xy, vertex[0].position, size.xyyy;\n"
    "ADD v3.xy, vertex[0].position, size.zyyy;\n"
    "MOV result.color, vertex[0].color;\n"
    "MOV result.position, v0;\n"
    "EMIT;\n"
    "MOV result.color, vertex[0].color;\n"
    "MOV result.position, v1;\n"
    "EMIT;\n"
    "MOV result.color, vertex[0].color;\n"
    "MOV result.position, v3;\n"
    "EMIT;\n"
    "MOV result.color, vertex[0].color;\n"
    "MOV result.position, v2;\n"
    "EMIT;\n"
    "END\n" ; // can't have any chars after END

const char * GpuPgmGmBezierLwrve =
    "!!LWgp4.0\n"
    "# filled quadratic bezier lwrve geometry program\n"
    "PRIMITIVE_IN TRIANGLES;\n"
    "#PRIMITIVE_OUT LINE_STRIP;\n"
    "PRIMITIVE_OUT TRIANGLE_STRIP;\n"
    "VERTICES_OUT 100;\n"
    // we could pass this in as a random value
    "#PARAM segments = program.local[0];\n"
    "TEMP segments;\n"
    "MOV segments.x, 20;\n"
    "PARAM mvp[4] = { state.matrix.mvp };\n"
    "TEMP pos, t, dt, temp;\n"
    "TEMP omt, b, prev, origin;\n"
    "DIV dt.x, 1.0, segments.x;\n"
    "MOV t.x, 0.0;\n"
    "ADD temp.x, segments.x, 1;\n"
    "MOV prev, vertex[0].position;\n"
    "ADD origin, vertex[0].position, vertex[1].position;\n"
    "MUL origin, origin, 0.5;\n"
    "REP temp.x;\n"
    "# Callwlate quadratic Bernstein blending functions:\n"
    "# B0(t) = (1-t)^2 \n"
    "# B1(t) = 2t(1-t) \n"
    "# B2(t) = t^2 \n"
    " ADD omt.x, 1.0, -t.x;     # omt = 1-t \n"
    " MUL b.x, omt.x, omt.x;    # B0 = (1-t)^2 \n"
    " MUL temp.x, 2.0, t.x;     # temp = 2*t \n"
    " MUL b.y, temp.x, omt.x;   # B1 = 2*t*(1-t) \n"
    " MUL b.z, t.x, t.x;        # B2 = t*t \n"
    "# evaluate lwrve\n"
    " MUL pos, vertex[0].position, b.x;\n"
    " MAD pos, vertex[2].position, b.y, pos;\n"
    " MAD pos, vertex[1].position, b.z, pos;\n"
    "#MOV result.position, pos;\n"
    "#EMIT;\n"
    " MOV result.color, 0;\n"
    " MOV result.position, origin;\n"
    " EMIT;\n"
    " MOV result.color, 0;\n"
    " MOV result.color.x, t.x;\n"
    " MOV result.position, prev;\n"
    " EMIT;\n"
    " MOV result.color.yzw, 0;\n"
    " MOV result.color.x, t.x;\n"
    " MOV result.position, pos;\n"
    " EMIT;\n"
    " ENDPRIM;\n"
    " MOV prev, pos;\n"
    " ADD t, t, dt;\n"
    " ENDREP;\n"
    "END\n" ;

const GmGpuPgmGen::GmFixedPgms s_gmFixed[] =
{
    // PrimIn,                  PrimOut             Factor Name,           PgmString
    {GL_TRIANGLES,              GL_TRIANGLE_STRIP,  1.0F,  "gmTri",        GpuPgmGmPassthruTri},
    {GL_TRIANGLES_ADJACENCY_EXT,GL_TRIANGLE_STRIP,  1.0F,  "gmTriAdj",     GpuPgmGmPassthruTriAdj},
    {GL_LINES,                  GL_LINE_STRIP,      1.0F,  "gmLine",       GpuPgmGmPassthruLine},
    {GL_LINES_ADJACENCY_EXT,    GL_LINE_STRIP,      1.0F,  "gmLineAdj",    GpuPgmGmPassthruLineAdj},
    {GL_LINES_ADJACENCY_EXT,    GL_LINE_STRIP,      1.0F,  "gmLineAdj2",   GpuPgmGmPassthruLineAdj2},
    {GL_POINTS,                 GL_POINTS,          1.0F,  "gmPoints",     GpuPgmGmPassthruPoints},
    {GL_POINTS,                 GL_TRIANGLE_STRIP,  4.0F,  "gmPointSprite",GpuPgmGmPointSprite}
    //{GL_TRIANGLES,              GL_TRIANGLE_STRIP,  33.3F, "gmBezierLwrve", GpuPgmGmBezierLwrve} needs debugging
};

//------------------------------------------------------------------------------
// Geometry program specific generation routines
//
//------------------------------------------------------------------------------
GmGpuPgmGen::GmGpuPgmGen (GLRandomTest * pGLRandom,FancyPickerArray *pPicker)
 :  GpuPgmGen(pGLRandom, pPicker, pkGeometry)
    ,m_NumVtxIn(0)
    ,m_NumVtxOut(0)
    ,m_NumIlwocations(0)
    ,m_IlwtxId(0)
    ,m_IsLwrrGMProgPassThru(false)
{
}

//------------------------------------------------------------------------------
GmGpuPgmGen::~GmGpuPgmGen()
{
}

//------------------------------------------------------------------------------
// Return the given property's IO attribute
int GmGpuPgmGen::GetPropIOAttr(ProgProperty prop) const
{
    if (prop >= ppPos && prop <= ppTxCd7)
    {
        // 32 entries per UINT64
        const int index = prop / 32;
        const int shift = prop % 32;
        return ((s_GmppIOAttribs[index] >> shift*2) & 0x3);
    }
    return ppIONone;
}

//------------------------------------------------------------------------------
// Generate a new random program.
//
void GmGpuPgmGen::GenerateFixedPrograms
(
    Programs * pProgramStorage
)
{
    InitPgmElw(NULL);

    GLint pgmIdx;
    for (pgmIdx = 0; pgmIdx < RND_GPU_PROG_GM_INDEX_NumFixedProgs; pgmIdx++)
    {
        ProgProperty i;
        // Fill in common defaults for this Program struct:
        Program tmp;
        InitProg(&tmp, GL_GEOMETRY_PROGRAM_LW,NULL); //!< Initialize the Program struct

        // Setup the default input/output pgm requirements for most of the passthru pgms
        tmp.PgmRqmt.Inputs[ppPos]      = omXYZW;
        tmp.PgmRqmt.Inputs[ppNrml]     = omXYZ;
        tmp.PgmRqmt.Inputs[ppPriColor] = omXYZW;
        tmp.PgmRqmt.Inputs[ppSecColor] = omXYZW;
        tmp.PgmRqmt.Inputs[ppBkPriColor] = omXYZW;
        tmp.PgmRqmt.Inputs[ppBkSecColor] = omXYZW;
        tmp.PgmRqmt.Inputs[ppFogCoord] = omX;
        for ( i=ppTxCd0; i<=ppTxCd7; i = i++)
            tmp.PgmRqmt.Inputs[i] = omXYZW | omPassThru;

        tmp.PgmRqmt.Outputs[ppPos]     = omXYZW;
        tmp.PgmRqmt.Outputs[ppNrml]     = omXYZ;
        tmp.PgmRqmt.Outputs[ppPriColor]= omXYZW;
        tmp.PgmRqmt.Outputs[ppSecColor]= omXYZW;
        tmp.PgmRqmt.Outputs[ppBkPriColor]= omXYZW;
        tmp.PgmRqmt.Outputs[ppBkSecColor]= omXYZW;
        tmp.PgmRqmt.Outputs[ppPtSize]  = omX;
        tmp.PgmRqmt.Outputs[ppFogCoord]= omX;
        for ( i=ppTxCd0; i<=ppTxCd7; i++)
            tmp.PgmRqmt.Outputs[i] = omXYZW | omPassThru ;

        tmp.PgmRqmt.PrimitiveIn = s_gmFixed[pgmIdx].primIn;
        tmp.PgmRqmt.PrimitiveOut = s_gmFixed[pgmIdx].primOut;
        tmp.PgmRqmt.VxMultiplier = s_gmFixed[pgmIdx].vxMultiplier;

        switch (pgmIdx)
        {
            // These are basically passthru programs for specific input primitives
            case RND_GPU_PROG_GM_INDEX_UseTri:
            case RND_GPU_PROG_GM_INDEX_UseTriAdj:
            case RND_GPU_PROG_GM_INDEX_UseLine:
            case RND_GPU_PROG_GM_INDEX_UseLineAdj:
            case RND_GPU_PROG_GM_INDEX_UseLineAdj2:
            case RND_GPU_PROG_GM_INDEX_UsePoint:
                tmp.Name = s_gmFixed[pgmIdx].name;
                tmp.Pgm = s_gmFixed[pgmIdx].pgm;
                tmp.XfbStdAttrib = RndGpuPrograms::AllStd;
                break;

            // Special primitive specific programs
            case RND_GPU_PROG_GM_INDEX_UsePointSprite:
                tmp.Name = s_gmFixed[pgmIdx].name;
                tmp.Pgm = s_gmFixed[pgmIdx].pgm;
                tmp.XfbStdAttrib = RndGpuPrograms::AllStd;
                // override the defaults
                tmp.PgmRqmt.Inputs.erase(ppSecColor);
                tmp.PgmRqmt.Inputs.erase(ppNrml);
                tmp.PgmRqmt.Inputs.erase(ppBkPriColor);
                tmp.PgmRqmt.Inputs.erase(ppBkSecColor);
                tmp.PgmRqmt.Inputs.erase(ppFogCoord);
                for ( i=ppTxCd0; i<=ppTxCd7; i++)
                    tmp.PgmRqmt.Inputs.erase(i);

                tmp.PgmRqmt.Outputs.erase(ppSecColor);
                tmp.PgmRqmt.Outputs.erase(ppNrml);
                tmp.PgmRqmt.Outputs.erase(ppBkPriColor);
                tmp.PgmRqmt.Outputs.erase(ppBkSecColor);
                tmp.PgmRqmt.Outputs.erase(ppPtSize);
                tmp.PgmRqmt.Outputs.erase(ppFogCoord);
                for ( i= ppTxCd0; i<= ppTxCd7; i++)
                    tmp.PgmRqmt.Outputs.erase(i);

                break;
            case RND_GPU_PROG_GM_INDEX_UseBezierLwrve:
                // This only properties we write are color and position
                tmp.Name = s_gmFixed[pgmIdx].name;
                tmp.Pgm = s_gmFixed[pgmIdx].pgm;
                tmp.XfbStdAttrib = RndGpuPrograms::AllStd;
                // override the defaults
                tmp.PgmRqmt.Inputs.erase(ppSecColor);
                tmp.PgmRqmt.Inputs.erase(ppNrml);
                tmp.PgmRqmt.Inputs.erase(ppBkPriColor);
                tmp.PgmRqmt.Inputs.erase(ppBkSecColor);
                tmp.PgmRqmt.Inputs.erase(ppFogCoord);
                tmp.PgmRqmt.Inputs.erase(ppPtSize);
                for ( i=ppTxCd0; i<=ppTxCd7; i++)
                    tmp.PgmRqmt.Inputs.erase(i);

                // we only write position and color
                tmp.PgmRqmt.Outputs.erase(ppSecColor);
                tmp.PgmRqmt.Outputs.erase(ppNrml);
                tmp.PgmRqmt.Outputs.erase(ppBkPriColor);
                tmp.PgmRqmt.Outputs.erase(ppBkSecColor);
                tmp.PgmRqmt.Outputs.erase(ppPtSize);
                tmp.PgmRqmt.Outputs.erase(ppFogCoord);
                for ( i= ppTxCd0; i<= ppTxCd7; i++)
                    tmp.PgmRqmt.Outputs.erase(i);

        }
        pProgramStorage->AddProg(psFixed, tmp);
    }
}

//------------------------------------------------------------------------------
// Generate a new random program.
//
void GmGpuPgmGen::GenerateRandomProgram
(
    GLRandom::PgmRequirements *pRqmts,
    Programs * pProgramStorage,
    const string & pgmName
)
{
    m_pPgmRqmts = pRqmts;
    InitPgmElw(pRqmts);
    m_GS.Prog.Name = pgmName;

    // Pick the number of disconnected primitives, vertices per primitive,
    // and max number of vertices to emit.
    m_GS.Prog.PgmRqmt.PrimitiveIn = (*m_pPickers)[RND_GPU_PROG_GM_PRIM_IN].Pick();
    m_GS.Prog.PgmRqmt.PrimitiveOut = (*m_pPickers)[RND_GPU_PROG_GM_PRIM_OUT].Pick();
    m_ThisProgWritesLayerVport = (0 != (*m_pPickers)[RND_GPU_PROG_GM_WRITES_TO_LAYR_VPORT].Pick());
    m_IsLwrrGMProgPassThru = (0 != (*m_pPickers)[RND_GPU_PROG_GM_ISPASSTHRU].Pick());

    if(IsLwrrGMProgPassThru() &&
        !m_pGLRandom->HasExt(GLRandomTest::ExtLW_geometry_shader_passthrough))
    {
        m_IsLwrrGMProgPassThru = false;
    }

    if (IsLwrrGMProgPassThru())
    {
        // only supports POINTS,LINES,TRIANGLES
        if (m_GS.Prog.PgmRqmt.PrimitiveIn == GL_LINES_ADJACENCY_EXT)
            m_GS.Prog.PgmRqmt.PrimitiveIn = GL_LINES;
        if (m_GS.Prog.PgmRqmt.PrimitiveIn == GL_TRIANGLES_ADJACENCY_EXT)
            m_GS.Prog.PgmRqmt.PrimitiveIn = GL_TRIANGLES;
    }

    m_NumVtxOut = (*m_pPickers)[RND_GPU_PROG_GM_VERTICES_OUT].Pick();
    m_NumIlwocations = (*m_pPickers)[RND_GPU_PROG_GM_PRIMITIVES_OUT].Pick();

    if (!m_pGLRandom->HasExt(GLRandomTest::ExtLW_gpu_program5) ||
        m_pGLRandom->GetNumLayersInFBO() > 0)
    {
        // Instanced geometry programs is a gpu_program5 feature.
        // Force 1 invocation in case of layered rendering as we want to write to
        // only a single layer and viewport per loop
        m_NumIlwocations = 1;
    }

    switch (m_GS.Prog.PgmRqmt.PrimitiveIn)
    {
        default:
            m_GS.Prog.PgmRqmt.PrimitiveIn = GL_TRIANGLES;
            // fall through
        case GL_TRIANGLES:                  m_NumVtxIn = 3;  break;
        case GL_POINTS:                     m_NumVtxIn = 1;  break;
        case GL_LINES:                      m_NumVtxIn = 2;  break;
        case GL_LINES_ADJACENCY_EXT:        m_NumVtxIn = 4;  break;
        case GL_TRIANGLES_ADJACENCY_EXT:    m_NumVtxIn = 6;  break;
    }

    // Range check output vertices
    switch (m_GS.Prog.PgmRqmt.PrimitiveOut)
    {
        default:
            m_GS.Prog.PgmRqmt.PrimitiveOut = GL_TRIANGLE_STRIP;
            // fall through
        case GL_TRIANGLE_STRIP:
            m_NumVtxOut = MINMAX(3,m_NumVtxOut, 20);
            break;
        case GL_POINTS:
            m_NumVtxOut = MINMAX(1, m_NumVtxOut, 10);
            break;
        case GL_LINE_STRIP:
            m_NumVtxOut = MINMAX(2,m_NumVtxOut, 20);
            break;
    }
    m_GS.Prog.PgmRqmt.VxMultiplier = m_NumIlwocations *
        static_cast<float>(m_NumVtxOut) / static_cast<float>(m_NumVtxIn);

    if (m_BnfExt != 0)
    {
        ProgramSequence(m_PgmHeader.c_str());
    }

    m_GS.Prog.CalcXfbAttributes();
    m_pPgmRqmts = NULL;

    pProgramStorage->AddProg(psRandom, m_GS.Prog);

    m_pGLRandom->LogData(RND_GPU_PROG, &m_ThisProgWritesLayerVport,
                         sizeof(m_ThisProgWritesLayerVport));
    m_pGLRandom->LogData(RND_GPU_PROG, &m_IsLwrrGMProgPassThru, sizeof(m_IsLwrrGMProgPassThru));
    m_pGLRandom->LogData(RND_GPU_PROG, &m_NumVtxOut, sizeof(m_NumVtxOut));
    m_pGLRandom->LogData(RND_GPU_PROG, &m_NumIlwocations, sizeof(m_NumIlwocations));
}

//-----------------------------------------------------------------------------
bool GmGpuPgmGen::IsLwrrGMProgPassThru() const
{
    return m_IsLwrrGMProgPassThru;
}

//------------------------------------------------------------------------------
// Setup the BNF grammar and program header based on the highest supported
// OpenGL extensions.
void GmGpuPgmGen::GetHighestSupportedExt()
{
    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_gpu_program5))
    {
        m_PgmHeader = "!!LWgp5.0\n";
        m_BnfExt = GLRandomTest::ExtLW_gpu_program5;
    }
    else if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_gpu_program4_1))
    {
        m_PgmHeader = "!!LWgp4.1\n";
        m_BnfExt = GLRandomTest::ExtLW_gpu_program4_1;
    }
    else if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_gpu_program4))
    {
        m_PgmHeader = "!!LWgp4.0\n";
        m_BnfExt = GLRandomTest::ExtLW_gpu_program4_0;
    }
    else
        MASSERT(!"OpenGL extensions dont support geometry programs!");
}

//------------------------------------------------------------------------------
void GmGpuPgmGen::InitMailwars
(
    Block * pMainBlock
)
{
    // Initialize our registers array:
    for (int reg = 0; reg < RND_GPU_PROG_GM_REG_NUM_REGS; reg++)
    {
        RegState v;
        v.Id = reg;

        // Note: ReadableMask, WriteableMask, WrittenMask and Scores
        // are all 0 unless overridden below!
        //
        // InitReadOnlyReg also sets ReadableMask and WrittenMask.

        switch (reg)
        {
            case RND_GPU_PROG_GM_REG_vHPOS:
            case RND_GPU_PROG_GM_REG_vCOL0:
            case RND_GPU_PROG_GM_REG_vCOL1:
            case RND_GPU_PROG_GM_REG_vBFC0:
            case RND_GPU_PROG_GM_REG_vBFC1:
                v.Attrs |= regIn;
                v.InitReadOnlyReg(50, compXYZW);
                break;

            case RND_GPU_PROG_GM_REG_vFOGC:
            case RND_GPU_PROG_GM_REG_vPSIZ:
                v.Attrs |= regIn;
                v.InitReadOnlyReg(50, compX);
                break;

            case RND_GPU_PROG_GM_REG_vTEX0:
            case RND_GPU_PROG_GM_REG_vTEX1:
            case RND_GPU_PROG_GM_REG_vTEX2:
            case RND_GPU_PROG_GM_REG_vTEX3:
            case RND_GPU_PROG_GM_REG_vTEX4:
            case RND_GPU_PROG_GM_REG_vTEX5:
            case RND_GPU_PROG_GM_REG_vTEX6:
            case RND_GPU_PROG_GM_REG_vTEX7:
                if (reg - RND_GPU_PROG_GM_REG_vTEX0 < m_pGLRandom->NumTxCoords())
                {
                    v.Attrs |= regIn;
                    v.InitReadOnlyReg(40, compXYZW);
                    v.Score[1]    = 30;
                    v.Score[2]    = 20;
                    v.Score[3]    = 10;
                }
                break;

            case RND_GPU_PROG_GM_REG_vNRML:
                v.Attrs |= regIn;
                v.InitReadOnlyReg(50, compXYZ);
                break;

            case RND_GPU_PROG_GM_REG_vCLP0:
            case RND_GPU_PROG_GM_REG_vCLP1:
            case RND_GPU_PROG_GM_REG_vCLP2:
            case RND_GPU_PROG_GM_REG_vCLP3:
            case RND_GPU_PROG_GM_REG_vCLP4:
            case RND_GPU_PROG_GM_REG_vCLP5:
                v.Attrs |= regIn;
                v.InitReadOnlyReg(0, compX);
                break;

            case RND_GPU_PROG_GM_REG_vID:
            case RND_GPU_PROG_GM_REG_pID:
                v.Attrs |= regIn;
                v.InitReadOnlyReg(5, compX);
                break;

            case RND_GPU_PROG_GM_REG_pILWO:
                if (m_NumIlwocations > 1)
                {
                    // Not a regIn or regOut.
                    v.InitReadOnlyReg(5, compX);
                }
                break;

            case RND_GPU_PROG_GM_REG_oHPOS:
                //We will handle this reg in MainSequenceEnd()
                v.Attrs        |= regOut;
                v.WriteableMask = compXYZW;
                break;

            case RND_GPU_PROG_GM_REG_oCOL0:
            case RND_GPU_PROG_GM_REG_oCOL1:
            case RND_GPU_PROG_GM_REG_oBFC0:
            case RND_GPU_PROG_GM_REG_oBFC1:
                v.Attrs |= (regOut | regRqd);
                v.WriteableMask = compXYZW;
                break;

            case RND_GPU_PROG_GM_REG_oFOGC:
            case RND_GPU_PROG_GM_REG_oPSIZ:
                v.Attrs |= (regOut | regRqd);
                v.WriteableMask = compX;
                break;

            case RND_GPU_PROG_GM_REG_oTEX0:
            case RND_GPU_PROG_GM_REG_oTEX1:
            case RND_GPU_PROG_GM_REG_oTEX2:
            case RND_GPU_PROG_GM_REG_oTEX3:
            case RND_GPU_PROG_GM_REG_oTEX4:
            case RND_GPU_PROG_GM_REG_oTEX5:
            case RND_GPU_PROG_GM_REG_oTEX6:
            case RND_GPU_PROG_GM_REG_oTEX7:
                if (reg - RND_GPU_PROG_GM_REG_oTEX0 < m_pGLRandom->NumTxCoords())
                {
                    v.Attrs |= (regOut | regRqd);
                    v.WriteableMask = compXYZW;
                }
                break;

            case RND_GPU_PROG_GM_REG_oNRML:
                v.Attrs |= (regOut | regRqd);
                v.WriteableMask = compXYZ;
                break;

            case RND_GPU_PROG_GM_REG_oPID:
            case RND_GPU_PROG_GM_REG_oLAYR:
            case RND_GPU_PROG_GM_REG_oVPORTIDX:
            case RND_GPU_PROG_GM_REG_oVPORTMASK:
            case RND_GPU_PROG_GM_REG_oCLP0:
            case RND_GPU_PROG_GM_REG_oCLP1:
            case RND_GPU_PROG_GM_REG_oCLP2:
            case RND_GPU_PROG_GM_REG_oCLP3:
            case RND_GPU_PROG_GM_REG_oCLP4:
            case RND_GPU_PROG_GM_REG_oCLP5:
                v.Attrs |= regOut;
                v.WriteableMask = compX;
                break;

            case RND_GPU_PROG_GM_REG_R0   :
            case RND_GPU_PROG_GM_REG_R1   :
            case RND_GPU_PROG_GM_REG_R2   :
            case RND_GPU_PROG_GM_REG_R3   :
            case RND_GPU_PROG_GM_REG_R4   :
            case RND_GPU_PROG_GM_REG_R5   :
            case RND_GPU_PROG_GM_REG_R6   :
            case RND_GPU_PROG_GM_REG_R7   :
            case RND_GPU_PROG_GM_REG_R8   :
            case RND_GPU_PROG_GM_REG_R9   :
            case RND_GPU_PROG_GM_REG_R10  :
            case RND_GPU_PROG_GM_REG_R11  :
            case RND_GPU_PROG_GM_REG_R12  :
            case RND_GPU_PROG_GM_REG_R13  :
            case RND_GPU_PROG_GM_REG_R14  :
            case RND_GPU_PROG_GM_REG_R15  :
                v.Attrs |= regTemp;
                v.WriteableMask = compXYZW;
                v.ReadableMask  = compXYZW;
                // Leave WrittenMask and Scores at 0 initially.
                break;

            // Special register to store garbage results that WILL NOT be read by subsequent
            // instructions. Created to support atomic instructions and sanitization code.
            // This register will not affect the scoring code or the random sequence.
            case RND_GPU_PROG_GM_REG_Garbage:
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

            case RND_GPU_PROG_GM_REG_A128:
            case RND_GPU_PROG_GM_REG_A3:
            case RND_GPU_PROG_GM_REG_A1:
            case RND_GPU_PROG_GM_REG_ASubs:
                v.InitIndexReg();
                break;

            case RND_GPU_PROG_GM_REG_Const: // pgmElw[x]
                v.ArrayLen = m_ElwSize;
                v.InitReadOnlyReg(25, compXYZW);
                break;

            case RND_GPU_PROG_GM_REG_ConstVect: // 4-component literal
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

            case RND_GPU_PROG_GM_REG_LocalElw:  // unused!
            case RND_GPU_PROG_GM_REG_PaboElw:   // cbuf[x]
                v.ArrayLen = m_ElwSize;
                v.InitReadOnlyReg(0, compXYZW);
                break;

            case RND_GPU_PROG_GM_REG_Literal:
                v.Attrs |= regLiteral;
                v.InitReadOnlyReg(0, compXYZW);
                break;

            case RND_GPU_PROG_GM_REG_TempArray:
                v.Attrs |= regTemp;
                v.ArrayLen = TempArrayLen;
                if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_gpu_program5))
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

            case RND_GPU_PROG_GM_REG_Subs     :
                v.ArrayLen = MaxSubs;
                v.InitReadOnlyReg(0, compXYZW);
                break;

            default:
                MASSERT(!"Unknown geometry program register");
                break;
        }
        pMainBlock->AddVar(v);
    }
    MASSERT(pMainBlock->NumVars() == RND_GPU_PROG_GM_REG_NUM_REGS);
}

//------------------------------------------------------------------------------
// Initialize the programming environment & program generation state variables.
// Note: Setup the program environment, register attributes, program target,
//       common data types, and interestingness scoring.
//
void GmGpuPgmGen::InitPgmElw(GLRandom::PgmRequirements *pRqmts)
{
    m_NumVtxIn = 0;
    m_NumVtxOut= 0;
    m_IlwtxId    = 0;
    m_NumIlwocations = 0;

    glGetProgramivARB(GL_GEOMETRY_PROGRAM_LW,
                     GL_MAX_PROGRAM_ELW_PARAMETERS_ARB,
                     &m_ElwSize);
    // determine proper pgm header and BNF extension.
    GetHighestSupportedExt();

    InitGenState(
            RND_GPU_PROG_GM_OPCODE_NUM_OPCODES,// total number of opcodes
            NULL,                              // additional opcodes to append
            pRqmts,                            // optional program requirements
            GmPassthruRegs,                    // pointer to lookup table of passthru regs
            true);                             // allow literals
}

// Program environment accessors
//------------------------------------------------------------------------------
//
int GmGpuPgmGen::GetConstReg() const
{
    return RND_GPU_PROG_GM_REG_Const;
}
//------------------------------------------------------------------------------
//
int GmGpuPgmGen::GetPaboReg() const
{
    return RND_GPU_PROG_GM_REG_PaboElw;
}
//------------------------------------------------------------------------------
//
int GmGpuPgmGen::GetSubsReg() const
{
    return RND_GPU_PROG_GM_REG_Subs;
}
//------------------------------------------------------------------------------
int GmGpuPgmGen::GetIndexReg(IndexRegType t) const
{
    switch (t)
    {
        default:        MASSERT(!"Unexpected IndexRegType");
        // fall through

        case tmpA128:   return RND_GPU_PROG_GM_REG_A128;
        case tmpA3:     return RND_GPU_PROG_GM_REG_A3;
        case tmpA1:     return RND_GPU_PROG_GM_REG_A1;
        case tmpASubs:  return RND_GPU_PROG_GM_REG_ASubs;
    }
}
//------------------------------------------------------------------------------
bool GmGpuPgmGen::IsIndexReg(int regId) const
{
    switch (regId)
    {
        case RND_GPU_PROG_GM_REG_A128:
        case RND_GPU_PROG_GM_REG_A3:
        case RND_GPU_PROG_GM_REG_A1:
        case RND_GPU_PROG_GM_REG_ASubs:
            return true;

        default:
            return false;
    }
}
//------------------------------------------------------------------------------
//
int GmGpuPgmGen::GetFirstTempReg() const
{
    return RND_GPU_PROG_GM_REG_R0;
}
//------------------------------------------------------------------------------
int GmGpuPgmGen::GetGarbageReg() const
{
    return RND_GPU_PROG_GM_REG_Garbage;
}
//------------------------------------------------------------------------------
int GmGpuPgmGen::GetLiteralReg() const
{
    return RND_GPU_PROG_GM_REG_Literal;
}
//------------------------------------------------------------------------------
int GmGpuPgmGen::GetFirstTxCdReg() const
{
    return RND_GPU_PROG_GM_REG_vTEX0;
}
//------------------------------------------------------------------------------
int GmGpuPgmGen::GetLastTxCdReg() const
{
    return RND_GPU_PROG_GM_REG_vTEX7;
}
//------------------------------------------------------------------------------
//
int GmGpuPgmGen::GetLocalElwReg() const
{
    return RND_GPU_PROG_GM_REG_LocalElw;
}
//------------------------------------------------------------------------------
//
int GmGpuPgmGen::GetConstVectReg() const
{
    return RND_GPU_PROG_GM_REG_ConstVect;
}

//------------------------------------------------------------------------------
//
int GmGpuPgmGen::GetNumTempRegs() const
{
    return RND_GPU_PROG_GM_REG_R15 - RND_GPU_PROG_GM_REG_R0 + 1;
}
//------------------------------------------------------------------------------
//
int GmGpuPgmGen::GetMinOps()   const
{
    return m_GS.MainBlock.NumRqdUnwritten();
}
//------------------------------------------------------------------------------
//
int GmGpuPgmGen::GetMaxOps()   const
{
    GLint maxOps;
    // This call may not work. I couln'd find it in the spec.
    glGetProgramivARB(GL_GEOMETRY_PROGRAM_LW,
                      GL_MAX_PROGRAM_INSTRUCTIONS_ARB,
                      &maxOps);
    if (mglGetRC() != OK)
        return 0;
    // In practice the usuable number of ops is much lower than the system limit. i.e.
    // if we exceed 300 ops per program the test time is unacceptable.
    return maxOps;
}
//------------------------------------------------------------------------------
//
GLRandom::ProgProperty GmGpuPgmGen::GetProp(int reg) const
{
    MASSERT(reg >= 0 && reg < RND_GPU_PROG_GM_REG_NUM_REGS);
    return (ProgProperty)RegToProp[reg];

}
//------------------------------------------------------------------------------
//
int GmGpuPgmGen::GetTxCd(int reg) const      // return a texture coordinate for this register or <0 on error.
{
    if ( reg >= RND_GPU_PROG_GM_REG_vTEX0 && reg <= RND_GPU_PROG_GM_REG_vTEX7)
        return(reg-RND_GPU_PROG_GM_REG_vTEX0);
    else
        return -1;

}
//------------------------------------------------------------------------------
//
string  GmGpuPgmGen::GetRegName(int reg) const
{
    if ( reg >= RND_GPU_PROG_GM_REG_vHPOS && reg <= RND_GPU_PROG_GM_REG_vID)
    {
        MASSERT(m_IlwtxId >= 0 && m_IlwtxId < m_NumVtxIn);
        return Utility::StrPrintf(RegNames[reg],m_IlwtxId);
    }
    else if ( reg >= RND_GPU_PROG_GM_REG_pID && reg < int(NUMELEMS(RegNames)))
        return RegNames[reg];
    else if ( reg == RND_GPU_PROG_REG_none)
        return "";
    else
        return "Unknown";
}

//------------------------------------------------------------------------------
// RandomPicker accessors.
int GmGpuPgmGen::PickAbs() const
{
    return (*m_pPickers)[RND_GPU_PROG_GM_IN_ABS].Pick();
}
//------------------------------------------------------------------------------
//
int GmGpuPgmGen::PickNeg() const
{
    return (*m_pPickers)[RND_GPU_PROG_GM_IN_NEG].Pick();
}
//------------------------------------------------------------------------------
//
int GmGpuPgmGen::PickMultisample() const
{
    return 0;
}

//------------------------------------------------------------------------------
//
int GmGpuPgmGen::PickNumOps() const
{
    return(*m_pPickers)[RND_GPU_PROG_GM_NUM_OPS_PER_VERTEX].Pick();
}
//------------------------------------------------------------------------------
//
int GmGpuPgmGen::PickOp() const
{
    return (*m_pPickers)[RND_GPU_PROG_GM_OPCODE].Pick();
}
//------------------------------------------------------------------------------
//
int GmGpuPgmGen::PickPgmTemplate() const
{
    // Pick a flow-control and randomization theme.
    UINT32 t = (*m_pPickers)[RND_GPU_PROG_GM_TEMPLATE].Pick();
    if (t >= RND_GPU_PROG_TEMPLATE_NUM_TEMPLATES)
    {
        t = RND_GPU_PROG_TEMPLATE_Simple;
    }
    return(t);

}
//------------------------------------------------------------------------------
//
int GmGpuPgmGen::PickRelAddr() const
{
    return (*m_pPickers)[RND_GPU_PROG_GM_RELADDR].Pick();
}
//------------------------------------------------------------------------------
//
int GmGpuPgmGen::PickResultReg() const
{
    return (*m_pPickers)[RND_GPU_PROG_GM_OUT_REG].Pick();
}
//------------------------------------------------------------------------------
//
int GmGpuPgmGen::PickTempReg() const
{
    return( m_pFpCtx->Rand.GetRandom(RND_GPU_PROG_GM_REG_R0,
                                     RND_GPU_PROG_GM_REG_R15));

}

//------------------------------------------------------------------------------
int GmGpuPgmGen::PickTxOffset() const
{
    return (*m_pPickers)[RND_GPU_PROG_GM_TX_OFFSET].Pick();
}

//------------------------------------------------------------------------------
//
int GmGpuPgmGen::PickTxTarget() const
{
    return (*m_pPickers)[RND_GPU_PROG_GM_TX_DIM].Pick();
}

//------------------------------------------------------------------------------
void GmGpuPgmGen::OptionSequence()
{
    if (m_pGLRandom->GetNumLayersInFBO() > 0 && m_ThisProgWritesLayerVport)
    {
        PgmOptionAppend("OPTION ARB_viewport_array;\n");
        if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_viewport_array2) &&
            !m_pGLRandom->m_GpuPrograms.GetIsLwrrVportIndexed()) // for using result.viewportmask
        {
            PgmOptionAppend("OPTION LW_viewport_array2;\n");
        }
    }
    if (IsLwrrGMProgPassThru())
    {
        PgmOptionAppend("OPTION LW_geometry_shader_passthrough;\n");
    }

    GpuPgmGen::OptionSequence();
}

//-----------------------------------------------------------------------------
// This actually gets called after MainSequenceEnd() but allows use to put
// statements in the program string before the main body.
void GmGpuPgmGen::DeclSequence()
{
    PgmAppend(Utility::StrPrintf("PRIMITIVE_IN %s;\n",
            PrimitiveToString(m_GS.Prog.PgmRqmt.PrimitiveIn).c_str()));

    if (!IsLwrrGMProgPassThru())
    {
        PgmAppend(Utility::StrPrintf("PRIMITIVE_OUT %s;\n",
                PrimitiveToString(m_GS.Prog.PgmRqmt.PrimitiveOut).c_str()));
        PgmAppend(Utility::StrPrintf("VERTICES_OUT %d;\n", m_NumVtxOut));
        AddPgmComment(Utility::StrPrintf("Num ilwocations:%d",
                m_NumIlwocations).c_str());
        if (m_NumIlwocations > 1)
        {
            PgmAppend(Utility::StrPrintf("ILWOCATIONS %d;\n", m_NumIlwocations));
        }
    }

    GpuPgmGen::DeclSequence();
}

//----------------------------------------------------------------------------
// Override the default implementation of StatementSequence to account for
// the number of primitives/vertices we have to emit.
// Each vertex we emit must meet the downstream program requirements.
// When increasing the number of vertices we output the new vertex's position
// needs to be interpolated and perturbed to generate something interesting
// on the display. The basic rules are:
// 1. If VerticesOut <= VerticesIn, then use the VerticesIn position
// 2. If VerticesOut > VerticesIn, then use the VerticesIn position and
//    interpolate/perturb any new vertex.
void GmGpuPgmGen::StatementSequence()
{
    // Pass through GM shaders can only declare passthrough regs
    // and output per primitive variables
    if (IsLwrrGMProgPassThru())
    {
        PgmAppend("PASSTHROUGH result.position;\n");
        const int numvars = m_GS.MainBlock.NumVars();
        for (int iv = 0; iv < numvars; ++iv)
        {
            RegState v = m_GS.MainBlock.GetVar(iv);
            if (v.Attrs & regRqd)
            {
                PgmAppend (Utility::StrPrintf("PASSTHROUGH %s;\n", GetRegName(iv).c_str()));
                v.WrittenMask = v.WriteableMask;
                m_GS.MainBlock.UpdateVar(v);
            }
        }

        if (m_ThisProgWritesLayerVport)
        {
            AddLayeredOutputRegs(RND_GPU_PROG_GM_REG_oLAYR,
                 RND_GPU_PROG_GM_REG_oVPORTIDX, RND_GPU_PROG_GM_REG_oVPORTMASK);
        }
        return;
    }

    const bool IsMainBlock = (m_GS.Blocks.top() == &m_GS.MainBlock);
    if (!IsMainBlock)
    {
        return GpuPgmGen::StatementSequence();
    }
    const int opsPerOutVtx = m_GS.MainBlock.StatementsLeft();

    int vtxPerPrim;     // number of vertices per primitive to process
    int newVtxLeft;     // a pool of new vertices to emit
    // If out < in then passthru all vertex positions.
    if ( m_NumVtxOut < m_NumVtxIn)
    {
        vtxPerPrim = m_NumVtxOut;
        newVtxLeft = 0;
    }
    else
    {
        vtxPerPrim = m_NumVtxIn;
        newVtxLeft = m_NumVtxOut - m_NumVtxIn;
    }

    // determine how to deal with the HPOS register on new vertices
    const int action = (vtxPerPrim > 1) ? actInterpOffset : actOffset;

    for (m_IlwtxId = 0; m_IlwtxId < vtxPerPrim; m_IlwtxId++)
    {
        // Leave the original vertex intact
        DoVertex(opsPerOutVtx + m_GS.MainBlock.StatementCount(),
                 actPassThru, 1.0);

        // Pick a random number of vertices from the pool to  interpolate.
        const int newVtxHere = (vtxPerPrim < 2 || m_IlwtxId == vtxPerPrim-2) ?
            newVtxLeft :
            m_pFpCtx->Rand.GetRandom(0, newVtxLeft);

        newVtxLeft -= newVtxHere;
        const float blend = 1.0f/(newVtxHere+1.0f);

        // Create the instructions to EMIT a new vertex
        for ( int i=1; i <= newVtxHere; i++)
        {
            DoVertex(opsPerOutVtx + m_GS.MainBlock.StatementCount(),
                     action, blend*i);
        }
    }
    m_GS.Blocks.top()->AddStatement("ENDPRIM;");
}

//----------------------------------------------------------------------------
void GmGpuPgmGen::DoVertex(int numStatements, int action, float blendFactor)
{
    m_GS.MainBlock.MarkAllOutputsUnwritten();
    m_GS.MainBlock.SetTargetStatementCount(numStatements);

    if (m_GS.Prog.PgmRqmt.Outputs.count(ppPtSize))
    {
        // Treat point-size as passthru to avoid huge random
        // pointsizes that cause performance problems.
        UtilCopyReg(RND_GPU_PROG_GM_REG_oPSIZ, RND_GPU_PROG_GM_REG_vPSIZ);
    }

    GpuPgmGen::StatementSequence();
    StatementSequenceEnd(action, blendFactor);
}

//----------------------------------------------------------------------------
// Handle any special processing prior to the "EMIT" instruction.
void GmGpuPgmGen::StatementSequenceEnd(int Action, float Blend)
{
    UtilProcessPassthruRegs();

    // Special processing of the oHPOS register

    const int tmpPos = GetFirstTempReg();
    const int tmpReg = tmpPos + 1;

    // MOV TmpR1, vertex[%d].position (m_IlwtxId)
    UtilCopyReg(tmpPos, RND_GPU_PROG_GM_REG_vHPOS);

    switch (Action)
    {
        case actPassThru:
            break;
        case actInterpOffset:
        {
            float blendVect[4] = { Blend, Blend, Blend, 1.0f};
            // MOV tmpR2, blendVect
            UtilCopyConstVect(tmpReg, blendVect);

            // LRP TmpR1, TmpR1, TmpR2, vertex[%d].position (m_IlwtxId+1)
            m_IlwtxId++;
            UtilTriOp(RND_GPU_PROG_OPCODE_lrp,
                      tmpPos,
                      tmpPos,
                      tmpReg,
                      RND_GPU_PROG_GM_REG_vHPOS);
            m_IlwtxId--;
        }
        // fall through!
        case actOffset:
        {
            float perturbVect[4] = {
                m_pFpCtx->Rand.GetRandomFloat(0.995,1.005),
                m_pFpCtx->Rand.GetRandomFloat(0.995,1.005),
                m_pFpCtx->Rand.GetRandomFloat(0.995,1.005),
                1.0f
            };
            // MUL TmpR1, TmpR1, {.095-1.005, .095-1.005,.095-1.005,1};
            UtilBinOp(RND_GPU_PROG_OPCODE_mul, tmpPos, tmpPos, perturbVect);
        }
        break;

        default: MASSERT(!"Unexepected HPOS action");
            break;
    }
    if (m_NumIlwocations > 1)
    {
        PgmInstruction Op;
        // "I2F TmpR2.x, primitive.invocation.x;"
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_i2f, tmpReg, compX, dtNA);
        UtilOpInReg(&Op, RND_GPU_PROG_GM_REG_pILWO, scX);
        UtilOpCommit(&Op);

        // "MUL TmpR2.x, TmpR2.x, -0.05;"
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mul, tmpReg, compX, dtNA);
        UtilOpInReg(&Op, tmpReg, scX);
        UtilOpInConstF32(&Op, -0.05F);
        UtilOpCommit(&Op);

        // "MAD TmpR1.xyz, TmpR2.x, TmpR1.x, TmpR1;"
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mad, tmpPos, compXYZ, dtNA);
        UtilOpInReg(&Op, tmpReg, scX);
        UtilOpInReg(&Op, tmpPos, scX);
        UtilOpInReg(&Op, tmpPos);
        UtilOpCommit(&Op);
    }

    // Assign each primitive on a different layer and viewport
    if (m_ThisProgWritesLayerVport)
    {
        AddLayeredOutputRegs(RND_GPU_PROG_GM_REG_oLAYR,
                 RND_GPU_PROG_GM_REG_oVPORTIDX, RND_GPU_PROG_GM_REG_oVPORTMASK);
    }

    // MOV result[n].position, TmpR1;
    UtilCopyReg(RND_GPU_PROG_GM_REG_oHPOS, tmpPos);

    m_GS.Blocks.top()->AddStatement("EMIT;");
}

//------------------------------------------------------------------------------
// Random geometry programs tend to read every possible vertex attribute.
// We'd rather they sometimes left the input vertex size smaller.
//
// So prefer vertex attributes we've already read to ones not before read
// in this program.
//
// Increase the preference for previously-used attribs as we get further on
// in the geometry program (i.e. no pref on first vertex).
//
UINT32 GmGpuPgmGen::CalcScore(PgmInstruction *pOp, int reg, int argIdx)
{
    const UINT32 Score = GpuPgmGen::CalcScore(pOp, reg, argIdx);

    const RegState & v = GetVar(reg);
    if ((v.Attrs & regIn) && !(v.Attrs & regUsed))
    {
        return Score / (2 + m_IlwtxId);
    }

    return Score;
}

