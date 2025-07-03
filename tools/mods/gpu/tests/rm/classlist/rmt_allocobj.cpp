/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2004-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//!
//! \file rmt_allocobj.cpp
//! \brief Test ensures the complaince to the chip architecture
//!        on which it exelwtes.
//!
//! The purpose of this test is to make sure that the list of classes exported
//! by the chip's architecture matches actual implementation. To achieve that
//! this test gets the list of the classes exported by the chip's architecture
//! (architecture on which this test is lwrrently exelwting) and then to see
//! whether the actual implementation matches by trying to allocate objects of
//! all such classes and sending NULL CTRL command to each allocated object.
//!

#include "gpu/tests/rmtest.h"

#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "gpu/include/dispchan.h"
#include "gpu/tests/rm/utility/changrp.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/pmusub.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "core/include/xp.h"
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl0080/ctrl0080gpu.h"
#include "class/cl0002.h" // LW01_CONTEXT_DMA
#include "class/cl0004.h" // LW01_TIMER
#include "class/cl0005.h" // LW01_EVENT
#include "class/cl003e.h" // LW01_CONTEXT_ERROR_TO_MEMORY
#include "class/cl003f.h" // LW01_MEMORY_LOCAL_PRIVILEGED
#include "class/cl0040.h" // LW01_MEMORY_LOCAL_USER
#include "class/cl006a.h" // LW03_CHANNEL_PIO
#include "class/cl006b.h" // LW03_CHANNEL_DMA
#include "class/cl006c.h" // LW04_CHANNEL_DMA
#include "class/cl006d.h" // LW04_CHANNEL_PIO
#include "class/cl006e.h" // LW10_CHANNEL_DMA
#include "class/cl0072.h" // LW01_MEMORY_LOCAL_DESCRIPTOR
#include "class/cl0073.h" // LW04_DISPLAY_COMMON
#include "class/cl0076.h" // LW01_MEMORY_FRAMEBUFFER_CONSOLE
#include "class/cl206e.h" // LW20_CHANNEL_DMA
#include "class/cl90ec.h" // GF100_HDACODEC
#include "class/cl366e.h" // LW36_CHANNEL_DMA
#include "class/cl406e.h" // LW40_CHANNEL_DMA
#include "class/cl446e.h" // LW44_CHANNEL_DMA
#include "class/cle26e.h" // LWE2_CHANNEL_DMA
#include "class/cle22d.h" // LW_E2_TWOD
#include "class/cle26d.h" // LWE2_SYNCPOINT
#include "class/cle276.h" // LWE2_AVP
#include "class/cle24d.h" // LW_E2_CAPTURE
#include "class/cle2ad.h" // LWE2_SYNCPOINT
#include "class/cl506f.h" // LW50_CHANNEL_GPFIFO
#include "class/cl25a0.h" // LW25_MULTICHIP_VIDEO_SPLIT
#include "class/cl4176.h" // LW41_VIDEOPROCESSOR
#include "class/cl826f.h" // G82_CHANNEL_GPFIFO
#include "class/cl866f.h" // GT21A_CHANNEL_GPFIFO
#include "class/cl906f.h" // GF100_CHANNEL_GPFIFO
#include "class/cl9097.h" // FERMI_A
#include "class/cl8297.h" // G82_TESLA
#include "class/cl503b.h" // LW50_P2P
#include "class/cl503c.h" // LW50_THIRD_PARTY_P2P
#include "class/cl5097.h" // LW50_TESLA
#include "class/cl824d.h" // G82_EXTERNAL_VIDEO_DECODER
#include "class/cl8274.h" // LW82_VIDEOACCEL
#include "class/cl844c.h" // PERF BUFFER
#include "class/cl0030.h" // LW01_NULL
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cl007d.h" // LW04_SOFTWARE_TEST
#include "class/cl50c0.h" // LW50_COMPUTE
#include "class/cl502d.h" // LW50_TWOD
#include "class/cl50a0.h" // LW50_MEMORY_VIRTUAL
#include "class/cl90c0.h" // FERMI_COMPUTE_A
#include "class/cl902d.h" // FERMI_TWOD_A

#include "class/cl8570.h" // GT214_DISPLAY
#include "class/cl857a.h" // LW857A_LWRSOR_CHANNEL_PIO
#include "class/cl857b.h" // LW857B_OVERLAY_IMM_CHANNEL_PIO
#include "class/cl857c.h" // LW857C_BASE_CHANNEL_DMA
#include "class/cl857d.h" // LW857D_CORE_CHANNEL_DMA
#include "class/cl857e.h" // LW857E_OVERLAY_CHANNEL_DMA

#include "class/cl8370.h" // GT200_DISPLAY
#include "class/cl837c.h" // LW837C_BASE_CHANNEL_DMA
#include "class/cl837d.h" // LW837D_CORE_CHANNEL_DMA
#include "class/cl837e.h" // LW837E_OVERLAY_CHANNEL_DMA
#include "class/cl83de.h" // GT200_DEBUGGER

#include "class/cl8870.h" // G94_DISPLAY
#include "class/cl887d.h" // LW887D_CORE_CHANNEL_DMA

#include "class/cl8270.h" // G82_DISPLAY
#include "class/cl827a.h" // LW827A_LWRSOR_CHANNEL_PIO
#include "class/cl827b.h" // LW827B_OVERLAY_IMM_CHANNEL_PIO
#include "class/cl827c.h" // LW827C_BASE_CHANNEL_DMA
#include "class/cl827d.h" // LW827D_CORE_CHANNEL_DMA
#include "class/cl827e.h" // LW827E_OVERLAY_CHANNEL_DMA
#include "class/cl85b6.h" // GT212_SUBDEVICE_PMU

#include "class/cl5070.h" // LW50_DISPLAY
#include "class/cl507a.h" // LW507A_LWRSOR_CHANNEL_PIO
#include "class/cl507b.h" // LW507B_OVERLAY_IMM_CHANNEL_PIO
#include "class/cl507c.h" // LW507C_BASE_CHANNEL_DMA
#include "class/cl507d.h" // LW507D_CORE_CHANNEL_DMA
#include "class/cl507e.h" // LW507E_OVERLAY_CHANNEL_DMA
#include "class/cl907f.h" // GF100_REMAPPER
#include "class/cl9072.h" // GF100_DISP_SW
#include "class/cl9070.h" // LW9070_DISPLAY
#include "class/cl9170.h" // LW9170_DISPLAY
#include "class/cl9171.h" // LW9171_DISP_SF_USER
#include "class/cl9270.h" // LW9270_DISPLAY
#include "class/cl9470.h" // LW9470_DISPLAY
#include "class/cl9570.h" // LW9570_DISPLAY
#include "class/cl9770.h" // LW9770_DISPLAY
#include "class/cl9870.h" // LW9870_DISPLAY
#include "class/clc370.h" // LWC370_DISPLAY
#include "class/clc570.h" // LWC570_DISPLAY
#include "class/clc670.h" // LWC670_DISPLAY
#include "class/clc770.h" // LWC770_DISPLAY
#include "class/cl9271.h" // LW9271_DISP_SF_USER
#include "class/cl9471.h" // LW9471_DISP_SF_USER
#include "class/cl9571.h" // LW9571_DISP_SF_USER
#include "class/clc371.h" // LWC371_DISP_SF_USER
#include "class/clc671.h" // LWC671_DISP_SF_USER
#include "class/clc771.h" // LWC771_DISP_SF_USER
#include "class/cl90e0.h" // GF100_SUBDEVICE_GRAPHICS
#include "class/cl90e1.h" // GF100_SUBDEVICE_FB
#include "class/cl90e3.h" // GF100_SUBDEVICE_FLUSH
#include "class/cl90e4.h" // GF100_SUBDEVICE_LTCG
#include "class/cl90e5.h" // GF100_SUBDEVICE_TOP
#include "class/cl90e6.h" // GF100_SUBDEVICE_MASTER
#include "class/cl90e7.h" // GF100_SUBDEVICE_INFOROM
#include "class/cl90e8.h" // LW_PHYS_SUB_MEMALLOCATOR
#include "class/clc58b.h" // TURING_VMMU_A
#include "class/cla06c.h" // KEPLER_CHANNEL_GROUP_A
#include "class/cla06f.h" // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla0e0.h" // GK110_SUBDEVICE_GRAPHICS
#include "class/cla0e1.h" // GK110_SUBDEVICE_FB
#include "class/cla16f.h" // KEPLER_CHANNEL_GPFIFO_B
#include "class/cla26f.h" // KEPLER_CHANNEL_GPFIFO_C
#include "class/clb06f.h" // MAXWELL_CHANNEL_GPFIFO_A
#include "class/cl00db.h" // LW40_DEBUG_BUFFER
#include "class/cl9096.h" // GF100_ZBC_CLEAR
#include "class/cl9067.h" // FERMI_CONTEXT_SHARE_A
#include "class/cl90f1.h" // FERMI_VASPACE_A
#include "class/cle3f1.h" // TEGRA_VASPACE_A

#include "class/clb069.h"   // MAXWELL_FAULT_BUFFER_A
#include "class/clb069sw.h" // LWB069_ALLOCATION_PARAMETERS
#include "class/clb097.h"   // MAXWELL_A
#include "class/clb0c0.h"   // MAXWELL_COMPUTE_A
#include "class/clb1c0.h"   // MAXWELL_COMPUTE_B
#include "class/clb6b9.h"   // MAXWELL_SEC2

#include "class/clc097.h"   // PASCAL_A
#include "class/clc0c0.h"   // PASCAL_COMPUTE_A
#include "class/clc06f.h"   // PASCAL_CHANNEL_GPFIFO_A
#include "class/clc197.h"   // PASCAL_B
#include "class/clc1c0.h"   // PASCAL_COMPUTE_B
#include "class/clc0e0.h"   // GP100_SUBDEVICE_GRAPHICS
#include "class/clc0e1.h"   // GP100_SUBDEVICE_FB

#include "class/clc310.h"   // VOLTA_GSP
#include "class/clc361.h"   // VOLTA_USERMODE_A
#include "class/clc365.h"   // ACCESS_COUNTER_NOTIFY_BUFFER
#include "class/clc369.h"   // MMU_FAULT_BUFFER
#include "class/clc36f.h"   // VOLTA_CHANNEL_GPFIFO_A
#include "class/clc397.h"   // VOLTA_A
#include "class/clc3c0.h"   // VOLTA_COMPUTE_A
#include "class/clc3e0.h"   // GV100_SUBDEVICE_GRAPHICS
#include "class/clc3e1.h"   // GV100_SUBDEVICE_FB

#include "class/clc461.h"   // TURING_USERMODE_A
#include "class/clc46f.h"   // TURING_CHANNEL_GPFIFO_A
#include "class/clc597.h"   // TURING_A
#include "class/clc5c0.h"   // TURING_COMPUTE_A

#include "class/clc697.h"   // AMPERE_A
#include "class/clc6c0.h"   // AMPERE_COMPUTE_A
#include "class/clc561.h"   // AMPERE_USERMODE_A
#include "class/clc56f.h"   // AMPERE_CHANNEL_GPFIFO_A

#include "class/clc797.h"   // AMPERE_B
#include "class/clc7c0.h"   // AMPERE_COMPUTE_B

#include "class/clc997.h"   // ADA_A
#include "class/clc9c0.h"   // ADA_COMPUTE_A

#include "class/clcb97.h"   // HOPPER_A
#include "class/clcbc0.h"   // HOPPER_COMPUTE_A
#include "class/clc661.h"   // HOPPER_USERMODE_A
#include "class/clc86f.h"   // HOPPER_CHANNEL_GPFIFO_A

#include "class/clcc97.h"   // HOPPER_B
#include "class/clccc0.h"   // HOPPER_COMPUTE_B

#include "class/clcd97.h"   // BLACKWELL_A
#include "class/clcdc0.h"   // BLACKWELL_COMPUTE_A

#include "class/clc572.h"   // PHYSICAL_CHANNEL_GPFIFO

#include "class/cl907a.h" // LW907A_LWRSOR_CHANNEL_PIO
#include "class/cl907b.h" // LW907B_OVERLAY_IMM_CHANNEL_PIO
#include "class/cl907c.h" // LW907C_BASE_CHANNEL_DMA
#include "class/cl907d.h" // LW907D_CORE_CHANNEL_DMA
#include "class/cl907e.h" // LW907E_OVERLAY_CHANNEL_DMA

#include "class/cl917a.h" // LW917A_LWRSOR_CHANNEL_PIO
#include "class/cl917b.h" // LW917B_OVERLAY_IMM_CHANNEL_PIO
#include "class/cl917c.h" // LW917C_BASE_CHANNEL_DMA
#include "class/cl917d.h" // LW917D_CORE_CHANNEL_DMA
#include "class/cl917e.h" // LW917E_OVERLAY_CHANNEL_DMA

#include "class/cl927c.h" // LW927C_BASE_CHANNEL_DMA
#include "class/cl927d.h" // LW927D_CORE_CHANNEL_DMA

#include "class/cl947d.h" // LW947D_CORE_CHANNEL_DMA
#include "class/cl957d.h" // LW957D_CORE_CHANNEL_DMA
#include "class/cl977d.h" // LW977D_CORE_CHANNEL_DMA
#include "class/cl987d.h" // LW987D_CORE_CHANNEL_DMA

#include "class/cl9878.h" // LW9878_WRITEBACK_CHANNEL_DMA

#include "class/clc372sw.h" // LWC372_DISPLAY_SW

#include "class/clc378.h" // LWC378_WRITEBACK_CHANNEL_DMA
#include "class/clc373.h" // LWC373_DISP_CAPABILITIES
#include "class/clc37a.h" // LWC37A_LWRSOR_IMM_CHANNEL_PIO
#include "class/clc37b.h" // LWC37B_WINDOW_IMM_CHANNEL_DMA
#include "class/clc37d.h" // LWC37D_CORE_CHANNEL_DMA
#include "class/clc37e.h" // LWC37E_WINDOW_CHANNEL_DMA

#include "class/clc578.h" // LWC578_WRITEBACK_CHANNEL_DMA
#include "class/clc573.h" // LWC573_DISP_CAPABILITIES
#include "class/clc57a.h" // LWC57A_LWRSOR_IMM_CHANNEL_PIO
#include "class/clc57b.h" // LWC57B_WINDOW_IMM_CHANNEL_DMA
#include "class/clc57d.h" // LWC57D_CORE_CHANNEL_DMA
#include "class/clc57e.h" // LWC57E_WINDOW_CHANNEL_DMA

#include "class/clc678.h" // LWC678_WRITEBACK_CHANNEL_DMA
#include "class/clc673.h" // LWC673_DISP_CAPABILITIES
#include "class/clc67a.h" // LWC67A_LWRSOR_IMM_CHANNEL_PIO
#include "class/clc67b.h" // LWC67B_WINDOW_IMM_CHANNEL_DMA
#include "class/clc67d.h" // LWC67D_CORE_CHANNEL_DMA
#include "class/clc67e.h" // LWC67E_WINDOW_CHANNEL_DMA

#include "class/clc773.h" // LWC773_DISP_CAPABILITIES
#include "class/clc77d.h" // LWC77D_CORE_CHANNEL_DMA

#include "class/cla080.h" // KEPLER_DEVICE_VGPU

#include "class/cla081.h" // LWA081_VGPU_CONFIG

#include "class/cla082.h" // LWA082_HOST_VGPU_DEVICE

#include "class/cla083.h" // LWA083_GRID_DISPLAYLESS

#include "class/cla084.h" // LWA084_HOST_VGPU_DEVICE_KERNEL

#include "class/cl507a.h" // LW507A_LWRSOR_CHANNEL_PIO

#include "class/cl0060.h" // LW0060_SYNC_GPU_BOOST

#include "class/cl90cd.h" // LW_EVENT_BUFFER

#include "class/clb8fa.h" // LWB8FA_VIDEO_OFA
#include "class/clc6fa.h" // LWC6FA_VIDEO_OFA
#include "class/clc7fa.h" // LWC7FA_VIDEO_OFA
#include "class/clc9fa.h" // LWC9FA_VIDEO_OFA

#include "class/clcb33.h" // LW_CONFIDENTIAL_COMPUTE

#include "core/include/memcheck.h"

#define ALLOC_PB_SIZE               0x00001000 // size of channel pushbuffer
#define ALLOC_ERR_NOT_SIZE          0x00001000 // size of error notifier
#define CL507D_PB_SIZE              0x00001000 // size of Core display channel PB
#define CL507D_ERROR_NOTIFIER_SIZE  0x00001000 // size of Core chanl error notfr
#define GP_ENTRIES                  0x80       // number of GpFifo Entries
#define MEM_SIZE_VIRTUAL            0x00001000 // memory size for LW50_MEMORY_VIRTUAL class

typedef enum
{
    OBJECT_ALLOC_PARENT_CHANNEL,
    OBJECT_ALLOC_PARENT_DEVICE,
    OBJECT_ALLOC_PARENT_SUBDEVICE,
    OBJECT_ALLOC_PARENT_CHANGRP
} OBJECT_ALLOC_PARENT;

class AllocObjectTest: public RmTest
{
public:
    AllocObjectTest();
    virtual ~AllocObjectTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    LW0080_CTRL_GPU_GET_CLASSLIST_PARAMS m_params;
    LwU32 *m_classBuf; // Data type can't be changed to UINT32
    LwU32 *m_pClass;   // It results in type colwersion error if the data type
                       // changed to UINT32
    std::map<LwU32, LwU32> m_classToEngMap;

    // Push Buffer, GpFifo and Error notifier Related

    void *m_PushBufferBase;
    LwRm::Handle m_hPBCtxDma;
    LwRm::Handle m_hErrorNotifierCtxDma; // error notifier handle
    LwRm::Handle m_hGpCtxDma;  //gpFifoCtxDma handle
    LwRm::Handle m_hGpMem;    //gpFifoMem handle
    LwRm::Handle m_hDisplay;
    LwRm::Handle m_hObjectCore;
    LwRm::Handle m_hObject;
    Surface2D    *vpSurf;
    ChannelGroup *m_pChGrp;

    UINT32 m_PBCtxDmaFlags;
    LwRm::Handle m_hPBMem;

    void * m_ErrorNotifierBase;
    void * m_gpAddress;
    GpuDevMgr *m_pGpuDevMgr;

    UINT32 m_hErrorNotifierMem;
    UINT64 m_ErrorNotifierOffset;
    UINT32 m_ErrNotifierFlags;

    bool m_isPbMap;

    bool m_FreeObject;

    RC ObjectAllocate(OBJECT_ALLOC_PARENT, void *pParams = NULL);
    RC PbErrorAlloc();
    RC AllocCoreDisp(LwRm::Handle *ObjectHandle);
    RC AllocPmuSubDev();
    RC AllocP2P();
    RC AllocVP();
    RC AllocDispDma(LwRm::Handle *ObjectHandle);
    void Cleaner();
};

//! \brief AllocObjectTest constructor.
//!
//! \sa Setup
//----------------------------------------------------------------------------
AllocObjectTest::AllocObjectTest()
{
    SetName("AllocObjectTest");

    m_classBuf=NULL;
    m_pClass=NULL;
    m_PushBufferBase=NULL;
    m_hPBCtxDma=0;
    m_hErrorNotifierCtxDma=0;
    m_hGpCtxDma=0;
    m_hGpMem=0;
    m_hDisplay=0;
    m_hObjectCore=0;
    m_hObject=0;

    m_pChGrp = NULL;
    m_PBCtxDmaFlags=0;
    m_hPBMem=0;
    m_ErrorNotifierBase=NULL;
    m_gpAddress=NULL;

    m_hErrorNotifierMem=0;
    m_ErrorNotifierOffset=0;
    m_ErrNotifierFlags=0;

    m_isPbMap = false;

    m_FreeObject = true;
    vpSurf = NULL;
}

//! \brief AllocObjectTest destructor.
//!
//! \sa Cleanup
//----------------------------------------------------------------------------
AllocObjectTest::~AllocObjectTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//! returns true implying that all platforms are supported
//----------------------------------------------------------------------------
string AllocObjectTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test.
//!
//! Setup is used to reserve all the required resources used by this test,
//! To achieve that this test tries to allocate channel and then tries to get
//! the list of all the classes that current chip exports so that class list
//! can be used in Run ().
//!
//! \return RC -> OK if everything is allocated, test-specific RC if something
//!         failed while selwring resources.
//! \sa Run
//----------------------------------------------------------------------------
RC AllocObjectTest::Setup()
{
    RC rc = LW_OK;
    LwRmPtr pLwRm;
    LW0080_CTRL_GPU_GET_CLASSLIST_V2_PARAMS paramsV2 = {0};

    m_params.classList = 0;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
        LW0080_CTRL_CMD_GPU_GET_CLASSLIST_V2,
        &paramsV2,
        sizeof(paramsV2)));

    m_params.numClasses = paramsV2.numClasses;
    m_classBuf = new LwU32[m_params.numClasses];
    m_params.classList = LW_PTR_TO_LwP64(m_classBuf);

    if (0 == m_params.classList)
    {
        rc = RC::CANNOT_ALLOCATE_MEMORY;
        return rc;
    }

    for (UINT32 i = 0; i < m_params.numClasses; i++)
    {
        m_classBuf[i] = paramsV2.classList[i];
    }

    Printf(Tee::PriLow,"Following classes are supported on current elw\n");

    for (m_pClass = (LwU32 *) LwP64_VALUE(m_params.classList);
         m_pClass < (LwU32*) m_params.classList + m_params.numClasses;
         m_pClass++)
    {
        Printf(Tee::PriLow,
            " Class 0x%x\n", *m_pClass);
    }
    
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS engineParams;
    LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS classParams;

    std::vector<LwU32> locClassList;
    LwU32 engineType;
    std::map<LwU32, LwU32>::iterator it;
    LwU64 classType;
 
    // step 1: query the supported engines
    // pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
    CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                    LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                    &engineParams,
                    sizeof(engineParams)));

    // step 2: choose an engine to get all the classes for
    for (LwU32 i = 0; i < engineParams.engineCount; i++)
    {
        engineType = engineParams.engineList[i];

        // step 3: get all the classes for the selected engine
        classParams.engineType = engineType;
        classParams.numClasses = 0;
        classParams.classList = 0;
        
        // First call to LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST gets us the number of classes
        CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                        LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                        &classParams, sizeof(classParams)));

        // Allocate a buffer based upon the number of classes found for this engine
        locClassList.resize(classParams.numClasses);
        classParams.classList = LW_PTR_TO_LwP64(&(locClassList[0]));

        //
        // Second call to LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST will populate the buffer with the
        // list of classes supported by the engine
        //
        CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                &classParams, sizeof(classParams)));

        // step 4: check if our class is in that engine
        for (LwU32 j = 0; j < classParams.numClasses; j++)
        {
            classType = ((LwU32*)LwP64_VALUE(classParams.classList))[j];
            it = m_classToEngMap.find(classType);
            
            // Only insert the first engine that is associated with a class
            if (it == m_classToEngMap.end())
            {
                m_classToEngMap[classType] = engineType;
            }
            else
            {
                Printf(Tee::PriHigh, "Class 0x%04x already mapped to LW2080_ENGINE_TYPE 0x%02x cannot be mapped to 0x%02x \n",
                       (LwU32) classType, (LwU32) it->second, (LwU32) engineType);
            }
        }
    }
    
Cleanup:
    return rc;
}

//! \brief Run the test!
//!
//! After the setup is successful the allocated resources can be used.
//! Run function uses the allocated resources. Here the function iterates over
//! the list of exported classes and tries to allocate objects of all such
//! classes. As locally used function also frees all such allocated objects
//! after use. The function also prints (low priority) the list and
//! tells whether object allocation is successful or not and the sending
//! NULL CTRL CMD on that object is successful or not.
//! The purpose of test is also to check the control CMD complaince, but
//! lwrrently there are many classes that don't support NULL CMD so the test
//! lwrrently don't fail for known error retun codes on NULL CMDs.
//! Function returns appropriate error if any object allocation fails or
//! NULL CMD fails for the unknown errors.
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC AllocObjectTest::Run()
{
    LwRmPtr pLwRm;
    RC rc;
    RC nullCmdrc;

    LW00DB_ALLOCATION_PARAMETERS DebugBufferParams={0};
    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };

    m_ErrNotifierFlags = DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE);
    UINT64 memLimit = 2*ALLOC_PB_SIZE - 1;

#ifdef LW_VERIF_FEATURES
    UINT32 gpSize;
#endif

    m_pClass = (LwU32 *) LwP64_VALUE(m_params.classList);

    for (m_pClass = (LwU32 *) LwP64_VALUE(m_params.classList);
         m_pClass < (LwU32*) m_params.classList + m_params.numClasses;
         m_pClass++)
    {
        Printf(Tee::PriHigh,"Testing alloc object 0x%x\n", *m_pClass);

        switch(*m_pClass)
        {

        case GF100_REMAPPER:
            Printf(Tee::PriHigh, "Skipping Allocobject of GF100_REMAPPER, see bug 363393\n");
            continue;

        case LW907A_LWRSOR_CHANNEL_PIO:
        case LW917A_LWRSOR_CHANNEL_PIO:
        case LW907B_OVERLAY_IMM_CHANNEL_PIO:
        case LW917B_OVERLAY_IMM_CHANNEL_PIO:
        case LW907C_BASE_CHANNEL_DMA:
        case LW917C_BASE_CHANNEL_DMA:
        case LW927C_BASE_CHANNEL_DMA:
        case LW907D_CORE_CHANNEL_DMA:
        case LW917D_CORE_CHANNEL_DMA:
        case LW927D_CORE_CHANNEL_DMA:
        case LW947D_CORE_CHANNEL_DMA:
        case LW957D_CORE_CHANNEL_DMA:
        case LW977D_CORE_CHANNEL_DMA:
        case LW987D_CORE_CHANNEL_DMA:
        case LW907E_OVERLAY_CHANNEL_DMA:
        case LW917E_OVERLAY_CHANNEL_DMA:
        case LW9878_WRITEBACK_CHANNEL_DMA:
        case LWC378_WRITEBACK_CHANNEL_DMA:
        case LWC372_DISPLAY_SW:
        case LWC37A_LWRSOR_IMM_CHANNEL_PIO:
        case LWC373_DISP_CAPABILITIES:
        case LWC578_WRITEBACK_CHANNEL_DMA:
        case LWC57A_LWRSOR_IMM_CHANNEL_PIO:
        case LWC573_DISP_CAPABILITIES:
        case LWC678_WRITEBACK_CHANNEL_DMA:
        case LWC67A_LWRSOR_IMM_CHANNEL_PIO:
        case LWC673_DISP_CAPABILITIES:
        case LWC773_DISP_CAPABILITIES:

            Printf (Tee::PriHigh, "Testing of class 0x%x disabled\n", (UINT32)*m_pClass);
            continue;

        case LW01_CONTEXT_DMA:
            CHECK_RC_CLEANUP(
                pLwRm->AllocMemory(&m_hPBMem,
                LW01_MEMORY_SYSTEM,
                DRF_DEF(OS02,_FLAGS,_LOCATION,_PCI)|
                DRF_DEF(OS02,_FLAGS,_COHERENCY,_CACHED)|
                DRF_DEF(OS02,_FLAGS,_PHYSICALITY,_NONCONTIGUOUS)|
                DRF_DEF(OS02,_FLAGS,_MAPPING,_NO_MAP),
                NULL,
                &memLimit,
                GetBoundGpuDevice()));

            CHECK_RC_CLEANUP(pLwRm->AllocContextDma(&m_hObject,
                LWOS03_FLAGS_ACCESS_READ_WRITE,
                m_hPBMem, 0, ALLOC_PB_SIZE-1));

            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);

            break;

        // Do not test obsolete DMA channels -- everyone should be using GPFIFO instead
        // and MODS support for DMA channels is being retired.
        case LW04_CHANNEL_DMA:
        case LW03_CHANNEL_DMA:
        case LW10_CHANNEL_DMA:
        case LW20_CHANNEL_DMA:
        case LW36_CHANNEL_DMA:
        case LW40_CHANNEL_DMA:
        case LW44_CHANNEL_DMA:
        case LWE2_CHANNEL_DMA:
            continue;

        // Due to bug 126224, we cannot switch from DMA channels to PIO channels and
        // therefore we cannot test these.  They are super obsolete anyway.
        case LW03_CHANNEL_PIO:
        case LW04_CHANNEL_PIO:
            continue;

            //
            //GPFIFO Channel allocation start
            //

        case LW50_CHANNEL_GPFIFO:
        case G82_CHANNEL_GPFIFO:
        case GT21A_CHANNEL_GPFIFO:
        case GF100_CHANNEL_GPFIFO:
        case KEPLER_CHANNEL_GPFIFO_A:
        case KEPLER_CHANNEL_GPFIFO_B:
        case KEPLER_CHANNEL_GPFIFO_C:
        case MAXWELL_CHANNEL_GPFIFO_A:
        case PASCAL_CHANNEL_GPFIFO_A:
        case VOLTA_CHANNEL_GPFIFO_A:
        case TURING_CHANNEL_GPFIFO_A:
        case AMPERE_CHANNEL_GPFIFO_A:
        case HOPPER_CHANNEL_GPFIFO_A:

            //
            // Allocate error notifier memory ans context dma handle.
            // For this explicit allocation, we can't avoid this #ifdef as
            // the RM code already has them internally.
            //
#ifdef LW_VERIF_FEATURES
            CHECK_RC_CLEANUP(pLwRm->AllocSystemMemory(&m_hErrorNotifierMem,
                DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI)|
                DRF_DEF(OS02, _FLAGS, _COHERENCY, _WRITE_BACK)|
                DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS),
                ALLOC_ERR_NOT_SIZE,
                GetBoundGpuDevice())
                );

            CHECK_RC_CLEANUP(pLwRm->AllocContextDma(&m_hErrorNotifierCtxDma,
                DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE),
                m_hErrorNotifierMem, 0, ALLOC_ERR_NOT_SIZE - 1)
                );

            CHECK_RC_CLEANUP(
                pLwRm->MapMemory (m_hErrorNotifierMem,
                0,
                ALLOC_ERR_NOT_SIZE,
                &m_ErrorNotifierBase,
                0,
                GetBoundGpuSubdevice()));
            //
            // Size of GPfifo.
            // Here 8 indicates each GPFIFO entry's lenght in bytes
            //
            gpSize  = GP_ENTRIES * 8 + ALLOC_PB_SIZE;
            //
            //Allocate GPFIFO buffer memory
            // and associate context dma handle
            //

            CHECK_RC_CLEANUP(pLwRm->AllocSystemMemory(&m_hGpMem,
                DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
                DRF_DEF(OS02, _FLAGS, _COHERENCY, _WRITE_BACK)  |
                DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS),
                gpSize,
                GetBoundGpuDevice())
                );

            CHECK_RC_CLEANUP(pLwRm->AllocContextDma(&m_hGpCtxDma,
                DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_ONLY),
                m_hGpMem, 0, gpSize - 1)
                );

            CHECK_RC_CLEANUP(pLwRm->MapMemory(m_hGpMem, 0, gpSize, &m_gpAddress, 0, GetBoundGpuSubdevice()));

            m_PushBufferBase = (char*)m_gpAddress + GP_ENTRIES * 8;
            //
            // Allocate GPFIFO channel
            //
            CHECK_RC_CLEANUP(pLwRm->AllocChannelGpFifo(
                &m_hObject, *m_pClass,
                m_hErrorNotifierCtxDma,
                m_ErrorNotifierBase,
                m_hGpCtxDma,
                m_PushBufferBase,
                GP_ENTRIES*8,
                ALLOC_PB_SIZE,
                m_gpAddress,
                0,
                GP_ENTRIES,
                0,
                0, // verifFlags
                0, // verifFlags2
                0, // flags
                GetBoundGpuDevice(),
                NULL,
                0,
                LW2080_ENGINE_TYPE_GR(0))
                );

            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);
            break;
#endif
            continue;
            //GPFIFO Channel allocation end

        // Internal channel is not for client use
        case PHYSICAL_CHANNEL_GPFIFO:
            continue;

        case LW01_MEMORY_VIRTUAL:
            CHECK_RC_CLEANUP(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                          &m_hObject, LW01_MEMORY_VIRTUAL, &vmparams));
            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);

            break;

        case LW50_MEMORY_VIRTUAL:
        {
            UINT32 attr = (DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
                           DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
                           DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
                           DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _DEFAULT));
            UINT32 attr2 = LWOS32_ATTR2_NONE;
            CHECK_RC_CLEANUP(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                LWOS32_ALLOC_FLAGS_VIRTUAL,
                &attr,
                &attr2, //pAttr2
                MEM_SIZE_VIRTUAL,
                1,    // alignment
                NULL, // pFormat
                NULL, // pCoverage
                NULL, // pPartitionStride
                &m_hObject,
                NULL, // poffset,
                NULL, // pLimit
                NULL, // pRtnFree
                NULL, // pRtnTotal
                0,    // width
                0,    // height
                GetBoundGpuDevice()));

            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);

            break;
        }

            //
            // Display channels start
            //
            // The following classes allocate different types of
            // display channels. The fundamental is that
            // before allocating any type of channel of any of these
            // classes, channel of class CHANNEL_DMA_CORE is to be
            // allocated first and then proceed to the respective
            // classes thereafter
            //
            // The procedure is through steps:
            // 1. Allocate an object of class G82_DISPLAY/LW50_DISPLAY
            // 2. Allocate Push buffer and associate context dma to that
            // 3. Allocate error notifier memory and associate context dma
            //    for that
            // 4. Allocate a core channel first and then proceed to respective
            //    channel according to the cases listed below
            //

        case LW857A_LWRSOR_CHANNEL_PIO:
        case LW857B_OVERLAY_IMM_CHANNEL_PIO:
        case LW857C_BASE_CHANNEL_DMA:
        case LW857D_CORE_CHANNEL_DMA:
        case LW857E_OVERLAY_CHANNEL_DMA:

            CHECK_RC_CLEANUP(AllocCoreDisp(&m_hObject));

            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);

            break;

        case LW887D_CORE_CHANNEL_DMA:
        case LW837E_OVERLAY_CHANNEL_DMA:
        case LW837C_BASE_CHANNEL_DMA:
        case LW837D_CORE_CHANNEL_DMA:

            CHECK_RC_CLEANUP(AllocCoreDisp(&m_hObject));

            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);

            break;

        case LW827C_BASE_CHANNEL_DMA:

            if((Platform::GetSimulationMode() != Platform::Hardware))
            {
                CHECK_RC_CLEANUP(AllocCoreDisp(&m_hObjectCore));

                CHECK_RC_CLEANUP(pLwRm->AllocDisplayChannelDma(m_hDisplay,
                    &m_hObject,
                    LW827C_BASE_CHANNEL_DMA,
                    0,
                    m_hErrorNotifierCtxDma,
                    m_ErrorNotifierBase,
                    m_hPBCtxDma,
                    m_PushBufferBase,
                    CL507D_PB_SIZE,
                    0,
                    0,
                    GetBoundGpuDevice())
                    );

                Printf(Tee::PriLow,
                    "Object allocation for class 0x%x successful\n",
                    (UINT32)*m_pClass);

                break;
            }

        case LW827A_LWRSOR_CHANNEL_PIO:
        case LW827B_OVERLAY_IMM_CHANNEL_PIO:
        case LW827E_OVERLAY_CHANNEL_DMA:
        case LW827D_CORE_CHANNEL_DMA:

            CHECK_RC_CLEANUP(AllocCoreDisp(&m_hObject));

            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);

            break;

        case LW507C_BASE_CHANNEL_DMA:

            if((Platform::GetSimulationMode() != Platform::Hardware))
            {
                CHECK_RC_CLEANUP(AllocCoreDisp(&m_hObjectCore));

                CHECK_RC_CLEANUP(pLwRm->AllocDisplayChannelDma(m_hDisplay,
                    &m_hObject,
                    LW507C_BASE_CHANNEL_DMA,
                    0,
                    m_hErrorNotifierCtxDma,
                    m_ErrorNotifierBase,
                    m_hPBCtxDma,
                    m_PushBufferBase,
                    CL507D_PB_SIZE,
                    0,
                    0,
                    GetBoundGpuDevice())
                    );

                Printf(Tee::PriLow,
                    "Object allocation for class 0x%x successful\n",
                    (UINT32)*m_pClass);

                break;
            }
            //
            // As the Cursor Channel is owned by display driver and
            // Overlay channels owned by video driver we can't allocate
            // objects for them. Hence skipping those three cases.
            //
        case LW507A_LWRSOR_CHANNEL_PIO:
        case LW507B_OVERLAY_IMM_CHANNEL_PIO:
        case LW507E_OVERLAY_CHANNEL_DMA:
        case LW507D_CORE_CHANNEL_DMA:

            CHECK_RC_CLEANUP(AllocCoreDisp(&m_hObject));

            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);

            break;

        case LWC37B_WINDOW_IMM_CHANNEL_DMA:
        case LWC37E_WINDOW_CHANNEL_DMA:
        case LWC57B_WINDOW_IMM_CHANNEL_DMA:
        case LWC57E_WINDOW_CHANNEL_DMA:
        case LWC67B_WINDOW_IMM_CHANNEL_DMA:
        case LWC67E_WINDOW_CHANNEL_DMA:
            if (pLwRm->IsClassSupported(LW04_DISPLAY_COMMON, GetBoundGpuDevice()))
            {
                CHECK_RC_CLEANUP(AllocCoreDisp(&m_hObjectCore));
                CHECK_RC_CLEANUP(pLwRm->AllocDisplayChannelDma(m_hDisplay,
                                &m_hObject,
                                *m_pClass,
                                0,
                                m_hErrorNotifierCtxDma,
                                m_ErrorNotifierBase,
                                m_hPBCtxDma,
                                m_PushBufferBase,
                                CL507D_PB_SIZE,
                                0,
                                0,
                                GetBoundGpuDevice())
                                );
                Printf(Tee::PriLow,
                        "Object allocation for class 0x%x successful\n",
                        (UINT32)*m_pClass);
                break;
            }
            else
            {
                Printf(Tee::PriLow,
                    "Skipping allocation for class 0x%x\n",
                    (UINT32)*m_pClass);
                continue;
            }

        case LWC37D_CORE_CHANNEL_DMA:
        case LWC57D_CORE_CHANNEL_DMA:
        case LWC67D_CORE_CHANNEL_DMA:
        case LWC77D_CORE_CHANNEL_DMA:
            if (pLwRm->IsClassSupported(LW04_DISPLAY_COMMON, GetBoundGpuDevice()))
            {
                CHECK_RC_CLEANUP(AllocCoreDisp(&m_hObject));
                Printf(Tee::PriLow,
                    "Object allocation for class 0x%x successful\n",
                    (UINT32)*m_pClass);
                break;
            }
            else
            {
                Printf(Tee::PriLow,
                    "Skipping allocation for class 0x%x\n",
                    (UINT32)*m_pClass);
                continue;
            }
            //Display channels end

        case LW41_VIDEOPROCESSOR:

            // 4176 : Video Processor (VP1) is only supported on hardware
            if (Platform::GetSimulationMode() == Platform::Hardware)
            {
                CHECK_RC(ObjectAllocate(OBJECT_ALLOC_PARENT_CHANNEL));

                Printf(Tee::PriLow,
                    "Object allocation for class 0x%x successful\n",
                    (UINT32)*m_pClass);

                break;
            }
            else
            {
                Printf(Tee::PriLow,\
                    "Class 0x%x VP1 supported only on hardware\n",
                    (UINT32)*m_pClass);

                continue;
            }

            //
            // Following are Non-targeted / Omitted classes so simply move
            // ahead in the class array
            //
            // Following Classes: Does not allocate any memory but just
            // returns back the dump mapping of Video memory.But for allocating
            // Video memory there is altogether different API, and these
            // classes might not be used lwrrently so omitting those.
            //
        case LW01_MEMORY_LOCAL_PRIVILEGED:
        case LW01_MEMORY_LOCAL_USER:
        case LW01_MEMORY_FRAMEBUFFER_CONSOLE:
            Printf(Tee::PriLow,\
                "Class 0x%x Not supported\n",(UINT32)*m_pClass);
            Printf(Tee::PriLow,\
                "as this Memory related only gives dump mapping of FB\n");

            continue;

        case LW01_CONTEXT_ERROR_TO_MEMORY:
            Printf(Tee::PriLow,\
                "Class 0x%x Not supported\n",(UINT32)*m_pClass);
            Printf(Tee::PriLow,\
                "Class CTX DMA with RW is good case,No need to have all\n");

            continue;

        case GT212_SUBDEVICE_PMU:
            CHECK_RC_CLEANUP(AllocPmuSubDev());
            break;

        case LW50_P2P:
        {
            rc = AllocP2P();
            if (RC::LWRM_NOT_SUPPORTED == rc)
            {
                Printf(Tee::PriLow,
                       "Object allocation for class 0x%x not supported since P2P disabled\n",
                       (UINT32)*m_pClass);
                rc.Clear();
                continue;
            }
            else if (OK != rc)
            {
                Printf(Tee::PriLow,
                       "Error in Object allocation for class 0x%x",
                       (UINT32)*m_pClass);
                m_FreeObject = false;
                goto Cleanup;
            }
            Printf(Tee::PriLow,
                   "Object allocation for class 0x%x successful\n",
                   (UINT32)*m_pClass);
            break;
        }

        case LW40_DEBUG_BUFFER:
            // DebugBuffer needs to be allocated with a size parameter
            DebugBufferParams.size = 4096;

            CHECK_RC_CLEANUP(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                        &m_hObject,
                                        *m_pClass,
                                        &DebugBufferParams));

            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);
            break;

        case LW25_MULTICHIP_VIDEO_SPLIT:
        {
            LW25A0_ALLOCATION_PARAMETERS params={0};
            params.logicalHeadId = 0;
            CHECK_RC_CLEANUP(ObjectAllocate(OBJECT_ALLOC_PARENT_CHANNEL, &params));
            break;
        }
        case GF100_DISP_SW:
        {
            // When display is disabled, we shouldn't test this.
            if (pLwRm->IsClassSupported(LW04_DISPLAY_COMMON, GetBoundGpuDevice()))
            {
                LW9072_ALLOCATION_PARAMETERS params = {0};
                CHECK_RC_CLEANUP(ObjectAllocate(OBJECT_ALLOC_PARENT_CHANNEL, &params));
            }
            else
                continue;
            break;
        }

        // Generic sub-device objects
        case GT200_DEBUGGER:
        case GF100_SUBDEVICE_GRAPHICS:
        case GF100_SUBDEVICE_FB:
        case GF100_SUBDEVICE_FLUSH:
        case GF100_SUBDEVICE_LTCG:
        case GF100_SUBDEVICE_TOP:
        case GF100_SUBDEVICE_MASTER:
        case GF100_SUBDEVICE_INFOROM:
        case GK110_SUBDEVICE_GRAPHICS:
        case GK110_SUBDEVICE_FB:
        case GP100_SUBDEVICE_GRAPHICS:
        case GP100_SUBDEVICE_FB:
        case GV100_SUBDEVICE_GRAPHICS:
        case GV100_SUBDEVICE_FB:
        case LW01_TIMER:
        case GF100_ZBC_CLEAR:
        case LW9171_DISP_SF_USER:
        case LW9271_DISP_SF_USER:
        case LW9471_DISP_SF_USER:
        case LW9571_DISP_SF_USER:
        case LWC371_DISP_SF_USER:
        case LWC671_DISP_SF_USER:
        case LWC771_DISP_SF_USER:
        case VOLTA_USERMODE_A:
        case TURING_USERMODE_A:
        case AMPERE_USERMODE_A:
        case HOPPER_USERMODE_A:
            CHECK_RC_CLEANUP(ObjectAllocate(OBJECT_ALLOC_PARENT_SUBDEVICE));
            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);
            break;

        // Generic device objects
        case GF100_HDACODEC:
        case G84_PERFBUFFER:
        case LW9070_DISPLAY:
        case LW9170_DISPLAY:
        case LW9270_DISPLAY:
        case LW9470_DISPLAY:
        case LW9570_DISPLAY:
        case LW9770_DISPLAY:
        case LW9870_DISPLAY:
        case LWC370_DISPLAY:
        case LWC570_DISPLAY:
        case LWC670_DISPLAY:
        case LWC770_DISPLAY:
        case G82_DISPLAY:
        case LW50_DISPLAY:
        case G94_DISPLAY:
        case GT200_DISPLAY:
        case GT214_DISPLAY:
        case LW04_DISPLAY_COMMON:
            CHECK_RC_CLEANUP(ObjectAllocate(OBJECT_ALLOC_PARENT_DEVICE));
            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);
            break;

        // Context Share Object
        case FERMI_CONTEXT_SHARE_A:
        {
            LW_CTXSHARE_ALLOCATION_PARAMETERS ctxshareParams = {0};
            LW0080_CTRL_FIFO_GET_CAPS_PARAMS fifoCapsParams = {0};
            LwU8 *pFifoCaps = NULL;
            pFifoCaps = new LwU8[LW0080_CTRL_FIFO_CAPS_TBL_SIZE];
            if (pFifoCaps == NULL)
            {
                return RC::LWRM_INSUFFICIENT_RESOURCES;
            }
            fifoCapsParams.capsTblSize = LW0080_CTRL_FIFO_CAPS_TBL_SIZE;
            fifoCapsParams.capsTbl = (LwP64) pFifoCaps;

            if (pLwRm->ControlByDevice(GetBoundGpuDevice(), LW0080_CTRL_CMD_FIFO_GET_CAPS,
                                                    &fifoCapsParams, sizeof(fifoCapsParams)))
            {
                return RC::SOFTWARE_ERROR;
            }

            if (LW0080_CTRL_FIFO_GET_CAP(pFifoCaps,
                                LW0080_CTRL_FIFO_CAPS_MULTI_VAS_PER_CHANGRP))
            {
                ctxshareParams.hVASpace = 0;
                ctxshareParams.flags = 0;
                CHECK_RC_CLEANUP(ObjectAllocate(OBJECT_ALLOC_PARENT_CHANGRP, &ctxshareParams));
                Printf(Tee::PriLow,
                    "Object allocation for class 0x%x successful\n",
                    (UINT32)*m_pClass);
                break;
            }
            else
            {
                continue;
            }
        }
        case FERMI_VASPACE_A:
        {
            LW_VASPACE_ALLOCATION_PARAMETERS vaParams = { 0 };
            CHECK_RC_CLEANUP(ObjectAllocate(OBJECT_ALLOC_PARENT_DEVICE, &vaParams));
            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);
            break;
        }

        case TEGRA_VASPACE_A:
        {
            LW_VASPACE_ALLOCATION_PARAMETERS vaParams = { 0 };
            CHECK_RC_CLEANUP(ObjectAllocate(OBJECT_ALLOC_PARENT_DEVICE, &vaParams));
            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);
            break;
        }

        case KEPLER_CHANNEL_GROUP_A:
        {
            LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS chGrpParams = { 0 };
            chGrpParams.engineType = LW2080_ENGINE_TYPE_GR(0);
            CHECK_RC_CLEANUP(ObjectAllocate(OBJECT_ALLOC_PARENT_DEVICE, &chGrpParams));
            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);
            break;
        }

        case LW50_THIRD_PARTY_P2P:
        {
            LW503C_ALLOC_PARAMETERS params={0};
            if (Xp::OS_LINUX == Xp::GetOperatingSystem())
                CHECK_RC_CLEANUP(ObjectAllocate(OBJECT_ALLOC_PARENT_SUBDEVICE, &params));
            else
                continue;
            break;
        }

        case KEPLER_DEVICE_VGPU:
        {
            //
            // This class allocation will only be successful for virtual GPU,
            // so skipping this on sim
            //
            Printf(Tee::PriLow,
                "Skipping allocation for class 0x%x\n",
                (UINT32)*m_pClass);
            continue;
        }

        case LWA081_VGPU_CONFIG:
        {
            //
            // This class allocation will only be successful for virtual GPU,
            // so skipping this on sim
            //
            Printf(Tee::PriLow,
                "Skipping allocation for class 0x%x\n",
                (UINT32)*m_pClass);
            continue;
        }

        case LWA082_HOST_VGPU_DEVICE:
        case LWA084_HOST_VGPU_DEVICE_KERNEL:
        case LW01_MEMORY_LOCAL_DESCRIPTOR:
        {
            //
            // These classes allocation will only be successful for virtual GPU,
            // so skipping this on sim
            //
            Printf(Tee::PriLow,
                "Skipping allocation for class 0x%x\n",
                (UINT32)*m_pClass);
            continue;
        }

        case LW0060_SYNC_GPU_BOOST:
        {
            Printf(Tee::PriLow,
                "Skipping allocation for class 0x%x\n",
                (UINT32)*m_pClass);
            continue;
        }

        case LWA083_GRID_DISPLAYLESS:
        {
            //
            // This class allocation will only be successful for Displayless
            // stack on display-less/connector-less GRID boards.
            // So skipping this on sim
            //
            Printf(Tee::PriLow,
                "Skipping allocation for class 0x%x\n",
                (UINT32)*m_pClass);
            continue;
        }

        case LW01_EVENT:
        {
            Printf(Tee::PriLow,
                "Skipping allocation for class 0x%x\n",
                (UINT32)*m_pClass);
            continue;
        }

        case LW_E2_TWOD:
        {
            LW_GR_ALLOCATION_PARAMETERS params = {0};
            params.size    = sizeof(LW_GR_ALLOCATION_PARAMETERS);
            params.caps    = LWE22D_CLASS_ID;
            params.version = 0x2;
            CHECK_RC_CLEANUP(ObjectAllocate(OBJECT_ALLOC_PARENT_CHANNEL, &params));
            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);
            break;
        }

        case LWE2_AVP:
        {
            CHECK_RC(AllocVP());
            LW_VP_ALLOCATION_PARAMETERS vp_params = {0};
            vp_params.size    = sizeof(LW_VP_ALLOCATION_PARAMETERS);
            vp_params.hMemoryCmdBuffer = vpSurf->GetMemHandle();
            CHECK_RC_CLEANUP(ObjectAllocate(OBJECT_ALLOC_PARENT_SUBDEVICE, &vp_params));
            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);
            MEM_WR32((&((LwE276Control *)(vp_params.pControl))->Idle), 1);
            break;
        }

        case MAXWELL_FAULT_BUFFER_A:
        {
             // Allocate the fault buffer object.
             LWB069_ALLOCATION_PARAMETERS faultBufferAllocParams = {0};
             CHECK_RC_CLEANUP(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                 &m_hObject,
                 MAXWELL_FAULT_BUFFER_A,
                 &faultBufferAllocParams));
             Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);
             break;
        }

        case ACCESS_COUNTER_NOTIFY_BUFFER:
        {
             // Allocate the access counter buffer object.
             LwU32 accessCounterBufferAllocParams = {0};
             CHECK_RC_CLEANUP(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                 &m_hObject,
                 ACCESS_COUNTER_NOTIFY_BUFFER,
                 &accessCounterBufferAllocParams));
             Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);
             break;
        }

        case MMU_FAULT_BUFFER:
        {
             // Allocate the mmu fault buffer object.
             LwU32 mmuFaultBufferAllocParams = {0};
             CHECK_RC_CLEANUP(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                 &m_hObject,
                 MMU_FAULT_BUFFER,
                 &mmuFaultBufferAllocParams));
             Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);
             break;
        }

        case MAXWELL_SEC2:
        case VOLTA_GSP:
        case LW_PHYS_MEM_SUBALLOCATOR:
        {
            Printf(Tee::PriLow,
                "Skipping allocation for class 0x%x\n",
                (UINT32)*m_pClass);
            continue;
        }
        case TURING_VMMU_A:
        case LW_EVENT_BUFFER: // rmt_eventbuffer.cpp tests the allocation path for event buffer
        case LW_EVENT_BUFFER_BIND:
        case LW_CONFIDENTIAL_COMPUTE:
        {
            Printf(Tee::PriLow,
                "Skipping allocation for class 0x%x\n",
                (UINT32)*m_pClass);
            continue;
        }

        // By default we assume channel objects.
       default:
            CHECK_RC_CLEANUP(ObjectAllocate(OBJECT_ALLOC_PARENT_CHANNEL));
            Printf(Tee::PriLow,
                "Object allocation for class 0x%x successful\n",
                (UINT32)*m_pClass);
            break;
        }
        nullCmdrc = pLwRm->Control(m_hObject, 0, 0, 0);

        // The NULL cmd should always succeed
        if( nullCmdrc != OK )
        {
            Printf(Tee::PriLow,
                "Error:%s on Null Command by 0x%x object\n",
                nullCmdrc.Message(), (UINT32)*m_pClass);

            rc = nullCmdrc;
            goto Cleanup;
        }

        Cleaner();
    }

Cleanup:
    Cleaner();

    if(rc != OK)
    {
        Printf(Tee::PriHigh,
            "Object allocation/ NULL CMD for class 0x%x UnSuccessful\n",
            (UINT32)*m_pClass);

        Printf(Tee::PriHigh,
            "Unexpected RC:%s, not a known one\n",rc.Message());

        Printf(Tee::PriHigh,
            "Exiting MODS from AllocObjectTest\n");
    }

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//! Cleanup can be called immediately if Setup fails or Run fails,
//! so cleaning up the allocated channel and the exported class buffer.
//!
//! \sa Setup
//----------------------------------------------------------------------------
RC AllocObjectTest::Cleanup()
{
    if (m_params.classList)
        delete [] m_classBuf;

    return OK;
}

//!---------------------------------------------------------------------------
//! Private Member Functions
//!---------------------------------------------------------------------------
RC AllocObjectTest::AllocVP()
{
    RC rc;
    vpSurf = new Surface2D();
    vpSurf->SetLayout(Surface2D::Pitch);
    vpSurf->SetPitch(ALLOC_PB_SIZE);
    vpSurf->SetColorFormat(ColorUtils::I8);
    vpSurf->SetWidth(ALLOC_PB_SIZE);
    vpSurf->SetHeight(1);
    vpSurf->SetLocation(Memory::Coherent);
    vpSurf->SetPhysContig(true);
    vpSurf->SetKernelMapping(true);
    rc = vpSurf->Alloc(GetBoundGpuDevice(),m_TestConfig.GetRmClient());
    if(rc != OK)
    {
        vpSurf->Free();
        delete vpSurf;
    }
    return rc;
}
RC AllocObjectTest::AllocPmuSubDev()
{
    RC rc;
    LwRmPtr pLwRm;

    // Allocate Subdevice PMU
    rc = pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                              &m_hObject,
                              *m_pClass,
                              NULL);

    if (rc == RC::LWRM_IN_USE)
    {
        rc.Clear();

        PMU *pPmu;

        // GT214 only one allocation is allowed, check whether mods has done it.
        CHECK_RC(GetBoundGpuSubdevice()->GetPmu(&pPmu));

        // PMU is already allocated, let's return that one.
        m_hObject = pPmu->GetHandle();
        m_FreeObject = false;
        Printf(Tee::PriHigh, "Unable to allocate GT212_SUBDEVICE_PMU. Already taken\n");
    }
    return rc;
}

RC AllocObjectTest::AllocP2P()
{
    LwRmPtr pLwRm;
    LW0000_CTRL_GPU_GET_DEVICE_IDS_PARAMS deviceIdsParams={0};
    LW503B_ALLOC_PARAMETERS p2pAllocParams={0};
    RC rc;

    m_pGpuDevMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
    // get valid device instances mask
    CHECK_RC(pLwRm->Control(pLwRm->GetClientHandle(),
                            LW0000_CTRL_CMD_GPU_GET_DEVICE_IDS,
                            &deviceIdsParams, sizeof (deviceIdsParams)));

    // Default to a loopback mapping, assume one gpu.
    p2pAllocParams.hSubDevice     = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    p2pAllocParams.hPeerSubDevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    // Look for a second gpu for a real peer mapping.
    for (GpuSubdevice * pSub = m_pGpuDevMgr->GetFirstGpu();
              pSub != NULL; pSub = m_pGpuDevMgr->GetNextGpu(pSub))
    {
        if (pSub != GetBoundGpuSubdevice())
        {
            p2pAllocParams.hPeerSubDevice = pLwRm->GetSubdeviceHandle(pSub);
            break;
        }
    }
    if (p2pAllocParams.hPeerSubDevice == p2pAllocParams.hSubDevice)
        Printf(Tee::PriLow, "Loopback P2P mapping on gpu 0\n");
    else
        Printf(Tee::PriLow, "P2P mapping between gpus 0 and 1\n");

    rc = pLwRm->Alloc(pLwRm->GetClientHandle(),
                      &m_hObject,
                      *m_pClass,
                      &p2pAllocParams);

    return rc;
}

//! \brief Object allocating function
//!
//! Basing on the class, we segregate the type of allocation
//! of object whether it is associated with the channel or with the device
//! handle. Also for some specific classes, we also will pass some parameters
//! reflected in the last argument. Sometimes it may be NULL and sometimes it
//! may take class specific parameters.
//!
//! return->OK when the allocation succeds else returns error RC
//!
//! \sa Run()
//----------------------------------------------------------------------------
RC AllocObjectTest::ObjectAllocate(OBJECT_ALLOC_PARENT parent, void* pParams)
{
    LwU32 engineType = LW2080_ENGINE_TYPE_GR(0);
    RC rc;
    LwRmPtr pLwRm;
    std::map<LwU32, LwU32>::iterator it;

    //
    // If there is a specific engine type associated with this class, swap
    // engine type to mapped type. If there is no mapping associated, then
    // just keep default LW2080_ENGINE_TYPE_GR(0) type.
    //
    it = m_classToEngMap.find(*m_pClass);
    if (it != m_classToEngMap.end())
    {
        Printf(Tee::PriHigh, "Class 0x%04x will allocate with LW2080_ENGINE_TYPE 0x%02x\n",
               (LwU32) *m_pClass, it->second);
        engineType = it->second;
    }

    switch (parent)
    {
        case OBJECT_ALLOC_PARENT_CHANNEL:
            CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, engineType));
            CHECK_RC_CLEANUP(
                pLwRm->Alloc(m_hCh,
                &m_hObject,
                *m_pClass,
                pParams));
            break;
        case OBJECT_ALLOC_PARENT_DEVICE:
            CHECK_RC_CLEANUP(
                pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                &m_hObject,
                *m_pClass,
                pParams)
                );
            break;
        case OBJECT_ALLOC_PARENT_SUBDEVICE:
            CHECK_RC_CLEANUP(
                pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                &m_hObject,
                *m_pClass,
                pParams)
                );
            break;
        case OBJECT_ALLOC_PARENT_CHANGRP:
            LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS channelGroupParams = {0};
            channelGroupParams.engineType = engineType;
            m_pChGrp = new ChannelGroup(&m_TestConfig, &channelGroupParams);
            CHECK_RC(m_pChGrp->Alloc());
            CHECK_RC_CLEANUP(
                pLwRm->Alloc(m_pChGrp->GetHandle(),
                &m_hObject,
                *m_pClass,
                pParams)
                );
            break;

    }
    return rc;
Cleanup:

    if (m_pChGrp)
    {
        m_pChGrp->Free();
        m_pChGrp = NULL;
    }
    pLwRm->Free(m_hObject);
    m_hObject = 0;
    if (m_pCh)
        m_TestConfig.FreeChannel(m_pCh);

    return rc;
}

//! \brief Core Display channel allocating function
//!
//! Allocating PushBuffer and ErrorNotifier buffer by calling PbErrorAlloc()
//! Then allocating core display channel rspective to LW50 or LW82 classes
//!
//! Allocating Core Channel if the test is going to run on Simulator
//! Otherwise allocating Base Channel as already core channel is existing
//!
//! return->OK when the allocation succeds else returns error RC
//!
//! \sa Run()
//! \sa PbErrorAlloc()
//----------------------------------------------------------------------------
RC AllocObjectTest::AllocCoreDisp(LwRm::Handle *ObjectHandle)
{
    RC rc;
    LwRmPtr pLwRm;

    //Local Variables
    UINT32 ClasArg = 0;
    UINT32 DispChanArg = 0;
    void *pParams;

    pParams = NULL;

    // parameters for allocating Evo object
    LW5070_ALLOCATION_PARAMETERS DispAllocParams={0};
    //Parameters for allocating GT EVO object
    LW8370_ALLOCATION_PARAMETERS GTParams={0};
    //Parameters for allocating GT214 EVO object
    LW8570_ALLOCATION_PARAMETERS GT214Params={0};
    //Parameters for allocating G94 EVO object
    LW8870_ALLOCATION_PARAMETERS G94Params={0};
    // Parameters for allocating LWDISPLAY object
    LWC370_ALLOCATION_PARAMETERS LwDispC3AllocParams = {0};
    LWC570_ALLOCATION_PARAMETERS LwDispC5AllocParams = {0};
#if LWCFG(GLOBAL_ARCH_ADA)
    LWC770_ALLOCATION_PARAMETERS LwDispC7AllocParams = {0};
#else
    LWC670_ALLOCATION_PARAMETERS LwDispC6AllocParams = {0};
#endif

    switch(*m_pClass)
    {
    case LW507A_LWRSOR_CHANNEL_PIO:
    case LW507B_OVERLAY_IMM_CHANNEL_PIO:
    case LW507C_BASE_CHANNEL_DMA:
    case LW507D_CORE_CHANNEL_DMA:
    case LW507E_OVERLAY_CHANNEL_DMA:

        ClasArg = LW50_DISPLAY;
        DispChanArg = (Platform::GetSimulationMode() != Platform::Hardware)?
            LW507D_CORE_CHANNEL_DMA:LW507C_BASE_CHANNEL_DMA;
        pParams = &DispAllocParams;

        break;

    case LW827A_LWRSOR_CHANNEL_PIO:
    case LW827B_OVERLAY_IMM_CHANNEL_PIO:
    case LW827C_BASE_CHANNEL_DMA:
    case LW827D_CORE_CHANNEL_DMA:
    case LW827E_OVERLAY_CHANNEL_DMA:

        ClasArg = GT200_DISPLAY;
        // In sim and platform like dos, core channel won't be allocated, we can use them.
        DispChanArg = (Platform::HasClientSideResman()) ?
            LW837D_CORE_CHANNEL_DMA:LW837C_BASE_CHANNEL_DMA;
        pParams = &GTParams;

        break;

    case LW857A_LWRSOR_CHANNEL_PIO:
    case LW857B_OVERLAY_IMM_CHANNEL_PIO:
    case LW857C_BASE_CHANNEL_DMA:
    case LW857D_CORE_CHANNEL_DMA:
    case LW857E_OVERLAY_CHANNEL_DMA:

        ClasArg = GT214_DISPLAY;
        DispChanArg = (Platform::HasClientSideResman()) ?
            LW857D_CORE_CHANNEL_DMA:LW857C_BASE_CHANNEL_DMA;
        pParams = &GT214Params;

       break;

    case LW837C_BASE_CHANNEL_DMA:
    case LW837D_CORE_CHANNEL_DMA:
    case LW837E_OVERLAY_CHANNEL_DMA:
    case LW887D_CORE_CHANNEL_DMA:

        {
            ClasArg = G94_DISPLAY;
            DispChanArg = (Platform::HasClientSideResman()) ?
                LW887D_CORE_CHANNEL_DMA:LW837C_BASE_CHANNEL_DMA;
            pParams = &G94Params;
        }

        break;

    case LWC37B_WINDOW_IMM_CHANNEL_DMA:
    case LWC37D_CORE_CHANNEL_DMA:
    case LWC37E_WINDOW_CHANNEL_DMA:
        ClasArg = LWC370_DISPLAY;
        DispChanArg = LWC37D_CORE_CHANNEL_DMA;
        pParams = &LwDispC3AllocParams;
        break;

    case LWC57B_WINDOW_IMM_CHANNEL_DMA:
    case LWC57D_CORE_CHANNEL_DMA:
    case LWC57E_WINDOW_CHANNEL_DMA:
        ClasArg = LWC570_DISPLAY;
        DispChanArg = LWC57D_CORE_CHANNEL_DMA;
        pParams = &LwDispC5AllocParams;
        break;

    case LWC67B_WINDOW_IMM_CHANNEL_DMA:
    case LWC67D_CORE_CHANNEL_DMA:
    case LWC67E_WINDOW_CHANNEL_DMA:
    case LWC77D_CORE_CHANNEL_DMA:
#if LWCFG(GLOBAL_ARCH_ADA)
        ClasArg = LWC770_DISPLAY;
        pParams = &LwDispC7AllocParams;
        DispChanArg = LWC77D_CORE_CHANNEL_DMA;
#else
        ClasArg = LWC670_DISPLAY;
        pParams = &LwDispC6AllocParams;
        DispChanArg = LWC67D_CORE_CHANNEL_DMA;
#endif
        break;
    }

    CHECK_RC_CLEANUP(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
            &m_hDisplay,
            ClasArg,
            pParams)
            );
    //
    // Allocate PB and errornotifier buffers
    //
    CHECK_RC_CLEANUP(PbErrorAlloc());
    //
    // Allocate display core/base channel
    //
    CHECK_RC_CLEANUP(pLwRm->AllocDisplayChannelDma(m_hDisplay,
        ObjectHandle,
        DispChanArg,
        0,
        m_hErrorNotifierCtxDma,
        m_ErrorNotifierBase,
        m_hPBCtxDma,
        m_PushBufferBase,
        CL507D_PB_SIZE,
        0,
        0,
        GetBoundGpuDevice())
        );

    return rc;

Cleanup:
    pLwRm->Free(m_hErrorNotifierCtxDma);
    m_hErrorNotifierCtxDma = 0;

    pLwRm->Free(m_hPBCtxDma);
    m_hPBCtxDma = 0;

    pLwRm->UnmapMemory(m_hErrorNotifierMem, m_ErrorNotifierBase, 0, GetBoundGpuSubdevice());
    m_ErrorNotifierBase = NULL;

    if (m_isPbMap)
    {
        pLwRm->UnmapMemory(m_hPBMem, m_PushBufferBase, 0, GetBoundGpuSubdevice());
        m_PushBufferBase = NULL;
        m_isPbMap = false;
    }

    pLwRm->Free(m_hErrorNotifierMem);
    m_hErrorNotifierMem = 0;

    pLwRm->Free(m_hPBMem);
    m_hPBMem = 0;

    pLwRm->Free(*ObjectHandle);
    m_hObject = 0;

    pLwRm->Free(m_hDisplay);
    m_hDisplay=0;

    return rc;
}

//! \brief PushBuffer and ErrorNotifier allocating function
//!
//! Allocating PushBuffer and ErrorNotifier buffer and associate context Dma
//! handles to them
//!
//! return->OK when the allocation succeds else returns error RC
//!
//! \sa Run()
//! \sa AllocCoreDisp()
//----------------------------------------------------------------------------
RC AllocObjectTest::PbErrorAlloc()
{
    RC rc;
    LwRmPtr pLwRm;
    UINT32 attr;
    CHECK_RC_CLEANUP(
        pLwRm->AllocSystemMemory (&m_hPBMem,
        DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI)|
        DRF_DEF(OS02, _FLAGS, _COHERENCY, _WRITE_COMBINE),
        ALLOC_PB_SIZE,
        GetBoundGpuDevice()));

    // Map Push Buffer Memory
    CHECK_RC_CLEANUP(
        pLwRm->MapMemory (m_hPBMem,
        0,
        ALLOC_PB_SIZE,
        &m_PushBufferBase,
        0,
        GetBoundGpuSubdevice()));
    m_isPbMap = true;

    // Allocate CTX DMA for Push Buf
    CHECK_RC_CLEANUP(
        pLwRm->AllocContextDma (&m_hPBCtxDma,
        m_PBCtxDmaFlags,
        m_hPBMem,
        0,
        ALLOC_PB_SIZE - 1));

    // END :About PUSH BUffer

    //
    // START :About Error Notifier
    // allocate memory in FB for error notifier
    //
    if (Memory::Fb == Surface2D::GetActualLocation(Memory::Fb, GetBoundGpuSubdevice()))
    {
        attr = LWOS32_ATTR_NONE;
    }
    else
    {
        attr  = DRF_DEF(OS32, _ATTR, _LOCATION, _PCI);
    }
    CHECK_RC_CLEANUP(
        pLwRm->VidHeapAllocSize (LWOS32_TYPE_IMAGE,
        attr,
        LWOS32_ATTR2_NONE,
        ALLOC_ERR_NOT_SIZE,
        &m_hErrorNotifierMem,
        &m_ErrorNotifierOffset,
        nullptr,
        nullptr,
        nullptr,
        GetBoundGpuDevice()));

    // Map Memory for Error Notifier
    CHECK_RC_CLEANUP(
        pLwRm->MapMemory (m_hErrorNotifierMem,
        0,
        ALLOC_ERR_NOT_SIZE,
        &m_ErrorNotifierBase,
        0,
        GetBoundGpuSubdevice()));

    // Allocate CTX DMA for Error Notifier
    CHECK_RC_CLEANUP(
        pLwRm->AllocContextDma (&m_hErrorNotifierCtxDma,
        m_ErrNotifierFlags,
        m_hErrorNotifierMem,
        0,
        ALLOC_ERR_NOT_SIZE - 1));

    return rc;

Cleanup:

    pLwRm->Free(m_hErrorNotifierCtxDma);
    m_hErrorNotifierCtxDma = 0;

    pLwRm->Free(m_hPBCtxDma);
    m_hPBCtxDma = 0;

    pLwRm->UnmapMemory(m_hErrorNotifierMem, m_ErrorNotifierBase, 0, GetBoundGpuSubdevice());
    m_ErrorNotifierBase = NULL;

    pLwRm->Free(m_hErrorNotifierMem);
    m_hErrorNotifierMem = 0;

    if (m_isPbMap)
    {
        pLwRm->UnmapMemory(m_hPBMem, m_PushBufferBase, 0, GetBoundGpuSubdevice());
        m_PushBufferBase = NULL;
        m_isPbMap = false;
    }

    pLwRm->Free(m_hPBMem);
    m_hPBMem = 0;

    return rc;
}

//! \brief Display DMA control channel allocating test function
//!
//! First allocate an EVO superclass, and then allocate a DMA control object
//! for the overlay channel.
//!
//! return->OK when the allocation succeds else returns error RC
//----------------------------------------------------------------------------
RC AllocObjectTest::AllocDispDma(LwRm::Handle *ObjectHandle)
{
    RC rc;
    LwRmPtr pLwRm;

    //Local Variables
    UINT32 DispSuperclass = 0;
    UINT32 DispChanClass = 0;
    void *pParams = NULL;

    // parameters for allocating LW50 EVO object
    LW5070_ALLOCATION_PARAMETERS LW50Params={0};
    //Parameters for allocating GT200 EVO object
    LW8370_ALLOCATION_PARAMETERS GT200Params={0};
    //Parameters for allocating GT214 EVO object
    LW8570_ALLOCATION_PARAMETERS GT214Params={0};
    //Parameters for allocating G82 EVO object
    LW8270_ALLOCATION_PARAMETERS G82Params={0};
    //Parameters for allocating G94 EVO object
    LW8870_ALLOCATION_PARAMETERS G94Params={0};

    LwU32 *pClass = (LwU32 *) LwP64_VALUE(m_params.classList);

    for (UINT32 i = 0; i < m_params.numClasses; i++)
    {
        UINT32 lwrClass = (UINT32) pClass[i];
        switch (lwrClass)
        {
            // EVO Superclasses
            case LW50_DISPLAY:
                pParams = &LW50Params;
                DispSuperclass = lwrClass;
                break;
            case G82_DISPLAY:
                pParams = &G82Params;
                DispSuperclass = lwrClass;
                break;
            case G94_DISPLAY:
                pParams = &G94Params;
                DispSuperclass = lwrClass;
                break;
            case GT200_DISPLAY:
                pParams = &GT200Params;
                DispSuperclass = lwrClass;
                break;
            case GT214_DISPLAY:
                pParams = &GT214Params;
                DispSuperclass = lwrClass;
                break;

            // Overlay classes
            case LW507E_OVERLAY_CHANNEL_DMA:
            case LW827E_OVERLAY_CHANNEL_DMA:
            case LW837E_OVERLAY_CHANNEL_DMA:
            case LW857E_OVERLAY_CHANNEL_DMA:
                DispChanClass = lwrClass;
                break;

            default:
                continue;
        }
        if (DispSuperclass && DispChanClass)
        {
            break;
        }
    }

    if (!DispSuperclass || !DispChanClass)
    {
        Printf(Tee::PriHigh, "Missing EVO superclass or overlay channel. "
               "DispSuperclass = 0x%x, DispChanClass = 0x%x\n",
               DispSuperclass, DispChanClass);
        rc = RC::LWRM_ILWALID_CLASS;
        return rc;
    }

    // Allocate EVO superclass
    CHECK_RC_CLEANUP(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
        &m_hDisplay,
        DispSuperclass,
        pParams)
        );

    return rc;

Cleanup:
    pLwRm->Free(*ObjectHandle);
    *ObjectHandle = 0;

    pLwRm->Free(m_hDisplay);
    m_hDisplay = 0;

    return rc;
}

//! \brief Free any resources that this test selwred during Run()
//!
//! It frees the allocated resources that the test selwred during run.
//! Cleaner() can be called immediately if allocation of resources/object
//! fails.
//!
//! \sa Setup
//----------------------------------------------------------------------------
void AllocObjectTest::Cleaner()
{
    LwRmPtr pLwRm;

    pLwRm->Free(m_hErrorNotifierCtxDma);
    m_hErrorNotifierCtxDma = 0;

    pLwRm->Free(m_hPBCtxDma);
    m_hPBCtxDma = 0;

    pLwRm->Free(m_hGpCtxDma);
    m_hGpCtxDma = 0;

    pLwRm->UnmapMemory(m_hErrorNotifierMem, m_ErrorNotifierBase, 0, GetBoundGpuSubdevice());
    m_ErrorNotifierBase = NULL;

    pLwRm->Free(m_hErrorNotifierMem);
    m_hErrorNotifierMem = 0;

    if (m_isPbMap)
    {
        pLwRm->UnmapMemory(m_hPBMem, m_PushBufferBase, 0, GetBoundGpuSubdevice());
        m_PushBufferBase = NULL;
        m_isPbMap = false;
    }

    if (m_FreeObject)
        pLwRm->Free(m_hObject);
    else
        m_FreeObject = true;
    m_hObject = 0;

    pLwRm->Free(m_hObjectCore);
    m_hObjectCore = 0;

    pLwRm->Free(m_hPBMem);
    m_hPBMem = 0;

    pLwRm->Free(m_hDisplay);
    m_hDisplay=0;

    pLwRm->UnmapMemory(m_hGpMem,m_gpAddress,0, GetBoundGpuSubdevice());
    m_gpAddress = NULL;

    pLwRm->Free(m_hGpMem);
    m_hGpMem=0;

    if (m_pChGrp)
    {
        m_pChGrp->Free();
        m_pChGrp = NULL;
    }

    if (vpSurf)
    {
        vpSurf->Free();
        delete vpSurf;
    }
    vpSurf = NULL;

    if (m_pCh)
        m_TestConfig.FreeChannel(m_pCh);
    m_pCh = NULL;
}

//----------------------------------------------------------------------------
// JS Linkage
//----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ AllocObjectTest
//! object.
//!
//----------------------------------------------------------------------------
JS_CLASS_INHERIT(AllocObjectTest, RmTest,
                 "Gpu Object Allocation test.");
