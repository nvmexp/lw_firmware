/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009, 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _TURING_BL_REMAPPER_H
#define _TURING_BL_REMAPPER_H

#include "mdiag/tests/gpu/host/bl_remapper.h"
#include "core/include/refparse.h"

class TuringBlockLinearRemapperTest: virtual public BlockLinearRemapperTest
{
public:

    //! Test Constructor
    //!
    //! Reads and sets up the command line parameters. Also initializes some class variables.
    //!
    //! \param parent The parent object of type class bl_remapperTest. This is needed to set test status.
    //! \param params Test arguments
    //! \param lwgpu Pointer to an LWGpuResource object for this test.
    //! \param ch Pointer to an LWGpuChannel object to use for the test.
    TuringBlockLinearRemapperTest(bl_remapperTest& parent, ArgReader *params);

    //! Set's up one host block linear remapper register
    //!
    //! \param remapper Remapper to setup, Turing has 0-7
    //! \param baseBuf MdiagSurf pointing at the pitch surface allocated in vidmem
    //! \param blockBaseBuf MdiagSurf buffer for the block surface allocated in vidmem
    //! \param baseOffset Pitch surface location for remapper is base of baseBuf + baseOffset
    //! \param blockBaseOffset Block surface location for remapper is base address of blockBaseBuf + blockBaseOffset
    int SetupHostBLRemapper(int remapper, MdiagSurf &baseBuf, MdiagSurf &blockBaseBuf,
                            uint baseOffset, uint blockBaseOffset
                           );

    //! Completely disables the passed in remapper
    //!
    //! \param remapper The remapper to disable gf100 has 0-7
    void DisableRemapper(int remapper);

    //! Does a Block2Pitch memory_to_memory_format_a dma transfer.
    //!
    //! \param srcBuffer reference to source MdiagSurf.
    //! \param dstBuffer reference to destination MdiagSurf.
    //! \param semaphore reference to semaphore MdiagSurf
    //! \return 1 on success0.
    int DoBlock2PitchDma( MdiagSurf &srcBuffer, MdiagSurf &dstBuffer,
                          MdiagSurf &semaphore, UINT32 remapper );

    //! Does a Pitch2Block memory_to_memory_format_a dma transfer.
    //!
    //! \param srcBuffer reference to source MdiagSurf.
    //! \param dstBuffer reference to destination MdiagSurf.
    //! \param semaphore reference to semaphore MdiagSurf
    //! \return 1 on success 0 on some terrible error.
    int DoPitch2BlockDma( MdiagSurf &srcBuffer, MdiagSurf &dstBuffer,
                          MdiagSurf &semaphore, UINT32 remapper );

    //! Back up the host BlRemapper registers
    void BackupHostBlRegisters(void);
    //! Restore the host BlRemapper registers
    void RestoreHostBlRegisters(void);

    void SetupDev(void);
    
    //! This Parses the Program Parameters
    virtual int ParseParameters(void);
protected:
    //! Pointer to the LW_PBUS_BL_REMAP_1 register manual information
    const RefManualRegister *pRegRemap1;
    //! Pointer to the LW_PBUS_BL_REMAP_2 register manual information
    const RefManualRegister *pRegRemap2;
    //! Pointer to the LW_PBUS_BL_REMAP_4 register manual information
    const RefManualRegister *pRegRemap4;
    //! Pointer to the LW_PBUS_BL_REMAP_6 register manual information
    const RefManualRegister *pRegRemap6;
    //! Pointer to the LW_PBUS_BL_REMAP_7 register manual information
    const RefManualRegister *pRegRemap7;
};

#endif
