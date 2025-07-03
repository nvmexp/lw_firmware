#ifndef __FT_SASS_GEMM_KERNEL_RECORD_HPP__
#define __FT_SASS_GEMM_KERNEL_RECORD_HPP__

#include "ft_common_types.hpp"

namespace cask {
namespace sass_gemm_kr {

enum layout_t { N = 0, T, C, LAYOUT_COUNT };
enum shape_t { RECT = 0, UPPER, LOWER, SHAPE_COUNT };
enum slice_t { SLICE_NONE = 0, SLICE_1X2, SLICE_1X4, SLICE_COUNT };
enum stage_t { STAGE_NONE = 0, STAGE_16X2, STAGE_32X1, STAGE_64X1, STAGE_COUNT };
enum n_residue_t { N_RESIDUE_GENERAL = 0, N_RESIDUE_0_MOD_32, N_RESIDUE_COUNT };

struct sass_gemm_kernel_record_t {
    kr::spa_t spa;
    kr::version_t ver;

    kr::datatype_t a;
    kr::datatype_t b;
    kr::datatype_t c;
    kr::datatype_t aclwm;
    kr::datatype_t bias;

    layout_t a_lay;
    layout_t b_lay;
    layout_t c_lay;

    shape_t shp;
    slice_t slc;
    stage_t stg;

    kr::tensor_t tns;
    n_residue_t n_res;

    kr::relubias_support_t rb;

    kr::ldga_t ldga;
    kr::ldgb_t ldgb;

    const char *shader_name;
};

const char *const layout_strings[] = {"N", "T", "C"};

const char *const shape_strings[]     = {"RECT", "UPPER", "LOWER"};
const char *const slice_strings[]     = {"NONE", "1X2", "1X4"};
const char *const stage_strings[]     = {"NONE", "16X2", "32X1", "64X1"};
const char *const n_residue_strings[] = {"N_RESIDUE_GENERAL", "N_RESIDUE_0_MOD_32"};

Error
colwertLayout(const GemmLayout &gl, layout_t *v);
Error
colwertLayout(const sass_private::layoutType_t &gl, layout_t *v);
Error
colwertShape(const sass_private::shapeTypeC_t &c, shape_t *v);
Error
colwertSlice(const sass_private::ShaderParams *sp, slice_t *v);
Error
colwertStage(const sass_private::ShaderParams *sp, stage_t *v);

}  // sass_gemm_kr
}  // cask

#endif
