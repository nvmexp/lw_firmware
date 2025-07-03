/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <map>
#include <vector>
#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>
#include "mdiag/utils/types.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "core/include/lwrm.h"
#include "mdiag/sysspec.h"

#include "class/cla0b0.h" // LWA0B0_VIDEO_DECODER
#include "class/clb0b0.h" // LWB0B0_VIDEO_DECODER
#include "class/clb6b0.h" // LWB6B0_VIDEO_DECODER
#include "class/clc1b0.h" // LWC1B0_VIDEO_DECODER
#include "class/clc2b0.h" // LWC2B0_VIDEO_DECODER
#include "class/clc3b0.h" // LWC3B0_VIDEO_DECODER
#include "class/clc4b0.h" // LWC4B0_VIDEO_DECODER
#include "class/clc6b0.h" // LWC6B0_VIDEO_DECODER
#include "class/clc7b0.h" // LWC7B0_VIDEO_DECODER
#include "class/clb8b0.h" // LWB8B0_VIDEO_DECODER
#include "class/clc9b0.h" // LWC9B0_VIDEO_DECODER

#include "class/cl90b7.h" // LW90B7_VIDEO_ENCODER
#include "class/clc0b7.h" // LWC0B7_VIDEO_ENCODER
#include "class/cld0b7.h" // LWD0B7_VIDEO_ENCODER
#include "class/clc1b7.h" // LWC1B7_VIDEO_ENCODER
#include "class/clc2b7.h" // LWC2B7_VIDEO_ENCODER
#include "class/clc3b7.h" // LWC3B7_VIDEO_ENCODER
#include "class/clc4b7.h" // LWC4B7_VIDEO_ENCODER
#include "class/clb4b7.h" // LWB4B7_VIDEO_ENCODER
#include "class/clb6b7.h" // LWB6B7_VIDEO_ENCODER
#include "class/clc7b7.h" // LWC7B7_VIDEO_ENCODER
#include "class/clc9b7.h" // LWC9B7_VIDEO_ENCODER

#include "class/cl90b5.h" // GF100_DMA_COPY
#include "class/cla0b5.h" // KEPLER_DMA_COPY_A
#include "class/clb0b5.h" // MAXWELL_DMA_COPY_A
#include "class/clc0b5.h" // PASCAL_DMA_COPY_A
#include "class/clc1b5.h" // PASCAL_DMA_COPY_B
#include "class/clc3b5.h" // VOLTA_DMA_COPY_A
#include "class/clc5b5.h" // TURING_DMA_COPY_A
#include "class/clc6b5.h" // AMPERE_DMA_COPY_A
#include "class/clc7b5.h" // AMPERE_DMA_COPY_B
#include "class/clc8b5.h" // HOPPER_DMA_COPY_A
#include "class/clc9b5.h" // BLACKWELL_DMA_COPY_A

#include "class/cl90c0.h" // FERMI_COMPUTE_A
#include "class/cl91c0.h" // FERMI_COMPUTE_B
#include "class/cla0c0.h" // KEPLER_COMPUTE_A
#include "class/cla1c0.h" // KEPLER_COMPUTE_B
#include "class/clb0c0.h" // MAXWELL_COMPUTE_A
#include "class/clb1c0.h" // MAXWELL_COMPUTE_B
#include "class/clc0c0.h" // PASCAL_COMPUTE_A
#include "class/clc1c0.h" // PASCAL_COMPUTE_B
#include "class/clc3c0.h" // VOLTA_COMPUTE_A
#include "class/clc4c0.h" // VOLTA_COMPUTE_B
#include "class/clc5c0.h" // TURING_COMPUTE_A
#include "class/clc6c0.h" // AMPERE_COMPUTE_A
#include "class/clc7c0.h" // AMPERE_COMPUTE_B
#include "class/clc9c0.h" // ADA_COMPUTE_A
#include "class/clcbc0.h" // HOPPER_COMPUTE_A
#include "class/clccc0.h" // HOPPER_COMPUTE_B
#include "class/clcdc0.h" // BLACKWELL_COMPUTE_A

#include "class/clb8d1.h" // LWB8D1_VIDEO_LWJPG
#include "class/clc4d1.h" // LWC4D1_VIDEO_LWJPG
#include "class/clc9d1.h" // LWC9D1_VIDEO_LWJPG

#include "class/clb8fa.h" // LWB8FA_VIDEO_OFA
#include "class/clc6fa.h" // LWC6FA_VIDEO_OFA
#include "class/clc7fa.h" // LWC7FA_VIDEO_OFA
#include "class/clc9fa.h" // LWC9FA_VIDEO_OFA

#include "class/cl9097.h" // FERMI_A
#include "class/cl9197.h" // FERMI_B
#include "class/cl9297.h" // FERMI_C
#include "class/cla097.h" // KEPLER_A
#include "class/cla197.h" // KEPLER_B
#include "class/cla297.h" // KEPLER_C
#include "class/clb097.h" // MAXWELL_A
#include "class/clb197.h" // MAXWELL_B
#include "class/clc097.h" // PASCAL_A
#include "class/clc197.h" // PASCAL_B
#include "class/clc397.h" // VOLTA_A
#include "class/clc497.h" // VOLTA_B
#include "class/clc597.h" // TURING_A
#include "class/clc697.h" // AMPERE_A
#include "class/clc797.h" // AMPERE_B
#include "class/clc997.h" // ADA_A
#include "class/clcb97.h" // HOPPER_A
#include "class/clcc97.h" // HOPPER_B
#include "class/clcd97.h" // BLACKWELL_A

#include "class/cl906f.h" // GF100_CHANNEL_GPFIFO
#include "class/cla06f.h" // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla16f.h" // KEPLER_CHANNEL_GPFIFO_B
#include "class/cla26f.h" // KEPLER_CHANNEL_GPFIFO_C
#include "class/clb06f.h" // MAXWELL_CHANNEL_GPFIFO_A
#include "class/clc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "class/clc36f.h" // VOLTA_CHANNEL_GPFIFO_A
#include "class/clc46f.h" // TURING_CHANNEL_GPFIFO_A
#include "class/clc56f.h" // AMPERE_CHANNEL_GPFIFO_A
#include "class/clc86f.h" // HOPPER_CHANNEL_GPFIFO_A

#include "class/cl95a1.h" // LW95A1_TSEC

#include "class/cl007d.h" // LW04_SOFTWARE_TEST

namespace LWGpuClasses
{
    //-------------------------------------------------------------------------
    //! Host Semaphore Methods
    //!
    SemaphoreSet g_Host906fSemaphores =
    {
        LW906F_SEMAPHOREA,
        LW906F_SEMAPHOREB,
        LW906F_SEMAPHOREC,
        LW906F_SEMAPHORED
    };

    SemaphoreSet g_Hostc36fSemaphores =
    {
        LWC36F_SEM_ADDR_LO,
        LWC36F_SEM_ADDR_HI,
        LWC36F_SEM_PAYLOAD_LO,
        LWC36F_SEM_PAYLOAD_HI,
        LWC36F_SEM_EXELWTE
    };

    ClassSemaphoreMap g_HostSemaphoreMap =
    {
        { GF100_CHANNEL_GPFIFO,     &g_Host906fSemaphores },
        { KEPLER_CHANNEL_GPFIFO_A,  &g_Host906fSemaphores },
        { KEPLER_CHANNEL_GPFIFO_B,  &g_Host906fSemaphores },
        { KEPLER_CHANNEL_GPFIFO_C,  &g_Host906fSemaphores },
        { PASCAL_CHANNEL_GPFIFO_A,  &g_Host906fSemaphores },
        { MAXWELL_CHANNEL_GPFIFO_A, &g_Host906fSemaphores },
        { VOLTA_CHANNEL_GPFIFO_A,   &g_Hostc36fSemaphores },
        { TURING_CHANNEL_GPFIFO_A,  &g_Hostc36fSemaphores },
        { AMPERE_CHANNEL_GPFIFO_A,  &g_Hostc36fSemaphores },
        { HOPPER_CHANNEL_GPFIFO_A,  &g_Hostc36fSemaphores }
    };

    //-------------------------------------------------------------------------
    //! LWDEC Classes
    //!
    
    SemaphoreSet g_LwDeca0b0Semaphores =
    {
        LWA0B0_SEMAPHORE_A,
        LWA0B0_SEMAPHORE_B,
        LWA0B0_SEMAPHORE_C,
        LWA0B0_SEMAPHORE_D
    };

    SemaphoreSet g_LwDecc7b0Semaphores =
    {
        LWC7B0_SEMAPHORE_A,
        LWC7B0_SEMAPHORE_B,
        LWC7B0_SEMAPHORE_C,
        LWC7B0_SET_SEMAPHORE_PAYLOAD_LOWER,
        LWC7B0_SET_SEMAPHORE_PAYLOAD_UPPER,
        LWC7B0_SEMAPHORE_D
    };

    EngineData g_LwDecClasses = 
    {
        make_tuple( LWC9B0_VIDEO_DECODER,   "lwc9b0_video_decoder",     &g_LwDecc7b0Semaphores,     GPU_CLASS_ADA       ),
        make_tuple( LWB8B0_VIDEO_DECODER,   "lwb8b0_video_decoder",     &g_LwDecc7b0Semaphores,     GPU_CLASS_HOPPER    ),
        make_tuple( LWC7B0_VIDEO_DECODER,   "lwc7b0_video_decoder",     &g_LwDecc7b0Semaphores,     GPU_CLASS_AMPERE    ),
        make_tuple( LWC6B0_VIDEO_DECODER,   "lwc6b0_video_decoder",     &g_LwDeca0b0Semaphores,     GPU_CLASS_AMPERE    ),
        make_tuple( LWC4B0_VIDEO_DECODER,   "lwc4b0_video_decoder",     &g_LwDeca0b0Semaphores,     GPU_CLASS_TURING    ),
        make_tuple( LWC3B0_VIDEO_DECODER,   "lwc3b0_video_decoder",     &g_LwDeca0b0Semaphores,     GPU_CLASS_VOLTA     ),
        make_tuple( LWC2B0_VIDEO_DECODER,   "lwc2b0_video_decoder",     &g_LwDeca0b0Semaphores,     GPU_CLASS_PASCAL_B  ),
        make_tuple( LWC1B0_VIDEO_DECODER,   "lwc1b0_video_decoder",     &g_LwDeca0b0Semaphores,     GPU_CLASS_PASCAL_A  ),
        make_tuple( LWB6B0_VIDEO_DECODER,   "lwb6b0_video_decoder",     &g_LwDeca0b0Semaphores,     GPU_CLASS_MAXWELL   ),
        make_tuple( LWB0B0_VIDEO_DECODER,   "lwb0b0_video_decoder",     &g_LwDeca0b0Semaphores,     GPU_CLASS_MAXWELL   ),
        make_tuple( LWA0B0_VIDEO_DECODER,   "lwa0b0_video_decoder",     &g_LwDeca0b0Semaphores,     GPU_CLASS_KEPLER    )
    };

    //-------------------------------------------------------------------------
    //! LWENC Classes
    //!

    SemaphoreSet g_LwEnc90b7Semaphores =
    {
        LW90B7_SEMAPHORE_A,
        LW90B7_SEMAPHORE_B,
        LW90B7_SEMAPHORE_C,
        LW90B7_SEMAPHORE_D
    };

    SemaphoreSet g_LwEncc7b7Semaphores =
    {
        LWC7B7_SEMAPHORE_A,
        LWC7B7_SEMAPHORE_B,
        LWC7B7_SEMAPHORE_C,
        LWC7B7_SET_SEMAPHORE_PAYLOAD_LOWER,
        LWC7B7_SET_SEMAPHORE_PAYLOAD_UPPER,
        LWC7B7_SEMAPHORE_D
        
    };

    EngineData g_LwEncClasses = 
    {
        make_tuple( LWC9B7_VIDEO_ENCODER,   "lwc9b7_video_encoder",     &g_LwEncc7b7Semaphores,     GPU_CLASS_ADA       ),
        make_tuple( LWC7B7_VIDEO_ENCODER,   "lwc7b7_video_encoder",     &g_LwEncc7b7Semaphores,     GPU_CLASS_AMPERE    ),
        make_tuple( LWB6B7_VIDEO_ENCODER,   "lwb6b7_video_encoder",     &g_LwEnc90b7Semaphores,     GPU_CLASS_AMPERE    ),
        make_tuple( LWB4B7_VIDEO_ENCODER,   "lwb4b7_video_encoder",     &g_LwEnc90b7Semaphores,     GPU_CLASS_TURING    ),
        make_tuple( LWC4B7_VIDEO_ENCODER,   "lwc4b7_video_encoder",     &g_LwEnc90b7Semaphores,     GPU_CLASS_TURING    ),
        make_tuple( LWC3B7_VIDEO_ENCODER,   "lwc3b7_video_encoder",     &g_LwEnc90b7Semaphores,     GPU_CLASS_VOLTA     ),
        make_tuple( LWC2B7_VIDEO_ENCODER,   "lwc2b7_video_encoder",     &g_LwEnc90b7Semaphores,     GPU_CLASS_PASCAL_B  ),
        make_tuple( LWC1B7_VIDEO_ENCODER,   "lwc1b7_video_encoder",     &g_LwEnc90b7Semaphores,     GPU_CLASS_PASCAL_A  ),
        make_tuple( LWD0B7_VIDEO_ENCODER,   "lwd0b7_video_encoder",     &g_LwEnc90b7Semaphores,     GPU_CLASS_MAXWELL   ),
        make_tuple( LWC0B7_VIDEO_ENCODER,   "lwc0b7_video_encoder",     &g_LwEnc90b7Semaphores,     GPU_CLASS_MAXWELL   ),
        make_tuple( LW90B7_VIDEO_ENCODER,   "lw90b7_video_encoder",     &g_LwEnc90b7Semaphores,     GPU_CLASS_FERMI     )
    };

    //-------------------------------------------------------------------------
    //! CE Classes
    //!
    
    SemaphoreSet g_CE90b5Semaphores =
    {
        LW90B5_SET_SEMAPHORE_A,
        LW90B5_SET_SEMAPHORE_B,
        LW90B5_SET_SEMAPHORE_PAYLOAD,
        LW90B5_LAUNCH_DMA
    };

    SemaphoreSet g_CEc7b5Semaphores =
    {
        LWC7B5_SET_SEMAPHORE_A,
        LWC7B5_SET_SEMAPHORE_B,
        LWC7B5_SET_SEMAPHORE_PAYLOAD,
        LWC7B5_SET_SEMAPHORE_PAYLOAD_UPPER,
        LWC7B5_LAUNCH_DMA
    };

    EngineData g_CEClasses =
    {
        make_tuple( BLACKWELL_DMA_COPY_A, "blackwell_dma_copy_a", &g_CEc7b5Semaphores,    GPU_CLASS_BLACKWELL ),
        make_tuple( HOPPER_DMA_COPY_A,    "hopper_dma_copy_a",    &g_CEc7b5Semaphores,    GPU_CLASS_HOPPER    ),
        make_tuple( AMPERE_DMA_COPY_B,    "ampere_dma_copy_b",    &g_CEc7b5Semaphores,    GPU_CLASS_AMPERE    ),
        make_tuple( AMPERE_DMA_COPY_A,    "ampere_dma_copy_a",    &g_CE90b5Semaphores,    GPU_CLASS_AMPERE    ),
        make_tuple( TURING_DMA_COPY_A,    "turing_dma_copy_a",    &g_CE90b5Semaphores,    GPU_CLASS_TURING    ),
        make_tuple( VOLTA_DMA_COPY_A,     "volta_dma_copy_a",     &g_CE90b5Semaphores,    GPU_CLASS_VOLTA     ),
        make_tuple( PASCAL_DMA_COPY_B,    "pascal_dma_copy_b",    &g_CE90b5Semaphores,    GPU_CLASS_PASCAL_B  ),
        make_tuple( PASCAL_DMA_COPY_A,    "pascal_dma_copy_a",    &g_CE90b5Semaphores,    GPU_CLASS_PASCAL_A  ),
        make_tuple( MAXWELL_DMA_COPY_A,   "maxwell_dma_copy_a",   &g_CE90b5Semaphores,    GPU_CLASS_MAXWELL   ),
        make_tuple( KEPLER_DMA_COPY_A,    "kepler_dma_copy_a",    &g_CE90b5Semaphores,    GPU_CLASS_KEPLER    ),
        make_tuple( GF100_DMA_COPY,       "gf100_dma_copy",       &g_CE90b5Semaphores,    GPU_CLASS_FERMI     )
    };

    //-------------------------------------------------------------------------
    //! COMPUTE Classes
    //!
    
    SemaphoreSet g_Compute90c0Semaphores = 
    {
        LW90C0_SET_REPORT_SEMAPHORE_A,
        LW90C0_SET_REPORT_SEMAPHORE_B,
        LW90C0_SET_REPORT_SEMAPHORE_C,
        LW90C0_SET_REPORT_SEMAPHORE_D
    };

    SemaphoreSet g_Computec7c0Semaphores = 
    {
        LWC7C0_SET_REPORT_SEMAPHORE_A,
        LWC7C0_SET_REPORT_SEMAPHORE_B,
        LWC7C0_SET_REPORT_SEMAPHORE_C,
        LWC7C0_SET_REPORT_SEMAPHORE_D,
        LWC7C0_SET_REPORT_SEMAPHORE_PAYLOAD_LOWER,
        LWC7C0_SET_REPORT_SEMAPHORE_PAYLOAD_UPPER,
        LWC7C0_SET_REPORT_SEMAPHORE_ADDRESS_LOWER,
        LWC7C0_SET_REPORT_SEMAPHORE_ADDRESS_UPPER,
        LWC7C0_REPORT_SEMAPHORE_EXELWTE
    };

    EngineData g_ComputeClasses = 
    {
        make_tuple( BLACKWELL_COMPUTE_A,  "blackwell_compute_a",    &g_Computec7c0Semaphores,   GPU_CLASS_BLACKWELL ),
        make_tuple( HOPPER_COMPUTE_B,     "hopper_compute_b",       &g_Computec7c0Semaphores,   GPU_CLASS_HOPPER    ),
        make_tuple( HOPPER_COMPUTE_A,     "hopper_compute_a",       &g_Computec7c0Semaphores,   GPU_CLASS_HOPPER    ),
        make_tuple( ADA_COMPUTE_A,        "ada_compute_a",          &g_Computec7c0Semaphores,   GPU_CLASS_ADA       ),
        make_tuple( AMPERE_COMPUTE_B,     "ampere_compute_b",       &g_Computec7c0Semaphores,   GPU_CLASS_AMPERE    ),
        make_tuple( AMPERE_COMPUTE_A,     "ampere_compute_a",       &g_Compute90c0Semaphores,   GPU_CLASS_AMPERE    ),
        make_tuple( TURING_COMPUTE_A,     "turing_compute_a",       &g_Compute90c0Semaphores,   GPU_CLASS_TURING    ),
        make_tuple( VOLTA_COMPUTE_B,      "volta_compute_b",        &g_Compute90c0Semaphores,   GPU_CLASS_VOLTA     ),
        make_tuple( VOLTA_COMPUTE_A,      "volta_compute_a",        &g_Compute90c0Semaphores,   GPU_CLASS_VOLTA     ),
        make_tuple( PASCAL_COMPUTE_B,     "pascal_compute_b",       &g_Compute90c0Semaphores,   GPU_CLASS_PASCAL_B  ),
        make_tuple( PASCAL_COMPUTE_A,     "pascal_compute_a",       &g_Compute90c0Semaphores,   GPU_CLASS_PASCAL_A  ),
        make_tuple( MAXWELL_COMPUTE_B,    "maxwell_compute_b",      &g_Compute90c0Semaphores,   GPU_CLASS_MAXWELL   ),
        make_tuple( MAXWELL_COMPUTE_A,    "maxwell_compute_a",      &g_Compute90c0Semaphores,   GPU_CLASS_MAXWELL   ),
        make_tuple( KEPLER_COMPUTE_B,     "kepler_compute_b",       &g_Compute90c0Semaphores,   GPU_CLASS_KEPLER    ),
        make_tuple( KEPLER_COMPUTE_A,     "kepler_compute_a",       &g_Compute90c0Semaphores,   GPU_CLASS_KEPLER    ),
        make_tuple( FERMI_COMPUTE_B,      "fermi_compute_b",        &g_Compute90c0Semaphores,   GPU_CLASS_FERMI     ),
        make_tuple( FERMI_COMPUTE_A,      "fermi_compute_a",        &g_Compute90c0Semaphores,   GPU_CLASS_FERMI     )
    };

    //-------------------------------------------------------------------------
    //! LWJPG Classes
    //!
    
    SemaphoreSet g_LwJpgc4d1Semaphores = 
    {
        LWC4D1_SEMAPHORE_A,
        LWC4D1_SEMAPHORE_B,
        LWC4D1_SEMAPHORE_C,
        LWC4D1_SEMAPHORE_D
    };

    EngineData g_LwJpgClasses =
    {
        make_tuple( LWC9D1_VIDEO_LWJPG, "lwc9d1_video_lwjpg",   &g_LwJpgc4d1Semaphores,     GPU_CLASS_ADA    ),
        make_tuple( LWB8D1_VIDEO_LWJPG, "lwb8d1_video_lwjpg",   &g_LwJpgc4d1Semaphores,     GPU_CLASS_HOPPER ),
        make_tuple( LWC4D1_VIDEO_LWJPG, "lwc4d1_video_lwjpg",   &g_LwJpgc4d1Semaphores,     GPU_CLASS_TURING )
    };

    //-------------------------------------------------------------------------
    //! OFA Classes
    //!
    
    SemaphoreSet g_Ofac6faSemaphores = 
    {
        LWC6FA_SEMAPHORE_A,
        LWC6FA_SEMAPHORE_B,
        LWC6FA_SEMAPHORE_C,
        LWC6FA_SEMAPHORE_D
    };

    SemaphoreSet g_Ofac7faSemaphores = 
    {
        LWC7FA_SEMAPHORE_A,
        LWC7FA_SEMAPHORE_B,
        LWC7FA_SEMAPHORE_C,
        LWC7FA_SET_SEMAPHORE_PAYLOAD_LOWER,
        LWC7FA_SET_SEMAPHORE_PAYLOAD_UPPER,
        LWC7FA_SEMAPHORE_D
    };

    EngineData g_OfaClasses =
    {
        make_tuple( LWC9FA_VIDEO_OFA,   "lwc9fa_video_ofa", &g_Ofac7faSemaphores,   GPU_CLASS_ADA       ),
        make_tuple( LWB8FA_VIDEO_OFA,   "lwb8fa_video_ofa", &g_Ofac7faSemaphores,   GPU_CLASS_HOPPER    ),
        make_tuple( LWC7FA_VIDEO_OFA,   "lwc7fa_video_ofa", &g_Ofac7faSemaphores,   GPU_CLASS_AMPERE    ),
        make_tuple( LWC6FA_VIDEO_OFA,   "lwc6fa_video_ofa", &g_Ofac6faSemaphores,   GPU_CLASS_AMPERE    )
    };

    //-------------------------------------------------------------------------
    //! SEC Classes
    //!
    
    SemaphoreSet g_Sec95a1Semaphores = 
    {
        LW95A1_SEMAPHORE_A,
        LW95A1_SEMAPHORE_B,
        LW95A1_SEMAPHORE_C,
        LW95A1_SEMAPHORE_D
    };

    EngineData g_SecClasses =
    {
        make_tuple( LW95A1_TSEC,   "lw95a1_video_sec", &g_Sec95a1Semaphores,    GPU_CLASS_FERMI )
    };

    //-------------------------------------------------------------------------
    //! SW Classes
    //!
    
    EngineData g_SwClasses =
    {
        make_tuple( LW04_SOFTWARE_TEST, "lw04_software_test",   nullptr,  GPU_CLASS_FERMI )
    };

    //-------------------------------------------------------------------------
    //! GRAPHICS Classes
    //!

    SemaphoreSet g_Gr9097Semaphores =
    {
        LW9097_SET_REPORT_SEMAPHORE_A,
        LW9097_SET_REPORT_SEMAPHORE_B,
        LW9097_SET_REPORT_SEMAPHORE_C,
        LW9097_SET_REPORT_SEMAPHORE_D
    };

    SemaphoreSet g_Grc797Semaphores = 
    {
        LWC797_SET_REPORT_SEMAPHORE_PAYLOAD_LOWER,
        LWC797_SET_REPORT_SEMAPHORE_PAYLOAD_UPPER,
        LWC797_SET_REPORT_SEMAPHORE_ADDRESS_LOWER,
        LWC797_SET_REPORT_SEMAPHORE_ADDRESS_UPPER,
        LWC797_REPORT_SEMAPHORE_EXELWTE,
    };

    EngineData g_GrClasses = 
    {
        make_tuple( BLACKWELL_A,  "blackwell_a",  &g_Grc797Semaphores,  GPU_CLASS_BLACKWELL ),
        make_tuple( HOPPER_B,     "hopper_b",     &g_Grc797Semaphores,  GPU_CLASS_HOPPER    ),
        make_tuple( HOPPER_A,     "hopper_a",     &g_Grc797Semaphores,  GPU_CLASS_HOPPER    ),
        make_tuple( ADA_A,        "ada_a",        &g_Grc797Semaphores,  GPU_CLASS_ADA       ),
        make_tuple( AMPERE_B,     "ampere_b",     &g_Grc797Semaphores,  GPU_CLASS_AMPERE    ),
        make_tuple( AMPERE_A,     "ampere_a",     &g_Gr9097Semaphores,  GPU_CLASS_AMPERE    ),
        make_tuple( TURING_A,     "turing_a",     &g_Gr9097Semaphores,  GPU_CLASS_TURING    ),
        make_tuple( VOLTA_B,      "volta_b",      &g_Gr9097Semaphores,  GPU_CLASS_VOLTA     ),
        make_tuple( VOLTA_A,      "volta_a",      &g_Gr9097Semaphores,  GPU_CLASS_VOLTA     ),
        make_tuple( PASCAL_B,     "pascal_b",     &g_Gr9097Semaphores,  GPU_CLASS_PASCAL_B  ),
        make_tuple( PASCAL_A,     "pascal_a",     &g_Gr9097Semaphores,  GPU_CLASS_PASCAL_A  ),
        make_tuple( MAXWELL_B,    "maxwell_b",    &g_Gr9097Semaphores,  GPU_CLASS_MAXWELL   ),
        make_tuple( MAXWELL_A,    "maxwell_a",    &g_Gr9097Semaphores,  GPU_CLASS_MAXWELL   ),
        make_tuple( KEPLER_C,     "kepler_c",     &g_Gr9097Semaphores,  GPU_CLASS_KEPLER    ),
        make_tuple( KEPLER_B,     "kepler_b",     &g_Gr9097Semaphores,  GPU_CLASS_KEPLER    ),
        make_tuple( KEPLER_A,     "kepler_a",     &g_Gr9097Semaphores,  GPU_CLASS_KEPLER    ),
        make_tuple( FERMI_C,      "fermi_c",      &g_Gr9097Semaphores,  GPU_CLASS_FERMI     ),
        make_tuple( FERMI_B,      "fermi_b",      &g_Gr9097Semaphores,  GPU_CLASS_FERMI     ),
        make_tuple( FERMI_A,      "fermi_a",      &g_Gr9097Semaphores,  GPU_CLASS_FERMI     )
    };

    vector<ClassTypeData> g_ClassTypes = 
    {
        { "Gr",         ClassType::GRAPHICS,    &g_GrClasses        },
        { "Compute",    ClassType::COMPUTE,     &g_ComputeClasses   },
        { "Lwdec",      ClassType::LWDEC,       &g_LwDecClasses     },
        { "Lwenc",      ClassType::LWENC,       &g_LwEncClasses     },
        { "Ce",         ClassType::COPY,        &g_CEClasses        },
        { "Lwjpg",      ClassType::LWJPG,       &g_LwJpgClasses     },
        { "Ofa",        ClassType::OFA,         &g_OfaClasses       },
        { "Sec",        ClassType::SEC,         &g_SecClasses       }
    };

    map<Engine, vector<ClassType>> g_EngineSupportedClassTypes=
    {
        { Engine::GRAPHICS, { ClassType::GRAPHICS, ClassType::COMPUTE, ClassType::COPY }    },
        { Engine::LWDEC,    { ClassType::LWDEC }                                            },
        { Engine::LWENC,    { ClassType::LWENC }                                            },
        { Engine::COPY,     { ClassType::COPY }                                             },
        { Engine::LWJPG,    { ClassType::LWJPG }                                            },
        { Engine::OFA,      { ClassType::OFA }                                              },
        { Engine::SEC,      { ClassType::SEC }                                              }
    };
}

vector<unique_ptr<EngineClasses>> EngineClasses::s_EngineClassObjs;

EngineClasses::EngineClasses
(
    string classTypeStr, 
    LWGpuClasses::ClassType classTypeEnum,
    ClassNameMap&& engineClasses,
    ClassSemaphoreMap&& engineSemaphores
) :
    m_ClassTypeStr(classTypeStr),
    m_ClassTypeEnum(classTypeEnum),
    m_EngineClassNameMap(engineClasses),
    m_EngineClassSemaphoreMap(engineSemaphores)
{
    m_ClassVec.resize(m_EngineClassNameMap.size());

    for_each(m_EngineClassNameMap.begin(), m_EngineClassNameMap.end(),
                [&](pair<const UINT32, string> &element) 
                { m_ClassVec.push_back(element.first); });
}

/* static */ void EngineClasses::FreeEngineClassesObjs()
{
    s_EngineClassObjs.clear();
}

/* static */ EngineClasses* EngineClasses::GetEngineClassesObj(string classType)
{
    auto it = find_if(s_EngineClassObjs.begin(), s_EngineClassObjs.end(),
                    [classType](unique_ptr<EngineClasses>& pEngineClasses)
                        {return boost::iequals(pEngineClasses->m_ClassTypeStr, classType);});

    if (it == s_EngineClassObjs.end())
    {
        ErrPrintf("%s: %s is not supported in LWGpuClasses\n", 
                    __FUNCTION__, classType.c_str());
        MASSERT(!"Generic assertion failure<refer to previous message>.");
    }

    return it->get();
}

/* static */ EngineClasses* EngineClasses::GetEngineClassesObj(LWGpuClasses::ClassType classType)
{
    auto it = find_if(s_EngineClassObjs.begin(), s_EngineClassObjs.end(),
                    [classType](unique_ptr<EngineClasses>& pEngineClasses)
                        {return pEngineClasses->m_ClassTypeEnum == classType;});

    if (it == s_EngineClassObjs.end())
    {
        ErrPrintf("classType %d is not supported in LWGpuClasses\n", 
                    classType);
        MASSERT(!"Generic assertion failure<refer to previous message>.");
    }

    return it->get();
}

/* static */ void EngineClasses::CreateAllEngineClasses()
{
    for (auto const & engineData : LWGpuClasses::g_ClassTypes)
    {
        ClassNameMap engineClassNameMap;
        ClassSemaphoreMap engineClassSemaphoreMap;
        ClassFamilyMap engineClassFamilyMap;

        for (auto const & classData : *(engineData.m_pEngineData)) 
        {
            engineClassNameMap[get<0>(classData)] = get<1>(classData);
            engineClassSemaphoreMap[get<0>(classData)] = get<2>(classData);
            engineClassFamilyMap[get<0>(classData)] = get<3>(classData);
        }
        unique_ptr<EngineClasses> pEngineClasses(
            new EngineClasses(engineData.m_ClassTypeStr, engineData.m_ClassTypeEnum, 
                move(engineClassNameMap), move(engineClassSemaphoreMap)));
        s_EngineClassObjs.push_back(move(pEngineClasses));
    }
}

/* static */ vector<UINT32>& EngineClasses::GetClassVec(string classType)
{
    return GetEngineClassesObj(classType)->m_ClassVec;
}
   
/* static */ RC EngineClasses::GetFirstSupportedClass
(
    LwRm* pLwRm,
    GpuDevice* pGpuDevice,
    string classType,
    UINT32* supportedClass
)
{
    RC rc;
    auto it = GetEngineClassesObj(classType);
        
    CHECK_RC(pLwRm->GetFirstSupportedClass(
             it->GetClassVec().size(),
             it->GetClassVec().data(),
             supportedClass,
             pGpuDevice));
    return rc;
}

/* static */ RC EngineClasses::GetFirstSupportedClass
(
    LwRm* pLwRm,
    GpuDevice* pGpuDevice,
    LWGpuClasses::Engine engineType,
    UINT32* supportedClass,
    LWGpuClasses::ClassType classType
)
{
    RC rc;

    if (LWGpuClasses::g_EngineSupportedClassTypes.find(engineType) == 
        LWGpuClasses::g_EngineSupportedClassTypes.end())
    {
        ErrPrintf("Unsupported engineType passed to get supported classes\n");
        return RC::SOFTWARE_ERROR;
    }

    if (LWGpuClasses::g_EngineSupportedClassTypes[engineType].size() > 1)
    {
        if (classType >= LWGpuClasses::ClassType::CLASS_TYPE_END)
        {
            ErrPrintf("classType is not provided for an engine which supports multiple classTypes\n");
            return RC::SOFTWARE_ERROR;
        }
        if (find(LWGpuClasses::g_EngineSupportedClassTypes[engineType].begin(), 
                    LWGpuClasses::g_EngineSupportedClassTypes[engineType].end(),
                    classType) ==
                LWGpuClasses::g_EngineSupportedClassTypes[engineType].end())
        {
            ErrPrintf("The provided classType is not supported by the engine\n");
            return RC::SOFTWARE_ERROR;
        }
    }
    else
    {
        classType = LWGpuClasses::g_EngineSupportedClassTypes[engineType][0];
    }

    auto it = GetEngineClassesObj(classType);

    CHECK_RC(pLwRm->GetFirstSupportedClass(
             it->GetClassVec().size(),
             it->GetClassVec().data(),
             supportedClass,
             pGpuDevice));

    return rc;
}

/* static */ bool EngineClasses::IsClassType
(
    string classType, 
    UINT32 classNum
)
{
    auto it = GetEngineClassesObj(classType);

    return (it->m_EngineClassNameMap.find(classNum) != it->m_EngineClassNameMap.end());;
}

/* static */ string EngineClasses::GetClassName
(
    UINT32 classNum
)
{
    for (auto const & pEngineObj : s_EngineClassObjs)
    {
        if (pEngineObj->m_EngineClassNameMap.find(classNum) != pEngineObj->m_EngineClassNameMap.end())
        {
            return pEngineObj->m_EngineClassNameMap[classNum];
        }
    }
    return "";
}

/* static */ bool EngineClasses::GetClassNum
(
    string className,
    UINT32* classNum
)
{
    for (auto const & pEngineObj : s_EngineClassObjs)
    {
        for (auto const & classNumName : pEngineObj->m_EngineClassNameMap)
        {
            if (className == classNumName.second)
            {
                *classNum = classNumName.first;
                return true;
            }
        }

    }
    return false;
}

/* static */ bool EngineClasses::IsSemaphoreMethod
(
    UINT32 method,
    UINT32 subChannelClass,
    UINT32 channelClass
)
{
    for (auto const & pEngineObj : s_EngineClassObjs)
    {
        vector<UINT32> classVec = pEngineObj->GetClassVec();
        if (find(classVec.begin(), classVec.end(), subChannelClass) != classVec.end())
        {
            SemaphoreSet* pSemaphoreMethods = pEngineObj->m_EngineClassSemaphoreMap[subChannelClass];
            if (pSemaphoreMethods->find(method) != pSemaphoreMethods->end())
            {
                return true;
            }
        }
    }

    // check for Host Semaphore

    if (LWGpuClasses::g_HostSemaphoreMap.find(channelClass) != LWGpuClasses::g_HostSemaphoreMap.end())
    {
        SemaphoreSet* pSemaphoreMethods = LWGpuClasses::g_HostSemaphoreMap[channelClass];
        if (pSemaphoreMethods->find(method) != pSemaphoreMethods->end())
        {
            return true;
        }
    }

    return false;
}

/* static */ bool EngineClasses::IsGpuFamilyClassOrLater(UINT32 classNum, UINT32 gpuFamily)
{
    for (auto const & pEngineObj : s_EngineClassObjs)
    {
        if (pEngineObj->m_EngineClassFamilyMap.find(classNum) != pEngineObj->m_EngineClassFamilyMap.end())
        {
            return (pEngineObj->m_EngineClassFamilyMap[classNum] >= gpuFamily);
        }
    }

    // For very old classes like I2M fallback to old method of determining family
    return ((classNum & 0xFFF00) >= gpuFamily);
}
