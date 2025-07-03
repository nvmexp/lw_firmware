#ifndef INCLUDE_GUARD_CASK_LWDA_PLATFORM_H
#define INCLUDE_GUARD_CASK_LWDA_PLATFORM_H

#include <lwda_runtime_api.h>   // for lwdaError_t and lwdaStream_t


#include "cask.h"


namespace cask {

class LwdaPlatform : public Platform{
public:
    LwdaPlatform() { }
    virtual ~LwdaPlatform() { }

    virtual platformError_t launchKernel(const void* kernel,
                                         dim3 gridDim,
                                         dim3 blockDim,
                                         void** args,
                                         unsigned int shaderMemSize,
                                         platformStream_t stream);
    virtual platformError_t deviceMalloc(void **devicePtr, size_t size, platformStream_t stream);
    virtual platformError_t deviceMemfree(void *devicePtr, platformStream_t stream);
    virtual platformError_t deviceMemset(void* devicePtr, int32_t val, size_t size, platformStream_t stream);
    virtual platformError_t memcpyHostToDevice(void * devicePtr, const void* hostPtr, size_t size, platformStream_t stream);
    virtual platformError_t memcpyDeviceToHost(void * hostPtr, const void* devicePtr, size_t size, platformStream_t stream);
    virtual platformError_t handleLargeSharedMem(const void* kernel, size_t sharedMemSize);
    virtual GemmChip getHostArchitecture();
    virtual void* caskDynOpen(std::string name);
    virtual Error kernelLibInit(void* libHandle, Shader*** sassShaders, uint32_t* shaderCount);
};

}

#endif
