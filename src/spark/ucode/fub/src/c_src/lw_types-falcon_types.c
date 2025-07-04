#define IN_ADA_BODY
#include "lw_types-falcon_types.h"

const lw_types__lwu32 lw_types__falcon_types__max_falcon_imem_size_bytes = 65536U;
const lw_types__lwu32 lw_types__falcon_types__max_falcon_imem_offset_bytes_for_last_dword_datum = 65532U;
const lw_types__lwu16 lw_types__falcon_types__max_num_of_imem_tags = 256U;
const lw_types__lwu16 lw_types__falcon_types__max_falcon_imem_tag_index = 255U;
const lw_types__lwu32 lw_types__falcon_types__max_falcon_dmem_size_bytes = 65536U;
const lw_types__lwu32 lw_types__falcon_types__max_falcon_dmem_offset_bytes = 65532U;
typedef lw_types__Tlwu16B lw_types__falcon_types__Tucode_imem_tagB;
typedef lw_types__falcon_types__Tucode_imem_tagB lw_types__falcon_types__ucode_imem_tag;
typedef lw_types__Tlwu32B lw_types__falcon_types__Tucode_imem_offset_in_falcon_bytesB;
typedef lw_types__falcon_types__Tucode_imem_offset_in_falcon_bytesB lw_types__falcon_types__ucode_imem_offset_in_falcon_bytes;
typedef lw_types__falcon_types__Tucode_imem_offset_in_falcon_bytesB lw_types__falcon_types__ucode_imem_offset_in_falcon_4b_aligned_bytes;
extern boolean lw_types__falcon_types__ucode_imem_offset_in_falcon_4b_aligned_bytesPredicate(lw_types__falcon_types__ucode_imem_offset_in_falcon_4b_aligned_bytes I13s);
typedef lw_types__Tlwu32B lw_types__falcon_types__Tucode_dmem_offset_in_falcon_bytesB;
typedef lw_types__falcon_types__Tucode_dmem_offset_in_falcon_bytesB lw_types__falcon_types__ucode_dmem_offset_in_falcon_bytes;
typedef lw_types__falcon_types__Tucode_dmem_offset_in_falcon_bytesB lw_types__falcon_types__ucode_dmem_offset_in_falcon_4b_aligned_bytes;
extern boolean lw_types__falcon_types__ucode_dmem_offset_in_falcon_4b_aligned_bytesPredicate(lw_types__falcon_types__ucode_dmem_offset_in_falcon_4b_aligned_bytes I15s);
typedef lw_types__Tlwu32B lw_types__falcon_types__Tucode_imem_size_in_falcon_bytesB;
typedef lw_types__falcon_types__Tucode_imem_size_in_falcon_bytesB lw_types__falcon_types__ucode_imem_size_in_falcon_bytes;
typedef lw_types__falcon_types__Tucode_imem_size_in_falcon_bytesB lw_types__falcon_types__ucode_imem_size_in_falcon_4b_aligned_bytes;
extern boolean lw_types__falcon_types__ucode_imem_size_in_falcon_4b_aligned_bytesPredicate(lw_types__falcon_types__ucode_imem_size_in_falcon_4b_aligned_bytes I17s);
typedef lw_types__Tlwu32B lw_types__falcon_types__Tucode_dmem_size_in_falcon_bytesB;
typedef lw_types__falcon_types__Tucode_dmem_size_in_falcon_bytesB lw_types__falcon_types__ucode_dmem_size_in_falcon_bytes;
typedef lw_types__falcon_types__Tucode_dmem_size_in_falcon_bytesB lw_types__falcon_types__ucode_dmem_size_in_falcon_4b_aligned_bytes;
extern boolean lw_types__falcon_types__ucode_dmem_size_in_falcon_4b_aligned_bytesPredicate(lw_types__falcon_types__ucode_dmem_size_in_falcon_4b_aligned_bytes I19s);
typedef lw_types__Tlwu32B lw_types__falcon_types__Tucode_desc_dmem_offsetB;
typedef lw_types__falcon_types__Tucode_desc_dmem_offsetB lw_types__falcon_types__ucode_desc_dmem_offset;
typedef lw_types__falcon_types__Tucode_desc_dmem_offsetB lw_types__falcon_types__ucode_desc_dmem_4b_aligned_offset;
extern boolean lw_types__falcon_types__ucode_desc_dmem_4b_aligned_offsetPredicate(lw_types__falcon_types__ucode_desc_dmem_4b_aligned_offset I21s);
#ifdef IN_ADA_BODY
boolean lw_types__falcon_types__ucode_imem_offset_in_falcon_4b_aligned_bytesPredicate(lw_types__falcon_types__ucode_imem_offset_in_falcon_4b_aligned_bytes I13s) {
  return ((I13s % 4U) == 0U);
}
#endif /* IN_ADA_BODY */
#ifdef IN_ADA_BODY
boolean lw_types__falcon_types__ucode_dmem_offset_in_falcon_4b_aligned_bytesPredicate(lw_types__falcon_types__ucode_dmem_offset_in_falcon_4b_aligned_bytes I15s) {
  return ((I15s % 4U) == 0U);
}
#endif /* IN_ADA_BODY */
#ifdef IN_ADA_BODY
boolean lw_types__falcon_types__ucode_imem_size_in_falcon_4b_aligned_bytesPredicate(lw_types__falcon_types__ucode_imem_size_in_falcon_4b_aligned_bytes I17s) {
  return ((I17s % 4U) == 0U);
}
#endif /* IN_ADA_BODY */
#ifdef IN_ADA_BODY
boolean lw_types__falcon_types__ucode_dmem_size_in_falcon_4b_aligned_bytesPredicate(lw_types__falcon_types__ucode_dmem_size_in_falcon_4b_aligned_bytes I19s) {
  return ((I19s % 4U) == 0U);
}
#endif /* IN_ADA_BODY */
#ifdef IN_ADA_BODY
boolean lw_types__falcon_types__ucode_desc_dmem_4b_aligned_offsetPredicate(lw_types__falcon_types__ucode_desc_dmem_4b_aligned_offset I21s) {
  return ((I21s % 4U) == 0U);
}
#endif /* IN_ADA_BODY */
