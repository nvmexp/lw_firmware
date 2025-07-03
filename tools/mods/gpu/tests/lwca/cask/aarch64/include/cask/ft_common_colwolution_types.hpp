#ifndef __FT_COMMON_COLWOLUTION_TYPES_HPP__
#define __FT_COMMON_COLWOLUTION_TYPES_HPP__

namespace cask {
namespace krcc {  // kernel record colwolution common

enum colw_t { COLW_FPROP = 0, COLW_DGRAD, COLW_WGRAD, COLW_COUNT };
enum layout_t { NCHW = 0, NHWC, LAYOUT_COUNT };
enum vect_t { VECT_NONE = 0, VECT_C, VECT_N, VECT_COUNT };
enum vect_scalars_t { VECT_1 = 0, VECT_2, VECT_4, VECT_32, VECT_SCALARS_COUNT };
enum idxing_t { INDEXING_NO = 0, INDEXING_YES, INDEXING_COUNT };
enum pad_t { PAD_ZERO = 0, PAD_GENERAL, PAD_COUNT };
enum stride_t { STRIDE_GENERAL = 0, STRIDE_1X1, STRIDE_SYMMETRIC, STRIDE_COUNT };
enum filter_sz_t { FILT_GENERAL = 0, FILT_1X1, FILT_3X3, FILT_SZ_COUNT };

enum filter_k_residue_t { FILTER_K_GENERAL = 0, FILTER_K_0_MOD_32, FILTER_K_0_MOD_8, FILTER_K_COUNT };
enum input_c_residue_t { INPUT_C_GENERAL = 0, INPUT_C_0_MOD_8, INPUT_C_COUNT };
enum group_support_t { GROUP_SUPPORT_NONE = 0, GROUP_SUPPORT_GENERAL, GROUP_SUPPORT_COUNT };
enum kblock_t { KBLOCK_4 = 0, KBLOCK_8, KBLOCK_16, KBLOCK_32, KBLOCK_64, KBLOCK_COUNT };
enum stages_t { STAGES_1 = 0, STAGES_3, STAGES_4, STAGES_5, STAGES_6, STAGES_DEFAULT, STAGES_COUNT };

const char *const colw_strings[]             = {"FPROP", "DGRAD", "WGRAD"};
const char *const layout_strings[]           = {"NCHW", "NHWC"};
const char *const vect_strings[]             = {"VECT_NONE", "VECT_C", "VECT_N"};
const char *const vect_scalars_strings[]     = {"VECT_1", "VECT_2", "VECT_4", "VECT_32"};
const char *const indexing_strings[]         = {"INDEXING_NO", "INDEXING_YES"};
const char *const pad_strings[]              = {"PAD_ZERO", "PAD_GENERAL"};
const char *const stride_strings[]           = {"STRIDE_GENERAL", "STRIDE_1X1", "STRIDE_SYMMETRIC"};
const char *const filter_sz_strings[]        = {"FILT_GENERAL", "FILT_1X1", "FILT_3X3"};
const char *const filter_k_residue_strings[] = {"FILTER_K_GENERAL", "FILTER_K_0_MOD_32", "FILTER_K_0_MOD_8"};
const char *const input_c_residue_strings[]  = {"INPUT_C_GENERAL", "INPUT_C_0_MOD_8"};
const char *const group_support_strings[]    = {"GROUP_SUPPORT_NONE", "GROUP_SUPPORT_GENERAL"};
const char *const kblock_strings[]           = {"KBLOCK_4", "KBLOCK_8", "KBLOCK_16", "KBLOCK_32", "KBLOCK_64"};
const char *const stage_strings[] = {"STAGES_1", "STAGES_3", "STAGES_4", "STAGES_5", "STAGES_6", "STAGES_DEFAULT"};

void
print_colw(colw_t v);
void
print_layout(layout_t v);
void
print_vect(vect_t v);
void
print_vect_s(vect_scalars_t v);
void
print_kb(kblock_t v);
void
print_pad(pad_t v);
void
print_stride(stride_t v);
void
print_filtersz(filter_sz_t v);
void
print_filtkres(filter_k_residue_t v);
void
print_inpcres(input_c_residue_t v);
void
print_group(group_support_t v);
void
print_idxing(idxing_t v);
void
print_stage(stages_t val);

Error
colwertVectorizedDim(int tensor_desc_dim, vect_t *v);
Error
colwertScalarsPerElement(int scalars_per_element, vect_scalars_t *v);
Error
colwertKBlock(uint16_t ldg, kblock_t *v);
Error
colwertStages(size_t stage, stages_t *v);
}  // krcc
}  // cask

#endif
