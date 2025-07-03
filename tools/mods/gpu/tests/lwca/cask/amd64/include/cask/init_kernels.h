#ifndef INCLUDE_GUARD_INIT_KERNELS_H
#define INCLUDE_GUARD_INIT_KERNELS_H
#include <cask/cask.h>

namespace cask {

#if defined(LWTLASS_CASK_PLUGIN_EXPLICIT_KERNEL_INSTS)
    void initialize_lwtlass_kernels(cask::Manifest &manifest);

namespace lwtlass_cask_plugin {

    void initialize_lwtlass_static_kernels(cask::Manifest &manifest);
    void initialize_lwtlass_generator_kernels(cask::Manifest &manifest);
    void initialize_lwtlass_cask_plugin_bladeworks_kernels(cask::Manifest &manifest);

}

#endif // #if defined(LWTLASS_CASK_PLUGIN_EXPLICIT_KERNEL_INSTS)

#if defined(CASK_CORE_EXPLICIT_KERNEL_INSTS)

    void initialize_cask_core_reference_kernels(cask::Manifest &manifest);

namespace cask_core_cask_plugin {

    void initialize_cask_core(cask::Manifest &manifest);

} // end namespace cask_core_cask_plugin

#endif

#if defined(XMMA_EXPLICIT_KERNEL_INSTS)
    void initialize_xmma_kernels(cask::Manifest &manifest);

namespace xmma_cask_plugin{

    void initialize_xmma_colw_dgrad(cask::Manifest &manifest);
    void initialize_xmma_colw_fprop(cask::Manifest &manifest);
    void initialize_xmma_colw_wgrad(cask::Manifest &manifest);
    void initialize_xmma_gemm(cask::Manifest &manifest);
    void initialize_xmma_gett(cask::Manifest &manifest);
    void initialize_xmma_elementwise(cask::Manifest &manifest);
    void initialize_xmma_misc(cask::Manifest &manifest);
    void initialize_xmma_softmax(cask::Manifest &manifest);
    void initialize_xmma_graph(cask::Manifest &manifest);
    void initialize_xmma_bn_fusion(cask::Manifest &manifest);

} // namespace xmma

#endif
#if defined(LWDA_STANDALONE_EXPLICIT_KERNEL_INSTS)

    void initialize_lwda_standalone_kernels(cask::Manifest &manifest);

#endif

} // namespace cask

namespace cask_safe {

#if defined(LWDA_STANDALONE_EXPLICIT_KERNEL_INSTS)

namespace lwtlass_cask_plugin {

    void kernelInit(cask::Manifest &manifest);

} // namespace lwtlass_cask_plugin

#endif

} // namespace cask_safe

#endif /* #ifndef INCLUDE_GUARD_INIT_KERNELS_H */ 
