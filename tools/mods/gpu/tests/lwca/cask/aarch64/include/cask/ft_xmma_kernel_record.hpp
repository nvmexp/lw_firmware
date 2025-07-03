#ifndef __FT_XMMA_KERNEL_RECORD_HPP__
#define __FT_XMMA_KERNEL_RECORD_HPP__

#include "ft_common_types.hpp"
#include "ft_common_colwolution_types.hpp"

namespace cask {
namespace xmmakr {  // xmma kernel record

enum interleaved_t { INTERLEAVED_NO = 0, INTERLEAVED_YES, INTERLEAVED_COUNT };
enum cta_splitk_t { CTA_SPLITK_NO = 0, CTA_SPLITK_YES, CTA_SPLITK_COUNT };

struct xmma_kernel_record_t {
    kr::spa_t spa;
    krcc::colw_t colw;

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

    krcc::idxing_t idxing;
    interleaved_t ilv;
    cta_splitk_t csk;

    krcc::filter_k_residue_t filt_k_res;
    krcc::input_c_residue_t inp_c_res;
    krcc::kblock_t kb;

    krcc::group_support_t gp;
    kr::relubias_support_t rb;
    krcc::stages_t stage;

#if defined(FT_AMPERE_TREE_LEVELS)
    kr::abi_ver_t abi;
#endif
    const char *shader_name;
};

const char *const interleaved_strings[] = {"INTERLEAVED_NO", "INTERLEAVED_YES"};
const char *const cta_splitk_strings[]  = {"CTA_SPLITK_NO", "CTA_SPLITK_YES"};

}  // xkr
}  // cask

#endif
