#ifndef __FT_SEARCH_HPP__
#define __FT_SEARCH_HPP__

#include <iomanip>
#include <iostream>

namespace cask {
namespace ft {

enum op_t { FPROP = 0, DGRAD, WGRAD, OP_COUNT };

class colwSearchOptions {
   private:
    kr::spa_t spa[static_cast<int>(kr::SPA_COUNT)];
    uint32_t spa_ct;
    sass_private::lwDnnLayout_t input_fmt;  // NCHW or NHWC
    sass_private::lwDnnLayout_t filter_fmt;
    sass_private::lwDnnLayout_t output_fmt;
    op_t op;
    sass_private::elementDataType_t aclwm_type;
    bool tensor_cores;

    bool spa_set;
    bool input_fmt_set;
    bool filter_fmt_set;
    bool output_fmt_set;
    bool op_set;
    bool aclwm_type_set;
    bool tensor_cores_set;

    sasskr::gemm_t gemmval;
    bool gemm_specified;
    bool enableSplitK, enableSplitK_specified;
    bool enableReLu, enableReLu_specified;
    bool enableStridedB, enableStridedB_specified;
    bool enableIdx, enableIdx_specified;
    sass_private::edgeType_t edge;
    bool edge_specified;
    krcc::kblock_t kblock;
    bool kblock_specified;
    kr::ldga_t ldga;
    bool ldga_specified;
    kr::ldgb_t ldgb;
    bool ldgb_specified;

    krcc::stages_t stage;
    bool stage_specified;

    bool cta_splitk_enabled;
    bool cta_splitk_specified;

    bool Nd_enabled;
    bool Nd_specified;

    bool sliced_enabled;
    bool sliced_specified;

    bool sbuf_enabled;
    bool sbuf_specified;

   public:
    colwSearchOptions();
    ~colwSearchOptions();
    colwSearchOptions(const colwSearchOptions &rhs);
    Error
    setSPAs(kr::spa_t *v, uint32_t ct);
    Error
    setSPA(int compute_capability);
    Error
    setSPA(kr::spa_t v);
    Error
    setInputFmt(sass_private::lwDnnLayout_t fmt);
    Error
    setFilterFmt(sass_private::lwDnnLayout_t fmt);
    Error
    setOutputFmt(sass_private::lwDnnLayout_t fmt);
    Error
    setAclwmType(sass_private::elementDataType_t at);
    Error
    setAclwmType(const ScalarType &at);
    Error
    setTensorCores(bool tc);
    Error
    setOp(op_t colw_op);
    Error
    setGemm(cask::sasskr::gemm_t val);
    Error
    setSplitK(bool val);
    Error
    setReLu(bool val);
    Error
    setStridedB(bool val);
    Error
    setIdx(bool val);
    Error
    setEdge(sass_private::edgeType_t val);
    Error
    setKBlock(krcc::kblock_t val);
    Error
    setLdgA(cask::kr::ldga_t val);
    Error
    setLdgB(cask::kr::ldgb_t val);
    Error
    setStage(cask::krcc::stages_t val);
    Error
    setCTASplitK(bool val);  // xmma tree only
    Error
    setNd(bool val);  // SASS only
    Error
    setSliced(bool val);  // SASS only
    Error
    setSingleBuffer(bool val);  // SASS only

    Error
    getSPAs(kr::spa_t *v, uint32_t *ct) const;
    Error
    getInputFmt(sass_private::lwDnnLayout_t *fmt) const;
    Error
    getFilterFmt(sass_private::lwDnnLayout_t *fmt) const;
    Error
    getOutputFmt(sass_private::lwDnnLayout_t *fmt) const;
    Error
    getAclwmType(sass_private::elementDataType_t *at) const;
    Error
    getTensorCores(bool *tc) const;
    Error
    getOp(op_t *op) const;

    // if specified is true, default behavior is over-ridden by a direct search configuration for val at that level in
    // the tree
    Error
    getGemm(bool *specifed, cask::sasskr::gemm_t *val) const;
    Error
    getSplitK(bool *specifed, bool *val) const;
    Error
    getReLu(bool *specifed, bool *val) const;
    Error
    getStridedB(bool *specified, bool *val) const;
    Error
    getIdx(bool *specifed, bool *val) const;
    Error
    getEdge(bool *specifed, sass_private::edgeType_t *val) const;
    Error
    getKBlock(bool *specifed, krcc::kblock_t *val) const;
    Error
    getLdgA(bool *specifed, cask::kr::ldga_t *val) const;
    Error
    getLdgB(bool *specifed, cask::kr::ldgb_t *val) const;

    Error
    getStage(bool *specified, cask::krcc::stages_t *val) const;  // xmma tree only
    Error
    getCTASplitK(bool *specified, bool *val) const;

    Error
    getNd(bool *specified, bool *val) const;
    Error
    getSliced(bool *specified, bool *val) const;
    Error
    getSingleBuffer(bool *specified, bool *val) const;

    bool
    isValid() const;
};

class gemmSearchOptions {
   private:
    kr::spa_t spa[static_cast<int>(kr::spa_t::SPA_COUNT)];
    uint32_t spa_ct;
    sass_private::layoutType_t a_layout;
    sass_private::layoutType_t b_layout;
    sass_private::layoutType_t c_layout;
    sass_private::elementDataType_t aclwm_type;
    sass_private::shapeTypeC_t shape;
    bool tensor_cores;

    bool spa_set;
    bool a_layout_set;
    bool b_layout_set;
    bool c_layout_set;
    bool aclwm_type_set;
    bool shape_set;
    bool tensor_cores_set;

   public:
    gemmSearchOptions();
    ~gemmSearchOptions();
    gemmSearchOptions(const gemmSearchOptions &rhs);

    Error
    setSPAs(kr::spa_t *v, uint32_t ct);
    Error
    setSPA(int compute_capability);
    Error
    setSPA(kr::spa_t v);
    Error
    setALayout(sass_private::layoutType_t lt);
    Error
    setBLayout(sass_private::layoutType_t lt);
    Error
    setCLayout(sass_private::layoutType_t lt);
    Error
    setAclwmType(sass_private::elementDataType_t at);
    Error
    setShape(sass_private::shapeTypeC_t shp);
    Error
    setTensorCores(bool tc);

    Error
    getSPAs(kr::spa_t *v, uint32_t *ct) const;
    Error
    getALayout(sass_private::layoutType_t *_lt) const;
    Error
    getBLayout(sass_private::layoutType_t *_lt) const;
    Error
    getCLayout(sass_private::layoutType_t *_lt) const;
    Error
    getAclwmType(sass_private::elementDataType_t *_at) const;
    Error
    getShape(sass_private::shapeTypeC_t *_shp) const;
    Error
    getTensorCores(bool *_tc) const;
    bool
    isValid() const;
};

class colwTreeHandle {
   private:
    subTree<cask::sasskr::ft_sass_level, cask::Colwolution, cask::ColwShader, cask::ColwShaderList> *cst;
    subTree<cask::sasskr::ft_sass_level,
            cask::ColwolutionDgrad,
            cask::ColwDgradShader,
            cask::ColwDgradShaderList> *dst;
    subTree<cask::sasskr::ft_sass_level,
            cask::ColwolutionWgrad,
            cask::ColwWgradShader,
            cask::ColwWgradShaderList> *wst;

    subTree<cask::xmmakr::ft_xmma_level, cask::Colwolution, cask::ColwShader, cask::ColwShaderList> *xmma_cst;
    subTree<cask::xmmakr::ft_xmma_level,
            cask::ColwolutionDgrad,
            cask::ColwDgradShader,
            cask::ColwDgradShaderList> *xmma_dst;
    subTree<cask::xmmakr::ft_xmma_level,
            cask::ColwolutionWgrad,
            cask::ColwWgradShader,
            cask::ColwWgradShaderList> *xmma_wst;

    subTree<cask::flkr::ft_first_layer_level, cask::Colwolution, cask::ColwShader, cask::ColwShaderList>
        *first_layer_cst;
    subTree<cask::flkr::ft_first_layer_level,
            cask::ColwolutionDgrad,
            cask::ColwDgradShader,
            cask::ColwDgradShaderList> *first_layer_dst;
    subTree<cask::flkr::ft_first_layer_level,
            cask::ColwolutionWgrad,
            cask::ColwWgradShader,
            cask::ColwWgradShaderList> *first_layer_wst;

    subTree<cask::lwtkr::ft_lwtlass_level, cask::Colwolution, cask::ColwShader, cask::ColwShaderList>
        *lwtlass_cst;
    subTree<cask::lwtkr::ft_lwtlass_level,
            cask::ColwolutionDgrad,
            cask::ColwDgradShader,
            cask::ColwDgradShaderList> *lwtlass_dst;
    subTree<cask::lwtkr::ft_lwtlass_level,
            cask::ColwolutionWgrad,
            cask::ColwWgradShader,
            cask::ColwWgradShaderList> *lwtlass_wst;

    bool initialized;

    typedef cask::sasskr::ft_sass_level::search_t search_t;
    typedef cask::xmmakr::ft_xmma_level::search_t xmma_search_t;
    typedef cask::flkr::ft_first_layer_level::search_t first_layer_search_t;
    typedef cask::lwtkr::ft_lwtlass_level::search_t lwtlass_search_t;

    search_t sch;
    xmma_search_t xch;
    first_layer_search_t fch;
    lwtlass_search_t cch;

    // forcibly disable
    colwTreeHandle(const colwTreeHandle &){};

   public:
    colwTreeHandle();
    ~colwTreeHandle();

    friend Error
    allocateSubTrees(colwTreeHandle *p);
    friend Error
    initializeColwTree(colwTreeHandle *p);
    friend Error
    initializeColwTree(colwTreeHandle *p, const kernel_record_t *beg, const kernel_record_t *end);

    friend Error
    initializeColwSearch(colwTreeHandle *h, const colwSearchOptions &so, const cask::Operation::Description &cd);

    template <typename U, record_type_t>
    friend Error
    colwSearch(colwTreeHandle *h, const U &comp, kernel_record_t **lwr, const kernel_record_t *end);

    size_t
    getNumInsertions() const;
    bool
    isInitialized() const {
        return initialized;
    }
    void
    printTree() const {
        if (isInitialized()) {
            std::cout << "colw SASS tree:" << std::endl;
            cst->printTree();

            std::cout << "colw XMMA tree:" << std::endl;
            xmma_cst->printTree();

            std::cout << "colw FIRST_LAYER tree:" << std::endl;
            first_layer_cst->printTree();

            std::cout << "colw LWTLASS tree:" << std::endl;
            lwtlass_cst->printTree();

            std::cout << "wgrad SASS tree:" << std::endl;
            wst->printTree();
            std::cout << "wgrad XMMA tree:" << std::endl;
            xmma_wst->printTree();

            std::cout << "wgrad FIRST_LAYER tree:" << std::endl;
            first_layer_wst->printTree();

            std::cout << "wgrad LWTLASS tree:" << std::endl;
            lwtlass_wst->printTree();

            std::cout << std::endl;
        }
    }
};

class gemmTreeHandle {
   private:
    subTree<cask::sass_gemm_kr::ft_sass_gemm_level, cask::Gemm, cask::GemmShader, cask::GemmShaderList> *gst;
    bool initialized;

    typename cask::sass_gemm_kr::ft_sass_gemm_level::search_t sch;

   public:
    gemmTreeHandle();
    ~gemmTreeHandle();
    gemmTreeHandle(const colwTreeHandle &rhs) = delete;

    friend Error
    initializeGemmTree(gemmTreeHandle *p);
    friend Error
    initializeGemmSearch(gemmTreeHandle *h, const gemmSearchOptions &so, const cask::Operation::Description &cd);

    template <typename U>
    friend Error
    gemmSearch(gemmTreeHandle *h, const U &comp, kernel_record_t **lwr, const kernel_record_t *end);

    size_t
    getNumInsertions() const;
    bool
    isInitialized() const {
        return initialized;
    }
    void
    printTree() const {
        if (isInitialized()) {
            std::cout << "gemm SASS tree:" << std::endl;
            gst->printTree();
        }
    }
};

Error
initializeColwTree(colwTreeHandle *p);
Error
initializeColwTree(colwTreeHandle *p, const kernel_record_t *beg, const kernel_record_t *end);

Error
initializeColwSearch(colwTreeHandle *h, const colwSearchOptions &so, const cask::Operation::Description &cd);
// U is either Colwolution or ColwolutionWgrad
template <typename U, record_type_t>
Error
colwSearch(colwTreeHandle *h, const U &comp, kernel_record_t **lwr, const kernel_record_t *end);

Error
initializeGemmTree(gemmTreeHandle *h);
Error
initializeGemmSearch(gemmTreeHandle *h, const gemmSearchOptions &so, const cask::Operation::Description &cd);
// U is Gemm ; present to support alternatives in the future
template <typename U>
Error
gemmSearch(gemmTreeHandle *h, const U &comp, kernel_record_t **lwr, const kernel_record_t *end);

template <typename S, typename U, typename W, typename V, record_type_t rt>
Error
buildTree(cask::ft::subTree<S, U, W, V> &st) {
    const bool debug = false;

    cask::kernel_record_t kr;
    cask::Colwolution::Error cc_err(cask::Colwolution::Error::OK);

    const V *sl = V::availableShaders();
    for (typename V::const_iterator i = sl->begin(); i != sl->end(); ++i) {
        memset(&kr, 0, sizeof(cask::kernel_record_t));
        cc_err = (*i)->getKernelRecord(&kr);

        if (cask::Colwolution::Error::OK != cc_err) {
            if (debug) {
                std::cerr << "Error getting kernel record for " << (*i)->name.c_str() << "\n";
            }
            continue;
        }

        if (!kr.valid) {
            if (debug) {
                std::cerr << "kernel_record_t for " << (*i)->name.c_str() << " is not valid.\n";
            }
            continue;
        }

        if (rt == kr.tag) {
            cc_err = st.insert(kr);
            if (debug && (cask::Colwolution::Error::OK != cc_err)) {
                std::cerr << "Error inserting kernel " << (*i)->name.c_str() << "into tree.\n";
                continue;
            }
        }
    }

    return Error::OK;
}

}  // ft
}  // cask

#endif
