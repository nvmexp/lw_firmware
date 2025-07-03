#ifdef LW_MODS
constexpr void* ampere_h16816gemm_256x128_ldg8_mods_stages_64x3_nt = nullptr;
constexpr void* ampere_h16816gemm_128x128_ldg8_mods_stages_64x3_nt = nullptr;
constexpr void* ampere_fp16_s16816gemm_fp16_256x128_ldg8_f2f_mods_stages_64x3_nt = nullptr;
constexpr void* ampere_fp16_s16816gemm_fp16_128x128_ldg8_f2f_mods_stages_64x3_nt = nullptr;
constexpr void* ampere_int8_i16832gemm_int8_256x128_ldg16_relu_mods_stages_64x3_nt = nullptr;
constexpr void* ampere_int8_i16832gemm_int8_128x128_ldg16_relu_mods_stages_64x3_nt = nullptr;
constexpr void* ampere_int32_i16832gemm_int8_256x128_ldg16_mods_stages_64x3_nt = nullptr;
constexpr void* ampere_int32_i16832gemm_int8_128x128_ldg16_mods_stages_64x3_nt = nullptr;
#endif

#define  ampere_h1688gemm_256x128_ldg8_mods_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_h1688gemm_256x128_ldg8_mods_nt",\
    /* kernel              */ (void*)ampere_h1688gemm_256x128_ldg8_mods_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ H1688GEMM,\
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
    /* log2ElementsPerLdgA */ 3,\
    /* log2ElementsPerLdgB */ 3,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 185,\
    /* usedRegisters          185, */\
    /* sharedMemSize       */ 65536,  /* 64.000 KB */\
    /* elementRowsPerCta   */ 256,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 256,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 8,\
    /* shiftFastA          */ 6,\
    /* multiplierSlowA     */ 64,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 6,\
    /* multiplierSlowB     */ 64,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 6,\
    /* multiplierSlowAm    */ 64,\
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
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_int8_h1688gemm_256x128_ldg8_mods_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_int8_h1688gemm_256x128_ldg8_mods_nt",\
    /* kernel              */ (void*)ampere_int8_h1688gemm_256x128_ldg8_mods_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ H1688GEMM,\
    /* typeA               */ R_16F,\
    /* typeAm              */ R_16F,\
    /* packCountA             1, */\
    /* typeB               */ R_16F,\
    /* packCountB             1, */\
    /* typeC               */ R_8I,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_16F,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 3,\
    /* log2ElementsPerLdgB */ 3,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 200,\
    /* usedRegisters          199, */\
    /* sharedMemSize       */ 65536,  /* 64.000 KB */\
    /* elementRowsPerCta   */ 256,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 256,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 8,\
    /* shiftFastA          */ 6,\
    /* multiplierSlowA     */ 64,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 6,\
    /* multiplierSlowB     */ 64,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 6,\
    /* multiplierSlowAm    */ 64,\
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
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_h1688gemm_128x128_ldg8_mods_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_h1688gemm_128x128_ldg8_mods_nt",\
    /* kernel              */ (void*)ampere_h1688gemm_128x128_ldg8_mods_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ H1688GEMM,\
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
    /* log2ElementsPerLdgA */ 3,\
    /* log2ElementsPerLdgB */ 3,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 185,\
    /* usedRegisters          185, */\
    /* sharedMemSize       */ 32768,  /* 32.000 KB */\
    /* elementRowsPerCta   */ 128,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 128,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 4,\
    /* shiftFastA          */ 5,\
    /* multiplierSlowA     */ 32,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 5,\
    /* multiplierSlowB     */ 32,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 5,\
    /* multiplierSlowAm    */ 32,\
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
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_int8_h1688gemm_128x128_ldg8_mods_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_int8_h1688gemm_128x128_ldg8_mods_nt",\
    /* kernel              */ (void*)ampere_int8_h1688gemm_128x128_ldg8_mods_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ H1688GEMM,\
    /* typeA               */ R_16F,\
    /* typeAm              */ R_16F,\
    /* packCountA             1, */\
    /* typeB               */ R_16F,\
    /* packCountB             1, */\
    /* typeC               */ R_8I,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_16F,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 3,\
    /* log2ElementsPerLdgB */ 3,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 200,\
    /* usedRegisters          199, */\
    /* sharedMemSize       */ 32768,  /* 32.000 KB */\
    /* elementRowsPerCta   */ 128,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 128,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 4,\
    /* shiftFastA          */ 5,\
    /* multiplierSlowA     */ 32,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 5,\
    /* multiplierSlowB     */ 32,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 5,\
    /* multiplierSlowAm    */ 32,\
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
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_h16816gemm_128x128_ldg8_mods_stages_64x3_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_h16816gemm_128x128_ldg8_mods_stages_64x3_nt",\
    /* kernel              */ (void*)ampere_h16816gemm_128x128_ldg8_mods_stages_64x3_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ H16816GEMM,\
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
    /* log2ElementsPerLdgA */ 3,\
    /* log2ElementsPerLdgB */ 3,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 164,\
    /* usedRegisters          161, */\
    /* sharedMemSize       */ 98304,  /* 96.000 KB */\
    /* elementRowsPerCta   */ 128,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 128,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 4,\
    /* shiftFastA          */ 5,\
    /* multiplierSlowA     */ 32,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 5,\
    /* multiplierSlowB     */ 32,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 5,\
    /* multiplierSlowAm    */ 32,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 64,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_h16816gemm_256x128_ldg8_mods_stages_64x3_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_h16816gemm_256x128_ldg8_mods_stages_64x3_nt",\
    /* kernel              */ (void*)ampere_h16816gemm_256x128_ldg8_mods_stages_64x3_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ H16816GEMM,\
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
    /* log2ElementsPerLdgA */ 3,\
    /* log2ElementsPerLdgB */ 3,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 156,\
    /* usedRegisters          153, */\
    /* sharedMemSize       */ 147456,  /* 144.000 KB */\
    /* elementRowsPerCta   */ 256,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 256,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 8,\
    /* shiftFastA          */ 6,\
    /* multiplierSlowA     */ 64,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 6,\
    /* multiplierSlowB     */ 64,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 6,\
    /* multiplierSlowAm    */ 64,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 64,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_fp16_s1688gemm_fp16_128x128_ldg8_f2f_mods_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_fp16_s1688gemm_fp16_128x128_ldg8_f2f_mods_nt",\
    /* kernel              */ (void*)ampere_fp16_s1688gemm_fp16_128x128_ldg8_f2f_mods_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ S1688GEMM,\
    /* typeA               */ R_16F,\
    /* typeAm              */ R_32F,\
    /* packCountA             1, */\
    /* typeB               */ R_16F,\
    /* packCountB             1, */\
    /* typeC               */ R_16F,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_32F,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 3,\
    /* log2ElementsPerLdgB */ 3,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 232,\
    /* usedRegisters          231, */\
    /* sharedMemSize       */ 32768,  /* 32.000 KB */\
    /* elementRowsPerCta   */ 128,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 128,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 4,\
    /* shiftFastA          */ 5,\
    /* multiplierSlowA     */ 32,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 5,\
    /* multiplierSlowB     */ 32,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 5,\
    /* multiplierSlowAm    */ 32,\
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
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_fp16_s1688gemm_fp16_256x128_ldg8_f2f_mods_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_fp16_s1688gemm_fp16_256x128_ldg8_f2f_mods_nt",\
    /* kernel              */ (void*)ampere_fp16_s1688gemm_fp16_256x128_ldg8_f2f_mods_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ S1688GEMM,\
    /* typeA               */ R_16F,\
    /* typeAm              */ R_32F,\
    /* packCountA             1, */\
    /* typeB               */ R_16F,\
    /* packCountB             1, */\
    /* typeC               */ R_16F,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_32F,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 3,\
    /* log2ElementsPerLdgB */ 3,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 232,\
    /* usedRegisters          231, */\
    /* sharedMemSize       */ 65536,  /* 64.000 KB */\
    /* elementRowsPerCta   */ 256,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 256,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 8,\
    /* shiftFastA          */ 6,\
    /* multiplierSlowA     */ 64,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 6,\
    /* multiplierSlowB     */ 64,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 6,\
    /* multiplierSlowAm    */ 64,\
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
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_fp16_s16816gemm_fp16_128x128_ldg8_f2f_mods_stages_64x3_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_fp16_s16816gemm_fp16_128x128_ldg8_f2f_mods_stages_64x3_nt",\
    /* kernel              */ (void*)ampere_fp16_s16816gemm_fp16_128x128_ldg8_f2f_mods_stages_64x3_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ S16816GEMM,\
    /* typeA               */ R_16F,\
    /* typeAm              */ R_32F,\
    /* packCountA             1, */\
    /* typeB               */ R_16F,\
    /* packCountB             1, */\
    /* typeC               */ R_16F,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_32F,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 3,\
    /* log2ElementsPerLdgB */ 3,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 228,\
    /* usedRegisters          225, */\
    /* sharedMemSize       */ 98304,  /* 96.000 KB */\
    /* elementRowsPerCta   */ 128,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 128,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 4,\
    /* shiftFastA          */ 5,\
    /* multiplierSlowA     */ 32,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 5,\
    /* multiplierSlowB     */ 32,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 5,\
    /* multiplierSlowAm    */ 32,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 64,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_fp16_s16816gemm_fp16_256x128_ldg8_f2f_mods_stages_64x3_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_fp16_s16816gemm_fp16_256x128_ldg8_f2f_mods_stages_64x3_nt",\
    /* kernel              */ (void*)ampere_fp16_s16816gemm_fp16_256x128_ldg8_f2f_mods_stages_64x3_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ S16816GEMM,\
    /* typeA               */ R_16F,\
    /* typeAm              */ R_32F,\
    /* packCountA             1, */\
    /* typeB               */ R_16F,\
    /* packCountB             1, */\
    /* typeC               */ R_16F,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_32F,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 3,\
    /* log2ElementsPerLdgB */ 3,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 220,\
    /* usedRegisters          217, */\
    /* sharedMemSize       */ 147456,  /* 144.000 KB */\
    /* elementRowsPerCta   */ 256,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 256,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 8,\
    /* shiftFastA          */ 6,\
    /* multiplierSlowA     */ 64,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 6,\
    /* multiplierSlowB     */ 64,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 6,\
    /* multiplierSlowAm    */ 64,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 64,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_int8_i8816gemm_int8_128x128_ldg16_relu_mods_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_int8_i8816gemm_int8_128x128_ldg16_relu_mods_nt",\
    /* kernel              */ (void*)ampere_int8_i8816gemm_int8_128x128_ldg16_relu_mods_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ I8816GEMM,\
    /* typeA               */ R_8I,\
    /* typeAm              */ R_32I,\
    /* packCountA             1, */\
    /* typeB               */ R_8I,\
    /* packCountB             1, */\
    /* typeC               */ R_8I,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_32F,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 4,\
    /* log2ElementsPerLdgB */ 4,\
    /* reLuAndBias         */ 1,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 251,\
    /* usedRegisters          251, */\
    /* sharedMemSize       */ 32768,  /* 32.000 KB */\
    /* elementRowsPerCta   */ 128,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 128,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 4,\
    /* shiftFastA          */ 0,\
    /* multiplierSlowA     */ 1,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 0,\
    /* multiplierSlowB     */ 1,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 0,\
    /* multiplierSlowAm    */ 1,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 64,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_int32_i8816gemm_int8_128x128_ldg16_mods_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_int32_i8816gemm_int8_128x128_ldg16_mods_nt",\
    /* kernel              */ (void*)ampere_int32_i8816gemm_int8_128x128_ldg16_mods_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ I8816GEMM,\
    /* typeA               */ R_8I,\
    /* typeAm              */ R_32I,\
    /* packCountA             1, */\
    /* typeB               */ R_8I,\
    /* packCountB             1, */\
    /* typeC               */ R_32I,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_32I,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 4,\
    /* log2ElementsPerLdgB */ 4,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 220,\
    /* usedRegisters          220, */\
    /* sharedMemSize       */ 32768,  /* 32.000 KB */\
    /* elementRowsPerCta   */ 128,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 128,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 4,\
    /* shiftFastA          */ 0,\
    /* multiplierSlowA     */ 1,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 0,\
    /* multiplierSlowB     */ 1,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 0,\
    /* multiplierSlowAm    */ 1,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 64,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_int8_i8816gemm_int8_256x128_ldg16_relu_mods_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_int8_i8816gemm_int8_256x128_ldg16_relu_mods_nt",\
    /* kernel              */ (void*)ampere_int8_i8816gemm_int8_256x128_ldg16_relu_mods_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ I8816GEMM,\
    /* typeA               */ R_8I,\
    /* typeAm              */ R_32I,\
    /* packCountA             1, */\
    /* typeB               */ R_8I,\
    /* packCountB             1, */\
    /* typeC               */ R_8I,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_32F,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 4,\
    /* log2ElementsPerLdgB */ 4,\
    /* reLuAndBias         */ 1,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 251,\
    /* usedRegisters          251, */\
    /* sharedMemSize       */ 49152,  /* 48.000 KB */\
    /* elementRowsPerCta   */ 256,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 256,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 8,\
    /* shiftFastA          */ 0,\
    /* multiplierSlowA     */ 1,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 0,\
    /* multiplierSlowB     */ 1,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 0,\
    /* multiplierSlowAm    */ 1,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 64,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_int32_i8816gemm_int8_256x128_ldg16_mods_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_int32_i8816gemm_int8_256x128_ldg16_mods_nt",\
    /* kernel              */ (void*)ampere_int32_i8816gemm_int8_256x128_ldg16_mods_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ I8816GEMM,\
    /* typeA               */ R_8I,\
    /* typeAm              */ R_32I,\
    /* packCountA             1, */\
    /* typeB               */ R_8I,\
    /* packCountB             1, */\
    /* typeC               */ R_32I,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_32I,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 4,\
    /* log2ElementsPerLdgB */ 4,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 217,\
    /* usedRegisters          217, */\
    /* sharedMemSize       */ 49152,  /* 48.000 KB */\
    /* elementRowsPerCta   */ 256,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 256,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 8,\
    /* shiftFastA          */ 0,\
    /* multiplierSlowA     */ 1,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 0,\
    /* multiplierSlowB     */ 1,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 0,\
    /* multiplierSlowAm    */ 1,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 64,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_int8_i16832gemm_int8_128x128_ldg16_relu_mods_stages_64x3_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_int8_i16832gemm_int8_128x128_ldg16_relu_mods_stages_64x3_nt",\
    /* kernel              */ (void*)ampere_int8_i16832gemm_int8_128x128_ldg16_relu_mods_stages_64x3_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ I16832GEMM,\
    /* typeA               */ R_8I,\
    /* typeAm              */ R_32I,\
    /* packCountA             1, */\
    /* typeB               */ R_8I,\
    /* packCountB             1, */\
    /* typeC               */ R_8I,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_32F,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 4,\
    /* log2ElementsPerLdgB */ 4,\
    /* reLuAndBias         */ 1,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 253,\
    /* usedRegisters          253, */\
    /* sharedMemSize       */ 49152,  /* 48.000 KB */\
    /* elementRowsPerCta   */ 128,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 128,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 4,\
    /* shiftFastA          */ 0,\
    /* multiplierSlowA     */ 1,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 0,\
    /* multiplierSlowB     */ 1,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 0,\
    /* multiplierSlowAm    */ 1,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 64,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_int32_i16832gemm_int8_128x128_ldg16_mods_stages_64x3_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_int32_i16832gemm_int8_128x128_ldg16_mods_stages_64x3_nt",\
    /* kernel              */ (void*)ampere_int32_i16832gemm_int8_128x128_ldg16_mods_stages_64x3_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ I16832GEMM,\
    /* typeA               */ R_8I,\
    /* typeAm              */ R_32I,\
    /* packCountA             1, */\
    /* typeB               */ R_8I,\
    /* packCountB             1, */\
    /* typeC               */ R_32I,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_32I,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 4,\
    /* log2ElementsPerLdgB */ 4,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 224,\
    /* usedRegisters          221, */\
    /* sharedMemSize       */ 49152,  /* 48.000 KB */\
    /* elementRowsPerCta   */ 128,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 128,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 4,\
    /* shiftFastA          */ 0,\
    /* multiplierSlowA     */ 1,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 0,\
    /* multiplierSlowB     */ 1,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 0,\
    /* multiplierSlowAm    */ 1,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 64,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_int8_i16832gemm_int8_256x128_ldg16_relu_mods_stages_64x3_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_int8_i16832gemm_int8_256x128_ldg16_relu_mods_stages_64x3_nt",\
    /* kernel              */ (void*)ampere_int8_i16832gemm_int8_256x128_ldg16_relu_mods_stages_64x3_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ I16832GEMM,\
    /* typeA               */ R_8I,\
    /* typeAm              */ R_32I,\
    /* packCountA             1, */\
    /* typeB               */ R_8I,\
    /* packCountB             1, */\
    /* typeC               */ R_8I,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_32F,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 4,\
    /* log2ElementsPerLdgB */ 4,\
    /* reLuAndBias         */ 1,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 253,\
    /* usedRegisters          253, */\
    /* sharedMemSize       */ 73728,  /* 72.000 KB */\
    /* elementRowsPerCta   */ 256,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 256,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 8,\
    /* shiftFastA          */ 0,\
    /* multiplierSlowA     */ 1,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 0,\
    /* multiplierSlowB     */ 1,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 0,\
    /* multiplierSlowAm    */ 1,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 64,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_AMPERE_G\
}

#define  ampere_int32_i16832gemm_int8_256x128_ldg16_mods_stages_64x3_nt_params   {\
    /* version             */ 1,\
    /* name                */ "ampere_int32_i16832gemm_int8_256x128_ldg16_mods_stages_64x3_nt",\
    /* kernel              */ (void*)ampere_int32_i16832gemm_int8_256x128_ldg16_mods_stages_64x3_nt,\
    /* chipFamily          */ GEMM_CHIP_AMPERE,\
    /* gemmType            */ I16832GEMM,\
    /* typeA               */ R_8I,\
    /* typeAm              */ R_32I,\
    /* packCountA             1, */\
    /* typeB               */ R_8I,\
    /* packCountB             1, */\
    /* typeC               */ R_32I,\
    /* packCountC             1, */\
    /* typeEpilog          */ R_32I,\
    /* packCountEpilog        1, */\
    /* shapeC              */ RECT,\
    /* layoutA             */ N,\
    /* layoutB             */ T,\
    /* log2ElementsPerLdgA */ 4,\
    /* log2ElementsPerLdgB */ 4,\
    /* reLuAndBias         */ 0,\
    /* isReleaseKernel     */ 1,\
    /* numRegisters        */ 220,\
    /* usedRegisters          220, */\
    /* sharedMemSize       */ 73728,  /* 72.000 KB */\
    /* elementRowsPerCta   */ 256,\
    /* elementRowsPerWarp     64, */\
    /* elementColsperCta   */ 128,\
    /* elementColsPerWarp     64, */\
    /* threadsPerCta       */ 256,\
    /* raggedMn            */ true,\
    /* warpStrideK         */ 8,\
    /* shiftFastA          */ 0,\
    /* multiplierSlowA     */ 1,\
    /* offsetSlowA         */ 0,\
    /* shiftFastB          */ 0,\
    /* multiplierSlowB     */ 1,\
    /* offsetSlowB         */ 0,\
    /* shiftFastAm         */ 0,\
    /* multiplierSlowAm    */ 1,\
    /* offsetSlowAm        */ 0,\
    /* kBlock              */ 64,\
    /* lwDnnEdges          */ LWDNN_NONE,\
    /* lwDnnLayout         */ NCHW,\
    /* lwDnnStridedB       */ false,\
    /* lwDnnSplitK         */ false,\
    /* lwDnnDgrad          */ false,\
    /* lwDnnWgrad          */ false,\
    /* sliceRows           */ 1,\
    /* sliceCols           */ 1,\
    /* abiVersion          */ ABI_AMPERE_G\
}

#ifndef LW_MODS
#ifdef LWBLAS_USE_CNP
__constant__
#else
extern const
#endif
ShaderParams ampere_modular_mods_params[] = {
    ampere_h1688gemm_256x128_ldg8_mods_nt_params,
    ampere_int8_h1688gemm_256x128_ldg8_mods_nt_params,
    ampere_h1688gemm_128x128_ldg8_mods_nt_params,
    ampere_int8_h1688gemm_128x128_ldg8_mods_nt_params,
    ampere_h16816gemm_128x128_ldg8_mods_stages_64x3_nt_params,
    ampere_h16816gemm_256x128_ldg8_mods_stages_64x3_nt_params,
    ampere_fp16_s1688gemm_fp16_128x128_ldg8_f2f_mods_nt_params,
    ampere_fp16_s1688gemm_fp16_256x128_ldg8_f2f_mods_nt_params,
    ampere_fp16_s16816gemm_fp16_128x128_ldg8_f2f_mods_stages_64x3_nt_params,
    ampere_fp16_s16816gemm_fp16_256x128_ldg8_f2f_mods_stages_64x3_nt_params,
    ampere_int8_i8816gemm_int8_128x128_ldg16_relu_mods_nt_params,
    ampere_int32_i8816gemm_int8_128x128_ldg16_mods_nt_params,
    ampere_int8_i8816gemm_int8_256x128_ldg16_relu_mods_nt_params,
    ampere_int32_i8816gemm_int8_256x128_ldg16_mods_nt_params,
    ampere_int8_i16832gemm_int8_128x128_ldg16_relu_mods_stages_64x3_nt_params,
    ampere_int32_i16832gemm_int8_128x128_ldg16_mods_stages_64x3_nt_params,
    ampere_int8_i16832gemm_int8_256x128_ldg16_relu_mods_stages_64x3_nt_params,
    ampere_int32_i16832gemm_int8_256x128_ldg16_mods_stages_64x3_nt_params,
};
#endif

