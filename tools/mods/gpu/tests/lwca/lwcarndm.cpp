/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2013,2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwdarndm.h"
#include "core/include/jscript.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include <math.h>

using Utility::AlignUp;
using Utility::AlignDown;

namespace LwdaRandom
{

//-----------------------------------------------------------------------------
// JS linkage for test.
JS_CLASS_INHERIT(LwdaRandomTest, LwdaStreamTest,
                "LWCA Random test (arithmetic operations)");

CLASS_PROP_READWRITE(LwdaRandomTest, SurfSizeOverride, UINT32,
        "Size in bytes of src and dst surfaces (if 0, matches display mode).");

CLASS_PROP_READWRITE(LwdaRandomTest, GoldenCheckInputs, bool,
        "Golden check input matrices");

CLASS_PROP_READWRITE(LwdaRandomTest, PrintHex, bool,
        "Print tile data in hex. (default = false, print as float)");

CLASS_PROP_READWRITE(LwdaRandomTest, SetMatrixValue, bool,
        "Enable filling matrices with user values");

CLASS_PROP_READWRITE(LwdaRandomTest, MatrixAValue, double,
        "Fill Matrix A with value");

CLASS_PROP_READWRITE(LwdaRandomTest, MatrixBValue, double,
        "Fill Matrix B with value");

CLASS_PROP_READWRITE(LwdaRandomTest, MatrixCValue, double,
        "Fill Matrix C with value");

CLASS_PROP_READWRITE(LwdaRandomTest, SetSmallInt, bool,
        "Enable Filling of matrices with UINT16 or UINT08 in SeqInt");

CLASS_PROP_READWRITE(LwdaRandomTest, SetByteInt, bool,
        "Fill matrices with UINT08 when true, else fill with UINT16");

CLASS_PROP_READWRITE(LwdaRandomTest, FillSurfaceOnGPU, bool,
        "Fill input surfaces using GPU");

CLASS_PROP_READWRITE(LwdaRandomTest, StopOnError, bool,
        "Stop testing on first detected error");

CLASS_PROP_READWRITE(LwdaRandomTest, ClearSurfacesWithGPUFill, bool,
        "Clear surfaces every frame when GPU fill is used (debugging aid)");

CLASS_PROP_READWRITE(LwdaRandomTest, Coverage, bool,
        "Prints Opcode Coverage Stats");

//------------------------------------------------------------------------------
// Non-member function for Goldelwalues callbacks:
static void LWRandom_Print(void * pvLwdaRandom)
{
    LwdaRandomTest * pLwdaRandom = (LwdaRandomTest *)pvLwdaRandom;
    pLwdaRandom->Print(Tee::PriNormal);
}

static void LWRandom_Error
(
    void *   pvLwdaGoldenSurfaces
    ,UINT32  loop
    ,RC      rc
    ,UINT32  surf
)
{
    LwdaGoldenSurfaces * pLwdaGoldenSurfaces =
            (LwdaGoldenSurfaces *) pvLwdaGoldenSurfaces;
    pLwdaGoldenSurfaces->SetRC((int)surf, rc);
}

CoverageTracker coverageTracker;

//-----------------------------------------------------------------------------
LwdaRandomTest::LwdaRandomTest()
    : m_SurfSizeOverride(0)
    ,m_GoldenCheckInputs(false)
    ,m_PrintHex(false)
    ,m_SetMatrixValue(false)
    ,m_MatrixAValue(1.0)
    ,m_MatrixBValue(1.0)
    ,m_MatrixCValue(1.0)
    ,m_MatrixSMIDValue(-1.0)
    ,m_SetSmallInt(false)
    ,m_SetByteInt(false)
    ,m_FillSurfaceOnGPU(true)
    ,m_StopOnError(true)
    ,m_ClearSurfacesWithGPUFill(false)
    ,m_Coverage(false)
    ,m_Cap(0,0)
    ,m_LoopsPerSurface(0)
    ,m_SurfWidth(0)
    ,m_SurfHeight(0)
    ,m_GotProperties(false)
    ,m_Property()
    ,m_devSurface()
    ,m_NumSMs(0)
    ,m_MsecTot(0.0F)
    ,m_AllocatedThreadsPerFrame(0)
    ,m_MaxThreadsPerFrame(0)
{
    SetName("LwdaRandomTest");
    // Allowing the use of Picker(s) with separate independent Fancy Picker Context
    // New Opcodes not supported on older GPUs get picked which are later skipped during the LWCA kernel exelwtion
    // resulting in reduced stress impact and coverage.
    // But it is important that the variation in number of opcodes doesn't impact the RNG pathway for other pickers.
    // They should remain consistent across GPU families.
    m_FpCtxMap.emplace_back(CRTST_KRNL_SEQ_MTX_OPCODE);

    m_pFpCtx = GetFpContext();
    CreateFancyPickerArray(CRTST_NUM_PICKERS);
    m_pPickers = GetFancyPickerArray();
}

//-----------------------------------------------------------------------------
LwdaRandomTest::~LwdaRandomTest()
{
    FreeLaunchParams();
}

//-----------------------------------------------------------------------------
RC LwdaRandomTest::SetDefaultsForPicker(UINT32 Index)
{
    FancyPickerArray * pPickers = GetFancyPickerArray();
    FancyPicker &fp = (*pPickers)[Index];
    LinkFpCtxMap(addressof(fp), Index);
    RC rc = OK;

    switch (Index)
    {
        case CRTST_CTRL_KERNEL:
            // The WAR for BUG 2964522 / 3063519 may override this in gpudecls.js
            // We can't override it in this function since GetBoundGpuSubdevice()
            // is unavailable this early
            fp.ConfigRandom("CRTST_CTRL_KERNEL");
            fp.AddRandItem(50, CRTST_CTRL_KERNEL_SeqMatrix );
            fp.AddRandItem(50, CRTST_CTRL_KERNEL_SeqMatrixDbl);
            fp.AddRandItem(20, CRTST_CTRL_KERNEL_SeqMatrixStress );
            fp.AddRandItem(20, CRTST_CTRL_KERNEL_SeqMatrixDblStress);
            fp.AddRandItem(10, CRTST_CTRL_KERNEL_SeqMatrixInt);
            fp.AddRandItem(10, CRTST_CTRL_KERNEL_Stress);
            fp.AddRandItem(10, CRTST_CTRL_KERNEL_Branch);
            fp.AddRandItem(10, CRTST_CTRL_KERNEL_Texture);
            fp.AddRandItem(10, CRTST_CTRL_KERNEL_Surface);
            fp.AddRandItem(5,  CRTST_CTRL_KERNEL_MatrixInit);
            fp.AddRandItem(15, CRTST_CTRL_KERNEL_Atom);
            fp.AddRandItem(15, CRTST_CTRL_KERNEL_GpuWorkCreation);
            fp.CompileRandom();
            break;

        case CRTST_CTRL_SHUFFLE_TILES:
            fp.ConfigRandom("CRTST_CTRL_SHUFFLE_TILES");
            fp.AddRandItem(20, false);
            fp.AddRandItem(80, true);
            fp.CompileRandom();
            break;

        //
        // Pickers to support SeqMatrixArithmeticTest &
        // SeqMatrixArithmeticTestDouble kernels in lwdarandom.lw
        //
        case CRTST_INPUT_DATA_FILL_STYLE:
            fp.ConfigConst(rs_exp);     // use rs_exp or rs_range
            break;

        // Picked if CRTST_INPUT_FILL_STYLE = rs_exp
        case CRTST_INPUT_DATA_FRAMEBASE_POW2:
            fp.FConfigRandom("CRTST_INPUT_DATA_FRAMEBASE_POW2");
            fp.FAddRandRange(1,0,1);
            fp.FAddRandRange(1,-5,5);
            fp.FAddRandRange(1,-10,10);
            fp.FAddRandRange(1,-50,50);
            fp.FAddRandRange(1,-100,100);
            fp.CompileRandom();
            break;

        // Picked if CRTST_INPUT_FILL_STYLE = rs_exp
        case CRTST_INPUT_DATA_FRAMEBASE_POW2_DBL:
            fp.FConfigRandom("CRTST_INPUT_DATA_FRAMEBASE_POW2_DBL");
            fp.FAddRandRange(1,0,1);
            fp.FAddRandRange(1,-5,5);
            fp.FAddRandRange(1,-10,10);
            fp.FAddRandRange(1,-100,100);
            fp.FAddRandRange(1,-500,500);
            fp.FAddRandRange(1,-1000,1000);
            fp.CompileRandom();
            break;

        // Picked if CRTST_INPUT_FILL_STYLE = rs_range
        case CRTST_INPUT_DATA_RANGE_MIN:
            fp.FConfigConst(0.0);
            break;

        // Picked if CRTST_INPUT_FILL_STYLE = rs_range
        case CRTST_INPUT_DATA_RANGE_MAX:
            fp.FConfigConst(1.0);
            break;

        case CRTST_CTRL_TILE_WIDTH:  // in threads (not bytes)
            fp.ConfigRandom("CRTST_CTRL_TILE_WIDTH");
            fp.AddRandRange(25, 80, 512);    // 5-32 blks
            fp.AddRandRange(30, 16, 160);    // 1-10 blks
            fp.AddRandRange(20, 80, 512);    // 5-32 blks
            fp.AddRandRange(25, 16, 160);    // 1-10 blks
            fp.CompileRandom();
            break;

        case CRTST_CTRL_TILE_HEIGHT: // in threads (not bytes)
            fp.ConfigRandom("CRTST_CTRL_TILE_HEIGHT");
            fp.AddRandRange(25, 80, 384);   // 5-24 blks
            fp.AddRandRange(30, 16, 96 );   // 1-6  blks
            fp.AddRandRange(20, 80, 256);   // 5-16 blks
            fp.AddRandRange(25, 16, 96 );   // 1-6  blks
            fp.CompileRandom();
            break;

        case CRTST_CTRL_THREADS_PER_LAUNCH:
            fp.ConfigConst(0);  // default: launch all kernels for a frame
            break;

        //
        // Pickers to support SeqMatrixArithmeticTest kernel in lwdarandom.lw
        //
        case CRTST_KRNL_SEQ_MTX_NUM_OPS: // Number of ops for this loop
            fp.ConfigRandom("CRTST_KRNL_SEQ_MTX_NUM_OPS");
            fp.AddRandRange(40,   10,  50);
            fp.AddRandRange(50,   50, 100);
            fp.AddRandRange(10,  100, Seq32OpsSize);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_SEQ_MTX_OP_IDX: // Index into global opcode list
            fp.ConfigRandom("CRTST_KRNL_SEQ_MTX_OP_IDX");
            fp.AddRandRange(1, 0, Seq32OpsSize-1);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_SEQ_MTX_OPCODE: // Opcodes to use
            fp.ConfigRandom("CRTST_KRNL_SEQ_MTX_OPCODE");
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_Add       );
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_Sub       );
            fp.AddRandItem(15, CRTST_KRNL_SEQ_MTX_OPCODE_Mul       );
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_Mul24     );
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_Sad       );
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_uSad      );
            fp.AddRandItem(15, CRTST_KRNL_SEQ_MTX_OPCODE_Div       );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_Abs       );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_Min       );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_Max       );
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_Sin       );
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_Cos       );
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_Tan       );
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_Log       );
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_Log2      );
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_Log10     );
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_Pow       );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_Int2Float );
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_AtomicAdd );
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_AtomicSub );
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_AtomicExch);
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_AtomicMin );
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_AtomicMax );
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_AtomicInc );
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_AtomicDec );
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_AtomicCas );
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_AtomicAnd );
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_AtomicOr  );
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_AtomicXor );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_Iadd );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_Isub );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_Imul );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_Idiv );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_Imul32i);
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_Fadd32i);
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_Fmul32i);
            fp.CompileRandom();
            // Total count of added opcodes
            fp.GetNumItems(&(coverageTracker.KernelNumOpcodes[CRTST_CTRL_KERNEL_SeqMatrix]));
            break;

        case CRTST_KRNL_SEQ_MTX_DBL_OPCODE: // Opcodes to use
            fp.ConfigRandom("CRTST_KRNL_SEQ_MTX_DBL_OPCODE");
            fp.AddRandItem( 5,CRTST_KRNL_SEQ_MTX_OPCODE_Add   );
            fp.AddRandItem( 3,CRTST_KRNL_SEQ_MTX_OPCODE_Sub   );
            fp.AddRandItem( 3,CRTST_KRNL_SEQ_MTX_OPCODE_Mul   );
            fp.AddRandItem( 3,CRTST_KRNL_SEQ_MTX_OPCODE_Div   );
            fp.AddRandItem( 5,CRTST_KRNL_SEQ_MTX_OPCODE_fmarn );
            fp.AddRandItem( 5,CRTST_KRNL_SEQ_MTX_OPCODE_fmarz );
            fp.AddRandItem( 5,CRTST_KRNL_SEQ_MTX_OPCODE_fmaru );
            fp.AddRandItem( 5,CRTST_KRNL_SEQ_MTX_OPCODE_fmard );
            fp.AddRandItem( 3,CRTST_KRNL_SEQ_MTX_OPCODE_addrn );
            fp.AddRandItem( 3,CRTST_KRNL_SEQ_MTX_OPCODE_addrz );
            fp.AddRandItem( 3,CRTST_KRNL_SEQ_MTX_OPCODE_addru );
            fp.AddRandItem( 3,CRTST_KRNL_SEQ_MTX_OPCODE_addrd );
            fp.AddRandItem( 5,CRTST_KRNL_SEQ_MTX_OPCODE_mulrn );
            fp.AddRandItem( 5,CRTST_KRNL_SEQ_MTX_OPCODE_mulrz );
            fp.AddRandItem( 5,CRTST_KRNL_SEQ_MTX_OPCODE_mulru );
            fp.AddRandItem( 5,CRTST_KRNL_SEQ_MTX_OPCODE_mulrd );
            fp.AddRandItem( 1,CRTST_KRNL_SEQ_MTX_OPCODE_divrn );
            fp.AddRandItem( 1,CRTST_KRNL_SEQ_MTX_OPCODE_divrz );
            fp.AddRandItem( 1,CRTST_KRNL_SEQ_MTX_OPCODE_divru );
            fp.AddRandItem( 1,CRTST_KRNL_SEQ_MTX_OPCODE_divrd );
            fp.AddRandItem( 1,CRTST_KRNL_SEQ_MTX_OPCODE_rcprn );
            fp.AddRandItem( 1,CRTST_KRNL_SEQ_MTX_OPCODE_rcprz );
            fp.AddRandItem( 1,CRTST_KRNL_SEQ_MTX_OPCODE_rcpru );
            fp.AddRandItem( 1,CRTST_KRNL_SEQ_MTX_OPCODE_rcprd );
            fp.AddRandItem( 1,CRTST_KRNL_SEQ_MTX_OPCODE_sqrtrn);
            fp.AddRandItem( 1,CRTST_KRNL_SEQ_MTX_OPCODE_sqrtrz);
            fp.AddRandItem( 1,CRTST_KRNL_SEQ_MTX_OPCODE_sqrtru);
            fp.AddRandItem( 1,CRTST_KRNL_SEQ_MTX_OPCODE_sqrtrd);
            fp.AddRandItem( 1,CRTST_KRNL_SEQ_MTX_OPCODE_ldsldu);
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_Iadd );
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_Isub );
            fp.AddRandItem( 3, CRTST_KRNL_SEQ_MTX_OPCODE_Imul );
            fp.AddRandItem( 2, CRTST_KRNL_SEQ_MTX_OPCODE_Idiv );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_Min  );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_Max  );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_ldg  );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_clmad);
            fp.CompileRandom();
            fp.GetNumItems(&(coverageTracker.KernelNumOpcodes[CRTST_CTRL_KERNEL_SeqMatrixDbl]));
            break;

        case CRTST_KRNL_SEQ_MTX_DBL_NUM_OPS:
            fp.ConfigRandom("CRTST_KRNL_SEQ_MTX_DBL_NUM_OPS");
            fp.AddRandRange(15,  1, 15);
            fp.AddRandRange(70, 15, 85);
            fp.AddRandRange(15, 85, Seq64OpsSize);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_SEQ_MTX_DBL_OP_IDX: // Index into global opcode list
            fp.ConfigRandom("CRTST_KRNL_SEQ_MTX_DBL_OP_IDX");
            fp.AddRandRange(1, 0, Seq64OpsSize-1);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_SEQ_MTX_OPERAND: // operand sources
            fp.ConfigRandom("CRTST_KRNL_SEQ_MTX_OPERAND");
            fp.AddRandItem(5,CRTST_KRNL_SEQ_MTX_OPERAND_MatrixA);
            fp.AddRandItem(5,CRTST_KRNL_SEQ_MTX_OPERAND_MatrixB);
            fp.AddRandItem(5,CRTST_KRNL_SEQ_MTX_OPERAND_TextureA);
            fp.AddRandItem(5,CRTST_KRNL_SEQ_MTX_OPERAND_TextureB);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_SEQ_MTX_INT_NUM_OPS: // Number of ops for this loop
            fp.ConfigRandom("CRTST_KRNL_SEQ_MTX_INT_NUM_OPS");
            fp.AddRandRange(10,   1, 20);
            fp.AddRandRange(70,  20, 80);
            fp.AddRandRange(20,  80, SeqIntOpsSize);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_SEQ_MTX_INT_OP_IDX: // Index into global opcode list
            fp.ConfigRandom("CRTST_KRNL_SEQ_MTX_INT_OP_IDX");
            fp.AddRandRange(1, 0, SeqIntOpsSize-1);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_SEQ_MTX_INT_OPCODE: // Opcodes to use
            fp.ConfigRandom("CRTST_KRNL_SEQ_MTX_INT_OPCODE");
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_vadd      );
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_vsub      );
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_vabsdiff  );
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_vmin      );
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_vmax      );
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_vshl      );
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_vshr      );
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_vmad      );
            fp.AddRandItem( 5, CRTST_KRNL_SEQ_MTX_OPCODE_vset      );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_vadd2     );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_vsub2     );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_vavrg2    );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_vabsdiff2 );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_vmin2     );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_vmax2     );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_vset2     );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_vadd4     );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_vsub4     );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_vavrg4    );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_vabsdiff4 );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_vmin4     );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_vmax4     );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_vset4     );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_prmt      );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_addcc     );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_subcc     );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_madcchi   );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_madcclo   );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_shfl      );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_shf       );
            fp.AddRandItem( 1, CRTST_KRNL_SEQ_MTX_OPCODE_dp4a      );
            fp.CompileRandom();
            fp.GetNumItems(&(coverageTracker.KernelNumOpcodes[CRTST_CTRL_KERNEL_SeqMatrixInt]));
            break;

        case CRTST_KRNL_SEQ_MTX_INT_MAT_A_MIN:
            fp.ConfigRandom();
            fp.AddRandItem(1, 0);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_SEQ_MTX_INT_MAT_A_MAX:
            fp.ConfigRandom();
            fp.AddRandItem(1, 0xffffffff);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_SEQ_MTX_INT_MAT_B_MIN:
            fp.ConfigRandom();
            fp.AddRandItem(1, 0);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_SEQ_MTX_INT_MAT_B_MAX:
            fp.ConfigRandom();
            fp.AddRandItem(1, 0xffffffff);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_SEQ_MTX_INT_MAT_C_MIN:
            fp.ConfigRandom();
            fp.AddRandItem(1, 0);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_SEQ_MTX_INT_MAT_C_MAX:
            fp.ConfigRandom();
            fp.AddRandItem(1, 0xffffffff);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_STRESS_NUM_OPS:
            fp.ConfigRandom();
            fp.AddRandRange(1, 50, 800);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_STRESS_MAT_C_HIGH_BOUND:
            fp.FConfigRandom("CRTST_KRNL_STRESS_MAT_C_HIGH_BOUND");
            fp.FAddRandRange(9, 1.5F, 1.75F);
            fp.FAddRandRange(5, 1.75F, 2.0F);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_STRESS_MAT_C_LOW_BOUND:
            fp.FConfigRandom("CRTST_KRNL_STRESS_MAT_C_LOW_BOUND");
            fp.FAddRandRange(9, 0.5F, 0.75F);
            fp.FAddRandRange(6, 0.75F, 0.90F);
            fp.CompileRandom();
            break;

        // Debug pickers.
        //increment the kernel's idx value by this amount.
        //Set to zero if you want to always use the same input matrix element
        //for a given thread.
        case CRTST_KRNL_SEQ_MTX_DEBUG_IDX_INC:
            fp.ConfigConst(1);
            break;

        // Print this kernel and its input/output tile data after launch.
        case CRTST_PRINT:
            fp.ConfigConst(0);
            break;

        // What print priority to use when printing raw tile data.
        case CRTST_PRINT_TILE_PRIORITY:
            fp.ConfigConst(Tee::PriSecret);
            break;

        case CRTST_SYNC:
            fp.ConfigConst(0);
            break;

        case CRTST_LAUNCH_STREAM:
            fp.ConfigRandom();
            fp.AddRandRange(1, 0, CRTST_LAUNCH_STREAM_Num-1);
            fp.CompileRandom();
            break;

        case CRTST_BRANCH_INPUT_MIN:
            fp.ConfigRandom();
            fp.AddRandItem(1, 0);
            fp.CompileRandom();
            break;

        case CRTST_BRANCH_INPUT_MAX:
            fp.ConfigRandom();
            fp.AddRandItem(1, 0xffffffff);
            fp.CompileRandom();
            break;

        case CRTST_BRANCH_OUTPUT_MIN:
            fp.ConfigRandom();
            fp.AddRandItem(1, 0);
            fp.CompileRandom();
            break;

        case CRTST_BRANCH_OUTPUT_MAX:
            fp.ConfigRandom();
            fp.AddRandItem(1, 0);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_TEX_NUM_OPS: // Number of ops for this loop
            fp.ConfigRandom("CRTST_KRNL_TEX_NUM_OPS");
            fp.AddRandRange(10,   1, 20);
            fp.AddRandRange(70,  20, 80);
            fp.AddRandRange(20,  80, TexOpsSize);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_TEX_OP_IDX: // Index into global opcode list
            fp.ConfigRandom();
            fp.AddRandRange(1, 0, TexOpsSize-1);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_TEX_OPCODE: // Opcodes to use
            fp.ConfigRandom("CRTST_KRNL_TEX_OPCODE");
            fp.AddRandItem( 1, CRTST_KRNL_TEX_OPCODE_tex1D      );
            fp.AddRandItem( 1, CRTST_KRNL_TEX_OPCODE_tex2D      );
            fp.AddRandItem( 1, CRTST_KRNL_TEX_OPCODE_tex3D      );
            fp.AddRandItem( 1, CRTST_KRNL_TEX_OPCODE_tex1DL     );
            fp.AddRandItem( 1, CRTST_KRNL_TEX_OPCODE_tex2DL     );
            fp.AddRandItem( 1, CRTST_KRNL_TEX_OPCODE_tld4       );
            fp.AddRandItem( 1, CRTST_KRNL_TEX_OPCODE_texclamphi );
            fp.AddRandItem( 1, CRTST_KRNL_TEX_OPCODE_texclamplo );
            fp.AddRandItem( 1, CRTST_KRNL_TEX_OPCODE_texwraphi  );
            fp.AddRandItem( 1, CRTST_KRNL_TEX_OPCODE_texwraplo  );
            fp.CompileRandom();
            fp.GetNumItems(&(coverageTracker.KernelNumOpcodes[CRTST_CTRL_KERNEL_Texture]));
            break;

        case CRTST_KRNL_SURF_NUM_OPS: // Number of ops for this loop
            fp.ConfigRandom("CRTST_KRNL_SURF_NUM_OPS");
            fp.AddRandRange(70,   1, 25);
            fp.AddRandRange(30,  25, SurfOpsSize);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_SURF_OP_IDX: // Index into global opcode list
            fp.ConfigRandom();
            fp.AddRandRange(1, 0, SurfOpsSize-1);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_SURF_RND_BLOCK:
            fp.ConfigRandom();
            fp.AddRandRange(1, 0, 0xffffffff);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_SURF_OPCODE: // Opcodes to use
            fp.ConfigRandom("CRTST_KRNL_SURF_OPCODE");
            fp.AddRandItem( 1, CRTST_KRNL_SURF_OPCODE_suld1DA );
            fp.AddRandItem( 1, CRTST_KRNL_SURF_OPCODE_suld2DA );
            fp.CompileRandom();
            fp.GetNumItems(&(coverageTracker.KernelNumOpcodes[CRTST_CTRL_KERNEL_Surface]));
            break;

        case CRTST_INPUT_DATA_TEXSURF:
            fp.FConfigRandom();
            fp.FAddRandRange(1,-1.0, 1.0);
            fp.CompileRandom();
            break;

        // Number of sub-surfaces available for the SurfaceSubtest
        case CRTST_INPUT_NUM_SUBSURF:
            fp.ConfigRandom();
            fp.AddRandItem(1, 30);
            fp.CompileRandom();
            break;

        case CRTST_DEBUG_INPUT:
            fp.ConfigRandom();
            fp.AddRandItem(1, 0x3f800000);
            fp.CompileRandom();
            break;

        case CRTST_DEBUG_OUTPUT:
            fp.ConfigRandom();
            fp.AddRandItem(1, 0);
            fp.CompileRandom();
            break;

        case CRTST_MAT_INIT_INT_MIN:
            fp.ConfigRandom();
            fp.AddRandItem(1, 100);
            fp.CompileRandom();
            break;

        case CRTST_MAT_INIT_INT_MAX:
            fp.ConfigRandom();
            fp.AddRandItem(1, 9999);
            fp.CompileRandom();
            break;

        case CRTST_MAT_INIT_FLOAT_MIN:
            fp.FConfigRandom();
            fp.FAddRandItem(1, -100.0F);
            fp.CompileRandom();
            break;

        case CRTST_MAT_INIT_FLOAT_MAX:
            fp.FConfigRandom();
            fp.FAddRandItem(1, 100.0F);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_ATOM_OPCODE:
            fp.ConfigRandom("CRTST_KRNL_ATOM_OPCODE");
            // Global-memory atom/red compile to real HW instructions.
            // But shared-memory atom/red compile to LDSLK,op,STSUL.
            // Also, the SeqMatrix kernel also does shared-memory atomics.
            // So we concentrate effort on global.
            fp.AddRandRange(9, // Global-memory atomics
                            CRTST_KRNL_ATOM_atom_global_and_b32,
                            CRTST_KRNL_ATOM_atom_global_max_s32);
            fp.AddRandRange(5, // Global-memory reduce
                            CRTST_KRNL_ATOM_red_global_and_b32,
                            CRTST_KRNL_ATOM_red_global_max_s32);
            fp.AddRandRange(1, // Shared-memory atomics
                            CRTST_KRNL_ATOM_atom_shared_and_b32,
                            CRTST_KRNL_ATOM_atom_shared_max_u32);
            fp.AddRandRange(1, // Shared-memory reduce
                            CRTST_KRNL_ATOM_red_shared_and_b32,
                            CRTST_KRNL_ATOM_red_shared_max_u32);
            fp.AddRandRange(5, // 64 bit Global-memory atomics
                            CRTST_KRNL_ATOM_atom_global_and_b64,
                            CRTST_KRNL_ATOM_atom_global_max_u64);
            fp.AddRandRange(5, // 64 bit Shared-memory atomics
                            CRTST_KRNL_ATOM_atom_shared_and_b64,
                            CRTST_KRNL_ATOM_atom_shared_max_u64);
            fp.AddRandRange(5, // 32 bit shared memory redux sync
                            CRTST_KRNL_ATOM_redux_sync_add_u32,
                            CRTST_KRNL_ATOM_redux_sync_xor_b32);
            fp.CompileRandom();
            // As ranges are used, for total we use the max/end of range
            coverageTracker.KernelNumOpcodes[CRTST_CTRL_KERNEL_Atom] = CRTST_KRNL_ATOM_redux_sync_xor_b32;
            break;

        case CRTST_KRNL_ATOM_ARG0:
            fp.ConfigRandom();
            fp.AddRandRange(1, 0, 0xffffffff);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_ATOM_ARG1:
            fp.FConfigRandom();
            fp.FAddRandRange(1, -100.0F, 100.0F);
            fp.CompileRandom();
            break;

       case CRTST_KRNL_ATOM_SAME_WARP:
            fp.ConfigRandom("CRTST_KRNL_ATOM_SAME_WARP");
            fp.AddRandItem(3, 1);
            fp.AddRandItem(1, 0);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_ATOM_NUM_OPS:
            fp.ConfigRandom();
            fp.AddRandRange(1, 1, AtomOpsSize);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_ATOM_OP_IDX:
            fp.ConfigRandom();
            fp.AddRandRange(1, 0, AtomOpsSize-1);
            fp.CompileRandom();
            break;

        case CRTST_KRNL_ATOM_OUTPUT_MIN:
            fp.ConfigConst(0);
            break;

        case CRTST_KRNL_ATOM_OUTPUT_MAX:
            fp.ConfigConst(0xffffffff);
            break;

        default:
            MASSERT(!"Your new picker doesn't have a default configuration");
            rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LwdaRandomTest::InitFromJs()
{
    RC rc;
    // Tell TestConfiguration and Goldelwalues to get their user options
    // and print them at low priority.
    CHECK_RC( GpuTest::InitFromJs() );

    m_LoopsPerSurface =
        max<UINT32>(GetTestConfiguration()->RestartSkipCount(), 1);
    m_SurfWidth  = AlignDown<BLOCK_SIZE>(GetTestConfiguration()->SurfaceWidth());
    m_SurfHeight = AlignDown<BLOCK_SIZE>(GetTestConfiguration()->SurfaceHeight());

    if (m_SurfSizeOverride)
    {
        // Callwlate a width & height that are multiples of BLOCK_SIZE and meet
        // the m_SurfSizeOverride value.
        // This callwlation is based on single precision floats,
        // not double precision.
        const UINT32 surfSize = static_cast<UINT32>(
            AlignDown<BLOCK_SIZE * BLOCK_SIZE>(m_SurfSizeOverride / sizeof(float)));
        UINT32 width  = m_SurfWidth;
        UINT32 height = surfSize / m_SurfWidth;
        for (; height < BLOCK_SIZE && width > BLOCK_SIZE ;)
        {
            width -= BLOCK_SIZE;
            height = surfSize / width;
        }
        MASSERT(width >= BLOCK_SIZE && height >= BLOCK_SIZE);

        m_SurfWidth = width;
        m_SurfHeight = height;
    }
    //colwert to byte
    m_SurfWidth *= sizeof(float);

    return rc;
}

//-----------------------------------------------------------------------------
RC LwdaRandomTest::Setup()
{
    RC rc;
    UINT32 i;

    CHECK_RC(LwdaStreamTest::Setup());

    // Seed our RNG in case someone uses it during Setup (see bug 1435803).
    SetFpCtxMap({{FpCtxArgs::Seed, GetTestConfiguration()->Seed()}});

    //
    // Create subtests
    m_Subtests[CRTST_CTRL_KERNEL_SeqMatrix]         .reset(NewSeq32Subtest(this, GetLwdaInstancePtr(), GetBoundLwdaDevice()));
    m_Subtests[CRTST_CTRL_KERNEL_SeqMatrixDbl]      .reset(NewSeq64Subtest(this, GetLwdaInstancePtr(), GetBoundLwdaDevice()));
    m_Subtests[CRTST_CTRL_KERNEL_SeqMatrixStress]   .reset(NewSeq32StressSubtest(this, GetLwdaInstancePtr(), GetBoundLwdaDevice()));
    m_Subtests[CRTST_CTRL_KERNEL_SeqMatrixDblStress].reset(NewSeq64StressSubtest(this, GetLwdaInstancePtr(), GetBoundLwdaDevice()));
    m_Subtests[CRTST_CTRL_KERNEL_SeqMatrixInt]      .reset(NewSeqIntSubtest(this, GetLwdaInstancePtr(), GetBoundLwdaDevice()));
    m_Subtests[CRTST_CTRL_KERNEL_Stress]            .reset(NewStressSubtest(this));
    m_Subtests[CRTST_CTRL_KERNEL_SmallDebug]        .reset(NewDebugSubtest(this));
    m_Subtests[CRTST_CTRL_KERNEL_Branch]            .reset(NewBranchSubtest(this));
    m_Subtests[CRTST_CTRL_KERNEL_Texture]           .reset(NewTextureSubtest(this, GetLwdaInstancePtr(), GetBoundLwdaDevice()));
    m_Subtests[CRTST_CTRL_KERNEL_Surface]           .reset(NewSurfaceSubtest(this, GetLwdaInstancePtr(), GetBoundLwdaDevice()));
    m_Subtests[CRTST_CTRL_KERNEL_MatrixInit]        .reset(NewMatrixInitSubtest(this));
    m_Subtests[CRTST_CTRL_KERNEL_Atom]              .reset(NewAtomSubtest(this, GetLwdaInstancePtr(), GetBoundLwdaDevice()));
    m_Subtests[CRTST_CTRL_KERNEL_GpuWorkCreation]   .reset(NewGpuWorkCreationSubtest(this));

    //
    // Get a valid lwca context & our bound GPU device.
    //
    Lwca::Device device = GetBoundLwdaDevice();

    //
    // Get this GPUs device properties. We'll need then to compute
    // the optimum grid & block size later.
    //
    memset(&m_Property, 0, sizeof(m_Property));
    if (OK == device.GetPropertiesSilent(&m_Property))
    {
        m_GotProperties = true;
    }

    m_Cap = device.GetCapability();

    //
    // Load the right gpu code module.
    //
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("crnd", &m_Module));

    //
    // Allocate and bind textures and surfaces for input
    //
    const UINT32 TxSz = LwdaRandomTexSize;
    const LWarray_format Format = LW_AD_FORMAT_FLOAT;
    const UINT32 Layered = LWDA_ARRAY3D_LAYERED;
    const LWaddress_mode Clamp = LW_TR_ADDRESS_MODE_CLAMP;
    const LWaddress_mode Wrap = LW_TR_ADDRESS_MODE_WRAP;
    const UINT32 Norm = LW_TRSF_NORMALIZED_COORDINATES;
    const ArrayParam texParam[] =
    {
        { TxSz, TxSz, 0,    Format, 1, 0,       Clamp, 0,    "LwdaRandomTexInputA" }
        ,{TxSz, TxSz, 0,    Format, 1, 0,       Clamp, 0,    "LwdaRandomTexInputB" }
        ,{TxSz, 0,    0,    Format, 1, 0,       Clamp, 0,    "LwdaRandomTexInput1D" }
        ,{TxSz, TxSz, TxSz, Format, 1, 0,       Clamp, 0,    "LwdaRandomTexInput3D" }
        ,{TxSz, TxSz, 0,    Format, 1, Layered, Clamp, 0,    "LwdaRandomTexInput1DL" }
        ,{TxSz, TxSz, TxSz, Format, 1, Layered, Clamp, 0,    "LwdaRandomTexInput2DL" }
        ,{TxSz, TxSz, 0,    Format, 1, 0,       Clamp, Norm, "LwdaRandomTexInputNormClamp" }
        ,{TxSz, TxSz, 0,    Format, 1, 0,       Wrap,  Norm, "LwdaRandomTexInputNormWrap" }
    };

    MASSERT(NUM_TEX == NUMELEMS(texParam));

    for (UINT32 i = 0; i < NUM_TEX; i+=1)
    {
        m_InputTex[i] = GetLwdaInstance().AllocArray(
                                texParam[i].width,
                                texParam[i].height,
                                texParam[i].depth,
                                texParam[i].format,
                                texParam[i].channels,
                                texParam[i].flags);

        CHECK_RC(m_InputTex[i].InitCheck());
        Lwca::Texture inputTex = m_Module.GetTexture(texParam[i].name);
        inputTex.SetAddressMode(texParam[i].addrMode, 0);
        if (texParam[i].flags != Layered)
        {
            if (texParam[i].height != 0)
            {
                inputTex.SetAddressMode(texParam[i].addrMode, 1);
                if (texParam[i].depth != 0)
                    inputTex.SetAddressMode(texParam[i].addrMode, 2);
            }
        }
        else
        {
            if (texParam[i].height != 0 && texParam[i].depth != 0)
                inputTex.SetAddressMode(texParam[i].addrMode, 1);

        }
        inputTex.SetFlag(texParam[i].texSurfRefFlags);
        CHECK_RC(inputTex.BindArray(m_InputTex[i]));
    }

    // Alloc streams.
    for (i = 0; i < CRTST_LAUNCH_STREAM_Num; i++)
    {
        m_Streams[i] = GetLwdaInstance().CreateStream();
        CHECK_RC(m_Streams[i].InitCheck());
    }

    // Set up the kernels we will run.
    //
    for (i = 0; i < NUMELEMS(m_Subtests); i++)
    {
        if (m_Subtests[i]->IsSupported())
            CHECK_RC(m_Subtests[i]->Setup());
    }

    //
    // Allocate a stream and some events.
    //
    for (i = 0; i < evNumEvents; i++)
    {
        m_Events[i] = GetLwdaInstance().CreateEvent();
        CHECK_RC(m_Events[i].InitCheck());
    }

    //
    // Allocate some host & device memory for input/output matrices
    //
    for (i = 0; i < mxNumMatrices; i++)
    {
        m_devSurface[i].width = m_SurfWidth;
        m_devSurface[i].height= m_SurfHeight;
        m_devSurface[i].elementSize  = sizeof(char);
        m_devSurface[i].pitch = m_devSurface[i].width * m_devSurface[i].elementSize;
        m_devSurface[i].offsetX = 0;
        m_devSurface[i].offsetY = 0;
        m_devSurface[i].fillType = ft_uint32;

        CHECK_RC(GetLwdaInstance().AllocDeviceMem(
                     m_devSurface[i].pitch * m_devSurface[i].height, &m_devMemory[i]));
        m_devSurface[i].elements = m_devMemory[i].GetDevicePtr();
    }

    //
    // Get some local system memory to initialize the device memory
    //
    for (i = 0; i < mxNumMatrices; i++)
    {
        const UINT32 size = static_cast<UINT32>(m_devMemory[i].GetSize());
        m_sysMatrix[i].reset(new SysMatrixVec(size,
                                      UINT08(),
                                      Lwca::HostMemoryAllocator<UINT08>(GetLwdaInstancePtr(),
                                                                        GetBoundLwdaDevice())));
        InitSysMatrix(static_cast<MatrixIdx>(i));
    }

    if (m_GoldenCheckInputs == true)
        m_GSurfs.SetNumSurfaces(gldNumSurfaces);
    else
        m_GSurfs.SetNumSurfaces(gldNumOutputSurfaces);

    GetGoldelwalues()->SetSurfaces(&m_GSurfs);
    GetGoldelwalues()->SetPrintCallback( &LWRandom_Print, this );
    GetGoldelwalues()->AddErrorCallback( &LWRandom_Error, &m_GSurfs );

    const UINT32 maxSMs = GetBoundGpuSubdevice()->GetMaxShaderCount();
    if(GetGoldelwalues()->GetAction() == Goldelwalues::Store)
        m_NumSMs = maxSMs;
    else
        m_NumSMs = GetBoundLwdaDevice().GetShaderCount();

    m_MaxThreadsPerFrame =
        (UINT32)((m_sysMatrix[mxOutputC]->size()/sizeof(UINT32)));

    m_AllocatedThreadsPerFrame =
        (UINT32)(((float)m_NumSMs/(float)maxSMs) *
        m_sysMatrix[mxOutputC]->size()/sizeof(UINT32));

    Printf(GetVerbosePrintPri(),
        "MaxNumSMs:%d MaxThreadsPerFrame:%d\n",
        maxSMs, m_MaxThreadsPerFrame);
    Printf(GetVerbosePrintPri(),
        "InstalledSMs:%d AllocatedThreadsPerFrame:%d\n",
        GetBoundLwdaDevice().GetShaderCount(), m_AllocatedThreadsPerFrame);

    return rc;
}

//----------------------------------------------------------------------------
LaunchParam * LwdaRandomTest::NewLaunchParam()
{
    UINT32 subIdx = min((*m_pPickers)[CRTST_CTRL_KERNEL].Pick(),
                        UINT32(NUMELEMS(m_Subtests)-1));
    Subtest * pSubtest = m_Subtests[subIdx].get();

    if (!pSubtest->IsSupported())
    {
        pSubtest = m_Subtests[CRTST_CTRL_KERNEL_SeqMatrix].get();

        // Default kernel, should be supported on all lwca-compatible gpus.
        MASSERT(pSubtest->IsSupported());
    }

    // Create a subtest/kernel specific set of launch parameters.
    return pSubtest->NewLaunchParam();
}

//------------------------------------------------------------------------------
void LwdaRandomTest::FillLaunchParam(LaunchParam * const pLP)
{
    pLP->print = (*m_pPickers)[CRTST_PRINT].Pick() != 0;
    pLP->sync  = (*m_pPickers)[CRTST_SYNC].Pick() != 0;
    pLP->streamIdx = (*m_pPickers)[CRTST_LAUNCH_STREAM].Pick() %
        CRTST_LAUNCH_STREAM_Num;

    // TODO: vary block/CTA and grid dimensionality and sizes.
    // TODO: assign launch to one of N streams.

    pLP->blockWidth   =  BLOCK_SIZE;
    pLP->blockHeight  =  BLOCK_SIZE;
    pLP->gridWidth    =  pLP->outputTile.width /
        (pLP->pSubtest->ElementSize() * BLOCK_SIZE);
    pLP->gridHeight   =  pLP->outputTile.height / BLOCK_SIZE;
    pLP->numThreads   =  pLP->gridWidth  *
        pLP->gridHeight *
        pLP->blockWidth *
        pLP->blockHeight;
    pLP->event        = m_Streams[pLP->streamIdx].CreateEvent();
    pLP->outputTile.fillType = ft_uint32;

    MASSERT(OK == pLP->event.InitCheck());
}

//-----------------------------------------------------------------------------
RC LwdaRandomTest::Run()
{
    StickyRC firstRc = InnerRun();
    firstRc = GetGoldelwalues()->ErrorRateTest(GetJSObject());
    return firstRc;
}

RC LwdaRandomTest::InnerRun()
{
    StickyRC finalRc;
    const UINT32 startFrame = GetTestConfiguration()->StartLoop() /
                              m_LoopsPerSurface;
    const UINT32 endFrame   = (GetTestConfiguration()->StartLoop() +
                               GetTestConfiguration()->Loops() +
                               m_LoopsPerSurface - 1) / m_LoopsPerSurface;
    m_MsecTot = 0.0F;
    PrintProgressInit(endFrame - startFrame, "Test Configuration Loops");
    //-------------------------------------------------------------------------
    for (UINT32 frame = startFrame; frame < endFrame; frame++)
    {
        RC rc;

        // Clear the tiles, pick a new set of tiles, opcodes, etc.
        CHECK_RC(RandomizeOncePerRestart(frame));

        // Mark as skipped any loops outside the start/end loop.
        MarkSkippedLoops(frame);

        // Launch a frame's worth of tiles
        CHECK_RC(Launch(frame));

        CHECK_RC(EndLoop((frame + 1) * m_LoopsPerSurface));

        // Perform golden actions on this frames tiles
        RC frameRc;
        finalRc = frameRc = ProcessGoldelwalues(frame);

        const bool isMiscompare =
            (frameRc == RC::GOLDEN_VALUE_MISCOMPARE_Z) ||
            (frameRc == RC::GOLDEN_VALUE_MISCOMPARE);

        if (finalRc != RC::OK)
        {
            PrintErrorUpdate(frame - startFrame + 1, finalRc);
        }
        else
        {
            PrintProgressUpdate(frame - startFrame + 1);
        }

        if ((frameRc != OK) &&
            (m_StopOnError || !isMiscompare))
        {
            // Stop testing on first error if StopOnError == true.
            // Always stop on a non-miscompare error.
            break;
        }
    }
    // Print coverage stats
    if (m_Coverage)
    {
        FancyPicker &fp = (*m_pPickers)[CRTST_KRNL_SEQ_MTX_OPCODE];
        fp.GetNumItems(&(coverageTracker.KernelNumOpcodes[CRTST_CTRL_KERNEL_SeqMatrix]));
        Printf(Tee::PriNormal, "OpCode Coverage Stats ==> \n");
        // Ignoring non-opcode random kernels like SmallDebug, MatrixInit
        for (auto i :
                {
                    CRTST_CTRL_KERNEL_SeqMatrix,
                    CRTST_CTRL_KERNEL_SeqMatrixDbl,
                    CRTST_CTRL_KERNEL_SeqMatrixInt,
                    CRTST_CTRL_KERNEL_Texture,
                    CRTST_CTRL_KERNEL_Surface,
                    CRTST_CTRL_KERNEL_Atom
                })
        {
            float CoveragePercentage = (float)coverageTracker.KernelOpcodeStore[i].size() /
                                       (float)coverageTracker.KernelNumOpcodes[i] * 100;
            Printf(Tee::PriNormal, "%-40s %.2f%%\n",
                   m_Subtests[i]->Name(), CoveragePercentage);
        }
    }

    return finalRc;
}

//------------------------------------------------------------------------------
// If the start/stop points for the test run aren't aligned on a frame boundary,
// mark the first N or last N loops as skipped, without modifying the random seq.
//
void LwdaRandomTest::MarkSkippedLoops(const UINT32 Frame)
{
    const UINT32 start = max(Frame * m_LoopsPerSurface,
                             GetTestConfiguration()->StartLoop());
    const UINT32 end = min((Frame + 1) * m_LoopsPerSurface,
                           GetTestConfiguration()->StartLoop() +
                           GetTestConfiguration()->Loops());

    for (UINT32 loop = Frame * m_LoopsPerSurface;
         loop < (Frame + 1) * m_LoopsPerSurface; loop++)
    {
        if (loop < start || loop >= end)
            m_LaunchParams[loop % m_LoopsPerSurface]->skip = true;
    }
}

//------------------------------------------------------------------------------
void LwdaRandomTest::FreeLaunchParams()
{
    const size_t len = m_LaunchParams.size();
    for (size_t i = 0; i < len; i++)
    {
        m_LaunchParams[i]->event.Free();
        delete m_LaunchParams[i];
    }
}

//------------------------------------------------------------------------------
RC LwdaRandomTest::RandomizeOncePerRestart(const UINT32 Frame)
{
    RC rc;
    const UINT32 seed = GetTestConfiguration()->Seed() + Frame *
                        m_LoopsPerSurface;

    // Reset the random-number generator.
    SetFpCtxMap({{FpCtxArgs::Seed, seed},
                 {FpCtxArgs::LoopNum, Frame * m_LoopsPerSurface},
                 {FpCtxArgs::RestartNum, Frame}});

    // We will reseed at the end of this function, so that what happens
    // within the Pick* functions doesn't change the launch sequence later.
    // This makes debugging easier...
    const UINT32 nextSeed = m_pFpCtx->Rand.GetRandom();

    InitializeFreeTiles(&m_devSurface[mxOutputC]);
    FreeLaunchParams();
    m_LaunchParams.resize(m_LoopsPerSurface, NULL);
    m_LaunchParamIndices.resize(m_LoopsPerSurface, 0);

    m_pPickers->Restart();

    // Give each subtest a chance to do per-frame stuff.
    for (size_t i = 0; i < NUMELEMS(m_Subtests); i++)
    {
        if (m_Subtests[i]->IsSupported())
            CHECK_RC(m_Subtests[i]->RandomizeOncePerRestart(Frame));
    }

    // Pick the "loops" aka "launches" we will use this frame.
    for (UINT32 loop = 0; loop < m_LoopsPerSurface; loop++)
    {
        SetFpCtxMap({{FpCtxArgs::LoopNum, Frame * m_LoopsPerSurface + loop}});
        LaunchParam * pLP = NewLaunchParam();
        m_LaunchParams[loop] = pLP;

        // Pick a non-overlapping rectangular region of the output matrix C.
        // The corresponding tile in the input matrices A and B also
        // "belong" to this launch.
        PickOutputTile(&(pLP->outputTile), pLP->pSubtest->ElementSize());

        if (0 == pLP->outputTile.width)
        {
            // No room in output surface, can't launch this one.
            pLP->skip = true;
        }
        else
        {
            pLP->skip = false;

            // Pick generic launch properties (block/CTA dim, grid dim, etc).
            FillLaunchParam(pLP);

            // Finish filling in launch details (subtest-specific).
            pLP->pSubtest->FillLaunchParam(pLP);

            // Fill in starting data for matrix A,B,C in this launch's tile.
            // This actually fills only sysmem copy of A,B,C (CPU fill).
            if (!pLP->skip)
                pLP->pSubtest->FillSurfaces(pLP);
        }
    }

    // Pre-clear surfaces when using GPU fill so we can tell if something went
    // wrong in the kernel
    if (m_FillSurfaceOnGPU && m_ClearSurfacesWithGPUFill)
    {
        InitSysMatrix(mxInputA);
        InitSysMatrix(mxInputB);
        InitSysMatrix(mxOutputC);
    }

    // Copy sysmem copies of A,B,C to FB memory.
    if (!m_FillSurfaceOnGPU || m_ClearSurfacesWithGPUFill)
        CHECK_RC(CopyMemToDevice());

    // Always fill the entire SMID memory with 0xFF before starting a frame.
    // This allows verification that the kernels actually ran
    memset(&(*m_sysMatrix[mxOutputSMID])[0],
           0xFF,
           m_devMemory[mxOutputSMID].GetSize());
    CHECK_RC(m_devMemory[mxOutputSMID].Set(&(*m_sysMatrix[mxOutputSMID])[0],
                        m_devMemory[mxOutputSMID].GetSize()));

    // Randomize sysmem copy of texture and send to FB memory.
    CHECK_RC(RandomizeTexSurfData());

    // Choose some launches to skip.
    // We want to scale work back based on the number of SM engines available,
    // compared to the baseline when we set initial test duration.
    UINT32 loop, threads;
    for (loop = 0, threads = 0; loop < m_LoopsPerSurface; loop++)
    {
        LaunchParam * pLP = m_LaunchParams[loop];

        if (threads > m_MaxThreadsPerFrame)
            pLP->skip = true;

        if (!pLP->skip)
            threads += pLP->numThreads;
    }

    // Choose what order to do the launches: in order, or shuffled.
    for (UINT32 i = 0; i < m_LoopsPerSurface; i++)
        m_LaunchParamIndices[i] = i;

    if ((*m_pPickers)[CRTST_CTRL_SHUFFLE_TILES].Pick())
    {
        m_pFpCtx->Rand.Shuffle(m_LoopsPerSurface, &m_LaunchParamIndices[0]);
        for (auto& picker : m_FpCtxMap)
        {
            picker.Ctx.Rand.Shuffle(m_LoopsPerSurface, &m_LaunchParamIndices[0]);
        }
    }

    SetFpCtxMap({{FpCtxArgs::Seed, nextSeed}});

    return rc;
}

//------------------------------------------------------------------------------
RC LwdaRandomTest::ProcessGoldelwalues(const UINT32 Frame)
{
    RC rc;
    StickyRC finalRc;

    UINT08 * sysmemPtrs[mxNumMatrices];
    for (UINT32 m = 0; m < mxNumMatrices; m++)
        sysmemPtrs[m] = &(*m_sysMatrix[m])[0];

    const size_t getSize = static_cast<size_t>(m_devMemory[mxOutputC].GetSize());

    // copy the tiles from the device memory back to system memory
    if (m_GoldenCheckInputs == true )
    {
        m_devMemory[mxInputA].Get(sysmemPtrs[mxInputA], getSize);
        m_devMemory[mxInputB].Get(sysmemPtrs[mxInputB], getSize);
    }
    m_devMemory[mxOutputC].Get(sysmemPtrs[mxOutputC], getSize);
    m_devMemory[mxOutputSMID].Get(sysmemPtrs[mxOutputSMID],getSize);

    CHECK_RC(GetLwdaInstance().Synchronize());

    const UINT32 maxSMs = GetBoundGpuSubdevice()->GetMaxShaderCount();
    for (UINT32 loop = Frame * m_LoopsPerSurface;
         loop < (Frame + 1) * m_LoopsPerSurface; loop++)
    {
        // we have to set this var to get the failing loop properly reported
        SetFpCtxMap({{FpCtxArgs::LoopNum, loop}});
        (*m_pPickers)[CRTST_PRINT_TILE_PRIORITY].Pick();

        const UINT32 idx = m_LaunchParamIndices[loop%m_LoopsPerSurface];
        LaunchParam * pLP = m_LaunchParams[idx];

        if (!pLP->skip)
        {
            GetGoldelwalues()->SetLoop(loop);

            // We must describe each checked tile (just C or all of A,B,C) to
            // the GoldenSurfaces object.  We will pretend the surface format is
            // A8R8G8B8 because that format works for dumping .pngs etc.

            const Tile * pTile = &pLP->outputTile;
            UINT32 gWidth  = pTile->width / sizeof(UINT32);
            UINT32 gHeight = pTile->height;
            UINT32 gPitch  = pTile->pitch;
            UINT32 gOffset = static_cast<UINT32>
                (pTile->elements - m_devMemory[mxOutputC].GetDevicePtr());

            if (m_GoldenCheckInputs == true)
            {
                m_GSurfs.DescribeSurface(gWidth, gHeight, gPitch,
                                         sysmemPtrs[mxInputA] + gOffset,
                                         gldInputA, "mtxA");
                m_GSurfs.DescribeSurface(gWidth, gHeight, gPitch,
                                         sysmemPtrs[mxInputB] + gOffset,
                                         gldInputB, "mtxB");
            }
            m_GSurfs.DescribeSurface(gWidth, gHeight, gPitch,
                                     sysmemPtrs[mxOutputC] + gOffset,
                                     gldOutputC, "mtxC");

            RC tileRc;
            finalRc = tileRc = GetGoldelwalues()->Run();

            bool Afailed = false;
            bool Bfailed = false;
            bool Cfailed = tileRc != OK;
            if (m_GoldenCheckInputs)
            {
                Afailed = m_GSurfs.GetRC(gldInputA) != OK;
                Bfailed = m_GSurfs.GetRC(gldInputB) != OK;
                Cfailed = m_GSurfs.GetRC(gldOutputC) != OK;
            }

            if (tileRc != OK)
            {
                Printf(Tee::PriNormal, "Golden miscompare at loop %d idx %d\n",
                       loop, idx);
                pLP->print = true;

                if (Afailed || Bfailed)
                {
                    // An input matrix miscompare is a software bug.
                    finalRc.Clear();
                    finalRc = RC::SOFTWARE_ERROR;
                }
            }

            // Print details of this launch if the CRTST_PRINT picker says so,
            // or if it miscompared.
            if(pLP->print)
            {
                Printf(Tee::PriNormal, "Loop:%d\n", loop);
                PrintLaunch(Tee::PriNormal, pLP);

                if (Cfailed || GetTestConfiguration()->Verbose())
                {
                    if (!m_GoldenCheckInputs)
                    {
                        m_devMemory[mxInputA].Get(sysmemPtrs[mxInputA], getSize);
                        m_devMemory[mxInputB].Get(sysmemPtrs[mxInputB], getSize);
                        GetLwdaInstance().Synchronize();
                    }
                    PrintTile(mxInputA, pTile);
                    PrintTile(mxInputB, pTile);
                    PrintTile(mxOutputC, pTile);
                    PrintTile(mxOutputSMID, pTile);
                }
                else
                {
                    if (Afailed || GetTestConfiguration()->Verbose())
                        PrintTile(mxInputA, pTile);
                    if (Bfailed || GetTestConfiguration()->Verbose())
                        PrintTile(mxInputB, pTile);
                }
            }
            if ((tileRc != OK) && m_StopOnError)
                break;

            if (pLP->checkMat)
               CHECK_RC(CheckMatrix(mxOutputC, pTile, pLP));

            if (pLP->checkSmid)
            {
               Tile smidTile = pLP->outputTile;
               smidTile.fillType = ft_uint32;
               smidTile.min.u = 0;
               smidTile.max.u = maxSMs;
               smidTile.max.u--;
               if (OK != CheckMatrix(mxOutputSMID, &smidTile, pLP))
               {
                   Printf(Tee::PriError,
                          "%s : Invalid SMID data, suggests that one or more "
                          "kernel threads did not run!\n", GetName().c_str());
                   return RC::HW_STATUS_ERROR;
               }
            }
        }
    }

    return finalRc;
}

//------------------------------------------------------------------------------
RC LwdaRandomTest::RandomizeTexSurfHelper
(
    Lwca::Array * input
    ,FVector * hostTexSurf
    ,const size_t pitch
)
{
    RC rc;
    FVector::iterator itr;
    // Randomize for each texture
    for (itr = hostTexSurf->begin(); itr != hostTexSurf->end(); ++itr)
        *itr = (*m_pPickers)[CRTST_INPUT_DATA_TEXSURF].FPick();

    CHECK_RC(input->Set(&(*hostTexSurf)[0], static_cast<unsigned>(pitch)));
    CHECK_RC(GetLwdaInstance().Synchronize());

    return rc;
}

//------------------------------------------------------------------------------
RC LwdaRandomTest::RandomizeTexSurfData()
{
    RC rc;
    // Fill "input" textures with random values
    FVector hostTexSurf(Lwca::HostMemoryAllocator<float>(GetLwdaInstancePtr(),
                                                         GetBoundLwdaDevice()));
    hostTexSurf.resize(LwdaRandomTexSize * LwdaRandomTexSize * LwdaRandomTexSize, 0.0F);

    const size_t texPitch =
                (&hostTexSurf[LwdaRandomTexSize] - &hostTexSurf[0]) * sizeof(float);

    for (UINT32 i = 0; i < NUM_TEX; i+=1)
        CHECK_RC(RandomizeTexSurfHelper(&(m_InputTex[i]), &hostTexSurf, texPitch));

    return rc;
}

//------------------------------------------------------------------------------
void LwdaRandomTest::InitializeFreeTiles(Tile * const pEntireDstSurf)
{
    while (m_FreeTiles.size())
    {
        delete m_FreeTiles.front();
        m_FreeTiles.pop_front();
    }

    Tile * pInitialFreeTile = new Tile;
    *pInitialFreeTile = *pEntireDstSurf;

    m_FreeTiles.push_front(pInitialFreeTile);
}

//------------------------------------------------------------------------------
void LwdaRandomTest::PickOutputTile(Tile * pOutTile, UINT32 elementSize)
{
    // Setting usedTile.width=0 marks this unusable (assume can't get output tile).
    Tile usedTile = {0};

    if (!m_FreeTiles.empty())
    {
        // Consume the first free tile
        Tile * pFree = m_FreeTiles.front();
        m_FreeTiles.pop_front();
        usedTile = *pFree;
        usedTile.elementSize = elementSize;

        // get the dimensions of the next tile and adjust the
        // size to fit within the first free tile.
        UINT32 h = (*m_pPickers)[CRTST_CTRL_TILE_HEIGHT].Pick();
        UINT32 w = (*m_pPickers)[CRTST_CTRL_TILE_WIDTH].Pick();

        // 0 is interpreted to mean max possible width/height.
        // Picker returns "pixel" width, here we want byte width.
        if (w == 0)
            w = pFree->width;
        else
            w = w * elementSize;

        if (h == 0)
            h = pFree->height;

        // clamp to free tile size
        w = min(w, pFree->width);
        h = min(h, pFree->height);

        // for now make sure the width & height are increments of BLOCK_SIZE
        // TODO: Remove BLOCK_SIZE restrictions once the kernels have been updated
        w = AlignDown(w, BLOCK_SIZE * elementSize);
        h = AlignDown<BLOCK_SIZE>(h);

        if (w && h)
        {
            // Update the free list of our new tile
            if ((w < pFree->width) && (h < pFree->height))
            {
                // We wrote neither the full width nor height,
                // split it into 2 free blocks.
                Tile * pFree2 = new Tile;
                *pFree2 = *pFree;
                m_FreeTiles.push_front(pFree);
                m_FreeTiles.push_front(pFree2);

                if ((pFree->width - w) > (pFree->height - h))
                {   // Largest free space is at the right.
                    // full-height block to the right
                    pFree2->width   -= w;
                    pFree2->offsetX += w;

                    // little block below
                    pFree->width    = w;
                    pFree->height   -= h;
                    pFree->offsetY  += h;
                }
                else
                {   // Largest free space is at the bottom.
                    // little block to the right
                    pFree2->width   -= w;
                    pFree2->offsetX += w;
                    pFree2->height   = h;

                    // full-width block below (width doesn't change)
                    pFree->height  -= h;
                    pFree->offsetY += h;

                }
            }
            else if (w < pFree->width)
            {
                // We wrote the full height, but not the width.
                // Adjust freeblock in place.
                m_FreeTiles.push_front(pFree);
                pFree->width -= w;
                pFree->offsetX += w;

            }
            else if (h < pFree->height)
            {
                // We wrote the full width, but not the full height.
                // Adjust freeblock in place.
                m_FreeTiles.push_front(pFree);
                pFree->height -= h;
                pFree->offsetY += h;

            }
            else
            {
                // We wrote the whole free tile.
                delete pFree;
            }
            // update the used tile with the new dimensions
            usedTile.width  = w;
            usedTile.height = h;
            usedTile.elements = DevTileAddr(mxOutputC, &usedTile);
        }
        else // tile is too small for this kernel
        {
            m_FreeTiles.push_front(pFree);
            usedTile.width = 0; // Cause the launch to be skipped.
        }
    }
    // else, no free tile

    *pOutTile = usedTile;
}

//------------------------------------------------------------------------------
//! Simple utility for printing Tile coords (but not contents).
void PrintTile(Tee::Priority pri, const char * name, const Tile & t)
{
    Printf(pri,
           "%s:%dx%d,pitch:%d size:%d offsetX:%d offsetY:%d elements:0x%08llX\n",
           name,
           t.width,
           t.height,
           t.pitch,
           t.elementSize,
           t.offsetX,
           t.offsetY,
           t.elements);
}

//------------------------------------------------------------------------------
void TileDataCounts::Update(FLOAT64 x)
{
    if (Utility::IsNan(x))
    {
        // Not a Number, for example sqrt(-1.0).
        nan++;
    }
    else if (Utility::IsInf(x))
    {
        // Positive or negative infinity, for example 1.0/0.0.
        inf++;
    }
    else if (x == 0.0 || x == -0.0)
    {
        // Floating-point math includes a negative zero.
        zero++;
    }
    else
    {
        other++;
    }
}

//------------------------------------------------------------------------------
//! Fancy utility for printing a tile and its contents.
//! Note that we follow pTile only for the rectangle etc and print the data
//! and addresses for the MatrixIdx.  I.e. we ignore the Tile's device ptr.
//!
//! Note: We have to periodically flush the print queue to prevent a
//! SOFTWARE_ERROR failure during cleanup because the print queue was overloaded.
//! The print que can hold upto 32K calls (see ::QueueDepth in teeq.cpp) to
//! Print(), I picked 25K to allow other threads some space while dumping this
//! potentially large chunk of data.

void LwdaRandomTest::PrintTile(MatrixIdx mtx, const Tile* pTile)
{
    UINT32 prnts = 0;
    const char * name[] = {"TileA","TileB","TileC","TileSMID"};
    TileDataCounts counts;
    LwdaRandom::PrintTile(Tee::PriNormal, name[mtx], *pTile);
    Tee::Priority pri =
        (Tee::Priority)(*m_pPickers)[CRTST_PRINT_TILE_PRIORITY].GetPick();

    Tee::TeeFlushQueuedPrints(Tasker::NO_TIMEOUT);

    const bool printHex = (mtx == mxOutputSMID) || m_PrintHex;
    const UINT08 * pStart = static_cast<UINT08*>(SysTileAddr(mtx, pTile));
    for (UINT32 y = 0; y < pTile->height; y++)
    {
        if (pTile->elementSize == sizeof(double))
        {
            const FLOAT64 * pRow = reinterpret_cast<const FLOAT64*>
                (pStart + y * pTile->pitch);
            const FLOAT64 * pRowEnd = pRow + pTile->width/sizeof(FLOAT64);
            while (pRow < pRowEnd)
            {
                const double d = *pRow;
                if (printHex)
                    Printf(pri, "%llX ",
                           *reinterpret_cast<const UINT64*>(pRow));
                else
                    Printf(pri, "%g ", d);

                counts.Update(d);
                ++pRow;
                if ((++prnts % 25000) == 0)
                {
                    Tee::TeeFlushQueuedPrints(Tasker::NO_TIMEOUT);
                }
            }
        }
        else
        {
            const FLOAT32 * pRow = reinterpret_cast<const FLOAT32*>
                (pStart + y * pTile->pitch);
            const FLOAT32 * pRowEnd = pRow + pTile->width/sizeof(FLOAT32);
            while (pRow < pRowEnd)
            {
                const float f = *pRow;
                if(printHex)
                    Printf(pri, "%X ",
                           *reinterpret_cast<const UINT32*>(pRow));
                else
                    Printf(pri, "%g ", f);

                counts.Update(static_cast<FLOAT64>(*pRow));
                ++pRow;
                if ((++prnts % 25000) == 0)
                {
                    Tee::TeeFlushQueuedPrints(Tasker::NO_TIMEOUT);
                }
            }
        }
        Printf(pri, "\n");
    }
    double toPct = 100.0 / (pTile->height * (pTile->width / pTile->elementSize));
    Printf(Tee::PriNormal, "(zero:%04.1f%% inf:%04.1f%% nan:%04.1f%% good:%04.1f%%)\n",
               counts.zero * toPct,
               counts.inf * toPct,
               counts.nan * toPct,
               counts.other * toPct);
    // allow the print queue to drain to prevent print queue overflow on cleanup.
    Tee::TeeFlushQueuedPrints(Tasker::NO_TIMEOUT);
}

//------------------------------------------------------------------------------
RC LwdaRandomTest::CopyMemToDevice()
{
    RC rc;
    CHECK_RC(m_devMemory[mxInputA].Set(&(*m_sysMatrix[mxInputA])[0],
                         m_devMemory[mxInputA].GetSize()));

    CHECK_RC(m_devMemory[mxInputB].Set(&(*m_sysMatrix[mxInputB])[0],
                         m_devMemory[mxInputB].GetSize()));

    CHECK_RC(m_devMemory[mxOutputC].Set(&(*m_sysMatrix[mxOutputC])[0],
                        m_devMemory[mxOutputC].GetSize()));
    return rc;
}

//------------------------------------------------------------------------------
void LwdaRandomTest::InitSysMatrix(MatrixIdx mtx)
{
    // Pre-fill differently for gpugen vs. test, to help catch unwritten
    // memory bugs.
    const UINT32 size = static_cast<UINT32>(m_devMemory[mtx].GetSize());
    const int fill = (GetGoldelwalues()->GetAction() == Goldelwalues::Store) ?
        0x0a+mtx : 0xa0+(mtx<<4);
    memset(&(*m_sysMatrix[mtx])[0], fill, size);
}

//------------------------------------------------------------------------------
// Launch a frames worth of kernels.
RC LwdaRandomTest::Launch(const UINT32 Frame)
{
    StickyRC rc;
#if defined(DEBUG)
    // If the lwca driver asserts, print out what we were trying to do.
    Platform::BreakPointHooker hooker(&LWRandom_Print, this);
#endif

    CHECK_RC(m_Events[evBegin].Record());

    // Each loop will process a single tile of memory. We will launch a frame's
    // worth of tiles.

    const UINT32 startLoop = Frame * m_LoopsPerSurface;
    const UINT32 endLoop   = (Frame + 1) * m_LoopsPerSurface;
    for (UINT32 loop = startLoop; loop < endLoop; loop++)
    {
        // Set this so that Print (from BreakPointHooker) can see it.
        SetFpCtxMap({{FpCtxArgs::LoopNum, loop}});

        UINT32 idx = m_LaunchParamIndices[loop % m_LoopsPerSurface];
        LaunchParam * pLP = m_LaunchParams[idx];

        if (pLP->skip)
            continue;

        rc = FireIsmExpCallback(pLP->pSubtest->Name());

        rc = pLP->pSubtest->Launch(pLP);
        rc = pLP->event.Record();
        if (pLP->sync)
            rc = m_Streams[pLP->streamIdx].Synchronize();

        if (rc != OK)
        {
            Printf(GetVerbosePrintPri(), "Launch failed on loop:%d launch:%d\n", loop, idx);
            Print(GetVerbosePrintPri());
            break;
        }
    }

    const UINT32 syncTimeoutSec = static_cast<UINT32>(ceil(
            GetTestConfiguration()->TimeoutMs() / 1000));
    for (int i = 0; i < CRTST_LAUNCH_STREAM_Num; i++)
    {
        rc = m_Streams[i].Synchronize(syncTimeoutSec);
    }
    if (rc != OK)
    {
        Printf(GetVerbosePrintPri(), "Synchronization failed on loops:%d-%d",
               startLoop, endLoop - 1);
        Print(GetVerbosePrintPri());
    }

    // WAR for bug 858958.
    // Previously, the record takes place before the stream synchronization.
    // MCP89 seems to have has issues with event synchronization causing hangs
    // and assertion failures.
    CHECK_RC(m_Events[evEnd].Record());
    CHECK_RC(m_Events[evEnd].Synchronize());

    Stats stats;
    stats.maxBytes = m_devSurface[mxOutputC].width * m_devSurface[mxOutputC].height;
    CollectStats(&stats);

    float msThisFrame = m_Events[evEnd].TimeMsElapsedSinceF(m_Events[evBegin]);
    m_MsecTot += msThisFrame;
    if(GetTestConfiguration()->Verbose())
    {
        Printf(GetVerbosePrintPri(),
               "Loops:%d-%d completed in %.2fms, total:%.2fms\n"
               " Blks:%d SurfCov:%3.2f%% Skipped:%d\n"
               " Thrds:%d MaxThrdsPerFrm:%d\n",
               startLoop,
               endLoop-1,
               msThisFrame,
               m_MsecTot,
               stats.totBlks,
               (float)((float)stats.totBytes*100.0 / (float)stats.maxBytes),
               stats.skipped,
               stats.numThreads,
               m_MaxThreadsPerFrame);
        for (int i = 0; i < CRTST_CTRL_KERNEL_NUM_KERNELS; i++)
        {
            Printf(GetVerbosePrintPri(), " %-40s %02d launches %2.3fms\n",
                   m_Subtests[i]->Name(),
                   stats.kernels[i],
                   stats.msPerKernel[i]);
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
// Assign custom contexts to Pickers
void LwdaRandomTest::LinkFpCtxMap(FancyPicker* fp, UINT32 index)
{
    auto itr = find_if(m_FpCtxMap.begin(), m_FpCtxMap.end(), [&] (const FpCtxNode& x)
             { return x.Id == index;});

    if (itr != m_FpCtxMap.end())
    {
        itr->Ctx = *m_pFpCtx;
        fp->SetContext(addressof(itr->Ctx));
    }
}

void LwdaRandomTest::SetFpCtxMap(const vector<pair<FpCtxArgs,UINT32>>& FpParams)
{
    for (auto param : FpParams)
    {
        switch (param.first)
        {
            case FpCtxArgs::Seed:
            {
                m_pFpCtx->Rand.SeedRandom(param.second);
                for (auto& j : m_FpCtxMap)
                {
                    j.Ctx.Rand.SeedRandom(param.second);
                }
                break;
            }
            case FpCtxArgs::LoopNum:
            {
                m_pFpCtx->LoopNum = param.second;
                for (auto& j : m_FpCtxMap)
                {
                    j.Ctx.LoopNum = param.second;
                }
                break;
            }
            case FpCtxArgs::RestartNum:
            {
                m_pFpCtx->RestartNum = param.second;
                for (auto& j : m_FpCtxMap)
                {
                    j.Ctx.RestartNum = param.second;
                }
                break;
            }
        }
    }
}

// Collect some basic statistics.
void LwdaRandomTest::CollectStats(Stats * pS)
{
    const Lwca::Event * pPrevEvent = &m_Events[evBegin];

    for (UINT32 loop = 0; loop < m_LoopsPerSurface; loop++)
    {
        UINT32 idx = m_LaunchParamIndices[loop];
        LaunchParam * pLP = m_LaunchParams[idx];

        if (pLP->skip)
        {
            pS->skipped++;
        }
        else
        {
            pS->numThreads  += pLP->numThreads;
            pS->totBytes    +=  pLP->gridWidth * pLP->blockWidth *
                                pLP->gridHeight * pLP->blockHeight *
                                pLP->outputTile.elementSize;
            pS->totBlks     +=  pLP->gridWidth * pLP->gridHeight;

            for (int k = 0; k < CRTST_CTRL_KERNEL_NUM_KERNELS; k++)
            {
                if (pLP->pSubtest == m_Subtests[k].get())
                {
                    pS->kernels[k]++;
                    pS->msPerKernel[k] += pLP->event.TimeMsElapsedSinceF(
                        *pPrevEvent);
                }
            }
        }
    }
}

//-----------------------------------------------------------------------------
RC LwdaRandomTest::Cleanup()
{
    // Free memory.
    GetGoldelwalues()->ClearSurfaces();

    while (m_FreeTiles.size())
    {
        Tile *pTile = m_FreeTiles.front();
        delete pTile;
        m_FreeTiles.pop_front();
    }

    for (UINT32 i = 0; i < NUM_TEX; i+=1)
        m_InputTex[i].Free();

    for (int i = 0; i < CRTST_LAUNCH_STREAM_Num; i++)
    {
        m_Streams[i].Free();
    }

    m_Module.Unload();
    for (UINT32 i = 0; i < evNumEvents; i++)
        m_Events[i].Free();

    for (UINT32 i = 0; i < mxNumMatrices; i++)
    {
        m_devMemory[i].Free();
    }

    // Release memory ASAP instead of waiting for the destructors.
    for (UINT32 i = 0; i < mxNumMatrices; i++)
        m_sysMatrix[i].reset(0);

    FreeLaunchParams();
    m_LaunchParams.clear();
    m_LaunchParamIndices.clear();

    StickyRC rc;
    for (UINT32 i = 0; i < NUMELEMS(m_Subtests); i++)
    {
        if (0 != m_Subtests[i].get() && m_Subtests[i]->IsSupported())
            rc = m_Subtests[i]->Cleanup();
    }

    for (size_t i = 0; i < NUMELEMS(m_Subtests); i++)
    {
        m_Subtests[i].reset(0);
    }

    rc = LwdaStreamTest::Cleanup();
    return rc;
}

//------------------------------------------------------------------------------
void LwdaRandomTest::Print(const Tee::Priority Pri)
{
    const UINT32 loop = m_pFpCtx->LoopNum;
    const UINT32 idx  = m_LaunchParamIndices[loop % m_LoopsPerSurface];
    const LaunchParam* pLP = m_LaunchParams[idx];

    Printf(Pri, "Loop:%d ParamIdx:%d\n", loop, idx);
    Printf(Pri, "Compute Capabilities: %d.%d \tNumSMs:%d\n",
                m_Cap.MajorVersion(), m_Cap.MinorVersion(),m_NumSMs);
    if (m_GotProperties)
    {
        Printf(Pri, "Device Properties:\n"
                "\tMaxThreadsPerBlock:%d\n"
                "\tMaxThreadsDim: %dx%dx%d\n"
                "\tMaxGridSize: %dx%dx%d\n"
                "\tSharedMemPerBlock: %d bytes\n"
                "\tTotalConstantMemory: %d bytes\n"
                "\tWarpSize: %d threads\n"
                "\tMaxPitch: %d bytes\n"
                "\tRegsPerBlock: %d\n"
                "\tClockRate: %d khz"
                "\tTextureAlign: %d\n",
                m_Property.maxThreadsPerBlock,
                m_Property.maxThreadsDim[0],m_Property.maxThreadsDim[1],
                m_Property.maxThreadsDim[2],
                m_Property.maxGridSize[0],m_Property.maxGridSize[1],
                m_Property.maxGridSize[2],
                m_Property.sharedMemPerBlock,
                m_Property.totalConstantMemory,
                m_Property.SIMDWidth,
                m_Property.memPitch,
                m_Property.regsPerBlock,
                m_Property.clockRate,
                m_Property.textureAlign);
    }

    Printf(Pri, "Surface Params:%dx%d pitch:%d bytes\n",
                m_SurfWidth, m_SurfHeight, (UINT32)(m_SurfWidth));
    Printf(Pri, "Device Params:\n");
    LwdaRandom::PrintTile(Pri, "SurfA", m_devSurface[mxInputA]);
    LwdaRandom::PrintTile(Pri, "SurfB", m_devSurface[mxInputB]);
    LwdaRandom::PrintTile(Pri, "SurfC", m_devSurface[mxOutputC]);
    LwdaRandom::PrintTile(Pri, "SurfSMID", m_devSurface[mxOutputSMID]);

    PrintLaunch(Pri, pLP);
}

//------------------------------------------------------------------------------
void LwdaRandomTest::PrintLaunch(Tee::Priority Pri, const LaunchParam * pLP)
{
    Printf(Pri,
           "Launch Parameters:\n"
           "\tThreads:%d\n"
           "\tGrid:%dx%d\n"
           "\tBlock:%dx%d\n"
           "\tKernel:%s",
           pLP->numThreads,
           pLP->gridWidth, pLP->gridHeight,
           pLP->blockWidth, pLP->blockHeight,
           pLP->pSubtest->Name());

    if (pLP->skip)
    {
        Printf(Pri, " (skipped)\n");
    }
    else
    {
        Printf(Pri, " (%s) stream:%d\n",
               pLP->sync ? "sync" : "async",
               pLP->streamIdx);
    }
    pLP->pSubtest->Print(Pri, pLP);
}

//------------------------------------------------------------------------------
void LwdaRandomTest::PrintJsProperties(Tee::Priority Pri)
{
    const char * TF[2] = { "false", "true"};
    GpuTest::PrintJsProperties(Pri);
    Printf(Pri,
        "LwdaRandom Js Properties:\n"
        "\tSurfSizeOverride:\t\t0x%x (%.1fe6 bytes)\n"
        "\tGoldenCheckInputs:\t\t%s\n"
        "\tPrintHex:\t\t\t%s\n"
        "\tSetMatrixValue:\t\t\t%s\n"
        "\tMatrixAValue:\t\t\t%f\n"
        "\tMatrixBValue:\t\t\t%f\n"
        "\tMatrixCValue:\t\t\t%f\n"
        "\tSetSmallInt:\t\t\t%s\n"
        "\tSetByteInt:\t\t\t%s\n"
        "\tFillSurfaceOnGPU:\t\t%s\n"
        "\tStopOnError:\t\t\t%s\n"
        "\tClearSurfacesWithGPUFill:\t%s\n"
        "\tCoverage:\t\t\t%s\n",
        m_SurfSizeOverride,
        m_SurfSizeOverride/1.0e6,
        TF[m_GoldenCheckInputs],
        TF[m_PrintHex],
        TF[m_SetMatrixValue],
        m_MatrixAValue,
        m_MatrixBValue,
        m_MatrixCValue,
        TF[m_SetSmallInt],
        TF[m_SetByteInt],
        TF[m_FillSurfaceOnGPU],
        TF[m_StopOnError],
        TF[m_ClearSurfacesWithGPUFill],
        TF[m_Coverage]
    );
}

//------------------------------------------------------------------------------
//! gpu virtual address of top-left element of the tile in this matrix.
UINT64 LwdaRandomTest::DevTileAddr(MatrixIdx idx, const Tile* pTile) const
{
    MASSERT(idx < mxNumMatrices);

    const UINT64 base = m_devMemory[idx].GetDevicePtr();
    const UINT32 pitch = m_devSurface[idx].pitch;

    return  base + pTile->offsetY * pitch + pTile->offsetX;
}

//------------------------------------------------------------------------------
//! cpu virtual address of the sysmem-copy of the top-left-element of the tile
//! in this matrix.

// This version for when we know which matrix the tile is in, but don't
// trust or are lying about the tile's device-address.
void* LwdaRandomTest::SysTileAddr(MatrixIdx idx, const Tile * pTile) const
{
    MASSERT(idx < mxNumMatrices);

    UINT08 * const base = &((*m_sysMatrix[idx])[0]);
    const UINT32 pitch = m_devSurface[idx].width;  // sysmem copy has no padding

    return base + pTile->offsetY * pitch + pTile->offsetX;
}

// This version is for when we just have the tile itself.
void* LwdaRandomTest::SysTileAddr(const Tile * pTile) const
{
    for (int i = 0; i < mxNumMatrices; i++)
    {
        MatrixIdx idx = static_cast<MatrixIdx>(i);
        if (DevTileAddr(idx, pTile) == pTile->elements)
            return SysTileAddr(idx, pTile);
    }
    MASSERT(!"misformed Tile");
    return NULL;
}

//------------------------------------------------------------------------------
//! Used for verifying surface inits by checking if PRNG gives bad values
//! This is a slow function and should only be used with tiles associated
//! with the MatrixInit subtest.
//!
//! For integers: checks if generated value is within the range set in FP
//! For floats: checks if INF/NAN are generated
//!             if float is a range, checks if it is within range set in FP

RC LwdaRandomTest::CheckMatrix(MatrixIdx mtx, const Tile * pTile, LaunchParam * pLP)
{
    TileDataCounts counts;
    Tee::Priority pri = GetVerbosePrintPri();

    const UINT08 * pStart = static_cast<UINT08*>(SysTileAddr(mtx, pTile));
    for (UINT32 y = 0; y < pTile->height; y++)
    {
        switch (pTile->fillType)
        {
            default:
            {
                break;
            }
            case ft_float32:
            {
                const FLOAT32 * pRow = reinterpret_cast<const FLOAT32*>
                    (pStart + y * pTile->pitch);
                const FLOAT32 * pRowEnd = pRow + pTile->width/sizeof(FLOAT32);
                while (pRow < pRowEnd)
                {
                    counts.Update(static_cast<FLOAT64>(*pRow));
                    if (pTile->fillStyle == rs_range)
                    {
                        if ( *pRow < pTile->min.f || *pRow > pTile->max.f)
                        {
                            Printf(pri, "Matrix FillStyle: Float32\n");
                            Printf(pri, "Value: %g\n", *pRow);
                            return RC::ILWALID_INPUT;
                        }
                    }
                    ++pRow;
                }
                break;
            }
            case ft_float64:
            {
                const FLOAT64 * pRow = reinterpret_cast<const FLOAT64*>
                    (pStart + y * pTile->pitch);
                const FLOAT64 * pRowEnd = pRow + pTile->width/sizeof(FLOAT64);
                while (pRow < pRowEnd)
                {
                    counts.Update(*pRow);
                    if (pTile->fillStyle == rs_range)
                    {
                        if ( *pRow < pTile->min.d || *pRow > pTile->max.d)
                        {
                            Printf(pri, "Matrix FillStyle: Float64\n");
                            Printf(pri, "Value: %g\n", *pRow);
                            return RC::ILWALID_INPUT;
                        }
                    }
                    ++pRow;
                }
                break;
            }
            case ft_uint32:
            {
                const UINT32 * pRow = reinterpret_cast<const UINT32*>
                    (pStart + y * pTile->pitch);
                const UINT32 * pRowEnd = pRow + pTile->width/sizeof(UINT32);
                while (pRow < pRowEnd)
                {
                    if ( *pRow < pTile->min.u || *pRow > pTile->max.u)
                    {
                        Printf(pri, "Matrix FillStyle: UINT32\n");
                        Printf(pri, "Value: %u\n", *pRow);
                        return RC::ILWALID_INPUT;
                    }
                    ++pRow;
                }
                break;
            }
            case ft_uint16:
            {
                const UINT16 * pRow = reinterpret_cast<const UINT16*>
                    (pStart + y * pTile->pitch);
                const UINT16 * pRowEnd = pRow + pTile->width/sizeof(UINT16);
                while (pRow < pRowEnd)
                {
                    if ( *pRow < pTile->min.u || *pRow > pTile->max.u)
                    {
                        Printf(pri, "Matrix FillStyle: UINT16\n");
                        Printf(pri, "Value: %u\n", *pRow);
                        return RC::ILWALID_INPUT;
                    }
                    ++pRow;
                }
                break;
            }
            case ft_uint08:
            {
                const UINT08 * pRow = reinterpret_cast<const UINT08*>
                    (pStart + y * pTile->pitch);
                const UINT08 * pRowEnd = pRow + pTile->width/sizeof(UINT08);
                while (pRow < pRowEnd)
                {
                    if ( *pRow < pTile->min.u || *pRow > pTile->max.u)
                    {
                        Printf(pri, "Matrix FillStyle: UINT08\n");
                        Printf(pri, "Value: %u\n", *pRow);
                        return RC::ILWALID_INPUT;
                    }
                    ++pRow;
                }
                break;
            }
        }
    }

    if (counts.inf > 0 || counts.nan > 0)
    {
        Printf(pri, "ERROR: INF/NAN detected in generated values\n");
        return RC::ILWALID_INPUT;
    }

    return OK;
}

//------------------------------------------------------------------------------
Subtest::Subtest
(
    LwdaRandomTest * pParent
    ,const char * functionName
)
    : m_pParent(pParent),
      m_FunctionName(functionName),
      m_FunctionName2(NULL)
{
}

Subtest::Subtest
(
    LwdaRandomTest * pParent
    ,const char * functionName
    ,const char * functionName2
)
    : m_pParent(pParent),
      m_FunctionName(functionName),
      m_FunctionName2(functionName2)
{
}

Subtest::~Subtest()
{
}

//------------------------------------------------------------------------------
/*virtual*/
RC Subtest::Setup()
{
    RC rc;
    m_Function = GetModule()->GetFunction(m_FunctionName);
    if (m_FunctionName2 != NULL)
        m_Function2 = GetModule()->GetFunction(m_FunctionName2);

    return rc;
}

//------------------------------------------------------------------------------
// Helper class for the tile-filling routines.
//
class TileFiller
{
public:
    TileFiller(const Tile * pTile, char * pSysmemAddr)
        : m_pTile(pTile)
        , m_pBase(pSysmemAddr)
        , m_Y(0)
        , m_pLwr(pSysmemAddr)
        , m_pEndRow(pSysmemAddr + pTile->width)
    { }
    ~TileFiller() {}

    // Store one value in the (sysmem copy of) the tile, advance pointers.
    // Returns true when more room is left, false when done.
    template<class T>
    bool Fill(T val)
    {
        T * pLwr = reinterpret_cast<T*>(m_pLwr);
        *pLwr = val;
        return Advance(sizeof(T));
    }
    bool Advance(size_t bytes)
    {
        if (m_pLwr + bytes >= m_pEndRow)
        {
            if (m_Y + 1 == m_pTile->height)
            {
                // At the last address -- don't advance pointer, we're full.
                return false;
            }

            m_Y++;
            m_pLwr = m_pBase + m_Y * m_pTile->pitch;
            m_pEndRow = m_pLwr + m_pTile->width;
        }
        else
        {
            m_pLwr += bytes;
        }
        // Not full yet.
        return true;
    }

private:
    const Tile * m_pTile;
    char *       m_pBase;
    UINT32       m_Y;
    char *       m_pLwr;
    const char * m_pEndRow;
};

//------------------------------------------------------------------------------
void Subtest::FillTileFloat32Exp
(
    Tile * pTile,
    INT32 p2Min,
    INT32 p2Max,
    FLOAT64 dbgFill,
    LaunchParam * pLP
)
{
    if (!pTile)
        return;

    TileFiller Filler(pTile, static_cast<char*>(m_pParent->SysTileAddr(pTile)));
    const FLOAT32 dbgFill32 = static_cast<FLOAT32>(dbgFill);

    pTile->min.i     = p2Min;
    pTile->max.i     = p2Max;
    pTile->fillType  = ft_float32;
    pTile->fillStyle = rs_exp;

    if (m_pParent->m_SetMatrixValue)
    {
        while (Filler.Fill(dbgFill32)) /**/;
    }
    else if (m_pParent->m_FillSurfaceOnGPU)
    {
        pLP->gFP.GPUFill = true;
        pLP->gFP.seed = GetRand()->GetRandom();
    }
    else
    {
        while (Filler.Fill(GetRand()->GetRandomFloatExp(p2Min, p2Max))) /**/;
    }
}

//------------------------------------------------------------------------------
void Subtest::FillTileFloat32Range
(
    Tile * pTile,
    FLOAT32 valMin,
    FLOAT32 valMax,
    FLOAT64 dbgFill,
    LaunchParam * pLP
)
{
    if (!pTile)
        return;

    if (m_pParent->m_SetMatrixValue)
    {
        valMin = valMax = static_cast<FLOAT32>(dbgFill);
    }
    pTile->min.f     = valMin;
    pTile->max.f     = valMax;
    pTile->fillType  = ft_float32;
    pTile->fillStyle = rs_range;

    if (m_pParent->m_FillSurfaceOnGPU)
    {
        pLP->gFP.GPUFill = true;
        pLP->gFP.seed = GetRand()->GetRandom();
    }
    else
    {
        TileFiller Filler(pTile, static_cast<char*>(m_pParent->SysTileAddr(pTile)));
        while (Filler.Fill(GetRand()->GetRandomFloat(valMin, valMax)))
            /**/;
    }
}

//------------------------------------------------------------------------------
void Subtest::FillTileFloat64Range
(
    Tile * pTile,
    FLOAT64 valMin,
    FLOAT64 valMax,
    FLOAT64 dbgFill,
    LaunchParam * pLP
)
{
    if (!pTile)
        return;

    if (m_pParent->m_SetMatrixValue)
    {
        valMin = valMax = dbgFill;
    }
    pTile->min.d     = valMin;
    pTile->max.d     = valMax;
    pTile->fillType  = ft_float64;
    pTile->fillStyle = rs_range;

    if (m_pParent->m_FillSurfaceOnGPU)
    {
        pLP->gFP.GPUFill = true;
        pLP->gFP.seed = GetRand()->GetRandom();
    }
    else
    {
        TileFiller Filler(pTile, static_cast<char*>(m_pParent->SysTileAddr(pTile)));
        while (Filler.Fill(GetRand()->GetRandomDouble(valMin, valMax)))
            /**/;
    }
}

//------------------------------------------------------------------------------
void Subtest::FillTileFloat64Exp
(
    Tile * pTile,
    INT32 p2Min,
    INT32 p2Max,
    FLOAT64 dbgFill,
    LaunchParam * pLP
)
{
    if (!pTile)
        return;

    TileFiller Filler(pTile, static_cast<char*>(m_pParent->SysTileAddr(pTile)));

    pTile->min.i     = p2Min;
    pTile->max.i     = p2Max;
    pTile->fillType  = ft_float64;
    pTile->fillStyle = rs_exp;

    if (m_pParent->m_SetMatrixValue)
    {
        while (Filler.Fill(dbgFill)) /**/;
    }
    else if (m_pParent->m_FillSurfaceOnGPU)
    {
        pLP->gFP.GPUFill = true;
        pLP->gFP.seed = GetRand()->GetRandom();
    }
    else
    {
        while (Filler.Fill(GetRand()->GetRandomDoubleExp(p2Min, p2Max))) /**/;
    }
}

//------------------------------------------------------------------------------
void Subtest::FillTileUint32
(
    Tile * pTile,
    UINT32 milwal,
    UINT32 maxVal,
    LaunchParam * pLP
)
{
    if (!pTile)
        return;

    pTile->min.u     = milwal;
    pTile->max.u     = maxVal;
    pTile->fillType  = ft_uint32;

    if (m_pParent->m_FillSurfaceOnGPU)
    {
        pLP->gFP.GPUFill = true;
        pLP->gFP.seed = GetRand()->GetRandom();
        return;
    }

    TileFiller Filler(pTile, static_cast<char*>(m_pParent->SysTileAddr(pTile)));
    while (Filler.Fill(GetRand()->GetRandom(milwal, maxVal))) /**/;
}

//------------------------------------------------------------------------------
void Subtest::FillTileUint16
(
    Tile * pTile,
    UINT32 milwal,
    UINT32 maxVal,
    LaunchParam * pLP
)
{
    if (!pTile)
        return;

    pTile->min.u     = milwal;
    pTile->max.u     = maxVal;
    pTile->fillType  = ft_uint16;

    if (m_pParent->m_FillSurfaceOnGPU)
    {
        pLP->gFP.GPUFill = true;
        pLP->gFP.seed = GetRand()->GetRandom();
        return;
    }

    TileFiller Filler(pTile, static_cast<char*>(m_pParent->SysTileAddr(pTile)));
    while (Filler.Fill(static_cast<UINT16>(GetRand()->GetRandom(milwal, maxVal)))) /**/;
}

//------------------------------------------------------------------------------
void Subtest::FillTileUint08
(
    Tile * pTile,
    UINT32 milwal,
    UINT32 maxVal,
    LaunchParam * pLP
)
{
    if (!pTile)
        return;

    pTile->min.u     = milwal;
    pTile->max.u     = maxVal;
    pTile->fillType  = ft_uint08;

    if (m_pParent->m_FillSurfaceOnGPU)
    {
        pLP->gFP.GPUFill = true;
        pLP->gFP.seed = GetRand()->GetRandom();
        return;
    }

    TileFiller Filler(pTile, static_cast<char*>(m_pParent->SysTileAddr(pTile)));
    while (Filler.Fill(static_cast<UINT08>(GetRand()->GetRandom(milwal, maxVal)))) /**/;
}

//------------------------------------------------------------------------------
void Subtest::FillTilesFloat32Range
(
    Tile * pA,
    Tile * pB,
    Tile * pC,
    LaunchParam * pLP
)
{
    // For best test coverage we should be using FillTilesFloat32Exp.
    // This routine is intended to support debug activities when we get a
    // miscompare on the Sequential64bit kernels.
    const float min = FPick(CRTST_INPUT_DATA_RANGE_MIN);
    const float max = FPick(CRTST_INPUT_DATA_RANGE_MAX);

    FillTileFloat32Range(pA, min, max, GetDebugFill(mxInputA), pLP);
    FillTileFloat32Range(pB, min, max, GetDebugFill(mxInputB), pLP);
    FillTileFloat32Range(pC, min, max, GetDebugFill(mxOutputC), pLP);
}

//------------------------------------------------------------------------------
void Subtest::FillTilesFloat64Range
(
    Tile * pA,
    Tile * pB,
    Tile * pC,
    LaunchParam * pLP
)
{
    // For best test coverage we should be using FillTilesFloat64Exp.
    // This routine is intended to support debug activities when we get a
    // miscompare on the Sequential64bit kernels.
    const double min = static_cast<FLOAT64>(FPick(CRTST_INPUT_DATA_RANGE_MIN));
    const double max = static_cast<FLOAT64>(FPick(CRTST_INPUT_DATA_RANGE_MAX));

    FillTileFloat64Range(pA, min, max, GetDebugFill(mxInputA), pLP);
    FillTileFloat64Range(pB, min, max, GetDebugFill(mxInputB), pLP);
    FillTileFloat64Range(pC, min, max, GetDebugFill(mxOutputC), pLP);
}

//------------------------------------------------------------------------------
void Subtest::FillTilesFloat64Exp
(
    Tile * pA,
    Tile * pB,
    Tile * pC,
    LaunchParam * pLP
)
{
    // For best test coverage, we want nearly all the values used within this
    // launch to be in the same rough magnitude.
    // That way we don't discard too many significant bits due to normalization.
    //
    // Float64 has exponent range of +- 1023, 54 significant bits.

    INT32 launchBasePow2 =
        static_cast<INT32>(FPick(CRTST_INPUT_DATA_FRAMEBASE_POW2_DBL));
    const INT32 p2Min = MinMax<INT32>(-1022, launchBasePow2 - 12, 1023);
    const INT32 p2Max = MinMax<INT32>(-1022, launchBasePow2 + 12, 1023);

    FillTileFloat64Exp(pA, p2Min, p2Max, GetDebugFill(mxInputA), pLP);
    FillTileFloat64Exp(pB, p2Min, p2Max, GetDebugFill(mxInputB), pLP);
    FillTileFloat64Exp(pC, p2Min, p2Max, GetDebugFill(mxOutputC), pLP);
}

//------------------------------------------------------------------------------
void Subtest::FillTilesFloat32Exp
(
    Tile * pA,
    Tile * pB,
    Tile * pC,
    LaunchParam * pLP
)
{
    // For best test coverage, we want nearly all the values used within this
    // launch to be in the same rough magnitude.
    // That way we don't discard too many significant bits due to normalization.
    //
    // Float32 has exponent range of +- 127, 24 significant bits.

    INT32 launchBasePow2 =
        static_cast<INT32>(FPick(CRTST_INPUT_DATA_FRAMEBASE_POW2));
    const INT32 p2Min = MinMax<INT32>(-126, launchBasePow2 - 12, 127);
    const INT32 p2Max = MinMax<INT32>(-126, launchBasePow2 + 12, 127);

    FillTileFloat32Exp(pA, p2Min, p2Max, GetDebugFill(mxInputA), pLP);
    FillTileFloat32Exp(pB, p2Min, p2Max, GetDebugFill(mxInputB), pLP);
    FillTileFloat32Exp(pC, p2Min, p2Max, GetDebugFill(mxOutputC), pLP);
}

//------------------------------------------------------------------------------
FLOAT64 Subtest::GetDebugFill(MatrixIdx idx)
{
    MASSERT(idx < mxNumMatrices);

    switch (idx)
    {
        case mxInputA:  return m_pParent->m_MatrixAValue;
        case mxInputB:  return m_pParent->m_MatrixBValue;
        case mxOutputC: return m_pParent->m_MatrixCValue;
        case mxOutputSMID: return m_pParent->m_MatrixSMIDValue;
        default: return 0.0; // for compiler, not reachable
    }
}

UINT64 Subtest::DevTileAddr(MatrixIdx idx, const Tile* pTile) const
{
    return m_pParent->DevTileAddr(idx, pTile);
}

void Subtest::SetLaunchDim(const LaunchParam * pLP)
{
    m_Function.SetGridDim(pLP->gridWidth, pLP->gridHeight);
    m_Function.SetBlockDim(pLP->blockWidth, pLP->blockHeight);
}

const Lwca::Stream& Subtest::GetStream(int streamIdx) const
{
    MASSERT(streamIdx < CRTST_LAUNCH_STREAM_Num);
    return m_pParent->m_Streams[streamIdx];
}

UINT32 Subtest::GetIntType()
{
    if (m_pParent->m_SetSmallInt)
    {
        if (m_pParent->m_SetByteInt)
        {
            return ft_uint08;
        }
        else
        {
            return ft_uint16;
        }
    }
    return ft_uint32;
}

void Subtest::AllocateArrays
(
    Lwca::Array inputArray[],
    UINT32 arraySize,
    const ArrayParam params[]
)
{
    for (UINT32 i = 0; i < arraySize; i+=1)
    {
        inputArray[i] = m_pParent->GetLwdaInstance().AllocArray(
                            params[i].width,
                            params[i].height,
                            params[i].depth,
                            params[i].format,
                            params[i].channels,
                            params[i].flags);
    }
}

RC Subtest::FillArray(Lwca::Array inputArray[], UINT32 arraySize, UINT32 width)
{
    RC rc;
    FVector hostTexSurf(Lwca::HostMemoryAllocator<float>(m_pParent->GetLwdaInstancePtr(),
                                                         m_pParent->GetBoundLwdaDevice()));
    hostTexSurf.resize(width * LwdaRandomTexSize * LwdaRandomTexSize);
    const size_t pitch = (&hostTexSurf[width] -
                 &hostTexSurf[0]) * sizeof(float);
    for (UINT32 i = 0; i < arraySize; i+=1)
    {
        CHECK_RC(m_pParent->RandomizeTexSurfHelper(&(inputArray[i]), &hostTexSurf, pitch));
    }
    return rc;
}

RC Subtest::BindSurfaces
(
    Lwca::Array inputArray[],
    UINT32 arraySize,
    const ArrayParam params[]
)
{
    RC rc;
    for (UINT32 i = 0; i < arraySize; i+=1)
    {
        CHECK_RC(inputArray[i].InitCheck());
        Lwca::Surface inputSurf = m_pParent->m_Module.GetSurface(params[i].name);
        CHECK_RC(inputSurf.BindArray(inputArray[i]));
    }
    return rc;
}

bool Subtest::HasBug(UINT32 bugNo) const
{
    return m_pParent->GetBoundGpuSubdevice()->HasBug(bugNo);
}

} // namespace LwdaRandom
