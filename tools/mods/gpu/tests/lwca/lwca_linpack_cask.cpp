/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#include "lwda_linpack_cask.h"
#include "core/include/help.h"
#include "core/include/utility.h"
#include <numeric>

// Groupings of instructions that belong to the same CaskLinpack variant
using Instr = GpuSubdevice::Instr;
const vector<set<Instr>> LwdaLinpackCask::s_VariantTypes =
{
    {
        Instr::HMMA_884_F16_F16,
        Instr::HMMA_1688_F16_F16,
        Instr::HMMA_16816_F16_F16,
        Instr::HMMA_884_F32_F16,
        Instr::HMMA_1688_F32_F16,
        Instr::HMMA_16816_F32_F16,
        Instr::HGMMA_F32_F16,
        Instr::HGMMA_F16_F16,
    },
    {
        Instr::HMMA_1688_F32_BF16,
        Instr::HMMA_16816_F32_BF16,
        Instr::HGMMA_F32_BF16,
    },
    {
        Instr::HMMA_1684_F32_TF32,
        Instr::HMMA_1688_F32_TF32,
        Instr::HGMMA_F32_TF32,
    },
    {
        Instr::HMMA_16832SP_F32_F16,
        Instr::HGMMA_SP_F32_F16,
        Instr::HGMMA_SP_F16_F16,
    },
    {
        Instr::IMMA_8816_S32_S8,
        Instr::IMMA_16832_S32_S8,
        Instr::IGMMA_S32_S8,
    },
    {
        Instr::IMMA_16864SP_S32_S8,
        Instr::IGMMA_SP_S32_S8,
    },
    {
        Instr::DMMA_884,
        Instr::DMMA_16816,
    },
};

// Aliases for parts of the kernel name
const vector<pair<const char*, const char*>> LwdaLinpackCask::s_AliasMap =
{
    // E8M7 and E8M10 were renamed after GA100 bringup
    {"fe8m7",  "bf16"},
    {"fe8m10", "tf32"},
};

// Prefixes used to locate kernels when the GPU family name is unknown
// Search the prefixes in order
const map<Lwca::Capability::Enum, vector<const char*>> LwdaLinpackCask::s_FamilyMap =
{
   {Lwca::Capability::SM_80, {"ampere_", "sm80_"}},
   {Lwca::Capability::SM_86, {"ampere_", "sm80_"}},
   {Lwca::Capability::SM_87, {"ampere_", "sm80_"}},
   {Lwca::Capability::SM_89, {"ampere_", "sm80_"}},
   {Lwca::Capability::SM_90, {"sm90_",   "sm80_"}},
};

// Prefixes used to whitelist kernels as supported by the given SM version
const map<Lwca::Capability::Enum, vector<const char*>> LwdaLinpackCask::s_SupportedMap =
{
   {Lwca::Capability::SM_80, {"lwtlass_simt_", "lwtlass_tensorop_", "ampere_", "sm80_"}},
   {Lwca::Capability::SM_86, {"lwtlass_simt_", "lwtlass_tensorop_", "ampere_", "sm86_", "sm80_"}},
   {Lwca::Capability::SM_87, {"lwtlass_simt_", "lwtlass_tensorop_", "ampere_", "sm86_", "sm80_"}},
   {Lwca::Capability::SM_89, {"lwtlass_simt_", "lwtlass_tensorop_", "ampere_", "sm86_", "sm80_"}},
   {Lwca::Capability::SM_90, {"lwtlass_simt_", "lwtlass_tensorop_", "sm90_",   "sm86_", "sm80_"}},
};

// Whitelist of supported kernels
const LwdaLinpackCask::KernelMap LwdaLinpackCask::s_KernelMap =
{
    // ================
    // SASS-LIB Kernels
    // ================

    // FP32
    {"ampere_sgemm_128x128_nt_v0", {Instr::FFMA,  "NaiveSgemm"}},
    // FP64
    {"ampere_dgemm_128x64_nt_v0", {Instr::DFMA,  "NaiveDgemm"}},
    // FP16
    {"ampere_hgemm_256x128_nt_v0", {Instr::HFMA2, "NaiveHgemm"}},
    // INT8
    {"ampere_igemm_int8_128x128_ldg4_nt_v0", {Instr::IDP4A_S32_S8, "NaiveIgemm"}},
    // HMMA FP16
    {"ampere_fp16_s16816gemm_fp16_128x128_ldg8_f2f_stages_64x3_nt_v1", {Instr::HMMA_16816_F32_F16, ""}},
    {"ampere_fp16_s16816gemm_fp16_256x128_ldg8_f2f_stages_64x3_nt_v1", {Instr::HMMA_16816_F32_F16, ""}},
    {"ampere_h16816gemm_128x128_ldg8_stages_64x3_nt_v1", {Instr::HMMA_16816_F16_F16, ""}},
    {"ampere_h16816gemm_256x128_ldg8_stages_64x3_nt_v1", {Instr::HMMA_16816_F16_F16, ""}},
    // HMMA BF16
    {"ampere_bf16_s1688gemm_bf16_128x128_ldg8_f2f_nt_v1", {Instr::HMMA_1688_F32_BF16, ""}},
    {"ampere_bf16_s1688gemm_bf16_256x128_ldg8_f2f_nt_v1", {Instr::HMMA_1688_F32_BF16, ""}},
    // HMMA TF32
    {"ampere_fp32_s1684tf32gemm_fp32_128x128_ldg4_nt_v1", {Instr::HMMA_1684_F32_TF32, ""}},
    {"ampere_fp32_s1684tf32gemm_fp32_256x128_ldg4_nt_v1", {Instr::HMMA_1684_F32_TF32, ""}},
    // SparseHMMA FP16
    {"ampere_fp16_s16832gemm_sp_fp16_128x128_ldg8_relu_f2f_stages_64x3_nt_v1", {Instr::HMMA_16832SP_F32_F16, ""}},
    {"ampere_fp16_s16832gemm_sp_fp16_256x128_ldg8_relu_f2f_stages_64x3_nt_v1", {Instr::HMMA_16832SP_F32_F16, ""}},
    // IMMA INT8
    {"ampere_int32_i16832gemm_int8_128x128_ldg16_stages_64x3_nt_v1", {Instr::IMMA_16832_S32_S8, ""}},
    {"ampere_int32_i16832gemm_int8_256x128_ldg16_stages_64x3_nt_v1", {Instr::IMMA_16832_S32_S8, ""}},
    {"ampere_int8_i16832gemm_int8_128x128_ldg16_relu_stages_64x3_nt_v1", {Instr::IMMA_16832_S32_S8, ""}},
    {"ampere_int8_i16832gemm_int8_256x128_ldg16_relu_stages_64x3_nt_v1", {Instr::IMMA_16832_S32_S8, ""}},
    // SparseIMMA INT8
    {"ampere_int32_i16864gemm_sp_int8_256x128_ldg16_stages_128x3_nt_v1", {Instr::IMMA_16864SP_S32_S8, ""}},
    {"ampere_int32_i16864gemm_sp_int8_128x128_ldg16_stages_128x3_nt_v1", {Instr::IMMA_16864SP_S32_S8, ""}},
    {"ampere_int8_i16864gemm_sp_int8_256x128_ldg16_relu_stages_128x3_nt_v1", {Instr::IMMA_16864SP_S32_S8, ""}},

    // ===============
    // LWTLASS Kernels
    // ===============

    // FP32
    {"lwtlass_simt_sgemm_256x128_8x5_nt_align1", {Instr::FFMA,  "NaiveSgemm"}},
    {"lwtlass_simt_sgemm_128x128_8x5_nt_align1", {Instr::FFMA,  "NaiveSgemm"}},
    {"lwtlass_simt_sgemm_128x128_8x2_nt_align1", {Instr::FFMA,  "NaiveSgemm"}},
    {"lwtlass_simt_sgemm_128x64_8x2_nt_align1",  {Instr::FFMA,  "NaiveSgemm"}},
    {"lwtlass_simt_sgemm_128x64_8x5_nt_align1",  {Instr::FFMA,  "NaiveSgemm"}},
    // FP64
    {"lwtlass_simt_dgemm_128x128_8x3_nt_align1",      {Instr::DFMA,  "NaiveDgemm"}},
    {"lwtlass_simt_dgemm_128x128_8x2_nt_align1",      {Instr::DFMA,  "NaiveDgemm"}},
    {"lwtlass_simt_dgemm_128x64_8x2_nt_align1",       {Instr::DFMA,  "NaiveDgemm"}},
    // FP16
    {"lwtlass_simt_hgemm_256x128_8x2_nt_align1",      {Instr::HFMA2, "NaiveHgemm"}},
    {"lwtlass_simt_hgemm_128x128_8x2_nt_align1",      {Instr::HFMA2, "NaiveHgemm"}},
    // INT8
    {"lwtlass_simt_igemm_s8_128x128_32x2_nt_align1",   {Instr::IDP4A_S32_S8, "NaiveIgemm"}},
    {"lwtlass_simt_igemm_s8_128x64_32x2_nt_align1",    {Instr::IDP4A_S32_S8, "NaiveIgemm"}},
    // DMMA
    {"lwtlass_tensorop_d884gemm_128x128_16x3_nt_align1", {Instr::DMMA_884, "NaiveDgemm"}},
    {"lwtlass_tensorop_d884gemm_128x64_16x3_nt_align1",  {Instr::DMMA_884, "NaiveDgemm"}},
    {"lwtlass_tensorop_d884gemm_64x64_16x4_nt_align1",   {Instr::DMMA_884, "NaiveDgemm"}},
    // HMMA FP16<-FP16 (FP16 accumulate)
    {"lwtlass_tensorop_h16816gemm_256x128_32x3_nt_align8", {Instr::HMMA_16816_F16_F16, ""}},
    {"lwtlass_tensorop_h16816gemm_128x128_32x5_nt_align8", {Instr::HMMA_16816_F16_F16, ""}},
    {"lwtlass_tensorop_h16816gemm_256x128_64x3_nt_align8", {Instr::HMMA_16816_F16_F16, ""}},
    {"lwtlass_tensorop_h16816gemm_128x128_64x4_nt_align8", {Instr::HMMA_16816_F16_F16, ""}},
    // HMMA FP32<-FP16
    {"lwtlass_tensorop_s16816gemm_f16_256x128_32x3_nt_align8", {Instr::HMMA_16816_F32_F16, ""}},
    {"lwtlass_tensorop_s16816gemm_f16_128x128_32x5_nt_align8", {Instr::HMMA_16816_F32_F16, ""}},
    {"lwtlass_tensorop_s16816gemm_f16_256x128_64x3_nt_align8", {Instr::HMMA_16816_F32_F16, ""}},
    {"lwtlass_tensorop_s16816gemm_f16_128x128_64x4_nt_align8", {Instr::HMMA_16816_F32_F16, ""}},
    // HMMA FP16<-FP16
    {"lwtlass_tensorop_f16_s16816gemm_f16_256x128_32x3_nt_align8", {Instr::HMMA_16816_F32_F16, ""}},
    {"lwtlass_tensorop_f16_s16816gemm_f16_128x128_32x5_nt_align8", {Instr::HMMA_16816_F32_F16, ""}},
    {"lwtlass_tensorop_f16_s16816gemm_f16_256x128_64x3_nt_align8", {Instr::HMMA_16816_F32_F16, ""}},
    {"lwtlass_tensorop_f16_s16816gemm_f16_128x128_64x4_nt_align8", {Instr::HMMA_16816_F32_F16, ""}},
    // HMMA FP32<-BF16
    {"lwtlass_tensorop_s16816gemm_bf16_256x128_32x3_nt_align8", {Instr::HMMA_16816_F32_BF16, ""}},
    {"lwtlass_tensorop_s16816gemm_bf16_128x128_32x5_nt_align8", {Instr::HMMA_16816_F32_BF16, ""}},
    {"lwtlass_tensorop_s16816gemm_bf16_256x128_64x3_nt_align8", {Instr::HMMA_16816_F32_BF16, ""}},
    {"lwtlass_tensorop_s16816gemm_bf16_128x128_64x4_nt_align8", {Instr::HMMA_16816_F32_BF16, ""}},
    // HMMA BF16<-BF16
    {"lwtlass_tensorop_bf16_s16816gemm_bf16_256x128_32x3_nt_align8", {Instr::HMMA_16816_F32_BF16, ""}},
    {"lwtlass_tensorop_bf16_s16816gemm_bf16_128x128_32x5_nt_align8", {Instr::HMMA_16816_F32_BF16, ""}},
    {"lwtlass_tensorop_bf16_s16816gemm_bf16_256x128_64x3_nt_align8", {Instr::HMMA_16816_F32_BF16, ""}},
    {"lwtlass_tensorop_bf16_s16816gemm_bf16_128x128_64x4_nt_align8", {Instr::HMMA_16816_F32_BF16, ""}},
    // HMMA TF32
    {"lwtlass_tensorop_s1688gemm_tf32_256x128_16x3_nt_align4", {Instr::HMMA_1688_F32_TF32, ""}},
    {"lwtlass_tensorop_s1688gemm_tf32_128x128_16x5_nt_align4", {Instr::HMMA_1688_F32_TF32, ""}},
    {"lwtlass_tensorop_s1688gemm_tf32_256x128_32x3_nt_align4", {Instr::HMMA_1688_F32_TF32, ""}},
    {"lwtlass_tensorop_s1688gemm_tf32_128x128_32x4_nt_align4", {Instr::HMMA_1688_F32_TF32, ""}},
    // IMMA INT8
    {"lwtlass_tensorop_i16832gemm_s8_256x128_64x3_tn_align16", {Instr::IMMA_16832_S32_S8, ""}},
    {"lwtlass_tensorop_i16832gemm_s8_128x128_64x5_tn_align16", {Instr::IMMA_16832_S32_S8, ""}},
    {"lwtlass_tensorop_i16832gemm_s8_256x128_128x3_tn_align16", {Instr::IMMA_16832_S32_S8, ""}},
    {"lwtlass_tensorop_i16832gemm_s8_128x128_128x4_tn_align16", {Instr::IMMA_16832_S32_S8, ""}},
    // IMMA INT4
    {"lwtlass_tensorop_i16864gemm_s4_256x128_128x3_tn_align32", {Instr::IMMA_16864_S32_S4, ""}},
    {"lwtlass_tensorop_i16864gemm_s4_128x128_128x5_tn_align32", {Instr::IMMA_16864_S32_S4, ""}},
    {"lwtlass_tensorop_i16864gemm_s4_256x128_256x3_tn_align32", {Instr::IMMA_16864_S32_S4, ""}},
    {"lwtlass_tensorop_i16864gemm_s4_128x128_256x4_tn_align32", {Instr::IMMA_16864_S32_S4, ""}},
    // BMMA (AND)
    {"lwtlass_tensorop_i168256andgemm_b1_256x128_512x3_tn_align128", {Instr::BMMA_AND_168256_S32_B1, ""}},
    {"lwtlass_tensorop_i168256andgemm_b1_128x128_512x5_tn_align128", {Instr::BMMA_AND_168256_S32_B1, ""}},
    {"lwtlass_tensorop_i168256andgemm_b1_256x128_1024x3_tn_align128", {Instr::BMMA_AND_168256_S32_B1, ""}},
    {"lwtlass_tensorop_i168256andgemm_b1_128x128_1024x4_tn_align128", {Instr::BMMA_AND_168256_S32_B1, ""}},
    // BMMA (XOR)
    {"lwtlass_tensorop_i168256xorgemm_b1_256x128_512x3_tn_align128", {Instr::BMMA_XOR_168256_S32_B1, ""}},
    {"lwtlass_tensorop_i168256xorgemm_b1_128x128_512x5_tn_align128", {Instr::BMMA_XOR_168256_S32_B1, ""}},
    {"lwtlass_tensorop_i168256xorgemm_b1_256x128_1024x3_tn_align128", {Instr::BMMA_XOR_168256_S32_B1, ""}},
    {"lwtlass_tensorop_i168256xorgemm_b1_128x128_1024x4_tn_align128", {Instr::BMMA_XOR_168256_S32_B1, ""}},

    // ============
    // XMMA Kernels
    // ============

    // FP32
    {"sm80_xmma_gemm_f32f32_f32f32_f32_nt_n_tilesize128x128x8_stage3_warpsize2x2x1_ffma_aligna4_alignc4",
        {Instr::FFMA, "NaiveSgemm"}},
    {"sm80_xmma_gemm_f32f32_f32f32_f32_nt_n_tilesize256x128x8_stage3_warpsize4x2x1_ffma_aligna4_alignc4",
        {Instr::FFMA, "NaiveSgemm"}},
    // HMMA FP16<-FP16
    {"sm80_xmma_gemm_f16f16_f16f32_f32_nt_n_tilesize128x128x32_stage4_warpsize2x2x1_tensor16x8x16",
        {Instr::HMMA_16816_F32_F16, ""}},
    {"sm80_xmma_gemm_f16f16_f16f32_f32_nt_n_tilesize256x128x32_stage3_warpsize4x2x1_tensor16x8x16",
        {Instr::HMMA_16816_F32_F16, ""}},
    // HMMA FP16<-FP16 (FP16 accumulate)
    {"sm80_xmma_gemm_f16f16_f16f16_f16_nt_n_tilesize128x256x32_stage3_warpsize2x4x1_tensor16x8x16",
        {Instr::HMMA_16816_F16_F16, ""}},
    {"sm80_xmma_gemm_f16f16_f16f16_f16_nt_n_tilesize256x128x32_stage3_warpsize4x2x1_tensor16x8x16",
        {Instr::HMMA_16816_F16_F16, ""}},
    // HMMA TF32
    {"sm80_xmma_gemm_tf32tf32_tf32f32_f32_nt_n_tilesize128x128x16_stage4_warpsize2x2x1_tensor16x8x8",
        {Instr::HMMA_1688_F32_TF32, ""}},
    // IMMA INT8<-INT8
    {"sm80_xmma_gemm_i8i8_i8i32_f32_tn_n_tilesize256x128x64_stage4_warpsize4x2x1_tensor16x8x32",
        {Instr::IMMA_16832_S32_S8, ""}},
    // SparseHMMA FP16<-FP16
    {"sm80_xmma_sparse_gemm_f16f16_f16f32_f32_nt_n_tilesize128x128x64_stage3_warpsize2x2x1_sptensor16x8x32",
        {Instr::HMMA_16832SP_F32_F16, ""}},
    {"sm80_xmma_sparse_gemm_f16f16_f16f32_f32_nt_n_tilesize256x128x64_stage4_warpsize4x2x1_sptensor16x8x32",
        {Instr::HMMA_16832SP_F32_F16, ""}},
    {"sm86_xmma_sparse_gemm_f16f16_f16f32_f32_nt_n_tilesize256x128x64_stage2_warpsize4x2x1_sptensor16x8x32",
        {Instr::HMMA_16832SP_F32_F16, ""}},
    // SparseIMMA INT8<-INT8
    {"sm80_xmma_sparse_gemm_i8i8_i8i32_f32_tn_n_tilesize256x128x128_stage4_warpsize4x2x1_sptensor16x8x64",
        {Instr::IMMA_16864SP_S32_S8, ""}},
    {"sm80_xmma_sparse_gemm_i8i8_i8i32_f32_tn_t_tilesize256x128x128_stage4_warpsize4x2x1_sptensor16x8x64",
        {Instr::IMMA_16864SP_S32_S8, ""}},
    {"sm86_xmma_sparse_gemm_i8i8_i8i32_f32_tn_t_tilesize256x128x128_stage2_warpsize4x2x1_sptensor16x8x64",
        {Instr::IMMA_16864SP_S32_S8, ""}},

    // ==================
    // SM90+ XMMA Kernels
    // ==================
    // TODO kernel selection may not be optimal, especially nt_n/nt_t/tn_t/etc
    // TODO add FP8/E8M7 kernels when available

    // HGMMA FP16<-FP16
    {"sm90_xmma_gemm_f16f16_f16f32_f32_nt_n_tilesize256x128x64_cgasize1x2x1_stage4_gmmastage2_warpgroupsize2x1x1_hgmma64x128x16",
        {Instr::HGMMA_F32_F16, ""}},
    {"sm90_xmma_gemm_f16f16_f16f32_f32_nt_n_tilesize128x128x64_cgasize1x2x1_stage4_gmmastage2_warpgroupsize1x1x1_hgmma64x128x16",
        {Instr::HGMMA_F32_F16, ""}},
    {"sm90_xmma_gemm_f16f16_f16f32_f32_nt_n_tilesize256x128x64_stage4_gmmastage2_warpgroupsize2x1x1_hgmma64x128x16",
        {Instr::HGMMA_F32_F16, ""}},
    {"sm90_xmma_gemm_f16f16_f16f32_f32_nt_n_tilesize128x128x64_stage4_gmmastage2_warpgroupsize1x1x1_hgmma64x128x16",
        {Instr::HGMMA_F32_F16, ""}},
    // HGMMA FP16<-FP16 (FP16 accumulate)
    {"sm90_xmma_gemm_f16f16_f16f16_f16_nt_n_tilesize256x128x64_cgasize1x2x1_stage4_gmmastage2_warpgroupsize1x1x1_hgmma64x128x16",
        {Instr::HGMMA_F16_F16, ""}},
    {"sm90_xmma_gemm_f16f16_f16f16_f16_nt_n_tilesize128x128x64_cgasize1x2x1_stage4_gmmastage2_warpgroupsize1x1x1_hgmma64x128x16",
        {Instr::HGMMA_F16_F16, ""}},
    {"sm90_xmma_gemm_f16f16_f16f16_f16_nt_n_tilesize256x128x64_stage4_gmmastage2_warpgroupsize1x1x1_hgmma64x128x16",
        {Instr::HGMMA_F16_F16, ""}},
    {"sm90_xmma_gemm_f16f16_f16f16_f16_nt_n_tilesize128x128x64_stage4_gmmastage2_warpgroupsize1x1x1_hgmma64x128x16",
        {Instr::HGMMA_F16_F16, ""}},
    // HGMMA TF32
    {"sm90_xmma_gemm_f32f32_tf32f32_f32_tn_n_tilesize256x128x32_cgasize1x2x1_stage4_gmmastage2_warpgroupsize2x1x1_hgmma64x128x8",
        {Instr::HGMMA_F32_TF32, ""}},
    {"sm90_xmma_gemm_f32f32_tf32f32_f32_tn_n_tilesize128x128x32_cgasize1x2x1_stage4_gmmastage2_warpgroupsize1x1x1_hgmma64x128x8",
        {Instr::HGMMA_F32_TF32, ""}},

    // IGMMA INT32<-INT8
    {"sm90_xmma_gemm_i8i32_i8i32_f32_tn_n_tilesize256x128x128_cgasize1x2x1_stage4_gmmastage2_warpgroupsize2x1x1_igmma64x128x32",
        {Instr::IGMMA_S32_S8, ""}},
    {"sm90_xmma_gemm_i8i32_i8i32_f32_tn_n_tilesize128x128x128_cgasize1x2x1_stage4_gmmastage2_warpgroupsize1x1x1_igmma64x128x32",
        {Instr::IGMMA_S32_S8, ""}},
    {"sm90_xmma_gemm_i8i32_i8i32_f32_tn_n_tilesize256x128x128_stage4_gmmastage2_warpgroupsize2x1x1_igmma64x128x32",
        {Instr::IGMMA_S32_S8, ""}},
    {"sm90_xmma_gemm_i8i32_i8i32_f32_tn_n_tilesize128x128x128_stage4_gmmastage2_warpgroupsize1x1x1_igmma64x128x32",
        {Instr::IGMMA_S32_S8, ""}},
    // IGMMA INT8<-INT8
    {"sm90_xmma_gemm_i8i8_i8i32_f32_tn_n_tilesize256x128x128_cgasize1x2x1_stage4_gmmastage2_warpgroupsize2x1x1_igmma64x128x32",
        {Instr::IGMMA_S32_S8, ""}},
    {"sm90_xmma_gemm_i8i8_i8i32_f32_tn_n_tilesize128x128x128_cgasize1x2x1_stage4_gmmastage2_warpgroupsize1x1x1_igmma64x128x32",
        {Instr::IGMMA_S32_S8, ""}},
    {"sm90_xmma_gemm_i8i8_i8i32_f32_tn_n_tilesize256x128x128_stage4_gmmastage2_warpgroupsize2x1x1_igmma64x128x32",
        {Instr::IGMMA_S32_S8, ""}},
    {"sm90_xmma_gemm_i8i8_i8i32_f32_tn_n_tilesize128x128x128_stage4_gmmastage2_warpgroupsize1x1x1_igmma64x128x32",
        {Instr::IGMMA_S32_S8, ""}},

    // SparseHGMMA FP16<-FP16
    {"sm90_xmma_sparse_gemm_f16f16_f16f32_f32_nt_t_tilesize128x128x64_cgasize2x1x1_stage4_gmmastage2_warpgroupsize1x1x1_sptensor_hgmma64x128x32",
        {Instr::HGMMA_SP_F32_F16, ""}},
    {"sm90_xmma_sparse_gemm_f16f16_f16f32_f32_nt_t_tilesize128x64x64_stage4_gmmastage2_warpgroupsize1x1x1_sptensor_hgmma64x64x32",
        {Instr::HGMMA_SP_F32_F16, ""}},
    // SparseHGMMA FP16<-FP16 (FP16 accumulate)
    {"sm90_xmma_sparse_gemm_f16f16_f16f16_f16_nt_t_tilesize128x256x64_cgasize2x1x1_stage4_gmmastage2_warpgroupsize1x1x1_sptensor_hgmma64x256x32",
        {Instr::HGMMA_SP_F16_F16, ""}},
    {"sm90_xmma_sparse_gemm_f16f16_f16f16_f16_nt_t_tilesize64x256x64_cgasize2x1x1_stage4_gmmastage2_warpgroupsize1x1x1_sptensor_hgmma64x256x32",
        {Instr::HGMMA_SP_F16_F16, ""}},

    // SparseIGMMA INT32<-INT8
    {"sm90_xmma_sparse_gemm_i8i32_i8i32_f32_tn_t_tilesize128x64x128_cgasize1x1x1_stage4_gmmastage2_warpgroupsize1x1x1_sptensor_igmma64x64x64_alignc64",
        {Instr::IGMMA_SP_S32_S8, ""}},
    // SparseIGMMA INT8<-INT8
    {"sm90_xmma_sparse_gemm_i8i8_i8i32_f32_tn_t_tilesize128x128x128_cgasize2x1x1_stage4_gmmastage2_warpgroupsize1x1x1_sptensor_igmma64x128x64",
        {Instr::IGMMA_SP_S32_S8, ""}},

    // DMMA Kernels
    {"sm90_xmma_gemm_f64f64_f64f64_f64_nt_n_tilesize128x128x16_stage3_warpsize4x2x1_tensor16x8x16",
        {Instr::DMMA_16816, "NaiveDgemm"}},
    {"sm90_xmma_gemm_f64f64_f64f64_f64_nt_n_tilesize128x64x32_stage3_warpsize4x2x1_tensor16x8x16",
        {Instr::DMMA_16816, "NaiveDgemm"}},
};

// Static vars for CASK init
bool LwdaLinpackCask::s_ManifestInit = false;
Tasker::Mutex LwdaLinpackCask::s_pCaskInitMutex =
    Tasker::CreateMutex("CaskManifest", Tasker::mtxUnchecked);
cask::Manifest* LwdaLinpackCask::s_pManifest(cask::Manifest::GetInstance());

//-----------------------------------------------------------------------
//! \brief Constructor
//!
LwdaLinpackCask::LwdaLinpackCask()
{
    SetName("LwdaLinpackCask");
}

const LwdaLinpackCask::KernelMap& LwdaLinpackCask::GetKernelMap() const
{
    return s_KernelMap;
}

bool LwdaLinpackCask::IsSupported()
{
    // Check if compute is supported at allocate
    if (!LwdaStreamTest::IsSupported())
    {
        return false;
    }
    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    if (cap < Lwca::Capability::SM_80)
    {
        Printf(Tee::PriLow, "%s does not support this SM version\n", GetName().c_str());
        return false;
    }
    return true;
}

void LwdaLinpackCask::TestHelp(JSContext *pCtx, regex *pRegex)
{
    bool bObjectFound;
    Help::JsObjHelp(GetJSObject(), pCtx, pRegex, 0, nullptr, &bObjectFound);
    LwdaLinpack::PropertyHelp(pCtx, pRegex);
}

//------------------------------------------------------------------------------
//! \brief Return name of GEMM function
//!
const char* LwdaLinpackCask::GemmFunctionName()
{
    return m_KernelName.c_str();
}

//------------------------------------------------------------------------------
//! \brief Allocate buffers required for SMID dump
//! This map gets populated by the running Linpack kernel
//!
RC LwdaLinpackCask::AllocateSmidBuffers()
{
    RC rc;
    const UINT64 bufferSize = sizeof(UINT16) * m_GridDimX * m_GridDimY;
    CHECK_RC(GetLwdaInstance().AllocHostMem(bufferSize, &m_HostSmidMap));
    // The device mem is allocated automatically by CASK
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Copy from device to host smid buffer
RC LwdaLinpackCask::CopySmidMapToHost(Lwca::HostMemory& hostSmidMap)
{
    RC rc;
    CHECK_CASK_RC(
        m_pCaskShader->populateSMid(m_CaskRunInfo,
#if !defined(LWCPU_AARCH64) // TODO Using older CASK as temp WAR to unblock AMD64
                                    reinterpret_cast<void*>(m_CaskDeviceWorkspace.GetDevicePtr()),
#endif
                                    hostSmidMap.GetPtr(),
                                    hostSmidMap.GetSize()),
        "Failed to populate SMID buffer in CASK",
        m_CaskRunInfo.hostBuf
    );
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Check that Alpha and Beta are valid (unless the check is overridden)
//!
RC LwdaLinpackCask::ValidateAlphaBeta()
{
    StickyRC stickyRc;
    switch (m_KernParams.typeofOutputElement)
    {
        case R_8I:
            // Alpha and Beta work with INT8 IMMAGEMM, but must be integral since they are colwerted
            // by the kernel from float to INT32.
            //
            // Alpha must be positive to ensure consistency. This is because Alpha scales a 32bit
            // intermediate value while Beta scales an upcolwerted 8bit result. Since the IMMA results
            // saturate to -128 or 127, a negative Alpha would result in overflowed values swapping signs.
            //
            // For instance, consider the case where Alpha=-1 and Beta=2:
            // -300 * -1 + -128 * 2
            //  300      + -256
            //   44
            //   44 != -128
            //
            // Now if Alpha=2 and Beta=-1 everything is fine since the result saturates:
            // -300 * 2  + -128 * -1
            // -600      +  128
            // -472
            // -128 == -128
            stickyRc = LwdaLinpack::ValidateAlphaBeta();
            if (floor(m_Alpha) != m_Alpha || floor(m_Beta) != m_Beta)
            {
                Printf(Tee::PriError, "Alpha and Beta must be integers for INT8 output.\n");
                stickyRc = RC::BAD_PARAMETER;
            }
            if (m_Alpha < 0)
            {
                Printf(Tee::PriError, "Alpha must be >= 0 for INT8 output.\n");
                stickyRc = RC::BAD_PARAMETER;
            }
            break;
        case R_32I:
            // The INT32-output kernels only allow values of 0 and 1
            if (m_Alpha != 1.0 || m_Beta != 1.0)
            {
                Printf(Tee::PriError, "Both Alpha and Beta must be set to 1 for INT32 output.\n");
                stickyRc = RC::BAD_PARAMETER;
            }
            break;
        case R_16F:
            // HGEMM kernels are not consistent due to Alpha/Beta scaling:
            // Alpha=1 Beta=0 and Alpha=Beta=0.5 do not always yield consistent results
            //
            // The likely cause is a lack of precision in the intermediate result when multiplying
            // by a fraction. It's not clear why this doesn't impact the HMMA FP16 acc kernels,
            // maybe the epilog scaling is handled differently.
            //
            stickyRc = LwdaLinpack::ValidateAlphaBeta();
            if (m_Instr == Instr::HFMA2 &&
                (floor(m_Alpha) != m_Alpha || floor(m_Beta) != m_Beta))
            {
                Printf(Tee::PriError, "Alpha and Beta must be integers for HGEMM kernels.\n");
                stickyRc = RC::BAD_PARAMETER;
            }
            break;
        default:
            stickyRc = LwdaLinpack::ValidateAlphaBeta();
            break;
    }
    return stickyRc;
}

RC LwdaLinpackCask::ScaleReferenceData(UINT32 scale)
{
    RC rc;
    if (m_KernParams.typeofOutputElement == R_32I)
    {
        if (UseReferenceData())
        {
            CHECK_RC(m_ScaleMatrixFunc.Launch(
                            CalcRefMatrixCPtr(),
                            m_OutputBufferLen,
                            scale));
        }
        else
        {
            CHECK_RC(m_ScaleMatrixFunc.Launch(
                            CalcMatrixCPtr(),
                            m_OutputBufferLen,
                            scale));
            CallwlateCrc(CalcMatrixCPtr(), CalcRefCrcBufPtr());
        }
    }
    return rc;
}

RC LwdaLinpackCask::ResetReferenceData()
{
    RC rc;
    if (m_KernParams.typeofOutputElement == R_32I)
    {
        auto cPtr = (UseReferenceData()) ? CalcRefMatrixCPtr() : CalcMatrixCPtr();
        return RunGemm(CalcMatrixAPtr(), CalcMatrixBPtr(), cPtr, true);
    }
    return rc;
}

//! \brief colwert to the enum type used by the rest of LwdaLinpack
//!
//! Cask uses its own verison of the enum type that may be mismatched.
//! Better to keep the two types separate than attempt to keep them in sync.
elementDataType_t LwdaLinpackCask::GetLegacyEnumType(cask::ScalarType type)
{
    switch (type)
    {
        case cask::ScalarType::FP64:
            return R_64F;
        case cask::ScalarType::FP32:
            return R_32F;
        case cask::ScalarType::FP16:
            return R_16F;
        case cask::ScalarType::INT32:
            return R_32I;
        case cask::ScalarType::INT8:
            return R_8I;
        case cask::ScalarType::UINT8:
            return R_8U;
        case cask::ScalarType::UINT16:
            return R_16U;
        case cask::ScalarType::TF32:
            return TF32_19F;
        case cask::ScalarType::BF16:
            return BF16_16F;
        case cask::ScalarType::INT4:
            return R_4I;
        case cask::ScalarType::UINT4:
            return R_4U;
        case cask::ScalarType::UINT1:
            return R_1U;
        default:
            MASSERT(!"UNSUPPORTED KERNEL ENUM TYPE");
            return R_32F;
    }
}

cask::GemmShader* LwdaLinpackCask::FindCaskShader
(
    string* pShaderName
)
{
    // Replace any substring aliases
    string replacedStr = *pShaderName;
    for (const auto& mapping : s_AliasMap)
    {
        replacedStr = Utility::StringReplace(replacedStr, mapping.first, mapping.second);
    }

    // Direct lookup
    const auto* pGemmShaders = cask::GemmShaderList::availableShaders();
    cask::GemmShader* pShader = pGemmShaders->findByName(replacedStr);
    if (pShader)
    {
        *pShaderName = replacedStr;
        return pShader;
    }

    // If the kernel wasn't found directly, iterate through the prefix map in order
    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    if (s_FamilyMap.count(cap))
    {
        for (const string& prefix : s_FamilyMap.at(cap))
        {
            const string prefixedStr = prefix + replacedStr;
            pShader = pGemmShaders->findByName(prefixedStr);
            if (pShader)
            {
                // Update kernel name
                *pShaderName = prefixedStr;
                return pShader;
            }
        }
    }
    return nullptr;
}

bool LwdaLinpackCask::KernelInstrMatchesDefault(const string& kernelName, const Tee::Priority pri)
{
    // If we are using a LwdaLinpackCask test variant, check that the provided kernel
    // tests the same instruction as the default kernel.
    if (m_DefaultKernelName != "")
    {
        // Check that the default kernel (if set) is in whitelist
        if (!GetKernelMap().count(m_DefaultKernelName))
        {
            Printf(pri, "Kernel %s not supported by %s (Cannot find default kernel %s in whitelist)\n",
                   kernelName.c_str(), GetName().c_str(), m_DefaultKernelName.c_str());
            return false;
        }
        const Instr instr        = GetKernelMap().at(kernelName).instr;
        const Instr defaultInstr = GetKernelMap().at(m_DefaultKernelName).instr;
        bool commonType = false;
        for (const auto& types : s_VariantTypes)
        {
            if (types.count(instr) && types.count(defaultInstr))
            {
                commonType = true;
                break;
            }
        }
        if (instr != defaultInstr && !commonType)
        {
            Printf(pri, "Kernel %s not supported by %s (GEMM instruction-type MISMATCH)\n",
                   kernelName.c_str(), GetName().c_str());
            return false;
        }
    }

    return true;
}

bool LwdaLinpackCask::KernelInWhitelist(const string& kernelName, const Tee::Priority pri)
{
    // Check that the provided kernel is in whitelist
    if (!GetKernelMap().count(kernelName))
    {
        Printf(pri, "Kernel %s not supported by %s (Cannot find kernel in whitelist)\n",
               kernelName.c_str(), GetName().c_str());
        return false;
    }

    // Check that the SM version is supported by the kernel
    bool smSupported = false;
    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    if (s_SupportedMap.count(cap))
    {
        for (const string& prefix : s_SupportedMap.at(cap))
        {
            if (0 == kernelName.compare(0, prefix.size(), prefix))
            {
                smSupported = true;
                break;
            }
        }
    }
    if (!smSupported)
    {
        Printf(pri, "Kernel %s not supported by %s on SM %d.%d\n",
               kernelName.c_str(),
               GetName().c_str(),
               cap.MajorVersion(), cap.MinorVersion());
        return false;
    }

    // Check that the kernel's GEMM instruction is supported on the current chip
    const Instr instr = GetKernelMap().at(kernelName).instr;
    if (Platform::GetSimulationMode() != Platform::Amodel &&
        GetBoundGpuSubdevice()->GetArchEfficiency(instr) == 0.0)
    {
        Printf(pri, "Kernel %s uses unsupported GEMM instruction on SM %d.%d\n",
               kernelName.c_str(),
               cap.MajorVersion(), cap.MinorVersion());
        return false;
    }

    return true;
}

RC LwdaLinpackCask::SetCaskShader()
{
    RC rc;
    const auto* pGemmShaders = cask::GemmShaderList::availableShaders();
    MASSERT(pGemmShaders);

    // Update m_DefaultKernelName to the full kernel name if needed
    if (m_DefaultKernelName != "")
    {
        cask::GemmShader* pDefaultShader = FindCaskShader(&m_DefaultKernelName);
        if (pDefaultShader == nullptr)
        {
            Printf(Tee::PriError,
                   "Default Kernel %s not found in the list of CASK shaders packaged with MODS!\n",
                   m_DefaultKernelName.c_str());
            return RC::SOFTWARE_ERROR;
        }
    }

    // Dump list of kernels if requested
    vector<std::tuple<string, INT32>> shaderNames;
    if (m_PrintKernelList || m_PrintPackagedKernels)
    {
        LWdevice devHandle = GetBoundLwdaDevice().GetHandle();
        for (auto* pShader : *pGemmShaders)
        {
            const INT32 oclwpancy =
                pShader->QueryOclwpancyProvider().getOclwpancy(devHandle);
            shaderNames.emplace_back(pShader->name, oclwpancy);
            // Reset error if getOclwpancy failed
            lwdaGetLastError();
        }
        // Sort the names lexicographically
        std::sort(shaderNames.begin(), shaderNames.end());
    }
    if (m_PrintKernelList)
    {
        const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
        Printf(Tee::PriNormal, "\nKernels supported by %s on SM %d.%d:\n",
               GetName().c_str(), cap.MajorVersion(), cap.MinorVersion());
        for (const auto& tup : shaderNames)
        {
            const string& shaderName = std::get<0>(tup);
            const int32& oclwpancy   = std::get<1>(tup);
            if (KernelInWhitelist(shaderName, Tee::PriNone) &&
                KernelInstrMatchesDefault(shaderName, Tee::PriNone) &&
                oclwpancy > 0)
            {
                Printf(Tee::PriNormal, "%s : Oclwpancy %d\n", shaderName.c_str(), oclwpancy);
            }
        }
        Printf(Tee::PriNormal, "\n");
    }
    if (m_PrintPackagedKernels)
    {
        Printf(Tee::PriWarn, "\nDumping list of all shaders packaged with MODS CASK.\n"
                             "This is for debugging only, not all kernels are supported!\n");
        for (const auto& tup : shaderNames)
        {
            Printf(Tee::PriNormal, "%s : Oclwpancy %d%s\n",
                   std::get<0>(tup).c_str(),
                   std::get<1>(tup),
                   GetKernelMap().count(std::get<0>(tup)) ? "" : " (Not in Whitelist)");
        }
    }

    // Ensure that all kernels in the whitelist are packaged with MODS
    // This prevents SW regressions in kernel support
    bool whitelistPackaged = true;
    for (const auto& entry : GetKernelMap())
    {
        if (!pGemmShaders->findByName(entry.first))
        {
            Printf(Tee::PriError,
                   "Whitelisted kernel %s is not packaged with MODS!\n", entry.first.c_str());
            whitelistPackaged = false;
        }
    }
    if (!whitelistPackaged)
    {
        return RC::SOFTWARE_ERROR;
    }

    // If the kernel name is not explicitly set use the test default (if present)
    if (m_KernelName.empty() && !m_DefaultKernelName.empty())
    {
        m_KernelName = m_DefaultKernelName;
    }

    // Verify that kernel name exists
    if (m_KernelName.empty())
    {
        Printf(Tee::PriError, "LwdaLinpackCask test argument KernelName isn't set!\n");
        return RC::BAD_PARAMETER;
    }

    // Fetch shader and update m_KernelName to the full kernel name if needed
    m_pCaskShader = FindCaskShader(&m_KernelName);
    if (m_pCaskShader == nullptr)
    {
        Printf(Tee::PriError,
               "Kernel %s not found in the list of CASK shaders packaged with MODS!\n",
               GemmFunctionName());
        return RC::BAD_PARAMETER;
    }

    // Verify that the kernel is supported by LwdaLinpackCask
    if (!KernelInWhitelist(m_KernelName, Tee::PriError) ||
        !KernelInstrMatchesDefault(m_KernelName, Tee::PriError))
    {
        return RC::BAD_PARAMETER;
    }

    // Fetch kernel oclwpancy from CASK
    const LWdevice devHandle = GetBoundLwdaDevice().GetHandle();
    INT32 kernelOclwpancy = m_pCaskShader->QueryOclwpancyProvider().getOclwpancy(devHandle);
    if (kernelOclwpancy < 1)
    {
        const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
        Printf(Tee::PriError, "Kernel %s not supported by %s (Reported oclwpancy of %d on SM %d.%d)\n",
               GemmFunctionName(),
               GetName().c_str(),
               kernelOclwpancy,
               cap.MajorVersion(), cap.MinorVersion());
        return RC::BAD_PARAMETER;
    }
    m_MaxBlocksPerSM = kernelOclwpancy;

    // Lookup kernel info
    const KernelDesc& myKernel = GetKernelMap().at(m_KernelName);
    m_Instr                      = myKernel.instr;
    m_GemmData.naiveGemmFuncName = myKernel.naiveGemmFuncName;

    return rc;
}

RC LwdaLinpackCask::SetupCaskParams()
{
    RC rc;
    KernelParameters params = {};

    // Default kernel values
    params.isSparse = false;
    params.metadataRatio = 0;
    params.sizeofMetadataElement = 0;

    // Fill in info from KernelInfo
    const auto* pKernInfo = m_pCaskShader->getInfo();
    MASSERT(pKernInfo);
    m_TypeA             = pKernInfo->aTensorType().scalarType;
    m_TypeB             = pKernInfo->bTensorType().scalarType;
    m_TypeC             = pKernInfo->cTensorType().scalarType;
    m_LayoutA           = pKernInfo->aLayout();
    m_LayoutB           = pKernInfo->bLayout();
    m_LayoutC           = pKernInfo->cLayout();
    params.rowsPerBlock = pKernInfo->optional().getMetadata<cask::BLOCK_TILE_M>();
    params.colsPerBlock = pKernInfo->optional().getMetadata<cask::BLOCK_TILE_N>();

    // These parameters are either constant or derived from the variable parameters
    MASSERT(m_TypeA == m_TypeB);
    params.typeofInputElement  = GetLegacyEnumType(m_TypeA);
    params.typeofOutputElement = GetLegacyEnumType(m_TypeC);
    params.mSizeMultiple = params.rowsPerBlock;
    params.nSizeMultiple = params.colsPerBlock;

    // Get default Ksize alignment
    MASSERT(pKernInfo->aTensorType().alignment ==
            pKernInfo->bTensorType().alignment);
    params.kSizeMultiple =
        NumBytesToElems(params.typeofInputElement,
                        pKernInfo->aTensorType().alignment);
    // Ensure we have enough scalars per element
    MASSERT(pKernInfo->aTensorType().scalarsPerElement ==
            pKernInfo->bTensorType().scalarsPerElement);
    params.kSizeMultiple =
        std::max<UINT32>(params.kSizeMultiple, pKernInfo->aTensorType().scalarsPerElement);

    // Infer the kernels needed for generating random data / checking results
    switch (params.typeofInputElement)
    {
        case R_1U:
        case R_4I:
            // For elements less than 8 bits in size, use 8-bit functions
            m_GemmData.genConstantFuncName = "GenConstantDataI";
            m_GemmData.genRandomFuncName = "GenRandomDataI";

            // Override default Alpha/Beta to accumulate
            m_DefaultAlpha = 1.0;
            m_DefaultBeta  = 1.0;
            break;
        case R_8I:
            // For INT32 output we want uniformly distributed random data.
            // INT8 output must be normally distributed, otherwise it will overflow
            m_GemmData.genConstantFuncName = "GenConstantDataI";
            if (params.typeofOutputElement == R_32I)
            {
                m_GemmData.genRandomFuncName = "GenRandomDataI";
                // Override default Alpha/Beta to accumulate
                m_DefaultAlpha = 1.0;
                m_DefaultBeta  = 1.0;
            }
            else if (params.typeofOutputElement == R_8I)
            {
                m_GemmData.genRandomFuncName = "GenRandomDataIMMA";
                // Override default Alpha/Beta to prevent overflow
                // (See ValidateAlphaBeta for why these constraints exist)
                m_DefaultAlpha =  2.0;
                m_DefaultBeta  = -1.0;
            }
            else
            {
                MASSERT(!"Unsupported input type for INT8 GenRandomData");
                return RC::SOFTWARE_ERROR;
            }
            break;
        case R_16F:
            m_GemmData.genRandomFuncName   = "GenRandomDataH";
            m_GemmData.genConstantFuncName = "GenConstantDataH";

            // HGEMM kernels require integral alpha/beta to ensure consistency
            // (See LwdaLinpackCask::ValidateAlphaBeta for explanation)
            if (m_Instr == Instr::HFMA2)
            {
                m_DefaultAlpha = 2.0;
                m_DefaultBeta = -1.0;
            }
            break;
        case BF16_16F:
            m_GemmData.genRandomFuncName   = "GenRandomDataBF16";
            m_GemmData.genConstantFuncName = "GenConstantDataBF16";
            break;
        // For TF32 just generate FP32 and ignore the lower bits
        case TF32_19F:
        case R_32F:
            m_GemmData.genRandomFuncName   = "GenRandomDataF";
            m_GemmData.genConstantFuncName = "GenConstantDataF";
            break;
        case R_64F:
            m_GemmData.genRandomFuncName   = "GenRandomDataD";
            m_GemmData.genConstantFuncName = "GenConstantDataD";
            break;
        default:
            MASSERT(!"Unsupported input type for GenRandomData/GenConstantData");
            return RC::SOFTWARE_ERROR;
    }
    switch (params.typeofOutputElement)
    {
        case R_8I:
            m_GemmData.checkFuncName = "CompareOnDeviceIMMA";
            break;
        case R_32I:
            m_GemmData.checkFuncName = "CompareOnDeviceI";
            break;
        case R_16F:
            m_GemmData.checkFuncName = "CompareOnDeviceH";
            break;
        case BF16_16F:
            m_GemmData.checkFuncName = "CompareOnDeviceBF16";
            break;
        case TF32_19F:
            m_GemmData.checkFuncName = "CompareOnDeviceTF32";
            break;
        case R_32F:
            m_GemmData.checkFuncName = "CompareOnDeviceF";
            break;
        case R_64F:
            m_GemmData.checkFuncName = "CompareOnDeviceD";
            break;
        default:
            MASSERT(!"Unsupported output type for checkFuncName");
            return RC::SOFTWARE_ERROR;
    }
    MASSERT(m_GemmData.genRandomFuncName);
    MASSERT(m_GemmData.genConstantFuncName);
    MASSERT(m_GemmData.checkFuncName);

    // ====================
    // Additional Overrides
    // ====================

    // Sparse kernels have some special settings
    if (m_pCaskShader->isSparse())
    {
        params.isSparse = true;

        // Sparse XMMA kernels have some weird Ksize requirements
        // Seems that lwrrently it must be aligned to BLOCK_TILE_K (perhaps a bug?)
        // TODO revisit this
        if (m_KernelName.find("xmma") != std::string::npos)
        {
            params.kSizeMultiple =
                std::max<UINT32>(params.kSizeMultiple,
                                 pKernInfo->optional().getMetadata<cask::BLOCK_TILE_K>());
        }
        // Otherwise Ksize is implicitly doubled on sparse
        else
        {
            params.kSizeMultiple *= 2;
        }

        // Every 4 bits maps to 2 elements in the compressed A matrix
        // So each uint16 element maps to 8 elements in A
        params.metadataRatio = 8;
        params.sizeofMetadataElement = sizeof(UINT16);
    }

    // XMMA kernels limit the size of the matrices
    if (m_KernelName.find("xmma") != std::string::npos)
    {
        if (params.typeofOutputElement == R_32I ||
            params.typeofOutputElement == R_8I)
        {
            m_MaxMatrixNumElements = std::numeric_limits<INT32>::max();
        }
        else
        {
            m_MaxMatrixNumElements = std::numeric_limits<UINT32>::max();
        }
    }

    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    m_GemmData.smParameters[cap] = params;
    return rc;
}

RC LwdaLinpackCask::SetupCaskKernel()
{
    RC rc;

    // Setup CASK
    CHECK_RC(InitCaskGemm(&m_CaskInitGemm, true));
    CHECK_RC(InitCaskGemm(&m_CaskGemm, false));
    CHECK_RC(InitCaskBuffers(
        m_CaskInitGemm,
        &m_CaskInitRunInfo,
        &m_CaskInitHostBuffer,
        &m_CaskInitDeviceBuffer,
        &m_CaskInitDeviceWorkspace));
    CHECK_RC(InitCaskBuffers(
        m_CaskGemm,
        &m_CaskRunInfo,
        &m_CaskHostBuffer,
        &m_CaskDeviceBuffer,
        &m_CaskDeviceWorkspace));
    CHECK_RC(GetLwdaInstance().Synchronize());
    return rc;
}

// Cask does not use m_GemmFunc
RC LwdaLinpackCask::InitGemmFunc()
{
    return RC::OK;
}

// Cask does not use m_GemmFunc
RC LwdaLinpackCask::ConfigGemmFuncDim()
{
    return RC::OK;
}

RC LwdaLinpackCask::HandleCaskError
(
    const cask::Error& caskError,
    const string& errorMsg,
    void* pHostBuf
)
{
    if (caskError != cask::Error::OK)
    {
        Printf(Tee::PriError, "%s (%s)\n", errorMsg.c_str(), cask::getErrorString(caskError));
        if (pHostBuf && caskError == cask::Error::PLATFORM)
        {
            lwdaError_t result = m_pCaskShader->lastPlatformError(pHostBuf);
            Printf(Tee::PriError, "libcask failure was in LWCA runtime (%s)\n",
                   lwdaGetErrorString(result));
            return RC::LWDA_ERROR;
        }
        else
        {
            return RC::SOFTWARE_ERROR;
        }
    }
    return RC::OK;
}

RC LwdaLinpackCask::LaunchGemmKernel(const ByteStream& paramBuffer)
{
    RC rc;
    cask::RunInfo& runInfo =
        m_TmpRunParams.init ?
        m_CaskInitRunInfo :
        m_CaskRunInfo;
    UINT64 deviceWorkspacePtr =
        m_TmpRunParams.init ?
        m_CaskInitDeviceWorkspace.GetDevicePtr() :
        m_CaskDeviceWorkspace.GetDevicePtr();

    // Hopper kernels lwrrently must synchronize between launches of kernels
    // with different parameters
    //
    // Maybe some race condition ilwolving the TMA or LWCA entry/exit code
    if (GetBoundGpuSubdevice()->HasBug(3522446))
    {
        CHECK_RC(GetLwdaInstance().Synchronize());
    }
    CHECK_CASK_RC(
        m_pCaskShader->run(
            runInfo,
            reinterpret_cast<void*>(deviceWorkspacePtr),
            reinterpret_cast<void*>(m_TmpRunParams.C),
            reinterpret_cast<void*>(m_TmpRunParams.A),
            reinterpret_cast<void*>(m_TmpRunParams.Am),
            reinterpret_cast<void*>(m_TmpRunParams.B),
            reinterpret_cast<void*>(m_TmpRunParams.C),
            nullptr, // Bias Vector
            nullptr, // Alpha Vector
            nullptr, // Beta Vector
            GetLwdaInstance().GetStream(GetBoundLwdaDevice()).GetHandle()),
        "Cask kernel launch failed!",
        runInfo.hostBuf
    );
    return rc;
}

RC LwdaLinpackCask::CheckHardwareInfo(const cask::HardwareInformation& hwInfo)
{
    StickyRC stickyRc;

    // Check reported compute cap
    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    const cask::SmIsa& isa = hwInfo.isa();
    if (isa.majorVersion() != cap.MajorVersion() ||
        isa.minorVersion() != cap.MinorVersion())
    {
        Printf(Tee::PriError,
               "Cask HardwareInfo: Reported SM version (%d.%d) != MODS SM version (%d.%d)\n",
               isa.majorVersion(), isa.minorVersion(),
               cap.MajorVersion(), cap.MinorVersion());
        stickyRc = RC::SOFTWARE_ERROR;
    }

    // Check reported SM count
    const INT32 smCount = GetBoundLwdaDevice().GetShaderCount();
    if (hwInfo.multiProcessorCount != smCount)
    {
        Printf(Tee::PriError,
               "Cask HardwareInfo: Reported SM count (%d) != MODS count (%d)\n",
               hwInfo.multiProcessorCount, smCount);
        stickyRc = RC::SOFTWARE_ERROR;
    }

    // Check reported max shared memory count
    const INT32 maxSharedMemPerSM =
        GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_MULTIPROCESSOR);
    if (hwInfo.getSharedMemPerMultiprocessor() != maxSharedMemPerSM)
    {
        Printf(Tee::PriError,
               "Cask HardwareInfo: Reported max sharedmem/SM size (%d bytes) != MODS size (%d bytes)\n",
               hwInfo.getSharedMemPerMultiprocessor(), maxSharedMemPerSM);
        stickyRc = RC::SOFTWARE_ERROR;
    }

    // Check reported max registers count
    const INT32 maxRegistersPerSM =
        GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_MULTIPROCESSOR);
    if (hwInfo.getRegistersPerMultiprocessor() != maxRegistersPerSM)
    {
        Printf(Tee::PriError,
               "Cask HardwareInfo: Reported max register/SM size (%d reg) != MODS size (%d reg)\n",
               hwInfo.getRegistersPerMultiprocessor(), maxRegistersPerSM);
        stickyRc = RC::SOFTWARE_ERROR;
    }

    // Check reported skyline (only on SM90+ HW)
    if (cap >= Lwca::Capability::SM_90 &&
        Platform::GetSimulationMode() != Platform::Amodel &&
        !Platform::IsVirtFunMode() &&
        !GetBoundGpuSubdevice()->IsSMCEnabled())
    {
        // Get SM counts from HardwareInfo
        vector<UINT32> caskCounts;
        for (const auto& gpcAttr : hwInfo.skyline)
        {
            caskCounts.push_back(gpcAttr.multiProcessorCount());
        }

        // Get SM counts from MODS
        GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
        const UINT32 gpcCount = pSubdev->GetGpcCount();
        MASSERT(gpcCount > 0);
        vector<UINT32> modsCounts;
        for (UINT32 virtGpc = 0; virtGpc < gpcCount; virtGpc++)
        {
            modsCounts.push_back(
                pSubdev->GetTpcCountOnGpc(virtGpc) * pSubdev->GetMaxSmPerTpcCount());
        }

        // Check total SM and GPC counts
        const UINT32 caskSMCount = std::accumulate(modsCounts.begin(), modsCounts.end(), 0);
        const UINT32 modsSMCount = std::accumulate(modsCounts.begin(), modsCounts.end(), 0);
        const UINT32 shaderCount = GetBoundLwdaDevice().GetShaderCount();
        if (caskSMCount != shaderCount)
        {
            Printf(Tee::PriError,
                   "Cask HardwareInfo: Reported SM count (%d) != MODS shader count (%d)\n",
                   caskSMCount, shaderCount);
            stickyRc = RC::SOFTWARE_ERROR;
        }
        if (modsSMCount != shaderCount)
        {
            Printf(Tee::PriError,
                   "MODS SM count (%d) != MODS shader count (%d)\n",
                   modsSMCount, shaderCount);
            stickyRc = RC::SOFTWARE_ERROR;
        }
        if (caskCounts.size() != gpcCount)
        {
            Printf(Tee::PriError,
                   "Cask HardwareInfo: Reported GPC count (%zu) != MODS GPC count (%d)\n",
                    caskCounts.size(), gpcCount);
            stickyRc = RC::SOFTWARE_ERROR;
        }
        else
        {
            // Compare skyline
            //
            // The order of the GPCs doesn't really matter in the skyline used by CASK,
            // so sort from smallest to largest before comparing
            std::sort(caskCounts.begin(), caskCounts.end());
            std::sort(modsCounts.begin(), modsCounts.end());
            for (UINT32 i = 0; i < gpcCount; i++)
            {
                if (caskCounts[i] != modsCounts[i])
                {
                    Printf(Tee::PriError,
                           "Cask HardwareInfo: Reported SM count at Index %d (%d) != MODS SM count (%d)\n",
                            i, caskCounts[i], modsCounts[i]);
                    stickyRc = RC::SOFTWARE_ERROR;
                }
            }
        }
    }

    return stickyRc;
}

RC LwdaLinpackCask::InitCaskGemm(cask::Gemm* pGemm, bool initialize)
{
    RC rc;
    const auto* pKernInfo = m_pCaskShader->getInfo();
    MASSERT(pKernInfo);
    MASSERT(pGemm);

    cask::TensorDesc descA;
    cask::TensorDesc descB;
    cask::TensorDesc descC;
    cask::TensorDesc descAm;

    *pGemm = cask::Gemm();

    // Init A/Am/B/C tensor descriptions
    // Add a special case for shmoo/pulse mode
    UINT32 setupKsize = m_Ksize;
    if (m_TestMode == MODE_SHMOO || m_TestMode == MODE_PULSE)
    {
        setupKsize = (m_Ktrue > 0) ? m_Ktrue : m_Ksize;
    }
    const UINT32 adjustedK = (setupKsize / pKernInfo->aTensorType().scalarsPerElement);

    // Init A tensor desciption
    switch (m_LayoutA)
    {
        case cask::md::MatrixLayoutType::N:
            descA = cask::TensorDesc::make_Matrix(
                m_TypeA,
                m_Msize, adjustedK,
                1, m_Msize,
                pKernInfo->aTensorType().scalarsPerElement,
                pKernInfo->aTensorType().vectorizedDim,
                0
            );
            break;
        case cask::md::MatrixLayoutType::T:
            descA = cask::TensorDesc::make_Matrix(
                m_TypeA,
                m_Msize, adjustedK,
                adjustedK, 1,
                pKernInfo->aTensorType().scalarsPerElement,
                pKernInfo->aTensorType().vectorizedDim,
                0
            );
            break;
        default:
            MASSERT(!"UNSUPPORTED KERNEL SHAPE");
            return RC::SOFTWARE_ERROR;
    }
    // Init Am tensor (metadata) description based on A tensor
    if (m_KernParams.isSparse)
    {
        cask::TensorDesc descADense;
        const cask::KernelInfo* pInfo = m_pCaskShader->getKernelInfo();
        cask::getCompressedTensorDesc(*pInfo, descA, descADense, descAm, false);
    }

    // Init B tensor desciption
    switch (m_LayoutB)
    {
        case cask::md::MatrixLayoutType::N:
            descB = cask::TensorDesc::make_Matrix(
                m_TypeB,
                adjustedK, m_Nsize,
                1, adjustedK,
                pKernInfo->bTensorType().scalarsPerElement,
                pKernInfo->bTensorType().vectorizedDim,
                0
            );
            break;
        case cask::md::MatrixLayoutType::T:
            descB = cask::TensorDesc::make_Matrix(
                m_TypeB,
                adjustedK, m_Nsize,
                m_Nsize, 1,
                pKernInfo->bTensorType().scalarsPerElement,
                pKernInfo->bTensorType().vectorizedDim,
                0
            );
            break;
        default:
            MASSERT(!"UNSUPPORTED KERNEL SHAPE");
            return RC::SOFTWARE_ERROR;
    }
    // Init C tensor desciption
    switch (m_LayoutC)
    {
        case cask::md::MatrixLayoutType::N:
            descC = cask::TensorDesc::make_Matrix(
                m_TypeC,
                m_Msize, m_Nsize,
                1, m_Msize,
                pKernInfo->cTensorType().scalarsPerElement,
                pKernInfo->cTensorType().vectorizedDim,
                0
            );
            break;
        case cask::md::MatrixLayoutType::T:
            descC = cask::TensorDesc::make_Matrix(
                m_TypeC,
                m_Msize, m_Nsize,
                m_Nsize, 1,
                pKernInfo->cTensorType().scalarsPerElement,
                pKernInfo->cTensorType().vectorizedDim,
                0
            );
            break;
        default:
            MASSERT(!"UNSUPPORTED KERNEL SHAPE");
            return RC::SOFTWARE_ERROR;
    }

    if (m_KernParams.isSparse)
    {
        pGemm->setAmDesc(descAm);
    }
    pGemm->setADesc(descA);
    pGemm->setBDesc(descB);
    pGemm->setOutputDesc(descC);

    FLOAT32 alpha = 0.0f;
    FLOAT32 beta  = 0.0f;

    // C = alpha * AB + beta * C (C is initialized to A*B, with alpha = 1, beta = 0)
    if (initialize)
    {
        alpha = 1.0f;
        beta  = 0.0f;
    }
    // else, values like alpha = 0.5, beta = 0.5, yields C = 0.5*AB + 0.5*AB = AB once again
    else
    {
        alpha = static_cast<FLOAT32>(m_Alpha);
        beta  = static_cast<FLOAT32>(m_Beta);
    }
    pGemm->setAlpha(alpha);
    pGemm->setBeta(beta);

    if (m_CtaSwizzle)
    {
        UINT32 maxBlocksPerWave = m_MaxBlocksPerSM * GetBoundLwdaDevice().GetShaderCount();
        // The gemm wrapper will callwlate the swizzle when ctasPerWave is set
        pGemm->setCtasPerWave(maxBlocksPerWave);
    }

    // Check that the selected shader can implement the GEMM
    // TODO Check for matrix dimension consistency (lwrrently not working for some IMMA kernels)
    CHECK_CASK_RC(m_pCaskShader->canImplement(*pGemm),
                  Utility::StrPrintf("Shader %s cannot implement GEMM!", GemmFunctionName()),
                  nullptr);
    return rc;
}

RC LwdaLinpackCask::InitCaskBuffers
(
    cask::Gemm& caskGemm,
    cask::RunInfo* pRunInfo,
    Lwca::HostMemory* pHostBuffer,
    Lwca::DeviceMemory* pDeviceBuffer,
    Lwca::DeviceMemory* pDeviceWorkspace
)
{
    RC rc;

    auto& runInfo = *pRunInfo;
    runInfo = {};
    runInfo.op = static_cast<cask::Operation*>(&caskGemm);
    runInfo.caskManagedTensorA = false;
    runInfo.caskManagedTensorAm = false;
    runInfo.caskManagedTensorB = false;
    runInfo.caskManagedBias = false;
    runInfo.caskManagedAlphaBeta = false;
    runInfo.caskManagedReLu = false;
    runInfo.hostBias = nullptr;
    runInfo.hostTensorA = nullptr;
    runInfo.hostTensorAm = nullptr;
    runInfo.hostTensorB = nullptr;
    runInfo.hostAlpha = nullptr;
    runInfo.hostBeta = nullptr;
    runInfo.batchSize = 0;
    runInfo.mode = 0;

    // Init CASK HardwareInformation
    // Accurate HardwareInformation is required by SM90+ GMMA kernels
    cask::HardwareInformation hwInfo = {};
    CHECK_CASK_RC(
        cask::HardwareInformation::queryFromDevice(hwInfo, GetBoundLwdaDevice().GetHandle()),
        "HardwareInformation::queryFromDevice failed!",
        nullptr);

    // Check against known config from MODS
    CHECK_RC(CheckHardwareInfo(hwInfo));

    // Set HardwareInformation
    runInfo.op->setHardwareInfo(hwInfo);

    // Enable clusters if supported (must be called before initHostReservedSpace)
    if (auto& clusterConf = m_pCaskShader->blockClusterSupport())
    {
        dim3 clusterShape = {};
        if (clusterConf.supportsRuntimeClusterConfiguration())
        {
            // Use {1, 2, 1} shape as the default for runtime-configured clusters,
            // since that will run on any floorsweep configuration
            clusterShape = {1, 2, 1};

            // Allow the user to specify the cluster shape
            if (m_ClusterX)
            {
                clusterShape.x = m_ClusterX;
            }
            if (m_ClusterY)
            {
                clusterShape.y = m_ClusterY;
            }
            if (m_ClusterZ)
            {
                clusterShape.z = m_ClusterZ;
            }

            // TODO Print the recommended shape, but don't use it for now
            dim3 recClusterShape = {};
            CHECK_CASK_RC(clusterConf.getRecommendedClusterConfiguration(runInfo, &recClusterShape),
                          "getRecommendedClusterConfiguration failed!",
                          nullptr);
            VerbosePrintf("Recommended Cluster: {%d, %d, %d}\n",
                          recClusterShape.x, recClusterShape.y, recClusterShape.z);

            // Check that the specified cluster shape is supported
            const dim3 maxClusterShape = clusterConf.getMaximumSupportedClusterConfiguration();
            VerbosePrintf("Max Cluster: {%d, %d, %d}\n",
                          maxClusterShape.x, maxClusterShape.y, maxClusterShape.z);
            if (clusterShape.x > maxClusterShape.x ||
                clusterShape.y > maxClusterShape.y ||
                clusterShape.z > maxClusterShape.z)
            {
                Printf(Tee::PriError,
                       "Kernel %s runtime-configured cluster shape {%d, %d, %d) "
                       "exceeds maximum of {%d, %d, %d}\n",
                       GemmFunctionName(),
                       clusterShape.x, clusterShape.y, clusterShape.z,
                       maxClusterShape.x, maxClusterShape.y, maxClusterShape.z);
                return RC::BAD_PARAMETER;
            }

            // Determine oclwpancy for the given cluster shape based on the GPC skyline
            // (floorsweeping config)
            const UINT32 clusterSize = clusterShape.x * clusterShape.y * clusterShape.z;
            UINT32 smCount = 0;
            UINT32 smUsage = 0;
            UINT32 attrIdx = 0;
            for (const auto& gpcAttr : hwInfo.skyline)
            {
                const UINT32 smCountInGpc = gpcAttr.multiProcessorCount();
                const UINT32 smUsageInGpc = ALIGN_DOWN(smCountInGpc, clusterSize);
                smCount += smCountInGpc;
                smUsage += smUsageInGpc;
                if (smCountInGpc == 0)
                {
                    Printf(Tee::PriError, "Skyline reports GPC with 0 SMs\n");
                    return RC::SOFTWARE_ERROR;
                }
                VerbosePrintf("%d: %d/%d SM Used (%d%)\n",
                              attrIdx,
                              smUsageInGpc, smCountInGpc,
                              (100 * smUsageInGpc) / smCountInGpc);
                attrIdx++;
            }
            VerbosePrintf("Total SM Utilization: %d/%d (%.0f%)\n",
                          smUsage, smCount,
                          static_cast<double>((100.0 * smUsage) / smCount));
        }
        else
        {
            // Kernels not supporting runtime cluster config have a fixed cluster shape
            clusterShape = clusterConf.getMaximumSupportedClusterConfiguration();
            if (m_ClusterX || m_ClusterY || m_ClusterZ)
            {
                Printf(Tee::PriError,
                       "Kernel %s has a fixed cluster shape {%d, %d, %d} and "
                       "does not support specifying cluster dimensions.\n",
                       GemmFunctionName(),
                       clusterShape.x, clusterShape.y, clusterShape.z);
                return RC::BAD_PARAMETER;
            }
        }

        // Set cluster shape
        CHECK_CASK_RC(clusterConf.setClusterConfiguration(runInfo, clusterShape),
                      "setClusterConfiguration failed!",
                      nullptr);
        VerbosePrintf("Cluster: {%d, %d, %d}\n", clusterShape.x, clusterShape.y, clusterShape.z);
    }
    else
    {
        // Kernels not supporting GPC-CGA clusters cannot have the cluster shape be specified
        if (m_ClusterX || m_ClusterY || m_ClusterZ)
        {
            Printf(Tee::PriError,
                   "Kernel %s does not use clusters, so cluster dimensions cannot be specified.\n",
                   GemmFunctionName());
            return RC::BAD_PARAMETER;
        }
    }

    // Get Host Buffer Size
    CHECK_CASK_RC(m_pCaskShader->getHostReservedSize(runInfo),
                  "getHostReservedSize failed!",
                  nullptr);
    // Allocate Host Buffer if not yet allocated
    if (!pHostBuffer->GetPtr())
    {
        CHECK_RC(GetLwdaInstance().AllocHostMem(runInfo.hostBufSize, pHostBuffer));
    }
    // Init Host Buffer
    runInfo.hostBuf = pHostBuffer->GetPtr();
    CHECK_CASK_RC(m_pCaskShader->initHostReservedSpace(runInfo),
                  "initHostReservedSpace failed!",
                  nullptr);

    // Get Device Buffer Size
    CHECK_CASK_RC(m_pCaskShader->getDeviceReservedSize(runInfo),
                  "getDeviceReservedSize failed!",
                  runInfo.hostBuf);
    // Allocate Device Buffer if not yet allocated
    if (runInfo.deviceBufSize && !pDeviceBuffer->IsValid())
    {
        CHECK_RC(GetLwdaInstance().AllocDeviceMem(runInfo.deviceBufSize, pDeviceBuffer));
    }
    // Init Device Buffer
    runInfo.deviceBuf = reinterpret_cast<void*>(pDeviceBuffer->GetDevicePtr());
    CHECK_CASK_RC(m_pCaskShader->initDeviceReservedSpace(
                      runInfo,
                      GetLwdaInstance().GetStream(GetBoundLwdaDevice()).GetHandle()),
                  "initDeviceReservedSpace failed!",
                  runInfo.hostBuf);

    // Allocate Device Workspace if not yet allocated
    // (must be called after init of reserved spaces)
    const UINT64 workspaceSizeBytes = m_pCaskShader->getDeviceWorkspaceSize(runInfo);
    if (workspaceSizeBytes > 0 && !pDeviceWorkspace->IsValid())
    {
        CHECK_RC(GetLwdaInstance().AllocDeviceMem(workspaceSizeBytes, pDeviceWorkspace));
    }

    // Check ReportFailingSmids support by querying the SMID map size
    //
    // We'd like to do this in AllocateSmidBuffers but
    // the current test ordering doesn't allow for that
    if (m_ReportFailingSmids)
    {
        const UINT64 modsSmidSize = m_HostSmidMap.GetSize();
        const UINT64 caskSmidSize = static_cast<UINT64>(m_pCaskShader->getSMidSize(runInfo));
        if (caskSmidSize != modsSmidSize)
        {
            Printf(Tee::PriError,
                   "Returned SMID map size (%llu) not equal to value required by test (%llu)\n"
                   "ReportFailingSmids may not be supported for this kernel variant\n",
                    caskSmidSize, modsSmidSize);
            return RC::BAD_PARAMETER;
        }
    }
    return rc;
}

RC LwdaLinpackCask::Setup()
{
    RC rc;

    // Initialize LWCA runtime and CASK if not already initialized
    CHECK_RC(Lwca::InitLwdaRuntime(GetBoundLwdaDevice()));
    {
        Tasker::MutexHolder lock(s_pCaskInitMutex);
        cask::initCask();
        // TODO Manifest CASK code should be updated to only init once
        if (!s_ManifestInit)
        {
            cask::initCaskKernels(*(LwdaLinpackCask::s_pManifest));
            s_ManifestInit = true;
        }
    }

    // Main test setup
    CHECK_RC(SetCaskShader());
    CHECK_RC(SetupCaskParams());
    CHECK_RC(LwdaLinpack::Setup());
    CHECK_RC(SetupCaskKernel());

    // Initialize Matrix scaling function if needed
    if (m_KernParams.typeofOutputElement == R_32I)
    {
        m_ScaleMatrixFunc = m_Module.GetFunction("ScaleMatrixI");
        CHECK_RC(m_ScaleMatrixFunc.InitCheck());
        m_ScaleMatrixFunc.SetGridDim(m_NumBlocks);
        m_ScaleMatrixFunc.SetBlockDim(m_NumThreads);
    }
    return rc;
}

RC LwdaLinpackCask::Cleanup()
{
    StickyRC stickyRc;

    m_CaskInitHostBuffer.Free();
    m_CaskInitDeviceBuffer.Free();
    m_CaskInitDeviceWorkspace.Free();

    m_CaskHostBuffer.Free();
    m_CaskDeviceBuffer.Free();
    m_CaskDeviceWorkspace.Free();

    stickyRc = LwdaLinpack::Cleanup();
    stickyRc = Lwca::FreeLwdaRuntime(GetBoundLwdaDevice());
    return stickyRc;
}

RC LwdaLinpackCask::GenerateCMatrices(double *pEstTimeMsForOneLoop)
{
    RC rc;
    // Init CASK GEMM
    CHECK_RC(InitCaskGemm(&m_CaskInitGemm, true));
    CHECK_RC(InitCaskGemm(&m_CaskGemm, false));

    // We may need to re-init some state so cask will pick up the new M/N/K size
    CHECK_RC(InitCaskBuffers(
        m_CaskInitGemm,
        &m_CaskInitRunInfo,
        &m_CaskInitHostBuffer,
        &m_CaskInitDeviceBuffer,
        &m_CaskInitDeviceWorkspace));
    CHECK_RC(InitCaskBuffers(
        m_CaskGemm,
        &m_CaskRunInfo,
        &m_CaskHostBuffer,
        &m_CaskDeviceBuffer,
        &m_CaskDeviceWorkspace));

    return LwdaLinpack::GenerateCMatrices(pEstTimeMsForOneLoop);
}

//-----------------------------------------------------------------------------------
//! \brief Calls LWCA kernel to run GEMM
//!
RC LwdaLinpackCask::RunGemm(UINT64 A, UINT64 B, UINT64 C, bool init)
{
    UINT64 Am = 0;
    RC rc;

    if (m_KernParams.isSparse)
    {
        Am = CalcMatrixAmPtr();
    }

    // TODO For now we store the params as a member variable
    // Replace this later with a more robust approach
    m_TmpRunParams = { init, A, Am, B, C };
    ByteStream paramBuffer;

    return Launch(paramBuffer);
}

JS_CLASS_INHERIT(LwdaLinpackCask, LwdaLinpack, "Run CASK-derived GEMM tests.");
CLASS_PROP_READWRITE_FULL(LwdaLinpackCask, DefaultKernelName, string, "Default CASK kernel name set by CaskLinpack variant", 0, 0);
CLASS_PROP_READWRITE(LwdaLinpackCask, KernelName, string, "Full CASK kernel name");
CLASS_PROP_READWRITE(LwdaLinpackCask, ClusterX, UINT32, "GPC-CGA cluster width");
CLASS_PROP_READWRITE(LwdaLinpackCask, ClusterY, UINT32, "GPC-CGA cluster height");
CLASS_PROP_READWRITE(LwdaLinpackCask, ClusterZ, UINT32, "GPC-CGA cluster depth");
CLASS_PROP_READWRITE(LwdaLinpackCask, PrintKernelList, bool,
    "Dump the list of all kernels usable by the current CASK test variant");
CLASS_PROP_READWRITE(LwdaLinpackCask, PrintPackagedKernels, bool,
    "Dump the list of all kernels packaged with CASK (for debugging)");
