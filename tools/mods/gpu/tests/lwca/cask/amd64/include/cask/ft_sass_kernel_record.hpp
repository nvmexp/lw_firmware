#ifndef __FT_SASS_KERNEL_RECORD_HPP__
#define __FT_SASS_KERNEL_RECORD_HPP__

#include "ft_common_types.hpp"
#include "ft_common_colwolution_types.hpp"

namespace cask {
namespace sasskr {

enum gemm_t {
    GEMM_LWDNN = 0,
    GEMM_LWDNN_884,
    GEMM_LWDNN_1688,
    GEMM_LWDNN_16816,
    GEMM_LWDNN_8816,
    GEMM_LWDNN_16832,
    GEMM_LWDNN_1684,
    GEMM_WINOGRAD,
    GEMM_GEMM,
    GEMM_COUNT
};
enum strided_b_t { STRIDED_B_NO = 0, STRIDED_B_YES, STRIDED_B_COUNT };
enum split_k_t { SPLITK_NO = 0, SPLITK_YES, SPLITK_COUNT };
enum beta_t { BETA_GENERAL = 0, BETA_ZERO, BETA_ONE, BETA_ZERO_OR_ONE, BETA_COUNT };
enum edge_t { EDGE_INTERIOR = 0, EDGE_SMALL, EDGE_MEDIUM, EDGE_LARGE, EDGE_NONE, EDGE_COUNT };
enum dilation_t { DILATION_GENERAL = 0, DILATION_1X1, DILATION_COUNT };  // document this
enum dynamic_resize_t { DYNAMIC_RESIZE_NO = 0, DYNAMIC_RESIZE_YES, DYNAMIC_RESIZE_COUNT };
enum batch_n_residue_t { BATCH_GENERAL = 0, BATCH_0_MOD_2, BATCH_0_MOD_4, BATCH_COUNT };
enum sbuf_t { BUF_DEFAULT = 0, BUF_SINGLE, BUF_COUNT };
enum sliced_t { SLICED_NO = 0, SLICED_YES, SLICED_COUNT };
enum nd_support_t { ND_NO = 0, ND_YES, ND_COUNT };

struct sass_kernel_record_t {
    kr::spa_t spa;
    krcc::colw_t colw;
    gemm_t gemm;
    kr::version_t ver;

    kr::datatype_t inp;
#if defined(FT_AMPERE_TREE_LEVELS)
    kr::datatype_t inp_am;
#endif
    kr::datatype_t flt;
    kr::datatype_t out;
    kr::datatype_t aclwm;
    kr::datatype_t bias;

    krcc::layout_t inp_fmt;
    krcc::layout_t flt_fmt;
    krcc::layout_t out_fmt;

    krcc::vect_t inp_vect;
    krcc::vect_t flt_vect;
    krcc::vect_t out_vect;

    krcc::vect_scalars_t inp_vect_scalars;
    krcc::vect_scalars_t flt_vect_scalars;
    krcc::vect_scalars_t out_vect_scalars;

    kr::tensor_t tns;

    strided_b_t strd_b;
    krcc::idxing_t idxing;
    split_k_t splitk;

    beta_t beta;

    edge_t edge;
    krcc::pad_t pad;
    dilation_t dil;
    krcc::stride_t strd;

    krcc::filter_sz_t filt_sz;
    krcc::filter_k_residue_t filt_k_res;
    krcc::input_c_residue_t inp_c_res;

    krcc::group_support_t gp;
    kr::relubias_support_t rb;

    kr::ldga_t ldga;
    kr::ldgb_t ldgb;
    krcc::kblock_t kb;

    dynamic_resize_t dr;
    batch_n_residue_t bn;
#if defined(FT_AMPERE_TREE_LEVELS)
    kr::abi_ver_t abi;
#endif

    sbuf_t buf;
    sliced_t slc;
    krcc::stages_t stage;
    nd_support_t nd;

    const char *shader_name;
};

const char *const gemm_strings[] =
    {"LWDNN", "LWDNN_884", "LWDNN_1688", "LWDNN_16816", "LWDNN_8816", "LWDNN_16832", "LWDNN_1684", "WINOGRAD", "GEMM"};
const char *const strided_b_strings[] = {"STRIDED_B_NO", "STRIDED_B_YES"};

const char *const splitk_strings[]          = {"SPLITK_NO", "SPLITK_YES"};
const char *const beta_strings[]            = {"BETA_GENERAL", "BETA_ZERO", "BETA_ONE", "BETA_ZERO_OR_ONE"};
const char *const edge_strings[]            = {"EDGE_INTERIOR", "EDGE_SMALL", "EDGE_MEDIUM", "EDGE_LARGE", "EDGE_NONE"};
const char *const dilation_strings[]        = {"DILATION_GENERAL", "DILATION_1X1"};
const char *const dynamic_resize_strings[]  = {"DYNAMIC_RESIZE_NO", "DYNAMIC_RESIZE_YES"};
const char *const batch_n_residue_strings[] = {"BATCH_GENERAL", "BATCH_0_MOD_2", "BATCH_0_MOD_4"};
const char *const sbuf_strings[]            = {"BUF_DEFAULT", "BUF_SINGLE"};
const char *const sliced_strings[]          = {"SLICED_NO", "SLICED_YES"};
const char *const nd_support_strings[]      = {"ND_NO", "ND_YES"};

Error
colwertEdges(sass_private::edgeType_t e, edge_t *v);

Error
colwertTileN(int tile_n, batch_n_residue_t *v);

Error
colwertGemm(sass_private::gemmType_t val, gemm_t *v);

}  // sasskr (sass kernel record)
}  // cask

#endif
