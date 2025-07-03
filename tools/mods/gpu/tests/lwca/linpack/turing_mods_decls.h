#ifdef LW_MODS
constexpr void* turing_hgemm_256x128_mods_nt = nullptr;
constexpr void* turing_hgemm_128x128_mods_nt = nullptr;
constexpr void* turing_sgemm_128x128_mods_nt = nullptr;
constexpr void* turing_igemm_int8_128x128_ldg4_mods_nt = nullptr;
constexpr void* turing_dgemm_128x64_mods_nt = nullptr;
#endif

#define  turing_hgemm_256x128_mods_nt_params   {\
    /* version             */ 1,\
    /* name                */ "turing_hgemm_256x128_mods_nt",\
    /* kernel              */ (void*)turing_hgemm_256x128_mods_nt,\
    /* chipFamily          */ GEMM_CHIP_TURING,\
    /* gemmType            */ HGEMM,\
    /* typeA               */ R_16F,\
    /* typeAm              */ R_16F,\
    /* packCountA             1, */\
    /* typeB               */ R_16F,\
    /* packCountB             1, */\
    /* typeC               */ R_16F,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_16F,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 0,\
    /* log2ElementsPerLdgB */ 0,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 125,\
    /* usedRegisters          125, */\
    /* sharedMemSize       */ 12288,  /* 12.000 KB */\
    /* elementRowsPerCta   */ 256,\
    /* elementRowsPerWarp     128, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     32, */\
    /* threadsPerCta       */ 256,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 8,\
    /* shiftFastA          */ 1,\
    /* multiplierSlowA     */ 16,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 1,\
    /* multiplierSlowB     */ 16,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 0,\
    /* multiplierSlowAm    */ 0,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 8,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_PREAMPERE_G\
}

#define  turing_hgemm_128x128_mods_nt_params   {\
    /* version             */ 1,\
    /* name                */ "turing_hgemm_128x128_mods_nt",\
    /* kernel              */ (void*)turing_hgemm_128x128_mods_nt,\
    /* chipFamily          */ GEMM_CHIP_TURING,\
    /* gemmType            */ HGEMM,\
    /* typeA               */ R_16F,\
    /* typeAm              */ R_16F,\
    /* packCountA             1, */\
    /* typeB               */ R_16F,\
    /* packCountB             1, */\
    /* typeC               */ R_16F,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_16F,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 0,\
    /* log2ElementsPerLdgB */ 0,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 125,\
    /* usedRegisters          125, */\
    /* sharedMemSize       */ 8192,  /* 8.000 KB */\
    /* elementRowsPerCta   */ 128,\
    /* elementRowsPerWarp     128, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     32, */\
    /* threadsPerCta       */ 128,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 2,\
    /* shiftFastA          */ 1,\
    /* multiplierSlowA     */ 4,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 1,\
    /* multiplierSlowB     */ 4,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 0,\
    /* multiplierSlowAm    */ 0,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 8,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_PREAMPERE_G\
}

#define  turing_sgemm_128x128_mods_nt_params   {\
    /* version             */ 1,\
    /* name                */ "turing_sgemm_128x128_mods_nt",\
    /* kernel              */ (void*)turing_sgemm_128x128_mods_nt,\
    /* chipFamily          */ GEMM_CHIP_TURING,\
    /* gemmType            */ SGEMM,\
    /* typeA               */ R_32F,\
    /* typeAm              */ R_32F,\
    /* packCountA             1, */\
    /* typeB               */ R_32F,\
    /* packCountB             1, */\
    /* typeC               */ R_32F,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_32F,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 0,\
    /* log2ElementsPerLdgB */ 0,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 116,\
    /* usedRegisters          113, */\
    /* sharedMemSize       */ 16384,  /* 16.000 KB */\
    /* elementRowsPerCta   */ 128,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     32, */\
    /* threadsPerCta       */ 256,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 4,\
    /* shiftFastA          */ 2,\
    /* multiplierSlowA     */ 16,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 2,\
    /* multiplierSlowB     */ 16,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 0,\
    /* multiplierSlowAm    */ 0,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 8,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_PREAMPERE_G\
}

#define  turing_igemm_int8_128x128_ldg4_mods_nt_params   {\
    /* version             */ 1,\
    /* name                */ "turing_igemm_int8_128x128_ldg4_mods_nt",\
    /* kernel              */ (void*)turing_igemm_int8_128x128_ldg4_mods_nt,\
    /* chipFamily          */ GEMM_CHIP_TURING,\
    /* gemmType            */ IGEMM,\
    /* typeA               */ R_8I,\
    /* typeAm              */ R_32I,\
    /* packCountA             1, */\
    /* typeB               */ R_8I,\
    /* packCountB             1, */\
    /* typeC               */ R_32I,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_32F,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 2,\
    /* log2ElementsPerLdgB */ 2,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 124,\
    /* usedRegisters          123, */\
    /* sharedMemSize       */ 16384,  /* 16.000 KB */\
    /* elementRowsPerCta   */ 128,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     32, */\
    /* threadsPerCta       */ 256,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 4,\
    /* shiftFastA          */ 0,\
    /* multiplierSlowA     */ 13,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 0,\
    /* multiplierSlowB     */ 13,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 0,\
    /* multiplierSlowAm    */ 0,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 32,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_PREAMPERE_G\
}

#define  turing_dgemm_128x64_mods_nt_params   {\
    /* version             */ 1,\
    /* name                */ "turing_dgemm_128x64_mods_nt",\
    /* kernel              */ (void*)turing_dgemm_128x64_mods_nt,\
    /* chipFamily          */ GEMM_CHIP_TURING,\
    /* gemmType            */ DGEMM,\
    /* typeA               */ R_64F,\
    /* typeAm              */ R_64F,\
    /* packCountA             1, */\
    /* typeB               */ R_64F,\
    /* packCountB             1, */\
    /* typeC               */ R_64F,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_64F,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 0,\
    /* log2ElementsPerLdgB */ 0,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 232,\
    /* usedRegisters          229, */\
    /* sharedMemSize       */ 24576,  /* 24.000 KB */\
    /* elementRowsPerCta   */ 128,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 64,\
    /* elementColsPerWarp     32, */\
    /* threadsPerCta       */ 128,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 4,\
    /* shiftFastA          */ 3,\
    /* multiplierSlowA     */ 32,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 3,\
    /* multiplierSlowB     */ 32,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 0,\
    /* multiplierSlowAm    */ 0,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 8,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_PREAMPERE_G\
}

#ifndef LW_MODS
#ifdef LWBLAS_USE_CNP
__constant__
#else
extern const
#endif
ShaderParams turing_mods_params[] = {
    turing_hgemm_256x128_mods_nt_params,
    turing_hgemm_128x128_mods_nt_params,
    turing_sgemm_128x128_mods_nt_params,
    turing_igemm_int8_128x128_ldg4_mods_nt_params,
    turing_dgemm_128x64_mods_nt_params,
};
#endif