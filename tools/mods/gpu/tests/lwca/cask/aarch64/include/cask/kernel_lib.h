#ifndef CASK_KERNEL_LIBRARY_H
#define CASK_KERNEL_LIBRARY_H
#include <vector>
#include <string>


namespace cask{

struct KernelLibBuffer {
    const uint8_t* buffer;
    int64_t size;
};
class KernelLibrary {
public:

    KernelLibrary(const KernelLibBuffer* buffers, int64_t length, const cask::HardwareInformation& hardware_info);
    ~KernelLibrary();

    const GemmShaderList* availableGemmShaders() const;
    const ColwShaderList* availableColwShaders() const;
    const DecolwShaderList* availableDecolwShaders() const;
    const ColwDgradShaderList* availableColwDgradShaders() const;
    const ColwWgradShaderList* availableColwWgradShaders() const;
    const GraphShaderList* availableGraphShaders() const;
    const LinkableGemmShaderList* availableLinkableGemmShaders() const;
    const LinkableColwShaderList* availableLinkableColwShaders() const;
    const GettShaderList* availableGettShaders() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
}

#endif
