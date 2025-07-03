/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/channel.h"
#include "core/include/types.h"
#include "core/tests/testconf.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gralloc.h"
#include "gpu/include/nfysema.h"
#include "gpu/utility/atomwrap.h"
#include "mme64igen.h"
#include "mme64macro.h"

#include <memory>
#include <vector>

/*
  DOC(stewarts):

  Terms:
  - mme_instruction_ram
  - mme_start_addr table
*/

namespace Mme64
{
    //!
    //! \brief Represents the MME64 instruction RAM.
    //!
    //! NOTE: mme_instruction_ram is indexed 4 bytes at a time.
    //!
    class InstructionRam
    {
    public:
        using Addr = UINT32;

        InstructionRam(UINT32 size, Channel* pCh3d);

        RC Load(const Macro::Groups& groups, Addr* pOutStartAddr);
        RC Free(Addr addr);
        RC FreeAll();

        UINT32 GetRamSize() const { return m_RamSize; }

    private:
        //!
        //! \brief Contiguous section of MME64 instruction RAM.
        //!
        struct RamChunk
        {
            Addr addr;                  //!< Instruction ram address of the chunk
            UINT32 size;                //!< Number of dwords
            bool isFree;                //!< True if the memory is free

            RamChunk(Addr addr, UINT32 size, bool isFree)
                : addr(addr), size(size), isFree(isFree) {}

            bool operator==(const RamChunk& other) const;
            bool operator!=(const RamChunk& other) const;
        };
        using chunkIter = std::list<RamChunk>::iterator;

        std::list<RamChunk> m_Memory; //!< Memory chunk information
        UINT32 m_RamSize;     //!< Num dwords in the MME64 Instruction RAM
        Channel* m_pCh;       //!< DMA Channel to the 3d engine w/ MME64

        RC LoadInstrMem(Addr startAddr, const Macro::Groups& groups);
        RC RecordAlloc
        (
            chunkIter* pFromChunkIter
            ,const Macro::Groups& groups
            ,UINT32 packSize
        );
        RC RemoveChunk(chunkIter* pChunkIter);
    };

    //!
    //! \brief Loads MME64 Groups into the MME64 unit.
    //!
    class MacroLoader
    {
    public:
        MacroLoader(GpuSubdevice* pSubdev, Channel* pCh3d);

        RC Load(Macro* pMacro);

        //!
        //! \brief Load the given macro into the given macro start
        //! address table entry.
        //!
        //! NOTE: You can override a given entry in this table. Remember the
        //! data in the instruction ram associated with the old table entry is
        //! now considered free.
        //!
        RC LoadAt(Macro* pMacro, Macro::StartAddrPtr mmeTableEntry)
            { return RC::UNSUPPORTED_FUNCTION; }

        RC Free(Macro* pMacro);
        RC FreeAll();

        UINT32 GetInstrRamSize() const { return m_pInstrRam->GetRamSize(); }
        UINT32 GetMaxNumMacros() const { return m_StartAddrRamSize; }

        void SetDebugPrintPri(Tee::Priority pri);

    private:
        Tee::Priority m_DebugPrintPri;

        //! DMA channel to the 3d engine w/ MME64
        Channel* m_pCh;
        //! MME64 Instruction RAM
        unique_ptr<InstructionRam> m_pInstrRam;

        //! Start address ram (table) size in dwords (w/ dword size table
        //! entries).
        UINT32 m_StartAddrRamSize;
        //! Maps start address ram pointer (read 'table entry') the associated
        //! macro's load location in the instruction ram.
        map<Macro::StartAddrPtr, InstructionRam::Addr> m_AllocationMap;

        RC FindAvailableStartAddrPtr(Macro::StartAddrPtr* pTableEntry);
    };

    using VirtualAddr = uintptr_t;

    class DataRam
    {
    public:
        //!
        //! \brief Possible sizes for the Data FIFO.
        //!
        //! Ref: https://p4viewer.lwpu.com/get/hw/lwgpu/class/mfs/class/common/CommonMmeMethods.mfs //$
        //!
        enum class FifoSize
        {
            _0KB   = 0
            ,_4KB  = 1
            ,_8KB  = 2
            ,_12KB = 3
            ,_16KB = 4
        };

        static UINT32 RamSizeWhenFifoSize
        (
            UINT32 totalDataRamSize
            ,FifoSize dataFifoSize
        );
        static UINT32 FifoSizeInDwords(FifoSize size);

        DataRam(Channel* pCh, UINT32 totalRamSize);

        RC SetDataFifoSize(FifoSize size);
        RC Fill
        (
            GpuSubdevice* pSubdev
            ,const vector<UINT32>& dataRamValues
            ,FLOAT64 timeoutMs
        );
        RC Copy(GpuSubdevice* pSubdev, Surface2D* pBuf, FLOAT64 timeoutMs);

        UINT32 GetTotalRamSize() const;
        UINT32 GetRamSize() const;
        UINT32 GetFifoSize() const;

    private:
        Channel* m_pCh;          //!< 3d engine channel containing the MME.
        UINT32 m_TotalRamSize;   //!< Size of the total data ram unit in DWORDs.

        //! Size of the Data RAM dedicated to the Data FIFO.
        FifoSize m_FifoSize;

        RC AllocRamSizedSurface(GpuSubdevice* pSubdev, Surface2D* pSurf) const;
    };

    //!
    //! \brief MME64 hardware unit manager.
    //!
    class Manager
    {
    public:
        Manager
        (
            GpuTest* pGpuTest
            ,GpuTestConfiguration* pTestConfig
            ,unique_ptr<ThreeDAlloc> pThreeDAlloc
        );

        RC Setup();
        RC Cleanup();
        RC LoadMacro(Macro* pMacro);
        RC FreeMacro(Macro* pMacro);
        RC FreeAll();
        RC RunMacro(const Macro& macro, VirtualAddr* pOutOutputAddr);
        RC SetDataFifoSize(DataRam::FifoSize size);
        RC FillDataRam(const vector<UINT32>& dataRamValues);
        RC CopyDataRam(Surface2D* pBuf);
        RC WaitIdle();

        UINT32 GetUsableDataRamSize() const;
        UINT32 GetDataFifoSize() const;
        UINT32 GetInstrRamSize() const;
        UINT32 GetMaxNumMacros() const;

        void SetDebugPrintPri(Tee::Priority pri);

    private:
        //!
        //! \brief Inline-to-memory (I2M) configuration setting for MME's
        //! inline-to-memory mode.
        //!
        struct I2mConfig
        {
            /* Destination parameters */
            UINT64 outputBufDmaOffsetGpu; //!< Output buffer GPU DMA offset
            UINT32 pitchOut;              //!< Delta in bytes between the same
                                          //!< index in two adjacent lines in
                                          //!< the output buf.

            /* Transfer parameters */
            UINT32 lineLengthIn; //!< Size in bytes of one line in output buf.
            UINT32 lineCount;    //!< Number of lines of data to transfer. If 0,
                                 //!< no data is transferred.

            I2mConfig() : I2mConfig(0, 0, 0, 0) {}
            I2mConfig
            (
                UINT64 outputBufDmaOffsetGpu
                ,UINT32 pitchOut
                ,UINT32 lineLengthIn
                ,UINT32 lineCount
            );

            RC Apply(Channel* pCh);
            void Print(Tee::Priority printLevel);
        };

        GpuTest*                m_pGpuTest;
        GpuSubdevice*           m_pSubdev;
        GpuTestConfiguration*   m_pGpuTestConfig;
        //FancyPicker::FpContext* m_pFpCtx;

        /* DMA Channel */
        unique_ptr<ThreeDAlloc> m_pThreeDAlloc;   //!< 3D channel class allocater
        Channel*                m_pCh;            //!< DMA Channel
        UINT32                  m_hCh;            //!< Handle for DMA Channel
        AtomChannelWrapper*     m_pAtomChWrapper; //!< Atomic method group wrapper for channel
        Surface2D*              m_pI2mBuf;        //!< I2M framebuf memory
        NotifySemaphore         m_3dSemaphore;    //!< 3d Engine DMA channel notifier
        UINT32                  m_SemaPayload;    //!< Next semaphore payload value

        //! True if the MME is sending output over inline-to-mem (I2M), false
        //! if it is outputting to the graphics pipeline.
        bool   m_OutputToInlineToMemoryEnabled;

        unique_ptr<MacroLoader> m_pMacroLoader; //!< Loads macros into the MME64
        unique_ptr<DataRam> m_pDataRam;         //!< MME Data RAM

        RC SetOutputToInlineToMemory(bool enable);
        RC OpenI2mTransfer
        (
            UINT32 numEmissions
            ,I2mConfig* const pOutI2mConfig
        );
        RC SendI2mTransactionPadding
        (
            UINT32 numEmissions
            ,const I2mConfig& i2mConfig
        );
    };
};
