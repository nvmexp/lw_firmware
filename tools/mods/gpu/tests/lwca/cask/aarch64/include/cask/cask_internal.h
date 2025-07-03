#ifndef INCLUDE_GUARD_CASK_INTERNAL_H
#define INCLUDE_GUARD_CASK_INTERNAL_H

#include <cstdio>

#include <cask/cask.h>

namespace cask {

// Utils for Sparse Tensor

struct SparseTensorsParams
{
    TensorDesc sparseTD;
    void *sparseData{nullptr};
    TensorDesc denseTD;
    void *denseData{nullptr};
    TensorDesc mdTD;
    void *mdData{nullptr};
    int64_t mdOffset{0};
    void *scratchBuffer{nullptr};
    int64_t groups{1};
    int64_t arrayCount{0};
};

void uncompressSparseTensor(SparseTensorsParams &params, bool isVerticalSparsity, lwdaStream_t stream);
void compressSparseTensor(SparseTensorsParams &compressParams, const KernelInfo &ki, bool isVerticalSparsity, bool needReorder, lwdaStream_t stream);
void compressSparseTensor(SparseTensorsParams &compressParams, uint32_t ctaBlockRows, bool isVerticalSparsity, bool needReorder, lwdaStream_t stream);
void compressSparseTensor(SparseTensorsParams &compressParams, bool isVerticalSparsity, bool needReorder, lwdaStream_t stream);
//!< Deprecated: Use compressSparseTensor(SparseTensorsParams&, uint32_t ctaBlockRows, bool isVerticalSparsity, bool needReorder)
//! to explicitly set the CTA block row size. This methods applies ctaBlockRows as 128.

void getCompressedTensorDesc(const KernelInfo &ki, TensorDesc inputDesc, TensorDesc &compressedDesc, TensorDesc &compressedMetaDesc, bool isB=false);

void compressSparseFilter(SparseTensorsParams &params, bool needReorder, lwdaStream_t stream);
void uncompressSparseFilter(SparseTensorsParams &params, lwdaStream_t stream);
void getCompressedFilterDesc(const KernelInfo &ki, TensorDesc inputDesc, TensorDesc &compressedDesc, TensorDesc &compressedMetaDesc, int64_t groups);
void compressFilterGenerateMetadata(void *in, void *compressedFilter, void *metaData, const int64_t *dim); 
void reorderCompressedFilter(void *in, void *out, const int64_t *dim); 

}

#endif
