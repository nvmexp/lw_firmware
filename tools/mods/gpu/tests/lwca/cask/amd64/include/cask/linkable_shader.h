#ifndef INCLUDE_GUARD_CASK_LINKABLE_GEMM_H
#define INCLUDE_GUARD_CASK_LINKABLE_GEMM_H

#include <iostream>
#include <string>
#include <vector>

#include <lwca.h>

#include <cask/abi_info.h>

namespace cask {

enum class LinkOption {
    UseDriver = 0,
    UseLinker,
};

////////////////////////////////////////////////////////////////////////////////

// LinkableShader defination
class LinkableShader {
protected:
    const sass_private::ShaderParams* shaderParams;
    const KernelInfo* kernelInfo;
    const AbiInfo* info;
    //lwbin position pointer in data section
    const char* start;
    const char* end;
    mutable int linkedLwbinSize;
    LinkOption linkOp;
public:
    typedef cask::Error Error;  // for existing code relying on Shader::Error
    const std::string  name;
    const uint64_t handle;
    // constructor used in instance in .lw file
    LinkableShader(const sass_private::ShaderParams* sP, const KernelInfo* kI, const AbiInfo* aI, const char* start_, const char* end_);
    virtual ~LinkableShader() {}

    // return the device function name
    virtual const char*  queryLinkageSymbol() const { return "activation"; }
    virtual void init();
    virtual const KernelInfo* getKernelInfo() const { return kernelInfo;}
    const BaseKernelInfo* getBaseInfo() const;
    virtual int getABIRegisterStart() const { return 0; }
    virtual int getABIRegisterNum() const { return 0; }
    virtual int getABIReturnRegister() const { return 0; }
    virtual const int*  getABIScratchRegs(size_t & size) const { size = 0; return nullptr; }
    virtual void setLinkOption(LinkOption op);
    virtual LinkOption getLinkOption()const { return linkOp;}
    virtual cask::ScalarType getAccType() const =0;

    //pure virtual function

    // runtime para APIs
    virtual int queryRuntimeParaAddrStart() const { return 0; }

    virtual int queryRuntimeParaNumber() const { return 0; }

    virtual int queryRuntimeSharedStart() const { return 0; };
//    void serialization(void* buffer) const;
//    static LinkableShader<ShaderType>*  deserialization(void* buffer, int* size);

};
// end of linkable shader defination
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// linkable gemm shader
class LinkableGemmShader: public LinkableShader {
public:
    //ctor
    LinkableGemmShader(const sass_private::ShaderParams* sP, const KernelInfo* kI, const AbiInfo* aI, const char* start_, const char* end_);
    // binary + device fusion pattern, usually used in SASS + PTX
    virtual GemmShader*  link(const char* ptx, const unsigned ptxSize, const char* deviceName) const = 0;
    // source level fusion pattern, usually used in XMMA, needs more ilwestigation
    virtual void*  generate() const = 0;

    virtual void init() override;
    virtual GemmChip chip() const;

    virtual Error canImplement(const Gemm& operation) const;

    virtual const GemmKernelInfo* getInfo() const;
    virtual cask::ScalarType getAccType() const override;

        // return the serialization size
    virtual int64_t getSerializedSize( const GemmShader* shader) const { return static_cast<int64_t>(shader->linkedLwbin.capacity()); }

    virtual void serialize(char* buffer, int64_t size, const GemmShader* shader) const;

    virtual GemmShader* deserialize(const char* buffer, int64_t size) const = 0;

    void setUsePTX(bool flag) { usePTX = flag;}
    bool getUsePTX() const { return usePTX; }
private:
    bool usePTX;

};

// define shader list
typedef ShaderList<LinkableGemmShader, Gemm> LinkableGemmShaderList;

inline const LinkableGemmShaderList* availableLinkableGemmShaders() {
    return LinkableGemmShaderList::availableShaders();
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// linkable colwolution shader
class LinkableColwShader: public LinkableShader {
public:
    //ctor
    LinkableColwShader(const sass_private::ShaderParams* sP, const KernelInfo* kI, const AbiInfo* aI, const char* start_, const char* end_);
    // binary + device fusion pattern, usually used in SASS + PTX
    virtual ColwShader*  link(const char* ptx, const unsigned ptxSize,  const char* deviceName) const = 0;

    // source level fusion pattern, usually used in XMMA, needs more ilwestigation
    virtual void*  generate() const = 0;

    virtual void init() override;
    virtual GemmChip chip() const;

    virtual Error canImplement(const Colwolution& operation) const;

    virtual const ColwKernelInfo* getInfo() const;
    virtual cask::ScalarType getAccType() const override;

        // return the serialization size
    virtual int32_t getSerializedSize(const ColwShader* shader) const { return static_cast<int32_t>(shader->linkedLwbin.capacity()); }

    virtual void serialize(char* buffer, int64_t size, const ColwShader* shader) const;

    virtual ColwShader* deserialize(const char* buffer, int64_t size) const = 0;

    void setUsePTX(bool flag) { usePTX = flag;}
    bool getUsePTX() const { return usePTX; }
private:
    bool usePTX;
    friend class LinkableColwShaderWrapper;
};

// define shader list
typedef ShaderList<LinkableColwShader, Colwolution> LinkableColwShaderList;

inline const LinkableColwShaderList* availableLinkableColwShaders() {
    return LinkableColwShaderList::availableShaders();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//APIs to build fusion APIs
// get compute ability
LWjit_target getCompilationTarget(cask::GemmChip family);

// load lwbin file
 std::vector<char> loadFile(const char* start, const char* end);

// write lwbin file
 void writeFile(const char* filename, const std::vector<char>& data);

// link lwbin with PTX
 std::vector<char> linkKernel(const std::vector<char>& linkableKernel,
                                    LWjitInputType kernelType,
                                    const std::vector<char>& callback,
                                    LWjitInputType callbackType,
                                    LWjit_target target,
                                    const char* kernelName,
                                    const char* deviceName,
                                    LinkOption op);

// string substitution
std::string sub(std::string in, const char* v1, const char* v2);
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// end namespace cask
}

#endif
