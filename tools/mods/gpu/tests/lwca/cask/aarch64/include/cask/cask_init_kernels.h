#ifndef INCLUDE_GUARD_CASK_INIT_KERNELS_H
#define INCLUDE_GUARD_CASK_INIT_KERNELS_H

#include <cask/cask.h>

namespace cask {
// brief: Manifest of initialized CASK kernels
// This needs to be passed as reference in initCaskKernels CASK API
class Manifest {
public:
    // Singleton constructor
    static Manifest *GetInstance();

    // Add shader to shader_instances
    void emplaceShaderInstance(std::unique_ptr<cask::Shader> shader);
    // Add Linkable shader to linkable_shader_instances
    void emplaceLinkableShaderInstance(std::unique_ptr<cask::LinkableShader> shader);

    // Delete copy/move constuctors
    Manifest(const Manifest&) = delete;
    Manifest& operator=(const Manifest&) = delete;
    Manifest(Manifest&&) = delete;
    Manifest& operator=(Manifest&&) = delete;
    
private:
    Manifest() = default;
    std::vector<std::unique_ptr<cask::Shader>> shader_instances_;
    std::vector<std::unique_ptr<cask::LinkableShader>> linkable_shader_instances_;
};



// cask API
CASK_INIT_DLL_EXPORT void initCaskKernels(cask::Manifest &);

} // end namespace cask

#endif /*INCLUDE_GUARD_CASK_INIT_KERNELS_H */
