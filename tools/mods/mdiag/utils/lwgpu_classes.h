/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2014,2018-2021 by LWPU Corporation.  All rights reserved.  
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _LWGPU_CLASSES_H_
#define _LWGPU_CLASSES_H_

#include "core/include/rc.h"
#include <memory>
#include <set>

// this file contains ONLY:
//   chip-independent class names and numbers

typedef set<UINT32> SemaphoreSet;
typedef map<UINT32, string, greater<UINT32> > ClassNameMap;
typedef map<UINT32, SemaphoreSet*> ClassSemaphoreMap;
typedef map<UINT32, UINT32> ClassFamilyMap;
typedef vector<tuple<UINT32, string, SemaphoreSet*, UINT32>> EngineData;
class LwRm;
class GpuDevice;

namespace LWGpuClasses
{
    enum {
        GPU_LW_NULL_CLASS                   = 0x0030,
    };

    enum {
        GPU_CLASS_UNDEFINED                 = 0x0,
        GPU_CLASS_FERMI                     = 0x9000,
        GPU_CLASS_KEPLER                    = 0xa000,
        GPU_CLASS_MAXWELL                   = 0xb000,
        GPU_CLASS_PASCAL_A                  = 0xc000,
        GPU_CLASS_PASCAL_B                  = 0xc100,
        GPU_CLASS_VOLTA                     = 0xc300,
        GPU_CLASS_TURING                    = 0xc500,
        GPU_CLASS_AMPERE                    = 0xc600,
        GPU_CLASS_ADA                       = 0xc900,
        GPU_CLASS_HOPPER                    = 0xcb00,
        GPU_CLASS_BLACKWELL                 = 0xcd00,
    };

    enum class Engine
    {
        GRAPHICS,
        LWDEC,
        LWENC,
        COPY,
        LWJPG,
        OFA,
        SEC,
        SW,
        ENGINE_NUM
    };

    enum class ClassType
    {
        GRAPHICS,
        COMPUTE,
        LWDEC,
        LWENC,
        COPY,
        LWJPG,
        OFA,
        SEC,
        SW,
        CLASS_TYPE_END
    };

    struct ClassTypeData 
    {
        string m_ClassTypeStr;
        ClassType m_ClassTypeEnum;
        EngineData* m_pEngineData;
    };
}

class EngineClasses
{
public:
    static RC GetFirstSupportedClass
    (
        LwRm* pLwRm,
        GpuDevice* pGpuDevice,
        string classType,
        UINT32* supportedClass
    );
    static RC GetFirstSupportedClass
    (
        LwRm* pLwRm,
        GpuDevice* pGpuDevice,
        LWGpuClasses::Engine engineType,
        UINT32* supportedClass,
        LWGpuClasses::ClassType classType = LWGpuClasses::ClassType::CLASS_TYPE_END
    );
    static vector<UINT32>& GetClassVec(string classType);
    static void CreateAllEngineClasses();
    static bool IsClassType(string classType, UINT32 classNum);
    static string GetClassName(UINT32 classNum);
    static bool GetClassNum(string className, UINT32* classNum);
    static void FreeEngineClassesObjs();
    static bool IsSemaphoreMethod(UINT32 method, UINT32 subChannelClass, UINT32 channelClass);
    static bool IsGpuFamilyClassOrLater(UINT32 classNum, UINT32 gpuFamily);

private:
    static EngineClasses* GetEngineClassesObj(string classType);
    static EngineClasses* GetEngineClassesObj(LWGpuClasses::ClassType classType);
    EngineClasses
    (
        string classTypeStr, 
        LWGpuClasses::ClassType classTypeEnum,
        ClassNameMap&& engineClasses,
        ClassSemaphoreMap&& engineSemaphores
    );
    vector<UINT32>& GetClassVec() { return m_ClassVec; }

    string m_ClassTypeStr;
    LWGpuClasses::ClassType m_ClassTypeEnum;
    vector<UINT32> m_ClassVec;
    ClassNameMap m_EngineClassNameMap;
    ClassSemaphoreMap m_EngineClassSemaphoreMap;
    ClassFamilyMap m_EngineClassFamilyMap;
    static vector<unique_ptr<EngineClasses>> s_EngineClassObjs;
};
#endif
