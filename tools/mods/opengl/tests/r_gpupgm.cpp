/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// BnfGenerator         vertex, geometry and fragment program generator for
//                      GLRandom. This class strickly follows the Bakus-Nuar
//                      form to specify the grammar.
//
// 45678901234567890123456789012345678901234567890123456789012345678901234567890
#include "glrandom.h"  // declaration of our namespace
#include "pgmgen.h"
#include "core/include/massert.h"
#include "gpu/include/gpusbdev.h"
#include <math.h>
#include <string>

#include "core/include/xp.h"
#include "core/include/utility.h"
#include "core/include/fileholder.h"
#include "core/include/errormap.h"
#include "core/include/platform.h"

using namespace GLRandom;

const char * const PgmGen::pkNicknames[PgmKind_NUM] =
{
    "vx"    // pkVertex
    ,"tc"   // pkTessCtrl
    ,"te"   // pkTessEval
    ,"gm"   // pkGeometry
    ,"fr"   // pkFragment
};

const char * const PgmGen::pkNicknamesUC[PgmKind_NUM] =
{
    "VX"    // pkVertex
    ,"TC"   // pkTessCtrl
    ,"TE"   // pkTessEval
    ,"GM"   // pkGeometry
    ,"FR"   // pkFragment
};
const char * const PgmGen::pkNames[PgmKind_NUM] =
{
    "Vertex"    // pkVertex
    ,"TessCtrl"   // pkTessCtrl
    ,"TessEval"   // pkTessEval
    ,"Geometry"   // pkGeometry
    ,"Fragment"   // pkFragment
};
const GLenum PgmGen::pkTargets[PgmKind_NUM] =
{
    GL_VERTEX_PROGRAM_LW            // pkVertex
    ,GL_TESS_CONTROL_PROGRAM_LW     // pkTessCtrl
    ,GL_TESS_EVALUATION_PROGRAM_LW  // pkTessEval
    ,GL_GEOMETRY_PROGRAM_LW         // pkGeometry
    ,GL_FRAGMENT_PROGRAM_LW         // pkFragment
};
const GLRandomTest::ExtensionId PgmGen::pkExtensions[PgmKind_NUM] =
{
    GLRandomTest::ExtLW_vertex_program3           // pkVertex
    ,GLRandomTest::ExtLW_tessellation_program5    // pkTessCtrl
    ,GLRandomTest::ExtLW_tessellation_program5    // pkTessEval
    ,GLRandomTest::ExtLW_gpu_program4             // pkGeometry
    ,GLRandomTest::ExtLW_fragment_program2        // pkFragment
};
const UINT32 PgmGen::pkPickEnable[PgmKind_NUM] =
{
    RND_GPU_PROG_VX_ENABLE  // pkVertex
    ,RND_GPU_PROG_TC_ENABLE // pkTessCtrl
    ,RND_GPU_PROG_TE_ENABLE // pkTessEval
    ,RND_GPU_PROG_GM_ENABLE // pkGeometry
    ,RND_GPU_PROG_FR_ENABLE // pkFragment
};
const UINT32 PgmGen::pkPickPgmIdx[PgmKind_NUM] =
{
    RND_GPU_PROG_VX_INDEX  // pkVertex
    ,RND_GPU_PROG_TC_INDEX // pkTessCtrl
    ,RND_GPU_PROG_TE_INDEX // pkTessEval
    ,RND_GPU_PROG_GM_INDEX // pkGeometry
    ,RND_GPU_PROG_FR_INDEX // pkFragment
};
const UINT32 PgmGen::pkPickPgmIdxDebug[PgmKind_NUM] =
{
    RND_GPU_PROG_VX_INDEX_DEBUG  // pkVertex
    ,RND_GPU_PROG_TC_INDEX_DEBUG // pkTessCtrl
    ,RND_GPU_PROG_TE_INDEX_DEBUG // pkTessEval
    ,RND_GPU_PROG_GM_INDEX_DEBUG // pkGeometry
    ,RND_GPU_PROG_FR_INDEX_DEBUG // pkFragment
};

// We don't create random TessCtrl or TessEval shaders... yet.
const GLenum PgmGen::PaBOTarget[PgmKind_NUM] =
{
    GL_VERTEX_PROGRAM_PARAMETER_BUFFER_LW
    ,0 //,GL_TESS_CONTROL_PROGRAM_PARAMETER_BUFFER_LW
    ,0 //,GL_TESS_EVALUATION_PROGRAM_PARAMETER_BUFFER_LW
    ,GL_GEOMETRY_PROGRAM_PARAMETER_BUFFER_LW
    ,GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW
};

// Lookup table map Texture Handle indices to Texture attributes and visa-versa
GLRandom::pgmKind PgmGen::TargetToPgmKind (GLenum Target)
{
    int i;
    for (i = 0; i < PgmKind_NUM; i++)
    {
        if (PgmGen::pkTargets[i] == Target)
            return static_cast<GLRandom::pgmKind>(i);
    }

    MASSERT(!"Unknown Target for pgmKind");

    return pkVertex;  // Wrong, but safer than an out-of-bounds value.
}

//------------------------------------------------------------------------------
// RndGpuPrograms implementation
//
int RndGpuPrograms::EndStrategy() const
{
    Program *pLwr = m_Progs[pkFragment].Lwr();
    if (pLwr)
    {
        return pLwr->PgmRqmt.EndStrategy;
    }
    return RND_GPU_PROG_FR_END_STRATEGY_unknown;
}

//------------------------------------------------------------------------------
void RndGpuPrograms::GetXfbAttrFlags(UINT32 *pXfbStdAttr, UINT32 *pXfbUsrAttr)
{
    Program *pLwr = NULL;
    for (int pk = pkGeometry; pk >= pkVertex; pk--)
    {
        if (m_State[pk].Enable)
        {
            pLwr = m_Progs[pk].Lwr();
            break;
        }
    }

    if( pLwr != NULL)
    {
        *pXfbStdAttr = pLwr->XfbStdAttrib;
        *pXfbUsrAttr = pLwr->XfbUsrAttrib;
    }
    else // Ask RndVertexes for its XFB attributes
    {
        m_pGLRandom->m_Vertexes.GetXfbAttrFlags(pXfbStdAttr,pXfbUsrAttr);
    }
}

//------------------------------------------------------------------------------
RndGpuPrograms::~RndGpuPrograms()
{
    for (int i = 0; i < PgmKind_NUM; i++)
    {
        delete m_Gen[i];
    }
    delete [] m_Progs;
}

//------------------------------------------------------------------------------
RndGpuPrograms::RndGpuPrograms
(
    GLRandomTest * pglr
)
 :  GLRandomHelper(pglr, RND_GPU_PROG_NUM_PICKERS, "GpuPrograms")
   ,m_LogRndPgms(false)
   ,m_LogToJsFile(false)
   ,m_StrictPgmLinking(0)
   ,m_VerboseOutput(false)
   ,m_MultisampleNeeded(0)
   ,m_SboMask(0)
   ,m_LwrrentLayer(0)
   ,m_IsLwrrVportIndexed(false)
   ,m_LwrrViewportIdxOrMask(0)
{
    m_Gen[pkVertex]   = new VxGpuPgmGen (pglr, &m_Pickers);
    m_Gen[pkTessCtrl] = new TcGpuPgmGen (pglr, &m_Pickers);
    m_Gen[pkTessEval] = new TeGpuPgmGen (pglr, &m_Pickers);
    m_Gen[pkGeometry] = new GmGpuPgmGen (pglr, &m_Pickers);
    m_Gen[pkFragment] = new FrGpuPgmGen (pglr, &m_Pickers);

    m_Progs = new Programs[PgmKind_NUM];

    for (int i = 0; i < PgmKind_NUM; i++)
    {
        m_Progs[i].SetName(PgmGen::pkNicknames[i]);
    }

    memset( (void*)&m_Sbos[0], 0, sizeof(m_Sbos));
    memset( (void*)&m_PaBO[0], 0, sizeof(m_PaBO));
    memset( (void*)&m_State[0], 0, sizeof(m_State));
    memset( (void*)&m_ExtraState, 0, sizeof(m_ExtraState));
}

//------------------------------------------------------------------------------
void  RndGpuPrograms::SetContext(FancyPicker::FpContext * pFpCtx)
{
    for (int i = 0; i < PgmKind_NUM; i++)
    {
        MASSERT(m_Gen[i]);
        m_Gen[i]->SetContext(pFpCtx);
    }
    GLRandomHelper::SetContext(pFpCtx);
}

//------------------------------------------------------------------------------
RC RndGpuPrograms::SetDefaultPicker(int picker)
{
    switch (picker)
    {
        // RndGpuPrograms Pickers
        default:
            return RC::BAD_PICKER_INDEX;

        case RND_GPU_PROG_LP_PICK_PGMS:
            // do/don't pick a new program on this loop.
            m_Pickers[RND_GPU_PROG_LP_PICK_PGMS].ConfigConst(1);
            break;

        case RND_GPU_PROG_LOG_RND_PGMS:
            // Log all randomly generated programs to individual files.
            m_Pickers[RND_GPU_PROG_LOG_RND_PGMS].ConfigConst(0);
            break;

        case RND_GPU_PROG_LOG_TO_JS_FILE:
            // Log all randomly generated programs to individual files.
            m_Pickers[RND_GPU_PROG_LOG_TO_JS_FILE].ConfigConst(0);
            break;

        case RND_GPU_PROG_OPCODE_DATA_TYPE:
            // get the common OpCode data type to be used by all programs.
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem(10, dtNA);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtF16);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtF32);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 3, dtF64);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtS8);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtS16);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtS24);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtS32);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtS32_HI);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtS64);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtU8);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtU16);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtU24);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtU32);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtU32_HI);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtU64);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtF64X2);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtF64X4);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtS64X2);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtS64X4);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtU64X2);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtU64X4);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtF32X2);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtF32X4);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtS32X2);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtS32X4);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtU32X2);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtU32X4);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtF16X4a);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].AddRandItem( 1, dtF16X2a);
            m_Pickers[RND_GPU_PROG_OPCODE_DATA_TYPE].CompileRandom();
            break;

        case RND_GPU_PROG_ATOMIC_MODIFIER:
            m_Pickers[RND_GPU_PROG_ATOMIC_MODIFIER].ConfigRandom();
            m_Pickers[RND_GPU_PROG_ATOMIC_MODIFIER].AddRandItem( 1,amAdd);
            m_Pickers[RND_GPU_PROG_ATOMIC_MODIFIER].AddRandItem( 1,amMin);
            m_Pickers[RND_GPU_PROG_ATOMIC_MODIFIER].AddRandItem( 1,amMax);
            m_Pickers[RND_GPU_PROG_ATOMIC_MODIFIER].AddRandItem( 1,amIwrap);
            m_Pickers[RND_GPU_PROG_ATOMIC_MODIFIER].AddRandItem( 1,amDwrap);
            m_Pickers[RND_GPU_PROG_ATOMIC_MODIFIER].AddRandItem( 1,amAnd);
            m_Pickers[RND_GPU_PROG_ATOMIC_MODIFIER].AddRandItem( 1,amOr);
            m_Pickers[RND_GPU_PROG_ATOMIC_MODIFIER].AddRandItem( 1,amXor);
            m_Pickers[RND_GPU_PROG_ATOMIC_MODIFIER].AddRandItem( 1,amExch);
            m_Pickers[RND_GPU_PROG_ATOMIC_MODIFIER].AddRandItem( 1,amCSwap);
            m_Pickers[RND_GPU_PROG_ATOMIC_MODIFIER].CompileRandom();
            break;

        case RND_GPU_PROG_VERBOSE_OUTPUT:
            m_Pickers[RND_GPU_PROG_VERBOSE_OUTPUT].ConfigConst(0);
            break;

        case RND_GPU_PROG_STRICT_PGM_LINKING:
            m_Pickers[RND_GPU_PROG_STRICT_PGM_LINKING].ConfigConst(0);
            break;

        case RND_GPU_PROG_USE_BINDLESS_TEXTURES:
            m_Pickers[RND_GPU_PROG_USE_BINDLESS_TEXTURES].ConfigRandom();
            m_Pickers[RND_GPU_PROG_USE_BINDLESS_TEXTURES].AddRandItem(7, GL_FALSE);
            m_Pickers[RND_GPU_PROG_USE_BINDLESS_TEXTURES].AddRandItem(3, GL_TRUE);
            m_Pickers[RND_GPU_PROG_USE_BINDLESS_TEXTURES].CompileRandom();
            break;

        case RND_GPU_PROG_IS_VPORT_SELECT_AS_INDEXED:
            m_Pickers[RND_GPU_PROG_IS_VPORT_SELECT_AS_INDEXED].ConfigRandom();
            m_Pickers[RND_GPU_PROG_IS_VPORT_SELECT_AS_INDEXED].AddRandItem(8, GL_FALSE);
            m_Pickers[RND_GPU_PROG_IS_VPORT_SELECT_AS_INDEXED].AddRandItem(2, GL_TRUE);
            m_Pickers[RND_GPU_PROG_IS_VPORT_SELECT_AS_INDEXED].CompileRandom();
            break;

        case RND_GPU_PROG_LAYER_OUT:
            m_Pickers[RND_GPU_PROG_LAYER_OUT].ConfigRandom();
            m_Pickers[RND_GPU_PROG_LAYER_OUT].AddRandItem(75, 0);
            if (m_pGLRandom->GetNumLayersInFBO())
            {
                m_Pickers[RND_GPU_PROG_LAYER_OUT].AddRandRange(25, 1,
                                             m_pGLRandom->GetNumLayersInFBO() - 1);
            }
            m_Pickers[RND_GPU_PROG_LAYER_OUT].CompileRandom();
            break;

        case RND_GPU_PROG_VPORT_IDX:
            m_Pickers[RND_GPU_PROG_VPORT_IDX].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VPORT_IDX].AddRandItem(10, 0);
            m_Pickers[RND_GPU_PROG_VPORT_IDX].AddRandRange(90, 1, MAX_VIEWPORTS - 1);
            m_Pickers[RND_GPU_PROG_VPORT_IDX].CompileRandom();
            break;

        case RND_GPU_PROG_VPORT_MASK:
            m_Pickers[RND_GPU_PROG_VPORT_MASK].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VPORT_MASK].AddRandItem(75, 0xF); //All viewports
            m_Pickers[RND_GPU_PROG_VPORT_MASK].AddRandRange(25, 1, (1<<MAX_VIEWPORTS)-2); // A mask of other viewports
            m_Pickers[RND_GPU_PROG_VPORT_MASK].CompileRandom();
            break;

        // VxGpuPgmGen Pickers
        case RND_GPU_PROG_VX_ENABLE:
            // enable vertex program
            m_Pickers[RND_GPU_PROG_VX_ENABLE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_ENABLE].AddRandItem(4, GL_FALSE);
            m_Pickers[RND_GPU_PROG_VX_ENABLE].AddRandItem(6, GL_TRUE);
            m_Pickers[RND_GPU_PROG_VX_ENABLE].CompileRandom();
            break;

        case RND_GPU_PROG_VX_CLAMP_COLOR_ENABLE:
            m_Pickers[RND_GPU_PROG_VX_CLAMP_COLOR_ENABLE].ConfigConst(0);
            break;

        case RND_GPU_PROG_VX_INDEX:
            // Which vertex program to use
            m_Pickers[RND_GPU_PROG_VX_INDEX].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_INDEX].AddRandRange(1, 0, RND_GPU_PROG_VX_INDEX_NumFixedProgs-1);
            m_Pickers[RND_GPU_PROG_VX_INDEX].AddRandItem (2, psRandom);
            m_Pickers[RND_GPU_PROG_VX_INDEX].CompileRandom();
            break;

        case RND_GPU_PROG_VX_NUMRANDOM:
            // how many random vertex programs to generate per frame
            m_Pickers[RND_GPU_PROG_VX_NUMRANDOM].ConfigConst(20);
            break;

        case RND_GPU_PROG_VX_TEMPLATE:
            // which template to use for randomly generated vertex programs?
            m_Pickers[RND_GPU_PROG_VX_TEMPLATE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_TEMPLATE].AddRandItem(70,RND_GPU_PROG_TEMPLATE_Simple);
            m_Pickers[RND_GPU_PROG_VX_TEMPLATE].AddRandItem(10,RND_GPU_PROG_TEMPLATE_Call);
            m_Pickers[RND_GPU_PROG_VX_TEMPLATE].AddRandItem(10,RND_GPU_PROG_TEMPLATE_Flow);
            m_Pickers[RND_GPU_PROG_VX_TEMPLATE].AddRandItem(10,RND_GPU_PROG_TEMPLATE_CallIndexed);
            m_Pickers[RND_GPU_PROG_VX_TEMPLATE].CompileRandom();
            break;

        case RND_GPU_PROG_VX_NUM_OPS:
            // how many instructions long should a random vxpgm be?
            m_Pickers[RND_GPU_PROG_VX_NUM_OPS].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_NUM_OPS].AddRandRange(90, 25, 40);
            m_Pickers[RND_GPU_PROG_VX_NUM_OPS].AddRandRange(10, 10, 100);
            m_Pickers[RND_GPU_PROG_VX_NUM_OPS].CompileRandom();
            break;

        case RND_GPU_PROG_VX_OPCODE:
            // random vertex program instruction
            m_Pickers[RND_GPU_PROG_VX_OPCODE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_abs    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_add    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_and    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(0, RND_GPU_PROG_OPCODE_brk    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_cal    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_cei    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_cmp    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(0, RND_GPU_PROG_OPCODE_con    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_cos    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_div    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_dp2    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_dp2a   );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_dp3    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_dp4    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_dph    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_dst    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(0, RND_GPU_PROG_OPCODE_else   );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(0, RND_GPU_PROG_OPCODE_endif  );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(0, RND_GPU_PROG_OPCODE_endrep );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_ex2    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_flr    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_frc    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_i2f    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_if     );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_lg2    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_lit    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_lrp    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_mad    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_max    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_min    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_mod    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_mov    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_mul    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_not    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_nrm    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_or     );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_pk2h   );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_pk2us  );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_pk4b   );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_pk4ub  );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_pow    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_rcc    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_rcp    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_rep    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(0, RND_GPU_PROG_OPCODE_ret    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_rfl    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_round  );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_rsq    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_sad    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_scs    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_seq    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_sfl    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_sge    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_sgt    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_shl    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_shr    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_sin    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_sle    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_slt    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_sne    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_ssg    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_str    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_sub    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_swz    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_tex    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_trunc  );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_txb    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_txd    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_txf    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_txl    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_txp    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(0, RND_GPU_PROG_OPCODE_txq    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_up2h   );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_up2us  );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_up4b   );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_up4ub  );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_x2d    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_xor    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_xpd    );
            // LW_gpu_program4_1 opcodes
            // TODO: Uncomment these lines when EXT_texture_array is implemented
            //m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_tex2   );
            //m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_txb2   );
            //m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_txl2   );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_txg    );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_txfms);
            // LW_gpu_program5 opcodes
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_bfe  );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_bfi  );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_bfr  );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_btc  );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_btfl );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_btfm );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_txgo );
            // Waiting on data types.
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_cvt  );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_pk64 );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_up64 );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_ldc );
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_loadim);
            m_Pickers[RND_GPU_PROG_VX_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_storeim);

            m_Pickers[RND_GPU_PROG_VX_OPCODE].CompileRandom();
            break;

        case RND_GPU_PROG_VX_OUT_TEST:
            // CC test for conditional result writes.
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem(10, RND_GPU_PROG_OUT_TEST_none);
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_eq  );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 2, RND_GPU_PROG_OUT_TEST_ge  );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 2, RND_GPU_PROG_OUT_TEST_gt  );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 2, RND_GPU_PROG_OUT_TEST_le  );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 2, RND_GPU_PROG_OUT_TEST_lt  );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_ne  );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_tr  );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_fl  );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_nan );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_leg );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_cf  );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_ncf );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_of  );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_nof );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_ab  );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_ble );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_sf  );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_nsf );
            m_Pickers[RND_GPU_PROG_VX_OUT_TEST].CompileRandom();
            break;

        case RND_GPU_PROG_VX_WHICH_CC:
            // do/don't update the CC (condition code) in an instruction in a random vertex program
            m_Pickers[RND_GPU_PROG_VX_WHICH_CC].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_WHICH_CC].AddRandItem(1, RND_GPU_PROG_CCnone);
            m_Pickers[RND_GPU_PROG_VX_WHICH_CC].AddRandItem(1, RND_GPU_PROG_CC0);
            m_Pickers[RND_GPU_PROG_VX_WHICH_CC].AddRandItem(1, RND_GPU_PROG_CC1);
            m_Pickers[RND_GPU_PROG_VX_WHICH_CC].CompileRandom();
            break;

        case RND_GPU_PROG_VX_SWIZZLE_ORDER:
            // input or CC swizzle ordering
            m_Pickers[RND_GPU_PROG_VX_SWIZZLE_ORDER].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_SWIZZLE_ORDER].AddRandItem(1, RND_GPU_PROG_SWIZZLE_ORDER_xyzw    );
            m_Pickers[RND_GPU_PROG_VX_SWIZZLE_ORDER].AddRandItem(1, RND_GPU_PROG_SWIZZLE_ORDER_shuffle );
            m_Pickers[RND_GPU_PROG_VX_SWIZZLE_ORDER].AddRandItem(1, RND_GPU_PROG_SWIZZLE_ORDER_random  );
            m_Pickers[RND_GPU_PROG_VX_SWIZZLE_ORDER].CompileRandom();
            break;

        case RND_GPU_PROG_VX_SWIZZLE_SUFFIX:
            // input or CC swizzle format
            m_Pickers[RND_GPU_PROG_VX_SWIZZLE_SUFFIX].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_SWIZZLE_SUFFIX].AddRandItem(1, RND_GPU_PROG_SWIZZLE_SUFFIX_none    );
            m_Pickers[RND_GPU_PROG_VX_SWIZZLE_SUFFIX].AddRandItem(1, RND_GPU_PROG_SWIZZLE_SUFFIX_component );
            m_Pickers[RND_GPU_PROG_VX_SWIZZLE_SUFFIX].AddRandItem(10, RND_GPU_PROG_SWIZZLE_SUFFIX_all  );
            m_Pickers[RND_GPU_PROG_VX_SWIZZLE_SUFFIX].CompileRandom();
            break;

        case RND_GPU_PROG_VX_OUT_REG:
            // output register for random vertex program instruction
            m_Pickers[RND_GPU_PROG_VX_OUT_REG].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_OUT_REG].AddRandRange(45, RND_GPU_PROG_VX_REG_R0, RND_GPU_PROG_VX_REG_R15 );
            m_Pickers[RND_GPU_PROG_VX_OUT_REG].AddRandItem(  5, RND_GPU_PROG_VX_REG_TempArray );
            m_Pickers[RND_GPU_PROG_VX_OUT_REG].AddRandRange( 3, RND_GPU_PROG_VX_REG_oCLP0, RND_GPU_PROG_VX_REG_oCLP5 );
            m_Pickers[RND_GPU_PROG_VX_OUT_REG].AddRandRange( 3, RND_GPU_PROG_VX_REG_oTEX0, RND_GPU_PROG_VX_REG_oTEX7 );
            m_Pickers[RND_GPU_PROG_VX_OUT_REG].AddRandItem(  1, RND_GPU_PROG_VX_REG_oCOL0 );
            m_Pickers[RND_GPU_PROG_VX_OUT_REG].AddRandItem(  1, RND_GPU_PROG_VX_REG_oCOL1 );
            m_Pickers[RND_GPU_PROG_VX_OUT_REG].AddRandItem(  1, RND_GPU_PROG_VX_REG_oBFC0 );
            m_Pickers[RND_GPU_PROG_VX_OUT_REG].AddRandItem(  1, RND_GPU_PROG_VX_REG_oBFC1 );
            m_Pickers[RND_GPU_PROG_VX_OUT_REG].AddRandItem(  1, RND_GPU_PROG_VX_REG_oFOGC );
            m_Pickers[RND_GPU_PROG_VX_OUT_REG].AddRandItem(  1, RND_GPU_PROG_VX_REG_oNRML );
            m_Pickers[RND_GPU_PROG_VX_OUT_REG].CompileRandom();
            break;

        case RND_GPU_PROG_VX_OUT_MASK:
            // output register component mask
            m_Pickers[RND_GPU_PROG_VX_OUT_MASK].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_OUT_MASK].AddRandItem (4, 0xf);
            m_Pickers[RND_GPU_PROG_VX_OUT_MASK].AddRandRange(1, 1, 0xf);
            m_Pickers[RND_GPU_PROG_VX_OUT_MASK].CompileRandom();
            break;

        case RND_GPU_PROG_VX_SATURATE:
            // do/don't  saturate output (clamp to [0,1], or [-1,1]).
            m_Pickers[RND_GPU_PROG_VX_SATURATE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_SATURATE].AddRandItem (90, RND_GPU_PROG_OP_SAT_none);
            m_Pickers[RND_GPU_PROG_VX_SATURATE].AddRandItem ( 5, RND_GPU_PROG_OP_SAT_unsigned);
            m_Pickers[RND_GPU_PROG_VX_SATURATE].AddRandItem ( 5, RND_GPU_PROG_OP_SAT_signed);
            m_Pickers[RND_GPU_PROG_VX_SATURATE].CompileRandom();
            break;

        case RND_GPU_PROG_VX_RELADDR:
            // do/don't use addr-reg  addressing for reg reads
            m_Pickers[RND_GPU_PROG_VX_RELADDR].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_RELADDR].AddRandItem (1, GL_FALSE);
            m_Pickers[RND_GPU_PROG_VX_RELADDR].AddRandItem (1, GL_TRUE);
            m_Pickers[RND_GPU_PROG_VX_RELADDR].CompileRandom();
            break;

        case RND_GPU_PROG_VX_IN_ABS:
            // do/don't take abs. value of input argument
            m_Pickers[RND_GPU_PROG_VX_IN_ABS].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_IN_ABS].AddRandItem (3, GL_FALSE);
            m_Pickers[RND_GPU_PROG_VX_IN_ABS].AddRandItem (1, GL_TRUE);
            m_Pickers[RND_GPU_PROG_VX_IN_ABS].CompileRandom();
            break;

        case RND_GPU_PROG_VX_IN_NEG:
            // do/don't negate input argument
            m_Pickers[RND_GPU_PROG_VX_IN_NEG].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_IN_NEG].AddRandItem (3, RND_GPU_PROG_OP_SIGN_none);
            m_Pickers[RND_GPU_PROG_VX_IN_NEG].AddRandItem (1, RND_GPU_PROG_OP_SIGN_neg);
            m_Pickers[RND_GPU_PROG_VX_IN_NEG].CompileRandom();
            break;

        case RND_GPU_PROG_VX_TX_DIM:
            // Which texture attribute dim (1d/2d/lwbe/3d) to fetch from.
            // LW40 only supports 1D/2D
            m_Pickers[RND_GPU_PROG_VX_TX_DIM].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_TX_DIM].AddRandItem(5, Is2d);
            m_Pickers[RND_GPU_PROG_VX_TX_DIM].AddRandItem(1, Is3d);
            m_Pickers[RND_GPU_PROG_VX_TX_DIM].AddRandItem(2, IsLwbe);
            m_Pickers[RND_GPU_PROG_VX_TX_DIM].AddRandItem(1, Is1d);
            m_Pickers[RND_GPU_PROG_VX_TX_DIM].AddRandItem(1, Is2dMultisample);
            m_Pickers[RND_GPU_PROG_VX_TX_DIM].CompileRandom();
            break;

        case RND_GPU_PROG_VX_TX_OFFSET:
            m_Pickers[RND_GPU_PROG_VX_TX_OFFSET].ConfigConst(toNone);
            break;

        case RND_GPU_PROG_VX_LIGHT0:
            // which "light" to use in certain vertex programs
            m_Pickers[RND_GPU_PROG_VX_LIGHT0].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_LIGHT0].AddRandRange(1, 0, 0xffffffff);
            m_Pickers[RND_GPU_PROG_VX_LIGHT0].CompileRandom();
            break;

        case RND_GPU_PROG_VX_LIGHT1:
            // which "light" to use in certain vertex programs
            m_Pickers[RND_GPU_PROG_VX_LIGHT1].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_LIGHT1].AddRandRange(1, 0, 0xffffffff);
            m_Pickers[RND_GPU_PROG_VX_LIGHT1].CompileRandom();
            break;

        case RND_GPU_PROG_VX_POINTSZ:
            // do/don't generate point size from vertex program
            m_Pickers[RND_GPU_PROG_VX_POINTSZ].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_POINTSZ].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_GPU_PROG_VX_POINTSZ].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_GPU_PROG_VX_POINTSZ].CompileRandom();
            break;

        case RND_GPU_PROG_VX_TWOSIDE:
            // do/don't callwlate two-sided lighting in vertex programs
            m_Pickers[RND_GPU_PROG_VX_TWOSIDE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_TWOSIDE].AddRandItem(2, GL_TRUE);
            m_Pickers[RND_GPU_PROG_VX_TWOSIDE].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_GPU_PROG_VX_TWOSIDE].CompileRandom();
            break;

        case RND_GPU_PROG_VX_POINTSZ_VAL:
            // (float) point size used inside vx progs
            m_Pickers[RND_GPU_PROG_VX_POINTSZ_VAL].FConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_POINTSZ_VAL].FAddRandItem(3, 1.0F);
            m_Pickers[RND_GPU_PROG_VX_POINTSZ_VAL].FAddRandRange(2, 1.0F, 8.0F);
            m_Pickers[RND_GPU_PROG_VX_POINTSZ_VAL].FAddRandRange(1, 1.0F, 64.0F);
            m_Pickers[RND_GPU_PROG_VX_POINTSZ_VAL].CompileRandom();
            break;

        case RND_GPU_PROG_VX_COL0_ALIAS:
            // which vertex attrib will contain input primary color?
            m_Pickers[RND_GPU_PROG_VX_COL0_ALIAS].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_COL0_ALIAS].AddRandItem (20, att_COL0 );
            m_Pickers[RND_GPU_PROG_VX_COL0_ALIAS].AddRandItem ( 5, att_6    );
            m_Pickers[RND_GPU_PROG_VX_COL0_ALIAS].AddRandItem ( 5, att_7    );
            m_Pickers[RND_GPU_PROG_VX_COL0_ALIAS].AddRandItem ( 1, att_1    );
            m_Pickers[RND_GPU_PROG_VX_COL0_ALIAS].AddRandItem ( 1, att_NRML );
            m_Pickers[RND_GPU_PROG_VX_COL0_ALIAS].AddRandItem ( 1, att_FOGC );
            m_Pickers[RND_GPU_PROG_VX_COL0_ALIAS].AddRandItem ( 1, att_TEX2 );
            m_Pickers[RND_GPU_PROG_VX_COL0_ALIAS].AddRandItem ( 1, att_TEX3 );
            m_Pickers[RND_GPU_PROG_VX_COL0_ALIAS].AddRandItem ( 1, att_TEX4 );
            m_Pickers[RND_GPU_PROG_VX_COL0_ALIAS].AddRandItem ( 1, att_TEX5 );
            m_Pickers[RND_GPU_PROG_VX_COL0_ALIAS].AddRandItem ( 1, att_TEX6 );
            m_Pickers[RND_GPU_PROG_VX_COL0_ALIAS].AddRandItem ( 1, att_TEX7 );
            m_Pickers[RND_GPU_PROG_VX_COL0_ALIAS].CompileRandom();
            break;

        case RND_GPU_PROG_VX_POSILWAR:
            // use OPTION LW_position_ilwariant (vp1.1 and up)
            m_Pickers[RND_GPU_PROG_VX_POSILWAR].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_POSILWAR].AddRandItem(2, GL_FALSE);
            m_Pickers[RND_GPU_PROG_VX_POSILWAR].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_GPU_PROG_VX_POSILWAR].CompileRandom();
            break;

        case RND_GPU_PROG_VX_MULTISAMPLE:
            // use OPTION LW_explicit_multisample (gpu4.1 and up)
            m_Pickers[RND_GPU_PROG_VX_MULTISAMPLE].ConfigConst(0);
            break;

        case RND_GPU_PROG_VX_WRITES_TO_LAYR_VPORT:
            m_Pickers[RND_GPU_PROG_VX_WRITES_TO_LAYR_VPORT].ConfigRandom();
            m_Pickers[RND_GPU_PROG_VX_WRITES_TO_LAYR_VPORT].AddRandItem(2, GL_FALSE);
            m_Pickers[RND_GPU_PROG_VX_WRITES_TO_LAYR_VPORT].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_GPU_PROG_VX_WRITES_TO_LAYR_VPORT].CompileRandom();
            break;

        case RND_GPU_PROG_VX_INDEX_DEBUG:
            // which program to use (debug override)
            m_Pickers[RND_GPU_PROG_VX_INDEX_DEBUG].ConfigConst(RND_GPU_PROG_INDEX_DEBUG_NO_OVERRIDE);
            break;

        //---------------------------------------------------------
        // TcGpuPgmGen Pickers
        //---------------------------------------------------------
        case RND_GPU_PROG_TC_ENABLE:
            m_Pickers[RND_GPU_PROG_TC_ENABLE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_TC_ENABLE].AddRandItem(9, GL_FALSE);
            m_Pickers[RND_GPU_PROG_TC_ENABLE].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_GPU_PROG_TC_ENABLE].CompileRandom();
            break;

        case RND_GPU_PROG_TC_INDEX:
            m_Pickers[RND_GPU_PROG_TC_INDEX].ConfigConst(RND_GPU_PROG_TC_INDEX_PassThruPatch4);
            break;

        case RND_GPU_PROG_TC_NUMRANDOM:
            m_Pickers[RND_GPU_PROG_TC_NUMRANDOM].ConfigConst(0);
            break;

        case RND_GPU_PROG_TC_TESSRATE_OUTER:
            m_Pickers[RND_GPU_PROG_TC_TESSRATE_OUTER].FConfigRandom();
            m_Pickers[RND_GPU_PROG_TC_TESSRATE_OUTER].FAddRandRange(1, 1, 64);
            m_Pickers[RND_GPU_PROG_TC_TESSRATE_OUTER].CompileRandom();
            break;

        case RND_GPU_PROG_TC_TESSRATE_INNER:
            m_Pickers[RND_GPU_PROG_TC_TESSRATE_INNER].FConfigRandom();
            m_Pickers[RND_GPU_PROG_TC_TESSRATE_INNER].FAddRandRange(1, 1, 64);
            m_Pickers[RND_GPU_PROG_TC_TESSRATE_INNER].CompileRandom();
            break;

        case RND_GPU_PROG_TC_INDEX_DEBUG:
            // which program to use (debug override)
            m_Pickers[RND_GPU_PROG_TC_INDEX_DEBUG].ConfigConst(RND_GPU_PROG_INDEX_DEBUG_NO_OVERRIDE);
            break;

        //---------------------------------------------------------
        // TeGpuPgmGen Pickers
        //---------------------------------------------------------
        case RND_GPU_PROG_TE_ENABLE:
            m_Pickers[RND_GPU_PROG_TE_ENABLE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_TE_ENABLE].AddRandItem(3, GL_FALSE);
            m_Pickers[RND_GPU_PROG_TE_ENABLE].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_GPU_PROG_TE_ENABLE].CompileRandom();
            break;

        case RND_GPU_PROG_TE_INDEX:
            m_Pickers[RND_GPU_PROG_TE_INDEX].ConfigRandom();
            m_Pickers[RND_GPU_PROG_TE_INDEX].AddRandItem(2, RND_GPU_PROG_TE_INDEX_Quad);
            m_Pickers[RND_GPU_PROG_TE_INDEX].AddRandItem(2, RND_GPU_PROG_TE_INDEX_Tri);
            m_Pickers[RND_GPU_PROG_TE_INDEX].AddRandItem(1, RND_GPU_PROG_TE_INDEX_Line);
            m_Pickers[RND_GPU_PROG_TE_INDEX].AddRandItem(1, RND_GPU_PROG_TE_INDEX_Point);
            m_Pickers[RND_GPU_PROG_TE_INDEX].CompileRandom();
            break;

        case RND_GPU_PROG_TE_BULGE:
            m_Pickers[RND_GPU_PROG_TE_BULGE].FConfigRandom();
            m_Pickers[RND_GPU_PROG_TE_BULGE].FAddRandRange(1, 0.0, 1.5);
            m_Pickers[RND_GPU_PROG_TE_BULGE].CompileRandom();
            break;

        case RND_GPU_PROG_TE_TESS_MODE:
            m_Pickers[RND_GPU_PROG_TE_TESS_MODE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_TE_TESS_MODE].AddRandItem(3, GL_TRIANGLES);
            m_Pickers[RND_GPU_PROG_TE_TESS_MODE].AddRandItem(3, GL_QUADS);
            m_Pickers[RND_GPU_PROG_TE_TESS_MODE].AddRandItem(1, GL_ISOLINES);
            m_Pickers[RND_GPU_PROG_TE_TESS_MODE].CompileRandom();
            break;

        case RND_GPU_PROG_TE_TESS_POINT_MODE:
            m_Pickers[RND_GPU_PROG_TE_TESS_POINT_MODE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_TE_TESS_POINT_MODE].AddRandItem(9, GL_FALSE);
            m_Pickers[RND_GPU_PROG_TE_TESS_POINT_MODE].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_GPU_PROG_TE_TESS_POINT_MODE].CompileRandom();
            break;

        case RND_GPU_PROG_TE_TESS_SPACING:
            m_Pickers[RND_GPU_PROG_TE_TESS_SPACING].ConfigRandom();
            m_Pickers[RND_GPU_PROG_TE_TESS_SPACING].AddRandItem(3, GL_EQUAL);
            m_Pickers[RND_GPU_PROG_TE_TESS_SPACING].AddRandItem(1, GL_FRACTIONAL_ODD);
            m_Pickers[RND_GPU_PROG_TE_TESS_SPACING].AddRandItem(1, GL_FRACTIONAL_EVEN);
            m_Pickers[RND_GPU_PROG_TE_TESS_SPACING].CompileRandom();
            break;

        case RND_GPU_PROG_TE_CW:
            m_Pickers[RND_GPU_PROG_TE_CW].ConfigRandom();
            m_Pickers[RND_GPU_PROG_TE_CW].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_GPU_PROG_TE_CW].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_GPU_PROG_TE_CW].CompileRandom();
            break;

        case RND_GPU_PROG_TE_WRITES_TO_LAYR_VPORT:
            m_Pickers[RND_GPU_PROG_TE_WRITES_TO_LAYR_VPORT].ConfigRandom();
            m_Pickers[RND_GPU_PROG_TE_WRITES_TO_LAYR_VPORT].AddRandItem(2, GL_FALSE);
            m_Pickers[RND_GPU_PROG_TE_WRITES_TO_LAYR_VPORT].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_GPU_PROG_TE_WRITES_TO_LAYR_VPORT].CompileRandom();
            break;

        case RND_GPU_PROG_TE_NUMRANDOM:
            // Since we only generate randoms for strict mode, leave this at 0.
            m_Pickers[RND_GPU_PROG_TE_NUMRANDOM].ConfigConst(0);
            break;

        case RND_GPU_PROG_TE_INDEX_DEBUG:
            // which program to use (debug override)
            m_Pickers[RND_GPU_PROG_TE_INDEX_DEBUG].ConfigConst(RND_GPU_PROG_INDEX_DEBUG_NO_OVERRIDE);
            break;

        //---------------------------------------------------------
        // GmGpuPgmGen Pickers
        //---------------------------------------------------------
        case RND_GPU_PROG_GM_ENABLE:
            m_Pickers[RND_GPU_PROG_GM_ENABLE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_ENABLE].AddRandItem(8, GL_FALSE);
            m_Pickers[RND_GPU_PROG_GM_ENABLE].AddRandItem(2, GL_TRUE);
            m_Pickers[RND_GPU_PROG_GM_ENABLE].CompileRandom();
            break;

        case RND_GPU_PROG_GM_INDEX:
            // Which geometry program to use
            m_Pickers[RND_GPU_PROG_GM_INDEX].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_INDEX].AddRandRange(1, 0, RND_GPU_PROG_GM_INDEX_NumFixedProgs-1);
            m_Pickers[RND_GPU_PROG_GM_INDEX].AddRandItem (2,psRandom);
            m_Pickers[RND_GPU_PROG_GM_INDEX].CompileRandom();
            break;

        case RND_GPU_PROG_GM_NUMRANDOM:
            // how many random geometry programs to generate per frame
            m_Pickers[RND_GPU_PROG_GM_NUMRANDOM].ConfigConst(25);
            break;

        case RND_GPU_PROG_GM_ISPASSTHRU:
            // how many times do we select a pass through GM?
            m_Pickers[RND_GPU_PROG_GM_ISPASSTHRU].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_ISPASSTHRU].AddRandItem(2, GL_TRUE);
            m_Pickers[RND_GPU_PROG_GM_ISPASSTHRU].AddRandItem(8, GL_FALSE);
            m_Pickers[RND_GPU_PROG_GM_ISPASSTHRU].CompileRandom();
            break;

        case RND_GPU_PROG_GM_TEMPLATE:
            // which template to use for randomly generated vertex programs?
            m_Pickers[RND_GPU_PROG_GM_TEMPLATE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_TEMPLATE].AddRandItem(80,RND_GPU_PROG_TEMPLATE_Simple);
            m_Pickers[RND_GPU_PROG_GM_TEMPLATE].CompileRandom();
            break;

        case RND_GPU_PROG_GM_NUM_OPS_PER_VERTEX:
            // how many instructions long should a random geometry be?
            m_Pickers[RND_GPU_PROG_GM_NUM_OPS_PER_VERTEX].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_NUM_OPS_PER_VERTEX].AddRandRange(90, 3, 5 );
            m_Pickers[RND_GPU_PROG_GM_NUM_OPS_PER_VERTEX].AddRandRange(10, 3, 10);
            m_Pickers[RND_GPU_PROG_GM_NUM_OPS_PER_VERTEX].CompileRandom();
            break;

        case RND_GPU_PROG_GM_OPCODE:
            // random vertex program instruction
            m_Pickers[RND_GPU_PROG_GM_OPCODE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_abs    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_add    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_and    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(0, RND_GPU_PROG_OPCODE_brk    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_cal    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_cei    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_cmp    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(0, RND_GPU_PROG_OPCODE_con    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_cos    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_div    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_dp2    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_dp2a   );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_dp3    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_dp4    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_dph    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_dst    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(0, RND_GPU_PROG_OPCODE_else   );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(0, RND_GPU_PROG_OPCODE_endif  );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(0, RND_GPU_PROG_OPCODE_endrep );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_ex2    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_flr    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_frc    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_i2f    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_if     );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_lg2    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_lit    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_lrp    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_mad    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_max    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_min    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_mod    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_mov    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_mul    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_not    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_nrm    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_or     );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_pk2h   );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_pk2us  );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_pk4b   );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_pk4ub  );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_pow    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_rcc    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(2, RND_GPU_PROG_OPCODE_rcp    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_rep    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(0, RND_GPU_PROG_OPCODE_ret    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_rfl    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_round  );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_rsq    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_sad    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_scs    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_seq    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_sfl    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_sge    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_sgt    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_shl    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_shr    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_sin    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_sle    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_slt    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_sne    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_ssg    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_str    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_sub    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_swz    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_tex    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_trunc  );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_txb    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_txd    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_txf    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_txl    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_txp    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(0, RND_GPU_PROG_OPCODE_txq    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_up2h   );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_up2us  );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_up4b   );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_up4ub  );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_x2d    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_xor    );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_xpd    );
            // LW_gpu_program4_1 opcodes
            // TODO: Uncomment these lines when EXT_texture_array is implemented
            //m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_tex2   );
            //m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_txb2   );
            //m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem(1, RND_GPU_PROG_OPCODE_txl2   );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_txg   );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_txfms );
            // LW_gpu_program5 opcodes
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_bfe  );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_bfi  );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_bfr  );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_btc  );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_btfl );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_btfm );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_txgo );
            // Waiting on data types
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_cvt  );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_pk64 );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_up64 );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_ldc );
            m_Pickers[RND_GPU_PROG_GM_OPCODE].CompileRandom();
            break;

        case RND_GPU_PROG_GM_OUT_TEST:
            // CC test for conditional result writes.
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem(10, RND_GPU_PROG_OUT_TEST_none);
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_eq  );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 2, RND_GPU_PROG_OUT_TEST_ge  );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 2, RND_GPU_PROG_OUT_TEST_gt  );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 2, RND_GPU_PROG_OUT_TEST_le  );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 2, RND_GPU_PROG_OUT_TEST_lt  );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_ne  );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_tr  );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_fl  );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_nan );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_leg );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_cf  );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_ncf );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_of  );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_nof );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_ab  );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_ble );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_sf  );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_nsf );
            m_Pickers[RND_GPU_PROG_GM_OUT_TEST].CompileRandom();
            break;

        case RND_GPU_PROG_GM_WHICH_CC:
            // do/don't update the CC (condition code) in an instruction in a random vertex program
            m_Pickers[RND_GPU_PROG_GM_WHICH_CC].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_WHICH_CC].AddRandItem(1, RND_GPU_PROG_CCnone);
            m_Pickers[RND_GPU_PROG_GM_WHICH_CC].AddRandItem(1, RND_GPU_PROG_CC0);
            m_Pickers[RND_GPU_PROG_GM_WHICH_CC].AddRandItem(1, RND_GPU_PROG_CC1);
            m_Pickers[RND_GPU_PROG_GM_WHICH_CC].CompileRandom();
            break;

        case RND_GPU_PROG_GM_SWIZZLE_ORDER:
            // input or CC swizzle ordering
            m_Pickers[RND_GPU_PROG_GM_SWIZZLE_ORDER].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_SWIZZLE_ORDER].AddRandItem(1, RND_GPU_PROG_SWIZZLE_ORDER_xyzw    );
            m_Pickers[RND_GPU_PROG_GM_SWIZZLE_ORDER].AddRandItem(1, RND_GPU_PROG_SWIZZLE_ORDER_shuffle );
            m_Pickers[RND_GPU_PROG_GM_SWIZZLE_ORDER].AddRandItem(1, RND_GPU_PROG_SWIZZLE_ORDER_random  );
            m_Pickers[RND_GPU_PROG_GM_SWIZZLE_ORDER].CompileRandom();
            break;

        case RND_GPU_PROG_GM_SWIZZLE_SUFFIX:
            // input or CC swizzle format
            m_Pickers[RND_GPU_PROG_GM_SWIZZLE_SUFFIX].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_SWIZZLE_SUFFIX].AddRandItem(1, RND_GPU_PROG_SWIZZLE_SUFFIX_none    );
            m_Pickers[RND_GPU_PROG_GM_SWIZZLE_SUFFIX].AddRandItem(1, RND_GPU_PROG_SWIZZLE_SUFFIX_component );
            m_Pickers[RND_GPU_PROG_GM_SWIZZLE_SUFFIX].AddRandItem(10, RND_GPU_PROG_SWIZZLE_SUFFIX_all  );
            m_Pickers[RND_GPU_PROG_GM_SWIZZLE_SUFFIX].CompileRandom();
            break;

        case RND_GPU_PROG_GM_OUT_REG:
            // output register for random vertex program instruction
            m_Pickers[RND_GPU_PROG_GM_OUT_REG].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_OUT_REG].AddRandRange(45, RND_GPU_PROG_GM_REG_R0, RND_GPU_PROG_GM_REG_R15 );
            m_Pickers[RND_GPU_PROG_GM_OUT_REG].AddRandItem(  5, RND_GPU_PROG_GM_REG_TempArray );
            m_Pickers[RND_GPU_PROG_GM_OUT_REG].AddRandRange( 3, RND_GPU_PROG_GM_REG_oCLP0, RND_GPU_PROG_GM_REG_oCLP5 );
            m_Pickers[RND_GPU_PROG_GM_OUT_REG].AddRandRange( 3, RND_GPU_PROG_GM_REG_oTEX0, RND_GPU_PROG_GM_REG_oTEX7 );
            m_Pickers[RND_GPU_PROG_GM_OUT_REG].AddRandItem( 10, RND_GPU_PROG_GM_REG_oHPOS );
            m_Pickers[RND_GPU_PROG_GM_OUT_REG].AddRandItem(  1, RND_GPU_PROG_GM_REG_oCOL0 );
            m_Pickers[RND_GPU_PROG_GM_OUT_REG].AddRandItem(  1, RND_GPU_PROG_GM_REG_oCOL1 );
            m_Pickers[RND_GPU_PROG_GM_OUT_REG].AddRandItem(  1, RND_GPU_PROG_GM_REG_oBFC0 );
            m_Pickers[RND_GPU_PROG_GM_OUT_REG].AddRandItem(  1, RND_GPU_PROG_GM_REG_oBFC1 );
            m_Pickers[RND_GPU_PROG_GM_OUT_REG].AddRandItem(  1, RND_GPU_PROG_GM_REG_oFOGC );
            m_Pickers[RND_GPU_PROG_GM_OUT_REG].AddRandItem(  1, RND_GPU_PROG_GM_REG_oNRML );
            m_Pickers[RND_GPU_PROG_GM_OUT_REG].CompileRandom();
            break;

        case RND_GPU_PROG_GM_OUT_MASK:
            // output register component mask
            m_Pickers[RND_GPU_PROG_GM_OUT_MASK].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_OUT_MASK].AddRandItem (4, 0xf);
            m_Pickers[RND_GPU_PROG_GM_OUT_MASK].AddRandRange(1, 1, 0xf);
            m_Pickers[RND_GPU_PROG_GM_OUT_MASK].CompileRandom();
            break;

        case RND_GPU_PROG_GM_SATURATE:
            // do/don't  saturate output (clamp to [0,1], or [-1,1]).
            m_Pickers[RND_GPU_PROG_GM_SATURATE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_SATURATE].AddRandItem (90, RND_GPU_PROG_OP_SAT_none);
            m_Pickers[RND_GPU_PROG_GM_SATURATE].AddRandItem ( 5, RND_GPU_PROG_OP_SAT_unsigned);
            m_Pickers[RND_GPU_PROG_GM_SATURATE].AddRandItem ( 5, RND_GPU_PROG_OP_SAT_signed);
            m_Pickers[RND_GPU_PROG_GM_SATURATE].CompileRandom();
            break;

        case RND_GPU_PROG_GM_RELADDR:
            // do/don't use addr-reg  addressing for reg reads
            m_Pickers[RND_GPU_PROG_GM_RELADDR].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_RELADDR].AddRandItem (1, GL_FALSE);
            m_Pickers[RND_GPU_PROG_GM_RELADDR].AddRandItem (1, GL_TRUE);
            m_Pickers[RND_GPU_PROG_GM_RELADDR].CompileRandom();
            break;

        case RND_GPU_PROG_GM_IN_ABS:
            // do/don't take abs. value of input argument
            m_Pickers[RND_GPU_PROG_GM_IN_ABS].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_IN_ABS].AddRandItem (3, GL_FALSE);
            m_Pickers[RND_GPU_PROG_GM_IN_ABS].AddRandItem (1, GL_TRUE);
            m_Pickers[RND_GPU_PROG_GM_IN_ABS].CompileRandom();
            break;

        case RND_GPU_PROG_GM_IN_NEG:
            // do/don't negate input argument
            m_Pickers[RND_GPU_PROG_GM_IN_NEG].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_IN_NEG].AddRandItem (3, RND_GPU_PROG_OP_SIGN_none);
            m_Pickers[RND_GPU_PROG_GM_IN_NEG].AddRandItem (1, RND_GPU_PROG_OP_SIGN_neg);
            m_Pickers[RND_GPU_PROG_GM_IN_NEG].CompileRandom();
            break;

        case RND_GPU_PROG_GM_TX_DIM:
            // Which texture attribute dim (1d/2d/lwbe/3d) to fetch from.
            // LW40 only supports 1D/2D
            m_Pickers[RND_GPU_PROG_GM_TX_DIM].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_TX_DIM].AddRandItem(5, Is2d);
            m_Pickers[RND_GPU_PROG_GM_TX_DIM].AddRandItem(1, Is3d);
            m_Pickers[RND_GPU_PROG_GM_TX_DIM].AddRandItem(2, IsLwbe);
            m_Pickers[RND_GPU_PROG_GM_TX_DIM].AddRandItem(1, Is1d);
            m_Pickers[RND_GPU_PROG_GM_TX_DIM].AddRandItem(1, Is2dMultisample);
            m_Pickers[RND_GPU_PROG_GM_TX_DIM].CompileRandom();
            break;

        case RND_GPU_PROG_GM_TX_OFFSET:
            m_Pickers[RND_GPU_PROG_GM_TX_OFFSET].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_TX_OFFSET].AddRandItem(50, toNone);
            m_Pickers[RND_GPU_PROG_GM_TX_OFFSET].AddRandItem(25, toConstant);
            m_Pickers[RND_GPU_PROG_GM_TX_OFFSET].AddRandItem(25, toProgrammable);
            m_Pickers[RND_GPU_PROG_GM_TX_OFFSET].CompileRandom();
            break;

        case RND_GPU_PROG_GM_LIGHT0:
            // which "light" to use in certain vertex programs
            m_Pickers[RND_GPU_PROG_GM_LIGHT0].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_LIGHT0].AddRandRange(1, 0, 0xffffffff);
            m_Pickers[RND_GPU_PROG_GM_LIGHT0].CompileRandom();
            break;

        case RND_GPU_PROG_GM_LIGHT1:
            // which "light" to use in certain vertex programs
            m_Pickers[RND_GPU_PROG_GM_LIGHT1].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_LIGHT1].AddRandRange(1, 0, 0xffffffff);
            m_Pickers[RND_GPU_PROG_GM_LIGHT1].CompileRandom();
            break;

        case RND_GPU_PROG_GM_PRIM_IN:
            m_Pickers[RND_GPU_PROG_GM_PRIM_IN].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_PRIM_IN].AddRandItem(4, GL_POINTS);
            m_Pickers[RND_GPU_PROG_GM_PRIM_IN].AddRandItem(5, GL_LINES);
            //m_Pickers[RND_GPU_PROG_GM_PRIM_IN].AddRandItem(1, GL_LINES_ADJACENCY_EXT);
            m_Pickers[RND_GPU_PROG_GM_PRIM_IN].AddRandItem(80, GL_TRIANGLES);
            //m_Pickers[RND_GPU_PROG_GM_PRIM_IN].AddRandItem(10, GL_TRIANGLES_ADJACENCY_EXT);
            m_Pickers[RND_GPU_PROG_GM_PRIM_IN].CompileRandom();
            break;

        case RND_GPU_PROG_GM_PRIM_OUT:
            m_Pickers[RND_GPU_PROG_GM_PRIM_OUT].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_PRIM_OUT].AddRandItem(5,  GL_POINTS);
            m_Pickers[RND_GPU_PROG_GM_PRIM_OUT].AddRandItem(15, GL_LINE_STRIP);
            m_Pickers[RND_GPU_PROG_GM_PRIM_OUT].AddRandItem(80, GL_TRIANGLE_STRIP);
            m_Pickers[RND_GPU_PROG_GM_PRIM_OUT].CompileRandom();
            break;

        case RND_GPU_PROG_GM_VERTICES_OUT:
            m_Pickers[RND_GPU_PROG_GM_VERTICES_OUT].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_VERTICES_OUT].AddRandRange(80, 1, 3);
            m_Pickers[RND_GPU_PROG_GM_VERTICES_OUT].AddRandRange(20, 3, 15);
            m_Pickers[RND_GPU_PROG_GM_VERTICES_OUT].CompileRandom();
            break;

        case RND_GPU_PROG_GM_PRIMITIVES_OUT:
            m_Pickers[RND_GPU_PROG_GM_PRIMITIVES_OUT].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_PRIMITIVES_OUT].AddRandItem (1, 1);
            m_Pickers[RND_GPU_PROG_GM_PRIMITIVES_OUT].AddRandRange(1, 1, 10);
            m_Pickers[RND_GPU_PROG_GM_PRIMITIVES_OUT].CompileRandom();
            break;

        case RND_GPU_PROG_GM_WRITES_TO_LAYR_VPORT:
            m_Pickers[RND_GPU_PROG_GM_WRITES_TO_LAYR_VPORT].ConfigRandom();
            m_Pickers[RND_GPU_PROG_GM_WRITES_TO_LAYR_VPORT].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_GPU_PROG_GM_WRITES_TO_LAYR_VPORT].AddRandItem(4, GL_TRUE);
            m_Pickers[RND_GPU_PROG_GM_WRITES_TO_LAYR_VPORT].CompileRandom();
            break;

        case RND_GPU_PROG_GM_INDEX_DEBUG:
            // which program to use (debug override)
            m_Pickers[RND_GPU_PROG_GM_INDEX_DEBUG].ConfigConst(RND_GPU_PROG_INDEX_DEBUG_NO_OVERRIDE);
            break;

        //---------------------------------------------------------
        // FrGpuPgmGen Pickers
        case RND_GPU_PROG_FR_ENABLE:
            // do/don't use fragment program to render.
            m_Pickers[RND_GPU_PROG_FR_ENABLE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_ENABLE].AddRandItem(4, GL_TRUE);
            m_Pickers[RND_GPU_PROG_FR_ENABLE].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_GPU_PROG_FR_ENABLE].CompileRandom();
            break;

        case RND_GPU_PROG_FR_INDEX:
            // Which fragment program to use
            m_Pickers[RND_GPU_PROG_FR_INDEX].ConfigRandom();
            //m_Pickers[RND_GPU_PROG_FR_INDEX].AddRandRange(1, 0, RND_GPU_PROG_FR_INDEX_NumFixedProgs);
            m_Pickers[RND_GPU_PROG_FR_INDEX].AddRandItem(1, psRandom);
            m_Pickers[RND_GPU_PROG_FR_INDEX].CompileRandom();
            break;

        case RND_GPU_PROG_FR_NUMRANDOM:
            // how many random fragment programs to generate per frame
            m_Pickers[RND_GPU_PROG_FR_NUMRANDOM].ConfigConst(25);
            break;

        case RND_GPU_PROG_FR_TEMPLATE:
            // which template to use for randomly generated vertex programs?
            m_Pickers[RND_GPU_PROG_FR_TEMPLATE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_TEMPLATE].AddRandItem(70,RND_GPU_PROG_TEMPLATE_Simple);
            m_Pickers[RND_GPU_PROG_FR_TEMPLATE].AddRandItem(10,RND_GPU_PROG_TEMPLATE_Call);
            m_Pickers[RND_GPU_PROG_FR_TEMPLATE].AddRandItem(10,RND_GPU_PROG_TEMPLATE_Flow);
            m_Pickers[RND_GPU_PROG_FR_TEMPLATE].AddRandItem(10,RND_GPU_PROG_TEMPLATE_CallIndexed);
            m_Pickers[RND_GPU_PROG_FR_TEMPLATE].CompileRandom();
            break;

        case RND_GPU_PROG_FR_NUM_OPS:
            // min number of opcodes for each emitted vertex
            m_Pickers[RND_GPU_PROG_FR_NUM_OPS].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_NUM_OPS].AddRandRange(80, 2,    5);
            m_Pickers[RND_GPU_PROG_FR_NUM_OPS].AddRandRange(18, 1,   10);
            m_Pickers[RND_GPU_PROG_FR_NUM_OPS].AddRandRange(1,  1,   20);
            m_Pickers[RND_GPU_PROG_FR_NUM_OPS].AddRandRange(1,  1,   50);
            m_Pickers[RND_GPU_PROG_FR_NUM_OPS].CompileRandom();
            break;

        case RND_GPU_PROG_FR_OPCODE:
            // which fragment program opcode to use next when generating programs
            m_Pickers[RND_GPU_PROG_FR_OPCODE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_abs      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_add      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_and      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_brk      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_cal      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_cei      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_cmp      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_con      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_cos      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_div      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_dp2      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_dp2a     );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_dp3      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_dp4      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_dph      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_dst      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_else     );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_endif    );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_endrep   );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_ex2      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_flr      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 3, RND_GPU_PROG_OPCODE_frc      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_i2f      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_if       );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_lg2      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_lit      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 3, RND_GPU_PROG_OPCODE_lrp      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_mad      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_max      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_min      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_mod      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_mov      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_mul      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_not      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_nrm      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_or       );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_pk2h     );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_pk2us    );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 3, RND_GPU_PROG_OPCODE_pk4b     );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 3, RND_GPU_PROG_OPCODE_pk4ub    );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_pow      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_rcc      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_rcp      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_rep      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_ret      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 3, RND_GPU_PROG_OPCODE_rfl      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_round    );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_rsq      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_sad      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_scs      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_seq      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_sfl      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_sge      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_sgt      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_shl      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_shr      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_sin      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_sle      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_slt      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_sne      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_ssg      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_str      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_sub      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_swz      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem(25, RND_GPU_PROG_OPCODE_tex      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_trunc    );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_txb      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem(25, RND_GPU_PROG_OPCODE_txd      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_txf      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_txl      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem(25, RND_GPU_PROG_OPCODE_txp      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 5, RND_GPU_PROG_OPCODE_txq      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_up2h     );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 2, RND_GPU_PROG_OPCODE_up2us    );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 3, RND_GPU_PROG_OPCODE_up4b     );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 3, RND_GPU_PROG_OPCODE_up4ub    );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_x2d      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_xor      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_xpd      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 3, RND_GPU_PROG_FR_OPCODE_ddx   );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 3, RND_GPU_PROG_FR_OPCODE_ddy   );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 3, RND_GPU_PROG_FR_OPCODE_kil   );
            // LW_gpu_program4_1 opcodes
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem(15, RND_GPU_PROG_OPCODE_tex2     );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_txb2     );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_txl2     );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_txg      );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 25, RND_GPU_PROG_FR_OPCODE_lod  );
            // LW_explicit_multisample opcodes
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 20, RND_GPU_PROG_OPCODE_txfms   );
            // LW_gpu_program5 opcodes
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1,  RND_GPU_PROG_OPCODE_bfe     );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1,  RND_GPU_PROG_OPCODE_bfi     );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1,  RND_GPU_PROG_OPCODE_bfr     );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1,  RND_GPU_PROG_OPCODE_btc     );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1,  RND_GPU_PROG_OPCODE_btfl    );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1,  RND_GPU_PROG_OPCODE_btfm    );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem(15,  RND_GPU_PROG_OPCODE_txgo    );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1,  RND_GPU_PROG_FR_OPCODE_ipac );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1,  RND_GPU_PROG_FR_OPCODE_ipao );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1,  RND_GPU_PROG_FR_OPCODE_ipas );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem(25,  RND_GPU_PROG_FR_OPCODE_atom );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem(15,  RND_GPU_PROG_FR_OPCODE_load );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem(15,  RND_GPU_PROG_FR_OPCODE_store);
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 2,  RND_GPU_PROG_OPCODE_ldc     );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1,  RND_GPU_PROG_OPCODE_cvt     );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 2,  RND_GPU_PROG_OPCODE_pk64    );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1,  RND_GPU_PROG_OPCODE_up64    );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 20, RND_GPU_PROG_OPCODE_loadim  );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 15, RND_GPU_PROG_OPCODE_storeim );
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 10, RND_GPU_PROG_FR_OPCODE_atomim);
            m_Pickers[RND_GPU_PROG_FR_OPCODE].AddRandItem( 1, RND_GPU_PROG_OPCODE_membar);
            m_Pickers[RND_GPU_PROG_FR_OPCODE].CompileRandom();
            break;

        case RND_GPU_PROG_FR_OUT_TEST:
            // CC test for conditional result writes.
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem(10, RND_GPU_PROG_OUT_TEST_none);
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_eq  );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 2, RND_GPU_PROG_OUT_TEST_ge  );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 2, RND_GPU_PROG_OUT_TEST_gt  );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 2, RND_GPU_PROG_OUT_TEST_le  );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 2, RND_GPU_PROG_OUT_TEST_lt  );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_ne  );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_tr  );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_fl  );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_nan );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_leg );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_cf  );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_ncf );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_of  );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_nof );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_ab  );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_ble );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_sf  );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].AddRandItem( 1, RND_GPU_PROG_OUT_TEST_nsf );
            m_Pickers[RND_GPU_PROG_FR_OUT_TEST].CompileRandom();
            break;

        case RND_GPU_PROG_FR_WHICH_CC:
            // do/don't update the CC (condition code) in an instruction in a random vertex program
            m_Pickers[RND_GPU_PROG_FR_WHICH_CC].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_WHICH_CC].AddRandItem(1, RND_GPU_PROG_CCnone);
            m_Pickers[RND_GPU_PROG_FR_WHICH_CC].AddRandItem(1, RND_GPU_PROG_CC0);
            m_Pickers[RND_GPU_PROG_FR_WHICH_CC].AddRandItem(1, RND_GPU_PROG_CC1);
            m_Pickers[RND_GPU_PROG_FR_WHICH_CC].CompileRandom();
            break;

        case RND_GPU_PROG_FR_SWIZZLE_ORDER:
            // Do/don't set condition code.
            m_Pickers[RND_GPU_PROG_FR_SWIZZLE_ORDER].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_SWIZZLE_ORDER].AddRandItem(6, RND_GPU_PROG_SWIZZLE_xyzw   );
            m_Pickers[RND_GPU_PROG_FR_SWIZZLE_ORDER].AddRandItem(1, RND_GPU_PROG_SWIZZLE_shuffle);
            m_Pickers[RND_GPU_PROG_FR_SWIZZLE_ORDER].AddRandItem(1, RND_GPU_PROG_SWIZZLE_random );
            m_Pickers[RND_GPU_PROG_FR_SWIZZLE_ORDER].CompileRandom();
            break;

        case RND_GPU_PROG_FR_SWIZZLE_SUFFIX:
            // input or CC swizzle format
            m_Pickers[RND_GPU_PROG_FR_SWIZZLE_SUFFIX].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_SWIZZLE_SUFFIX].AddRandItem(1, RND_GPU_PROG_SWIZZLE_SUFFIX_none);
            m_Pickers[RND_GPU_PROG_FR_SWIZZLE_SUFFIX].AddRandItem(1, RND_GPU_PROG_SWIZZLE_SUFFIX_component);
            m_Pickers[RND_GPU_PROG_FR_SWIZZLE_SUFFIX].AddRandItem(10,RND_GPU_PROG_SWIZZLE_SUFFIX_all);
            m_Pickers[RND_GPU_PROG_FR_SWIZZLE_SUFFIX].CompileRandom();
            break;

        case RND_GPU_PROG_FR_OUT_REG:
            // Which register to use for frag prog opcode result?
            m_Pickers[RND_GPU_PROG_FR_OUT_REG].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_OUT_REG].AddRandRange(20, RND_GPU_PROG_FR_REG_R0, RND_GPU_PROG_FR_REG_R15);  // tmp reg
            m_Pickers[RND_GPU_PROG_FR_OUT_REG].AddRandItem(  5, RND_GPU_PROG_FR_REG_TempArray );
            m_Pickers[RND_GPU_PROG_FR_OUT_REG].AddRandRange(10, RND_GPU_PROG_FR_REG_oCOL0, RND_GPU_PROG_FR_REG_oCOL15);
            m_Pickers[RND_GPU_PROG_FR_OUT_REG].AddRandItem ( 2, RND_GPU_PROG_FR_REG_oDEP );
            m_Pickers[RND_GPU_PROG_FR_OUT_REG].CompileRandom();
            break;

        case RND_GPU_PROG_FR_OUT_MASK:
            // Output register mask
            m_Pickers[RND_GPU_PROG_FR_OUT_MASK].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_OUT_MASK].AddRandItem(1, 0xf);
            m_Pickers[RND_GPU_PROG_FR_OUT_MASK].AddRandRange(4, 1, 0xf);
            m_Pickers[RND_GPU_PROG_FR_OUT_MASK].CompileRandom();
            break;

        case RND_GPU_PROG_FR_SATURATE:
            // do/don't clamp result to [0, 1] or [-1,1].
            m_Pickers[RND_GPU_PROG_FR_SATURATE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_SATURATE].AddRandItem (8, RND_GPU_PROG_OP_SAT_none);
            m_Pickers[RND_GPU_PROG_FR_SATURATE].AddRandItem (1, RND_GPU_PROG_OP_SAT_unsigned);
            m_Pickers[RND_GPU_PROG_FR_SATURATE].AddRandItem (1, RND_GPU_PROG_OP_SAT_signed);
            m_Pickers[RND_GPU_PROG_FR_SATURATE].CompileRandom();
            break;

        case RND_GPU_PROG_FR_RELADDR:
            // do/don't use addr-reg  addressing for reg reads
            m_Pickers[RND_GPU_PROG_FR_RELADDR].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_RELADDR].AddRandItem (1, GL_FALSE);
            m_Pickers[RND_GPU_PROG_FR_RELADDR].AddRandItem (1, GL_TRUE);
            m_Pickers[RND_GPU_PROG_FR_RELADDR].CompileRandom();
            break;

        case RND_GPU_PROG_FR_IN_ABS:
            // do/don't take absolute value of frag prog opcode input
            m_Pickers[RND_GPU_PROG_FR_IN_ABS].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_IN_ABS].AddRandItem(4, GL_FALSE);
            m_Pickers[RND_GPU_PROG_FR_IN_ABS].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_GPU_PROG_FR_IN_ABS].CompileRandom();
            break;

        case RND_GPU_PROG_FR_IN_NEG:
            // do/don't negate frag prog opcode input
            m_Pickers[RND_GPU_PROG_FR_IN_NEG].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_IN_NEG].AddRandItem(4, GL_FALSE);
            m_Pickers[RND_GPU_PROG_FR_IN_NEG].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_GPU_PROG_FR_IN_NEG].CompileRandom();
            break;

        case RND_GPU_PROG_FR_TX_DIM:
            // Texture attribute dimensionality) Is2d, Is3d, IsLwbe, etc.
            m_Pickers[RND_GPU_PROG_FR_TX_DIM].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_TX_DIM].AddRandItem(5, Is2d);
            m_Pickers[RND_GPU_PROG_FR_TX_DIM].AddRandItem(1, Is3d);
            m_Pickers[RND_GPU_PROG_FR_TX_DIM].AddRandItem(2, IsLwbe);
            m_Pickers[RND_GPU_PROG_FR_TX_DIM].AddRandItem(1, Is1d);
            m_Pickers[RND_GPU_PROG_FR_TX_DIM].AddRandItem(1, Is2dMultisample);
            m_Pickers[RND_GPU_PROG_FR_TX_DIM].AddRandItem(2, IsArray1d);
            m_Pickers[RND_GPU_PROG_FR_TX_DIM].AddRandItem(2, IsArray2d);
            m_Pickers[RND_GPU_PROG_FR_TX_DIM].AddRandItem(1, IsArrayLwbe);
            m_Pickers[RND_GPU_PROG_FR_TX_DIM].AddRandItem(1, IsShadow1d);
            m_Pickers[RND_GPU_PROG_FR_TX_DIM].AddRandItem(1, IsShadow2d);
            m_Pickers[RND_GPU_PROG_FR_TX_DIM].AddRandItem(1, IsShadowLwbe);
            m_Pickers[RND_GPU_PROG_FR_TX_DIM].AddRandItem(1, IsShadowArray1d);
            m_Pickers[RND_GPU_PROG_FR_TX_DIM].AddRandItem(1, IsShadowArray2d);
            m_Pickers[RND_GPU_PROG_FR_TX_DIM].AddRandItem(1, IsShadowArrayLwbe);
            m_Pickers[RND_GPU_PROG_FR_TX_DIM].CompileRandom();
            break;

        case RND_GPU_PROG_FR_TX_OFFSET:
            m_Pickers[RND_GPU_PROG_FR_TX_OFFSET].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_TX_OFFSET].AddRandItem(50, toNone);
            m_Pickers[RND_GPU_PROG_FR_TX_OFFSET].AddRandItem(25, toConstant);
            m_Pickers[RND_GPU_PROG_FR_TX_OFFSET].AddRandItem(25, toProgrammable);
            m_Pickers[RND_GPU_PROG_FR_TX_OFFSET].CompileRandom();
            break;

        // Unique Fragment programs pickers
        case RND_GPU_PROG_FR_DEPTH_RQD:
            m_Pickers[RND_GPU_PROG_FR_DEPTH_RQD].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_DEPTH_RQD].AddRandItem(9, false);
            m_Pickers[RND_GPU_PROG_FR_DEPTH_RQD].AddRandItem(1, true);
            m_Pickers[RND_GPU_PROG_FR_DEPTH_RQD].CompileRandom();
            break;

        case RND_GPU_PROG_FR_DRAW_BUFFERS:
            // OPTION=ARB_draw_buffers in fragment programs
            m_Pickers[RND_GPU_PROG_FR_DRAW_BUFFERS].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_DRAW_BUFFERS].AddRandItem(66, GL_FALSE);
            m_Pickers[RND_GPU_PROG_FR_DRAW_BUFFERS].AddRandItem(33, GL_TRUE);
            m_Pickers[RND_GPU_PROG_FR_DRAW_BUFFERS].CompileRandom();
            break;

        case RND_GPU_PROG_FR_MULTISAMPLE:
            // use OPTION LW_explicit_multisample (gpu4.1 and up)
            m_Pickers[RND_GPU_PROG_FR_MULTISAMPLE].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_MULTISAMPLE].AddRandItem(10, GL_TRUE);
            m_Pickers[RND_GPU_PROG_FR_MULTISAMPLE].AddRandItem(90, GL_FALSE);
            m_Pickers[RND_GPU_PROG_FR_MULTISAMPLE].CompileRandom();
            break;

        case RND_GPU_PROG_FR_END_STRATEGY:
            // which strategy to use for the fragment.color0 write
            m_Pickers[RND_GPU_PROG_FR_END_STRATEGY].ConfigRandom();
            m_Pickers[RND_GPU_PROG_FR_END_STRATEGY].AddRandItem(1, RND_GPU_PROG_FR_END_STRATEGY_frc);
            m_Pickers[RND_GPU_PROG_FR_END_STRATEGY].AddRandItem(1, RND_GPU_PROG_FR_END_STRATEGY_mov);
            m_Pickers[RND_GPU_PROG_FR_END_STRATEGY].AddRandItem(8, RND_GPU_PROG_FR_END_STRATEGY_mod);
            m_Pickers[RND_GPU_PROG_FR_END_STRATEGY].AddRandItem(1, RND_GPU_PROG_FR_END_STRATEGY_per_vtx_attr);
            // Don't turn this one on by default. There are special considerations to enable it.
            // Its controlled in the glrandom.js file.
            //m_Pickers[RND_GPU_PROG_FR_END_STRATEGY].AddRandItem(1, RND_GPU_PROG_FR_END_STRATEGY_coarse);
            m_Pickers[RND_GPU_PROG_FR_END_STRATEGY].CompileRandom();
            break;

        case RND_GPU_PROG_FR_DISABLE_CENTROID_SAMPLING:
            m_Pickers[RND_GPU_PROG_FR_DISABLE_CENTROID_SAMPLING].ConfigConst(0);
            break;

        case RND_GPU_PROG_FR_INDEX_DEBUG:
            // which program to use (debug override)
            m_Pickers[RND_GPU_PROG_FR_INDEX_DEBUG].ConfigConst(RND_GPU_PROG_INDEX_DEBUG_NO_OVERRIDE);
            break;
    }
    return OK;
}

//------------------------------------------------------------------------------
// Use this routine to bind a GL program.
// And re-define any state that gets reset while binding a program. e.g. subroutine indices
RC RndGpuPrograms::BindProgram(Program * pProg)
{
    RC rc;
    glBindProgramLW(pProg->Target, pProg->Id);

    if (pProg->SubArrayLen)
    {
        // Fill in the subroutine-array with valid addresses after loading.
        const int numSubs = pProg->SubArrayLen;
        vector<GLuint> subIdxs(numSubs);

        for (int i = 0; i < numSubs; i++)
            subIdxs[i] = i;

        glProgramSubroutineParametersuivLW(pProg->Target,  numSubs, &subIdxs[0]);
    }
    return mglGetRC();
}

//------------------------------------------------------------------------------
RC RndGpuPrograms::InitOpenGL()
{
    RC rc = OK;

    if (!m_pGLRandom->HasExt(GLRandomTest::ExtLW_gpu_program5))
        m_pGLRandom->SetUseSbos(false);

    // We need RndTexturing's bindless-textures SBO created now,
    // because we need its gpu-vaddr when creating programs.
    CHECK_RC(m_pGLRandom->m_Texturing.InitOpenGL());

    CHECK_RC(CreateSbos());
    CHECK_RC(InitSboData( 1<<sboRead));
    CHECK_RC(CreatePaBOs());

    const bool enableShaderReplacement = m_pGLRandom->GetShaderReplacement();

    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        m_Progs[pk].SetEnableReplacement(enableShaderReplacement);

        if (m_pGLRandom->HasExt(PgmGen::pkExtensions[pk]))
        {
            // Load all user programs to the OpenGL driver.
            for (int i = 0; i < m_Progs[pk].NumProgs(psUser); i++)
            {
                Program * pProg = m_Progs[pk].Get(psUser, i);
                CHECK_RC( LoadProgram (pProg) );
            }

            // If using user programs don't bother creating fixed programs
            if (m_Progs[pk].NumProgs(psUser) == 0)
            {
                // Generate, then load all fixed programs.
                m_Gen[pk]->GenerateFixedPrograms(&m_Progs[pk]);

                for (int i = 0; i < m_Progs[pk].NumProgs(psFixed); i++)
                {
                    Program * pProg = m_Progs[pk].Get(psFixed, i);
                    CHECK_RC(LoadProgram(pProg));
                }
            }

            const GLenum target = PgmGen::pkTargets[pk];
            glProgramParameter4fLW (target, GpuPgmGen::AmbientColorReg,
                    0.25f, 0.31f, 0.27f, 0.0f); // global ambient color
            glProgramParameter4fLW (target, GpuPgmGen::Constants1Reg,
                    3.0f, 4.0f, 5.0f, 1.0f); // useful consts
            glProgramParameter4fLW (target, GpuPgmGen::Constants2Reg,
                    0.0f, 2.0f, 0.5f, 1.0f); // useful consts

            if (OK != (rc = mglGetRC()))
            {
                Printf(Tee::PriHigh, "glProgramParameter4fLW()\n");
                return rc;
            }

            if (pkVertex == pk)
            {
                glTrackMatrixLW(target,
                                GpuPgmGen::ModelViewProjReg, // 0
                                GL_MODELVIEW_PROJECTION_LW,
                                GL_IDENTITY_LW);
                glTrackMatrixLW(target,
                                GpuPgmGen::ModelViewIlwReg, // 4
                                GL_MODELVIEW,
                                GL_ILWERSE_TRANSPOSE_LW);
                if (OK != (rc = mglGetRC()))
                {
                    Printf(Tee::PriHigh, "glTrackMatrixLW()\n");
                    return rc;
                }
            }
            else
            {
                for(int i =GpuPgmGen::ModelViewProjReg;
                    i < GpuPgmGen::Constants1Reg; i++)
                {
                    glProgramParameter4fLW (target, i, 1.0f, 1.0f, 1.0f, 1.0f);
                }
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC RndGpuPrograms::Restart()
{
    StickyRC rc = OK;

    m_Pickers.Restart();
    m_LogRndPgms      = (0 != m_Pickers[RND_GPU_PROG_LOG_RND_PGMS].Pick());
    m_LogToJsFile     = (0 != m_Pickers[RND_GPU_PROG_LOG_TO_JS_FILE].Pick());
    m_StrictPgmLinking = (0 != m_Pickers[RND_GPU_PROG_STRICT_PGM_LINKING].Pick());
    m_VerboseOutput = (0 != m_Pickers[RND_GPU_PROG_VERBOSE_OUTPUT].Pick());
    m_Pickers[RND_GPU_PROG_USE_BINDLESS_TEXTURES].Pick();

    // pick indexed or masked method of selecting viewport to render
    bool indexedVport = (0 != m_Pickers[RND_GPU_PROG_IS_VPORT_SELECT_AS_INDEXED].Pick());
    if (!indexedVport && !m_pGLRandom->HasExt(GLRandomTest::ExtLW_viewport_array2))
    {
        indexedVport = true;
    }
    m_pGLRandom->m_GpuPrograms.SetIsLwrrVportIndexed(indexedVport);

    if (m_pFpCtx->LoopNum == m_pGLRandom->StartLoop())
        LogFixedPrograms();

    // Free random programs from previous frame, if any.

    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        m_Progs[pk].ClearProgs(psRandom);
    }

    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_vertex_program3) )
    {
        // Generate new random programs for this frame.
        CHECK_RC( GenerateRandomPrograms() );
        for (int pk = 0; pk < PgmKind_NUM; pk++)
        {
            Printf(Tee::PriLow
                    ," %s programs loaded: %d fixed,  %d user,  %d random \n"
                    ,PgmGen::pkNames[pk]
                    ,m_Progs[pk].NumProgs(psFixed)
                    ,m_Progs[pk].NumProgs(psUser)
                    ,m_Progs[pk].NumProgs(psRandom)
                    );
        }

        // Note: Vertex and Fragment programs may not have the same size pgmElw[]
        //
        // We really don't have anything special for the geometry or fragment
        // programs but we need to put in something interesting, so just use the
        // randomness of the light settings.
        // lights (in sets of 4 vxprog const registers)
        GLint maxLights = 0;
        for (int i = pkVertex; i < PgmKind_NUM; i++)
        {
            maxLights =
                MAX(maxLights,
                   (m_Gen[i]->GetPgmElwSize() - GpuPgmGen::FirstLightReg)/4);
        }

        GLint light;
        for (light = 0; light < maxLights; light++)
        {
            GLfloat vecs[4][4];

            // Light direction.
            m_pGLRandom->PickNormalfv(&vecs[0][0], GL_TRUE/* true means Z >= 0 */);
            vecs[0][3] = 1.0;

            // Light-eye half-angle, for spelwlar highlights.
            // Add eye vector (0,0,1) to light direction vector, normalize.
            const double v1x = vecs[0][0] + 0.0;
            const double v1y = vecs[0][1] + 0.0;
            const double v1z = vecs[0][2] + 1.0;
            const double norm = 1.0 / sqrt((v1x*v1x) + (v1y*v1y) + (v1z*v1z));
            vecs[1][0] = static_cast<GLfloat>(v1x * norm);
            vecs[1][1] = static_cast<GLfloat>(v1y * norm);
            vecs[1][2] = static_cast<GLfloat>(v1z * norm);
            vecs[1][3] = 0.0f;

            // Light color.
            vecs[2][0] = m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0);
            vecs[2][1] = m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0);
            vecs[2][2] = m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0);
            vecs[2][3] = m_pFpCtx->Rand.GetRandomFloat(0.6, 1.0);

            // Spelwlar power (shininess exponent).
            vecs[3][0] = m_pFpCtx->Rand.GetRandomFloat(64.0, 128.0);

            for (int pk = 0; pk < PgmKind_NUM; pk++)
            {
                if (! m_pGLRandom->HasExt(PgmGen::pkExtensions[pk]))
                    continue;

                GLint numLights =
                    (m_Gen[pk]->GetPgmElwSize() - GpuPgmGen::FirstLightReg)/4;

                if (light >= numLights)
                {
                    continue;
                }

                // Next light index (wraps around)
                vecs[3][1] = (GLfloat) m_Gen[pk]->LightToRegIdx((light + 1) % numLights);
                // Light number.
                vecs[3][2] = (GLfloat) (light % 4);
                vecs[3][3] = (GLfloat) (light % 16);

                GLint reg = GpuPgmGen::LightToRegIdx(light);

                const GLenum target = PgmGen::pkTargets[pk];
                glProgramParameter4fvLW (target, reg + 0, vecs[0]);
                glProgramParameter4fvLW (target, reg + 1, vecs[1]);
                glProgramParameter4fvLW (target, reg + 2, vecs[2]);
                glProgramParameter4fvLW (target, reg + 3, vecs[3]);
            }
            if (OK != (rc = mglGetRC()))
            {
                Printf(Tee::PriHigh, "glProgramParameter4fvLW()\n");
                return rc;
            }
        }

        // write any remaining registers with known constants for debugging purposes
        for (int pk = 0; pk < PgmKind_NUM; pk++)
        {
            if (! m_pGLRandom->HasExt(PgmGen::pkExtensions[pk]))
                continue;
            const GLenum target = PgmGen::pkTargets[pk];
            int numLights =
                (m_Gen[pk]->GetPgmElwSize() - GpuPgmGen::FirstLightReg)/4;

            GLint reg = GpuPgmGen::LightToRegIdx(numLights);
            for ( ; reg < m_Gen[pk]->GetPgmElwSize(); reg++)
            {
                glProgramParameter4fLW (target, reg, 0.0f, 1.0f, 2.0f, 3.0f);
            }
        }

        PickFragmentElwPerFrame();

        #ifdef CREATE_INCONSISTENT_SHADER
        if(m_pGLRandom->GetLogMode() == GLRandomTest::Store)
            glProgramParameter4fLW(GL_FRAGMENT_PROGRAM_LW, 255, 1.7f, 1.2f, 1.9f, 1.5f);
        else
            glProgramParameter4fLW(GL_FRAGMENT_PROGRAM_LW, 255, -1.7f, -1.2f, -1.9f, -1.5f);
        #endif

        if (OK != (rc = mglGetRC()))
        {
            Printf(Tee::PriHigh, "glProgramParameter4fLW()\n");
            return rc;
        }

        rc = InitSboData(1<<sboWrite);

        // Pick a new random offset for the SBOs
        GLint       Rect[4] = {0,0,0,0};
        glGetIntegerv(GL_SCISSOR_BOX,Rect);

        const GLint offsetX = (GLint)m_pFpCtx->Rand.GetRandom(0,
                Rect[sboWidth] - m_Sbos[sboWrite].Rect[sboWidth]);
        const GLint offsetY = (GLint)m_pFpCtx->Rand.GetRandom(0,
                Rect[sboHeight] - m_Sbos[sboWrite].Rect[sboHeight]);

        m_Sbos[sboRead].Rect[sboOffsetX] = offsetX;
        m_Sbos[sboRead].Rect[sboOffsetY] = offsetY;
        m_Sbos[sboWrite].Rect[sboOffsetX] = offsetX;
        m_Sbos[sboWrite].Rect[sboOffsetY] = offsetY;
    }

    rc = InitPaBO();
    return rc;
}

RC RndGpuPrograms::InitPaBO()
{
    RC rc = OK;
    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_parameter_buffer_object2))
    {
        for (int pk = 0; pk < PgmKind_NUM; pk++)
        {
            if( PgmGen::PaBOTarget[pk] != 0)
            {
                glBindBufferBaseLW(PgmGen::PaBOTarget[pk],0,m_PaBO[pk]);
                const int numRegs = m_Gen[pk]->GetPgmElwSize();
                for (int idx = 0; idx < numRegs; idx += 4)
                {
                    // I don't have anything special to put into these buffers so for
                    // now I'll just put in normalized values.
                    float vec[4];
                    vec[0] = m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0);
                    vec[1] = m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0);
                    vec[2] = m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0);
                    vec[3] = m_pFpCtx->Rand.GetRandomFloat(0.6, 1.0);
                    glProgramBufferParametersfvLW(PgmGen::PaBOTarget[pk],0,idx,4,vec);
                }
            }
        }
        if (OK != (rc = mglGetRC()))
        {
            Printf(Tee::PriHigh, "InitPaBO::glProgramBufferParametersfvLW()\n");
        }
    }
    return rc;
}
//------------------------------------------------------------------------------
void RndGpuPrograms::PickFragmentElwPerFrame()
{
    // Set randomized program parameters.

    float vec[4];
    vec[0] = m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0);
    vec[1] = m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0);
    vec[2] = m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0);
    vec[3] = m_pFpCtx->Rand.GetRandomFloat(0.6, 1.0);
    glProgramParameter4fvLW (GL_FRAGMENT_PROGRAM_LW,
                             FrGpuPgmGen::RandomNrmReg + 0,
                             vec);

    vec[0] = m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0);
    vec[1] = m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0);
    vec[2] = m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0);
    vec[3] = m_pFpCtx->Rand.GetRandomFloat(0.6, 1.0);
    glProgramParameter4fvLW (GL_FRAGMENT_PROGRAM_LW,
                             FrGpuPgmGen::RandomNrmReg + 4,
                             vec);

    vec[0] = m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0) - 1.0f;
    vec[1] = m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0) - 1.0f;
    vec[2] = m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0) - 1.0f;
    vec[3] = m_pFpCtx->Rand.GetRandomFloat(0.6, 1.0) - 1.0f;
    glProgramParameter4fvLW (GL_FRAGMENT_PROGRAM_LW,
                             FrGpuPgmGen::RandomSgnNrmReg,
                             vec);
}
string ProgramIndexToString(uint32 index)
{
    return Utility::StrPrintf("RndPgmIdx_%d", index);
}
//------------------------------------------------------------------------------
RC RndGpuPrograms::GenerateRandomPrograms()
{
    const UINT32 NumProgsPickIdxs[PgmKind_NUM] =
    {
        RND_GPU_PROG_VX_NUMRANDOM
        ,RND_GPU_PROG_TC_NUMRANDOM
        ,RND_GPU_PROG_TE_NUMRANDOM
        ,RND_GPU_PROG_GM_NUMRANDOM
        ,RND_GPU_PROG_FR_NUMRANDOM
    };

    int maxProgsDownstream = 0;
    RC rc;

    const UINT32 seed = m_pGLRandom->GetSeed(m_pFpCtx->LoopNum);

    // Generate program kinds down- to upstream order: fragment ... vertex.

    for (int pk = PgmKind_NUM-1; pk >= 0; pk--)
    {
        m_Gen[pk]->Restart();
        int numToGenerate = m_Pickers[NumProgsPickIdxs[pk]].Pick();

        if (m_StrictPgmLinking)
        {
            // If we are strictly matching programs, we need to generate
            // at least as many progs as the max found downstream of us.
            numToGenerate = max(numToGenerate, maxProgsDownstream);
        }
        if (! m_pGLRandom->HasExt(PgmGen::pkExtensions[pk]))
            numToGenerate = 0;

        if (!numToGenerate)
            continue;

        maxProgsDownstream = max(maxProgsDownstream, numToGenerate);

        m_Progs[pk].SetSeed(seed);  // for generating program name
        m_pGLRandom->LogBegin(numToGenerate, ProgramIndexToString);

        for (int pi = 0; pi < numToGenerate; pi++)
        {
            // In strict linking mode, we do the non-fragment Enable picks
            // at Restart time, so that each fragment program has a fixed
            // set of upstream programs it links with.
            //
            // Upstream programs that are not enabled in this program chain
            // are marked with dummy programs.

            if (m_StrictPgmLinking &&
                (pk != pkFragment) &&
                (0 == m_Pickers[PgmGen::pkPickEnable[pk]].Pick()))
            {
                m_Gen[pk]->GenerateDummyProgram(&m_Progs[pk],PgmGen::pkTargets[pk]);
                continue;
            }

            // Find the nearest downstream random program at this index, if any.
            // Treat its input requirements as our output requirements.
            // This guarantees that each program will be a "match" for at
            // least one program upstream of it.

            Program * pDownPgm = NULL;
            for (int downpk = pk+1; downpk < PgmKind_NUM; downpk++)
            {
                if (pi < m_Progs[downpk].NumProgs(psRandom))
                {
                    pDownPgm = m_Progs[downpk].Get(psRandom, pi);
                    if (! pDownPgm->IsDummy())
                    {
                        break;
                    }
                }
            }

            if (pDownPgm)
            {
                GLRandom::PgmRequirements * pDownReq = &(pDownPgm->PgmRqmt);
                GLRandom::PgmRequirements MyReq;

                MyReq.RsvdTxf = pDownReq->RsvdTxf;
                MyReq.Outputs = pDownReq->Inputs;
                if (pk == pkTessCtrl || pk == pkTessEval)
                {
                    MyReq.PrimitiveOut  = pDownReq->PrimitiveIn;
                }
                MyReq.VxPerPatchOut = pDownReq->VxPerPatchIn;
                MyReq.OutTxc  = pDownReq->InTxc;
                MyReq.UsrComments.push_back(
                        Utility::StrPrintf("Created for %s", pDownPgm->Name.c_str()));

                // Merge in the downstream pgm's UsedTxf with our RsvdTxf.
                PgmRequirements::TxfMapItr iter;
                for (iter = pDownReq->UsedTxf.begin();
                        iter != pDownReq->UsedTxf.end();
                            iter++)
                {

                    // If a fetcher is using an ImageUnit, then the mmLevel,
                    // unit, & elems values must match, because we are going to
                    // bind the same texture object to both the ImageUnit and
                    // fetcher.
                    const int txf = iter->first;
                    if (MyReq.RsvdTxf.count(txf))
                    {
                        MASSERT((MyReq.RsvdTxf[txf].Attrs & SASMNbDimFlags) ==
                                (iter->second.Attrs & SASMNbDimFlags));
                        if (MyReq.RsvdTxf[txf].IU.unit >= 0)
                        {
                            if ( iter->second.IU.unit >= 0)
                            {
                                 MASSERT((MyReq.RsvdTxf[txf].IU.unit ==
                                            iter->second.IU.unit)  &&
                                        (MyReq.RsvdTxf[txf].IU.mmLevel ==
                                            iter->second.IU.mmLevel) &&
                                        (MyReq.RsvdTxf[txf].IU.elems ==
                                            iter->second.IU.elems));

                                MyReq.RsvdTxf[txf].Attrs |= iter->second.Attrs;
                                MyReq.RsvdTxf[txf].IU.access |=
                                    iter->second.IU.access;
                            }
                        }
                        else
                        {
                            MyReq.RsvdTxf[txf].Attrs |= iter->second.Attrs;
                            MyReq.RsvdTxf[txf].IU.access |= iter->second.IU.access;
                            MyReq.RsvdTxf[txf].IU.unit    = iter->second.IU.unit;
                            MyReq.RsvdTxf[txf].IU.mmLevel = iter->second.IU.mmLevel;
                            MyReq.RsvdTxf[txf].IU.elems   = iter->second.IU.elems;
                        }
                    }
                    else
                    {
                        MyReq.RsvdTxf[txf].Attrs |= iter->second.Attrs;
                        MyReq.RsvdTxf[txf].IU.access |= iter->second.IU.access;
                        MyReq.RsvdTxf[txf].IU.unit    = iter->second.IU.unit;
                        MyReq.RsvdTxf[txf].IU.mmLevel = iter->second.IU.mmLevel;
                        MyReq.RsvdTxf[txf].IU.elems   = iter->second.IU.elems;
                    }
                }

                m_Gen[pk]->GenerateRandomProgram(&MyReq, &m_Progs[pk],
                    Utility::StrPrintf("%s%s%08x%03d",
                        PgmGen::pkNicknames[pk], m_Progs[pk].PgmSrcToStr(psRandom), seed, pi));
            }
            else
            {
                m_Gen[pk]->GenerateRandomProgram(NULL, &m_Progs[pk],
                    Utility::StrPrintf("%s%s%08x%03d",
                        PgmGen::pkNicknames[pk], m_Progs[pk].PgmSrcToStr(psRandom), seed, pi));
            }

            // Load the program, catch errors
            Program * pProg = m_Progs[pk].Get(psRandom, pi);
            MASSERT(pProg);
            // If using user programs don't bother loading this program.
            if (m_Progs[pk].NumProgs(psUser) == 0)
            {
                CHECK_RC(LoadProgram(pProg));
            }

            // Log the program
            m_pGLRandom->LogData(pi, pProg,
                    ((char*)&pProg->EndOfChecksummedArea - (char*)pProg));
            LogProgramRequirements(pi, pProg->PgmRqmt);
            m_pGLRandom->LogData(pi, pProg->Pgm.c_str(), pProg->Pgm.size());
            m_pGLRandom->LogData(pi, pProg->Name.c_str(), pProg->Name.size());

        } // each program

        string logName = Utility::StrPrintf("%sProg", PgmGen::pkNames[pk]);
        CHECK_RC(m_pGLRandom->LogFinish(logName.c_str()));

    } // each program kind

    return rc;
}

//-----------------------------------------------------------------------------
// Creating two Shader Buffer Objects the same size of the display surface
// consumes too much memory. Each element must be uniquely addressible and
// support data formats of upto 256 bits (32 bytes). So reduce the size to
// around 3MBs total.
RC RndGpuPrograms::CreateSbos()
{
    GLint size = 0;
    if ( !m_pGLRandom->GetUseSbos() )
        return OK;

    // Create R/W VBOs, that are accessable to all shaders.
    for (int idx = sboRead; idx < sboMaxNum; idx++)
    {
        GLenum mode = GL_READ_WRITE;
        m_Sbos[idx].colorFmt = ColorUtils::RU32;
        glGenBuffers( 1, &m_Sbos[idx].Id);
        glBindBuffer(GL_ARRAY_BUFFER, m_Sbos[idx].Id);   // read-only Sbo
        switch (idx)
        {
            case sboRead:
                mode = GL_READ_ONLY;
                // fall through
            case sboWrite:
                m_pGLRandom->GetSboSize(&m_Sbos[idx].Rect[sboWidth], &m_Sbos[idx].Rect[sboHeight]);
                m_Sbos[sboWrite].Rect[sboOffsetX] = 0;
                m_Sbos[sboWrite].Rect[sboOffsetY] = 0;

                size = m_Sbos[idx].Rect[sboWidth] *
                       m_Sbos[idx].Rect[sboHeight] *
                       sboRWElemSize;
                break;
        }
        // allocate storage, data initialization oclwrs in InitSboData()
        glBufferData(GL_ARRAY_BUFFER,   // target
                     size,              //sboSize,
                     NULL,              // data
                     GL_DYNAMIC_COPY);  // hint
        // get gpu address
        glGetBufferParameterui64vLW(
                    GL_ARRAY_BUFFER_ARB,
                    GL_BUFFER_GPU_ADDRESS_LW,
                    &m_Sbos[idx].GpuAddr);

        // make it accessable to all the shaders.
        glMakeBufferResidentLW(GL_ARRAY_BUFFER_ARB, mode);
    }

    // release the current binding for now
    glBindBuffer(GL_ARRAY_BUFFER,0);

    return mglGetRC();
}

//-----------------------------------------------------------------------------
RC RndGpuPrograms::PrintSbos(int sboMask, int startLine, int endLine)
{
    if ( !m_pGLRandom->GetUseSbos() )
        return OK;

    if (endLine < 0)
        endLine = startLine;
    MASSERT( (startLine >= 0) && (endLine >= 0) && (endLine >= startLine));

    Tee::Priority pri = Tee::PriHigh;
    // make sure all writes to these SBOs are complete
    glMemoryBarrierEXT(GL_BUFFER_UPDATE_BARRIER_BIT_EXT);

    for (UINT32 idx = sboRead; idx < sboMaxNum; idx++)
    {
        if (0 == (sboMask & (1 << idx)))
            continue;

        MASSERT((startLine < m_Sbos[idx].Rect[sboHeight]) &&
                (endLine < m_Sbos[idx].Rect[sboHeight]));

        glBindBuffer(GL_ARRAY_BUFFER, m_Sbos[idx].Id);
        UINT32 * pData =
            (UINT32*)glMapBuffer(GL_ARRAY_BUFFER_ARB, GL_READ_WRITE);
        MASSERT(pData);

        const UINT32 wordsPerPixel = sboRWElemSize/sizeof(UINT32);
        const UINT32 widthInPixels = m_Sbos[idx].Rect[sboWidth];

        // The r/w sbos are 512x25 by default with 8 dwords per "pixel".
        // Print 8 dwords per row in mods.log for easier reading.
        const UINT32 pixPerRow = 8 / wordsPerPixel;

        for (int line = startLine; line <= endLine; line++)
        {
            const UINT32 startLineOffset = widthInPixels * wordsPerPixel * line;
            for (UINT32 col = 0; col < widthInPixels; col++)
            {
                const bool beginRow = (0           == col % pixPerRow);
                const bool endRow   = (pixPerRow-1 == col % pixPerRow);
                if (beginRow)
                    Printf(pri, "(%3d,%2d) ", col, line);

                for (UINT32 w = 0; w < wordsPerPixel; w++)
                {
                    Printf(pri, " %08x", MEM_RD32(pData + startLineOffset +
                                                  col * wordsPerPixel + w));
                }

                if (endRow)
                    Printf(pri, "\n");
            }
        }

        glUnmapBuffer(GL_ARRAY_BUFFER_ARB);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    return mglGetRC();
}

//-----------------------------------------------------------------------------
void RndGpuPrograms::GetSboState( GLRandom::SboState * pSbo, int sbo)
{
    MASSERT(sbo >= sboRead && sbo < sboMaxNum);
    *pSbo = m_Sbos[sbo];
}
//-----------------------------------------------------------------------------
RC RndGpuPrograms::InitSboData(int sboMask)
{
    if ( !m_pGLRandom->GetUseSbos() )
        return OK;

    // Pick some random data for the Shader Buffer Objects
    UINT32 numElems;

    // Make sure all writes to these buffers are complete before changing.
    glMemoryBarrierEXT(GL_BUFFER_UPDATE_BARRIER_BIT_EXT);
    for (UINT32 idx = sboRead; idx < sboMaxNum; idx++)
    {
        if ( sboMask & (1 << idx))
        {
            glBindBuffer(GL_ARRAY_BUFFER, m_Sbos[idx].Id);

            UINT32 * pData = (UINT32*)glMapBuffer(GL_ARRAY_BUFFER_ARB, GL_READ_WRITE);
            MASSERT(pData);

            numElems = m_Sbos[idx].Rect[sboWidth] * m_Sbos[idx].Rect[sboHeight];
            numElems *= sboRWElemSize/sizeof(UINT32); // need to support 256bit elements

            for ( UINT32 i = 0; i < numElems; i++)
            {
                switch(idx)
                {
                    case sboRead:
                        MEM_WR32(&pData[i], m_pFpCtx->Rand.GetRandom());
                        break;
                    case sboWrite:
                        MEM_WR32(&pData[i],i);
                        break;
                }
            }
            glUnmapBuffer(GL_ARRAY_BUFFER_ARB);
        }
    }
    glMemoryBarrierEXT(GL_BUFFER_UPDATE_BARRIER_BIT_EXT);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return mglGetRC();
}

//-----------------------------------------------------------------------------
void RndGpuPrograms::UpdateSboLocalElw(GLenum target)
{
    if ((target == GL_FRAGMENT_PROGRAM_LW) && m_pGLRandom->GetUseSbos())
    {
        // Shader Buffer Object gpuaddress.
        glProgramLocalParameterI4uiLW (GL_FRAGMENT_PROGRAM_LW,
                GpuPgmGen::SboRwAddrs,
                static_cast<GLuint>(m_Sbos[sboRead].GpuAddr),
                static_cast<GLuint>(m_Sbos[sboRead].GpuAddr >> 32),
                static_cast<GLuint>(m_Sbos[sboWrite].GpuAddr),
                static_cast<GLuint>(m_Sbos[sboWrite].GpuAddr >> 32));
    }
    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_bindless_texture))
    {
        // Texture object gpuaddress buffer.
        const GLRandom::SboState * pSbo =
            m_pGLRandom->m_Texturing.GetBindlessTextureState();

        glProgramLocalParameterI4uiLW(
            target,
            GpuPgmGen::SboTxAddrReg,
            (UINT32)(pSbo->GpuAddr),        // lo32
            (UINT32)(pSbo->GpuAddr >> 32),  // hi32
            0, 0);
    }
}

//-----------------------------------------------------------------------------
void RndGpuPrograms::UpdateSboPgmElw(int pgmKind)
{
    const GLenum target = PgmGen::pkTargets[pgmKind];

    if ( m_pGLRandom->GetUseSbos() )
    {
        // Dimensions of the SBO and where is virtually is placed over the
        // viewport.
        glProgramElwParameterI4iLW (target,
                GpuPgmGen::SboWindowRect,
                (UINT32)m_Sbos[sboWrite].Rect[sboWidth],
                (UINT32)m_Sbos[sboWrite].Rect[sboHeight],
                (UINT32)m_Sbos[sboWrite].Rect[sboOffsetX],
                (UINT32)m_Sbos[sboWrite].Rect[sboOffsetY]);

    }
    else
    {
        glProgramElwParameter4fARB (target,
                GpuPgmGen::SboWindowRect,
                m_State[pgmKind].Random[0], // was m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0)
                m_State[pgmKind].Random[1], // was m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0)
                m_State[pgmKind].Random[2], // was m_pFpCtx->Rand.GetRandomFloat(0.2, 1.0)
                m_State[pgmKind].Random[3]);// was m_pFpCtx->Rand.GetRandomFloat(0.6, 1.0)
    }

}

//-----------------------------------------------------------------------------
// Create a set of Constant Parameter Buffer Objects (PaBO) for use by the shaders.
//
RC RndGpuPrograms::CreatePaBOs()
{
    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_parameter_buffer_object2))
    {
        glGenBuffersARB(PgmKind_NUM, m_PaBO);
        for (int pk = pkVertex; pk < PgmKind_NUM; pk++)
        {
            if (PgmGen::PaBOTarget[pk] != 0)
            {
                const int numRegs = m_Gen[pk]->GetPgmElwSize();
                glBindBufferARB(PgmGen::PaBOTarget[pk], m_PaBO[pk]);
                glBufferDataARB(PgmGen::PaBOTarget[pk], // target
                                numRegs*sizeof(float),  // size
                                NULL,                   // data
                                GL_STREAM_COPY);        // hint
                glBindBufferARB(PgmGen::PaBOTarget[pk], 0);
            }
        }
        return mglGetRC();
    }
    return OK;
}

//-----------------------------------------------------------------------------
void RndGpuPrograms::Pick()
{
    deque<Program *> pgmChain;
    bool bPick = (m_Pickers[RND_GPU_PROG_LP_PICK_PGMS].Pick() != 0);

    const bool firstLoop =
        ((m_pFpCtx->LoopNum % m_pGLRandom->LoopsPerFrame()) == 0) ||
        (m_pFpCtx->LoopNum == m_pGLRandom->StartLoop());

    // Pick current layer and viewport for this loop
    m_LwrrentLayer = m_Pickers[RND_GPU_PROG_LAYER_OUT].Pick();
    m_LwrrViewportIdxOrMask = m_pGLRandom->m_GpuPrograms.GetIsLwrrVportIndexed() ?
      m_Pickers[RND_GPU_PROG_VPORT_IDX].Pick() :
      m_Pickers[RND_GPU_PROG_VPORT_MASK].Pick();

    // we must pick on the first loop of each frame to stay consistent.
    if (!bPick && !firstLoop)
        return;

    memset(m_State, 0, sizeof(m_State));
    memset(&m_ExtraState, 0, sizeof(m_ExtraState));

    PickEnablesAndBindings();
    PickAtomicModifierMasks();

    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        switch (pk)
        {
            case pkVertex:
                m_State[pk].Light0     = m_Pickers[RND_GPU_PROG_VX_LIGHT0].Pick();
                m_State[pk].Light1     = m_Pickers[RND_GPU_PROG_VX_LIGHT1].Pick();
                m_State[pk].PtSize     = m_Pickers[RND_GPU_PROG_VX_POINTSZ_VAL].FPick();
                m_ExtraState.VxClampColor = (0 != m_Pickers[RND_GPU_PROG_VX_CLAMP_COLOR_ENABLE].Pick());
                m_ExtraState.VxTwoSided = (0 != m_Pickers[RND_GPU_PROG_VX_TWOSIDE].Pick());
                m_ExtraState.VxSendPtSize = (0 != m_Pickers[RND_GPU_PROG_VX_POINTSZ].Pick());

                if (m_State[pkFragment].Enable)
                {
                    if (!m_Progs[pkFragment].Lwr()->IsTwoSided())
                        m_ExtraState.VxTwoSided = false;
                    if (!m_Progs[pkFragment].Lwr()->ReadsPointSize())
                        m_ExtraState.VxSendPtSize = false;
                }

                CommonPickPgmElw(static_cast<pgmKind>(pk));
                break;

            case pkTessCtrl:
                m_ExtraState.TessOuter[0] =
                    m_Pickers[RND_GPU_PROG_TC_TESSRATE_OUTER].FPick();
                m_ExtraState.TessOuter[1] =
                    m_Pickers[RND_GPU_PROG_TC_TESSRATE_OUTER].FPick();
                m_ExtraState.TessOuter[2] =
                    m_Pickers[RND_GPU_PROG_TC_TESSRATE_OUTER].FPick();
                m_ExtraState.TessOuter[3] =
                    m_Pickers[RND_GPU_PROG_TC_TESSRATE_OUTER].FPick();
                m_ExtraState.TessInner[0] =
                    m_Pickers[RND_GPU_PROG_TC_TESSRATE_INNER].FPick();
                m_ExtraState.TessInner[1] =
                    m_Pickers[RND_GPU_PROG_TC_TESSRATE_INNER].FPick();
                CommonPickPgmElw(static_cast<pgmKind>(pk));
                break;

            case pkTessEval:
                m_ExtraState.TessBulge = m_Pickers[RND_GPU_PROG_TE_BULGE].FPick();
                CommonPickPgmElw(static_cast<pgmKind>(pk));
                break;

            case pkGeometry:
                m_State[pk].Light0 = m_Pickers[RND_GPU_PROG_GM_LIGHT0].Pick();
                m_State[pk].Light1 = m_Pickers[RND_GPU_PROG_GM_LIGHT1].Pick();
                CommonPickPgmElw(static_cast<pgmKind>(pk));
                if (m_State[pk].Enable &&
                    static_cast<GmGpuPgmGen*>(m_Gen[pkGeometry])->IsLwrrGMProgPassThru())
                {
                    m_pGLRandom->m_Misc.XfbCaptureNotPossible();
                }
                break;

            default:
                CommonPickPgmElw(static_cast<pgmKind>(pk));
                break;
        }
    }

    // Get our total vertex multiplier (ratio of input to output vertexes)
    // below the user-specified limit, to control draw time.

    const float maxMult = max(1.0F, static_cast<float>
                              (m_pGLRandom->GetMaxVxMultiplier()));
    while (GetVxMultiplier() > maxMult)
    {
        bool reducedMult = false;
        // First, try reducing tessellation rates.
        for (size_t i = 0; i < Utility::ArraySize(m_ExtraState.TessOuter); i++)
        {
            if (m_ExtraState.TessOuter[i] > 1.0F)
            {
                reducedMult = true;
                m_ExtraState.TessOuter[i] -= 1.0F;
            }
        }
        for (size_t i = 0; i < Utility::ArraySize(m_ExtraState.TessInner); i++)
        {
            if (m_ExtraState.TessInner[i] > 1.0F)
            {
                reducedMult = true;
                m_ExtraState.TessInner[i] -= 1.0F;
            }
        }
        if (!reducedMult)
        {
            // We can't disable any of the programs at this point without calling
            // CheckPgmRequirements(), which is called inside of PickEnablesAndBindings().
            // The intent of this code is to reduce test time for Fmodel & Emulation. So
            // just do our best at reducing the multiplier as much as possible and print a
            // warning if we cant get to the final limit.
            Printf(Tee::PriLow, "Unable to reduce the VxMultiplier of %2.2f down to %2.2f.\n",
                   GetVxMultiplier(), maxMult);
            break;
        }
    }

    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        const UINT32 pickIdx = PgmGen::pkPickPgmIdxDebug[pk];
        m_Pickers[pickIdx].Pick();
    }

    m_pGLRandom->LogData(RND_GPU_PROG, &m_State, sizeof(m_State));
    m_pGLRandom->LogData(RND_GPU_PROG, &m_ExtraState, sizeof(m_ExtraState));

    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        UINT32 tmp = m_Progs[pk].LwrCombinedIdx();
        m_pGLRandom->LogData(RND_GPU_PROG, &tmp, sizeof(tmp));
    }

    m_pGLRandom->LogData(RND_GPU_PROG, &m_LwrrentLayer, sizeof(m_LwrrentLayer));
    m_pGLRandom->LogData(RND_GPU_PROG, &m_LwrrViewportIdxOrMask,
                          sizeof(m_LwrrViewportIdxOrMask));
}

//----------------------------------------------------------------------------
void RndGpuPrograms::PickAtomicModifierMasks()
{
    // The goal here is to allow as many atomic instructions in a single shader
    // as possible. Some operations are complementary if you can control the
    // bits they access for a give memory location(ie, AND/OR/XOR). So we
    // create some masks to allow each operation to modify a select set of bits
    // or use constrained values.
    // Note: These masks must be consistent for all shader stages.
    GLuint64 mask[ammNUM] =
    {
        // ammAnd
        0xFFFFFF00FFFFFF00ULL | (m_pFpCtx->Rand.GetRandom64(0, 0xFF) |
                                (m_pFpCtx->Rand.GetRandom64(0, 0xFF) << 32)),
        // ammOr
        0x0000FF000000FF00ULL & ((m_pFpCtx->Rand.GetRandom64(0, 0xFF) << 8) |
                                 (m_pFpCtx->Rand.GetRandom64(0, 0xff) <<(32+8))),
        // ammXor
        0x00FF000000FF0000ULL & ((m_pFpCtx->Rand.GetRandom64(0, 0xFF) << 16) |
                                 (m_pFpCtx->Rand.GetRandom64(0, 0xFF) <<(16+32))),
        m_pFpCtx->Rand.GetRandom64(0, 0xFFFFFFFFFFFFFFFFULL), // ammMinMax
        m_pFpCtx->Rand.GetRandom64(0, 0xFFFFFFFFFFFFFFFFULL), // ammAdd
        m_pFpCtx->Rand.GetRandom64(0, 0xFFFFFFFFFFFFFFFFULL), // ammWrap
        m_pFpCtx->Rand.GetRandom64(0, 0xFFFFFFFFFFFFFFFFULL), // ammXchange
        m_pFpCtx->Rand.GetRandom64(0, 0xFFFFFFFFFFFFFFFFULL), // ammCSwap
    };

    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        m_State[pk].AtomicMask[ammAnd]      = mask[ammAnd   ];
        m_State[pk].AtomicMask[ammOr]       = mask[ammOr    ];
        m_State[pk].AtomicMask[ammXor]      = mask[ammXor   ];
        m_State[pk].AtomicMask[ammMinMax]   = mask[ammMinMax];
        m_State[pk].AtomicMask[ammAdd]      = mask[ammAdd   ];
        m_State[pk].AtomicMask[ammWrap]     = mask[ammWrap  ];
        m_State[pk].AtomicMask[ammExch]     = mask[ammExch];
        m_State[pk].AtomicMask[ammCSwap]    = mask[ammCSwap ];
    }
}
//-----------------------------------------------------------------------------
void RndGpuPrograms::PickEnablesAndBindings()
{
    // Pick enable/disable and current program for each kind of program.
    bool bStrictPgmLinking = m_StrictPgmLinking;
    do
    {
        deque<GLRandom::Program*> chain;

        pgmKind downstreamKind = PgmKind_NUM;

        for (int pk = PgmKind_NUM-1; pk >= 0; pk--)
        {
            const pgmKind kind = static_cast<pgmKind>(pk);

            // Enable/disable:
            if (bStrictPgmLinking && (kind != pkFragment))
            {
                m_State[kind].Enable = true;
            }
            else
            {
                m_State[kind].Enable =
                        (0 != m_Pickers[PgmGen::pkPickEnable[kind]].Pick());
            }

            // The selected pgm-idx is checked by PickConsistency, so force a
            // deterministic value here.
            m_Progs[kind].ClearLwr();

            if (0 == m_Progs[kind].NumProgs())
                m_State[kind].Enable = false;

            if (! m_pGLRandom->HasExt(PgmGen::pkExtensions[kind]))
                m_State[kind].Enable = false;

            if (!m_State[kind].Enable)
                continue;

            // Pick which program to bind:

            int downstreamIdx = (downstreamKind == PgmKind_NUM) ? 0 :
                    m_Progs[downstreamKind].LwrCombinedIdx();

            const bool downstreamIsRandom =
                    (downstreamIdx >= psRandom) && (downstreamIdx < psUser);

            const bool downstreamIsUser =
                    (downstreamIdx >= psUser) && (downstreamIdx < psSpecial);

            const bool haveEnoughRandoms =
                    m_Progs[kind].NumProgs(psRandom) > (downstreamIdx - psRandom);

            const bool haveEnoughUsers =
                    m_Progs[kind].NumProgs(psUser) > (downstreamIdx - psUser);

            if (bStrictPgmLinking &&
                    ((downstreamIsRandom && haveEnoughRandoms) ||
                     (downstreamIsUser && haveEnoughUsers))
                )
            {
                // Use exactly the user or random program generated for the immediate
                // downstream program, i.e. use programs in matched sets only.

                m_Progs[kind].SetLwr (downstreamIdx);

                if (m_Progs[kind].Lwr()->IsDummy())
                {
                    m_State[kind].Enable = false;
                    continue;
                }

                if (OK == CheckPgmRequirements (&chain, m_Progs[kind].Lwr()))
                {
                    downstreamKind = kind;
                }
                else
                {
                    MASSERT(!"requirements fail with strict linking");
                    m_State[kind].Enable = false;
                }
            }
            else
            {
                // Mix & match programs randomly.
                m_Progs[kind].PickLwr (&m_Pickers[PgmGen::pkPickPgmIdx[kind]],
                                       &m_pFpCtx->Rand);

                const UINT32 numProgs = m_Progs[kind].NumProgs();
                UINT32 tries;
                for (tries = 0; tries < numProgs; tries++)
                {
                    if (OK == CheckPgmRequirements (&chain, m_Progs[kind].Lwr()))
                    {
                        // success
                        downstreamKind = kind;
                        break;
                    }

                    // Doesn't meet requirements.  Try the next program.
                    m_Progs[kind].PickNext();
                }
                if (tries == numProgs)
                {
                    // Couldn't satisfy requirements -- disable program kind.
                    m_State[kind].Enable = false;

                    Printf(Tee::PriLow,
                            "Can't satisfy requirements, disabling %s programs.\n",
                            PgmGen::pkNames[kind]);
                }
            }
        } // kind

        // If we are using the mix & match approach and we were forced to disable the
        // Vertex program fallback to strict program linking. Not having a vertex shader when the
        // original pick required it should be given a higher priority than the mix and match mode.
        // Note: Not having a vertex shader forces GL to use fixed function vertex processing
        // which is deprecated and hasn't been verified in years! I would be suprised if it still
        // works with all of the new GL features implemented in the past 5 years.
        if (!bStrictPgmLinking &&
            m_State[pkVertex].Enable != (0 != m_Pickers[PgmGen::pkPickEnable[pkVertex]].GetPick()))
        {
            if (m_VerboseOutput)
            {
                Printf(Tee::PriNormal,
                       "Loop:%d PickEnablesAndBindings() failed to use a required VtxPgm"
                       ", enabling strict program linking.\n", m_pFpCtx->LoopNum);
            }
            bStrictPgmLinking = true;
        }
        else
        {
            bStrictPgmLinking = false;
        }
    } while (bStrictPgmLinking);
}

//------------------------------------------------------------------------------
// Pick the common program environment settings for a specific shader.
void RndGpuPrograms::CommonPickPgmElw(pgmKind pk)
{
    const int numRegs = m_Gen[pk]->GetPgmElwSize();
    const int numLights = (numRegs - GpuPgmGen::FirstLightReg)/4;

    m_State[pk].Light0 %= numLights;
    m_State[pk].Light1 %= numLights;

    m_State[pk].PtSize = 2.0;

    const int maxTxCoordIdx  = m_pGLRandom->NumTxCoords()/2;
    const int maxPgmElwIdx   = numRegs/2;
    const int maxTmpArrayIdx = (GpuPgmGen::TempArrayLen/2)-1;

    int maxSubIdx = GpuPgmGen::MaxSubs-1;
    if (m_State[pk].Enable && m_Progs[pk].Lwr()->SubArrayLen)
    {
        maxSubIdx = m_Progs[pk].Lwr()->SubArrayLen - 1;
    }

    for (int i = 0; i < 4; i++)
    {
        m_State[pk].TxLookup[i]     = m_pFpCtx->Rand.GetRandom(0, maxTxCoordIdx);
        m_State[pk].PgmElwIdx[i]    = m_pFpCtx->Rand.GetRandom(0, maxPgmElwIdx);
        m_State[pk].TmpArrayIdxs[i] = m_pFpCtx->Rand.GetRandom(0, maxTmpArrayIdx);
        m_State[pk].SubIdxs[i]      = m_pFpCtx->Rand.GetRandom(0, maxSubIdx);
        m_State[pk].TxHandleIdx[i]  = m_pFpCtx->Rand.GetRandom(0, NumTxHandles-1);
    }

    m_State[pk].Random[0]  = m_pFpCtx->Rand.GetRandomFloat(0.2,1.0);
    m_State[pk].Random[1]  = m_pFpCtx->Rand.GetRandomFloat(0.2,1.0);
    m_State[pk].Random[2]  = m_pFpCtx->Rand.GetRandomFloat(0.2,1.0);
    m_State[pk].Random[3]  = m_pFpCtx->Rand.GetRandomFloat(0.6,1.0);
}

//------------------------------------------------------------------------------
// Make sure the output requirements of front node meet the downstream
// requirements.
//
// If it does, return OK and push the new upstream program onto the chain.
// Otherwise return error and leave the chain alone.

RC RndGpuPrograms::CheckPgmRequirements
(
    deque<GLRandom::Program*> *pChain,
    Program * pProposedNextUpstream
)
{
    RC rc;

    // Spelwlatively push the new program on the chain.
    // Remember to pop it off before returning error!
    pChain->push_front(pProposedNextUpstream);
    const int chainLen = (int) pChain->size();

    if (chainLen == 1)
    {
        // Can't use programs that need SBOs if they are not enabled!
        if (pProposedNextUpstream->PgmRqmt.NeedSbos &&
            !m_pGLRandom->GetUseSbos())
        {
            pChain->pop_front();
            if (m_VerboseOutput)
            {
                Printf(Tee::PriNormal,"%s needs SBOs but they are not enabled!\n",
                    pProposedNextUpstream->Name.c_str());
            }
            return RC::SOFTWARE_ERROR;
        }
        else
        {
            // A single program always matches its own requirements.
            return OK;
        }
    }

    if (m_VerboseOutput)
    {
        Printf(Tee::PriNormal, "%s:", __FUNCTION__);
        const char * arrow = "";

        for (int i = 0; i < chainLen; i++)
        {
            Printf(Tee::PriNormal, " %s %s", arrow, (*pChain)[i]->Name.c_str());
            arrow = "->";
        }
        Printf(Tee::PriNormal, "\n");
    }

    // Check that tess or geometry programs match primitive types.
    string badPrimStr;
    if (OK != CheckPgmPrims(pChain, &badPrimStr))
    {
        rc = RC::SOFTWARE_ERROR;
        if (m_VerboseOutput)
        {
            Printf (Tee::PriNormal, "%s ", badPrimStr.c_str());
        }
    }

    // Make sure each input of the downstream program is written by the
    // new upstream program's outputs.
    {
        PgmRequirements::PropMap & upOut  = (*pChain)[0]->PgmRqmt.Outputs;
        PgmRequirements::PropMap & downIn = (*pChain)[1]->PgmRqmt.Inputs;

        for (ProgProperty prop = ppPos; prop < ppNUM_ProgProperty; prop++)
        {
            const int in  = downIn.count(prop) ? downIn[prop] : 0;
            const int out = upOut.count(prop)  ? upOut[prop]  : 0;
            const int pgmKind = PgmGen::TargetToPgmKind((*pChain)[0]->Target);
            const int outIoMask = m_Gen[pgmKind]->GetPropIOAttr(prop);
            //If all of the following are true, then fail:
            // a)Downstream shader is reading the property
            // b)Upstream shader is capable of writing this property
            // c)Upstream didn't write all the required components of the property.
            if ((in != 0) && (outIoMask & ppIOWr) && ((out & in) != in))
            {
                rc = RC::SOFTWARE_ERROR;
                if (m_VerboseOutput)
                {
                    Printf (Tee::PriNormal,"Prop %s Output 0x%x != Input 0x%x ",
                        GpuPgmGen::PropToString((INT32)prop).c_str(),
                        out, in);
                }
                break;
            }
        }
    }

    // Texture fetchers are a global resource shared by all programs.
    //
    // Build a map of texture-fetcher requirements for all programs.
    // Make sure that there are no conflicts between programs in the chain.

    PgmRequirements::TxfMap & txfMapUp = (*pChain)[0]->PgmRqmt.UsedTxf;

    for (int i = 1; (i < chainLen) && (OK == rc); i++)
    {
        PgmRequirements::TxfMap & txfMapDown = (*pChain)[i]->PgmRqmt.UsedTxf;

        PgmRequirements::TxfMapItr it;
        for (it = txfMapUp.begin(); it != txfMapUp.end(); ++it)
        {
            const int txf = it->first;
            if (txfMapDown.count(txf))
            {
                const int upAttr = it->second.Attrs & SASMNbDimFlags;
                const int dnAttr = txfMapDown[txf].Attrs & SASMNbDimFlags;

                // Basic attributes match?
                // Since RndTexturing::ValidateTexture is used to match textures
                // to fetchers, this attribute check should match the attribute
                // check in RndTexturing::ValidateTexture (dnAttr is supplying
                // the requirements for upAttr)
                if (dnAttr != (upAttr & dnAttr))
                {
                    rc = RC::SOFTWARE_ERROR;
                    if (m_VerboseOutput)
                    {
                        Printf (Tee::PriNormal, "TxFetcher %d  %s:%s != %s:%s",
                                txf,
                                (*pChain)[0]->Name.c_str(),
                                TxAttrToString(upAttr).c_str(),
                                (*pChain)[i]->Name.c_str(),
                                TxAttrToString(dnAttr).c_str());
                    }
                    break;
                }
            }
        }
    }

    // Make sure the referenced image units don't cause write violations.
    // For example if both Vtx & Frag are writing to image unit 0, that could
    // be a problem.
    for (PgmRequirements::TxfMapItr itup = pProposedNextUpstream->PgmRqmt.UsedTxf.begin();
          itup != pProposedNextUpstream->PgmRqmt.UsedTxf.end();
          ++itup)
    {
        int unit = itup->second.IU.unit;
        if (unit >= 0)
        {
            // See if this image unit is being used in the other programs.
            for (int i = 1; (i < chainLen) && (OK == rc); i++)
            {
                PgmRequirements::TxfMap & txfMapDown = (*pChain)[i]->PgmRqmt.UsedTxf;
                for (PgmRequirements::TxfMapItr itdown = txfMapDown.begin();
                     itdown != txfMapDown.end();
                     ++itdown)
                {
                    if ((itdown->second.IU.unit == unit) &&
                        (itdown->second.IU.access == iuaWrite ||
                         itup->second.IU.access == iuaWrite))
                    {
                        rc = RC::SOFTWARE_ERROR;
                        if (m_VerboseOutput)
                        {
                            Printf (Tee::PriNormal,
                                    "ImageUnit%d write violation, upstream pgm is using Txf:%d &"
                                    " downstream pgm is using Txf:%d\n",
                                    unit, itup->first, itdown->first);
                        }
                        break;
                    }
                }
            }

        }
    }

    // Any texture coord used for TXFMS must be passed through the chain
    // unmodified from the value generated by RndVertexes.
    //
    // Walk the chain from downstream to upstream to verify this.

    for (int downIdx = chainLen-1; downIdx > 0 && OK == rc; downIdx--)
    {
        PgmRequirements & downReq = (*pChain)[downIdx]->PgmRqmt;
        PgmRequirements::PropMap &upOuts =(*pChain)[downIdx-1]->PgmRqmt.Outputs;

        for (PgmRequirements::TxcMapItr iter = downReq.InTxc.begin();
                iter != downReq.InTxc.end(); ++iter)
        {
            const ProgProperty prop = iter->second.Prop;

            if (downReq.TxcRequires(prop, Is2dMultisample, ioIn))
            {
                MASSERT(prop != ppIlwalid);
                const int out = upOuts.count(prop) ? upOuts[prop] : 0;
                if (0 == (out & omPassThru))
                {
                    rc = RC::SOFTWARE_ERROR;
                    if (m_VerboseOutput)
                    {
                        Printf (Tee::PriNormal,
                                "%s output prop %s is not omPassThru",
                                (*pChain)[downIdx-1]->Name.c_str(),
                                GpuPgmGen::PropToString(prop).c_str());
                    }
                    break;
                }
            }
        }
    }

    if (m_VerboseOutput)
        Printf(Tee::PriHigh, "... %s.\n", rc == OK ? "passed" : "failed");

    if (OK != rc)
        pChain->pop_front();

    return rc;
}

//------------------------------------------------------------------------------
RC RndGpuPrograms::CheckPgmPrims
(
    deque<GLRandom::Program*> *pChain,
    string * pErrorMessage
)
{
    // Make sure the new upstream program (at link 0 of the chain) writes
    // the primitive expected by the next downstream program (at link 1).
    //
    // Also, for patches, the downstream reader mustn't read more vertices than
    // the upstream writer produces.
    //
    // A primitive of -1 here means "don't care".
    // I.e. vertex and fragment programs don't deal with primitives.

    const int primUpOut  = (*pChain)[0]->PgmRqmt.PrimitiveOut;
    const int primDownIn = (*pChain)[1]->PgmRqmt.PrimitiveIn;
    const int vxPerPatchUpOut  = (*pChain)[0]->PgmRqmt.VxPerPatchOut;
    const int vxPerPatchDownIn = (*pChain)[1]->PgmRqmt.VxPerPatchIn;

    if ((primUpOut == -1) || (primDownIn == -1))
        return OK;

    if (primUpOut != primDownIn)
    {
        *pErrorMessage = Utility::StrPrintf("prim mismatch, %s to %s ",
                GpuPgmGen::PrimitiveToString(primUpOut).c_str(),
                GpuPgmGen::PrimitiveToString(primDownIn).c_str());
        return RC::SOFTWARE_ERROR;
    }

    if (primUpOut == GL_PATCHES && vxPerPatchDownIn > vxPerPatchUpOut)
    {
        *pErrorMessage = Utility::StrPrintf("patch size mismatch, %d to %d ",
                vxPerPatchUpOut, vxPerPatchDownIn);
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//------------------------------------------------------------------------------
void RndGpuPrograms::GetAllRefTextureAttribs(GLRandom::TxfSet *pTxfReq)
{
    pTxfReq->clear();

    const pgmSource srcs[] = { psUser, psFixed, psRandom };

    for (int pk = PgmKind_NUM-1; pk >= 0; pk--)
    {
        for (size_t ps = 0; ps < NUMELEMS(srcs); ps++)
        {
            const pgmSource pgmSrc = srcs[ps];

            for (int i = 0; i < m_Progs[pk].NumProgs(pgmSrc); i++)
            {
                PgmRequirements * pRqmt =
                        &(m_Progs[pk].Get(pgmSrc, i)->PgmRqmt);

                PgmRequirements::TxfMapItr iter;
                unsigned int bindlessTxAttr = pRqmt->BindlessTxAttr;

                for (iter = pRqmt->UsedTxf.begin();
                        iter != pRqmt->UsedTxf.end();
                            iter++)
                {
                    GLRandom::TxfReq req(
                        iter->second.Attrs,
                        iter->second.IU.unit,
                        iter->second.IU.mmLevel,
                        iter->second.IU.access,
                        iter->second.IU.elems);

                    GLRandom::TxfSetItr it = pTxfReq->find(req);
                    if( it != pTxfReq->end())
                    {
                        // we only want to keep the largest MMLevel.
                        if (it->IU.mmLevel < req.IU.mmLevel)
                        {
                            pTxfReq->erase(it);
                            pTxfReq->insert(req);
                        }
                    }
                    else
                    {
                        pTxfReq->insert(req);
                    }

                    // don't double count for bindless texture requirements.
                    bindlessTxAttr &= ~(iter->second.Attrs & IsDimMask);
                }
                // Check any bindless texture attributes requirements.
                if (bindlessTxAttr)
                {
                    for (unsigned flag=Is1d; flag<IsDimMask; flag=flag << 1)
                    {
                        if (bindlessTxAttr & flag)
                        {
                            GLRandom::TxfReq req(bindlessTxAttr & flag);
                            pTxfReq->insert(req);
                        }
                    }
                }
            } // next program
        } // next program src (usr,fixed,random)
    } // next program kind( vtx,geo,etc)
}

//------------------------------------------------------------------------------
void RndGpuPrograms::Print(Tee::Priority pri)
{
    // Only print when we are drawing vertices, not pixels.
    if (RND_TSTCTRL_DRAW_ACTION_vertices != m_pGLRandom->m_Misc.DrawAction())
        return;

    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        if (! m_State[pk].Enable)
        {
            Printf(pri, "\t%s program disabled.\n", PgmGen::pkNames[pk]);
            continue;
        }

        Printf(pri,
                "\t%s Prog=%s ID=%d L=%d,%d txLU=%d,%d,%d,%d peIdx=%d,%d,%d,%d taIdx=%d,%d,%d,%d",
               PgmGen::pkNames[pk],
               m_Progs[pk].Lwr()->Name.c_str(),
               m_Progs[pk].Lwr()->Id,
               m_State[pk].Light0,
               m_State[pk].Light1,
               m_State[pk].TxLookup[0],
               m_State[pk].TxLookup[1],
               m_State[pk].TxLookup[2],
               m_State[pk].TxLookup[3],
               m_State[pk].PgmElwIdx[0],
               m_State[pk].PgmElwIdx[1],
               m_State[pk].PgmElwIdx[2],
               m_State[pk].PgmElwIdx[3],
               m_State[pk].TmpArrayIdxs[0],
               m_State[pk].TmpArrayIdxs[1],
               m_State[pk].TmpArrayIdxs[2],
               m_State[pk].TmpArrayIdxs[3]);
        if (m_Progs[pk].Lwr()->SubArrayLen)
        {
            Printf(pri, " subIdx=%d,%d,%d,%d",
                    m_State[pk].SubIdxs[0],
                    m_State[pk].SubIdxs[1],
                    m_State[pk].SubIdxs[2],
                    m_State[pk].SubIdxs[3]);
        }

        if (UseBindlessTextures())
        {
            Printf(pri, " txHndIdx=%d,%d,%d,%d",
                    m_State[pk].TxHandleIdx[0],
                    m_State[pk].TxHandleIdx[1],
                    m_State[pk].TxHandleIdx[2],
                    m_State[pk].TxHandleIdx[3]);
        }

        if (pkVertex == pk)
        {
            Printf(pri, " C0=att%d %s",
                   m_Progs[pkVertex].Lwr()->Col0Alias,
                   m_ExtraState.VxTwoSided  ? " 2side" : ""
                  );
            if (m_ExtraState.VxSendPtSize)
            {
                Printf(pri, " PtSz %0.0f", m_State[pkVertex].PtSize);
            }
        }
        Printf(pri, "\n");
    }

    if (m_State[pkTessEval].Enable)
    {
        Printf(pri, " Tessellation Outer=%f,%f,%f,%f Inner=%f,%f Bulge=%f\n",
               m_ExtraState.TessOuter[0],
               m_ExtraState.TessOuter[1],
               m_ExtraState.TessOuter[2],
               m_ExtraState.TessOuter[3],
               m_ExtraState.TessInner[0],
               m_ExtraState.TessInner[1],
               m_ExtraState.TessBulge);
    }

    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        if (m_State[pk].Enable)
        {
            m_Progs[pk].Lwr()->Print(pri);
            m_Progs[pk].Lwr()->PgmRqmt.Print(pri);
        }
    }

    if (m_pGLRandom->m_Misc.XfbEnabled(xfbModeCapture) &&
        m_pGLRandom->m_Misc.XfbIsPlaybackPossible())
    {
        // dump the special vertex pass-thru shader
        m_Gen[pkVertex]->GenerateXfbPassthruProgram(&m_Progs[pkVertex]);
        // Get the last special program,it should be the new pass-thru program.
        if ( m_Progs[pkVertex].NumProgs(psSpecial) > 0)
        {
            Program * pProg = m_Progs[pkVertex].Get(psSpecial,
                m_Progs[pkVertex].NumProgs(psSpecial)-1);
            pProg->Print(pri);
        }
    }
}

//-----------------------------------------------------------------------------
RC RndGpuPrograms::Send()
{
    RC rc;
    // For debugging
    static int pgmUseCounts[PgmKind_NUM][pgmSource_NUM] = {{0}};
    static unsigned int FpMsCnt = 0;

    if (m_pGLRandom->LogShaderCombinations())
    {
        CreateBugJsFile();
    }

    if (RND_TSTCTRL_DRAW_ACTION_vertices != m_pGLRandom->m_Misc.DrawAction())
        return OK;

    // Do last-minute debug overrides on enables and bindings.
    //
    // Beware!
    //
    // Since all the dependent picks (textures, vertices, XFB, etc) were
    // made based on the "real" picks not these overrides, the override must
    // be compatible.  Otherwise expect GL errors and bad rendering.
    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        const UINT32 pickIdx = PgmGen::pkPickPgmIdxDebug[pk];
        const UINT32 override = m_Pickers[pickIdx].GetPick();

        if (override == RND_GPU_PROG_INDEX_DEBUG_NO_OVERRIDE)
        {
            continue;  // normal case
        }
        else if (override == RND_GPU_PROG_INDEX_DEBUG_FORCE_DISABLE)
        {
            m_State[pk].Enable = false;
        }
        else
        {
            m_State[pk].Enable = true;
            m_Progs[pk].PickLwr(&m_Pickers[pickIdx], 0);
        }
    }

    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        if (!m_State[pk].Enable)
            continue;

        const GLenum target = PgmGen::pkTargets[pk];

        glEnable(target);
        CHECK_RC(BindProgram(m_Progs[pk].Lwr()));

        m_Gen[pk]->ReportTestCoverage(m_Progs[pk].Lwr()->Pgm);

        UpdateSboPgmElw(pk);

        if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_gpu_program4))
        {
            // Random relative index values for indexing into the result.texcoord[]
            // or vertex.texcoord[] from within a program.
            glProgramElwParameterI4iLW (
                                   target,
                                   GpuPgmGen::RelativeTxReg,      // index
                                   m_State[pk].TxLookup[0],   // x component
                                   m_State[pk].TxLookup[1],   // y component
                                   m_State[pk].TxLookup[2],   // z component
                                   m_State[pk].TxLookup[3]    // w component
                                   );

            // Random relative index values for extracting a bindless texture
            // object handle.
            glProgramElwParameterI4iLW (
                                   target,
                                   GpuPgmGen::RelativeTxHndlReg,      // index
                                   m_State[pk].TxHandleIdx[0],   // x component
                                   m_State[pk].TxHandleIdx[1],   // y component
                                   m_State[pk].TxHandleIdx[2],   // z component
                                   m_State[pk].TxHandleIdx[3]    // w component
                                   );

            // Random relative index values for indexing into pgmElw[].
            glProgramElwParameterI4iLW (
                                   target,
                                   GpuPgmGen::RelativePgmElwReg,  // index
                                   m_State[pk].PgmElwIdx[0],  // x component
                                   m_State[pk].PgmElwIdx[1],  // y component
                                   m_State[pk].PgmElwIdx[2],  // z component
                                   m_State[pk].PgmElwIdx[3]   // w component
                                   );

            // Random 0 or 1 values for indexing into tmpArray[].
            glProgramElwParameterI4iLW (
                                   target,
                                   GpuPgmGen::TmpArrayIdxReg,
                                   m_State[pk].TmpArrayIdxs[0],
                                   m_State[pk].TmpArrayIdxs[1],
                                   m_State[pk].TmpArrayIdxs[2],
                                   m_State[pk].TmpArrayIdxs[3]);

            // Random function-pointer index values for indexing into Subs[].
            glProgramElwParameterI4iLW (
                                   target,
                                   GpuPgmGen::SubIdxReg,
                                   m_State[pk].SubIdxs[0],
                                   m_State[pk].SubIdxs[1],
                                   m_State[pk].SubIdxs[2],
                                   m_State[pk].SubIdxs[3]);

            // Atomic operation modifier masks
            glProgramElwParameterI4iLW (
                                   target,
                                   GpuPgmGen::AtomicMask1,
                                   (GLuint)(m_State[pk].AtomicMask[ammAnd   ] & 0xFFFFFFFF),
                                   (GLuint)(m_State[pk].AtomicMask[ammOr    ] & 0xFFFFFFFF),
                                   (GLuint)(m_State[pk].AtomicMask[ammXor   ] & 0xFFFFFFFF),
                                   (GLuint)(m_State[pk].AtomicMask[ammMinMax] & 0xFFFFFFFF));

            glProgramElwParameterI4iLW (
                                   target,
                                   GpuPgmGen::AtomicMask2,
                                   (GLuint)(m_State[pk].AtomicMask[ammAdd   ] & 0xFFFFFFFF),
                                   (GLuint)(m_State[pk].AtomicMask[ammWrap  ] & 0xFFFFFFFF),
                                   (GLuint)(m_State[pk].AtomicMask[ammExch  ] & 0xFFFFFFFF),
                                   (GLuint)(m_State[pk].AtomicMask[ammCSwap ] & 0xFFFFFFFF));

            // these masks will get packed into a 64 bit temp variable in the shaders like this
            // LONG TEMP atom64;
            // PK64.U atom64.xy, pgmElw[20]; # for ATOM.AND/ATOM.OR inst.
            // PK64.U atom64.zw, pgmElw[21]; # for ATOM.XOR/ATOM.MIN/ATOM.MAX inst.
            // PK64.U atom64.zw, pgmElw[22]; # for ATOM.ADD/ATOM.XCHNG/ATOM.CSWAP inst.
            glProgramElwParameterI4iLW (
                                   target,
                                   GpuPgmGen::AtomicMask3,
                                   (GLuint)(m_State[pk].AtomicMask[ammAnd   ] & 0xFFFFFFFF),
                                   (GLuint)(m_State[pk].AtomicMask[ammAnd   ] >> 32),
                                   (GLuint)(m_State[pk].AtomicMask[ammOr    ] & 0xFFFFFFFF),
                                   (GLuint)(m_State[pk].AtomicMask[ammOr    ] >> 32));

            glProgramElwParameterI4iLW (
                                   target,
                                   GpuPgmGen::AtomicMask4,
                                   (GLuint)(m_State[pk].AtomicMask[ammXor   ] & 0xFFFFFFFF),
                                   (GLuint)(m_State[pk].AtomicMask[ammXor   ] >> 32),
                                   (GLuint)(m_State[pk].AtomicMask[ammMinMax] & 0xFFFFFFFF),
                                   (GLuint)(m_State[pk].AtomicMask[ammMinMax] >> 32));

            // Note: we are going touse teh ammExch for both ATOM.EXCH & ATOM.CSWAP because
            // they are mutually exclusive instructions to keep the shaders deterministic.
            glProgramElwParameterI4iLW (
                                   target,
                                   GpuPgmGen::AtomicMask5,
                                   (GLuint)(m_State[pk].AtomicMask[ammAdd   ] & 0xFFFFFFFF),
                                   (GLuint)(m_State[pk].AtomicMask[ammAdd   ] >> 32),
                                   (GLuint)(m_State[pk].AtomicMask[ammExch  ] & 0xFFFFFFFF),
                                   (GLuint)(m_State[pk].AtomicMask[ammExch  ] >> 32));
        }
        else
        {
            // need to use floats
            // Random relative index values for indexing into the result.texcoord[]
            // or vertex.texcoord[] from within a program.
            glProgramParameter4fLW (
                                   target,
                                   GpuPgmGen::RelativeTxReg,      // index
                                   (float)m_State[pk].TxLookup[0],   // x component
                                   (float)m_State[pk].TxLookup[1],   // y component
                                   (float)m_State[pk].TxLookup[2],   // z component
                                   (float)m_State[pk].TxLookup[3]    // w component
                                   );

            // Random relative index values for extracting a bindless texture
            // object handle.
            glProgramParameter4fLW (
                                   target,
                                   GpuPgmGen::RelativeTxHndlReg,      // index
                                   (float)m_State[pk].TxHandleIdx[0],   // x component
                                   (float)m_State[pk].TxHandleIdx[1],   // y component
                                   (float)m_State[pk].TxHandleIdx[2],   // z component
                                   (float)m_State[pk].TxHandleIdx[3]    // w component
                                   );

            // Random relative index values for indexing into pgmElw[].
            glProgramParameter4fLW (
                                   target,
                                   GpuPgmGen::RelativePgmElwReg,  // index
                                   (float)m_State[pk].PgmElwIdx[0],  // x component
                                   (float)m_State[pk].PgmElwIdx[1],  // y component
                                   (float)m_State[pk].PgmElwIdx[2],  // z component
                                   (float)m_State[pk].PgmElwIdx[3]   // w component
                                   );

            // Random 0 or 1 values for indexing into tmpArray[].
            glProgramParameter4fLW (
                                   target,
                                   GpuPgmGen::TmpArrayIdxReg,
                                   (float)m_State[pk].TmpArrayIdxs[0],
                                   (float)m_State[pk].TmpArrayIdxs[1],
                                   (float)m_State[pk].TmpArrayIdxs[2],
                                   (float)m_State[pk].TmpArrayIdxs[3]);

            // Random function-pointer index values for indexing into Subs[].
            glProgramParameter4fLW (
                                   target,
                                   GpuPgmGen::SubIdxReg,
                                   (float)m_State[pk].SubIdxs[0],
                                   (float)m_State[pk].SubIdxs[1],
                                   (float)m_State[pk].SubIdxs[2],
                                   (float)m_State[pk].SubIdxs[3]);

            // Atomic operation modifier masks
            glProgramParameter4fLW (
                                   target,
                                   GpuPgmGen::AtomicMask1,
                                   (float)m_State[pk].AtomicMask[ammAnd   ],
                                   (float)m_State[pk].AtomicMask[ammOr    ],
                                   (float)m_State[pk].AtomicMask[ammXor   ],
                                   (float)m_State[pk].AtomicMask[ammMinMax]);

            glProgramParameter4fLW (
                                   target,
                                   GpuPgmGen::AtomicMask2,
                                   (float)m_State[pk].AtomicMask[ammAdd   ],
                                   (float)m_State[pk].AtomicMask[ammWrap  ],
                                   (float)m_State[pk].AtomicMask[ammExch  ],
                                   (float)m_State[pk].AtomicMask[ammCSwap ]);

            glProgramParameter4fLW (
                                   target,
                                   GpuPgmGen::AtomicMask3,
                                   (float)(m_State[pk].AtomicMask[ammAnd   ] & 0xFFFFFFFF),
                                   (float)(m_State[pk].AtomicMask[ammAnd   ] >> 32),
                                   (float)(m_State[pk].AtomicMask[ammOr    ] & 0xFFFFFFFF),
                                   (float)(m_State[pk].AtomicMask[ammOr    ] >> 32));

            glProgramParameter4fLW (
                                   target,
                                   GpuPgmGen::AtomicMask4,
                                   (float)(m_State[pk].AtomicMask[ammXor   ] & 0xFFFFFFFF),
                                   (float)(m_State[pk].AtomicMask[ammXor   ] >> 32),
                                   (float)(m_State[pk].AtomicMask[ammMinMax] & 0xFFFFFFFF),
                                   (float)(m_State[pk].AtomicMask[ammMinMax] >> 32));

            glProgramParameter4fLW (
                                   target,
                                   GpuPgmGen::AtomicMask5,
                                   (float)(m_State[pk].AtomicMask[ammAdd   ] & 0xFFFFFFFF),
                                   (float)(m_State[pk].AtomicMask[ammAdd   ] >> 32),
                                   (float)(m_State[pk].AtomicMask[ammExch] & 0xFFFFFFFF),
                                   (float)(m_State[pk].AtomicMask[ammExch] >> 32));

        }

        // Light index values & other constants.
        glProgramParameter4fLW (
                           target,
                           GpuPgmGen::LightIndexReg,
                           (GLfloat) GpuPgmGen::LightToRegIdx(m_State[pk].Light0),
                           (GLfloat) GpuPgmGen::LightToRegIdx(m_State[pk].Light1),
                           m_State[pk].PtSize,
                           64.0f
                           );

        // Set current layer and viewport to render into
        glProgramParameter4fLW(target,
                           GpuPgmGen::LayerAndVportIDReg,
                           (GLfloat) m_LwrrentLayer,
                           (GLfloat) m_LwrrViewportIdxOrMask,
                           0.0f, 0.0f);

        if (m_VerboseOutput)
        {
            if (m_Progs[pk].LwrCombinedIdx() < psRandom)
                pgmUseCounts[pk][0]++; // fixed
            else if(m_Progs[pk].LwrCombinedIdx() < psUser)
                pgmUseCounts[pk][1]++; // random
            else
                pgmUseCounts[pk][2]++; // user

            Printf(Tee::PriHigh,
                    "Binding program \"%15s\", GL_id=%d Fixed:%d, Rnd:%d Usr:%d",
                    m_Progs[pk].Lwr()->Name.c_str(),
                    m_Progs[pk].Lwr()->Id,
                    pgmUseCounts[pk][0],
                    pgmUseCounts[pk][1],
                    pgmUseCounts[pk][2]);

            if (pkFragment == pk)
            {
                if(m_Progs[pk].Lwr()->UsingGLFeature[Program::Multisample])
                    FpMsCnt++;

                Printf(Tee::PriHigh, " MS=%s, MsCnt=%d\n",
                        m_Progs[pkFragment].Lwr()->UsingGLFeature[Program::Multisample] ? "on ":"off",
                        FpMsCnt);
            }
            else
            {
                Printf(Tee::PriHigh, "\n");
            }
        }
    }

    if (m_State[pkVertex].Enable)
    {
        if (m_ExtraState.VxSendPtSize)
            glEnable(GL_VERTEX_PROGRAM_POINT_SIZE_LW);

        if (m_ExtraState.VxTwoSided)
            glEnable(GL_VERTEX_PROGRAM_TWO_SIDE_LW);
        glClampColorARB(GL_CLAMP_VERTEX_COLOR_ARB, m_ExtraState.VxClampColor);
    }

    if (m_State[pkTessCtrl].Enable)
    {
        const GLenum target = PgmGen::pkTargets[pkTessCtrl];

        glProgramParameter4fLW (
                target,
                TcGpuPgmGen::TessOuterReg,  // index
                m_ExtraState.TessOuter[0],  // first outer edge rate
                m_ExtraState.TessOuter[1],  // second outer edge rate
                m_ExtraState.TessOuter[2],  // ...
                m_ExtraState.TessOuter[3]);
        glProgramParameter4fLW (
                target,
                TcGpuPgmGen::TessInnerReg,  // index
                m_ExtraState.TessInner[0],  // inner subdivision rate U
                m_ExtraState.TessOuter[1],  // inner subdivision rate V
                0.0F, 0.0F);
    }
    else if (m_State[pkTessEval].Enable)
    {
        glPatchParameterfvLW (GL_PATCH_DEFAULT_OUTER_LEVEL,
                m_ExtraState.TessOuter);
        glPatchParameterfvLW (GL_PATCH_DEFAULT_INNER_LEVEL,
                m_ExtraState.TessInner);
    }
    if (m_State[pkTessEval].Enable)
    {
        const GLenum target = PgmGen::pkTargets[pkTessEval];
        glProgramParameter4fLW (
                target,
                TeGpuPgmGen::TessBulge,  // index
                m_ExtraState.TessInner[0],  // inner subdivision rate U
                m_ExtraState.TessOuter[1],  // inner subdivision rate V
                m_ExtraState.TessBulge,
                0.0F);
    }
    return mglGetRC();
}

RC RndGpuPrograms::Playback()
{
    RC rc;
    // To play back a captured set of vertices we have to make sure the pipeline
    // is consistent. This means we have to make sure the shaders in the
    // pipeline only pass-thru the XFB attributes that were captured. We have
    // two options here.
    // 1. Build up a pgmRequirements structure for each shader that specifies
    //    passthru and call the random pgm generator to generate the random
    //    programs with those requirements.
    // 2. Disable all of the shaders(except fragment) and let the fixed function
    //    pipeline do it for us. This option didn't work!
    // 3. Generate a vertex pass-thru program that is tailored to the streamed
    //    attributes and disable the tessellation & geometry programs.
    // I'm going with option 3.
    CHECK_RC(m_Gen[pkVertex]->GenerateXfbPassthruProgram(&m_Progs[pkVertex]));

    // Get the last special program,it should be the new pass-thru program.
    if ( m_Progs[pkVertex].NumProgs(psSpecial) > 0)
    {
        Program * pProg = m_Progs[pkVertex].Get(psSpecial,
                m_Progs[pkVertex].NumProgs(psSpecial)-1);
        CHECK_RC(LoadProgram(pProg));
        glEnable(pProg->Target);
    }

    // Disable everything else except the fragment pgm.
    for (int pk = pkTessCtrl; pk < pkFragment; pk++)
    {
        if (m_State[pk].Enable)
        {
            glDisable(PgmGen::pkTargets[pk]);
        }
    }
    return mglGetRC();
}

RC RndGpuPrograms::UnPlayback()
{
    // Get the last special program and remove it from the list.
    if ( m_Progs[pkVertex].NumProgs(psSpecial) > 0)
    {
        Program * pProg = m_Progs[pkVertex].Get(psSpecial,
            m_Progs[pkVertex].NumProgs(psSpecial)-1);
        glDisable(pProg->Target);
        if (pProg->Id)
            glDeleteProgramsLW(1,&pProg->Id);

        m_Progs[pkVertex].RemoveProg(psSpecial,
            m_Progs[pkVertex].NumProgs(psSpecial)-1);
        return OK;
    }
    return RC::SOFTWARE_ERROR;
}

//-----------------------------------------------------------------------------
RC RndGpuPrograms::Cleanup()
{
    RC rc;
    if (m_pGLRandom->IsGLInitialized())
    {
        for (int pk = 0; pk < PgmKind_NUM; pk++)
        {
            if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_parameter_buffer_object2) &&
                    PgmGen::PaBOTarget[pk] != 0)
            {
                glBindBufferARB(PgmGen::PaBOTarget[pk], 0);
            }
            m_Progs[pk].ClearProgs(psFixed);
            m_Progs[pk].UnloadProgs(psUser);
            m_Progs[pk].ClearProgs(psRandom);
            m_Progs[pk].ClearProgs(psSpecial);
        }

        for (int idx = sboRead; idx < sboMaxNum; idx++)
        {
            if( m_Sbos[idx].Id != 0)
            {
                glBindBuffer(GL_ARRAY_BUFFER, m_Sbos[idx].Id);
                glMakeBufferNonResidentLW (GL_ARRAY_BUFFER_ARB);
                glDeleteBuffers(1, &m_Sbos[idx].Id);
                m_Sbos[idx].Id = 0;
            }
        }
        glBindBuffer(GL_ARRAY_BUFFER,0);

        if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_parameter_buffer_object2))
        {
            for (int idx = 0; idx < PgmKind_NUM; idx++)
            {
                if (m_PaBO[idx])
                {
                    glDeleteBuffers(1, &m_PaBO[idx]);
                    m_PaBO[idx] = 0;
                }
            }
        }
        rc = mglGetRC();
    }
    return rc;
}

//-----------------------------------------------------------------------------
bool RndGpuPrograms::VxProgEnabled() const
{
    return m_State[pkVertex].Enable;
}

//-----------------------------------------------------------------------------
bool RndGpuPrograms::FrProgEnabled() const
{
    return m_State[pkFragment].Enable;
}

//-----------------------------------------------------------------------------
bool RndGpuPrograms::GmProgEnabled() const
{
    return m_State[pkGeometry].Enable;
}

//-----------------------------------------------------------------------------
bool RndGpuPrograms::TessProgEnabled() const
{
    return m_State[pkTessEval].Enable || m_State[pkTessCtrl].Enable;
}

//-----------------------------------------------------------------------------
GLenum RndGpuPrograms::ExpectedPrim() const
{
    if (TessProgEnabled())
    {
        return GL_PATCHES;
    }
    if (GmProgEnabled())
    {
        MASSERT(m_Progs[pkGeometry].Lwr());
        return m_Progs[pkGeometry].Lwr()->PgmRqmt.PrimitiveIn;
    }
    return (GLenum)-1;  // any will do
}

//-----------------------------------------------------------------------------
// Returns the final privitive in the program chain before the clipping stage.
GLenum RndGpuPrograms::GetFinalPrimOut() const
{
    for (int pk = pkGeometry; pk >= pkVertex; pk--)
    {
        if (m_State[pk].Enable)
        {
            const Program * pLwr = m_Progs[pk].Lwr();
            MASSERT(pLwr);
            return pLwr->PgmRqmt.PrimitiveOut;
        }
    }
    return (GLenum)-1; // don't care
}

namespace { // anonymous namespace
bool anyTessRateZeroOrLess
(
    const GLfloat * pOuter,
    int numOuter,
    const GLfloat * pInner,
    int numInner
)
{
    for (int i = 0; i < numOuter; i++)
        if (pOuter[i] <= 0.0F)
            return true;
    for (int i = 0; i < numInner; i++)
        if (pInner[i] <= 0.0F)
            return true;
    return false;
}
} // anonymous namespace
//-----------------------------------------------------------------------------
//! Callwlate the approximate ratio of output/input vertexes produced by
//! the enabled shaders and shader options.
//!
//! Needed for a rough estimation of workload & limiting render time.
//!
float RndGpuPrograms::GetVxMultiplier() const
{
    float mult = 1.0F;
    if (m_State[pkGeometry].Enable)
    {
        mult = m_Progs[pkGeometry].Lwr()->PgmRqmt.VxMultiplier;

        // In practice, a multiplying geometry program seems to cause timeouts
        // in combination with tesselation.  Fudge the multiplier to exaggerate
        // the impact of geometry.
        mult = max(1.0F, mult * mult);
    }
    if (m_State[pkTessEval].Enable)
    {
        // See GL_ARB_tessellation_shader.txt re: the math below.
        // There are edge cases I'm ignoring, we only need a coarse guess.

        // All tess-rates are rounded up to integer for vx-counting.
        const float outerTR[] =
        {
            ceil(m_ExtraState.TessOuter[0]),
            ceil(m_ExtraState.TessOuter[1]),
            ceil(m_ExtraState.TessOuter[2]),
            ceil(m_ExtraState.TessOuter[3])
        };
        const float innerTR[] =
        {
            ceil(m_ExtraState.TessInner[0]),
            ceil(m_ExtraState.TessInner[1])
        };

        float vxOut = 1.0F;
        switch (m_Progs[pkTessEval].Lwr()->TessMode)
        {
            case GL_QUADS:
                if (anyTessRateZeroOrLess(m_ExtraState.TessOuter, 4,
                                          m_ExtraState.TessInner, 2))
                {
                    vxOut = 0.0F;
                }
                else
                {
                    // Outer tessellation levels 0..3 give number of vertexes
                    // along outside edge of the final rectangular grid.
                    // Each outer-edge vertex will participate in 2 or 3 tris.
                    vxOut = 3 *
                        (outerTR[0] + outerTR[1] + outerTR[2] + outerTR[3]);

                    // Inner tessellation levels 0,1 give number of inner
                    // subdivisions.  Remember outer have already been handled.
                    // Each inside vertex will participate in 6 triangles.
                    vxOut += 6 * max(1.0F, innerTR[0]-1) * max(1.0F, innerTR[1]-1);
                }
                break;
            case GL_TRIANGLES:
                if (anyTessRateZeroOrLess(m_ExtraState.TessOuter, 3,
                                          m_ExtraState.TessInner, 1))
                {
                    vxOut = 0.0F;
                }
                else
                {
                    // Outer tessellation levels 0..2 give number of vertexes
                    // along outside edge of the final triangular grid.
                    // Each outer-edge vertex will participate in 2 or 3 tris.
                    vxOut = 3 * (outerTR[0] + outerTR[1] + outerTR[2]);

                    // How many nested concentric inner triangles do we have?
                    //   inner-tess-rate 1: 0
                    //   inner-tess-rate 2: 1 (degenerate, a single vx)
                    //   ...             3: 1 (not degenerate: 3 vx)
                    //   ...             4: 2 (innermost degenerate)
                    //   ...             5: 2 (innermost not degenerate)
                    //
                    // A concentric triangle has 2 fewer vx per side than the
                    // the triangle outside it.
                    // Remember to skip the outermost triangle, its edges
                    // were already counted with the TessOuter stuff above.
                    // Each inside vertex will participate in 5 or 6 tris.

                    for (float vxPerSide = innerTR[0] - 2;
                         vxPerSide >= 0; vxPerSide -= 2)
                    {
                        if (vxPerSide == 0)
                            vxOut += 6 * 1;  // degenerate innermost triangle
                        else
                            vxOut += 6 * vxPerSide * 3;
                    }
                }
                break;
            case GL_ISOLINES:
            {
                // Each vertex will participate in 1 or 2 lines.
                vxOut = 2 * max(0.0F, outerTR[0] * outerTR[1]);
                break;
            }
            default:
                MASSERT(!"Unhandled tesselation mode");
                break;
        }
        mult *= (vxOut / m_Progs[pkTessEval].Lwr()->PgmRqmt.VxPerPatchIn);
    }

    return mult;
}

//-----------------------------------------------------------------------------
int RndGpuPrograms::ExpectedVxPerPatch() const
{
    if (TessProgEnabled())
    {
        if (m_State[pkTessCtrl].Enable)
            return m_Progs[pkTessCtrl].Lwr()->PgmRqmt.VxPerPatchIn;
        else
            return m_Progs[pkTessEval].Lwr()->PgmRqmt.VxPerPatchIn;
    }
    return -1;
}

//-----------------------------------------------------------------------------
VxAttribute RndGpuPrograms::Col0Alias() const
{
    if (m_State[pkVertex].Enable)
    {
        MASSERT(m_Progs[pkVertex].Lwr());
        return m_Progs[pkVertex].Lwr()->Col0Alias;
    }
    // else
    return att_COL0;
}

//------------------------------------------------------------------------------
UINT32 RndGpuPrograms::LogProgramRequirements (int index, PgmRequirements &PgmRqmt)
{
    UINT32 crc = 0;
    PgmRequirements::PropMapItr io;
    for (io = PgmRqmt.Inputs.begin(); io != PgmRqmt.Inputs.end(); io++)
    {
        crc = m_pGLRandom->LogData(index,&io->second, sizeof(int));
    }
    for (io = PgmRqmt.Outputs.begin(); io != PgmRqmt.Outputs.end(); io++)
    {
        crc = m_pGLRandom->LogData(index,&io->second, sizeof(int));
    }

    PgmRequirements::TxfMapItr txf;
    for (txf = PgmRqmt.UsedTxf.begin(); txf != PgmRqmt.UsedTxf.end();txf++)
    {
        crc = m_pGLRandom->LogData(index, &txf->second, sizeof(TxfReq));
    }

    for(txf = PgmRqmt.RsvdTxf.begin(); txf != PgmRqmt.RsvdTxf.end(); txf++)
    {
        crc = m_pGLRandom->LogData(index, &txf->second,sizeof(TxfReq));
    }

    PgmRequirements::TxcMapItr txc;
    for(txc = PgmRqmt.InTxc.begin(); txc != PgmRqmt.InTxc.end(); txc++)
    {
        crc = m_pGLRandom->LogData(index, &txc->second, sizeof(TxcReq));
    }

    for(txc = PgmRqmt.OutTxc.begin(); txc != PgmRqmt.OutTxc.end(); txc++)
    {
        crc = m_pGLRandom->LogData(index, &txc->second, sizeof(TxcReq));
    }
    return crc;
}

//------------------------------------------------------------------------------
// Non-member functions for printing shaders that caused asserts during
// LoadProgram
static void Program_Print(void * pvProgram)
{
    Program * pProg = (Program *)pvProgram;
    Printf(Tee::PriHigh,"Assert generated while compiling this program!\n");
    Printf(Tee::PriHigh,"%s\n", pProg->Pgm.c_str());
}

//------------------------------------------------------------------------------
// Load a vertex/fragment program to the GL driver, check it for errors.
//
RC RndGpuPrograms::LoadProgram (Program * pProg)
{
    StickyRC rc;
    MASSERT(pProg);

    // Print the shader we are compiling, if GL asserts.
    Platform::BreakPointHooker hooker(&Program_Print, pProg);

    if (pProg->IsDummy())
    {
        return OK;
    }
    if (0 != pProg->Id)
    {
        // Already loaded.
        glDeleteProgramsLW(1, &(pProg->Id));
        pProg->Id = 0;
    }
    glGenProgramsLW(1, &pProg->Id);
    glBindProgramLW(pProg->Target, pProg->Id);
    glProgramStringARB(pProg->Target,
                       GL_PROGRAM_FORMAT_ASCII_ARB,
                       (GLsizei)pProg->Pgm.size(),
                       (const GLubyte *)pProg->Pgm.c_str());

    if (OK != (rc = mglGetRC()))
    {
        //
        // Print a report to the screen and logfile, showing where the syntax
        // error oclwrred in the program string.
        //
        GLint errOffset=0;
        const GLubyte * errMsg = (const GLubyte *)"";

        glGetIntegerv(GL_PROGRAM_ERROR_POSITION_LW, &errOffset);
        errMsg = glGetString(GL_PROGRAM_ERROR_STRING_LW);

        Printf(Tee::PriHigh, "glLoadProgramLW err %s at %d,\n\"",
               (const char *)errMsg,errOffset);
        pProg->Print(Tee::PriHigh, errOffset);
        Printf(Tee::PriHigh, "\"\n");
    }

    // Set some program constants (program.local) that don't change during
    // a mods run (i.e. fragment-program sbo gpu vaddrs).
    // This must be done while the program object is bound.
    UpdateSboLocalElw(pProg->Target);

    if ( m_VerboseOutput){
        Printf(Tee::PriHigh,"Loaded program: %s\n",pProg->Name.c_str());
    }

    rc = Utility::DumpProgram(pProg->Name + ".glprog", pProg->Pgm.c_str(), pProg->Pgm.size());

    if (m_LogRndPgms)
    {
        pProg->Print(Tee::PriDebug);
        Printf(Tee::PriDebug, "\"\n");
        Printf(Tee::PriDebug, "XfbStdAttrMask:0x%x XfbUsrAttrMask:0x%x\n",
               pProg->XfbStdAttrib, pProg->XfbUsrAttrib);
        pProg->PgmRqmt.Print(Tee::PriDebug);
        pProg->PrintToFile();
    }
    if (m_LogToJsFile)
    {
        pProg->PrintToJsFile();
    }

    return rc;
}

//------------------------------------------------------------------------------
void RndGpuPrograms::LogFixedPrograms()
{
    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        for (int i = 0; i < m_Progs[pk].NumProgs(psFixed); i++)
        {
            Program * pProg = m_Progs[pk].Get(psFixed, i);
            if (m_LogRndPgms)
            {
                pProg->PrintToFile();
            }
            if (m_LogToJsFile)
            {
                pProg->PrintToJsFile();
            }
        }
    }
}

//------------------------------------------------------------------------------
void Program::PrintToFile () const
{
    if (IsDummy())
    {
        return;
    }
    FileHolder fh (Name + ".txt", "w+");

    FILE * pFile = fh.GetFile();
    if ( pFile)
    {
        fprintf(pFile,"%s\n", Pgm.c_str());
    }
}

//------------------------------------------------------------------------------
void Program::PrintToJsFile () const
{
    if (IsDummy())
    {
        return;
    }
    FileHolder fh (Name + ".js", "w+");

    FILE * pFile = fh.GetFile();
    if ( pFile)
    {
        fprintf(pFile, "%s\n", ToJsString().c_str());
    }
}

//------------------------------------------------------------------------------
string Program::ToJsString () const
{

    if (IsDummy())
    {
        // Empty program string -- we use this for dummy placeholder programs
        // in strict-linking mode, to indicate that this pgmKind should be left
        // disabled in the chain.
        return "dummy";
    }
    string s;
    pgmKind pk = PgmGen::TargetToPgmKind(Target);

    s = Utility::StrPrintf("    this.LoadUserGpu%sProgram(\n    %d,"
                           " // file format version",
            PgmGen::pkNames[pk], JSIFC_VERSION);
    s += PgmRqmt.ToJsString();

    switch (pk)
    {
        case pkVertex:
            s += "    1,//Col0Alias attribute \n";
            s += QuotedJsText();
            s += "\n    );\n";
            break;

        case pkTessCtrl:
        case pkGeometry:
            s += PgmRqmt.PrimToJsString();
            s += QuotedJsText();
            s += "\n    );\n";
            break;

        case pkTessEval:
            s += PgmRqmt.PrimToJsString();
            s += Utility::StrPrintf("    %d,//TessMode(GL_TRIANGLES/GL_ISOLINES/GL_QUADS)\n", TessMode);
            s += QuotedJsText();
            s += "\n    );\n";
            break;

        case pkFragment:
            s += QuotedJsText();
            s += "\n    );\n";
            break;

        default:
            MASSERT(!"Unknown program kind");
            break;
    }
    return s;
}

//------------------------------------------------------------------------------
bool Program::IsDummy() const
{
    return (0 == Pgm.size());
}

//------------------------------------------------------------------------------
void Program::CalcXfbAttributes()
{
    UINT32 attrib = 0;
    UINT32 generic = 0;

    for (PgmRequirements::ConstPropMapItr iter = PgmRqmt.Outputs.begin();
         iter != PgmRqmt.Outputs.end();
             iter++)
    {
        const int prop = (int)iter->first;
        switch (prop)
        {
            case ppPos       :  attrib |= RndGpuPrograms::Pos;      break;
            case ppNrml      :  generic|= RndGpuPrograms::Usr6;     break;
            case ppPriColor  :  attrib |= RndGpuPrograms::Col0;     break;
            case ppSecColor  :  attrib |= RndGpuPrograms::Col1;     break;
            case ppBkPriColor:  attrib |= RndGpuPrograms::BkCol0;   break;
            case ppBkSecColor:  attrib |= RndGpuPrograms::BkCol1;   break;
            case ppFogCoord  :  attrib |= RndGpuPrograms::Fog;      break;
            case ppPtSize    :  attrib |= RndGpuPrograms::PtSz;     break;
            case ppVtxId     :  attrib |= RndGpuPrograms::VertId;   break;
            case ppPrimId    :  attrib |= RndGpuPrograms::PrimId;   break;
            case ppClip0     :  attrib |= RndGpuPrograms::Clp0;     break;
            case ppClip0+1   :  attrib |= RndGpuPrograms::Clp1;     break;
            case ppClip0+2   :  attrib |= RndGpuPrograms::Clp2;     break;
            case ppClip0+3   :  attrib |= RndGpuPrograms::Clp3;     break;
            case ppClip0+4   :  attrib |= RndGpuPrograms::Clp4;     break;
            case ppClip5     :  attrib |= RndGpuPrograms::Clp5;     break;
            case ppTxCd0     :  attrib |= RndGpuPrograms::Tex0;     break;
            case ppTxCd0+1   :  attrib |= RndGpuPrograms::Tex1;     break;
            case ppTxCd0+2   :  attrib |= RndGpuPrograms::Tex2;     break;
            case ppTxCd0+3   :  attrib |= RndGpuPrograms::Tex3;     break;
            case ppTxCd0+4   :  attrib |= RndGpuPrograms::Tex4;     break;
            case ppTxCd0+5   :  attrib |= RndGpuPrograms::Tex5;     break;
            case ppTxCd0+6   :  attrib |= RndGpuPrograms::Tex6;     break;
            case ppTxCd7     :  attrib |= RndGpuPrograms::Tex7;     break;
            default:
                if( prop >= ppGenAttr0 && prop <= ppGenAttr31)
                {
                    generic |= 1 << (prop-ppGenAttr0);
                }
                break;
        }
    }
    XfbStdAttrib = attrib;
    XfbUsrAttrib = generic;
}

//------------------------------------------------------------------------------
string PgmRequirements::PrimToJsString() const
{
    return Utility::StrPrintf(
            "    [%d,%d,%d,%d,%.1f], //Prim In=%s Out=%s vxPerPatch in,out vxMult\n",
            PrimitiveIn,
            PrimitiveOut,
            VxPerPatchIn,
            VxPerPatchOut,
            VxMultiplier,
            GpuPgmGen::PrimitiveToString(PrimitiveIn).c_str(),
            GpuPgmGen::PrimitiveToString(PrimitiveOut).c_str());
}

//------------------------------------------------------------------------------
string PgmRequirements::ToJsString () const
{
    string s;
    const char * comma;
    s = "\n    [ // Program Requirements";
    s += "\n     [ // Texture fetchers used / texture object requirements:";
    s += "\n       // fetcher,attr,ImageUnit,MMLevel,Access,elems";
    comma = "";
    for (ConstTxfMapItr iter = UsedTxf.begin(); iter != UsedTxf.end(); iter++)
    {
        if (iter->second.Attrs & SASMNbDimFlags)
        {
            s += Utility::StrPrintf("%s\n         [ %d, %s, %d, %d, %d, %d ]",
                    comma,
                    iter->first,
                    TxAttrToString(iter->second.Attrs & SASMNbDimFlags).c_str(),
                    iter->second.IU.unit,
                    iter->second.IU.mmLevel,
                    iter->second.IU.access,
                    iter->second.IU.elems);
            comma = ",";
        }
    }
    s += "\n     ],\n";

    s += "\n     [ // Registers read:";
    comma = "";
    for (ConstPropMapItr iter = Inputs.begin(); iter != Inputs.end(); iter++)
    {
        s += Utility::StrPrintf("%s\n         [%s, %s]",
                comma,
                GpuPgmGen::PropToString(iter->first).c_str(),
                GpuPgmGen::CompToString(iter->second).c_str());
        comma = ",";
    }
    s += "\n     ],\n";

    s += "\n     [ // Registers written:";
    comma = "";
    for (ConstPropMapItr iter = Outputs.begin(); iter != Outputs.end(); iter++)
    {
        s += Utility::StrPrintf("%s\n         [%s, %s]",
                comma,
                GpuPgmGen::PropToString(iter->first).c_str(),
                GpuPgmGen::CompToString(iter->second).c_str());
        comma = ",";
    }
    s += "\n     ],\n";

    s += "\n     [ // Texture coord. properties:\n";
    s +=   "       // Prop,FetcherMask";
    comma = "";
    for (ConstTxcMapItr iter = InTxc.begin(); iter != InTxc.end(); iter++)
    {
        s += Utility::StrPrintf("%s\n         [%s,0x%x]",
            comma,
            GpuPgmGen::PropToString(iter->second.Prop).c_str(),
            iter->second.TxfMask);
        comma = ",";
    }
    s += "\n     ],\n";

    s += Utility::StrPrintf("     0x%x, //Need SBOs flag.\n",
        NeedSbos);
    s += Utility::StrPrintf("     0x%x, //Bindless texture attribute mask\n",
        BindlessTxAttr);
    s += Utility::StrPrintf("     %d    //EndStrategy (%s)\n",
        EndStrategy, GpuPgmGen::EndStrategyToString(EndStrategy));
    s += "    ],\n";
    return s;
}

//------------------------------------------------------------------------------
string Program::QuotedJsText () const
{
    // Produce a new string that contains JavaScript code for the program
    // as a concatenated string declaration.
    //
    // I.e. go from this:
    //   main:
    //     CAL sub_always;
    //
    // to this:
    //     "  main:\n" +
    //     "    CAL sub_always;\n" +
    //
    // but with no trailing + on the last line.

    vector<string> lines;
    if (OK != Utility::Tokenizer(Pgm, "\n", &lines))
    {
        MASSERT(!"Tokenizer failed");
        return string();
    }

    string s;
    const size_t numLines = lines.size();
    for (size_t i = 0; i < numLines; i++)
    {
        const char * plus = (i == numLines-1) ? "" : "+";

        s += "    \"" + lines[i] + "\\n\"" + plus + "\n";
    }
    return s;
}

//------------------------------------------------------------------------------
void Program::Print
(
    Tee::Priority pri,
    int errOffset /*=-1*/
) const
{
    if (IsDummy())
    {
        return;
    }
    const char * pstr = Pgm.c_str();
    const GLint  pgmLen = (GLint) Pgm.size();
    vector<char> buf (1024, 0);

    for (int pgmOffset = 0; pgmOffset < pgmLen; /**/)
    {
        int bytesThisLoop = static_cast<int>(buf.size() - 1);

        if (pgmOffset < errOffset)
        {
            if (pgmOffset + bytesThisLoop > errOffset)
                bytesThisLoop = errOffset - pgmOffset;
        }
        else if (pgmOffset == errOffset)
        {
            Printf(pri, "@@@");
        }
        if (pgmOffset + bytesThisLoop > pgmLen)
        {
            bytesThisLoop = pgmLen - pgmOffset;
        }
        memcpy (&buf[0], pstr + pgmOffset, bytesThisLoop);
        buf[bytesThisLoop] = '\0';

        Printf(pri, "%s", &buf[0]);

        pgmOffset += bytesThisLoop;
    }
}

//-----------------------------------------------------------------------------
bool Program::IsTwoSided() const
{
    return (PgmRqmt.Inputs.count(ppBkPriColor) ||
            PgmRqmt.Inputs.count(ppBkSecColor));
}

//-----------------------------------------------------------------------------
bool Program::ReadsPointSize() const
{
    return (PgmRqmt.Inputs.count(ppPtSize) != 0);
}

//-----------------------------------------------------------------------------
void PgmRequirements::Print (Tee::Priority pri) const
{
    //Note: Keep this in sync with the ProgramProperty defines in glr_comn.h
    static const char *szProp[ppNUM_ProgProperty] = {
        "Pos",          "Nrml",         "PriColor",     "SecColor",     "BkPriColor",
        "BkSecColor",   "FogCoord",     "PtSize",       "VtxId",        "Instance",
        "Facing",       "Depth",        "PrimId",       "Layer",        "ViewportIdx",
        "Primitive",    "FullyCovered", "BaryCoord",    "BaryNoPersp",  "ShadingRate",
        "Clip0",        "Clip1",        "Clip2",        "Clip3",        "Clip4",
        "Clip5",        "GenAttr0",     "GenAttr1",     "GenAttr2",     "GenAttr3",
        "GenAttr4",     "GenAttr5",     "GenAttr6",     "GenAttr7",     "GenAttr8",
        "GenAttr9",     "GenAttr10",    "GenAttr11",    "GenAttr12",    "GenAttr13",
        "GenAttr14",    "GenAttr15",    "GenAttr16",    "GenAttr17",    "GenAttr18",
        "GenAttr19",    "GenAttr20",    "GenAttr21",    "GenAttr22",    "GenAttr23",
        "GenAttr24",    "GenAttr25",    "GenAttr26",    "GenAttr27",    "GenAttr28",
        "GenAttr29",    "GenAttr30",    "GenAttr31",    "ClrBuf0",      "ClrBuf1",
        "ClrBuf2",      "ClrBuf3",      "ClrBuf4",      "ClrBuf5",      "ClrBuf6",
        "ClrBuf7",      "ClrBuf8",      "ClrBuf9",      "ClrBuf10",     "ClrBuf11",
        "ClrBuf12",     "ClrBuf13",     "ClrBuf14",     "ClrBuf15",     "TxCd0",
        "TxCd1",        "TxCd2",        "TxCd3",        "TxCd4",        "TxCd5",
        "TxCd6",        "TxCd7"
    };
    const static char *szXYZW[16] =
    {
        "none",
        "x",
        "y",
        "xy",
        "z",
        "xz",
        "yz",
        "xyz",
        "w",
        "xw",
        "yw",
        "xyw",
        "zw",
        "xzw",
        "yzw",
        "xyzw"
    };

    Printf(pri, "Inputs registers read:");
    for (ConstPropMapItr iter = Inputs.begin(); iter != Inputs.end(); iter++)
    {
        Printf(pri,"\n  %s:%s", szProp[iter->first], szXYZW[iter->second & omXYZW]);
    }
    Printf(pri,"%s", (Inputs.empty()) ? "None.\n" : "\n");

    Printf(pri, "Input texture coordinate properties:");
    for (ConstTxcMapItr iter = InTxc.begin(); iter != InTxc.end(); iter++)
    {
        for(int txf = 0; txf<32; txf++)
        {
            if( iter->second.TxfMask & (1<<txf))
            {
                Printf(pri,"\n%s provides TxCoord info for %s image using fetcher %d",
                    szProp[iter->second.Prop],
                    TxAttrToString(UsedTxf.find(txf)->second.Attrs & SASMNbDimFlags).c_str(),
                    txf);
            }
        }
    }
    Printf(pri,"%s", (InTxc.empty()) ? "None.\n" : "\n");

    Printf(pri, "Output registers written:");
    for (ConstPropMapItr iter = Outputs.begin(); iter != Outputs.end(); iter++)
    {
        Printf(pri,"\n  %s:%s", szProp[iter->first], szXYZW[iter->second & omXYZW]);
    }
    Printf(pri,"%s", (Outputs.empty()) ? "None.\n" : "\n");

    Printf(pri, "\nRequired texture bindings:");
    for (ConstTxfMapItr iter = UsedTxf.begin(); iter != UsedTxf.end(); iter++)
    {
        if (iter->second.Attrs & SASMDimFlags)
        {
            Printf(pri, "\nTxf%d --> %s",iter->first,
                mglEnumToString(MapTxAttrToTxDim(iter->second.Attrs),"IlwalidTexDim",false));
        }
    }
    Printf(pri,"%s", (UsedTxf.empty()) ? "None.\n" : "\n");

    Printf(pri,"Bindless Texture Attribute Mask:0x%x,\n\n", BindlessTxAttr);
}

//------------------------------------------------------------------------------
//  Report if the specified texture attribute has been referenced by any of
//  the loaded shaders. Called by RndTexturing
//
bool RndGpuPrograms::BindlessTxAttrRef(UINT32 Attr)
{
    const pgmSource srcs[] = { psUser, psFixed, psRandom };

    for (int pk = PgmKind_NUM-1; pk >= 0; pk--)
    {
        for (size_t ps = 0; ps < NUMELEMS(srcs); ps++)
        {
            const pgmSource pgmSrc = srcs[ps];

            for (int i = 0; i < m_Progs[pk].NumProgs(pgmSrc); i++)
            {
                PgmRequirements * pRqmt =
                        &(m_Progs[pk].Get(pgmSrc, i)->PgmRqmt);

                if( Attr == (pRqmt->BindlessTxAttr & Attr))
                    return (true);
            }
        }
    }
    return false;
}

//------------------------------------------------------------------------------
// Return true if any of the shaders for this loop are using a bindless texture
// with txAttr.
bool RndGpuPrograms::BindlessTxAttrNeeded(UINT32 txAttr)
{
    if (RND_TSTCTRL_DRAW_ACTION_vertices != m_pGLRandom->m_Misc.DrawAction())
        return false;

    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        if (m_State[pk].Enable)
        {
            Program * pLwr = m_Progs[pk].Lwr();
            MASSERT(pLwr);
            if ((txAttr & pLwr->PgmRqmt.BindlessTxAttr) == txAttr)
            {
                return true;
            }
        }
    }

    return false;
}

//------------------------------------------------------------------------------
//  What texture-object attributes are needed?
//
// RndTexturing calls this to find out what kind of texture object it must
// bind to this texture-fetcher and Image Unit in each draw.
// If this texture-fetcher index is out of range, or if no program will read
// from this fetcher, returns 0.
UINT32 RndGpuPrograms::TxAttrNeeded(int txf,GLRandom::TxfReq *pTxfReq)
{
    if ((txf < 0) || (txf > m_pGLRandom->NumTxFetchers()))
        return 0;
    if (RND_TSTCTRL_DRAW_ACTION_vertices != m_pGLRandom->m_Misc.DrawAction())
        return 0;

    UINT32 result = 0;

    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        if (m_State[pk].Enable)
        {
            Program * pLwr = m_Progs[pk].Lwr();
            MASSERT(pLwr);
            if (pLwr->PgmRqmt.UsedTxf.count(txf))
            {
                result |= pLwr->PgmRqmt.UsedTxf[txf].Attrs;   // Is1d/Is3d/etc
                if (pTxfReq)
                {
                    if (pTxfReq->IU.unit == -1)
                    {
                        *pTxfReq = pLwr->PgmRqmt.UsedTxf[txf];
                    }
                    if (pTxfReq->IU.unit != -1 &&
                        pLwr->PgmRqmt.UsedTxf[txf].IU.unit != -1)
                    {
                        //if we are using same fetcher for multiple image units,
                        //then these must be same MM level of same image unit!
                        MASSERT(pTxfReq->IU.unit == pLwr->PgmRqmt.UsedTxf[txf].IU.unit &&
                            pTxfReq->IU.mmLevel == pLwr->PgmRqmt.UsedTxf[txf].IU.mmLevel);
                    }
                }
            }
        }
    }

    // make sure only a single texture target is referenced by this
    // texture fetcher (Texture Image Unit).
    MASSERT( !((result & SASMDimFlags) & ((result & SASMDimFlags)-1)));
    return result;
}

//------------------------------------------------------------------------------
int RndGpuPrograms::GetInTxAttr(int pgmProp)
{
    PgmRequirements * pRqmts = NULL;
    const ProgProperty prop = (ProgProperty)pgmProp;

    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        if (!pRqmts && m_State[pk].Enable)
        {
            Program * pProg = m_Progs[pk].Lwr();
            if (pProg->PgmRqmt.Outputs.count(prop) &&
                !(pProg->PgmRqmt.Outputs[prop] & omPassThru))
            {
                // This program is using this coordinate
                pRqmts = &(pProg->PgmRqmt);
            }
            else if (pkFragment == pk)
            {
                pRqmts = &(pProg->PgmRqmt);
            }
        }
    }

    if (!pRqmts)
        return  0;

    // Find out what fetchers are using this ProgProperty as a texture coordinate
    // and return the most significant attribute.
    UINT32 txAttr = 0;
    if( pRqmts->InTxc.count(prop))
    {
        UINT32 txf = pRqmts->InTxc[prop].TxfMask;
        for (UINT32 i=0; i<32; i++)
        {
            if (txf & (1<<i))
            {
                // get the fetchers attributes
                MASSERT(pRqmts->UsedTxf.count(i));
                txAttr = GetMostImportantTxAttr(txAttr, pRqmts->UsedTxf[i].Attrs);
            }
        }

    }
    return (txAttr & SASMDimFlags);
}

UINT32 RndGpuPrograms::GetMostImportantTxAttr( UINT32 attr1, UINT32 attr2)
{
    const UINT32 attrList[] =     // arranged from lowest to highest priority
    {                       // Coordinates used
                            // s t r  layer  shadow  sample
                            // ------ -----  ------  ------
        IsShadowArrayLwbe,  // x y z    w      *
        IsShadowArray2d,    // x y -    z      w
        IsShadowLwbe,       // x y z    -      w
        IsArrayLwbe,        // x y z    w      -
        Is2dMultisample,    // x y -    -      -       w
        IsLwbe,             // x y z    -      -
        IsArray2d,          // x y -    z      -
        IsShadowArray1d,    // x - -    y      z
        IsShadow2d,         // x y -    -      z
        IsShadow1d,         // x - -    -      z
        Is3d,               // x y z    -      -
        IsArray1d,          // x - -    y      -
        Is2d,               // x y -    -      -
        Is1d                // x - -    -      -
    };

    UINT32 idx1, idx2;
    for (idx1 = 0; idx1 < NUMELEMS(attrList); idx1++)
    {
        if (attrList[idx1] == (attr1 & SASMDimFlags))
            break;
    }
    for (idx2 = 0; idx2 < NUMELEMS(attrList); idx2++)
    {
        if (attrList[idx2] == (attr2 & SASMDimFlags))
            break;
    }
    return (idx1 < idx2) ? attr1 : attr2;

}

//------------------------------------------------------------------------------
// RndVertexFormat calls this to find out if a specific vertex attrib. is needed
UINT32 RndGpuPrograms::VxAttrNeeded(int attrib) const
{
    if (m_State[pkVertex].Enable)
    {
        ProgProperty prop = ppIlwalid;
        switch (attrib)
        {
            case att_OPOS:  prop = ppPos;           break;
            case att_NRML:  prop = ppNrml;          break;
            case att_COL0:  prop = ppPriColor;      break;
            case att_COL1:  prop = ppSecColor;      break;
            case att_FOGC:  prop = ppFogCoord;      break;
            case att_1:     prop = (ProgProperty)(ppGenAttr0+1);break;
            case att_6:     prop = (ProgProperty)(ppGenAttr0+6);break;
            case att_7:     prop = (ProgProperty)(ppGenAttr0+7);break;
            case att_TEX0:
            case att_TEX1:
            case att_TEX2:
            case att_TEX3:
            case att_TEX4:
            case att_TEX5:
            case att_TEX6:
            case att_TEX7:
                prop = (ProgProperty)(ppTxCd0+(attrib-att_TEX0));
                break;
            default:
                MASSERT(!"Unknown vertex attrib requested."); break;
        }
        if (m_Progs[pkVertex].Lwr()->PgmRqmt.Inputs.count(prop))
            return m_Progs[pkVertex].Lwr()->PgmRqmt.Inputs[prop];
        else
            return compNone;
    }

    // we're not sure what's required so return all components
    return compXYZW;

}
//------------------------------------------------------------------------------
//  How many of the s,t,r,q components are needed for this tex-coord.
//
// Called by RndGeometry for picking tx-coord-gen modes (when vxprogs are
// off) and by RndVertexFormat and RndVertexes to determine what to send
// per-vertex.  This function ignores tx-coord-gen.
//
// It is OK if extra components are sent.
//
// If vertex or fragment programs are enabled, returns how many of s,t,r,q
// will be read as inputs by the vertex program (or fragment program if
// vertex programs are disabled).
//
// If both program modes are disabled, calls RndTexturing to get the coords
// needed by the lw2x texture-shader opcode or traditional GL texture unit.
int RndGpuPrograms::TxCoordComponentsNeeded(int txc)
{
    if ((txc < 0) || (txc >= MaxTxCoords))
        return 0;

    const ProgProperty prop = (ProgProperty)(ppTxCd0+txc);
    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        if (m_State[pk].Enable)
        {
            Program * pProg = m_Progs[pk].Lwr();

            if (pProg->PgmRqmt.Inputs.count(prop))
            {
                return m_Gen[pk]->GetMaxTxCdSz(pProg->PgmRqmt.Inputs[prop]);
            }
            return 0;
        }
    }

    // No program-kind is enabled.
    return m_pGLRandom->m_Texturing.TxuComponentsNeeded(txc);
}

UINT32 RndGpuPrograms::ComponentsReadByAllTxCoords() const
{
    if (!m_State[pkFragment].Enable)
        return 0;

    Program * pProg = m_Progs[pkFragment].Lwr();

    GLuint retMask = compXYZW;
    for (int txc = 0; txc < MaxTxCoords; txc++)
    {
        const ProgProperty prop = static_cast<ProgProperty>(ppTxCd0 + txc);
        auto compIter = pProg->PgmRqmt.Inputs.find(prop);
        if (compIter != pProg->PgmRqmt.Inputs.end())
        {
            retMask &= compIter->second;
        }
    }

    return retMask;
}

//------------------------------------------------------------------------------
RC RndGpuPrograms::AddUserVertexProgram
(
    PgmRequirements &Rqmts,      //!< The input/output requirements
    VxAttribute Col0Alias,                  //!< Which program attrib we will
                                            //!< read for pri-color
    const char * Pgm                        //!< the program source code
)
{
    Program tmp(Pgm, Rqmts);
    switch (Col0Alias)
    {
        default:
            tmp.Col0Alias = att_COL0;
            break;

        case att_1   :
        case att_NRML:
        case att_FOGC:
        case att_6   :
        case att_7   :
        case att_TEX2:
        case att_TEX3:
        case att_TEX4:
        case att_TEX5:
        case att_TEX6:
        case att_TEX7:
            tmp.Col0Alias = Col0Alias;
            break;
    }

    return AddUserProgram(pkVertex, tmp);
}

//------------------------------------------------------------------------------
RC RndGpuPrograms::AddUserTessCtrlProgram
(
    PgmRequirements &Rqmts,  //!< The input/output requirements
    const char * Pgm         //!< The program string
)
{
    Program tmp(Pgm, Rqmts);
    return AddUserProgram(pkTessCtrl, tmp);
}

//------------------------------------------------------------------------------
RC RndGpuPrograms::AddUserTessEvalProgram
(
    PgmRequirements &Rqmts,
    GLenum tessMode,
    const char * Pgm
)
{
    Program tmp(Pgm, Rqmts);
    tmp.TessMode = tessMode;
    return AddUserProgram(pkTessEval, tmp);
}

//------------------------------------------------------------------------------
RC RndGpuPrograms::AddUserGeometryProgram
(
    PgmRequirements &Rqmts,     //!< The input/output requirements
    const char * Pgm            //!< the program source code
)
{
    Program tmp(Pgm, Rqmts);
    return AddUserProgram(pkGeometry, tmp);
}

//------------------------------------------------------------------------------
RC RndGpuPrograms::AddUserFragmentProgram
(
    PgmRequirements &Rqmts,  //!< The input/output requirements
    const char * Pgm         //!< The program string
)
{
    Program tmp(Pgm, Rqmts);
    return AddUserProgram(pkFragment, tmp);
}

//------------------------------------------------------------------------------
RC RndGpuPrograms::AddUserDummyProgram(GLRandom::pgmKind pk)
{
    MASSERT(pk >= pkVertex && pk < PgmKind_NUM);
    Program tmp;
    return AddUserProgram(pk,tmp);
}

//------------------------------------------------------------------------------
RC RndGpuPrograms::AddUserProgram
(
    pgmKind pk,
    Program & tmp,
    GLRandom::pgmSource ps /* = psUser*/
)
{
    tmp.Target  = PgmGen::pkTargets[pk];

    // Calc. XFB attribs based on current program requirements.
    tmp.CalcXfbAttributes();

    // check for multisample option
    if (tmp.Pgm.find("LW_explicit_multisample") != string::npos)
        tmp.UsingGLFeature[Program::Multisample] = true;

    // Grep program source for SubArrayLen property.
    const char * declText = "SUBROUTINE tSub Subs[";
    const size_t subOff = tmp.Pgm.find(declText);

    if (subOff != string::npos)
    {
        //nOff points to 1st char after "["
        const size_t nOff = subOff + strlen(declText);
        const size_t nEnd = tmp.Pgm.find("]", nOff);
        if (nEnd > nOff)
        {
            string ns = tmp.Pgm.substr(nOff, (nEnd - nOff));
            int n = 0;
            sscanf(ns.c_str(), "%d", &n);
            if (n > 0)
                tmp.SubArrayLen = n;
        }
    }

    // Send the program to OpenGL.
    RC rc = OK;
    if (m_pGLRandom->IsGLInitialized())
    {
        rc = LoadProgram(&tmp);
    }
    if (OK == rc)
    {
        m_Progs[pk].AddProg(ps, tmp);
    }

    return rc;
}

//------------------------------------------------------------------------------
// Create a JS file that includes all of the fancy picker settings to load and
// pick only the suspect shader programs when running GLRandom based programs.
// There are two modes this file needs to support:
// a)Regressing Loops Mode:
//   In this mode we want to be able to rerun the failing loop(s) that originaly
//   generated this set of shaders. However we may want to tweek the instructions
//   in the shader without impacting the random sequence. To do the the user will
//   pass "-regressing_loops -verify_suspect_shader glug*.js" command line argument.
//   We make use of the RND_GPU_PROG_%s_INDEX_DEBUG override feature to enable/disable
//   the shaders (which will not disturb the random sequence).
// b)General Regression Mode:
//   In this mode we want to see if this set of shaders will produce errors across
//   multiple frames using different glstate. In this case we are not concerned
//   about the random sequence, however we need to make sure the appropriate
//   pickers are selected to keep these shaders from producing inconsistent results.
//   We have to specifically set the following pickers to match the requirements of the shaders:
//   RND_GPU_PROG_STRICT_PGM_LINKING,
//   RND_GPU_PROG_USE_BINDLESS_TEXTURES,
//   RND_GPU_PROG_%s_INDEX,
//   RND_GPU_PROG_%s_ENABLE,
//   etc.
RC RndGpuPrograms::CreateBugJsFile()
{
    const UINT32 lwrrentLoop   = m_pGLRandom->GetLoop();
    const UINT32 loopsPerPgm   = m_pGLRandom->GetLoopsPerProgram();
    const UINT32 endLoop       = (lwrrentLoop / loopsPerPgm) * loopsPerPgm + loopsPerPgm - 1;
    const UINT32 loopsPerFrame = m_pGLRandom->LoopsPerFrame();
    const UINT32 lwrrentFrame  = lwrrentLoop / loopsPerFrame;
    const UINT32 offset        = lwrrentLoop % loopsPerPgm;
    const UINT32 failingSeed   = m_pGLRandom->GetSeed(lwrrentLoop - offset);
    const string filename = Utility::StrPrintf("glbug_%x.js", failingSeed);

    // See if we have already created this glbug*.js file.
    std::map<UINT32, bool>::iterator it;
    it = m_BugJsFileCreated.find(failingSeed);
    if (it != m_BugJsFileCreated.end())
    {
        Printf(Tee::PriNormal, "*********************************************\n");
        Printf(Tee::PriNormal, "%s file already created to verify this bug!\n", filename.c_str());
        Printf(Tee::PriNormal, "*********************************************\n");
        return OK; // yup, so just exit.
    }
    m_BugJsFileCreated[failingSeed] = true;


    Printf(Tee::PriNormal, "*********************************************\n");
    Printf(Tee::PriNormal, "Creating a %s file to verify this bug!\n", filename.c_str());
    Printf(Tee::PriNormal, "*********************************************\n");

    list<string> strings;

    strings.push_back(Utility::StrPrintf(
            "// Shaders from GLRandomTest %d failing loop %d\n"
            "//  part of frame %d which started at loop %d\n"
            "//  frame seed 0x%x\n"
            "//  loop seed  0x%x\n",
            ErrorMap::Test(),
            lwrrentLoop,
            lwrrentFrame,
            lwrrentFrame * loopsPerFrame,
            m_pGLRandom->GetSeed(lwrrentFrame * loopsPerFrame),
            failingSeed));

    // Setup the pickers to only pick these shader programs

    strings.push_back(Utility::StrPrintf(
        "//**********************************************************************************\n"
        "// Steps for confirming this suspect shaders.\n"
        "// 1.Run mods with gpugen.js -test %d -regressing_loops -verify_suspect_shader %s\n"
        "// 2.Run mods with gputest.js -test %d -regressing_loops -verify_suspect_shader %s\n"
        "// If step 1 causes an ASSERT, file a bug against Compiler-OCG and copy the\n"
        "// shader text below.\n"
        "// If step 2 causes a frame buffer miscompare. Then perform step 2 using -gltrace.\n"
        "// Then file a bug with OpenGL-Core and attach the lwtrace.txt file along with the\n"
        "// shader text below.\n"
        "// It is also very helpful to include the SPA code with the bug. You can generate\n"
        "// it using the perl script lw50dissall.pl\n"
        "//**********************************************************************************\n"
        "#include \"glr_comm.h\"\n"
        "function SetupGLRandomToReproFailure()\n"
        "{\n"
        "    Out.Printf(Out.PriHigh, \"******  ATTENTION ******\\n\");\n"
        "    Out.Printf(Out.PriHigh, \"Using special JS file to verify suspect shaders\\n\");\n"
        "    //Enable only the suspect programs for verification\n"
        "    //this.SetPicker( RND_GPU_PROG_VERBOSE_OUTPUT, [\"const\", [1]]);\n"
        "    this.FrameRetries = 0; // don't retry this frame on failure\n"
        "    g_FbiCheck = 0;        // don't retest with lower clocks\n"
        "    //This set of shaders was specifically generated to run on loops %d-%d. To use this\n"
        "    //file on other loops you must omit the \"-regressing_loops\" command line argument.\n" //$
        "    ErrorMap.EnableTestProgress = false; //Disable test progress updates to prevent asserts\n" //$
        "    if ((\"undefined\" != typeof(g_RegressingLoops)) && g_RegressingLoops)\n"
        "    {\n" //$
        "        this.FrameRetries = 0;\n"
        "        g_FbiCheck = 0;\n"
        "        g_RegressMiscompareCount = 0;\n"
        "        g_Isolate = ISOLATE_LOOPS;\n"
        "        this.isolateLoops(%d, %d);\n"
        "        this.Golden.SkipCount = 1;\n"
        "        this.Golden.DumpPng = Golden.OnError | Golden.OnStore | Golden.OnCheck;\n",
        ErrorMap::Test(), filename.c_str(),
        ErrorMap::Test(), filename.c_str(),
        lwrrentLoop, endLoop,
        lwrrentLoop, endLoop));

    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        strings.push_back(Utility::StrPrintf(
                "        this.SetPicker(RND_GPU_PROG_%s_INDEX_DEBUG, [\"const\", [%s]]);\n",
                PgmGen::pkNicknamesUC[pk]
                ,m_State[pk].Enable ? "psUser" : "RND_GPU_PROG_INDEX_DEBUG_NO_OVERRIDE"));
    }

    strings.push_back(Utility::StrPrintf(
        "    }\n"
        "    else\n"
        "    {\n" //$
        "        this.SetPicker(RND_GPU_PROG_STRICT_PGM_LINKING, [\"const\", GL_TRUE]);\n"
        "        this.SetPicker(RND_GPU_PROG_USE_BINDLESS_TEXTURES, [\"random\", [1, %s]]);\n"
        "        this.SetPicker(RND_TSTCTRL_ENABLE_GL_DEBUG, [\"const\", GL_TRUE]);\n",
            UseBindlessTextures() ? "GL_TRUE" : "GL_FALSE"));

    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        strings.push_back(Utility::StrPrintf(
                "        this.SetPicker(RND_GPU_PROG_%s_INDEX, [\"const\", [psUser]]);\n",
                PgmGen::pkNicknamesUC[pk]));

        strings.push_back(Utility::StrPrintf(
            "        this.SetPicker(RND_GPU_PROG_%s_ENABLE, [\"const\", [%d]]);\n",
            PgmGen::pkNicknamesUC[pk],
            m_State[pk].Enable ? 1 : 0 ));
    }

    strings.push_back(Utility::StrPrintf(
        "    }\n"));

    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        strings.push_back(Utility::StrPrintf(
            "    //Loading %s Program:\n", PgmGen::pkNames[pk]));
        if (m_State[pk].Enable)
        {
            strings.push_back(m_Progs[pk].Lwr()->ToJsString());
        }
        else
        {
            strings.push_back(Utility::StrPrintf(
                    "    this.LoadUserDummyProgram(%d);\n", pk));
        }
    }

    strings.push_back("}\n");

    // Now add just the program strings so they can be copied to the bug report
    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        if (m_State[pk].Enable)
        {
            strings.push_back(Utility::StrPrintf(
                    "/* %s Program String: %s */\n",
                    PgmGen::pkNames[pk],
                    m_Progs[pk].Lwr()->Name.c_str()));
            strings.push_back(Utility::StrPrintf(
                    "/* \n %s\n*/\n",
                    m_Progs[pk].Lwr()->Pgm.c_str()));
        }
    }

    JavaScriptPtr pJS;
    bool doInline = false;
    pJS->GetProperty(m_pGLRandom->GetJSObject(), "LogShaderInline", &doInline);
    if (doInline)
    {
        Printf(Tee::PriHigh, "*** begin %s ***\n", filename.c_str());

        list<string>::iterator it;
        for (it = strings.begin(); it != strings.end(); ++it)
            Printf(Tee::PriHigh, "%s", (*it).c_str());

        Printf(Tee::PriHigh, "*** end %s ***\n", filename.c_str());
    }
    else
    {
        FileHolder fh (filename.c_str(), "w+");
        FILE * f = fh.GetFile();
        if(f)
        {
            list<string>::iterator it;
            for (it = strings.begin(); it != strings.end(); ++it)
                fprintf(f, "%s", (*it).c_str());
        }
        else
        {
            Printf(Tee::PriError, "Cant open file:%s for writing!\n", filename.c_str());
            return RC::CANNOT_OPEN_FILE;
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
RC RndGpuPrograms::WriteLwrrentProgramsToJSFile()
{
    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        if (m_State[pk].Enable)
            m_Progs[pk].Lwr()->PrintToJsFile();
    }
    return OK;
}

//------------------------------------------------------------------------------
void RndGpuPrograms::FreeUserPrograms()
{
    for (int pk = 0; pk < PgmKind_NUM; pk++)
    {
        m_Progs[pk].ClearProgs(psUser);
    }
}

//------------------------------------------------------------------------------
// Routines to help in the debugging process. gdb doesn't like to print out this
// information.
void RndGpuPrograms::DebugPrintPropMap(GLRandom::PgmRequirements::PropMap & rPropMap)
{
    PgmRequirements::PropMapItr iter;
    for (iter = rPropMap.begin(); iter != rPropMap.end(); iter++)
    {
        Printf(Tee::PriHigh, "Prop:%s Value:%s\n",
                m_Gen[pkFragment]->PropToString(iter->first).c_str(),
                m_Gen[pkFragment]->CompToString(iter->second).c_str());
    }
}

//------------------------------------------------------------------------------
void RndGpuPrograms::DebugPrintTxcMap(GLRandom::PgmRequirements::TxcMap & rTxcMap)
{
    PgmRequirements::TxcMapItr iter;
    for (iter = rTxcMap.begin(); iter != rTxcMap.end();iter++)
    {
        Printf(Tee::PriHigh, "Prop:%s,FetcherMask:0x%x\n",
            m_Gen[pkFragment]->PropToString(iter->second.Prop).c_str(),
            iter->second.TxfMask);
    }
}

//------------------------------------------------------------------------------
// Return true if we should be using bindless textures.
bool RndGpuPrograms::UseBindlessTextures() const
{
    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_bindless_texture))
        return (m_Pickers[RND_GPU_PROG_USE_BINDLESS_TEXTURES].GetPick() != 0);
    else
        return false;
}

//------------------------------------------------------------------------------
void RndGpuPrograms::SetIsLwrrVportIndexed(bool vportIndexed)
{
    m_IsLwrrVportIndexed = vportIndexed;
}

//------------------------------------------------------------------------------
bool RndGpuPrograms::GetIsLwrrVportIndexed() const
{
    return m_IsLwrrVportIndexed;
}

//------------------------------------------------------------------------------
// Programs -- container for Program objects
//------------------------------------------------------------------------------
Programs::Programs ()
 :  m_Name("pgm")
   ,m_Seed(0)
   ,m_LwrCombinedIdx(0)
   ,m_LwrProg(NULL)
   ,m_EnableReplacement(false)
{
}

Programs::~Programs ()
{
}

//------------------------------------------------------------------------------
vector<Program> * Programs::PgmSrcToVec (pgmSource ps)
{
    switch (ps)
    {
        case psFixed:   return &m_FixedProgs;
        case psRandom:  return &m_RandomProgs;
        case psUser:    return &m_UserProgs;
        case psSpecial: return &m_SpecialProgs;

        default:
            MASSERT(!"Unknown program source");
            return NULL;
    }
}
const vector<Program> * Programs::PgmSrcToVec (pgmSource ps) const
{
    switch (ps)
    {
        case psFixed:   return &m_FixedProgs;
        case psRandom:  return &m_RandomProgs;
        case psUser:    return &m_UserProgs;
        case psSpecial: return &m_SpecialProgs;

        default:
            MASSERT(!"Unknown program source");
            return NULL;
    }
}

//------------------------------------------------------------------------------
const char * Programs::PgmSrcToStr (pgmSource ps) const
{
    switch (ps)
    {
        case psFixed:   return "Fix";
        case psRandom:  return "Rnd";
        case psUser:    return "Usr";
        case psSpecial: return "Spc";

        default:
            MASSERT(!"Unknown program source");
            return "Unk";
    }
}

//------------------------------------------------------------------------------
void Programs::AddProg (pgmSource src, const Program & prog)
{
    vector<Program> * pVec = PgmSrcToVec(src);
    MASSERT(pVec);

    const size_t n = pVec->size();
    pVec->push_back(prog);
    Program * pProg = &(*pVec)[n];

    if (0 == pProg->Name.size())
    {
        // Program has no name, provide one: example "vxRnd12345678003"

        (*pVec)[n].Name = Utility::StrPrintf("%s%s%08x%03d",
                m_Name, PgmSrcToStr(src), m_Seed, (int)n);
    }

    if (m_EnableReplacement && Xp::DoesFileExist(pProg->Name))
    {
        if (Xp::InteractiveFileRead(pProg->Name.c_str(), &pProg->Pgm) == OK)
        {
            Printf(Tee::PriLow, "Replaced program %s with content from file.\n",
                pProg->Name.c_str());
        }
        else
        {
            Printf(Tee::PriWarn, "Unable to read program %s content from file.\n",
                pProg->Name.c_str());
        }
    }
}
//------------------------------------------------------------------------------
void Programs::RemoveProg (pgmSource src, const int idx)
{
    vector<Program> * pVec = PgmSrcToVec(src);
    MASSERT(pVec);
    pVec->erase(pVec->begin() + idx);
}

//------------------------------------------------------------------------------
void Programs::ClearProgs (pgmSource src)
{
    UnloadProgs(src);

    vector<Program> * pVec = PgmSrcToVec(src);
    if (pVec)
        pVec->clear();
}

//------------------------------------------------------------------------------
void Programs::UnloadProgs (pgmSource src)
{
    vector<Program> * pVec = PgmSrcToVec(src);
    if (pVec)
    {
        for (size_t i = 0; i < pVec->size(); i++)
        {
            Program & prog = (*pVec)[i];
            if (prog.Id)
                glDeleteProgramsLW(1, &prog.Id);
            prog.Id = 0;
        }
    }
}

//------------------------------------------------------------------------------
int Programs::NumProgs() const
{
    size_t x = m_FixedProgs.size() +
           m_RandomProgs.size() +
           m_UserProgs.size();
    return static_cast<int>(x);
}

//------------------------------------------------------------------------------
int Programs::NumProgs (pgmSource src) const
{
    const vector<Program> * pVec = PgmSrcToVec(src);
    if (!pVec)
        return 0;

    return static_cast<int>(pVec->size());
}

//------------------------------------------------------------------------------
Program * Programs::Get (pgmSource src, UINT32 idx)
{
    vector<Program> * pVec = PgmSrcToVec(src);
    if (!pVec)
        return NULL;
    if (idx >= pVec->size())
        return NULL;

    return &(*pVec)[idx];
}

//------------------------------------------------------------------------------
void Programs::SetLwr (pgmSource src, UINT32 idx)
{
    Program * pProg = Get(src, idx);

    MASSERT(pProg);

    if (pProg)
    {
        m_LwrCombinedIdx = src + idx;
        m_LwrProg = pProg;
    }
}

//------------------------------------------------------------------------------
void Programs::SetLwr (UINT32 combinedIdx)
{
    int       offset = combinedIdx % psRandom;
    pgmSource src    = static_cast<pgmSource>(combinedIdx - offset);

    SetLwr(src, offset);

    // If this fails, there's a bug in PickEnablesAndBindings.
    MASSERT(Get(src, offset) == m_LwrProg);
}

//------------------------------------------------------------------------------
void Programs::ClearLwr ()
{
    m_LwrCombinedIdx = 0;
    m_LwrProg = NULL;
}

//------------------------------------------------------------------------------
void Programs::PickLwr
(
    FancyPicker * pPicker,
    Random *      pRand
)
{
    // We have three sources of programs:
    //   Fixed  - nonrandom programs, persistent across frames
    //   Random - randomly generated programs, new in each frame
    //   User   - user-specified programs from JS, persistent across frames
    //
    // The idx from the picker is ambiguous -- which of these arrays is
    // it an index into?
    //
    // We have constants psFixed(0), psRandom(1000), psUser(2000) to
    // specify the source, but what if the user-defined picker sets something
    // else?
    //
    // Our rule here is:
    //  - psUser(2000) means user program 0, 2001 means user program 1, etc.
    //
    //  - Exactly psRandom(1000) means any random program with equal
    //    probability per program.  This is the default in the C++ picker setup.
    //
    //  - Anything else selects a particular program in the array formed
    //    by just concatenating the size of all three kinds of programs.
    //    This is normally used for fixed programs only though.

    int idx = pPicker->Pick();
    pgmSource src;
    int       offset;
    const int numUser = NumProgs(psUser);
    const int numFixed = NumProgs(psFixed);
    const int numRandom = NumProgs(psRandom);

    if ((idx >= psUser) && (idx - psUser < numUser))
    {
        // A specified exact user program.
        src    = psUser;
        offset = idx - psUser;
    }
    else
    {
        if ((psRandom == idx) && (numRandom > 0))
        {
            // Requested any random program, and we have at least one.
            // Replace idx with a random number within the range of random progs.
            src = psRandom;
            if (pRand)
                offset = pRand->GetRandom(0, numRandom-1);
            else
                offset = (idx - psRandom) % numRandom;
        }
        else
        {
            // A specified exact fixed program,
            // or requested but out-of-range user program,
            // or a random program when none are present,
            // or any other possible idx value.
            //
            // Handle all of these cases by treating the pick as an index into
            // a virtual array of all fixed/random/user programs.

            // Force user's pick to be in-range.
            idx = idx % NumProgs();

            // Work out the pgmSource by subtraction.
            offset = idx;
            src    = psFixed;

            if (offset >= numFixed)
            {
                offset -= numFixed;
                src     = psRandom;

                if (offset >= numRandom)
                {
                    offset -= numRandom;
                    src     = psUser;
                }
            }
        }
    }
    SetLwr(src, static_cast<UINT32>(offset));
}

//------------------------------------------------------------------------------
// Often the random result of Pick doesn't meet requirements, i.e.
// a vertex program doesn't write the outputs required by the already-picked
// fragment progam.
// PickNext is called to cycle through the programs to find one that works.
void Programs::PickNext()
{
    if (0 == NumProgs())
        return;

    int       offset = m_LwrCombinedIdx % psRandom;
    pgmSource src    = static_cast<pgmSource>(m_LwrCombinedIdx - offset);

    offset++;

    while (offset >= NumProgs(src))
    {
        offset = 0;
        switch (src)
        {
            case psFixed:  src = psRandom; break;
            case psRandom: src = psUser; break;
            case psUser:   src = psFixed; break;

            default:
                MASSERT(!"Invalid pgmSource");
                src = psFixed;
                break;
        }
    }
    SetLwr(src, static_cast<UINT32>(offset));
}

//------------------------------------------------------------------------------
Program * Programs::Lwr() const
{
    return m_LwrProg;
}

//------------------------------------------------------------------------------
UINT32 Programs::LwrCombinedIdx() const
{
    return m_LwrCombinedIdx;
}

//------------------------------------------------------------------------------
void Programs::SetSeed(UINT32 seed)
{
    m_Seed = seed;
}

//------------------------------------------------------------------------------
void Programs::SetEnableReplacement(bool enable)
{
    m_EnableReplacement = enable;
}

//------------------------------------------------------------------------------
void Programs::SetName(const char * name)
{
    m_Name = name;
}
