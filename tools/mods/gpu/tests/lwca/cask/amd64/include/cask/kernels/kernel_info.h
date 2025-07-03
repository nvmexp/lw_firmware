
#ifndef CASK_KERNEL_INFO_H
#define CASK_KERNEL_INFO_H

#include <cstdint>
#include <typeinfo>

namespace cask {

enum KernelInfoNames {
   TYPEA,
   TYPEAM,
   TYPEB,
   TYPEC,
   TYPEEPILOG,
   CUDNNEDGES,
   CUDNNINDEXING,
   CUDNNLAYOUT,
   CUDNNNCHW1X1,
   CUDNNNHALFCHW2,
   CUDNNOUTPUTLAYOUT,
   CUDNNPASS,
   CUDNNSPLITK,
   CUDNNSTRIDEDB,
   CUDNNWINOGRAD,
   CUDNNWINOGRADTILE,
   ELEMENTCOLSPERCTA,
   ELEMENTROWSPERCTA,
   FAMILY,
   GROUPNAME,
   LAYOUTA,
   LAYOUTB,
   NAME,
   RELUANDBIAS,
   SHAPEC,
   SLICECOLS,
   SLICEROWS,
   SPAS,
   RAGGEDMN,
   KBLOCK,
   THREADSPERCTA,
   WARPSTRIDEK,
   SHIFTFASTA,
   MULTIPLIERSLOWA,
   OFFSETSLOWA,
   SHIFTFASTB,
   MULTIPLIERSLOWB,
   OFFSETSLOWB,
   SHAREDMEMSIZE,
   LOG2ELEMENTSPERLDGA,
   LOG2ELEMENTSPERLDGB,
   OPDEF,
   USESTC,
   SHADERCLASS,
   ABIVERSION
};

/**
 * Public (pure virtual) interface for KernelInfo
 */
class KernelInfo {

public:

    virtual sass_private::elementDataType_t typeA() const = 0;
    virtual sass_private::elementDataType_t typeAm() const = 0;
    virtual sass_private::elementDataType_t typeB() const = 0;
    virtual sass_private::elementDataType_t typeC() const = 0;
    virtual sass_private::elementDataType_t typeEpilog() const = 0;
    virtual sass_private::edgeType_t cuDnnEdges() const = 0;
    virtual bool cuDnnIndexing() const = 0;
    virtual sass_private::cuDnnLayout_t cuDnnLayout() const = 0;
    virtual bool cuDnnNchw1x1() const = 0;
    virtual bool cuDnnNhalfCHW2() const = 0;
    virtual sass_private::cuDnnLayout_t cuDnnOutputLayout() const = 0;
    virtual const char * cuDnnPass() const = 0;
    virtual bool cuDnnSplitK() const = 0;
    virtual bool cuDnnStridedB() const = 0;
    virtual bool cuDnnWinograd() const = 0;
    virtual const char * cuDnnWinogradTile() const = 0;
    virtual uint16_t elementColsPerCta() const = 0;
    virtual uint16_t elementRowsPerCta() const = 0;
    virtual sass_private::chipFamilyType_t family() const = 0;
    virtual const char * groupName() const = 0;
    virtual sass_private::layoutType_t layoutA() const = 0;
    virtual sass_private::layoutType_t layoutB() const = 0;
    virtual const char * name() const = 0;
    virtual bool reLuAndBias() const = 0;
    virtual sass_private::shapeTypeC_t shapeC() const = 0;
    virtual uint16_t sliceCols() const = 0;
    virtual uint16_t sliceRows() const = 0;
    virtual uint64_t spas() const = 0;
    virtual bool raggedMn() const = 0;
    virtual uint16_t kBlock() const = 0;
    virtual uint16_t threadsPerCta() const = 0;
    virtual uint16_t warpStrideK() const = 0;
    virtual uint32_t shiftFastA() const = 0;
    virtual int32_t multiplierSlowA() const = 0;
    virtual int32_t offsetSlowA() const = 0;
    virtual uint32_t shiftFastB() const = 0;
    virtual int32_t multiplierSlowB() const = 0;
    virtual int32_t offsetSlowB() const = 0;
    virtual uint32_t sharedMemSize() const = 0;
    virtual uint8_t log2ElementsPerLdgA() const = 0;
    virtual uint8_t log2ElementsPerLdgB() const = 0;
    virtual OpDefinition opDef() const = 0;
    virtual bool usesTc() const = 0;
    virtual const char * shaderClass() const = 0;
    virtual sass_private::abiVersion_t abiVersion() const = 0;

    virtual bool infoSupported(KernelInfoNames k) = 0;
    virtual ~KernelInfo() = default;
    virtual void destroy() { delete this;}
};

struct DeprecatedKernelInfo: public KernelInfo {
  using ElementDataType = sass_private::elementDataType_t;
#define ABORT_WRONG_KI \
    printf("The legacy metadata API `%s` is deprecated! Please switch to use `getInfo()` API to retrieve new kernel info object!\n", __func__); \
    abort()

    virtual sass_private::edgeType_t cuDnnEdges() const override { ABORT_WRONG_KI; return sass_private::CUDNN_NONE; }
    virtual bool cuDnnIndexing() const override { ABORT_WRONG_KI; return true; }
    virtual sass_private::cuDnnLayout_t cuDnnLayout() const override { ABORT_WRONG_KI; return sass_private::cuDnnLayout_t::NHWC; }
    virtual sass_private::cuDnnLayout_t cuDnnOutputLayout() const override { ABORT_WRONG_KI; return sass_private::cuDnnLayout_t::NHWC; }
    virtual bool cuDnnNchw1x1() const override { ABORT_WRONG_KI; return false; }
    virtual bool cuDnnNhalfCHW2() const override { ABORT_WRONG_KI; return false; }
    virtual const char * cuDnnPass() const override { ABORT_WRONG_KI; return "undefined"; }
    virtual bool cuDnnSplitK() const override { ABORT_WRONG_KI; return false; }
    virtual bool cuDnnStridedB() const override { ABORT_WRONG_KI; return false; }
    virtual bool cuDnnWinograd() const override { ABORT_WRONG_KI; return false; }
    virtual const char * cuDnnWinogradTile() const override { ABORT_WRONG_KI; return "undefined"; }
    virtual uint16_t elementColsPerCta() const override { ABORT_WRONG_KI; return 128; }
    virtual uint16_t elementRowsPerCta() const override { ABORT_WRONG_KI; return 128; }
    virtual ElementDataType typeA() const override { ABORT_WRONG_KI; return cask::sass_private::R_16U; }
    virtual ElementDataType typeAm() const override { ABORT_WRONG_KI; return cask::sass_private::R_16U; }
    virtual ElementDataType typeB() const override { ABORT_WRONG_KI; return cask::sass_private::R_16U; }
    virtual ElementDataType typeC() const override { ABORT_WRONG_KI; return cask::sass_private::R_16U; }
    virtual ElementDataType typeEpilog() const override { ABORT_WRONG_KI; return cask::sass_private::R_16U; }
    virtual sass_private::layoutType_t layoutA() const override { ABORT_WRONG_KI; return cask::sass_private::N; }
    virtual sass_private::layoutType_t layoutB() const override { ABORT_WRONG_KI; return cask::sass_private::N; }
    virtual uint32_t sharedMemSize() const override { ABORT_WRONG_KI; return 0; }
    virtual sass_private::chipFamilyType_t family() const override { ABORT_WRONG_KI; return sass_private::chipFamilyType_t::GEMM_CHIP_VOLTA; }
    virtual const char * groupName() const override { ABORT_WRONG_KI; return "undefined"; }
    virtual const char * name() const override { ABORT_WRONG_KI; return ""; }
    virtual bool reLuAndBias() const override { ABORT_WRONG_KI; return false; }
    virtual sass_private::shapeTypeC_t shapeC() const override {ABORT_WRONG_KI; return sass_private::shapeTypeC_t::RECT; }
    virtual uint16_t sliceCols() const override { ABORT_WRONG_KI; return 1; }
    virtual uint16_t sliceRows() const override { ABORT_WRONG_KI; return 1; }
    virtual uint64_t spas() const override { ABORT_WRONG_KI; return 0; }
    virtual bool raggedMn() const override { ABORT_WRONG_KI; return true; }
    virtual uint16_t kBlock() const override { ABORT_WRONG_KI; return 32; }
    virtual uint16_t threadsPerCta() const override {ABORT_WRONG_KI; return 0; }
    virtual uint16_t warpStrideK() const override { ABORT_WRONG_KI; return 1; }
    virtual uint32_t shiftFastA() const override { ABORT_WRONG_KI; return 1; }
    virtual int32_t multiplierSlowA() const override { ABORT_WRONG_KI; return 1; }
    virtual int32_t offsetSlowA() const override { ABORT_WRONG_KI; return 1; }
    virtual uint32_t shiftFastB() const override { ABORT_WRONG_KI; return 1; }
    virtual int32_t multiplierSlowB() const override { ABORT_WRONG_KI; return 1; }
    virtual int32_t offsetSlowB() const override { ABORT_WRONG_KI; return 1; }
    virtual uint8_t log2ElementsPerLdgA() const override { ABORT_WRONG_KI; return 8; }
    virtual uint8_t log2ElementsPerLdgB() const override { ABORT_WRONG_KI; return 8; }
    virtual cask::OpDefinition opDef() const override {ABORT_WRONG_KI; return cask::OpDefinition::OP_GRAPH; }
    virtual bool usesTc() const override { ABORT_WRONG_KI; return false; }
    virtual const char * shaderClass() const override {ABORT_WRONG_KI; return "undefined"; }
    virtual sass_private::abiVersion_t abiVersion() const override { ABORT_WRONG_KI; return sass_private::ABI_PREAMPERE_G; }
    // FIXME: automatically generate this function
    virtual bool infoSupported(cask::KernelInfoNames k) override { ABORT_WRONG_KI; return true; }
};

#ifndef SKIP_CASK_KERNELINFO_DEFAULT

/**
 * Default implementation of KernelInfo. Used by sass kernels, possibly others
 */
class KernelInfoDefault : public KernelInfo {

public:
    KernelInfoDefault(
        uint64_t supportVector_in,
        sass_private::elementDataType_t typeA_in,
        sass_private::elementDataType_t typeAm_in,
        sass_private::elementDataType_t typeB_in,
        sass_private::elementDataType_t typeC_in,
        sass_private::elementDataType_t typeEpilog_in,
        sass_private::edgeType_t cuDnnEdges_in,
        bool cuDnnIndexing_in,
        sass_private::cuDnnLayout_t cuDnnLayout_in,
        bool cuDnnNchw1x1_in,
        bool cuDnnNhalfCHW2_in,
        sass_private::cuDnnLayout_t cuDnnOutputLayout_in,
        const char * cuDnnPass_in,
        bool cuDnnSplitK_in,
        bool cuDnnStridedB_in,
        bool cuDnnWinograd_in,
        const char * cuDnnWinogradTile_in,
        uint16_t elementColsPerCta_in,
        uint16_t elementRowsPerCta_in,
        sass_private::chipFamilyType_t family_in,
        const char * groupName_in,
        sass_private::layoutType_t layoutA_in,
        sass_private::layoutType_t layoutB_in,
        const char * name_in,
        bool reLuAndBias_in,
        sass_private::shapeTypeC_t shapeC_in,
        uint16_t sliceCols_in,
        uint16_t sliceRows_in,
        uint64_t spas_in,
        bool raggedMn_in,
        uint16_t kBlock_in,
        uint16_t threadsPerCta_in,
        uint16_t warpStrideK_in,
        uint32_t shiftFastA_in,
        int32_t multiplierSlowA_in,
        int32_t offsetSlowA_in,
        uint32_t shiftFastB_in,
        int32_t multiplierSlowB_in,
        int32_t offsetSlowB_in,
        uint32_t sharedMemSize_in,
        uint8_t log2ElementsPerLdgA_in,
        uint8_t log2ElementsPerLdgB_in,
        OpDefinition opDef_in,
        bool usesTc_in,
        const char * shaderClass_in,
        sass_private::abiVersion_t abiVersion_in
        );


protected:
    uint64_t supportVector;
    sass_private::elementDataType_t _typeA;
    sass_private::elementDataType_t _typeAm;
    sass_private::elementDataType_t _typeB;
    sass_private::elementDataType_t _typeC;
    sass_private::elementDataType_t _typeEpilog;
    sass_private::edgeType_t _cuDnnEdges;
    bool _cuDnnIndexing;
    sass_private::cuDnnLayout_t _cuDnnLayout;
    bool _cuDnnNchw1x1;
    bool _cuDnnNhalfCHW2;
    sass_private::cuDnnLayout_t _cuDnnOutputLayout;
    const char * _cuDnnPass;
    bool _cuDnnSplitK;
    bool _cuDnnStridedB;
    bool _cuDnnWinograd;
    const char * _cuDnnWinogradTile;
    uint16_t _elementColsPerCta;
    uint16_t _elementRowsPerCta;
    sass_private::chipFamilyType_t _family;
    const char * _groupName;
    sass_private::layoutType_t _layoutA;
    sass_private::layoutType_t _layoutB;
    const char * _name;
    bool _reLuAndBias;
    sass_private::shapeTypeC_t _shapeC;
    uint16_t _sliceCols;
    uint16_t _sliceRows;
    uint64_t _spas;
    bool _raggedMn;
    uint16_t _kBlock;
    uint16_t _threadsPerCta;
    uint16_t _warpStrideK;
    uint32_t _shiftFastA;
    int32_t _multiplierSlowA;
    int32_t _offsetSlowA;
    uint32_t _shiftFastB;
    int32_t _multiplierSlowB;
    int32_t _offsetSlowB;
    uint32_t _sharedMemSize;
    uint8_t _log2ElementsPerLdgA;
    uint8_t _log2ElementsPerLdgB;
    OpDefinition _opDef;
    bool _usesTc;
    const char * _shaderClass;
    sass_private::abiVersion_t _abiVersion;

public:


    virtual sass_private::elementDataType_t typeA() const;
    virtual sass_private::elementDataType_t typeAm() const;
    virtual sass_private::elementDataType_t typeB() const;
    virtual sass_private::elementDataType_t typeC() const;
    virtual sass_private::elementDataType_t typeEpilog() const;
    virtual sass_private::edgeType_t cuDnnEdges() const;
    virtual bool cuDnnIndexing() const;
    virtual sass_private::cuDnnLayout_t cuDnnLayout() const;
    virtual bool cuDnnNchw1x1() const;
    virtual bool cuDnnNhalfCHW2() const;
    virtual sass_private::cuDnnLayout_t cuDnnOutputLayout() const;
    virtual const char * cuDnnPass() const;
    virtual bool cuDnnSplitK() const;
    virtual bool cuDnnStridedB() const;
    virtual bool cuDnnWinograd() const;
    virtual const char * cuDnnWinogradTile() const;
    virtual uint16_t elementColsPerCta() const;
    virtual uint16_t elementRowsPerCta() const;
    virtual sass_private::chipFamilyType_t family() const;
    virtual const char * groupName() const;
    virtual sass_private::layoutType_t layoutA() const;
    virtual sass_private::layoutType_t layoutB() const;
    virtual const char * name() const;
    virtual bool reLuAndBias() const;
    virtual sass_private::shapeTypeC_t shapeC() const;
    virtual uint16_t sliceCols() const;
    virtual uint16_t sliceRows() const;
    virtual uint64_t spas() const;
    virtual bool raggedMn() const;
    virtual uint16_t kBlock() const;
    virtual uint16_t threadsPerCta() const;
    virtual uint16_t warpStrideK() const;
    virtual uint32_t shiftFastA() const;
    virtual int32_t multiplierSlowA() const;
    virtual int32_t offsetSlowA() const;
    virtual uint32_t shiftFastB() const;
    virtual int32_t multiplierSlowB() const;
    virtual int32_t offsetSlowB() const;
    virtual uint32_t sharedMemSize() const;
    virtual uint8_t log2ElementsPerLdgA() const;
    virtual uint8_t log2ElementsPerLdgB() const;
    virtual OpDefinition opDef() const;
    virtual bool usesTc() const;
    virtual const char * shaderClass() const;
    virtual sass_private::abiVersion_t abiVersion() const;

    virtual bool infoSupported(KernelInfoNames k);

};

#endif // CASK_KERNELINFO_DEFAULT

} // end namespace cask

#endif // CASK_KERNEL_INFO_H
