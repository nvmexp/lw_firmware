/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009,2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "kepler_bl_remapper.h"
#include "class/cl906f.h"
#include "class/cla0b5.h" // KEPLER_DMA_COPY_A
#include "gpu/include/gpusbdev.h"
#include <string>
#include <sstream>

#define T_NAME "bl_remapperTest"

////////////////////////////////////////////////////////////////////////////////
// KeplerBlockLinearRemapperTestBase Implementation
////////////////////////////////////////////////////////////////////////////////

//! Constructor
KeplerBlockLinearRemapperTest::KeplerBlockLinearRemapperTest(bl_remapperTest& parent,
  ArgReader *params):
    BlockLinearRemapperTest(parent, params),
    pRegRemap1(NULL),
    pRegRemap2(NULL),
    pRegRemap4(NULL),
    pRegRemap6(NULL),
    pRegRemap7(NULL)
{
    copyClassToUse = parent.getClassId();
    // Width of a GOB in bytes
    GOB_WIDTH_BYTES = 64;
    // Height of a GOB in lines
    GOB_HEIGHT_IN_LINES = 8;
    // Total number of bytes in a GOB
    GOB_BYTES = GOB_WIDTH_BYTES * GOB_HEIGHT_IN_LINES;
}

void KeplerBlockLinearRemapperTest::SetupDev(void){
    // Get some default register values based on Current GPU
    // Set the default PTE kind for pitch surfaces on fermi to LW_MMU_PTE_KIND_PITCH
    GpuSubdevice * pGpuSubdev = m_testObject.getlwgpu()->GetGpuSubdevice();
    MASSERT(pGpuSubdev != NULL);
    if(!pGpuSubdev->GetPteKindFromName("PITCH", &pitchSurfacePteKind))
    {
        ErrPrintf(T_NAME"::Unable to set pitch surface PTE Kind.\n");
        pitchSurfacePteKind = 0;
    }

    DebugPrintf("Pitch surface kind: 0x%x.\n", pitchSurfacePteKind);

    // Set the default PTE kind for block surfaces on fermi to LW_MMU_PTE_KIND_GENERIC_16BX2
    if(!pGpuSubdev->GetPteKindFromName("GENERIC_16BX2", &blockSurfacePteKind))
    {
        ErrPrintf(T_NAME"::Unable to set block surface PTE Kind.\n");
        blockSurfacePteKind = 0;
    }
    DebugPrintf("Block surface kind: 0x%x.\n", blockSurfacePteKind);

    // Get pointers to all the bl_remapper registers.
    RefManual *pRefMan = pGpuSubdev->GetRefManual();
    MASSERT(pRefMan != NULL);
    pRegRemap1 = pRefMan->FindRegister("LW_PBUS_BL_REMAP_1");
    MASSERT(pRegRemap1 != NULL);
    pRegRemap2 = pRefMan->FindRegister("LW_PBUS_BL_REMAP_2");
    MASSERT(pRegRemap2 != NULL);
    pRegRemap4 = pRefMan->FindRegister("LW_PBUS_BL_REMAP_4");
    MASSERT(pRegRemap4 != NULL);
    pRegRemap6 = pRefMan->FindRegister("LW_PBUS_BL_REMAP_6");
    MASSERT(pRegRemap6 != NULL);
    pRegRemap7 = pRefMan->FindRegister("LW_PBUS_BL_REMAP_7");
    MASSERT(pRegRemap7 != NULL);
}

//! Set's up one host block linear remapper register
//!
//! \param remapper Remapper to setup, Fermi has 0-7
//! \param baseBuf MdiagSurf pointing at the pitch surface allocated in vidmem
//! \param blockBaseBuf MdiagSurf buffer for the block surface allocated in vidmem
//! \param baseOffset Pitch surface location for remapper is base of baseBuf + baseOffset
//! \param blockBaseOffset Block surface location for remapper is base address of blockBaseBuf + blockBaseOffset
int KeplerBlockLinearRemapperTest::SetupHostBLRemapper(int remapper, MdiagSurf &baseBuf,
                                                          MdiagSurf &blockBaseBuf, uint baseOffset,
                                                          uint blockBaseOffset)
{

    // Overridding global patch up if this works
    UINT32 blockHeightGobs = imgLogBlockHeight[remapper];
    UINT32 blockDepthGobs = imgLogBlockDepth[remapper];
    UINT32 totalSurfaceGobs = imgTotalGobs[remapper];

    DebugPrintf(T_NAME"::SetupHostBLRemapper() - _BL_REMAP_2_SIZE: %d,"
                " _BL_REMAP_2_BLOCK_HEIGHTT: %d, _BL_REMAP_2_BLOCK_DEPTH: %d\n",
                totalSurfaceGobs, blockHeightGobs, blockDepthGobs );
    UINT32 writeVal = 0;
    writeVal |= totalSurfaceGobs << (pRegRemap2->FindField("LW_PBUS_BL_REMAP_2_SIZE")->GetLowBit());
    writeVal |= blockHeightGobs << (pRegRemap2->FindField("LW_PBUS_BL_REMAP_2_BLOCK_HEIGHT")->GetLowBit());
    writeVal |= blockDepthGobs << (pRegRemap2->FindField("LW_PBUS_BL_REMAP_2_BLOCK_DEPTH")->GetLowBit());

    m_testObject.getlwgpu()->RegWr32(pRegRemap2->GetOffset(remapper), writeVal);

    UINT32 blockBase4 = ((blockBaseBuf.GetPhysAddress() - m_testObject.getlwgpu()->PhysFbBase() + blockBaseOffset) >> 9);
    DebugPrintf(T_NAME"::SetupHostBLRemapper() - _BL_REMAP_4_BLOCK_BASE: 0x%08x\n",
                blockBase4);

    writeVal = blockBase4 << (pRegRemap4->FindField("LW_PBUS_BL_REMAP_4_BLOCK_BASE")->GetLowBit());
    m_testObject.getlwgpu()->RegWr32(pRegRemap4->GetOffset(remapper), writeVal );

    DebugPrintf(T_NAME"::SetupHostBLRemapper() - _BL_REMAP_6_IMAGE_HEIGHT: %d\n",
                imgHeight[remapper]/GOB_HEIGHT_IN_LINES);

    writeVal = imgHeight[remapper]/GOB_HEIGHT_IN_LINES << (pRegRemap6->FindField("LW_PBUS_BL_REMAP_6_IMAGE_HEIGHT")->GetLowBit());
    m_testObject.getlwgpu()->RegWr32(pRegRemap6->GetOffset(remapper), writeVal);

    DebugPrintf(T_NAME"::SetupHostBLRemapper() - _BL_REMAP_7_PITCH: %d, "
                "_BL_REMAP_7_NUM_BYTES_PIXEL: %d\n", imgPitch[remapper]/GOB_WIDTH_BYTES,
                remapNumBytesEnum[remapper]);

    writeVal  = imgPitch[remapper]/GOB_WIDTH_BYTES << (pRegRemap7->FindField("LW_PBUS_BL_REMAP_7_PITCH")->GetLowBit());
    writeVal |= remapNumBytesEnum[remapper] << (pRegRemap7->FindField("LW_PBUS_BL_REMAP_7_NUM_BYTES_PIXEL")->GetLowBit());
    m_testObject.getlwgpu()->RegWr32(pRegRemap7->GetOffset(remapper), writeVal);

    // Enable the Remapper, and set the base address
    UINT32 remap_base1 = ((baseBuf.GetPhysAddress() - m_testObject.getlwgpu()->PhysFbBase() + baseOffset) >> 9);
    DebugPrintf(T_NAME"::SetupHostBLRemapper() - _BL_REMAP_1_BASE: 0x%08x\n",
                remap_base1);
    writeVal  = remap_base1 << (pRegRemap1->FindField("LW_PBUS_BL_REMAP_1_BASE")->GetLowBit());
    writeVal |= pRegRemap1->FindField("LW_PBUS_BL_REMAP_1_BL_ENABLE")->FindValue("LW_PBUS_BL_REMAP_1_BL_ENABLE_ON")->Value
                    << (pRegRemap1->FindField("LW_PBUS_BL_REMAP_1_BL_ENABLE")->GetLowBit());
    m_testObject.getlwgpu()->RegWr32(pRegRemap1->GetOffset(remapper), writeVal);

    return 0;
}

//! Completely disables the passed in remapper
//!
//! \param remapper The remapper to disable gf100 has 0-7
void KeplerBlockLinearRemapperTest::DisableRemapper(int remapper)
{
    // Enable the Remapper, and set the base address
    UINT32 remap_base1 = 0;
    DebugPrintf(T_NAME"::DisableRemapper() - disabling remapper %d\n",
                remapper);
    UINT32 writeVal  = remap_base1 << (pRegRemap1->FindField("LW_PBUS_BL_REMAP_1_BASE")->GetLowBit());
    writeVal |= pRegRemap1->FindField("LW_PBUS_BL_REMAP_1_BL_ENABLE")->FindValue("LW_PBUS_BL_REMAP_1_BL_ENABLE_OFF")->Value
                    << (pRegRemap1->FindField("LW_PBUS_BL_REMAP_1_BL_ENABLE")->GetLowBit());

    m_testObject.getlwgpu()->RegWr32(pRegRemap1->GetOffset(remapper), writeVal);
}

//! Does a Block2Pitch KEPLER_DMA_COPY_A dma transfer.
//!
//! \param srcBuffer reference to source MdiagSurf.
//! \param dstBuffer reference to destination MdiagSurf.
//! \param semaphore reference to semaphore MdiagSurf
//! \return 1 on success0.
int KeplerBlockLinearRemapperTest::DoBlock2PitchDma
(
    MdiagSurf &srcBuffer,
    MdiagSurf &dstBuffer,
    MdiagSurf &semaphore,
    UINT32 remapper
)
{

    int status = 1;
    // Used in to store method data for incrementing methods
    UINT32 methodData[5];
    UINT64 offset;

    // Let the Surface get block sizes
    UINT32 m2mBlockSizeWidth = imgLogBlockWidth[remapper];
    UINT32 m2mBlockSizeHeight = imgLogBlockHeight[remapper];
    UINT32 m2mBlockSizeDepth = imgLogBlockDepth[remapper];

    // Calc number of z copies of surface
    UINT32 twoDSize = imgPitch[remapper]*imgHeight[remapper];
    UINT32 maxZ = (copySize+copyOffset[remapper])/(twoDSize) + ((copySize+copyOffset[remapper])%twoDSize ? 0 : -1);
    UINT32 startZ = copyOffset[remapper]/twoDSize;
    UINT32 totalLinesToCopy = copySize/imgPitch[remapper] + (copySize%imgPitch[remapper]?1:0);
    UINT32 x, y, z, numLines;

    InfoPrintf(T_NAME"::DoBlock2PitchDma() - maxZ: %d, startZ: %d, totalLinesToCopy: %d.\n", maxZ, startZ, totalLinesToCopy);
    for(UINT32 i = startZ ; i <= maxZ ; ++i)
    {
        // Need to callwlate the x/y/z coord given an offset
        // offset = x + pitch*y + pitch*height*z
        z = i;
        if(i == startZ)
        {
            y = (copyOffset[remapper] - z*twoDSize)/imgPitch[remapper];
            x = copyOffset[remapper] - y*imgPitch[remapper] - z*twoDSize;

            if( (totalLinesToCopy > imgHeight[remapper]) ||
                ((totalLinesToCopy + copyOffset[remapper]/imgPitch[remapper]) > imgHeight[remapper]) )
            {
                numLines = imgHeight[remapper] - y;
            }
            else
            {
                numLines = totalLinesToCopy;
            }
        }
        else
        {
            y = 0;
            x = 0;

            if( totalLinesToCopy > imgHeight[remapper])
            {
                numLines = imgHeight[remapper] - y;
            }
            else
            {
                numLines = totalLinesToCopy;
            }
        }

        totalLinesToCopy -= numLines;
        MASSERT(((imgPitch[remapper] - x*imgBpp[remapper]) <= imgPitch[remapper]) && "Bad x callwlation");

        InfoPrintf(T_NAME"::DoBlock2PitchDma() - Remapper: %d offset: 0x%x x: %d y: %d z: %d twoDSize: %d numLines:%d.\n",
                    remapper, copyOffset[remapper], x, y, z, twoDSize, numLines);

        // Acquire a semaphore to make sure that cpu write has completed
        offset = semaphore.GetCtxDmaOffsetGpu();
        methodData[0] = static_cast<UINT32>(offset >> 32);
        methodData[1] = static_cast<UINT32>(offset);
        methodData[2] = CPU_SEMAPHORE_VALUE;
        status = status && m_testObject.getCh()->MethodWriteN(subChannel, LW906F_SEMAPHOREA, 3, methodData, INCREMENTING);

        status = status && m_testObject.getCh()->MethodWrite(subChannel, LW906F_SEMAPHORED,
                        DRF_NUM(906F, _SEMAPHORED, _OPERATION, LW906F_SEMAPHORED_OPERATION_ACQUIRE) |
                        DRF_NUM(906F, _SEMAPHORED, _ACQUIRE_SWITCH, LW906F_SEMAPHORED_ACQUIRE_SWITCH_DISABLED));

        // Set up the src offset
        offset = srcBuffer.GetCtxDmaOffsetGpu();
        methodData[0] = static_cast<UINT32>(offset >> 32);
        methodData[1] = static_cast<UINT32>(offset);
        status = status && m_testObject.getCh()->MethodWriteN(subChannel, LWA0B5_OFFSET_IN_UPPER,2,methodData, INCREMENTING);

        // Set up block linear source parameters
        // BlockSize, Width, Height, Depth, Layer
        //DebugPrintf(T_NAME"::DoBlock2PitchDma() - \n");

        methodData[0] = DRF_NUM(A0B5, _SET_SRC_BLOCK_SIZE, _WIDTH, m2mBlockSizeWidth) |
                        DRF_NUM(A0B5, _SET_SRC_BLOCK_SIZE, _HEIGHT, m2mBlockSizeHeight) |
                        DRF_NUM(A0B5, _SET_SRC_BLOCK_SIZE, _DEPTH, m2mBlockSizeDepth) |
                        DRF_NUM(A0B5, _SET_SRC_BLOCK_SIZE, _GOB_HEIGHT, LWA0B5_SET_DST_BLOCK_SIZE_GOB_HEIGHT_GOB_HEIGHT_FERMI_8);
        methodData[1] = imgPitch[remapper]; // Width in bytes of source
        methodData[2] = imgHeight[remapper]; // Height in Pixels of source
        methodData[3] = imgDepth[remapper]; // Depth i Pixels of source
        status = status && m_testObject.getCh()->MethodWriteN(subChannel, LWA0B5_SET_SRC_BLOCK_SIZE, 4, methodData, INCREMENTING);

        // Sets the layer (z coord) of the source surface if 3d surface
        status = status && m_testObject.getCh()->MethodWrite(subChannel, LWA0B5_SET_SRC_LAYER, z);

        // Set up Origin X bytes and Y samples
        // X is in pixels, y is in pixels
        methodData[0] = DRF_NUM(A0B5, _SET_SRC_ORIGIN, _X, x) |     // First pixel in x
                        DRF_NUM(A0B5, _SET_SRC_ORIGIN, _Y, y);      // First pixel in y
        status = status && m_testObject.getCh()->MethodWrite(subChannel, LWA0B5_SET_SRC_ORIGIN, methodData[0]);

        // Set line length in and number of lines
        UINT32 lineLength = imgPitch[remapper] - x*imgBpp[remapper];
        status = status && m_testObject.getCh()->MethodWrite(subChannel, LWA0B5_LINE_LENGTH_IN, lineLength);
        status = status && m_testObject.getCh()->MethodWrite(subChannel, LWA0B5_LINE_COUNT,  numLines);

        // Set up the dst offset (pitch)
        offset = dstBuffer.GetCtxDmaOffsetGpu();
        offset += (z==startZ) ? copyOffset[remapper] : twoDSize*z;
        methodData[0] = static_cast<UINT32>(offset >> 32);
        methodData[1] = static_cast<UINT32>(offset);
        status = status && m_testObject.getCh()->MethodWriteN(subChannel, LWA0B5_OFFSET_OUT_UPPER,2,methodData, INCREMENTING);

        // Set up the Pitch Out
        status = status && m_testObject.getCh()->MethodWrite(subChannel, LWA0B5_PITCH_OUT, imgPitch[remapper]);

        // Set up the semaphore to signal DMA complete
        offset = semaphore.GetCtxDmaOffsetGpu();
        methodData[0] = (UINT32)(offset >> 32);
        methodData[1] = (UINT32)(offset);
        methodData[2] = DMA_SEMAPHORE_RELEASE_VALUE;
        status = status && m_testObject.getCh()->MethodWriteN(subChannel, LWA0B5_SET_SEMAPHORE_A,3,methodData, INCREMENTING);

        // start the DMA of src surface to dst surface
        status = status &&
        m_testObject.getCh()->MethodWrite(subChannel, LWA0B5_LAUNCH_DMA,
            DRF_NUM(A0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE,     LWA0B5_LAUNCH_DMA_DATA_TRANSFER_TYPE_NON_PIPELINED) |
            DRF_NUM(A0B5, _LAUNCH_DMA, _FLUSH_ENABLE,           LWA0B5_LAUNCH_DMA_FLUSH_ENABLE_TRUE) |
            DRF_NUM(A0B5, _LAUNCH_DMA, _SEMAPHORE_TYPE,         (z == maxZ) ? LWA0B5_LAUNCH_DMA_SEMAPHORE_TYPE_RELEASE_ONE_WORD_SEMAPHORE : LWA0B5_LAUNCH_DMA_SEMAPHORE_TYPE_NONE) |
            DRF_NUM(A0B5, _LAUNCH_DMA, _INTERRUPT_TYPE,         LWA0B5_LAUNCH_DMA_INTERRUPT_TYPE_NONE) |
            DRF_NUM(A0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT,      LWA0B5_LAUNCH_DMA_SRC_MEMORY_LAYOUT_BLOCKLINEAR) |
            DRF_NUM(A0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT,      LWA0B5_LAUNCH_DMA_DST_MEMORY_LAYOUT_PITCH) |
            DRF_NUM(A0B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE,      LWA0B5_LAUNCH_DMA_MULTI_LINE_ENABLE_TRUE) |
            DRF_NUM(A0B5, _LAUNCH_DMA, _REMAP_ENABLE,           LWA0B5_LAUNCH_DMA_REMAP_ENABLE_FALSE) |
            DRF_NUM(A0B5, _LAUNCH_DMA, _BYPASS_L2,              LWA0B5_LAUNCH_DMA_BYPASS_L2_USE_PTE_SETTING) |
            DRF_NUM(A0B5, _LAUNCH_DMA, _SRC_TYPE,               LWA0B5_LAUNCH_DMA_SRC_TYPE_VIRTUAL) |
            DRF_NUM(A0B5, _LAUNCH_DMA, _DST_TYPE,               LWA0B5_LAUNCH_DMA_DST_TYPE_VIRTUAL) |
            DRF_NUM(A0B5, _LAUNCH_DMA, _SEMAPHORE_REDUCTION_ENABLE,    LWA0B5_LAUNCH_DMA_SEMAPHORE_REDUCTION_ENABLE_FALSE) |
            DRF_NUM(A0B5, _LAUNCH_DMA, _SEMAPHORE_REDUCTION_SIGN,   LWA0B5_LAUNCH_DMA_SEMAPHORE_REDUCTION_SIGN_SIGNED));
        status = status && m_testObject.getCh()->MethodFlush();
    } // end for

    return status;
}

//! Does a Pitch2Block memory_to_memory_format_a dma transfer.
//!
//! \param srcBuffer reference to source MdiagSurf.
//! \param dstBuffer reference to destination MdiagSurf.
//! \param semaphore reference to semaphore MdiagSurf
//! \return 1 on success 0 on some terrible error.
int KeplerBlockLinearRemapperTest::DoPitch2BlockDma
(
    MdiagSurf &srcBuffer,
    MdiagSurf &dstBuffer,
    MdiagSurf &semaphore,
    UINT32 remapper
)
{

    int status = 1;
    // Used in to store method data for incrementing methods
    UINT32 methodData[5];
    UINT64 offset;

    // Let the Surface get block sizes
    UINT32 m2mBlockSizeWidth = imgLogBlockWidth[remapper];
    UINT32 m2mBlockSizeHeight = imgLogBlockHeight[remapper];
    UINT32 m2mBlockSizeDepth = imgLogBlockDepth[remapper];

    // Need to callwlate the x/y/z coord given an offset
    // Calc number of z copies of surface
    UINT32 twoDSize = imgPitch[remapper]*imgHeight[remapper];
    UINT32 maxZ = (copySize+copyOffset[remapper])/(twoDSize) + ((copySize+copyOffset[remapper])%twoDSize ? 0 : -1);
    UINT32 startZ = copyOffset[remapper]/twoDSize;
    UINT32 totalLinesToCopy = copySize/imgPitch[remapper] + (copySize%imgPitch[remapper]?1:0);
    UINT32 x, y, z, numLines;

    DebugPrintf(T_NAME"::DoPitch2BlockDma() - maxZ: %d, startZ: %d, totalLinesToCopy: %d.\n", maxZ, startZ, totalLinesToCopy);
    for(UINT32 i = startZ ; i <= maxZ ; ++i)
    {

    // Need to callwlate the x/y/z coord given an offset
    // offset = x + pitch*y + pitch*height*z
    z = i;
    if(i == startZ)
    {
        y = (copyOffset[remapper] - z*twoDSize)/imgPitch[remapper];
        x = copyOffset[remapper] - y*imgPitch[remapper] - z*twoDSize;

        if( (totalLinesToCopy > imgHeight[remapper]) ||
            ((totalLinesToCopy + copyOffset[remapper]/imgPitch[remapper]) > imgHeight[remapper]) )
        {
            numLines = imgHeight[remapper] - y;
        }
        else
        {
            numLines = totalLinesToCopy;
        }
    }
    else
    {
        y = 0;
        x = 0;

        if( totalLinesToCopy > imgHeight[remapper])
        {
            numLines = imgHeight[remapper] - y;
        }
        else
        {
            numLines = totalLinesToCopy;
        }
    }

    totalLinesToCopy -= numLines;

    InfoPrintf(T_NAME"::DoPitch2BlockDma() - offset: 0x%x x: %d y: %d z: %d numLines:%d.\n",
                copyOffset[remapper], x, y, z, numLines);

    // Acquire a semaphore to make sure that cpu write has completed
    offset = semaphore.GetCtxDmaOffsetGpu();
    methodData[0] = static_cast<UINT32>(offset >> 32);
    methodData[1] = static_cast<UINT32>(offset);
    methodData[2] = CPU_SEMAPHORE_VALUE;
    status = status && m_testObject.getCh()->MethodWriteN(subChannel, LW906F_SEMAPHOREA, 3, methodData, INCREMENTING);

    status = status && m_testObject.getCh()->MethodWrite(subChannel, LW906F_SEMAPHORED,
                    DRF_NUM(906F, _SEMAPHORED, _OPERATION, LW906F_SEMAPHORED_OPERATION_ACQUIRE) |
                    DRF_NUM(906F, _SEMAPHORED, _ACQUIRE_SWITCH, LW906F_SEMAPHORED_ACQUIRE_SWITCH_DISABLED));

    // Set up the src offset (pitch)
    offset = srcBuffer.GetCtxDmaOffsetGpu();
    // Add in our copy offset
    offset += (z==startZ) ? copyOffset[remapper] : twoDSize*z;
    methodData[0] = static_cast<UINT32>(offset >> 32);
    methodData[1] = static_cast<UINT32>(offset);
    status = status && m_testObject.getCh()->MethodWriteN(subChannel, LWA0B5_OFFSET_IN_UPPER,2,methodData, INCREMENTING);

    // Set up the Pitch In
    status = status && m_testObject.getCh()->MethodWrite(subChannel, LWA0B5_PITCH_IN, imgPitch[remapper]);

    // Set up the dst offset (block)
    offset = dstBuffer.GetCtxDmaOffsetGpu();
    methodData[0] = static_cast<UINT32>(offset >> 32);
    methodData[1] = static_cast<UINT32>(offset);
    status = status && m_testObject.getCh()->MethodWriteN(subChannel, LWA0B5_OFFSET_OUT_UPPER,2,methodData, INCREMENTING);

    // Set up block linear source parameters
    // BlockSize, Width, Height, Depth, Layer
    methodData[0] = DRF_NUM(A0B5, _SET_DST_BLOCK_SIZE, _WIDTH, m2mBlockSizeWidth) |
                    DRF_NUM(A0B5, _SET_DST_BLOCK_SIZE, _HEIGHT, m2mBlockSizeHeight) |
                    DRF_NUM(A0B5, _SET_DST_BLOCK_SIZE, _DEPTH, m2mBlockSizeDepth) |
                    DRF_NUM(A0B5, _SET_DST_BLOCK_SIZE, _GOB_HEIGHT, LWA0B5_SET_DST_BLOCK_SIZE_GOB_HEIGHT_GOB_HEIGHT_FERMI_8);
    methodData[1] = imgPitch[remapper]; // Width in bytes of source
    methodData[2] = imgHeight[remapper]; // Height in Pixels of source
    methodData[3] = imgDepth[remapper]; // Depth i Pixels of source
    status = status && m_testObject.getCh()->MethodWriteN(subChannel, LWA0B5_SET_DST_BLOCK_SIZE, 4, methodData, INCREMENTING);

    // Sets the layer (z coord) of the source surface if 3d surface
    status = status && m_testObject.getCh()->MethodWrite(subChannel, LWA0B5_SET_DST_LAYER, z);

    // Set up Origin X bytes and Y samples
    // X is in pixels, y is in pixels
    methodData[0] = DRF_NUM(A0B5, _SET_DST_ORIGIN, _X, x) |     // First pixel in x
                    DRF_NUM(A0B5, _SET_DST_ORIGIN, _Y, y);      // First pixel in y
    status = status && m_testObject.getCh()->MethodWrite(subChannel, LWA0B5_SET_DST_ORIGIN, methodData[0]);

    // Set line length in and number of lines
    status = status && m_testObject.getCh()->MethodWrite(subChannel, LWA0B5_LINE_LENGTH_IN, imgPitch[remapper]-x*imgBpp[remapper]);
    status = status && m_testObject.getCh()->MethodWrite(subChannel, LWA0B5_LINE_COUNT, numLines);

    // Set up the semaphore to signal DMA complete
    offset = semaphore.GetCtxDmaOffsetGpu();
    methodData[0] = (UINT32)(offset >> 32);
    methodData[1] = (UINT32)(offset);
    methodData[2] = DMA_SEMAPHORE_RELEASE_VALUE;
    status = status && m_testObject.getCh()->MethodWriteN(subChannel, LWA0B5_SET_SEMAPHORE_A,3,methodData, INCREMENTING);

    // start the DMA of src surface to dst surface
    status = status &&
    m_testObject.getCh()->MethodWrite(subChannel, LWA0B5_LAUNCH_DMA,
        DRF_NUM(A0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE,     LWA0B5_LAUNCH_DMA_DATA_TRANSFER_TYPE_NON_PIPELINED) |
        DRF_NUM(A0B5, _LAUNCH_DMA, _FLUSH_ENABLE,           LWA0B5_LAUNCH_DMA_FLUSH_ENABLE_TRUE) |
        DRF_NUM(A0B5, _LAUNCH_DMA, _SEMAPHORE_TYPE,         (z == maxZ) ? LWA0B5_LAUNCH_DMA_SEMAPHORE_TYPE_RELEASE_ONE_WORD_SEMAPHORE : LWA0B5_LAUNCH_DMA_SEMAPHORE_TYPE_NONE) |
        DRF_NUM(A0B5, _LAUNCH_DMA, _INTERRUPT_TYPE,         LWA0B5_LAUNCH_DMA_INTERRUPT_TYPE_NONE) |
        DRF_NUM(A0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT,      LWA0B5_LAUNCH_DMA_SRC_MEMORY_LAYOUT_PITCH) |
        DRF_NUM(A0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT,      LWA0B5_LAUNCH_DMA_DST_MEMORY_LAYOUT_BLOCKLINEAR) |
        DRF_NUM(A0B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE,      LWA0B5_LAUNCH_DMA_MULTI_LINE_ENABLE_TRUE) |
        DRF_NUM(A0B5, _LAUNCH_DMA, _REMAP_ENABLE,           LWA0B5_LAUNCH_DMA_REMAP_ENABLE_FALSE) |
        DRF_NUM(A0B5, _LAUNCH_DMA, _BYPASS_L2,              LWA0B5_LAUNCH_DMA_BYPASS_L2_USE_PTE_SETTING) |
        DRF_NUM(A0B5, _LAUNCH_DMA, _SRC_TYPE,               LWA0B5_LAUNCH_DMA_SRC_TYPE_VIRTUAL) |
        DRF_NUM(A0B5, _LAUNCH_DMA, _DST_TYPE,               LWA0B5_LAUNCH_DMA_DST_TYPE_VIRTUAL) |
        DRF_NUM(A0B5, _LAUNCH_DMA, _SEMAPHORE_REDUCTION_ENABLE,    LWA0B5_LAUNCH_DMA_SEMAPHORE_REDUCTION_ENABLE_FALSE) |
        DRF_NUM(A0B5, _LAUNCH_DMA, _SEMAPHORE_REDUCTION_SIGN,   LWA0B5_LAUNCH_DMA_SEMAPHORE_REDUCTION_SIGN_SIGNED));
    status = status && m_testObject.getCh()->MethodFlush();

    } // end for

    return status;
}

//! Backs up host bl_remapper registers
void KeplerBlockLinearRemapperTest::BackupHostBlRegisters(void)
{
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        m_backupRegisters[i].remap_1 = m_testObject.getlwgpu()->RegRd32(pRegRemap1->GetOffset(i));
        m_backupRegisters[i].remap_2 = m_testObject.getlwgpu()->RegRd32(pRegRemap2->GetOffset(i));
        m_backupRegisters[i].remap_4 = m_testObject.getlwgpu()->RegRd32(pRegRemap4->GetOffset(i));
        m_backupRegisters[i].remap_6 = m_testObject.getlwgpu()->RegRd32(pRegRemap6->GetOffset(i));
        m_backupRegisters[i].remap_7 = m_testObject.getlwgpu()->RegRd32(pRegRemap7->GetOffset(i));
    }
}

//! Restores the host bl_remapper registers
void KeplerBlockLinearRemapperTest::RestoreHostBlRegisters(void)
{
    MASSERT(m_backupRegisters != NULL);
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        m_testObject.getlwgpu()->RegWr32(pRegRemap1->GetOffset(i), m_backupRegisters[i].remap_1);
        m_testObject.getlwgpu()->RegWr32(pRegRemap2->GetOffset(i), m_backupRegisters[i].remap_2);
        m_testObject.getlwgpu()->RegWr32(pRegRemap4->GetOffset(i), m_backupRegisters[i].remap_4);
        m_testObject.getlwgpu()->RegWr32(pRegRemap6->GetOffset(i), m_backupRegisters[i].remap_6);
        m_testObject.getlwgpu()->RegWr32(pRegRemap7->GetOffset(i), m_backupRegisters[i].remap_7);
    }
}

//! This Parses the Program Parameters
int KeplerBlockLinearRemapperTest::ParseParameters(void)
{
    // Call the parent's parse first
    int local_status = BlockLinearRemapperTest::ParseParameters();

    UINT32 copyGOBS = params->ParamUnsigned("-copy_gobs", 0);

    if(copyGOBS)
    {
        copySize = copyGOBS * GOB_BYTES;
    }
    else
    {
        copySize = imgSize[0];
        for(int i = 1 ; i < numRemappers ; ++i)
        {
            if(imgSize[i] < copySize)
            {
                copySize = imgSize[i];
            }
        }
    }

    // Get some default register values based on Current GPU
    // Set the default PTE kind for pitch surfaces on fermi to LW_MMU_PTE_KIND_PITCH
    GpuSubdevice * pGpuSubdev = m_testObject.getlwgpu()->GetGpuSubdevice();
    MASSERT(pGpuSubdev != NULL);
    RefManual *refMan = pGpuSubdev->GetRefManual();
    MASSERT(refMan != NULL);
    const RefManualRegister *tmpReg = refMan->FindRegister("LW_PBUS_BL_REMAP_7");
    MASSERT(tmpReg != NULL);

    const RefManualRegisterField *tmpField = tmpReg->FindField("LW_PBUS_BL_REMAP_7_NUM_BYTES_PIXEL");
    MASSERT(tmpField != NULL);

    const RefManualFieldValue *tmpVal = NULL;

    // Check bpp is a valid value
    for(int i = 0 ; i < numRemappers ; ++i )
    {
        switch(imgBpp[i])
        {
            case 1:
                tmpVal = tmpField->FindValue("LW_PBUS_BL_REMAP_7_NUM_BYTES_PIXEL_1");
                MASSERT(tmpVal != 0);
                //remapNumBytesEnum[i] = tmpField->FindValue(LW_PBUS_BL_REMAP_7_NUM_BYTES_PIXEL_1).Value;
                remapNumBytesEnum[i] = tmpVal->Value;
                imgColorFormat[i] = ColorUtils::Y8;
                break;
            case 2:
                //remapNumBytesEnum[i] = LW_PBUS_BL_REMAP_7_NUM_BYTES_PIXEL_2;
                tmpVal = tmpField->FindValue("LW_PBUS_BL_REMAP_7_NUM_BYTES_PIXEL_2");
                MASSERT(tmpVal != 0);
                remapNumBytesEnum[i] = tmpVal->Value;
                imgColorFormat[i] = ColorUtils::Z16;
                break;
            case 4:
                //remapNumBytesEnum[i] = LW_PBUS_BL_REMAP_7_NUM_BYTES_PIXEL_4;
                tmpVal = tmpField->FindValue("LW_PBUS_BL_REMAP_7_NUM_BYTES_PIXEL_4");
                MASSERT(tmpVal != 0);
                remapNumBytesEnum[i] = tmpVal->Value;
                imgColorFormat[i] = ColorUtils::R8G8B8A8;
                break;
            case 8:
                //remapNumBytesEnum[i] = LW_PBUS_BL_REMAP_7_NUM_BYTES_PIXEL_8;
                tmpVal = tmpField->FindValue("LW_PBUS_BL_REMAP_7_NUM_BYTES_PIXEL_8");
                MASSERT(tmpVal != 0);
                remapNumBytesEnum[i] = tmpVal->Value;
                imgColorFormat[i] = ColorUtils::RF16_GF16_BF16_AF16;
                break;
            case 16:
                //remapNumBytesEnum[i] = LW_PBUS_BL_REMAP_7_NUM_BYTES_PIXEL_16;
                tmpVal = tmpField->FindValue("LW_PBUS_BL_REMAP_7_NUM_BYTES_PIXEL_16");
                MASSERT(tmpVal != 0);
                remapNumBytesEnum[i] = tmpVal->Value;
                imgColorFormat[i] = ColorUtils::RF32_GF32_BF32_AF32;
                break;
            default:
                ErrPrintf(T_NAME": Invalid option %d for -img_bpp choose from 1, 2, 4, 8, 16.\n", imgBpp);
                return 0;
        }
    }

    return local_status;
}
