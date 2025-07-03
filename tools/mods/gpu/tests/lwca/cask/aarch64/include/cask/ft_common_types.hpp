#ifndef __FT_COMMON_TYPES_HPP__
#define __FT_COMMON_TYPES_HPP__

#include <cstdint>

namespace cask {
enum record_type_t {
    KR_SASS = 0,
    KR_SASS_GEMM,
    KR_GEMM_COLW1X1,
    KR_FIRST_LAYER,
    KR_DIRECT_GROUP,
    KR_XMMA,
    KR_LWTLASS,
    KR_COUNT
};

namespace kr {

// the set of supported SPAs for a given kernel
enum spa_t {
    SPA_SM_50_SM_52_SM_53_SM_60_SM_61_SM_62 = 0,
    SPA_SM_53_SM_60_SM_62,
    SPA_SM_61,
    SPA_SM_70_SM_72_SM_75,
    SPA_SM_72_SM_75,
    SPA_SM_75,
    SPA_SM_80,
    SPA_SM_90,
    SPA_COUNT
};

enum version_t { VER_0 = 0, VER_1, VER_COUNT };
enum tensor_t { TENSOR_NO = 0, TENSOR_YES, TENSOR_COUNT };
enum relubias_support_t { RELUBIAS_NO = 0, RELUBIAS_YES, RELUBIAS_COUNT };
enum datatype_t {
    R_16F = 0,
    C_16F,
    R_32F,
    C_32F,
    R_64F,
    C_64F,
    R_8I,
    C_8I,
    R_8U,
    C_8U,
    R_32I,
    C_32I,
    R_32U,
    C_32U,
    BF16_16F,
    R_16U,
    DATATYPE_COUNT
};
enum ldga_t { LOG2_LDGA_0 = 0, LOG2_LDGA_1, LOG2_LDGA_2, LOG2_LDGA_3, LOG2_LDGA_4, LOG2_LDGA_COUNT };
enum ldgb_t { LOG2_LDGB_0 = 0, LOG2_LDGB_2, LOG2_LDGB_3, LOG2_LDGB_4, LOG2_LDGB_COUNT };
#if defined(FT_AMPERE_TREE_LEVELS)
enum abi_ver_t {
    ABI_AMPERE_G = 0,
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
    ABI_COUNT
};
#endif
enum leaf_t { LEAF = 0, LEAF_COUNT };

// corresponding strings
const char *const arch_strings[] = {"MAXWELL_PASCAL", "VOLTA", "TURING", "AMPERE"};
const char *const spa_strings[]  = {
    "SPA_SM_50_SM_52_SM_53_SM_60_SM_61_SM_62",
    "SPA_SM_53_SM_60_SM_62",
    "SPA_SM_61",
    "SPA_SM_70_SM_72_SM_75",
    "SPA_SM_72_SM_75",
    "SPA_SM_75",
    "SPA_SM_80",
};

const char *const version_strings[]  = {"VER_0", "VER_1"};
const char *const datatype_strings[] = {"R_16F",
                                        "C_16F",
                                        "R_32F",
                                        "C_32F",
                                        "R_64F",
                                        "C_64F",
                                        "R_8I",
                                        "C_8I",
                                        "R_8U",
                                        "C_8U",
                                        "R_32I",
                                        "C_32I",
                                        "R_32U",
                                        "C_32U",
                                        "BF16_16F",
                                        "R_16U"};
const char *const tensor_strings[]           = {"TENSOR_NO", "TENSOR_YES"};
const char *const relubias_support_strings[] = {"RELUBIAS_NO", "RELUBIAS_YES"};
const char *const ldga_strings[] = {"LOG2_LDGA_0", "LOG2_LDGA_1", "LOG2_LDGA_2", "LOG2_LDGA_3", "LOG2_LDGA_4"};
const char *const ldgb_strings[] = {"LOG2_LDGB_0", "LOG2_LDGB_2", "LOG2_LDGB_3", "LOG2_LDGB_4"};
#if defined(FT_AMPERE_TREE_LEVELS)
const char *const abi_ver_strings[] = {
    "ABI_AMPERE_G",
    "ABI_AMPERE_WINOG",
    "ABI_AMPERE_WGRAD",
    "ABI_AMPERE_IGINTERIOR",
    "ABI_AMPERE_IGSMALL",
    "ABI_AMPERE_IGMEDIUM",
    "ABI_AMPERE_IGLARGE",
    "ABI_PREAMPERE_G",
    "ABI_PREAMPERE_WINOG",
    "ABI_PREAMPERE_WGRAD",
    "ABI_PREAMPERE_IGINTERIOR",
    "ABI_PREAMPERE_IGSMALL",
    "ABI_PREAMPERE_IGMEDIUM",
    "ABI_PREAMPERE_IGLARGE",
};
#endif
const char *const leaf_strings[] = {"LEAF"};

// helper functions
Error
colwertSPAFlags(kr::spa_t *v, uint64_t spa);

Error
colwertScalarType(const ScalarType &st, kr::datatype_t *);
Error
colwertDatatype(sass_private::elementDataType_t *d, kr::datatype_t v);
Error
colwertElementDatatype(kr::datatype_t *v, sass_private::elementDataType_t d);
#if defined(FT_AMPERE_TREE_LEVELS)
Error
colwertABI(const sass_private::abiVersion_t abiVersion, kr::abi_ver_t *v);
#endif
Error
colwertLdgA(uint8_t ldg, kr::ldga_t *v);
Error
colwertLdgB(uint8_t ldg, kr::ldgb_t *v);
bool
isAmpere(kr::spa_t v);
}
}

#endif
