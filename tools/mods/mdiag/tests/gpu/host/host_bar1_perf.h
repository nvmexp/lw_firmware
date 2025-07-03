/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2012, 2017, 2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
 //! \file host_bar1_perf.h
 //! \brief Definition of a class to test CPU bar 1 bandwidth.
 //!
 //!

#ifndef _HOST_BAR1_PERF_H
#define _HOST_BAR1_PERF_H

#include "mdiag/utils/types.h"
#include "mdiag/tests/test.h"
#include "mdiag/utils/buf.h"

class RandomStream;
class BuffInfo;

// host Bar1 bandwidth test.
#include "mdiag/tests/gpu/lwgpu_basetest.h"

//! Abstract class to implement chip/family specific features of the test
class HostBar1PerfTest
{
public:
    //! Constructor
    //!
    //! \param lwgpu Pointer to an LWGpuResource object to use for memory/register access. This pointer will not be freed by HostBar1PerfTest
    HostBar1PerfTest(LWGpuResource * lwgpu);
    //! Destructor
    virtual ~HostBar1PerfTest();
    //! Uses register writes to start the PmCounters
    virtual void StartPmTriggers() const = 0;
    //! Uses register writes to stop the PmCounters
    virtual void StopPmTriggers() const = 0;
    //! \brief Set's up host's BL Remapper
    //! Configures the passed in Host block linear remapping register to remap a surface.
    //!
    //! \param remapper Remapper to configure
    //! \param surface MdiagSurf to remap
    virtual void SetupHostBlRemapper(const UINT32 remapper, const MdiagSurf &surface, const MdiagSurf &blSurface, const UINT32 srcSize) const = 0;

    //! Backs up the current settings of the passed in remapper.
    virtual void BackupBlRemapperRegisters(const UINT32 remapper) = 0;
    //! Restores the previous settings of the passed in remapper.
    virtual void RestoreBlRemapperRegisters(const UINT32 remapper) const = 0;

    //! Returns the IMG_PITCH constant
    const UINT32 GetPitch() { return IMG_PITCH; };
    const UINT32 GetGobBytesWidth() { return GOB_BYTES_WIDTH; };
    const UINT32 GetGobHeight() { return GOB_HEIGHT; };
    const UINT32 GetDepth() { return (1<<DEPTH); };

    UINT32 blImgHeightInGobs(const UINT32 srcSize) const
    {
      if(IMG_PITCH*GOB_HEIGHT<=0)
        return 0;
      else
        return srcSize/(IMG_PITCH*GOB_HEIGHT) + ((srcSize%(IMG_PITCH*GOB_HEIGHT)) ? 1 : 0);
    }

    UINT32 blSurfaceSize(UINT32 srcSize) {
      return blImgHeightInGobs(srcSize) * GOB_HEIGHT * IMG_PITCH * (1<<DEPTH);
    }

protected:
    //! Struct to backup BL_REMAPPER registers
    struct bl_registers {
        UINT32 remap_1;
        UINT32 remap_2;
        UINT32 remap_3;
        UINT32 remap_4;
        UINT32 remap_5;
        UINT32 remap_6;
        UINT32 remap_7;

        bl_registers():remap_1(0),remap_2(0),remap_3(0),remap_4(0),remap_5(0),remap_6(0),remap_7(0){};
    };

    //! Holds the backed up block linear remapper registers for a single remapper.
    //! The test only uses a single register so we only need one backup.
    bl_registers backupBlRegister;

    //! Pointer to an LWGpuResource object to be used for register access.
    //! Object is owned by a parent class and so this class should NOT destroy the object.
    LWGpuResource *lwgpu;

    //! Offset of the register used to start a PmTrigger event
    UINT32 startPmTriggerRegOffset;
    //! Value to write to a register to start a PmTrigger event
    UINT32 startPmTriggerRegValue;
    //! Offset of the register used to stop a PmTrigger event
    UINT32 stopPmTriggerRegOffset;
    //! Value to write to a register to stop a PmTrigger event
    UINT32 stopPmTriggerRegValue;

    // GOB Constants for BL Remapper
    //! Width of a GOB in bytes
    UINT32 GOB_BYTES_WIDTH;
    //! Height of a GOB in lines
    UINT32 GOB_HEIGHT;
    //! Size of a GOB in bytes
    UINT32 GOB_BYTES;
    //! pitch of the image
    const UINT32 IMG_PITCH;
    const UINT32 DEPTH;

};

//! \brief Bar 1 bandwidth perf test
//!
//! This class tests the Bar 1 CPU bandwidth for writes or reads.
class host_bar1_perfTest : public LWGpuBaseTest
{
public:
    host_bar1_perfTest(ArgReader *params);

    virtual ~host_bar1_perfTest();

    static Test *Factory(ArgDatabase *args);

    //! a little extra setup to be done
    virtual int Setup();

    //! Run() overridden - now we're a real test!
    virtual void Run();

    //! a little extra cleanup to be done
    virtual void CleanUp();

protected:

    //! \brief Checks destination surface to make sure copy is correct.
    virtual bool CheckDst();
    //! \brief Checks cpu surface to verify copy is correct.
    virtual bool CheckCPU();

    //! \brief Set's up the performance monitors for this test.
    virtual int SetupPerfmon();

    //! \brief Initializes the source buffer.
    void InitSrcBuffer(UINT32 size);

    //! \brief Set's up a dummy BL Surface where the data will actually be written
    UINT32 SetupBlSurface(MdiagSurf &surface);

    //! \brief Set's up the TargetBuffer
    RC SetupTargetBuffer(MdiagSurf &targetBuffer);

    //! \brief Enables the backdoor access
    void EnableBackdoor() const;
    //! \brief Disable the backdoor access
    void DisableBackdoor() const;

    //! \brief Flushes the L2 cache to FB.
    void FlushL2Cache() const;
    UINT32 pitch2block_swizzle(UINT32 plOffset);
    UINT32 block2pitch_swizzle(UINT32 blOffset);

    //! A gpu
    LWGpuResource *lwgpu;

    // Perform a memory read or write
    bool read;
    bool write;
    // Use backdoor accesses
    bool paramNoBackdoor;
    bool useBlRemapper;

    //! Data to use in the src surface.
    UINT08 *srcData;
    UINT32 srcSize;
    UINT08 *refBuffer;

    UINT32 seed0,seed1,seed2;
    RandomStream *RndStream;
    ArgReader *params;

    //! Are we running a perf test?
    bool perfTest;

    int local_status;

    MdiagSurf targetBuffer;
    MdiagSurf blSurface;

    // BuffInfo Object to store info about allocated buffers, and do a nice
    // alloc print like trace3d
    BuffInfo *m_buffInfo;

    //! Size of bursts to pass to MODS, the test will do srcSize/burstSize reads/writes
    UINT32 burstSize;
    //! Time to yield between bursts, 0 disables yields. Default is for yield to be off.
    UINT32 burstYieldTime;

    //! Pointer to GPU specific object for test setup.
    unique_ptr<HostBar1PerfTest> m_gpuTestObj;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(host_bar1_perf, host_bar1_perfTest, "host Bar1 BW test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &host_bar1_perf_testentry
#endif

#endif //_HOST_BAR1_PERF_H

