#ifndef INCLUDE_GUARD_CASK_OPERATION_GETT_H
#define INCLUDE_GUARD_CASK_OPERATION_GETT_H
// enable doxygen:
//! @file

#include <lwComplex.h>
#include <lwda_runtime.h>
#include <lwca.h>
#include <limits>

#include "cask/cask_core.h"
#include "cask/evict_descriptor.h"
#include "cask/tensor_desc.h"
#include "generated/cask/problem_descs.h"

namespace cask {

/**
 * \brief Gett pure virtual base class to describe callwlations
 * performed by corresponding Shader classes.
 *
 * Gett provides minimal functionality beyond a default
 * Description definition targeted at GETT.
 */
class
Gett {
public:

    using Description = cask::problem_descs::Gett;

    Gett() {}
    ~Gett() {}
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the descriptor for the first input ("A") matrix.
    /////////////////////////////////////////////////////////////////////////
    inline void setADesc(const TensorDesc &inADesc)     { mDescription.a    = inADesc; }
    inline void setBDesc(const TensorDesc &inBDesc)     { mDescription.b    = inBDesc; }
    inline void setCDesc(const TensorDesc &inCDesc)     { mDescription.c    = inCDesc; }
    inline void setDDesc(const TensorDesc &outDDesc)    { mDescription.d    = outDDesc; }
    inline void setAlpha(float alpha)                   { mDescription.alpha         = alpha; }
    inline void setBeta(float beta)                     { mDescription.beta          = beta; }
    inline void setSplitK(int32_t splitK)               { mDescription.split_k.slices     = splitK; }
    inline void setSplitKMode(SplitKMode splitKMode)    { mDescription.split_k.kernels    = splitKMode; }
    inline void setSplitKBuffers(int32_t splitKBuffers) { mDescription.split_k.buffers    = splitKBuffers; }
    inline void setNumModesL(int32_t numModesL)         { mDescription.num_modes_l   = numModesL; }
    inline void setNumModesM(int32_t numModesM)         { mDescription.num_modes_m   = numModesM; }
    inline void setNumModesN(int32_t numModesN)         { mDescription.num_modes_n   = numModesN; }
    inline void setNumModesK(int32_t numModesK)         { mDescription.num_modes_k   = numModesK; }

    // set A dims
    inline void setExtentA(int32_t *extentL, int32_t *extentM, int32_t *extentK) {
        for(int i = 0; i < mDescription.num_modes_l; i++) {
            mDescription.a.dim[i] = extentL[i];
        }
        for(int i = 0; i < mDescription.num_modes_m; i++) {
            mDescription.a.dim[i+mDescription.num_modes_l] = extentM[i];
        }
        for(int i = 0; i < mDescription.num_modes_k; i++) {
            mDescription.a.dim[i+mDescription.num_modes_l+mDescription.num_modes_m] = extentK[i];
        }
    }
    // set B dims
    inline void setExtentB(int32_t *extentL, int32_t *extentN, int32_t *extentK) {
        for(int i = 0; i < mDescription.num_modes_l; i++) {
            mDescription.b.dim[i] = extentL[i];
        }
        for(int i = 0; i < mDescription.num_modes_n; i++) {
            mDescription.b.dim[i+mDescription.num_modes_l] = extentN[i];
        }
        for(int i = 0; i < mDescription.num_modes_k; i++) {
            mDescription.b.dim[i+mDescription.num_modes_l+mDescription.num_modes_n] = extentK[i];
        }
    }
    // set C dims
    inline void setExtentC(int32_t *extentL, int32_t *extentM, int32_t *extentN) {
        for(int i = 0; i < mDescription.num_modes_l; i++) {
            mDescription.c.dim[i] = extentL[i];
        }
        for(int i = 0; i < mDescription.num_modes_m; i++) {
            mDescription.c.dim[i+mDescription.num_modes_l] = extentM[i];
        }
        for(int i = 0; i < mDescription.num_modes_n; i++) {
            mDescription.c.dim[i+mDescription.num_modes_l+mDescription.num_modes_m] = extentN[i];
        }
    }
    // pass A strides
    inline void setStrideA(int64_t *strideAL, int64_t *strideAM, int64_t *strideAK){
        for(int i = 0; i < mDescription.num_modes_l; i++) {
            mDescription.a.stride[i] = strideAL[i];
        }
        for(int i = 0; i < mDescription.num_modes_m; i++) {
            mDescription.a.stride[i+mDescription.num_modes_l] = strideAM[i];
        }
        for(int i = 0; i < mDescription.num_modes_k; i++) {
            mDescription.a.stride[i+mDescription.num_modes_l+mDescription.num_modes_m] = strideAK[i];
        }
    }
    // pass B strides
    inline void setStrideB(int64_t *strideBL, int64_t *strideBN, int64_t *strideBK) {
        for(int i = 0; i < mDescription.num_modes_l; i++) {
            mDescription.b.stride[i] = strideBL[i];
        }
        for(int i = 0; i < mDescription.num_modes_n; i++) {
            mDescription.b.stride[i+mDescription.num_modes_l] = strideBN[i];
        }
        for(int i = 0; i < mDescription.num_modes_k; i++) {
            mDescription.b.stride[i+mDescription.num_modes_l+mDescription.num_modes_n] = strideBK[i];
        }
    }
    // pass C strides
    inline void setStrideC(int64_t *strideCL, int64_t *strideCM, int64_t *strideCN) {
        for(int i = 0; i < mDescription.num_modes_l; i++) {
            mDescription.c.stride[i] = strideCL[i];
        }
        for(int i = 0; i < mDescription.num_modes_m; i++) {
            mDescription.c.stride[i+mDescription.num_modes_l] = strideCM[i];
        }
        for(int i = 0; i < mDescription.num_modes_n; i++) {
            mDescription.c.stride[i+mDescription.num_modes_l+mDescription.num_modes_m] = strideCN[i];
        }
    }
    // FIXME: back to use consistent rule manually temporarily as rules in specs has potential bug to be fix.
    Error isConsistent() const {
        //return mDescription.isConsistent();
        // dimension limit
        if(mDescription.a.dimensions != (mDescription.num_modes_m + mDescription.num_modes_l + mDescription.num_modes_k) ||
           mDescription.b.dimensions != (mDescription.num_modes_n + mDescription.num_modes_l + mDescription.num_modes_k) ||
           mDescription.c.dimensions != (mDescription.num_modes_n + mDescription.num_modes_m + mDescription.num_modes_l) ||
           mDescription.d.dimensions != (mDescription.num_modes_n + mDescription.num_modes_m + mDescription.num_modes_l)) {
            return cask::Error::SIZE_MISMATCH;
        }

        for (int i = 0; i < cask::TensorDesc::MAX_DIMS; i++) {
            // a dim size limit, default -1 means invalid dim size
            if(i < mDescription.num_modes_m+mDescription.num_modes_k+mDescription.num_modes_l){
                if (mDescription.a.dim[i] <= 0)
                    return cask::Error::SIZE_MISMATCH;
            }
            else {
               if (mDescription.a.dim[i] != -1)
                    return cask::Error::SIZE_MISMATCH;
            }
            // b dim size limit, default -1 means invalid dim size
            if(i < mDescription.num_modes_n+mDescription.num_modes_k+mDescription.num_modes_l){
                if (mDescription.b.dim[i] <= 0)
                    return cask::Error::SIZE_MISMATCH;
            }
            else {
                if (mDescription.b.dim[i] != -1)
                    return cask::Error::SIZE_MISMATCH;
            }
            // c and d dim size limit, default -1 means invalid dim size
            if(i < mDescription.num_modes_m+mDescription.num_modes_n+mDescription.num_modes_l){
                if (mDescription.c.dim[i] <= 0 || mDescription.d.dim[i] <= 0)
                    return cask::Error::SIZE_MISMATCH;
            }
            else {
                if (mDescription.c.dim[i] != -1 || mDescription.d.dim[i] != -1)
                    return cask::Error::SIZE_MISMATCH;
            }
        }
        // a share same k mode with b
        for (int i = 0; i < mDescription.num_modes_k; i++) {
            if(mDescription.a.dim[i] != mDescription.b.dim[i])
                return cask::Error::SIZE_MISMATCH;
        }
        // a share same m mode with c and d
        for (int i = 0; i < mDescription.num_modes_m; i++) {
            if(mDescription.a.dim[i+mDescription.num_modes_k] != mDescription.c.dim[i+mDescription.num_modes_n] ||
               mDescription.a.dim[i+mDescription.num_modes_k] != mDescription.d.dim[i+mDescription.num_modes_n])
            return cask::Error::SIZE_MISMATCH;
        }
        // b share same n mode with c and d
        for (int i = 0; i < mDescription.num_modes_n; i++) {
            if(mDescription.b.dim[i+mDescription.num_modes_k] != mDescription.c.dim[i] ||
               mDescription.b.dim[i+mDescription.num_modes_k] != mDescription.d.dim[i])
            return cask::Error::SIZE_MISMATCH;
        }
        // a share same l mode with b, c and d
        for (int i = 0; i < mDescription.num_modes_l; i++) {
            if((mDescription.a.dim[i+mDescription.num_modes_k+mDescription.num_modes_m] !=
                mDescription.b.dim[i+mDescription.num_modes_k+mDescription.num_modes_n]) ||
               (mDescription.a.dim[i+mDescription.num_modes_k+mDescription.num_modes_m] !=
                mDescription.c.dim[i+mDescription.num_modes_n+mDescription.num_modes_m]) ||
               (mDescription.a.dim[i+mDescription.num_modes_k+mDescription.num_modes_m] !=
                mDescription.d.dim[i+mDescription.num_modes_n+mDescription.num_modes_m]))
                return cask::Error::SIZE_MISMATCH;
        }
        // Otherwise return OK
        return cask::Error::OK;

    }

    inline const Description& getDescription() const    { return mDescription; }
protected:
    Description mDescription;   
};

} // namespace cask

#endif
