/****************************************************************************\

Copyright (c) 2011-2017 NVIDIA CORPORATION.  All Rights Reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto.  Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.

\****************************************************************************/

#pragma once

// If these enums change, then class ShaderParams in maxwell_gemm_common.py,
// needs to change as well.


typedef enum chipFamilyType_t {
    GEMM_CHIP_MAXWELL = 0,
    GEMM_CHIP_PASCAL = 0,
    GEMM_CHIP_VOLTA = 1,
    GEMM_CHIP_TURING = 2,
    GEMM_CHIP_AMPERE = 3,
    GEMM_CHIP_HOPPER = 4,
    CHIPFAMILY_COUNT,
#ifdef DEPRECATED_CHIP_FAMILY
    MAXWELL = 0,
    PASCAL = 0,
    VOLTA = 1,
    TURING = 2,
    AMPERE = 3,
    HOPPER = 4,
#endif
} chipFamilyType_t;


typedef enum gemmType_t {   // accumulator type
    CGEMM,       // Complex single-precision
    DGEMM,       // Double-precision
    GCGEMM,      // Gaussian complex single-precision
    GZGEMM,      // Gaussian complex double-precision
    HGEMM,       // Half-precision
    IGEMM,       // Integer
    SGEMM,       // Single-precision
    ZGEMM,       // Complex double-precision
    H884GEMM,    // Half-precision using HMMA instruction
    H1688GEMM,   // Half-precision using super-HMMA instruction
    H1688CUDNN,  // Half-precision cudnn using super-HMMA instruction
    H16816GEMM,  // Half-precision using 16xHMMA instruction
    H16816CUDNN, // Half-precision cudnn using 16xHMMA instruction
    S884GEMM,    // Single-precision using HMMA instruction
    S1688GEMM,   // Single-precision using super-HMMA instruction
    S1688CUDNN,  // Single-precision using super-HMMA instruction
    S16816GEMM,  // Single-precision using 16xHMMA instruction
    S16832GEMM,  // Single-precision using 32xSparseHMMA instruction
    S16816CUDNN, // Single-precision using 16xHMMA instruction
    I8816GEMM,   // Int8-precision using IMMA instruction
    I16832GEMM,   // Int8-precision using 32xIMMA instruction
    I16864GEMM,   // Int8-precision using 64xSparseIMMA instruction
    I16832CUDNN,   // Int8-precision using IMMA instruction
    I8816CUDNN,  // Int8-precision using IMMA instruction
    GEMM_COUNT,  // only the types above are used in gemm
    SCUDNN,
    SCUDNN_WINOGRAD,
    HCUDNN,
    HCUDNN_WINOGRAD,
    ICUDNN,
    H884CUDNN,
    S884CUDNN,
    S1684TF32GEMM, //1684.TF32
    S1684TF32CUDNN,
} gemmType_t;


// Enum values match SW's lwcaDataType (cublasDataType_t) taken from //sw/gpgpu/lwca/tools/lwcart/library_types.h
typedef enum elementDataType_t {
    R_16F =  2, // 16 bit real
    C_16F =  6, // 16 bit complex
    R_32F =  0, // 32 bit real
    C_32F =  4, // 32 bit complex
    R_64F =  1, // 64 bit real
    C_64F =  5, // 64 bit complex
    R_8I  =  3, // 8 bit real as a signed integer
    C_8I  =  7, // 8 bit complex as a pair of signed integers
    R_8U  =  8, // 8 bit real as a signed integer
    C_8U  =  9, // 8 bit complex as a pair of signed integers
    R_32I = 10, // real as a signed int
    C_32I = 11, // complex as a pair of signed int numbers
    R_32U = 12, // real as a unsigned int
    C_32U = 13, // complex as a pair of unsigned int numbers
    R_64I = 14, // real as a signed int64
    C_64I = 15, // complex as a pair of signed int64 numbers
    R_64U = 16, // real as a unsigned int64
    C_64U = 17, // complex as a pair of unsigned int64 numbers
    BF16_16F    = 18, // bf16 16 bit half
    C_BF16_16F  = 19, // complex bf16 16 bit half
    TF32_19F   = 20, // tf32 19 bit float
    C_TF32_19F = 21, // complex tf32 19 bit float
    R_16I = 22, // 16 bit real as a signed integer
    R_16U = 23, // 16 bit real as a unsigned integer
    R_4I  = 24, // 4 bit real as a signed integer
    R_4U  = 25, // 4 bit real as a unsigned integer
    R_2I  = 26, // 2 bit real as a signed integer
    R_2U  = 27, // 2 bit real as a unsigned integer
    R_1U  = 28, // 1 bit real as a unsigned integer
    GEMM_DATATYPE_COUNT
} elementDataType_t;

inline int32_t elementDataSize(const elementDataType_t e) {
    switch (e) {
    case R_16F : return  2;
    case C_16F : return  4;
    case R_32F : return  4;
    case C_32F : return  8;
    case R_64F : return  8;
    case C_64F : return 16;
    case R_8I  : return  1;
    case C_8I  : return  2;
    case R_8U  : return  1;
    case C_8U  : return  2;
    case R_32I : return  4;
    case C_32I : return  8;
    case R_32U : return  4;
    case C_32U : return  8;
    case R_64I : return  8;
    case C_64I : return  16;
    case R_64U : return  8;
    case C_64U : return  16;
    case BF16_16F    : return 2;
    case C_BF16_16F  : return 4;
    case TF32_19F   : return 4;
    case C_TF32_19F : return 8;
    case R_16I : return  2;
    case R_16U : return  2;
    case R_4I  : return  1;
    case R_4U  : return  1;
    case R_2I  : return  1;
    case R_2U  : return  1;
    case R_1U  : return  1;
    default    : assert(false && "Unknown elementDataType_t value"); return 0;
    } // switch
} // elementDataSize

// Enum values match SW's cublasOperation_t, with CONJUGATE_NON_TRANSPOSED added
typedef enum layoutType_t {
    N = 0,
    NON_TRANSPOSED = 0,
    T = 1,
    TRANSPOSED = 1,
    C = 2,
    CONJUGATE_TRANSPOSED = 2,
    //CONJUGATE_NON_TRANSPOSED = 3,   // Not currently used, no assigned letter
    TRANSPOSITION_COUNT
} layoutType_t;


// Enum values match SW's cublasFillMode_t, with RECTANGLE added
typedef enum shapeTypeC_t {
    LOWER,
    UPPER,
    RECT,
    SHAPE_COUNT
} shapeTypeC_t;


#define MIN_TILE           32
#define MAX_TILE           256
#define MAX_CTA_TILE_SIZE  (log2(MAX_TILE / MIN_TILE) + 1)  // 32 is smallest tile size, 256 is max, in powers of 2
#define CTA_TILE_INDEX(mn) (log2(mn / MIN_TILE))

#define MAX_LDG_ELEMENTS_SIZE (log2(8) + 1)  // 1 is smallest ldg size, 8 is max

typedef enum edgeType_t {
    CUDNN_NONE,
    CUDNN_INTERIOR,
    CUDNN_SMALL,
    CUDNN_MEDIUM,
    CUDNN_LARGE
} edgeType_t;

typedef enum cuDnnLayout_t {
    CUDNN_LAYOUT_NONE,
    NCHW,
    NHWC
} cuDnnLayout_t;

/*
// Defined in //sw/gpgpu/cublas/src/cublas_api.h
typedef enum {
    CUBLAS_OP_N=0,
    CUBLAS_OP_T=1,
    CUBLAS_OP_C=2
} cublasOperation_t;

// Defined in //sw/gpgpu/cublas/src/cublas_api.h
typedef enum {
    CUBLAS_FILL_MODE_LOWER=0,
    CUBLAS_FILL_MODE_UPPER=1
} cublasFillMode_t;

// Defined in //sw/gpgpu/cublas/src/cublas_api.h
typedef enum {
    CUBLAS_DIAG_NON_UNIT=0,
    CUBLAS_DIAG_UNIT=1
} cublasDiagType_t;
*/


/*  GEMM modes of operation.  Mode is a combination of mode flags.
    Some flags are mutually exclusive: strided and array-based batching,
    and atomic vs spin-lock accumulation.  TODO: can't you do both, to get


    mode=0x0000: Normal GEMM

    mode&0x0001: Batched GEMM, contiguous matrices.  Starting addresses are
                 Base{A,B,C} + CTAid.Z * MatrixStride{A,B,C}.

    mode&0x0002: Batched GEMM, Base{A,B,C} are pointers to vectors of addresses
                 in global memory, indexed by CTAid.Z.

    mode&0x0004: Split-k-across-CTAs GEMM, chunks indexed by CTAid.Z.  This
                 flag automatically accounts for beta if "deterministic
                 accumulation" flag set.

    mode&0x0008: Split-k-across-CTAS deterministic accumulation of C via
                 spin-waits on counters in client-zeroed "Sync" memory.

    mode&0x0010: Split-k-across-CTAS non-deterministic accumulation via
                 RED.E.ADD (Requires beta=1, client zeros or prescales C.).
                 Support only for SGEMM.  Will accumulate deterministically,
                 but avoid read/modify/write all the way to SM, if used in
                 conjunction with SPINLOOP_RED?  I suspect.

    mode&0x0020: Split-k-across-CTAS, deterministic accumulation in another
                 kernel
    mode&0x0f00: 4-bit field for log2(groupCols) for CTA swizzling.  See
                 cta_swizzle.hpp for more details.

    mode&0x1000: Enable serpentine rasterization from one group of CTA
                 columns to the next, instead of always top to bottom.

    mode&0x10000: Manufacturing MODS only.  Map 3D CTA id to linearized CTA
                  index cta.Z * grid.Y * grid.X + cta.Y * grid.X + cta.X, then
                  index off Sync pointer and write a 16-bit virtual SM id.
                  *** Must not be used with SPINLOOP_RED! ***

*/

typedef enum gemmMode_t {
    BATCHED_CONTIG     = 0x0001,
    BATCHED_ARRAY      = 0x0002,
    SPLIT_K            = 0x0004,
    SPINLOOP_RED       = 0x0008,
    ATOMIC_RED         = 0x0010,
    SPLIT_NO_REDUCTION = 0x0020,
    // unused          = 0x0040,
    // unused          = 0x0080,

    // 4-bit log2GroupCols field starting at bit 8 for CTA id remapping
    // of a wave to a more squarish shape.
    CTA_SWIZZLE_MASK  = 0x0f00,
    CTA_SWIZZLE_START = 8,
    CTA_SWIZZLE_SERPENTINE = 0x1000,
    // unused          = 0x2000,
    // unused          = 0x4000,
    // unused          = 0x8000,

    MODS_MAP_CTA_TO_SM = 0x10000,

} gemmMode_t;

typedef enum abiVersion_t {
    ABI_AMPERE_G,
    ABI_AMPERE_WINOG,
    ABI_AMPERE_WGRAD,
    ABI_AMPERE_IGINTERIOR,
    ABI_AMPERE_IGSMALL,
    ABI_AMPERE_IGMEDIUM,
    ABI_AMPERE_IGLARGE,
    ABI_PREAMPERE_G,
    ABI_PREAMPERE_WINOG,
    ABI_PREAMPERE_WGRAD,
    ABI_PREAMPERE_IGINTERIOR,
    ABI_PREAMPERE_IGSMALL,
    ABI_PREAMPERE_IGMEDIUM,
    ABI_PREAMPERE_IGLARGE,
    ABI_PREAMPERE_1STLAYER_FWD,
    ABI_PREAMPERE_1STLAYER_IMMA_FWD,
    ABI_PREAMPERE_1STLAYER_WGARD,
    ABI_PREAMPERE_GEMM_AS_CON1X1,
    ABI_PREAMPERE_DIRECT_GCONV

} abiVersion_t;

struct ShaderParams {
    int version;

    //  long unique string identifier of the kernel
    const char *name;

    const void *kernel;
    /*const void (*kernel) (
        ptr64_t baseA, ptr64_t baseB, ptr64_t baseC,
        uint64_t incFastA, uint64_t incFastB,
        int64_t incSlowA, int64_t incSlowB,
        uint64_t matrixStrideA, uint64_t matrixStrideB, uint64_t matrixStrideC,
        uint32_t strideA, uint32_t strideB, uint32_t strideC,
        uint32_t countM, uint32_t countN, uint32_t countK,
        ptr64_t sync, uint32_t chunkK, uint32_t mode,
        ptr64_t alphaRef, ptr64_t betaRef,
        T_ACC alpha, T_ACC beta, int alphaBetaByRef);*/

    chipFamilyType_t    chipFamily;
    gemmType_t          gemmType;
    elementDataType_t   typeA, typeAm, typeB, typeC, typeEpilog;
    shapeTypeC_t        shapeC;
    layoutType_t        layoutA, layoutB;
    uint8_t             log2ElementsPerLdgA, log2ElementsPerLdgB; // No elementsPerLdgC, yet
    bool                reLuAndBias;
    bool                isReleaseKernel;

    // Number of registers allocated, but there may be free holes
    unsigned      numRegisters;

    // Shared memory per CTA in bytes, ceilinged to multiple of 256B
    unsigned      sharedMemSize;

    unsigned      elementRowsPerCta;
    unsigned      elementColsPerCta;
    unsigned      threadsPerCta;

    // Does kernel handle arbitrary m, n dimensions
    bool          raggedMn;

    // A single warp's stride in the k dimension
    unsigned      warpStrideK;

    // Params for computing IncFastA, IncSlowA
    unsigned      shiftFastA;
    int           multiplierSlowA;
    int           offsetSlowA;

    // Params for computing IncFastB, IncSlowB
    unsigned      shiftFastB;
    int           multiplierSlowB;
    int           offsetSlowB;

    // Params for computing IncFastAm, IncSlowAm
    unsigned      shiftFastAm;
    int           multiplierSlowAm;
    int           offsetSlowAm;
    // kBlock size
    unsigned      kBlock;

    // CUDNN properties
    edgeType_t    cuDnnEdges;
    cuDnnLayout_t cuDnnLayout;
    bool          cuDnnStridedB;
    bool          cuDnnSplitK;
    bool          cuDnnDgrad;
    bool          cuDnnWgrad;

    // Slicing properties
    int           sliceRows;
    int           sliceCols;

    // kernel ABI version
    abiVersion_t  abiVersion;

};
