#ifndef INCLUDE_GUARD_CASK_TENSORDESC_H
#define INCLUDE_GUARD_CASK_TENSORDESC_H

/// @file

#if !defined(INCLUDE_GUARD_CASK_H)
# error "Never use <cask/tensor_desc.h> directly; include <cask.h> instead."
#endif

namespace cask {

using MatrixLayoutType = md::MatrixLayoutType;
using ActivationLayoutType = md::ActivationLayoutType;
using WeightLayoutType = md::WeightLayoutType;
using GenericLayoutType = md::GenericLayoutType;

//////////////////////////////////////////////////////////////////////////////
/// \brief N-Tensor Descriptor (N <= 8)
///
/// Describes only the shape of the Tensor.  Pointer to actual data is passed
/// directly to Shader::run() or Shader::initDeviceReservedSpace()
///
/// Example: 32x128 Matrix (2-tensor) of fp32
/// dimensions=2, scalarType=ScalarType::FP32, dim[1]=32, dim[0]=128
///
/// element [i][j] is at element offset i*stride[1]+j*stride[0]
///
/// **row major**:
/// stride[1]=128,stride[0]=1
///
/// **column major**:
/// stride[1]=1,stride[0]=32
#pragma pack(4)
struct
TensorDesc {

    /////////////////////////////////////////////////////////////////////////
    /// \bried For existing code relying on TensorDesc::Error
    /////////////////////////////////////////////////////////////////////////
    typedef cask::Error Error;

    static const int MAX_DIMS = 36;

    /////////////////////////////////////////////////////////////////////////
    /// we really just want scoped enums here, not enum class safety (we want
    /// these to be usable as array indices)
    /// Dim is designed for colw kernel tensors.
    /// Tensor for colw kernels are assumed to be 5D.
    /////////////////////////////////////////////////////////////////////////
    struct Dim {
        enum { W = 0,
               H = 1,
               D = 2,
               C = 3,
               N = 4 };
    };

    /////////////////////////////////////////////////////////////////////////
    /// When we're working with matrices we want to call the dimensions
    /// something else
    /////////////////////////////////////////////////////////////////////////
    struct MatrixDim {
        enum { Cols = 0,
               Rows = 1,
               Batch = 2 };
    };

    /////////////////////////////////////////////////////////////////////////
    /// Specify the sparsity ratio if it's a sparse tensor
    /// \deprecated Use `cask::TensorSparsityRatio` instead
    /////////////////////////////////////////////////////////////////////////
    struct SparsityHint {
        typedef enum {
            R_1_1 = 0,  // stand for dense tensor, no need to compress
            R_4_2 = 1,  // 4:2 compression
            R_2_1 = 2   // 2:1 compression
        } sparseRatio_t;
    };

    /////////////////////////////////////////////////////////////////////////
    /// Default Ctor that sets a few fields with default value
    /////////////////////////////////////////////////////////////////////////
    TensorDesc();

    /////////////////////////////////////////////////////////////////////////
    /// Ctor for gemm.
    /////////////////////////////////////////////////////////////////////////
    TensorDesc(ScalarType sType,
               int64_t rowSize,
               int64_t colSize,
               int64_t rowStride,
               int64_t colStride,
               int32_t simd=1, //!< number of scalars per element
               int32_t vDim=-1, //!< which dimension is packed in elements
               int64_t imagStride=0)
        ;

    /// Helper to explicitly call 'Ctor for gemm'
    static TensorDesc make_Matrix(ScalarType sType,
                                  int64_t rowSize,
                                  int64_t colSize,
                                  int64_t rowStride,
                                  int64_t colStride,
                                  int32_t simd = 1,   //!< number of scalars per element
                                  int32_t vDim = -1,  //!< which dimension is packed in elements
                                  int64_t imagOffset = 0);

    /////////////////////////////////////////////////////////////////////////
    /// Ctor for 3D colw. Set strides based on NCDHW layout by default.
    /////////////////////////////////////////////////////////////////////////
    TensorDesc(int64_t nSize,
               int64_t cSize,
               int64_t dSize,
               int64_t hSize,
               int64_t wSize,
               ScalarType sType=ScalarType::FP32,
               int32_t simd=1, //!< number of scalars per element
               int32_t vDim=-1, //!< which dimension is packed in elements
               int64_t imagStride=0)
        ;

    /// Helper for 3D colw. Set strides based on NCDHW layout by default.
    static TensorDesc make_Tensor5D(
                int64_t nSize,
                int64_t cSize,
                int64_t dSize,
                int64_t hSize,
                int64_t wSize,
                ScalarType sType=ScalarType::FP32,
                int32_t simd=1, //!< number of scalars per element
                int32_t vDim=-1, //!< which dimension is packed in elements
                int64_t imagStride=0)
        ;

    /////////////////////////////////////////////////////////////////////////
    /// The old ctor for 2D colw. This ctor calls the 3D colw ctor and
    /// it will set the depth dimention and stride as if the size is 1.
    /// Keep this API so that existing user code is not broken.
    /////////////////////////////////////////////////////////////////////////
    TensorDesc(int64_t nSize,
               int64_t cSize,
               int64_t hSize,
               int64_t wSize,
               ScalarType sType=ScalarType::FP32,
               int32_t simd=1, //!< number of scalars per element
               int32_t vDim=-1, //!< which dimension is packed in elements
               int64_t imagStride=0)
        ;

    /// Helper to construct a 4D tensor for 2D colw
    static TensorDesc make_Tensor4D(
                int64_t nSize,
                int64_t cSize,
                int64_t hSize,
                int64_t wSize,
                ScalarType sType=ScalarType::FP32,
                int32_t simd=1, //!< number of scalars per element
                int32_t vDim=-1, //!< which dimension is packed in elements
                int64_t imagStride=0)
        ;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return true if this TensorDesc object
    ///        has vectorizatoin in one of dimensions.
    /////////////////////////////////////////////////////////////////////////
    inline bool hasVectorization(int scalarsPerElement, int vectorizedDim) const {
        if( (this->scalarsPerElement != scalarsPerElement) ||
            ((this->scalarsPerElement > 1) &&
             (this->vectorizedDim != vectorizedDim)) ) {
            return false;
        }

        return true;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return true if the input TensorDesc object
    ///        has vectorizatoin in one of dimensions.
    /////////////////////////////////////////////////////////////////////////
    inline bool hasVectorization(const TensorDesc& that) const {
        return this->hasVectorization(that.scalarsPerElement,
                                      that.vectorizedDim);
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return true if dimensions and scalarsType
    ///        of two TensorDesc objects are the same.
    /////////////////////////////////////////////////////////////////////////
    inline bool isSimilar(const TensorDesc& that) const {
        if( (this->dimensions != that.dimensions) ||
            (this->scalarType != that.scalarType) ) {
            return false;
        }
        for( int i = 0; i < this->dimensions; i++ ) {
            if( this->getDim(i) != that.getDim(i) ) {
                return false;
            }
        }

        return true;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return true if the input TensorDesc object
    ///        is similar to this TensorDesc and has the same strides.
    /////////////////////////////////////////////////////////////////////////
    inline bool operator==(const TensorDesc& that) const {
        if( !this->isSimilar(that) ||
            !this->hasVectorization(that) ) {
            return false;
        }
        // for equality we want additionally that the strides are actually the
        // same
        for( int i = 0; i < this->dimensions; i++ ) {
            if( this->stride[i] != that.stride[i] ) {
                return false;
            }
        }
        if (this->imagStride != that.imagStride) {
            return false;
        }
        return true;
    }

    /////////////////////////////////////////////////////////////////////////
    /// Transform this TensorDesc into NhalfCHW2, or NChalfCHW2, or
    /// NCquarterCHW4, starting from a NCHW TensorDesc
    ///
    /// \returns Error::OK: on success

    /// \returns Error::FMT_UNSUPPORTED if the input is anything other than
    /// dimension-order packed (i.e., NCHW with strideW = 1, strideH = W,
    /// strideC = HW, strideN = CHW) or if the input is already simdized, or if
    /// the requested vDim is not between 0 and this->dimensions.
    /////////////////////////////////////////////////////////////////////////

    Error simdize(int32_t vDim,       //!< which dimension to simdize
                  int32_t simd);      //!< how many scalars per simd vector?

    /////////////////////////////////////////////////////////////////////////
    // the general case, the dims should be a permutation of the Dim enum
    Error makePacked5(int dim4, int dim3, int dim2, int dim1, int dim0);

    /////////////////////////////////////////////////////////////////////////
    /// \brief Pack the activation layer tensor.
    ///
    /// Compute the strides as the tensor is packed as the layout speificies.
    /// Use this for activation tensor.
    /////////////////////////////////////////////////////////////////////////
    Error pack(ActivationLayoutType layout);

    /////////////////////////////////////////////////////////////////////////
    /// \brief Pack the weight layer tensor.
    ///
    /// Compute the strides as the tensor is packed as the layout speificies.
    /// Use this for weight tensor.
    /////////////////////////////////////////////////////////////////////////
    Error pack(WeightLayoutType layout);

    /////////////////////////////////////////////////////////////////////////
    /// \brief Pack as generic tensor shape
    ///
    /// Compute the strides as the tensor is packed as the layout speificies.
    /// Use this for weight tensor.
    /////////////////////////////////////////////////////////////////////////
    Error pack();

    /////////////////////////////////////////////////////////////////////////
    // the general case, the dims should be a permutation of the Dim enum
    // expand this function to support 5D tensor.
    bool isPacked5(int dim4, int dim3, int dim2, int dim1, int dim0) const;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return true if an actication tensor is packed with a certain layout
    ///
    /// Check if the tensor is correctly packed as the layout speficies.
    /// Use this for activation tensor.
    /////////////////////////////////////////////////////////////////////////
    bool isPacked(ActivationLayoutType layout) const;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return true if a weight tensor is packed with a certain layout
    ///
    /// Check if the tensor is correctly packed as the layout speficies.
    /// Use this for weight tensor.
    /////////////////////////////////////////////////////////////////////////
    bool isPacked(WeightLayoutType layout) const;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return whether this is a complex tensor
    /////////////////////////////////////////////////////////////////////////
    bool isComplex() const {
        return NumericTraits(scalarType).complex == cask::ComplexClassID::COMPLEX;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return whether this is a planar complex tensor
    /////////////////////////////////////////////////////////////////////////
    bool isPlanarComplex() const {
        return isComplex() && (imagStride != 0);
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return number of elements of the tensor.
    /////////////////////////////////////////////////////////////////////////
    int64_t sizeInElements() const;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return size in bytes of the buffer needed to store the tensor
    /////////////////////////////////////////////////////////////////////////
    inline int64_t sizeInBytes() const {
        int64_t elemSize = elementSize();
        int64_t size = sizeInElements() * elemSize; // max of dims * strides * elemSize
        if (isPlanarComplex()) {
            // CASK planar complex assumes real + imag are stored in a single allocation
            int64_t offsetSize = labs(imagStride * elemSize); // size of first planar component + alignment
            int32_t batchDim = (dimensions == 3) ? static_cast<int32_t>(MatrixDim::Batch) : static_cast<int32_t>(Dim::N);
            int64_t batchSize = stride[batchDim] * elemSize;
            int64_t planarSize = 0;
            for (int32_t i = batchDim-1; i >= 0; i--) {
                planarSize = std::max(planarSize, dim[i] * stride[i] * elemSize);
            }

            if (offsetSize >= size) { // pair of batched arrays
                return offsetSize + size; // strides don't account for planar component
            } else if (offsetSize + planarSize <= batchSize) { // batched array of pairs
                return size; // strides account for planar component
            } else { // invalid imagStride, planar components overlap
                return -1;
            }
        }
        return size;
    }

    /////////////////////////////////////////////////////////////////////////

    int32_t    dimensions;        //!< Number of dimensions in the tensor
    ScalarType scalarType;        //!< Data type
    int64_t    dim[MAX_DIMS];     //!< Number of elements in each dimension
    int64_t    stride[MAX_DIMS];  //!< Distance **in elements** to the next element in this dimension.

    /////////////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the number of scalars for one dimension.
    /////////////////////////////////////////////////////////////////////////
    inline int64_t getDim(int n) const {
        if (n < dimensions) {
            return dim[n] * ((vectorizedDim == n) ? scalarsPerElement : 1);
        }
        else {                  // error!
            return 0;
        }
    }

    /////////////////////////////////////////////////////////////////////////

    int32_t    scalarsPerElement; //!< Vectorization

    /////////////////////////////////////////////////////////////////////////

    int32_t    vectorizedDim;     //!< Which (if any) dimension is packed in elements  ("None" is -1)

    /////////////////////////////////////////////////////////////////////////

    int64_t    imagStride;        //!< Element offset to planar complex imaginary component

    /////////////////////////////////////////////////////////////////////////
    /// \brief returns default imagStride with optional byte alignment for planar complex
    ///
    /// default planar complex layout is pair of batched arrays, real before imag
    /////////////////////////////////////////////////////////////////////////
    int64_t computeImagStride(int byteAlignment=128) const;

    /////////////////////////////////////////////////////////////////////////
    /// \brief transform this tensor into planar complex, if scalarType is complex
    /////////////////////////////////////////////////////////////////////////
    Error makePlanarComplex() {
        if (!isComplex()) {
            imagStride = 0;
            return Error::PLANAR_COMPLEX;
        } else {
            imagStride = computeImagStride();
            return Error::OK;
        }
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief size of one element (the thing which sizes and strides are measured in)
    /////////////////////////////////////////////////////////////////////////
    inline int64_t elementSize() const {
        if (isPlanarComplex()) {
            return scalarsPerElement * (cask::sizeInBytes(scalarType) / 2);
        }
        return scalarsPerElement * cask::sizeInBytes(scalarType);
    }

    /////////////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////////
    /// \brief Print info about this TensorDesc.
    /////////////////////////////////////////////////////////////////////////
    void print() const;
};
#pragma pack()

//////////////////////////////////////////////////////////////////////////////

/// copy source tensor data of shape srcShape into destination tensor data of
/// shape destShape.
///
/// \pre destShape.sizeInBytes() == srcShape.sizeInBytes()
/// \pre The height and width of src and dest must be the same
/// \pre The scalarType of src and dest must be the same
/// \pre N*C*scalarsPerElement of src and dest must be the same
///
/// \post The dest buffer contains the data from src buffer in the shape
/// specified by destShape
///
/// \returns TensorDesc::Error::OK on successful data transform and copy
/// \returns TensorDesc::Error::??? otherwise
//////////////////////////////////////////////////////////////////////////////
TensorDesc::Error
transformTensor(void* dest,   //!< destination buffer, must be destShape.sizeInBytes() large
                const void* src, //!< source buffer, must be srcShape.sizeInBytes() large
                const TensorDesc& destShape, //!< shape of the destination buffer
                const TensorDesc& srcShape   //!< shape of the source buffer
    );


} // namespace cask

#endif // INCLUDE_GUARD_CASK_TENSORDESC_H
