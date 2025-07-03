/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DPU_GPUARCH_H
#define DPU_GPUARCH_H

/*!
 * @file dpu_gpuarch.h
 */

#include "dpu_task.h"
#include "gpuarch.h"

//
// RM shifts a nibble to the left to define GPU ARCHITECTURE.
// DPU only takes architecture number from RM's GPU_ARCHITECTURE.
//
#define DPU_XLATE_RM_GPU_ARCH(arch)  \
                                 (DRF_VAL(GPU, _ARCHITECTURE, _ARCH, arch) >> 4)

// Wrapper macro to define DPU ARCHITECTURE
#define DPU_ARCHITECTURE(arch)       \
                                 DPU_XLATE_RM_GPU_ARCH(GPU_ARCHITECTURE##arch)

#define DPU_CHIP_ID_DEFINE(arch, impl)  (GPU_IMPLEMENTATION##impl << 8 | \
                                         DPU_ARCHITECTURE(arch))

#define DPU_CHIP_ID_GF119                DPU_CHIP_ID_DEFINE(_FERMI2,   _GF119)

#define DPU_CHIP_ID_GK104                DPU_CHIP_ID_DEFINE(_KEPLER,   _GK104)
#define DPU_CHIP_ID_GK106                DPU_CHIP_ID_DEFINE(_KEPLER,   _GK106)
#define DPU_CHIP_ID_GK107                DPU_CHIP_ID_DEFINE(_KEPLER,   _GK107)

#define DPU_CHIP_ID_GK110                DPU_CHIP_ID_DEFINE(_KEPLER2,  _GK110)

#define DPU_CHIP_ID_GK207                DPU_CHIP_ID_DEFINE(_KEPLER20, _GK207)
#define DPU_CHIP_ID_GK208                DPU_CHIP_ID_DEFINE(_KEPLER20, _GK208)

#define DPU_CHIP_ID_GM107                DPU_CHIP_ID_DEFINE(_MAXWELL,  _GM107)
#define DPU_CHIP_ID_GM108                DPU_CHIP_ID_DEFINE(_MAXWELL,  _GM108)
#define DPU_CHIP_ID_GM200                DPU_CHIP_ID_DEFINE(_MAXWELL2, _GM200)
#define DPU_CHIP_ID_GM204                DPU_CHIP_ID_DEFINE(_MAXWELL2, _GM204)
#define DPU_CHIP_ID_GM206                DPU_CHIP_ID_DEFINE(_MAXWELL2, _GM206)

#define DPU_CHIP_ID_GP100                DPU_CHIP_ID_DEFINE(_PASCAL,   _GP100)
#define DPU_CHIP_ID_GP107                DPU_CHIP_ID_DEFINE(_PASCAL,   _GP107)
#define DPU_CHIP_ID_GP104                DPU_CHIP_ID_DEFINE(_PASCAL,   _GP104)

#define DPU_CHIP_ID_GV100                DPU_CHIP_ID_DEFINE(_VOLTA,   _GV100)

/*!
 * Colwenient GPU macros.
 */

#define IsARCH(a)               (DpuInitArgs.chipInfo.id.sId.arch == DPU_ARCHITECTURE(a))

#define IsGPU(g)                (DpuInitArgs.chipInfo.id.id == DPU_CHIP_ID##g)

#define IsNetList(n)            (DpuInitArgs.chipInfo.netList == (n))

#define IsGpuNetList(g,n)       ((IsGPU(g)) && IsNetList(n))

#define IsEmulation()           (!IsNetList(0))


/*!
 * Definitions of DISP IP versions
 */
#define DISP_VERSION_V02_00 (0x0200) 
#define DISP_VERSION_V02_01 (0x0201) 
#define DISP_VERSION_V02_02 (0x0202) 
#define DISP_VERSION_V02_03 (0x0203) 
#define DISP_VERSION_V02_04 (0x0204) 
#define DISP_VERSION_V02_05 (0x0205) 
#define DISP_VERSION_V02_06 (0x0206) 
#define DISP_VERSION_V02_07 (0x0207) 
#define DISP_VERSION_V02_08 (0x0208) 

/*!
 * Check if the DISP IP version of the chip we are running >= our expectation
 */
#define DISP_IP_VER_GREATER_OR_EQUAL(ipVersion) (DpuInitArgs.chipInfo.dispIpVer.dispIpVer >= (ipVersion))

#endif //DPU_GPUARCH_H
