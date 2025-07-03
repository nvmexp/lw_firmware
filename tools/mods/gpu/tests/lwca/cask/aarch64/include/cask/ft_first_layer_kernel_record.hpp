#ifndef __FT_FIRST_LAYER_KERNEL_RECORD_HPP__
#define __FT_FIRST_LAYER_KERNEL_RECORD_HPP__

#include "ft_common_types.hpp"
#include "ft_common_colwolution_types.hpp"

namespace cask {
namespace flkr {

enum fl_filter_pad_t { FL_FILT_3X3_PAD_1X1 = 0, FL_FILT_5X5_PAD_2X2, FL_FILT_7X7_PAD_3X3, FL_FILTER_PAD_COUNT };

enum fl_filter_k_t {
    FL_FILTER_K_16 = 0,
    FL_FILTER_K_32,
    FL_FILTER_K_64,
    FL_FILTER_K_COUNT,
};

enum fl_input_c_t { FL_INPUT_C_4 = 0, FL_INPUT_C_8, FL_INPUT_C_COUNT };

struct fl_kernel_record_t {
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

    fl_filter_pad_t fltpad;
    fl_filter_k_t filtk;
    fl_input_c_t inpc;

    krcc::group_support_t gp;
    kr::relubias_support_t rb;

#if defined(FT_AMPERE_TREE_LEVELS)
    kr::abi_ver_t abi;
#endif
    const char *shader_name;
};

const char *const fl_filter_pad_strings[] = {"FL_FILT_3X3_PAD_1X1", "FL_FILT_5X5_PAD_2X2", "FL_FILT_7X7_PAD_3X3"};
const char *const fl_filter_k_strings[]   = {"FL_FILTER_K_16", "FL_FILTER_K_32", "FL_FILTER_K_64"};
const char *const fl_input_c_strings[]    = {"FL_INPUT_C_4", "FL_INPUT_C_8"};
}
}
#endif
