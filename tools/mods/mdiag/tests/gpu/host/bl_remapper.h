/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013, 2017, 2020-2021 by LWPU Corporation.  All rights 
 * reserved. All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _BL_REMAPPER_H
#define _BL_REMAPPER_H

#include "mdiag/utils/types.h"

//#define HOST_NUM_REMAPPERS 8

class RandomStream;
class GpuDevice;

// very simple bl_remapper test - write to a surface using BAR1 writes followed by an BL_REMAPPER and then reads from another engine

#include "mdiag/tests/gpu/lwgpu_single.h"

class bl_remapperTest;

//! This class contains the main logic of the bl_remapper test
//!
//! This object is not directly instatiated, instead we declare a subclass for the appropriate GPU.
class BlockLinearRemapperTest
{
public:
    //! Test Constructor
    //!
    //! Reads and sets up the command line parameters. Also initializes some class variables.
    //!
    //! \param parent The parent object of type class bl_remapperTest. This is needed to set test status.
    //! \param params Test arguments
    BlockLinearRemapperTest(bl_remapperTest& parent, ArgReader *params);

    //! Destructor - Just clean up
    virtual ~BlockLinearRemapperTest(void);

    //! Additional test setup work.
    //!
    //! This method verifies parameters.
    //! Initializes the imageBuffer
    //! Creates the copy object
    //! Allocates the DMA surfaces
    //! Sets up any semaphores
    //! \param lwgpu Pointer to an LWGpuResource object for this test.
    //! \param ch Pointer to an LWGpuChannel object to use for the test.
    virtual int Setup(LWGpuResource *lwgpu, LWGpuChannel* ch);

    //! Needs to be overridden
    virtual void Run(void);

    virtual void CleanUp(void);

    UINT32 copyClassToUse;      //!< Use Fermi or Kepler copy class

protected:
    LWGpuResource *m_lwgpu; //!< Object used to access a GPU.
    LWGpuChannel *m_ch;     //!< Channel object used for copy operations, created by parent class.
    UINT32* imgWidth;     //!< Width of the image in pixels, one per remapper
    UINT32* imgHeight;    //!< Height of the image in lines, one per remapper
    UINT32* imgDepth;     //!< Depth of the image in slices, one per remapper
    UINT32* imgPitch;     //!< Pitch of the image in bytes, one per remapper
    UINT32* imgBpp;       //!< Bytes per pixel of the image, one per remapper
    UINT32* imgSize;      //!< Size of the image in bytes, one per remapper
    UINT32* burstImgSize;
    UINT32* imgTotalGobs; //!< Total GOBS in the image, one per remapper

    UINT32* imgLogBlockHeight; //!< log base 2 of the image height in BLOCKS, one per remapper
    UINT32* imgLogBlockWidth;  //!< log base 2 of the image width in BLOCKS, one per remapper
    UINT32* imgLogBlockDepth;  //!< log base 2 of the image depth in BLOCKS, one per remapper

    UINT32* copyOffset; //!< Offset to start of copy region, one per remapper
    UINT32 copySize;    //!< Number of bytes to copy

    UINT32* remapNumBytesEnum;          //!< Number of bytes per pixel colwerted to value from manuals enumeration, one per remapper
    ColorUtils::Format* imgColorFormat; //!< Color format of the image, one per remapper
    Memory::Location pitchLocation;     //!< Memory::Location of the pitch surface
    Memory::Location blockLocation;     //!< Memory::Location of the block linear surface

    bool* dumpSurface; //!< Should the test dump a raw version of the surface.

    //! BuffInfo Object to store info about allocated buffers, and do a nice
    //! alloc print like trace3d.
    BuffInfo *m_buffInfo;
    //! The GPU device
    GpuDevice * pGpuDev;

    UINT32 subChannel;         //!< Subchannel to use for the copy operation
    RandomStream *RndStream;

    //! Back door enabled?
    bool m_enableBackdoor;

    //! Use surface offset to assign position
    bool usePositionData;

    //! Did the initialization fail?
    bool initFail;

    //! Should we dump the surfaces on a failed check?
    bool dumpSurfaces;

    //! Do a bl->pitch single burst test rather than a normal test
    bool blPitchBurst;
    //! Size of the PCIE burst, used to make sure we cross a pitch/block boundary with a single transaction.
    UINT32 pcieBurstSize;
    //! Number of bytes to write before printing a status message
    UINT32 printStatusInterval;

    //! Commandline arguments
    ArgReader *params;

    UINT32 seed0,seed1,seed2;

    MdiagSurf* blockBuffer;       //!< The block linear surface, one per host remapper. This is also used as the dst buffer (post remap) in the BoundarySplit test.
    MdiagSurf* pitchBuffer;       //!< The pitch surface, one per host remapper. This is also used as the src buffer in the BoundarySplit test.
    MdiagSurf* semaphoreBuffer;   //!< Semaphore surface, one per host remapper

    UINT32 copyHandle;       //!< Handle to allocated copy object

    UINT08* *imageBuffer;       //!< Buffer to store the source data
    UINT08 *tmpBuffer;          //!< Buffer to store results of reads

    bl_remapperTest& m_testObject;  //!< The parent test object, needed to set status.

    //! Struct to backup BL_REMAPPER registers
    struct bl_registers {
        UINT32 remap_1;
        UINT32 remap_2;
        UINT32 remap_4;
        UINT32 remap_6;
        UINT32 remap_7;
    };

    bl_registers *m_backupRegisters; //! Array of backed up host block linear registers

    //! Number of remappers
    int numRemappers;

    // Class constants
    static const UINT32 CPU_SEMAPHORE_VALUE = 0x33;
    static const UINT32 DMA_SEMAPHORE_RELEASE_VALUE = 0x1234;

    //! Width of a GOB in bytes
    int GOB_WIDTH_BYTES;
    //! Height of a GOB in lines
    int GOB_HEIGHT_IN_LINES;
    //! Total number of bytes in a GOB
    int GOB_BYTES;
    //! PTE Kind for block surfaces
    UINT32 blockSurfacePteKind;
    //! PTE Kind for pitch surfaces
    UINT32 pitchSurfacePteKind;

    //! Allocates all array members of the class
    //! \return 0 on success
    virtual int AllocateMembers(void);

    //! Initialize and allocate the MdiagSurf objects
    int SetupSurfaces(void);
    //! Sets up the semaphore buffers and writes an initial value
    int SetupSemaphores(void);
    //! Set's up one host block linear remapper register
    //!
    //! \param remapper Remapper to setup, Fermi has 0-7
    //! \param baseBuf MdiagSurf pointing at the pitch surface allocated in vidmem
    //! \param blockBaseBuf MdiagSurf buffer for the block surface allocated in vidmem
    //! \param baseOffset Pitch surface location for remapper is base of baseBuf + baseOffset
    //! \param blockBaseOffset Block surface location for remapper is base address of blockBaseBuf + blockBaseOffset
    virtual int SetupHostBLRemapper( int remapper,
                                     MdiagSurf &baseBuf,
                                     MdiagSurf &blockBaseBuf,
                                     uint baseOffset,
                                     uint blockBaseOffset
                                   ) = 0;

    //! Disables a remapper
    virtual void DisableRemapper(int remapper) = 0;

    //! \brief Enables the backdoor access if the test and platform allows.
    void EnableBackdoor();
    //! \brief Disable the backdoor access
    void DisableBackdoor();

    //! This Parses the Program Parameters
    virtual int ParseParameters(void);
    //! \brief Parses one of the image arguements
    //!
    //! Reads and parses an image arguement
    //!
    //! \param param Parameter to process
    //! \param buffer Buffer to store the values in
    //! \param size Number of entries in the buffer
    //!
    //! \return 0 on success
    int ParseImgArg(const char *param, UINT32 *buffer, UINT32 size);

    //! Does a Block2Pitch memory_to_memory_format_a dma transfer.
    //!
    //! \param srcBuffer reference to source MdiagSurf.
    //! \param dstBuffer reference to destination MdiagSurf.
    //! \param semaphore reference to semaphore MdiagSurf
    //! \return 1 on success0.
    virtual int DoBlock2PitchDma(MdiagSurf &srcBuffer, MdiagSurf &dstBuffer,
                                 MdiagSurf &semaphore, UINT32 remapper) = 0;
    //! Does a Pitch2Block memory_to_memory_format_a dma transfer.
    //!
    //! \param srcBuffer reference to source MdiagSurf.
    //! \param dstBuffer reference to destination MdiagSurf.
    //! \param semaphore reference to semaphore MdiagSurf
    //! \return 1 on success 0 on some terrible error.
    virtual int DoPitch2BlockDma(MdiagSurf &srcBuffer, MdiagSurf &dstBuffer,
                                 MdiagSurf &semaphore, UINT32 remapper) = 0;

    // Writes or checks the images from the buffers
    void WriteBuffers(MdiagSurf *dst, const Memory::Location &blockLocation);
    void ZeroBuffers(MdiagSurf *dst);    
    int  CheckBuffers(MdiagSurf *dst);
    //! Initializes the source imageBuffer.
    void InitSrcBuffer(UINT08 *buffer, UINT32 size);

    //! \brief Dumps all the test surfaces
    //!
    //! Dumps all the test surfaces as a raw binary file for debugging purposes.
    void DumpSurfaces(void);

    // Back up and restore bl_remapper register.
    virtual void BackupHostBlRegisters(void) = 0;
    virtual void RestoreHostBlRegisters(void) = 0;

    // the commmon setup function for all sub-classes of bl_remapper
    virtual void SetupDev(void)=0;

    //! The test
    //!
    //! This tests the host block linear functionality.
    //! The procedure is as follows:
    //!
    //! (1) Setup/Enable Host Block Linear Remappers.
    //! (2) Write image data to block buffers through host.
    //! (4) Verify pitch buffers match source.
    //! (5) Write new data to pitch buffers.
    //! (6) Block Linear DMA Copy to block buffers.
    //! (7) Verify block buffers match reference via Host remappers.
    //!
    //! if (4) and (7) match then the test passes.
    void RunBasicBlockLinearRemapperTest(void);

    //! Sets up the test, allocates and initializes surfaces.
    int SetupBasicBlockLinearRemapperTest(void);

/////////////////////////////////
// Methods for burst testing
/////////////////////////////////
    //! Setups up a test which tests transaction splitting on pitch/block boundaries
    //!
    //! Setup ilwolves parsing some parameters, allocating surfaces, and initializing surfaces.
    //! See SetupBoundarySplitTestSurfaces for more details on surface allocation.
    int SetupBoundarySplitTest(void);

    //! Does all surface allocations for the Boundary split test.
    //!
    //! Setup ilwolves allocating two surfaces which are 2x the size of the
    //! test image.
    //! The remappers will be setup so that the first 1/4 of the allocated
    //! surface is pitch.
    //! Followed by a region of memory, 1/2 the size of the surface
    //! which will be configured to be remapped.
    //! The last 1/4 of the surface will also be pitch.
    //! The remapped region of the first surface will be set as the block linear pitch base.
    //! The center of the second surface will be set as the block linear block base.
    int SetupBoundarySplitSurfaces(void);

    //! Runs the Boundary Split test.
    //!
    //! Test run procedure is as follows.
    //! (1) Setup the host block linear remappers, so that the surface to remapped, and the block surface do not overlap.
    //! (2) Do a burst write which crosses a pitch/block boundary
    //! (3) Turn off the remappers, and read the pitch data + overlap into bl space to make sure nothing was clobberd
    //! (4) Read back the block linear data, also read extra data round the block linear data to make sure we didn't do weird clobbering
    //! (5) Write some new pitch and block linear data
    //! (6) Do a burst read and verify the data is correct
    void RunBoundarySplitTest(void);

    //! Does a burst write to the passed in surface at the given offest.
    //!
    //! \param dst MdiagSurf offset corresponding to buffer to write.
    //! \param data Data to write to the buffer.
    //! \param offset Write is done at address = dst base addr + offset.
    //! \param burstSize Size of the burst sent over PCIE.
    void DoBurstWrite(const MdiagSurf &dst, UINT08 *data, UINT32 offset, UINT32 burstSize);
    int PitchBlockBurstCheckBuffers(MdiagSurf *dst, UINT32 burstSize);

    //! Colwerts a pitch offset into a block offset.
    //!
    //! \param offset            Offset in pitch space.
    //! \param pitch             Surface pitch in bytes.
    //! \param imgHeight         Surface height in lines.
    //! \param pitchInGobs       Surface pitch in GOBS.
    //! \param imgSizeInGobs     Image size in GOBS.
    //! \param imgHeightInGobs   Image height in GOBS.
    //! \param imgLogBlockDepth  Log base 2 of the depth of a block in GOBS.
    //! \param imgLogBlockHeight Log base 2 of the height of a block in GOBS.
    UINT32 PitchToBlockOffset( const UINT32 offset, const UINT32 pitch,
                               const UINT32 imgHeight, const UINT32 pitchInGobs,
                               const UINT32 imgSizeInGobs, const UINT32 imgHeightInGobs,
                               const UINT32 imgLogBlockDepth, const UINT32 imgLogBlockHeight
                             );

    //! Verifies that the split write worked correctly.
    //! Checks the pitch surface, slight overead to verify data wasn't altered
    //! Checks the bl data. Also reads around the BL region to verify
    //! data which shouldn't be written wasn't altered.
    //!
    //! \param remapper    The host bl_remapper register which was used for the write.
    //!
    //! \return bool  Returns true on success, false on failure.
    bool VerifyBoundarySplitWrite(int remapper);
};

class bl_remapperTest : public LWGpuSingleChannelTest
{
public:
    bl_remapperTest(ArgReader *params);

    virtual ~bl_remapperTest(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    virtual void CleanUp(void);
    LWGpuResource* getlwgpu(void) const { return lwgpu; };
    LWGpuChannel*  getCh(void) const { return ch; };
    UINT32 getClassId(void) const { return copyClassToUse; };
    virtual void DoYield(unsigned int time = 0) { Yield(time); };

protected:
    //! The actual test to be run.
    unique_ptr<BlockLinearRemapperTest> remapperTestChip;
    UINT32 copyClassToUse;      // Use Fermi or Kepler copy class
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(bl_remapper, bl_remapperTest, "Verifies operation of reading to and writing from block-linear memory via the C11 interface on Host");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &bl_remapper_testentry
#endif

#endif
