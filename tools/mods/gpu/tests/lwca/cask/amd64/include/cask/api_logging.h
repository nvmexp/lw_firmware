#ifndef API_LOGGING_H
#define API_LOGGING_H
#pragma once
#include <string.h>
#include <iostream>

#define STR(x) #x
#define STRINGIFY(x) STR(x)

#if ! defined(CASK_LOGGING_DESTINATION)
  #define CASK_LOGGING_DESTINATION "cask_kernel.log"
#endif

namespace cask {

enum MethodType {
    HOST = 0,
    DEVICE,
    RUN
};

enum OperationType {
    COLW_OPERATION = 0,
    GEMM_OPERATION,
    POOL_OPERATION,
    SOFTMAX_OPERATION
}; 

struct logParams {
    std::string loggingDestination = "";
    bool enabled = false;
    bool host = false;
    bool device = false;
    bool run = false;
};

static logParams params;

//////////////////////////////////////////////////////////////////////////////
// Macros used for logging of kernel parameters
//////////////////////////////////////////////////////////////////////////////
static bool checkIsLoggingEnabledFromElwironment() {
    #ifndef CASK_LOGGING_ENABLED
        return false;
    #else
        return getelw("CASK_LOG_INFO") != NULL 
            && strncmp(getelw("CASK_LOG_INFO"),"0", 1) != 0
            && strncmp(getelw("CASK_LOG_INFO"),"OFF", 3) != 0
            && strncmp(getelw("CASK_LOG_INFO"),"off", 3) != 0;
    #endif
}

static bool isLoggingEnabled() {
    static bool loggingEnabled = params.enabled || checkIsLoggingEnabledFromElwironment();
    return loggingEnabled;
}

static std::string getApiLogFileFromElwironment() {
    std::string defaultFilename(STRINGIFY(CASK_LOGGING_DESTINATION));
    if (params.enabled && params.loggingDestination != "") {
        return params.loggingDestination;
    }
    else if(getelw("CASK_LOGGING_DESTINATION") != NULL) {
        defaultFilename.assign(getelw("CASK_LOGGING_DESTINATION")); 
    }
    return defaultFilename;
}

static void enableLogging(std::string logDest, bool logHost, bool logDevice, bool logRun){
    params.enabled = true;
    params.loggingDestination = logDest;
    params.host = logHost;
    params.device = logDevice;
    params.run = logRun;
}

static bool checkIfMethodEnabled(MethodType method) {
    if (method == HOST && params.host) {
        return true;
    }
    if (method == DEVICE && params.device) {
        return true;
    }
    if (method == RUN && params.run) {
        return true;
    }
    return false;
}

static inline void osprint_param(std::string name, const size_t &val) {
    std::clog << name << " = " << val << std::endl;
}

static inline void osprint_param(std::string name, const int64_t &val) {
    std::clog << name << " = " << val << std::endl;
}

static inline void osprint_param(std::string name, const int32_t &val) {
    std::clog << name << " = " << val << std::endl;
}

static inline void osprint_param(std::string name, const uint32_t &val) {
    std::clog << name << " = " << val << std::endl;
}

static inline void osprint_param(std::string name, const float &val) {
    std::clog << name << " = " << val << std::endl;
}

static inline void osprint_param(std::string name, const double &val) {
    std::clog << name << " = " << val << std::endl;
}

static inline void osprint_param(std::string name, const bool val) {
    std::clog << name << " = " << std::boolalpha << val << std::noboolalpha << std::endl;
}

#define CASK_OPEN_FILE()                                                                        \
    do {                                                                                        \
	    if (cask::isLoggingEnabled()) {                                                         \
            std::string filename = cask::getApiLogFileFromElwironment();                        \
            FILE *fp = freopen(filename.c_str(), "a+", stderr);                                 \
            if(fp == NULL) continue;                                                            \
        }                                                                                       \
    } while (false)

#define CASK_BEGIN_API_LOG() CASK_OPEN_FILE()

#define CASK_END_API_LOG()                                                                      \
    do {                                                                                        \
        if (cask::isLoggingEnabled()) {                                                         \
            std::clog << "########################################################\n";          \
            fclose(stderr);                                                                     \
        }                                                                                       \
    } while (false)

#define CASK_LOG_SHADER_NAME(NAME)                                                              \
    do {                                                                                        \
        if (cask::isLoggingEnabled()) {                                                         \
            std::clog << "shader_name = " << NAME             << "\n";                          \
        }                                                                                       \
    } while (false)

#define CASK_LOG_TENSOR_DIM(TENSOR)                                                             \
    do {                                                                                        \
        if (cask::isLoggingEnabled()) {                                                         \
            for(int i = 1; i <= TENSOR.dimensions; ++i) {                                       \
                int dimI = TENSOR.dimensions - i;                                               \
                std::clog << "[" << (int)TENSOR.dim[dimI];                                      \
                if ((TENSOR.vectorizedDim == dimI) && (TENSOR.scalarsPerElement > 1)) {         \
                    std::clog << "x" << (int)TENSOR.scalarsPerElement;                          \
                }                                                                               \
                std::clog << "]";                                                               \
            }                                                                                   \
        }                                                                                       \
    } while (false)

#define CASK_LOG_DIMENSIONS_GEMM(DESC)                                                          \
    do {                                                                                        \
        if (cask::isLoggingEnabled()) {                                                         \
            std::clog << "operation = gemm\n"                                                   \
                      << "inputA tensor (m,k) = ";                                              \
            CASK_LOG_TENSOR_DIM(DESC.inputADesc);                                               \
            std::clog << "\ninputB tensor (k,n) = ";                                            \
            CASK_LOG_TENSOR_DIM(DESC.inputBDesc);                                               \
            std::clog << "\n";                                                                  \
        }                                                                                       \
    } while (false)

#define CASK_LOG_AB(DESC)                                                                       \
    do {                                                                                        \
        if (cask::isLoggingEnabled()) {                                                         \
            std::clog << "alpha = "           << DESC.alpha                         << "\n"     \
                      << "beta = "            << DESC.beta                          << "\n";    \
        }                                                                                       \
    } while (false) 


#define CASK_LOG_FLAGS(DESC)                                                                    \
    do {                                                                                        \
        if (cask::isLoggingEnabled()) {                                                         \
            std::clog << "relu = "  << DESC.applyReLu   << "\n"                                 \
                      << "bias = "  << DESC.addBias     << "\n";                                \
        }                                                                                       \
    } while (false)

#define CASK_LOG_ARRAY(ARRAY)                                                                   \
    do {                                                                                        \
        if (cask::isLoggingEnabled()) {                                                         \
            for (unsigned int i = 0; i < 2; i++) {                                              \
                std::clog << "[" << ARRAY[i] << "]";                                            \
            }                                                                                   \
        }                                                                                       \
    } while (false)


static inline void log_splitK(const Operation::Description& DESC) {
    osprint_param("splitK", DESC.splitK);
    osprint_param("splitKMode", DESC.splitKMode);
    osprint_param("splitKBuffers", DESC.splitKBuffers);
    osprint_param("splitKT", DESC.splitKT);
    osprint_param("splitKR", DESC.splitKR);
    osprint_param("splitH", DESC.splitH);
}


static inline void log_desc_flags(const Operation::Description& DESC) {                                                               
    osprint_param("relu", DESC.applyReLu != 0);
    osprint_param("reLuUpperBound", DESC.reLuUpperBound);
    osprint_param("reLuThreshold", DESC.reLuThreshold);
    osprint_param("addBias", DESC.addBias);
    osprint_param("mode", DESC.mode);
    osprint_param("withoutReduction", DESC.withoutReduction);
    osprint_param("batchScalingStride", DESC.batchScalingStride);
    osprint_param("dynamicProhibited", DESC.dynamicProhibited);
    osprint_param("dynamicResize", DESC.dynamicResize);
    osprint_param("perChannelScaling", DESC.perChannelScaling);
    osprint_param("ctasPerWave", DESC.ctasPerWave);
    osprint_param("isBlasl3InputTriangularRight", DESC.isBlasl3InputTriangularRight);
}

static inline void log_colw_dimensions(const std::string shaderName, const Operation::Description& DESC) {                                                  	         
    if(shaderName.find("dgrad") != std::string::npos) {
        std::clog << "operation = dgrad";
        std::clog << "\nInput tensor (n,c,d,h,w) = ";                                                                                        
        CASK_LOG_TENSOR_DIM(DESC.outputDesc);
        std::clog << "\nFilter tensor (k,c,t,r,s) = ";                                      
        CASK_LOG_TENSOR_DIM(DESC.inputBDesc);                                               
        std::clog << "\nOutput tensor (n,k,o,p,q) = ";                                        
        CASK_LOG_TENSOR_DIM(DESC.inputADesc);                                               
    } else if(shaderName.find("wgrad") != std::string::npos) {
        std::clog << "operation = wgrad";
        std::clog << "\nInput tensor (n,c,d,h,w) = ";                                         
        CASK_LOG_TENSOR_DIM(DESC.inputBDesc);
        std::clog << "\nFilter tensor (k,c,t,r,s) = ";
        CASK_LOG_TENSOR_DIM(DESC.outputDesc); 
        std::clog << "\nOutput tensor (n,k,o,p,q) = ";  
        CASK_LOG_TENSOR_DIM(DESC.inputADesc);                                               
    } else {
        std::clog << "operation = colw";
        std::clog << "\nInput tensor (n,c,d,h,w) = ";                                         
        CASK_LOG_TENSOR_DIM(DESC.inputADesc);
        std::clog << "\nFilter tensor (k,c,t,r,s) = ";                                      
        CASK_LOG_TENSOR_DIM(DESC.inputBDesc);                                               
        std::clog << "\nOutput tensor (n,k,o,p,q) = ";                                        
        CASK_LOG_TENSOR_DIM(DESC.outputDesc);
    }
    std::clog << std::endl;                                                                                                                                                      
}
    
static inline void log_padding(const Operation::Description& DESC) {
    osprint_param("padTop", DESC.colwolutionPadTop);
    osprint_param("padBottom", DESC.colwolutionPadBottom);
    osprint_param("padRight", DESC.colwolutionPadRight);
    osprint_param("padLeft", DESC.colwolutionPadLeft);
    osprint_param("padFront", DESC.colwolutionPadFront);
    osprint_param("padBack", DESC.colwolutionPadBack);
    osprint_param("padValue", DESC.colwolutionPadValue);
}

static inline void log_dilation(const Operation::Description& DESC) {
    osprint_param("dilH", DESC.colwolutionDilationH);
    osprint_param("dilW", DESC.colwolutionDilationW);
    osprint_param("dilD", DESC.colwolutionDilationD);
}

static inline void log_stride(const Operation::Description& DESC) {
    osprint_param("strideH", DESC.colwolutionStrideH);
    osprint_param("strideW", DESC.colwolutionStrideW);
    osprint_param("strideD", DESC.colwolutionStrideD);                                                                                      
}

static inline void log_colw_flags(const Operation::Description& DESC) {
    osprint_param("alpha", DESC.alpha);
    osprint_param("beta", DESC.beta);
    osprint_param("colwGroups", DESC.colwGroups);
    osprint_param("isCrossCorrelation", DESC.isCrossCorrelation);
    osprint_param("colwGroupsPerPacket", DESC.colwGroupsPerPacket);
    osprint_param("isDgrad", DESC.isDgrad);
    osprint_param("isWgrad1x1BatchedGEMM", DESC.isWgrad1x1BatchedGEMM);	                                                                                                     
}

static inline void log_colw_desc(const std::string shaderName, const Operation::Description& colwDesc) {
    if (!isLoggingEnabled()) return;
    std::clog << "shader_name = " << shaderName << std::endl;
    log_colw_dimensions(shaderName, colwDesc);    
    log_padding(colwDesc);
    log_dilation(colwDesc);
    log_stride(colwDesc);
    log_desc_flags(colwDesc);
    log_colw_flags(colwDesc);
    log_splitK(colwDesc);
}


static inline void log_gemm_dimensions(const Operation::Description& DESC) {                                                  
    std::clog << "operation = gemm"
              << "\nInputA tensor(batch, m, k) = ";
    if(DESC.inputADesc.dimensions == 2) {
        std::clog << "[1]";
    }
    CASK_LOG_TENSOR_DIM(DESC.inputADesc);         
    std::clog << "\nInputB tensor(batch, k, n) = ";    
    if(DESC.inputBDesc.dimensions == 2) {
        std::clog << "[1]";
    }                                  
    CASK_LOG_TENSOR_DIM(DESC.inputBDesc);                                               
    std::clog << "\nOutput tensor(batch, m, n) = ";
    if(DESC.outputDesc.dimensions == 2) {
        std::clog << "[1]";
    }
    CASK_LOG_TENSOR_DIM(DESC.outputDesc);                                               
    std::clog << std::endl;                                                                                                                                                      
}

static inline void log_batch_flags(const Operation::Description& DESC) {
    osprint_param("batchBiasStride", DESC.batchBiasStride);
    osprint_param("batchScalingStride", DESC.batchScalingStride);
}

static inline void log_alpha_beta_flags(const Operation::Description& DESC) {                                                               
    osprint_param("alpha", DESC.alphaCp.x);
    osprint_param("beta", DESC.betaCp.x);
    osprint_param("alpha_imag", DESC.alphaCp.y);
    osprint_param("beta_imag", DESC.betaCp.y);
}

static inline void log_gemm_desc(const std::string shaderName, const Operation::Description& desc) {
    if (!isLoggingEnabled()) return; 
    std::clog << "shader_name = " << shaderName << std::endl;
    log_gemm_dimensions(desc);    
    log_desc_flags(desc);
    log_splitK(desc);
    log_batch_flags(desc);
    log_alpha_beta_flags(desc);
}


static void performLogging(int32_t methodEnum, int32_t operationEnum, const std::string name, const Operation::Description& desc, cask::RunInfo &ri) {
    MethodType method = static_cast<MethodType>(methodEnum);
    OperationType op = static_cast<OperationType>(operationEnum);
    
    if (checkIfMethodEnabled(method)) {
        switch(op) {
            case COLW_OPERATION:
                CASK_BEGIN_API_LOG();
                log_colw_desc(name, desc);
                CASK_END_API_LOG();
                break;

            case GEMM_OPERATION:
                log_gemm_desc(name, desc);
                CASK_END_API_LOG();
                break;

            default:
                break;
        }
    }
    
}
}

#endif // INCLUDE_GUARD_API_LOGGING_H
