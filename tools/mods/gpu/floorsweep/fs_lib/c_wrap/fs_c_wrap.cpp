#include "fs_lib.h"
#include <iostream>
#include <cassert>

#ifdef _MSC_VER
#pragma warning(disable : 4068) // disable warnings about gcc pragmas
#endif
#pragma GCC diagnostic ignored "-Wunused-value"


// This file is a c wrapper for interfacing with fs_lib
// It gets built into fs_lib.so, which can be loaded by python

struct IsFSValidCall{
    fslib::Chip chip;
    fslib::FSmode mode = fslib::FSmode::PRODUCT;
    fslib::FbpMasks fbpMasks;
    fslib::GpcMasks gpcMasks;
    std::string errMsg;
};


fslib::Chip getChipEnum(const char * name){
    std::string nameStr(name);
    if (fslib::getStrToChipMap().find(nameStr) == fslib::getStrToChipMap().end()) {
        std::cerr << nameStr << " is not supported in fslib!" << std::endl;
        assert(("unsupported chip!", 0));
    }
    return fslib::getStrToChipMap().find(nameStr)->second; // do something here
}

// allocate and return a pointer to a IsFSValidCall obj
extern "C" IsFSValidCall*  allocIsValidStruct() {
    return new IsFSValidCall();
}

// free the memory
extern "C" void freeIsValidStruct(IsFSValidCall* input) {
    delete input;
}

extern "C" void printFSCallStruct(IsFSValidCall* input){
    //std::cout << "this got called! " << input  << std::endl;
    std::cout << "chip: " << static_cast<int>(input->chip) << std::endl;
    std::cout << "fbpMask: " << std::hex << input->fbpMasks.fbpMask << std::endl;
    for (unsigned int i = 0; i < sizeof(input->fbpMasks.fbioPerFbpMasks)/sizeof(input->fbpMasks.fbioPerFbpMasks[0]); i++){
        std::cout << "fbioMask[" << std::dec << i << "]: " << std::hex << input->fbpMasks.fbioPerFbpMasks[i] << std::endl;
    }
    for (unsigned int i = 0; i < sizeof(input->fbpMasks.fbpaPerFbpMasks)/sizeof(input->fbpMasks.fbpaPerFbpMasks[0]); i++){
        std::cout << "fbpaMask[" << std::dec << i << "]: " << std::hex << input->fbpMasks.fbpaPerFbpMasks[i] << std::endl;
    }
    for (unsigned int i = 0; i < sizeof(input->fbpMasks.halfFbpaPerFbpMasks)/sizeof(input->fbpMasks.halfFbpaPerFbpMasks[0]); i++){
        std::cout << "halfFbpa[" << std::dec << i << "]: " << std::hex << input->fbpMasks.halfFbpaPerFbpMasks[i] << std::endl;
    }
    for (unsigned int i = 0; i < sizeof(input->fbpMasks.ltcPerFbpMasks)/sizeof(input->fbpMasks.ltcPerFbpMasks[0]); i++){
        std::cout << "ltcPerFBP[" << std::dec << i << "]: " << std::hex << input->fbpMasks.ltcPerFbpMasks[i] << std::endl;
    }
    for (unsigned int i = 0; i < sizeof(input->fbpMasks.l2SlicePerFbpMasks)/sizeof(input->fbpMasks.l2SlicePerFbpMasks[0]); i++){
        std::cout << "l2SlicePerFBP[" << std::dec << i << "]: " << std::hex << input->fbpMasks.l2SlicePerFbpMasks[i] << std::endl;
    }

    std::cout << "gpcMask: " << std::hex << input->gpcMasks.gpcMask << std::endl;
    for (unsigned int i = 0; i < sizeof(input->gpcMasks.pesPerGpcMasks)/sizeof(input->gpcMasks.pesPerGpcMasks[0]); i++){
        std::cout << "pesPerGpc[" << std::dec << i << "]: " << std::hex << input->gpcMasks.pesPerGpcMasks[i] << std::endl;
    }
    for (unsigned int i = 0; i < sizeof(input->gpcMasks.tpcPerGpcMasks)/sizeof(input->gpcMasks.tpcPerGpcMasks[0]); i++){
        std::cout << "tpcPerGpc[" << std::dec << i << "]: " << std::hex << input->gpcMasks.tpcPerGpcMasks[i] << std::endl;
    }


}

extern "C" void setChip(IsFSValidCall* input, const char* chipNameCharPtr){
    input->chip = getChipEnum(chipNameCharPtr);
}

extern "C" void setModeProduct(IsFSValidCall* input){
    input->mode = fslib::FSmode::PRODUCT;
}

extern "C" void setModeFuncValid(IsFSValidCall* input){
    input->mode = fslib::FSmode::FUNC_VALID;
}

extern "C" void setFBPMask(IsFSValidCall* input, unsigned int mask){
    input->fbpMasks.fbpMask = mask;
}

extern "C" unsigned int getFBPMask(IsFSValidCall* input){
    return input->fbpMasks.fbpMask;
}

extern "C" void setFBIOPerFBPMask(IsFSValidCall* input, unsigned int idx, unsigned int mask){
    input->fbpMasks.fbioPerFbpMasks[idx] = mask;
}

extern "C" unsigned int getFBIOPerFBPMask(IsFSValidCall* input, unsigned int idx){
    return input->fbpMasks.fbioPerFbpMasks[idx];
}

extern "C" void setFBPAPerFBPMask(IsFSValidCall* input, unsigned int idx, unsigned int mask){
    input->fbpMasks.fbpaPerFbpMasks[idx] = mask;
}

extern "C" unsigned int getFBPAPerFBPMask(IsFSValidCall* input, unsigned int idx){
    return input->fbpMasks.fbpaPerFbpMasks[idx];
}

extern "C" void sethalfFBPAPerFBPMask(IsFSValidCall* input, unsigned int idx, unsigned int mask){
    input->fbpMasks.halfFbpaPerFbpMasks[idx] = mask;
}

extern "C" unsigned int gethalfFBPAPerFBPMask(IsFSValidCall* input, unsigned int idx){
    return input->fbpMasks.halfFbpaPerFbpMasks[idx];
}

extern "C" void setLTCPerFBPMask(IsFSValidCall* input, unsigned int idx, unsigned int mask){
    input->fbpMasks.ltcPerFbpMasks[idx] = mask;
}

extern "C" unsigned int getLTCPerFBPMask(IsFSValidCall* input, unsigned int idx){
    return input->fbpMasks.ltcPerFbpMasks[idx];
}

extern "C" void setL2SlicePerFBPMask(IsFSValidCall* input, unsigned int idx, unsigned int mask){
    input->fbpMasks.l2SlicePerFbpMasks[idx] = mask;
}

extern "C" unsigned int getL2SlicePerFBPMask(IsFSValidCall* input, unsigned int idx){
    return input->fbpMasks.l2SlicePerFbpMasks[idx];
}

extern "C" void setGPCMask(IsFSValidCall* input, unsigned int mask){
    input->gpcMasks.gpcMask = mask;
}

extern "C" unsigned int getGPCMask(IsFSValidCall* input){
    return input->gpcMasks.gpcMask;
}

extern "C" void setPesPerGpcMask(IsFSValidCall* input, unsigned int idx, unsigned int mask){
    input->gpcMasks.pesPerGpcMasks[idx] = mask;
}

extern "C" unsigned int getPesPerGpcMask(IsFSValidCall* input, unsigned int idx){
    return input->gpcMasks.pesPerGpcMasks[idx];
}

extern "C" void setCpcPerGpcMask(IsFSValidCall* input, unsigned int idx, unsigned int mask){
    input->gpcMasks.cpcPerGpcMasks[idx] = mask;
}

extern "C" unsigned int getCpcPerGpcMask(IsFSValidCall* input, unsigned int idx){
    return input->gpcMasks.cpcPerGpcMasks[idx];
}

extern "C" void setTpcPerGpcMask(IsFSValidCall* input, unsigned int idx, unsigned int mask){
    input->gpcMasks.tpcPerGpcMasks[idx] = mask;
}

extern "C" unsigned int getTpcPerGpcMask(IsFSValidCall* input, unsigned int idx){
    return input->gpcMasks.tpcPerGpcMasks[idx];
}

extern "C" void setRopPerGpcMask(IsFSValidCall* input, unsigned int idx, unsigned int mask){
    input->gpcMasks.ropPerGpcMasks[idx] = mask;
}

extern "C" unsigned int getRopPerGpcMask(IsFSValidCall* input, unsigned int idx){
    return input->gpcMasks.ropPerGpcMasks[idx];
}

extern "C" bool isFSValid(IsFSValidCall* input){
    return IsFloorsweepingValid(input->chip, input->gpcMasks, input->fbpMasks, input->errMsg, input->mode);
}

extern "C" bool funcDownBin(IsFSValidCall* input, IsFSValidCall* output){
    bool result = FuncDownBin(input->chip, input->gpcMasks, input->fbpMasks, output->gpcMasks, output->fbpMasks, output->errMsg);
    return result;
}

extern "C" const char * getErrMsg(IsFSValidCall* input){
    return input->errMsg.c_str();
}


// allocate and return a pointer to a SkuConfig obj
extern "C" fslib::FUSESKUConfig::SkuConfig* allocSKUStruct(const char* skuName, unsigned int skuID) {
    fslib::FUSESKUConfig::SkuConfig* sku = new fslib::FUSESKUConfig::SkuConfig();
    sku->SkuName = skuName;
    sku->skuId = skuID;
    return sku;
}

// Free the sku struct memory
extern "C" void freeSKUConfigStruct(fslib::FUSESKUConfig::SkuConfig* input) {
    delete input;
}

// allocate and return a pointer to a SkuConfig obj
extern "C" fslib::FUSESKUConfig::FsConfig* allocFSConfigStruct(uint32_t minEnableCount, uint32_t maxEnableCount, uint32_t minEnablePerGroup, uint32_t enableCountPerGroup) {
    fslib::FUSESKUConfig::FsConfig* config = new fslib::FUSESKUConfig::FsConfig();
    config->minEnableCount = minEnableCount;
    config->maxEnableCount = maxEnableCount;
    config->minEnablePerGroup = minEnablePerGroup;
    config->enableCountPerGroup = enableCountPerGroup;
    return config;
}

extern "C" void setUGPUImbalance(fslib::FUSESKUConfig::FsConfig* config, uint32_t maxUGPUImbalance) {
    config->maxUGPUImbalance = maxUGPUImbalance;
}

extern "C" void addToSkyline(fslib::FUSESKUConfig::FsConfig* config, uint32_t tpc_count) {
    config->skyline.push_back(tpc_count);
}

// Free the config struct memory
extern "C" void freeFSConfigStruct(fslib::FUSESKUConfig::FsConfig* input) {
    delete input;
}

extern "C" void addmustEnable(fslib::FUSESKUConfig::FsConfig* config, uint32_t mustEnable){
    config->mustEnableList.push_back(mustEnable);
}

extern "C" void addmustDisable(fslib::FUSESKUConfig::FsConfig* config, uint32_t mustDisable){
    config->mustDisableList.push_back(mustDisable);
}

extern "C" void addMinMaxRepair(fslib::FUSESKUConfig::FsConfig * config, uint32_t minRepair, uint32_t maxRepair) {
    config->repairMinCount = minRepair;
    config->repairMaxCount = maxRepair;
}

extern "C" void addConfigToSKU(fslib::FUSESKUConfig::SkuConfig* sku, fslib::FUSESKUConfig::FsConfig* config, const char* name){
    sku->fsSettings[std::string(name)] = *config;
}

extern "C" bool isInSKU(IsFSValidCall* fsconfig, fslib::FUSESKUConfig::SkuConfig* potentialSKU){
    return IsInSKU(fsconfig->chip, fsconfig->gpcMasks, fsconfig->fbpMasks, *potentialSKU, fsconfig->errMsg);
}

extern "C" bool canFitSKU(IsFSValidCall* fsconfig, fslib::FUSESKUConfig::SkuConfig* potentialSKU){
    return CanFitSKU(fsconfig->chip, fsconfig->gpcMasks, fsconfig->fbpMasks, *potentialSKU, fsconfig->errMsg);
}

extern "C" bool downBin(IsFSValidCall * input, fslib::FUSESKUConfig::SkuConfig * sku, IsFSValidCall * outputFS, IsFSValidCall * outputSpares) {
    bool result = DownBin(input->chip, input->gpcMasks, input->fbpMasks, *sku, outputFS->gpcMasks, outputFS->fbpMasks, outputFS->errMsg);
    return result;
}

extern "C" bool downBinSpares(IsFSValidCall* input, fslib::FUSESKUConfig::SkuConfig* sku, IsFSValidCall* outputFS, IsFSValidCall* outputSpares){
    bool result = DownBin(
        input->chip, input->gpcMasks, input->fbpMasks, *sku, outputFS->gpcMasks, outputFS->fbpMasks, 
        outputSpares->gpcMasks, outputSpares->fbpMasks, outputFS->errMsg
    );
    return result;
}
