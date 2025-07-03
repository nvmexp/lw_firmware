#ifndef INLLWDE_GUARD_CASK_KLIB_LOADRE_H
#define INLLWDE_GUARD_CASK_KLIB_LOADRE_H

#include <cask/cask.h>

namespace cask {


class KernelLibLoader {
public:
    KernelLibLoader(const uint8_t* buffer, int64_t size);

    int64_t getKernelCount() const;

    KernelInfo* loadKernelInfo(int64_t index) const;

    uint64_t getKernelHandle(int64_t index) const;

    OpDefinition getOpDefinition(int64_t index) const;

    template<typename ShaderT>
    ShaderT* loadShader(int64_t index) const {
        if (index >= this->getKernelCount()) {
            return nullptr;
        }
        return ShaderT::deserialize(buffer_ + getKernelOffset(index), getKernelSize(index));
    }

    int64_t getKernelOffset(int64_t index) const;

    int32_t getKernelSize(int64_t index) const;

private:
    const uint8_t* buffer_;
    int64_t size_;

};
}
#endif
