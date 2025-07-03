/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008, 2017-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
 //! \file frontdoor.h
 //! \brief Definition of a class to test CPU frontdoor accesses.
 //!
 //!

#ifndef _FRONTDOOR_H
#define _FRONTDOOR_H

#include "mdiag/utils/types.h"
#include "mdiag/tests/test.h"
#include "mdiag/utils/buf.h"
#include "class/cl9068.h" // GF100IndirectFramebuffer

class RandomStream;
class BuffInfo;

//#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/tests/gpu/lwgpu_basetest.h"

//! \brief Bar 1 bandwidth perf test
//!
//! This class tests the Bar 1 CPU bandwidth for writes or reads.
class frontdoorTest : public LWGpuBaseTest
{
public:
    frontdoorTest(ArgReader *params);

    virtual ~frontdoorTest(void);

    static Test *Factory(ArgDatabase *args);

    //! a little extra setup to be done
    virtual int Setup(void);

    //! Run() overridden - now we're a real test!
    virtual void Run(void);

    //! a little extra cleanup to be done
    virtual void CleanUp(void);

protected:
    static const uint UPPER_BAR2_OFFSET = 0x1000000;

    //! A gpu
    LWGpuResource *lwgpu;

    // BuffInfo Object to store info about allocated buffers, and do a nice
    // alloc print like trace3d
    BuffInfo *m_buffInfo;

    //! Should we use random or incremental values in the buffers
    bool noRandomData;

    // Number of buffers
    static const int NUM_BAR_BUFFERS = 2;
    int numMapBuffers;

    //!  Stores the stride in DWORDS (4 bytes) for a write
    UINT32 mapWriteStride, copyWriteStride;
    //! Number of conselwtive 4byte writes, defaults to 1
    UINT32 mapWriteSize, copyWriteSize;
    //! Number of writes to do
    UINT32 mapWriteNum, copyWriteNum;
    //! Page Kind for bar2 Surface
    INT32 bar2BufferKind;
    //! Memory location of bar2 Surface
    Memory::Location bar2BufferLocation;

    // Buffer data
    //! Data to use in the src surface.
    UINT32 copyBufferSize;
    UINT08 *testBuffer[NUM_BAR_BUFFERS];
    UINT08 *refBuffer[NUM_BAR_BUFFERS];

    //! Size of an individual buffer for stressing TLB
    UINT32 mapBufferSize;
    //! Size of the buffer for the bar2 test
    UINT32 bar2BufferSize;

    //! Reference data for buffers stressing tlb
    UINT08* *mapBufferRef;

    //! Pointer to the gpu sub device we're manipulating
    GpuSubdevice *pGpuSub;

    // Parameters
    UINT32 seed0,seed1,seed2;
    RandomStream *RndStream;
    ArgReader *params;
    bool doBar2Testing;

    // Surfaces
    MdiagSurf barBuffer[NUM_BAR_BUFFERS]; //!< One buffer for bar 0, and 1
    MdiagSurf bar2Buffer;
    MdiagSurf semaphoreBuffer;
    MdiagSurf* mapBuffers; //!< MODS surface allocation for buffers stressting tlb
    MdiagSurf ifbBuffer;
    //! Should the VBIOS range BAR2 test be skipped. Needed to support 0FB.
    bool skipBar2VbiosRangeTest;

    // IFB Test Stuff
    bool doIfbTest;
    bool doIfbReadFault;
    bool doIfbWriteFault;
    bool m_gen5Capable;
    //! Size of the ifbBuffer
    static const unsigned int ifbBufferSize = 64;
    //! Page Kind for the IFB Surface
    INT32 ifbBufferKind;
    //! Memory location of the IFB Surface
    Memory::Location ifbBufferLocation;
    //! Handle to RM IFB Object
    LwRm::Handle hIfb;
    //! Pointer to the RM IFB object
    volatile GF100IndirectFramebuffer *pIfb;
    //! Offset of the IFB.
    LwU64 ifbOffset;
    //!interrupt_file name for IFB error recognition
    string ifbIntFileName;
    string ifbIntDir;

    bool SetupIFBTest(void);
    bool RunIFBTest(void);
    void CleanupIFBTest(void);

    //! Runs the TLB subtest
    bool RunBar1ToBar0TlbTest(void);

    //! Runs the BAR2 subtest
    RC RunBar2Test(void);

    // Utility functions
    bool ValidateParams(void);

    //! \brief Enables the backdoor access
    void EnableBackdoor(void);
    //! \brief Disable the backdoor access
    void DisableBackdoor(void);

    //! \brief Initializes the buffer with random data.
    void InitBuffer(UINT08 *buffer, UINT32 size);

    //! Sets up the surface object in memory.
    int SetupSurface( MdiagSurf *surface, UINT32 size,
                      Memory::Location memLoc, INT32 pageKind );
    //! Sets up a pointer to the BAR2 address space
    bool SetupBar2Address(void **);

    //! \brief Compares the two buffers
    //! Compares the two buffers, basically strncat with an error message
    //!
    //! \param buff0 Pointer to first buffer to compare
    //! \param buff1 Pointer to 2nd
    //! \param stride Stride in DWORDs to check
    //! \param writeSize Size in DWORDS of conselwtive comparisons
    //! \param count Number of comparisons to do
    //! \return true if buffers match, false otherwise
    bool CompareBuffers(UINT08 *buff0, UINT08 *buff1, UINT32 stride, UINT32 writeSize, UINT32 count);

    //! Returns the LW_PBUS_BAR0_WINDOW_TARGET for a surface
    //!
    //! \param surface Reference to the surface we want to get the target of
    //! \return The LW_PBUS_BAR0_WINDOW_TARGET
    uint GetBar0WindowTarget(const MdiagSurf& surface);
    //! Returns the physical offset for the surface at offset \p offset.
    //!
    //! \param[in] surface MdiagSurf to get offset of
    //! \param[in,out] offset As an input this contains the offset into the
    //! surface we want the address of. After calling this function \p offset
    //! will be set to the physical memory address of the the surface at the
    //! specified offset.
    //! \return An RC OK on success.
    RC GetBar0WindowOffset(const MdiagSurf& surface, LwU64& offset);

    //! Read and fill a buffer from BAR0_WINDOW.
    //!
    //! \param buffer Pointer to a buffer to fill with read data.
    //! \param readSize Number of bytes to read from BAR0 window
    void Bar0WindowRead(UINT08 * buffer, UINT32 readSize);

    //! Runs a test on the upper 16MB of BAR2. This test just clobbers the
    //! memory at that location which corresponds to the start of the GPU
    //! framebuffer.
    RC RulwbiosRangeBar2Test(void);

    //! Destroys Member Variables
    void DestroyMembers(void);
    TestEnums::TEST_STATUS CheckIntrPassFail(void);
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(frontdoor, frontdoorTest, "Frontdoor access memory test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &frontdoor_testentry
#endif

#endif //_HOST_BAR1_PERF_H
