#ifndef __FT_LWTLASS_KERNEL_RECORD_HPP__
#define __FT_LWTLASS_KERNEL_RECORD_HPP__

#include "ft_common_types.hpp"
#include "ft_common_colwolution_types.hpp"

namespace cask {
namespace lwtkr {  // lwtlass kernel record

struct lwtlass_kernel_record_t {
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

    krcc::group_support_t gp;
    kr::relubias_support_t rb;

#if defined(FT_AMPERE_TREE_LEVELS)
    kr::abi_ver_t abi;
#endif
    const char *shader_name;
};

}  // lwtkr
}  // cask

#endif
