/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef MODS_RM_CLK_UCT_PSTATE_H_
#define MODS_RM_CLK_UCT_PSTATE_H_

#include <string>
#include <sstream>

#include "ctrl/ctrl2080/ctrl2080perf.h"

#include "gpu/tests/rm/utility/rmtmemory.h"
#include "gpu/tests/rm/utility/rmtarray.h"

#include "uctdomain.h"
#include "uctoperator.h"
#include "uctfield.h"

namespace uct
{
    struct PState;
    class  FullPState;
    struct VbiosPState;
    struct VbiosPStateArray;

    //!
    //! \brief      Translate Between CLK v. PERF RMAPI Flags
    //! \see        ctrl/ctrl2080/ctrl2080perf.h
    //! \see        ctrl/ctrl2080/ctrl2080clk.h
    //!
    //! \details    The CLK RMAPI flags are defined by LW2080_CTRL_CLK_INFO_FLAGS.
    //!             The PERF RMAPI flags are defined by LW2080_CTRL_PERF_CLK_DOM_INFO_FLAGS.
    //!             The only two flags that are in common between the two APIs,
    //!             (therefore supperted herein) are _FORCE_PLL and _FORCE_BYPASS.
    //!
    struct ApiFlagMap
    {
        //! \brief      Translate CLK to PERF RMAPI flags
        static UINT32 translateClk2Perf(UINT32 clk);

        //! \brief      Translate PERF to CLK RMAPI flags
        static UINT32 translatePerf2Clk(UINT32 perf);

        //! \brief      Supported flags
        static const UINT32 clkFlagSet;
    };

    struct PState
    {
        //! \brief      Frequency in KHz per LW2080_CTRL_CLK_INFO
        typedef NumericOperator::value_t FreqValue;             // UINT32

        //! \brief      Clock signal source per LW2080_CTRL_CLK_SOURCE_xxx
        typedef UINT32 SourceValue;

        //! \brief      Flag bitmap per LW2080_CTRL_CLK_INFO_FLAGS_FORCE_xxx
        typedef FlagOperator::Bitmap FlagBitmap;                // UINT32

        //! \brief      Voltage in microvolts per LW2080_CTRL_PERF_VOLT_DOM_INFO
        typedef NumericOperator::value_t VoltValue;             // UINT32

        //! \brief      Frequencies per clock domain
        FreqValue       freq[FreqDomain_Count];

        //! \brief      Clock signal source per clock domain
        SourceValue     source[FreqDomain_Count];

        //!
        //! \brief      Flags per clock domain
        //! \see        uct::PartialPState::FreqField::flagNameMap
        //! \see        uct::ApiFlagMap::translateClk2Perf
        //!
        //! \note       Although this class does not dictate the definitions of
        //!             these flag bits, by convention they are the ones defined
        //!             in LW2080_CTRL_CLK_INFO_FLAGS per PartialPState::FreqField,
        //!             not LW2080_CTRL_PERF_CLK_DOM_INFO_FLAGS.
        //!
        FlagBitmap      flag[FreqDomain_Count];

        //! \brief      Voltages per voltage domain
        VoltValue       volt[VoltDomain_Count];

        //! \brief      Construction of invalid p-state
        inline PState()
        {
            ilwalidate();
        }

        //! \brief      Zero out everything in the p-state
        void ilwalidate();

        //! \brief      Check if this pstate has source setting applied to it
        bool hasSourceConfig() const;

        //!
        //! \brief      Code reflecting the force flags
        //!
        //! \details    These are the flags (along with the 'source' parameter
        //!             that impact the signal path through the clock schematic.
        //!
        inline rmt::String forceFlagCode(FreqDomain domain) const
        {
            switch (flag[domain] &
                (DRF_DEF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_PLL,    _ENABLE) |
                 DRF_DEF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_BYPASS, _ENABLE) |
                 DRF_DEF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_NAFLL,  _ENABLE)))
            {
            case DRF_DEF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_PLL,    _ENABLE):
                return "PLL";           // PLL

            case DRF_DEF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_BYPASS, _ENABLE):
                return "BYP";           // Bypass (sometimes called OSM path)

            case DRF_DEF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_NAFLL,  _ENABLE):
                return "NAFLL";         //NAFLL

            case(DRF_DEF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_PLL,    _ENABLE) |
                 DRF_DEF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_BYPASS, _ENABLE)):
                return "HYB";           // Hybrid (not yet used)

            default:
                return "---";           // No path flag specified
            }
        }
    };

    //!
    //! \brief          Fully-Defined P-State
    //!
    //! \details        Each object of this class contains all parameters that
    //!                 fully define a p-state through the RMAPI.
    //!
    class FullPState: public PState
    {
    protected:
        //! \brief      VBIOS P-State Number
        typedef UINT08 VbiosPStateIndex;

        //! \brief      Sentinel for an initialized p-state index
        static const VbiosPStateIndex IlwalidVbiosPStateIndex = (VbiosPStateIndex) -1;

        //! \brief      Sentinel for an initialized p-state index
        static const VbiosPStateIndex VbiosPStateCount = BIT_IDX_32(LW2080_CTRL_PERF_PSTATES_MAX) + 1;

    public:
        //! \brief      The index used by init PState
        static const VbiosPStateIndex initPStateIndex = 99;

        //! \brief      The name of the init PState
        static const rmt::String initPStateName;

        //!
        //! \brief      Name of the p-state definition
        //!
        rmt::String name;

        //! \brief      Flag indicating that this VbiosPState is init pstate
        bool bInitPState;

        //! \brief      Flag indicating for Vflwrve Block.
        bool bVfLwrveBlock = false;

        //!
        //! \brief      Location of the p-state definition
        //!
        rmt::FileLocation start;

        //!
        //! \brief      Base VBIOS P-State
        //!
        //! \details    This is the bit index of LW2080_CTRL_PERF_PSTATES_xxx
        //!             constants which is the same as the p-state number itself.
        //!             That is, P0=0, P1=1, etc.
        //!
        VbiosPStateIndex base;

        //! \brief      Construction of invalid p-state
        inline FullPState()
        {
            ilwalidate();
        }

        //! \brief      Mark the p-state as invalid (i.e. null/zero)
        void ilwalidate();

        //! \brief      The VbiosPState is considered valid if it is either a
        //!             fake VbiosPState or if the VbiosPState check passes
        inline bool isValid() const
        {
            return bInitPState ? true
                : base != IlwalidVbiosPStateIndex && base < VbiosPStateCount;
        }

        //! \brief      Prune domains where the mask bit is zero
        inline void prune(FreqDomainBitmap mask)
        {
            FreqDomain d;
            for (d = 0; d < FreqDomain_Count; ++d)
                if ( !(mask & BIT(d)))
                    freq[d] = 0;
        }

        //! \brief      Set of frequency domains set by this p-state
        inline FreqDomainBitmap freqDomainSet() const
        {
            FreqDomain d;
            FreqDomainBitmap db = 0;
            for (d = 0; d < FreqDomain_Count; ++d)
                if (freq[d])
                    db |= BIT(d);
            return db;
        }

        //! \brief      Number of frequency domains set by this p-state
        inline size_t freqDomainCount() const
        {
            FreqDomain d;
            size_t n = 0;
            for (d = 0; d < FreqDomain_Count; ++d)
                if (freq[d])
                    ++n;
            return n;
        }

        //! \brief      Set of voltage domains set by this p-state
        inline VoltDomainBitmap voltDomainSet() const
        {
            VoltDomain d;
            VoltDomainBitmap db = 0;
            for (d = 0; d < VoltDomain_Count; ++d)
                if (volt[d])
                    db |= BIT(d);
            return db;
        }

        //! \brief      Number of voltage domains set by this p-state
        inline size_t voltDomainCount() const
        {
            VoltDomain d;
            size_t n = 0;
            for (d = 0; d < VoltDomain_Count; ++d)
                if (volt[d])
                    ++n;
            return n;
        }

        //!
        //! \brief      Is this a VBIOS p-state?
        //!
        //! \details    If this function returns false, then it indicates that
        //!             this object represents a pseudo-p-state, in other words,
        //!             a p-state that has been modified (via a p-state reference).
        //!
        //!             If it returns true, then this is a VBIOS p-state,
        //!             Specifically, it indicates that the settings for this
        //!             p-state matches the corresponding one in the 'vbios'
        //!             array parameter.
        //!
        //! \details    The frequencies, voltages, flags, and base are compared;
        //!             but the name is not.
        //!
        bool matches(const VbiosPStateArray &vbios) const;

        //!
        //! \brief      Are the parameters equal?
        //!
        //! \details    The frequencies, voltages, and flags are compared;
        //!             but the name and base p-state are not.
        //!
        bool matches(const FullPState &that) const;

        //! \brief      Human-readable text
        rmt::String string() const;
    };

    //!
    //! \brief      VBIOS-Defined P-State
    //!
    //! \details    Since all VBIOS p-states are fully-defined, this class is an
    //!             alias of 'FullPState'.  It is useful for documentation
    //!             puposes.  Use this class if it is given that the object
    //!             represents an actual p-state defined by the VBIOS and use
    //!             'FullPState' otherwise.
    //!
    struct VbiosPState: public FullPState
    {
        //! \brief      VBIOS P-State Number
        typedef VbiosPStateIndex Index;

        //! \brief      Sentinel for an initialized p-state index
        static const Index invalid = IlwalidVbiosPStateIndex;

        //! \brief      Number of VBIOS P-States
        static const Index count = VbiosPStateCount;
    };

    //!
    //! \brief      Array of VBIOS P-States
    //!
    //! \details    An object of this type is initialized during the initialization
    //!             of the RM test from reading the VBIOS via these RMAPI points:
    //!                 LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO
    //!                 LW2080_CTRL_CMD_PERF_GET_PSTATE2_INFO
    //!
    struct VbiosPStateArray: public rmt::Array<VbiosPState, VbiosPState::count>
    {
        //! \brief      Index used int this array
        typedef VbiosPState::Index Index;

        //! \brief      Number of VBIOS P-States
        static const Index count = VbiosPState::count;

        //! \brief      A init p-state configured to the current clock
        //!             settings at the start of UCT
        VbiosPState initPState;

        //! \brief      Is there an initialized p-state at this location?
        inline bool isValid(Index i) const
        {
            return rmt::Array<VbiosPState, VbiosPState::count>::isValid(i) &&
                operator[](i).base == i;
        }

        //!
        //! \brief      Highest P-State (usually P0)
        //!
        //! \retval     NULL        P-State array is empty
        //!
        inline const VbiosPState *highest() const
        {
            Index i;
            for (i = 0; i < count; ++i)
                if (operator[](i).base == i)
                    return operator+(i);
            return NULL;
        }

        //! \brief      Human-Readable Representation
        inline rmt::String debugString() const
        {
            rmt::String s;
            Index i;

            // Print out the init PState first
            s += initPState.string() + "\n";

            // Print out all the vbios pstates
            for (i = 0; i < count; ++i)
                if (isValid(i))
                    s += operator[](i).string() + "\n";
            return s.trimq();
        }
    };
};

#endif /* MODS_RM_CLK_UCT_PSTATE_H_ */

