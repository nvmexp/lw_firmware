/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2010,2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "../tools/tools.h"

// Matrix block size for x/y
#define BLOCK_SIZE  16      // 16x16 matrix yields 256 threads/block

// Fill types for use with FillMatrices
enum FillType
{
    ft_float32,
    ft_float64,
    ft_uint32,
    ft_uint16,
    ft_uint08,
    ft_numTypes       // number of types
};

// Random fill style for use with FillMatrices
enum RandStyle
{
    rs_range,
    rs_exp
};

//! Some unions to allow less typecasting in the kernel itself.
union Union32
{
    UINT32  u;
    INT32   i;
    FLOAT32 f;
};
union Union64
{
    UINT64  u;
    INT64   i;
    FLOAT64 f;
};

union FillRange
{
    UINT32  u;
    INT32   i;
    FLOAT32 f;
    FLOAT64 d;
};

//! Describes one rectangular input or output matrix for any
//! kernel launch.
struct Tile
{
    device_ptr  elements;   // pointer to the start of this submatrix global data
    UINT32      width;      // x size (in bytes) of this matrix
    UINT32      height;     // y size (in bytes) of this matrix
    UINT32      pitch;      // in bytes,if nonzero its a submatrix of a larger matrix
    UINT32      elementSize;// size in bytes of a single element
    UINT32      offsetX;    // submatrix x offset (debug info)
    UINT32      offsetY;    // submatrix y offset (debug info)
    FillRange   min;        // the minimum value used for fills
    FillRange   max;        // the maxiumum value used for fills
    FillType    fillType;   // Fill type being performed.
                            // NOTE : Since this structure is actually instantiated
                            // in the LWCA kernel creating a constructor is not
                            // possible.  Be sure to intialize this variable
                            // appropriately
    RandStyle   fillStyle;  // Fill type dependent style
};

//! We keep a global array of these for each of the Seq32 and Seq64 kernels.
//! The arrays are randomized once per frame and shared by multiple launches.
struct SeqKernelOp
{
    UINT32 OpCode;          // see CRTST_KRNL_SEQ_MTX_OPCODE in lwr_comm.h
    INT32 Operand1;         // see CRTST_KRNL_SEQ_MTX_OPERAND in lwr_comm.h
    INT32 Operand2;         // dito
    INT32 Operand3;         // dito
};

//! Kernel launch paramater struct for the Seq32 and Seq64 kernels.
struct SeqKernelParam
{
    UINT32      numOps;  // number of commands in the command sequence
    UINT32      opIdx;   // start idx in Seq32KernelOps or Seq64KernelOps
    UINT32      inc;     // increment the idx value by this amount after each operation.
    UINT32      _dummy;
    Tile        A;       // First input matrix
    Tile        B;       // Second input matrix
    Tile        C;       // Output matrix
    Tile        SMID;    // Output tile to write the Smid for each thread (debug)
};

struct GwcKernelParam
{
    UINT32      gridW;   // Width of grid to launch
    UINT32      gridH;   // Height of grid to launch
    UINT32      numOps;  // passed to StressTest kernel in launch
    UINT32      _dummy;
    Tile        A;       // ... First input matrix
    Tile        B;       // ... Second input matrix
    Tile        C;       // ... Output matrix
    Tile        SMID;    // ... Output tile to write the Smid for each thread (debug)
};

//! Parameter struct for filling surfaces on GPU
struct GPUFillParam
{
    bool        GPUFill;
    UINT32      seed;

    GPUFillParam()
    :   GPUFill(false),
        seed(0)
    {}
};

//! The arrays are randomized once per frame and shared by multiple launches.
struct AtomKernelOp
{
    UINT32  OpCode;        // see CRTST_KRNL_ATOM_OPCODE in lwr_comm.h
    UINT32  Arg0;          // see CRTST_KRNL_ATOM_ARG0 in lwr_comm.h
    float   Arg1;          // see CRTST_KRNL_ATOM_ARG1 in lwr_comm.h
    // If true, threads that collide on a global/shared memory address
    // atomic operation will be in the same warp.
    // If false, threads will be in different warps (trickier synch problem)
    UINT32  UseSameWarp;   // see CRTST_KRNL_ATOM_SAME_WARP in lwr_comm.h
};

//! Kernel launch paramater struct for the Atomic kernel.
struct AtomKernelParam
{
    UINT32      numOps;  // number of commands in the command sequence
    UINT32      opIdx;   // start idx in Seq32KernelOps or Seq64KernelOps
    Tile        C;       // Output matrix
};

const UINT32 LwdaRandomTexSize = BLOCK_SIZE;
const UINT32 Seq32OpsSize  = 200;
const UINT32 Seq64OpsSize  = 100;
const UINT32 SeqIntOpsSize = 100;
const UINT32 TexOpsSize    = 50;
const UINT32 SurfOpsSize   = 40;
const UINT32 AtomOpsSize   = 50;
const UINT32 ConstValuesSize  = 10;
