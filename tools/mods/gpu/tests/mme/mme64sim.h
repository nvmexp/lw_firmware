/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file mme64sim.h
//! \brief Interface between the MME6 Simulator code and MODS.
//!
//! There is an expectation that there will be external changes to the simulator
//! interface. This file should act as the barrier between MODS usage code and
//! the simulator interface.
//!
//! Try not to leak simulator types into MODS, if possible.
//!

#pragma once

#include "core/include/massert.h"
#include "core/include/rc.h"
#include "mme64igen.h"
#include "lwmme/parser/parse.h"  // mmeError, mmeWarning, mmeTrace, parse
#include "lwmme/playback/mme2.h" // MME64 simulator interface

#include <vector>

namespace Mme64
{
    typedef void (*simulatorCallback)(void*, unsigned int, unsigned int);

    //!
    //! \brief Wrapper to the MME64 Simulator written by the hardware team.
    //!
    class Simulator
    {
    public:
        static constexpr int RUN_ERROR = -1;    //!< Simulator failed to run

        //!
        //! \brief Wrapper around the simulator's memory format.
        //!
        //! The simulator expects memory location values in an array of the
        //! form [addr, value, addr, value, ...] where i is the memory
        //! address and i+1 is the value at address i for all even i.
        //!
        class Memory
        {
        public:
            using Address = UINT32;
            using Value = UINT32;

            Memory() = default;
            Memory(UINT32 startAddr, UINT32 numEntries);
            Memory(Memory&& o);
            Memory(const Memory& o);

            void Set(Address addr, Value value);
            std::pair<Address, Value> GetEntry(UINT32 entryIndex) const;
            void SetEntry(UINT32 entryIndex, Address addr, Value value);
            void Clear();
            void Reserve(UINT32 numEntries);
            bool Empty() const;
            UINT32 GetNumEntries() const;

            unsigned int* GetSimArray();
            int GetSimArraySize() const;

            Memory& operator=(Memory&& o);
            Memory& operator=(const Memory& o);

        private:
            vector<unsigned int> m_Memory; //!< Simulator formatted array

            UINT32 GetSimMemArraySize(UINT32 numEntries) const;
        };

        struct RunConfiguration
        {
            //! True to enable verbose simulator tracing.
            bool traceEnabled;

            //! True if methods emitted by the MME should not be simulated.
            //!
            //! By default, emitted MME methods are exelwted and consumed rather
            //! than released. Disabling the memory unit has the same behaviour
            //! as the MME while in M2M Inline mode.
            bool disableMemoryUnit;

            //! Simulator callback function.
            //!
            //! Triggered for every emission.
            //!
            //! Parameters are:
            //! void*        callbackData  - RunConfiguration::callbackData
            //! unsigned int method        - Output method. Includes the method
            //!                              address, but not the method inc.
            //!                              Only bits [11:0] are populated.
            //! unsigned int data          - Output emit.
            simulatorCallback callback;

            //! Data passed to the first void* parameter of the simulator
            //! callback.
            void* pCallbackData;

            //! Address/data pairs of methods stored in the shadow RAM.
            Memory* pShadowRamState;

            //! Address/data pairs of data stored in the data RAM.
            Memory* pDataRamState;

            //! Address/data pairs of data stored in the GPU's memory accessed
            //! by the DMA unit.
            Memory* pMemoryState;
        };

        Simulator() = default;

        RC Run(Macro* const pMacro, RunConfiguration& config);
    };
};
