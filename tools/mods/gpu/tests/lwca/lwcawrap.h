/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef LWDA_WRAPPER_H
#define LWDA_WRAPPER_H

#include <map>
#include <string>
#define LWDA_ENABLE_DEPRECATED
#include "lwca.h"
#include "core/include/types.h"
#include "core/include/rc.h"
#include "core/include/memtypes.h"
#include "core/include/tar.h"
#include "core/include/platform.h"
#include "inc/bytestream.h"

class GpuDevice;
class Surface2D;
class GpuSubdevice;
typedef struct LWtoolsContextHandlesRm_st LWtoolsContextHandlesRm;

namespace Lwca {

class Instance;
class Stream;
class Module;
class Event;
class DeviceMemory;
class Array;
class HostMemory;
class ClientMemory;
class ModuleEntity;

//! Compute capability utility class.
class Capability
{
public:
    Capability(int major, int minor) : m_major(major), m_minor(minor) { }
    Capability() = delete;

    enum Enum : UINT32
    {
        SM_50,
        SM_52,
        SM_53,
        SM_60,
        SM_61,
        SM_70,
        SM_72,
        SM_75,
        SM_80,
        SM_86,
        SM_87,
        SM_89,
        SM_90,
        SM_UNKNOWN
    };

    // Implicit user-defined colwersion to enum
    operator const Enum() const { return GetEnum(); }

    int MajorVersion() const { return m_major; }
    int MinorVersion() const { return m_minor; }

    bool operator<(const Enum& cap) const {
        return GetEnum() < cap;
    }
    bool operator<=(const Enum& cap) const {
        return GetEnum() <= cap;
    }
    bool operator>(const Enum& cap) const {
        return GetEnum() > cap;
    }
    bool operator>=(const Enum& cap) const {
        return GetEnum() >= cap;
    }
    bool operator==(const Enum& cap) const {
        return GetEnum() == cap;
    }
    bool operator!=(const Enum& cap) const {
        return GetEnum() != cap;
    }

    Enum GetEnum() const
    {
        int version = GetInt();
        switch (version)
        {
            case 500:
                return SM_50;
            case 502:
                return SM_52;
            case 503:
                return SM_53;
            case 600:
                return SM_60;
            case 601:
                return SM_61;
            case 700:
                return SM_70;
            case 702:
                return SM_72;
            case 705:
                return SM_75;
            case 800:
                return SM_80;
            case 806:
                return SM_86;
            case 807:
                return SM_87;
            case 809:
                return SM_89;
            case 900:
                return SM_90;
            default:
                MASSERT(!"UNSUPPORTED/UNKNOWN SM VERSION");
                return SM_UNKNOWN;
        }
    }

private:
    int GetInt() const { return m_major * 100 + m_minor; }

    int m_major;
    int m_minor;
};

//! LWCA device wrapper.
class Device
{
public:
    enum { IlwalidId = 0xDEAD };

    Device() = default;
    //! Initializes Device from LWCA driver API device handle.
    explicit Device(LWdevice dev) : m_Dev(dev) { }
    //! Determines if the object has been correctly initialized.
    bool IsValid() const { return m_Dev != IlwalidId; }
    void Free() { m_Dev = IlwalidId; }
    //! Determines if the object has been correctly initialized and returns OK if it was.
    RC InitCheck() const;
    //! Returns a string identifying the given device.
    string GetName() const;
    //! Returns the compute capability (major & minor revisions) of the device.
    Capability GetCapability() const;
    //! Returns value of the specified device attribute.
    int GetAttribute(LWdevice_attribute attr) const;
    //! Returns all device properties.
    RC GetProperties(LWdevprop* prop) const;
    //! Returns all device properties.
    //! This function does not print any errors if it can't obtain
    //! device properties.
    //! During bringup RM is often unable to retrieve some properties
    //! on bringup boards and this causes the underlaying LWCA function to fail.
    RC GetPropertiesSilent(LWdevprop* prop) const;
    //! Returns device handle.
    LWdevice GetHandle() const { return m_Dev; }

    UINT32 GetShaderCount() const;

    // Enable usage of device as a key in a stl map
    bool operator<(Device rhs) const { return m_Dev < rhs.m_Dev; }

private:
    LWdevice m_Dev = IlwalidId;

    enum { MaxNameLength = 256 };
};

//! LWCA event wrapper.
class Event
{
public:
    Event()                        = default;
    Event(const Event&)            = delete;
    Event& operator=(const Event&) = delete;
    Event(Event&& event)
        : m_Instance(event.m_Instance)
        , m_Dev(event.m_Dev)
        , m_Stream(event.m_Stream)
        , m_Event(event.m_Event)
        { event.m_Event = 0; }
    Event& operator=(Event&&);
    Event(const Instance* pInstance, Device dev, LWstream stream, LWevent event)
        : m_Instance(pInstance), m_Dev(dev), m_Stream(stream), m_Event(event) { }
    ~Event() { Free(); }
    //! Determines if the object has been correctly initialized.
    bool IsValid() const { return (m_Event != 0); }
    //! Determines if the object has been correctly initialized and returns OK if it was.
    RC InitCheck() const;
    //! Destroys the event.
    void Free();
    //! Records the event in the stream it belongs to.
    //! This function must be ilwoked before one can synchronize to it.
    RC Record() const;
    //! Returns amount of time (ms) elapsed since another event.
    unsigned TimeMsElapsedSince(const Event& event) const;
    float    TimeMsElapsedSinceF(const Event& event) const;
    //! Returns amount of time (ms) elapsed since another event.
    unsigned operator-(const Event& event) const
    {
        return TimeMsElapsedSince(event);
    }
    //! Determines whether the event was triggered.
    bool IsDone() const;
    //! Waits indefinitely for the event to be triggered.
    RC Synchronize() const;
    //! Waits for the event to be triggered.
    RC Synchronize(unsigned timeout_sec) const;

private:
    const Instance* m_Instance = nullptr;
    Device          m_Dev;
    LWstream        m_Stream   = 0;
    LWevent         m_Event    = 0;
};

//! Wrapper for Lwca arrays.
class Array
{
public:
    Array()                        = default;
    Array(const Array&)            = delete;
    Array& operator=(const Array&) = delete;
    Array(Array&& array) : Array() { *this = move(array); }
    Array& operator=(Array&&);
    Array(const Instance* pInstance, Device dev, LWarray array, unsigned width, unsigned height,
          unsigned depth, unsigned bytesPerTexel, unsigned flags)
        : m_Instance(pInstance), m_Dev(dev), m_Array(array), m_Width(width), m_Height(height),
          m_Depth(depth), m_BytesPerTexel(bytesPerTexel), m_Flags(flags) { }
    ~Array() { Free(); }
    //! Determines if the object has been correctly initialized.
    bool IsValid() const { return (m_Array != 0); }
    //! Determines if the object has been correctly initialized and returns OK if it was.
    RC InitCheck() const;
    //! Releases the Lwca array.
    void Free();
    //! Sets contents of the Lwca array using the default stream.
    RC Set(const void* data, unsigned pitch);
    //! Sets contents of the Lwca array using specified stream.
    RC Set(const Stream& stream, const void* data, unsigned pitch);
    //! Returns array handle.
    LWarray GetHandle() const { return m_Array; }

private:
    const Instance* m_Instance      = nullptr;
    Device          m_Dev;
    LWarray         m_Array         = 0;
    unsigned        m_Width         = 0;
    unsigned        m_Height        = 0;
    unsigned        m_Depth         = 0;
    unsigned        m_BytesPerTexel = 0;
    unsigned        m_Flags         = 0;
};

//! Wrapper for LWCA textures.
class Texture
{
public:
    Texture() = default;
    Texture(const Instance* pInst, Device dev, LWtexref texRef) :
        m_Instance(pInst), m_Dev(dev), m_TexRef(texRef) { }
    //! Determines if the object has been correctly initialized.
    bool IsValid() const { return (m_TexRef != 0); }
    //! Determines if the object has been correctly initialized and returns OK if it was.
    RC InitCheck() const;
    //! Set flag
    RC SetFlag(UINT32 flag);
    //! Set filtering mode
    RC SetFilterMode(LWfilter_mode mode);
    //! Set address mode
    RC SetAddressMode(LWaddress_mode, int dim);
    //! Binds array to the texture.
    RC BindArray(const Array& a);
    //! Return texture handle.
    LWtexref GetHandle() const { return m_TexRef; }

private:
    const Instance* m_Instance = nullptr;
    Device          m_Dev;
    LWtexref        m_TexRef   = 0;
};

//! Wrapper for LWCA surfaces.
class Surface
{
public:
    Surface() = default;
    Surface(const Instance* pInst, Device dev, LWsurfref surfRef) :
        m_Instance(pInst), m_Dev(dev), m_SurfRef(surfRef) { }
    //! Determines if the object has been correctly initialized.
    bool IsValid() const { return (m_SurfRef != 0); }
    //! Determines if the object has been correctly initialized and returns OK if it was.
    RC InitCheck() const;
    //! Binds array to the texture.
    RC BindArray(const Array& a);
    //! Return texture handle.
    LWsurfref GetHandle() const { return m_SurfRef; }

private:
    const Instance* m_Instance = nullptr;
    Device          m_Dev;
    LWsurfref       m_SurfRef  = 0;
};

//! LWCA function wrapper.
class Function
{
public:
    Function() = default;
    Function(const Instance* pInst, Device dev, LWfunction func)
        : m_Instance(pInst), m_Dev(dev), m_Function(func),
          m_GridWidth(1), m_GridHeight(1),
          m_BlockWidth(1), m_BlockHeight(1), m_BlockDepth(1), m_SharedMemSize(0) { }
    //! Determines if the object has been correctly initialized.
    bool IsValid() const { return (m_Function != 0); }
    //! Determines if the object has been correctly initialized and returns OK if it was.
    RC InitCheck() const;
    //! Sets grid dimensions.
    void SetGridDim(unsigned width, unsigned height=1)
    {
        m_GridWidth = width;
        m_GridHeight = height;
    }
    //! Sets block dimensions.
    void SetBlockDim(unsigned width, unsigned height=1, unsigned depth=1)
    {
        m_BlockWidth = width;
        m_BlockHeight = height;
        m_BlockDepth = depth;
    }
    //! Sets grid dimension X to the number of SMs (y=0).
    void SetOptimalGridDim(const Instance& inst);
    //! Gets size of dynamic shared memory.
    RC GetSharedSize(unsigned *pBytes);
    //! Sets size of dynamic shared memory.
    RC SetSharedSize(unsigned bytes);
    //! Sets size of shared memory to maximum
    RC SetSharedSizeMax();
    //! Bind texture to the function.
    RC BindTexture(const Texture& tex);
    //! Returns value of the specified function attribute.
    int GetAttribute(LWfunction_attribute attr) const;
    //! Sets value of the specified function attribute.
    RC SetAttribute(LWfunction_attribute attr, int value);
    //! Configure CGA cluster size (Non-portable sizes are supported)
    RC SetClusterDim(unsigned clusterDimX, unsigned clusterDimY = 1, unsigned clusterDimZ = 1);
    //! Overrides the default Maximum Cluster Size Limit
    RC SetMaximumClusterSize(unsigned clusterSize);

    //! Asynchronous function launch with 0 arguments on the default stream.
    RC Launch() const
    {
        return LaunchKernel(GetStream(), nullptr, nullptr);
    }

    //! Asynchronous function launch with 0 arguments on the specified stream.
    RC Launch(const Stream& stream) const
    {
        return LaunchKernel(stream, nullptr, nullptr);
    }

    //! Asynchronous function launch on default stream
    template<typename ... Ts>
    RC Launch(const Ts& ... args) const
    {
        return Launch(GetStream(), args...);
    }

    //! Asynchronous function launch with arguments on the specified stream.
    template<typename ... Ts>
    RC Launch(const Stream& stream, const Ts& ... args) const
    {
        vector<const void*> kernelParams = { &args... };
        return LaunchKernel(stream, const_cast<void**>(kernelParams.data()), nullptr);
    }

    //! Asynchronous cooperative function launch on default stream
    template<typename ... Ts>
    RC CooperativeLaunch(const Ts& ... args) const
    {
        return CooperativeLaunch(GetStream(), args...);
    }

    //! Asynchronous cooperative function launch with arguments on the specified stream.
    template<typename ... Ts>
    RC CooperativeLaunch(const Stream& stream, const Ts& ... args) const
    {
        vector<const void*> kernelParams = { &args... };
        return LaunchCooperativeKernel(stream, const_cast<void**>(kernelParams.data()));
    }

    //! Asynchronous function launch with multiple arguments passed in a buffer
    //! on the default stream
    RC LaunchWithBuffer(const ByteStream& paramBuffer) const
    {
        RC rc;
        return LaunchWithBuffer(GetStream(), paramBuffer);
    }

    //! Asynchronous function launch with multiple arguments passed in a buffer
    //! on the specified stream
    RC LaunchWithBuffer(const Stream& stream, const ByteStream& paramBuffer) const
    {
        RC rc;
        CHECK_RC(InitCheck());

        const char* arg = reinterpret_cast<const char*>(paramBuffer.data());
        size_t size = paramBuffer.size();

        vector<const void*> kernelParams(5);
        kernelParams[0] = LW_LAUNCH_PARAM_BUFFER_POINTER;
        kernelParams[1] = arg;
        kernelParams[2] = LW_LAUNCH_PARAM_BUFFER_SIZE;
        kernelParams[3] = &size;
        kernelParams[4] = LW_LAUNCH_PARAM_END;

        CHECK_RC(LaunchKernel(stream, nullptr, const_cast<void**>(kernelParams.data())));
        return OK;
    }

    RC GetMaxActiveBlocksPerSM(UINT32* pNumBlocks, UINT32 blockSize, size_t dynamicSMemSize);
    RC GetMaxBlockSize(UINT32 *pMaxBlocSize, UINT32 *pMinGridSize, UINT32 blockSizeLimit);
    RC SetL1Distribution(UINT32 Preference);

private:
    RC LaunchKernel(const Stream& stream, void **kernelParams, void **extraParams) const;
    RC LaunchCooperativeKernel(const Stream& stream, void **kernelParams) const;
    const Stream& GetStream() const;

    const Instance* m_Instance      = nullptr;
    Device          m_Dev;
    LWfunction      m_Function      = 0;
    unsigned        m_GridWidth     = 0;
    unsigned        m_GridHeight    = 0;
    unsigned        m_BlockWidth    = 0;
    unsigned        m_BlockHeight   = 0;
    unsigned        m_BlockDepth    = 0;
    unsigned        m_SharedMemSize = 0;
};

//! LWCA host memory object wrapper.
class HostMemory
{
public:
    HostMemory()                             = default;
    HostMemory(const HostMemory&)            = delete;
    HostMemory& operator=(const HostMemory&) = delete;
    HostMemory(HostMemory&& hostMem)
        : m_Instance(hostMem.m_Instance)
        , m_Dev(hostMem.m_Dev)
        , m_HostPtr(hostMem.m_HostPtr)
        , m_Size(hostMem.m_Size)
        { hostMem.m_HostPtr = nullptr; }
    HostMemory& operator=(HostMemory&&);
    HostMemory(const Instance* pInstance, Device dev, void* hostPtr, size_t size)
        : m_Instance(pInstance), m_Dev(dev), m_HostPtr(hostPtr), m_Size(size) { }
    ~HostMemory() { Free(); }
    //! Frees host memory.
    void Free();
    //! Returns host pointer to the allocated host memory.
    void* GetPtr() { return m_HostPtr; }
    //! Returns constant host pointer to the allocated host memory.
    const void* GetPtr() const { return m_HostPtr; }
    //! Returns size of the allocated host memory.
    size_t GetSize() const { return m_Size; }
    RC Clear() { Platform::MemSet(m_HostPtr, 0x0, m_Size); return OK; }
    //! Returns a device usable pointer to host memory allocated by another instance
    //! (For use with shared memory and -conlwrrent_devices)
    UINT64 GetDevicePtr(const Instance* pInst, Device dev);
    //! Returns a device usable pointer to the host memory
    //! (requires UVA or LW_MEMHOSTALLOC_DEVICEMAP flag)
    UINT64 GetDevicePtr(Device dev);
private:
    const Instance* m_Instance = nullptr;
    Device          m_Dev;
    void*           m_HostPtr  = nullptr;
    size_t          m_Size     = 0;
};

class DeviceMemory;

//! LWCA global object wrapper.
class Global
{
public:
    //! Constructs uninitialized global object.
    Global() = default;
    Global(const Instance* pInst, Device dev, UINT64 devicePtr, UINT64 size)
        : m_Instance(pInst), m_Dev(dev), m_DevicePtr(devicePtr), m_Size(size) { }
    //! Determines if the object has been correctly initialized.
    bool IsValid() const { return m_Size > 0; }
    //! Determines if the object has been correctly initialized and returns OK if it was.
    RC InitCheck() const;
    //! Returns 64-bit device pointer to global.
    UINT64 GetDevicePtr() const { return m_DevicePtr; }
    //! Returns size of the global.
    UINT64 GetSize() const { return m_Size; }
    //! Sets a portion of the global with the speficied contents using an asynchronous copy on the default stream.
    RC Set(const void* buf, size_t offset, size_t size);
    //! Returns a portion of the global using an asynchronous copy on the default stream.
    RC Get(void* buf, size_t offset, size_t size) const;
    //! Sets a portion of the global with the speficied contents using an asynchronous copy on the default stream.
    RC Set(const void* buf, size_t size);
    //! Returns a portion of the global using an asynchronous copy on the default stream.
    RC Get(void* buf, size_t size) const;
    //! Asynchronously fills the global with the specified 32-bit value using the default stream.
    RC Fill(UINT32 Pattern);
    //! Asynchronously fills the global with zeroes using the default stream.
    RC Clear();
    //! Asynchronously sets the global to the value of the host memory object using the default stream.
    RC Set(const HostMemory& hostMem);
    //! Asynchronously copies contents of the global to the host memory object using the default stream.
    RC Get(HostMemory* hostMem) const;
    //! Set the global to the specified value using the default stream.
    template<typename T>
    RC Set(const T& arg)
    {
        RC rc;
        CHECK_RC(InitCheck());
        return Set(GetStream(), &arg, sizeof(arg));
    }
    //! Returns the global in a (hopefully POD-type) variable using the default stream.
    template<typename T>
    RC Get(T* arg) const
    {
        RC rc;
        CHECK_RC(InitCheck());
        return Get(GetStream(), arg, sizeof(T));
    }
    //! Sets a portion of the global with the speficied contents using an asynchronous copy on the specified stream.
    RC Set(const Stream& stream, const void* buf, size_t offset, size_t size);
    //! Returns a portion of the global using an asynchronous copy on the specified stream.
    RC Get(const Stream& stream, void* buf, size_t offset, size_t size) const;
    //! Sets a portion of the global with the speficied contents using an asynchronous copy on the specified stream.
    RC Set(const Stream& stream, const void* buf, size_t size)
    {
        return Set(stream, buf, 0, size);
    }
    //! Returns a portion of the global using an asynchronous copy on the specified stream.
    RC Get(const Stream& stream, void* buf, size_t size) const
    {
        return Get(stream, buf, 0, size);
    }
    //! Asynchronously fills the global with the specified 32-bit value using the specified stream.
    RC Fill(const Stream& stream, UINT32 Pattern);
    //! Asynchronously fills the global with zeroes using the specified stream.
    RC Clear(const Stream& stream)
    {
        return Fill(stream, 0);
    }
    //! Asynchronously sets the global to the value of the host memory object using the specified stream.
    RC Set(const Stream& stream, const HostMemory& hostMem)
    {
        return Set(stream, hostMem.GetPtr(), 0, hostMem.GetSize());
    }
    //! Asynchronously copies contents of the global to the host memory object using the specified stream.
    RC Get(const Stream& stream, HostMemory* hostMem) const
    {
        return Get(stream, hostMem->GetPtr(), 0, hostMem->GetSize());
    }
    //! Set the global to the specified value using the specified stream.
    template<typename T>
    RC Set(const Stream& stream, const T& arg)
    {
        return Set(stream, &arg, 0, sizeof(arg));
    }
    //! Returns the global in a (hopefully POD-type) variable using the specified stream.
    template<typename T>
    RC Get(const Stream& stream, T* arg) const
    {
        return Get(stream, arg, 0, sizeof(T));
    }

    //! Set memory from another block of device memory (which could reside on a
    //! peer device)
    RC Set(size_t offset, const DeviceMemory &src, size_t srcOffset, size_t size);

protected:
    void SetMemory(const Instance* Inst, Device dev, UINT64 DevicePtr, UINT64 Size)
    {
        m_Instance  = Inst;
        m_Dev       = dev;
        m_DevicePtr = DevicePtr;
        m_Size      = Size;
    }

    const Stream& GetStream() const;
    const Instance* GetInstance() const { return m_Instance; }
    const Device GetDevice() const { return m_Dev; }

private:
    const Instance* m_Instance  = nullptr;
    Device          m_Dev;
    UINT64          m_DevicePtr = 0;
    UINT64          m_Size      = 0;
};

//! LWCA module wrapper.
class Module
{
public:
    Module()                         = default;
    Module(const Module&)            = delete;
    Module& operator=(const Module&) = delete;
    Module(Module&& module)
        : m_Instance(module.m_Instance)
        , m_Dev(module.m_Dev)
        , m_Module(module.m_Module)
        { module.m_Module = 0; }
    Module& operator=(Module&&);
    Module(const Instance* pInstance, Device dev, LWmodule module)
        : m_Instance(pInstance), m_Dev(dev), m_Module(module) { }
    ~Module() { Unload(); }
    //! Determines if the object has been correctly initialized.
    bool IsValid() const { return (m_Module != 0); }
    //! Determines if the object has been correctly initialized and returns OK if it was.
    RC InitCheck() const;
    //! Unloads the module.
    void Unload();
    //! Returns a "polymorphic" item which can be a function or a global.
    ModuleEntity operator[](const char* name) const;
    //! Returns a function.
    Function GetFunction(const char* name) const;
    //! Returns a function with preset grid and block dimensions.
    Function GetFunction
    (
        const char* name,
        unsigned gridWidth,
        unsigned blockWidth
    ) const;
    //! Returns a global.
    Global GetGlobal(const char* name) const;
    //! Returns a texture.
    Texture GetTexture(const char* name) const;
    //! Returns a surface.
    Surface GetSurface(const char* name) const;

private:
    const Instance* m_Instance  = nullptr;
    Device          m_Dev;
    LWmodule        m_Module    = 0;
};

//! LWCA stream wrapper.
class Stream
{
public:
    Stream()                         = default;
    Stream(const Stream&)            = delete;
    Stream& operator=(const Stream&) = delete;
    Stream(Stream&& stream)
        : m_Instance(stream.m_Instance)
        , m_Dev(stream.m_Dev)
        , m_Stream(stream.m_Stream)
        {
          stream.m_Stream = 0;
          stream.m_Instance = nullptr;
          stream.m_Dev = Device();
        }
    Stream& operator=(Stream&&);
    Stream(const Instance* pInstance, Device dev, LWstream stream)
        : m_Instance(pInstance), m_Dev(dev), m_Stream(stream) { }
    ~Stream() { Free(); }
    //! Determines if the object has been correctly initialized.
    bool IsValid() const { return (m_Stream != 0); }
    //! Determines if the object has been correctly initialized and returns OK if it was.
    RC InitCheck() const;
    //! Deletes the stream object.
    void Free();
    //! Waits indefinitely for all kernels on the stream to finish.
    RC Synchronize() const;
    //! Waits for all kernels on the stream to finish.
    RC Synchronize(unsigned timeout_sec) const;
    //! Creates an event on the stream to which one can synchronize.
    Event CreateEvent() const;
    //! Returns stream handle.
    LWstream GetHandle() const { return m_Stream; }

private:
    const Instance* m_Instance = nullptr;
    Device          m_Dev;
    LWstream        m_Stream   = 0;
};

//! LWCA instance and context wrapper.
class Instance
{
public:

    Instance()                           = default;
    Instance(const Instance&)            = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&& instance) : Instance() { *this = move(instance); }
    Instance& operator=(Instance&&);
    ~Instance() { Free(); }
    //! Initialize the LWCA driver API.
    RC Init();
    //! Destroy the LWCA instance.
    void Free();
    //! Returns a device given an ordinal in the range [0, GetDeviceCount()-1].
    Device GetDevice(int ordinal) const;
    //! Finds device whose subdevice 0 has the specified BAR0 offset.
    RC FindDevice(GpuDevice& gpuDev, Device* dev) const;
    //! Returns the number of available device with compute capability >= 1.0.
    unsigned GetDeviceCount() const;
    RC GetMemInfo(Device dev, size_t *pFreeMem, size_t *pTotalMem) const;
    //! Initializes one primary context plus numSecondaryCtx LWCA contexts with the LWCA device.
    //! The primary context is always index 0 and has special properties. All other contexts from
    //! 1..numSecondaryCtx are secondary.
    RC InitContext(Device dev,                //!< the LWCA device to use for initialization
                   int numSecondaryCtx = 0,   //!< number of additional contexts you want created.
                   int activeContextIdx = 0); //!< the context to be active after initialization
    //! Initializes one primary context plus numSecondaryCtx LWCA contexts with the GPU device.
    //! The primary context is always index 0 and has special properties. All other contexts from
    //! 1..numSecondaryCtx are secondary.
    RC InitContext(GpuDevice* pGpuDev,        //!< the gpu device to use for initialization
                   int numSecondaryCtx = 0,   //!< number of additional contexts you want created.
                   int activeContextIdx = 0); //!< the context to be active after initialization
    //! Determines whether context was initialized for the current device.
    RC InitCheck() const { return InitCheck(GetLwrrentDevice()); }
    //! Determines whether context was initialized for the specified device.
    RC InitCheck(Device dev) const;
    //! Determines whether context was initialized for the specified device.
    bool IsContextInitialized(Device dev) const;
    //! Enable/disable dynamic partitioning of sub-contexts
    RC SetSubcontextState(Device dev, UINT32 state);
    RC GetSubcontextState(Device  dev, UINT32 *pState);
    UINT32 GetNumContexts() const { return m_NumSecondaryCtx+1; }
    //! Switches between sub-contexts
    RC SwitchContext(const Device& dev, int ctxIdx);
    //! Asserts if the specified device is invalid.
    bool AssertValid(Device dev) const;
    //! Returns device attached to the context.
    Device GetLwrrentDevice() const;
    //! Loads module from from a tar.gz archive containing all lwbins.
    RC LoadModule(const char* name, Module* pModule) const
        { return LoadModule(GetLwrrentDevice(), name, pModule); }
    RC LoadModule(Device dev, const char* name, Module* pModule) const;
    //! Loads newest supported module from a tar.gz archive containing all lwbins.
    RC LoadNewestSupportedModule(const char* nameBegin, Module* pModule) const
    {
        return LoadNewestSupportedModule(GetLwrrentDevice(), nameBegin, pModule);
    }
    RC LoadNewestSupportedModule(Device dev, const char* nameBegin, Module* pModule) const;
    //! Checks if a lwca module is present
    bool IsLwdaModuleSupported(const char* nameBegin) const;
    bool IsLwdaModuleSupported(Device dev, const char* nameBegin) const;
    //! Allocates specified amount of device memory.
    //! If the return code is not OK, pOut cannot be garanteed to be in a valid state.
    RC AllocDeviceMem(UINT64 size, DeviceMemory* pOut) const;
    RC AllocDeviceMem(Device dev, UINT64 size, DeviceMemory* pOut) const;
    //! Allocates 1D array in device memory.
    Array AllocArray
    (
        unsigned       width,
        LWarray_format format,
        unsigned       channels,
        unsigned       flags
    ) const
    {
        return AllocArray(GetLwrrentDevice(), width, 0, 0, format, channels, flags);
    }
    Array AllocArray
    (
        Device         dev,
        unsigned       width,
        LWarray_format format,
        unsigned       channels,
        unsigned       flags
    ) const
    {
        return AllocArray(dev, width, 0, 0, format, channels, flags);
    }
    //! Allocates 2D array in device memory.
    Array AllocArray
    (
        unsigned       width,
        unsigned       height,
        LWarray_format format,
        unsigned       channels,
        unsigned       flags
    ) const
    {
        return AllocArray(GetLwrrentDevice(), width, height, 0, format, channels, flags);
    }
    Array AllocArray
    (
        Device         dev,
        unsigned       width,
        unsigned       height,
        LWarray_format format,
        unsigned       channels,
        unsigned       flags
    ) const
    {
        return AllocArray(dev, width, height, 0, format, channels, flags);
    }
    //! Allocates 3D array in device memory.
    Array AllocArray
    (
        unsigned       width,
        unsigned       height,
        unsigned       depth,
        LWarray_format format,
        unsigned       channels,
        unsigned       flags
    ) const
    {
        return AllocArray(GetLwrrentDevice(), width, height, depth, format, channels, flags);
    }
    Array AllocArray
    (
        Device         dev,
        unsigned       width,
        unsigned       height,
        unsigned       depth,
        LWarray_format format,
        unsigned       channels,
        unsigned       flags
    ) const;
    //! Allocates specified amount of host memory.
    //! If not specified, use flags allowing hostmem to be accessible by multiple GPUs
    //! (multiple contexts, device-mapped)
    RC AllocHostMem(size_t size, HostMemory* pOut) const
        { return AllocHostMem(GetLwrrentDevice(), size, pOut); }
    RC AllocHostMem(Device dev, size_t size, HostMemory* pOut) const
        { return AllocHostMem(dev, size, LW_MEMHOSTALLOC_PORTABLE | LW_MEMHOSTALLOC_DEVICEMAP, pOut); }
    RC AllocHostMem(Device dev, size_t size, UINT32 flags, HostMemory* pOut) const;
    //! Allocates memory into Surface2D and maps it into Client Mem
    // The Surface2D allocates the memory in Pitch Linear layout
    // with Pixel Size of 4 bytes
    RC AllocSurfaceClientMem
    (
        UINT32           sizeBytes,
        Memory::Location loc,
        GpuSubdevice*    pSubdev,
        Surface2D*       pSurf,
        ClientMemory*    pClientMem
    ) const;
    //! Maps client memory handle into LWCA context.
    ClientMemory MapClientMem
    (
        UINT32           memHandle,
        UINT64           offset,
        UINT64           size,
        GpuDevice*       pGpuDevice,
        Memory::Location loc = Memory::Fb
    ) const;
    ClientMemory MapClientMem
    (
        Device           dev,
        UINT32           memHandle,
        UINT64           offset,
        UINT64           size,
        GpuDevice*       pGpuDevice,
        Memory::Location loc = Memory::Fb
    ) const;
    //! Creates an event on the default device (default stream), to which one can synchronize.
    Event CreateEvent() const { return GetStream(GetLwrrentDevice()).CreateEvent(); }
    //! Creates an event on the specified device (default stream), to which one can synchronize.
    Event CreateEvent(Device dev) const { return GetStream(dev).CreateEvent(); }
    //! Creates additional stream on the default device.
    Stream CreateStream() const { return CreateStream(GetLwrrentDevice()); }
    //! Creates additional stream on the specified device.
    Stream CreateStream(Device dev) const;
    //! Returns default stream.
    const Stream& GetStream(Device dev) const;
    //! Waits indefinitely for all kernels on the device context to finish.
    RC SynchronizeContext() const;
    RC SynchronizeContext(Device dev) const;
    //! Waits indefinitely for all kernels on the default stream to finish.
    RC Synchronize() const;
    RC Synchronize(Device dev) const;
    //! Waits for all kernels on the default stream to finish.
    RC Synchronize(unsigned timeout_sec) const;
    RC Synchronize(Device dev, unsigned timeout_sec) const;
    //! Polls while waiting for all kernels on the default stream to finish.
    RC SynchronizePoll(unsigned timeoutMs) const;
    RC SynchronizePoll(Device dev, unsigned timeoutMs) const;
    //! Enables or disables automatic synchronization after asynchronous LWCA API calls.
    //! By default synchronization is disabled and LWCA API calls are trully asynchronous.
    void EnableAutoSync(bool sync)
    {
        m_AutoSync = sync;
    }
    //! Determines whether automatic synchronization is enabled.
    bool IsAutoSyncEnabled() const
    {
        return m_AutoSync;
    }
    RC GetComputeEngineChannel(UINT32 *hClient,
                               UINT32 *hChannel);
    RC GetComputeEngineChannel(Device dev,
                               UINT32 *hClient,
                               UINT32 *hChannel);
    RC GetComputeEngineChannelGroup(UINT32 *hChannel);
    RC GetContextHandles(LWtoolsContextHandlesRm *pContextHandlesRm);
    RC GetContextHandles(Device dev,
                         LWtoolsContextHandlesRm *pContextHandlesRm);
    //! Gives access to LWBIN files in the LWCA archive
    const TarFile* GetLwbin(const string& name) const;
    //! Return LWcontext handle
    LWcontext GetHandle(Device dev) const;

    RC EnablePeerAccess(Device dev, Device peerDev) const;
    RC DisablePeerAccess(Device dev, Device peerDev) const;

    RC EnableLoopbackAccess(Device dev) const;
    RC DisableLoopbackAccess(Device dev) const;

private:
    Stream CreateStreamInternal(Device dev) const;

    struct PerDevInfo
    {
        Device    m_Dev;
        Instance* m_Inst;
        vector<LWcontext> m_Contexts; //[0]=primary, [1..n]=secondary
        vector<Stream>    m_Streams;  //[0]=primary, [1..n]=secondary
        Module    m_Tools;
        UINT32    m_LoopbackRefCount;

        PerDevInfo(Device dev, Instance* Inst) 
            : m_Dev(dev), m_Inst(Inst), m_LoopbackRefCount(0) { }
        ~PerDevInfo();
        PerDevInfo(const PerDevInfo&)            = delete;
        PerDevInfo& operator=(const PerDevInfo&) = delete;
        PerDevInfo(PerDevInfo&& info)
            : m_Dev(info.m_Dev)
            , m_Inst(info.m_Inst)
            , m_Contexts(move(info.m_Contexts))
            , m_Streams(move(info.m_Streams))
            , m_Tools(move(info.m_Tools))
            , m_LoopbackRefCount(info.m_LoopbackRefCount)
        {
            info.m_Dev = Device();
        }
        PerDevInfo& operator=(PerDevInfo&& info)
        {
            m_Contexts = move(info.m_Contexts);
            m_Streams  = move(info.m_Streams);
            m_Tools    = move(info.m_Tools);
            info.m_Dev = Device();
            m_LoopbackRefCount = info.m_LoopbackRefCount;
            m_Inst = move(info.m_Inst);
            return *this;
        }
        void Free();
    };

    bool m_Init              = false;
    bool m_AutoSync          = false;
    mutable map<Device, PerDevInfo> m_DeviceInfo;
    TarArchive m_Lwbins;
    TarArchive m_InternalLwbins;
    bool m_LwbinsInitialized = false;
    bool m_UseDefaultContext = true;
    int m_NumSecondaryCtx = 0;  // how may secondary contexts to create (max = 64)
    int m_ActiveContextIdx = 0; // which context should be active after initializing all contexts
};

//! An item belonging to a module, which can be either a function or a global.
class ModuleEntity
{
public:
    ModuleEntity(const Module& Module, const char* Name)
        : m_Module(&Module), m_Name(Name) { }

    //! Treat module entity as a function.
    operator Function() const
    {
        return m_Module->GetFunction(m_Name.c_str());
    }
    //! Treat module entity as a function.
    Function operator()(unsigned gridDim, unsigned blockDim) const
    {
        Function f = m_Module->GetFunction(m_Name.c_str());
        if (f.IsValid())
        {
            f.SetBlockDim(blockDim);
            f.SetGridDim(gridDim);
        }
        return f;
    }
    //! Treat module entity as a global.
    operator Global() const
    {
        return m_Module->GetGlobal(m_Name.c_str());
    }
    //! Treat module entity as a global.
    template<typename T>
    Global operator=(const T& value)
    {
        Global g = m_Module->GetGlobal(m_Name.c_str());
        if (g.IsValid())
        {
            g.Set(value);
        }
        return g;
    }
    //! Treat module entity as a texture.
    operator Texture() const
    {
        return m_Module->GetTexture(m_Name.c_str());
    }
    //! Treat module entity as a surface.
    operator Surface() const
    {
        return m_Module->GetSurface(m_Name.c_str());
    }

private:
    const Module* m_Module;
    string m_Name;
};

//! LWCA device memory object wrapper.
class DeviceMemory : public Global
{
public:
    DeviceMemory()                               = default;
    DeviceMemory(const DeviceMemory&)            = delete;
    DeviceMemory& operator=(const DeviceMemory&) = delete;
    DeviceMemory(DeviceMemory&& devMem)
        : Global(devMem), m_LoopbackDevicePtr(devMem.m_LoopbackDevicePtr)
    {
        devMem.SetMemory(nullptr, Device(), 0, 0);
    }
    DeviceMemory& operator=(DeviceMemory&&);
    DeviceMemory(const Instance* pInst, Device dev, UINT64 devicePtr, UINT64 size,
                 UINT64 loopbackDevicePtr)
        : Global(pInst, dev, devicePtr, size), m_LoopbackDevicePtr(loopbackDevicePtr) { }
    ~DeviceMemory() { Free(); }

    //! Frees allocated device memory.
    void Free();

    RC MapLoopback();
    RC UnmapLoopback();
    UINT64 GetLoopbackDevicePtr() { return m_LoopbackDevicePtr; }

private:
    UINT64 m_LoopbackDevicePtr = 0;
};

//! Wrapper for client-allocated memory accessible to LWCA.
class ClientMemory: public Global
{
public:
    ClientMemory()                               = default;
    ClientMemory(const ClientMemory&)            = delete;
    ClientMemory& operator=(const ClientMemory&) = delete;
    ClientMemory(ClientMemory&& devMem)
        : Global(devMem)
    {
        devMem.SetMemory(nullptr, Device(), 0, 0);
    }
    ClientMemory& operator=(ClientMemory&&);
    ClientMemory(const Instance* pInst, Device dev, UINT64 devicePtr, UINT64 size)
        : Global(pInst, dev, devicePtr, size) { }
    ~ClientMemory() { Free(); }
    //! Frees client memory object mapping.
    void Free();
};

//! Auxiliary class for host memory allocator.
class HostMemoryAllocatorBase
{
protected:
    static void* Allocate(const Instance *pInst, const Lwca::Device dev, unsigned size);
    static void Free(const Instance *pInst, const Lwca::Device dev, void* ptr);
};

//! Allocator type which allocates host memory for use in LWCA.
//! This allocator can be used in STL containers.
template<typename T>
class HostMemoryAllocator: public HostMemoryAllocatorBase
{
public:
    typedef T          value_type;
    typedef T*         pointer;
    typedef T&         reference;
    typedef const T*   const_pointer;
    typedef const T&   const_reference;
    typedef size_t     size_type;
    typedef ptrdiff_t  difference_type;
    const Instance    *m_Instance;
    Device             m_Dev;

    HostMemoryAllocator() = delete;
    HostMemoryAllocator(const Lwca::Instance *pInst, Lwca::Device dev) noexcept
        : m_Instance(pInst), m_Dev(dev) { }
    HostMemoryAllocator(const HostMemoryAllocator& rhs) noexcept
    {
        m_Instance = rhs.m_Instance;
        m_Dev      = rhs.m_Dev;
    }
    template<typename U>
    HostMemoryAllocator(const HostMemoryAllocator<U>& rhs) noexcept
    {
        m_Instance = rhs.m_Instance;
        m_Dev      = rhs.m_Dev;
    }
    ~HostMemoryAllocator() { }
    pointer address(reference val) noexcept { return &val; }
    const_pointer address(const_reference val) const noexcept { return &val; }
    size_type max_size() const noexcept { return ~0U/sizeof(T); }
    void construct(pointer p) { new((void*)p) T(); }
    void construct(pointer p, const_reference val) { new((void*)p) T(val); }
    void destroy(pointer p) { p->~T(); }
    template<typename U>
    struct rebind
    {
        typedef HostMemoryAllocator<U> other;
    };
    pointer allocate(size_type size, allocator<void>::const_pointer hint=0)
    {
        return reinterpret_cast<pointer>(Allocate(m_Instance, m_Dev,
                    static_cast<unsigned>(size*sizeof(T))));
    }
    void deallocate(pointer ptr, size_type size)
    {
        Free(m_Instance, m_Dev, const_cast<void*>(static_cast<const void*>(ptr)));
    }
    bool operator==(const HostMemoryAllocatorBase&) const
    {
        return true;
    }
    bool operator!=(const HostMemoryAllocatorBase&) const
    {
        return false;
    }
};

LWresult PrintResult(const char* function, LWresult result);
RC LwdaErrorToRC(LWresult result);

#define LW_RUN(func) Lwca::PrintResult(#func, (func))
#define CHECK_LW_RC(func) CHECK_RC(Lwca::LwdaErrorToRC(LW_RUN(func)))

#ifdef INCLUDE_LWDART
RC InitLwdaRuntime(const Device& dev);
RC FreeLwdaRuntime(const Device& dev);
#endif

} // namespace Lwca

#endif // LWDA_WRAPPER_H
