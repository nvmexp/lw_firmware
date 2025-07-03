#ifndef INCLUDE_GUARD_CASK_CASK_PLATFORM_ALL_H
#define INCLUDE_GUARD_CASK_CASK_PLATFORM_ALL_H


namespace cask {

    
class Platform {
public:
    Platform() { }
    virtual ~Platform() { }

    virtual platformError_t launchKernel(const void* kernel,
                                         dim3 gridDim,
                                         dim3 blockDim,
                                         void** args,
                                         unsigned int shaderMemSize,
                                         platformStream_t stream) = 0;
    virtual platformError_t deviceMalloc(void **devicePtr,
                                         size_t size,
                                         platformStream_t stream) = 0;
    virtual platformError_t deviceMemfree(void *devicePtr,
                                         platformStream_t stream) = 0;
    virtual platformError_t deviceMemset(void* devicePtr,
                                         int32_t val,
                                         size_t size,
                                         platformStream_t stream) = 0;
    virtual platformError_t memcpyHostToDevice(void * devicePtr,
                                               const void* hostPtr,
                                               size_t size,
                                               platformStream_t stream) = 0;
    virtual platformError_t memcpyDeviceToHost(void * hostPtr,
                                               const void* devicePtr,
                                               size_t size,
                                               platformStream_t stream) = 0;
    virtual platformError_t handleLargeSharedMem(const void* kernel,
                                                 size_t sharedMemSize) = 0;

    virtual GemmChip getHostArchitecture() = 0;

    virtual void* caskDynOpen(std::string name) = 0;

    virtual Error kernelLibInit(void* libHandle, Shader*** sassShaders, uint32_t* shaderCount) = 0;    
};

Platform* getPlatform();

}
#endif
