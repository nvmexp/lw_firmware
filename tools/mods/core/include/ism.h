/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_ISM_H
#define INCLUDED_ISM_H

#ifndef INCLUDED_TYPES_H
#include "types.h"
#endif

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif

#ifndef MASSERT_H__
#include "massert.h"
#endif

#ifndef INCLUDED_STL_VECTOR
#define INCLUDED_STL_VECTOR
#include <vector>
#endif

#ifndef INCLUDED_STL_SET
#define INCLUDED_STL_SET
#include <set>
#endif

#ifndef INCLUDED_STL_MAP
#define INCLUDED_STL_MAP
#include <map>
#endif

#ifndef INCLUDED_ISM_SPEEDO_H
#include "ismspeedo.h"
#endif

#include <bitset>

using namespace IsmSpeedo;

struct JSContext;
struct JSObject;

//------------------------------------------------------------------------------
// Base class for implementing access to the In-Silicon Measurement (ISM)macros.
// The following text is copied from the dev_ism.ref manual.

/*******************************************************************************
The In-Silicon Measurement (ISM) team is dedicated to measuring process, voltage,
temperature, jitter, and other noise.  Our cirlwits are sprinkled throughout the
chip.  They are accessible through Jtag.  The purpose of this document is to
describe the cirlwits and their Jtag chains so that software can access them
through host2jtag.

There are several types of ISM cirlwits.  Each type is described in Chapter 2.
The types are generally static across many chips.  So a MINI_1CLK has the same
interface and implementation in GF117 as it does in Kepler.  As a result, Chapter
2 will rarely change.

ISMs are placed on scan chains for Jtag access.  Each chain is described in
Chapter 3.  The chains tend to be different on every chip.  Familial chips,
such as GK104 and GK107 MAY share SOME chains, but you should expect the Chapter
3 descriptions to change frequently.

Each ISM contains a number of ring oscillators.  We may change the RO types
from chip to chip.  To avoid affecting the SW manuals, we have chosen not to
include RO descriptions or enumerations in this document.

The ISM in itself is a small module, but it interacts with other important
modules which the user needs to understand in order to completely obtain and
understand the ISM data.

ISMS use the host2jtag access for reading the data from the outputs. The Jtag
serial inteface controls all the test and debug resgiters on the Lwpu chip.
It is desirable to read/write these serial Jtag registers through the 32 bit
GPU Private Register Interface (PRI) bus.

For more information on host2jtag access please refer to the wiki at
https://wiki.lwpu.com/engwiki/index.php/Hwcad/GenPads/TestBus#Host2Jtag_access

*******************************************************************************/
class Ism
{
public:
    //--------------------------------------------------------------------------
    // Data Types
    //--------------------------------------------------------------------------
    enum IsmTypes {
        // Internal ISM Type Enumeration:
         LW_ISM_none                        = 0
        ,LW_ISM_ctrl                        = 1
        ,LW_ISM_MINI_1clk                   = 2
        ,LW_ISM_MINI_2clk                   = 3
        ,LW_ISM_MINI_1clk_dbg               = 4 //obsolete
        ,LW_ISM_ROSC_comp                   = 5
        ,LW_ISM_ROSC_bin                    = 6 // Kepler & Fermi are different
        ,LW_ISM_ROSC_bin_metal              = 7
        ,LW_ISM_VNSENSE_ddly                = 8
        ,LW_ISM_TSOSC_a                     = 9
        ,LW_ISM_MINI_1clk_no_ext_ro         = 10
        ,LW_ISM_MINI_2clk_dbg               = 11 // GK10x
        ,LW_ISM_MINI_1clk_no_ext_ro_dbg     = 12
        ,LW_ISM_MINI_2clk_no_ext_ro         = 13 // GMxxx
        ,LW_ISM_NMEAS_v2                    = 14 // GM2xx and newer
        ,LW_ISM_HOLD                        = 15 // T210 and newer
        ,LW_ISM_ctrl_v2                     = 16 // Pascal GP100+
        ,LW_ISM_NMEAS_v3                    = 17 // Pascal GP100+
        ,LW_ISM_ROSC_bin_aging              = 18 // Pascal GP100+
        ,LW_ISM_TSOSC_a2                    = 19 // Pascal GP100+
        ,LW_ISM_ctrl_v3                     = 20 // Pascal GP10x+
        ,LW_ISM_aging                       = 21 // Pascal GP10x+
        ,LW_ISM_MINI_2clk_v2                = 22 // Pascal GP10x+, T186 and newer
        ,LW_ISM_MINI_2clk_dbg_v2            = 23 // GP107, GP108
        ,LW_ISM_TSOSC_a3                    = 24 // Volta GV100+, TU102+
        ,LW_ISM_ROSC_bin_aging_v1           = 25 // Volta GV100+
        ,LW_ISM_NMEAS_lite                  = 26 // Volta GV100+
        ,LW_ISM_ctrl_v4                     = 27 // Volta GV100+, TU102+
        ,LW_ISM_aging_v1                    = 28 // Volta GV100+
        ,LW_ISM_cpm                         = 29 // Volta GV100+
        ,LW_ISM_ROSC_bin_v1                 = 30 // T214
        ,LW_ISM_MINI_2clk_dbg_v3            = 31 // T194
        ,LW_ISM_NMEAS_v4                    = 32 // T194, GV100+, this is obsolete,
        ,LW_ISM_NMEAS_lite_v2               = 33 // T194, TU102+
        ,LW_ISM_aging_v2                    = 34 // T194
        ,LW_ISM_cpm_v1                      = 35 // T194, TU102+
        ,LW_ISM_aging_v3                    = 36 // TU102+
        ,LW_ISM_aging_v4                    = 37 // TU102+
        ,LW_ISM_TSOSC_a4                    = 38 // TU102+
        ,LW_ISM_NMEAS_lite_v3               = 39 // GA100+
        ,LW_ISM_ROSC_bin_v2                 = 40 // GA100+
        ,LW_ISM_ROSC_comp_v1                = 41 // GA100+
        ,LW_ISM_cpm_v2                      = 42 // GA100+
        ,LW_ISM_NMEAS_lite_v4               = 43 // GA10X+
        ,LW_ISM_NMEAS_lite_v5               = 44 // GA10X+
        ,LW_ISM_NMEAS_lite_v6               = 45 // GA10X+
        ,LW_ISM_NMEAS_lite_v7               = 46 // GA10X+
        ,LW_ISM_cpm_v3                      = 47 // GA10X+
        ,LW_ISM_ROSC_bin_v3                 = 48 // T234_
        ,LW_ISM_cpm_v4                      = 49 // T234_ on the GPU
        ,LW_ISM_cpm_v5                      = 50 // T234_ on the CPU
        ,LW_ISM_ROSC_bin_v4                 = 51 // GH100+
        ,LW_ISM_ROSC_comp_v2                = 52 // GH100+
        ,LW_ISM_NMEAS_lite_v8               = 53 // GH100+
        ,LW_ISM_NMEAS_lite_v9               = 54 // GH100+
        ,LW_ISM_imon                        = 55 // GH100+
        ,LW_ISM_cpm_v6                      = 56 // GH100+
        ,LW_ISM_imon_v2                     = 57 // AD10x+
        ,LW_ISM_lwrrent_sink                = 58 // AD10x+
        ,LW_ISM_unknown  // Add new types above this one.
    };
    enum IsmFlags
    {
        FLAGS_NONE  = 0,
        // Keep this ISM active after running an experiment or reading values.
        KEEP_ACTIVE = (1<<0), 

        // On Hopper+ the GPU has multiple ISM controllers for running experiments on a 
        // sub-partition of the GPU and a single primary ISM controller for running on all
        // partitions. This flag identifies the "primary" controller, and there should only be one
        // in the system.
        PRIMARY = (1<<1), 

        // Since every ISM has different requirements per state,
        // defining these generic state values (bits 31:28).
        // We are planning to move to a state machine later (RFE).
        STATE_1     = (1<<27),
        STATE_2     = (2<<27),
        STATE_3     = (3<<27),
        STATE_4     = (4<<27),
        STATE_MASK  = (0xF<<27)
    };
    enum IsmChainFlags
    {
        NORMAL_CHAIN   = (1<<0),   // Chain is a Normal Chain.
        SLOW_CPU_CHAIN = (1<<1),   // Chain is a slow CPU Chain (CheetAh only).
        FAST_CPU_CHAIN = (1<<2),   // Chain is a fast CPU Chain (CheetAh only).
        CHAIN_SUBTYPE_MASK = (NORMAL_CHAIN | SLOW_CPU_CHAIN | FAST_CPU_CHAIN)
    };
    enum IsmCtrlTrigger
    {
        LW_ISM_CTRL_TRIGGER_off = 0, // no trigger
        LW_ISM_CTRL_TRIGGER_pri = 1, // trigger on a PRIV write
        LW_ISM_CTRL_TRIGGER_pmm = 2, // trigger on a PerfMon trigger
        LW_ISM_CTRL_TRIGGER_now = 3, // trigger now
    };

    // Structure holding the basic information to identify the size and type of
    // a speficic In-Silicon Measurement(ISM) circuit.
    struct IsmRef
    {
        IsmTypes Type;  // the type of ISM macro see dev_ism_ref.h for details
        INT32 Offset;   // bit offset within the chain
        INT08 Flags;    // bit 0: keep active (iddq is never powered off)
                        // bit 1: set if its the primary controller
                        // bits 7:2 reserved for future use
        IsmRef():Type(LW_ISM_none), Offset(0), Flags(0){}
        IsmRef(IsmTypes t, INT32 o):Type(t), Offset(o), Flags(0){}
        IsmRef(IsmTypes t, INT08 f, INT32 o): Type(t), Offset(o), Flags(f) {}
    };

    // Structure for configuring special noise measurement chains in GM20x parts.
    struct NoiseMeas2Param {     //Bits in the macro
        UINT08  tap;               //  5:1 //TODO: break this down to tapSel & tapDelay
        UINT08  mode;              // 10:8
        UINT08  outSel;            // 14:11
        UINT08  thresh;            // 20:16
        UINT16  trimInit;          // 30:21
        UINT16  measLen;           // 46:31
    };

    struct NoiseMeas3Param {    // Bits in the macro
        UINT08  hmDiv;          //  3:1
        UINT08  hmFineDlySel;   //  5:4
        UINT08  hmTapDlySel;    //  7:6
        UINT08  hmTapSel;       // 10:8
        UINT08  hmTrim;         // 19:11
        UINT08  lockPt;         // 25:21
        UINT08  mode;           // 28:26
        UINT08  outSel;         // 31:29
        UINT08  outClamp;       // 32:32
        UINT08  sat;            // 33:33
        UINT08  thresh;         // 38:34
        UINT16  winLen;         // 49:39
        UINT16  hmAvgWin;       // 53:50
        UINT08  fdUpdateOff;    // 54:54
    };
    struct NoiseMeasParam {
        UINT08 version;
        union {
            struct NoiseMeas2Param nm2;
            struct NoiseMeas3Param nm3;
        };
    };
    enum PollHoldMode
    {
        POLL_HOLD_AUTO,
        POLL_HOLD_MANUAL,
        POLL_HOLD_OSC,
        POLL_HOLD_READY
    };

    // Enumerations for the different programming stages of the noise mesasurement circuit
    enum NoiseMeasStage {
         nmsSTAGE_1     = 0x01
        ,nmsSTAGE_2     = 0x02
        ,nmsSTAGE_3     = 0x04
        ,nmsSTAGE_4     = 0x08
        ,nmsSTAGE_5     = 0x10
        ,nmsSTAGE_6     = 0x20
        ,nmsSTAGE_ALL   = 0x3f
    };

    //Structure holding the information necessary to identify a chain of ISMs
    struct IsmChainHdr {
        UINT32 InstrId;  // Instruction id used to access this chain via JTag
        UINT32 Size;     // Total number of bits in this chain
        UINT32 Chiplet;  // The chiplet this chain is located
        UINT32 Flags;    // Flags for chain special handling
        UINT32 SfeReg;   // Selects the specific NMEAS soft macro in a chain
        UINT32 VstCtlReg; // Selects the VstCtl index
        UINT32 VstDataReg; // Register to read the VstData
        bool operator <(const IsmChainHdr &rhs) const
        {
            // Cannot have the same chain multiple times with different flags
            if (InstrId != rhs.InstrId)
                return (InstrId < rhs.InstrId);
            if (Size != rhs.Size)
                return (Size < rhs.Size);
            return (Chiplet < rhs.Chiplet);
        }
        bool operator==(const IsmChainHdr &rhs) const
        {
            // Cannot have the same chain multiple times with different flags
            return ( InstrId == rhs.InstrId &&
                     Size == rhs.Size &&
                     Chiplet == rhs.Chiplet);
        }
    };
    typedef set<IsmChainHdr> ChainHdrSet;
    typedef ChainHdrSet::iterator ChainHdrSetItr;

    struct IsmChain
    {
        IsmChainHdr Hdr;
        vector<IsmRef> ISMs; // list of ISMs in this chain.

        IsmChain(UINT32 id, UINT32 s, UINT32 c, UINT32 f,UINT32 sfe, UINT32 vstc,
                  UINT32 vstd)
        {
            Hdr.InstrId= id;
            Hdr.Size= s;
            Hdr.Chiplet= c;
            Hdr.Flags= f;
            Hdr.SfeReg= sfe;
            Hdr.VstCtlReg= vstc;
            Hdr.VstDataReg= vstd;
        }

    };

    // Structure for describing the type of ISMs to filter.
    enum {
        maxISMsPerChain = 128 // lwrrently sized to 2x what GP100 has.
    };
    struct IsmChainFilter
    {
        const IsmChain * pChain;            // pointer to the chain
        UINT32     type;                    // type of ISM to filter
        bitset<maxISMsPerChain> ismMask;    // a mask of ISMs in the chain that match 'type'
        IsmChainFilter(const IsmChain *pIsmChain,UINT32 ismType,bitset<maxISMsPerChain>& rIsmMask):
            pChain(pIsmChain), type(ismType)
        {
            ismMask = rIsmMask;
        }
    };

    // On Hopper we have a single PRIMARY ISM controller and several ISM sub-controllers. Each
    // sub-controller will run experiments on a limited set of LW_ISM_NMEAS_lite_v* ISMs.
    // However the hardware team was not willing to put forth the effort to create unique tags
    // in the #defines for each ISM so MODS can create a link between the sub-controller and the 
    // LW_ISM_NMEAS_lite_v* ISMs it controls. So....
    // The solution is to cache the last PRIMARY or sub-controller that was setup and use that to
    // poll for completion of the experiments. 
    // The new wrinkle is that there may be multiple sub-controllers on a single chain. If so
    // we will poll all of them and return the ANDED results. Ie if there are 3 sub-controllers,
    // we return true only if all 3 have completed the experiment, false otherwise.
    const IsmChain * m_pCachedISMControlChain = nullptr;

    //--------------------------------------------------------------------------
    // APIs
    //--------------------------------------------------------------------------
    // Create the appropriate GpuIsm subclass based on the Gpu DeviceId.
    Ism(vector<IsmChain> *pTable);
    virtual ~Ism(){};

    static const char * IsmEnumToString(IsmTypes ism);
    IsmSpeedo::PART_TYPES GetPartType(IsmTypes ismType);

    RC CopyBits
    (
        const IsmChain *pChain      //!< the chain configuration
        ,vector<UINT32> &destArray  //!< Vector of raw bits for this chain.
        ,const vector<UINT64> &srcArray   //!< Vector of raw bits for this ISM type
        ,const bitset<maxISMsPerChain>& ismMask             //!< a mask of the ISMs you want to powerup.
    );

    virtual UINT32 GetDelayTimeUs
    (
        const IsmChain * pChain,    //!< the chain configuration being programmed
        UINT32 durClkCycles         //!< Jtag clock cycles to run the experiment.
    );

    // -------------------------------------------------------------------------
    // These APIs must be implemented by the CheetAh or Gpu subclass.
    // -------------------------------------------------------------------------
    virtual RC GetISMClkFreqKhz
    (
        UINT32* clkSrcFreqKHz
    )=0;

    //This API is required to read the HOLD isms and callwlate the proper T0/T1 OSC time
    // in pico-seconds. It's lwrrently only implemented on CheetAh SOCs
    virtual RC GetPLLXPeriodPs
    (
        UINT64* pllXPeriodPs
    ) 
    { return RC::UNSUPPORTED_FUNCTION; }
    
    // Return the number of bit for the Ism Type
    virtual UINT32 GetISMSize
    (
        IsmTypes ismType
    ) = 0;

    // Perform a single poll of the Control ISM for complete status. Update
    // pComplete with the LW_ISM_CTRL_DONE status.
    virtual RC PollISMCtrlComplete
    (
        UINT32 *pComplete
    ) = 0;

    // Return the value of the LW_ISM_CTRL*_DONE (chip specific) bit
    virtual UINT32 IsExpComplete
    (
        UINT32 ismType, //Ism::IsmTypes
        INT32 offset,
        const vector<UINT32> &JtagArray
    ) = 0;

    // Read ChainLength bits from a JTag stream using Chiplet & Instruction to
    // select the specific JTag stream.
    virtual RC ReadHost2Jtag
    (
        UINT32 chiplet,
        UINT32 instruction,
        UINT32 chainLength,
        vector<UINT32> *pResult
    )=0;

    virtual void GetKeepActiveMask(bitset<Ism::maxISMsPerChain>* pActiveMask, const IsmChain * pChain);
    // If bKeepActive = true then the iddq bit will not be set to power down the ISM
    virtual RC SetKeepActive(UINT32 instrId, UINT32 chiplet, INT32 offset, bool bKeepActive);

    // Trigger an ISM experiment when the ISM trigger source = priv write.
    virtual RC TriggerISMExperiment()=0;

    // Write ChainLength bits to a JTag stream using Chiplet & Instruction to
    // select the specific JTag stream.
    virtual RC WriteHost2Jtag
    (
        UINT32 chiplet,
        UINT32 instruction,
        UINT32 chainLength,
        const vector<UINT32> &inputData
    )=0;

    UINT32 GetIsmJsObjectVersion(IsmTypes type);

protected:
    virtual RC ConfigureISMBits
    (
        vector<UINT64> &IsmBits // vector to the stream of bits.
        ,IsmInfo &info          // specific ISM info. that needs configuration.
        ,IsmTypes ismType       // what type of ISM we are configuring,
                                // BIN/COMP/METAL/MINI/etc
        ,UINT32 iddq            // 0 = powerUp, 1 = powerDown
        ,UINT32 enable          // 0 = disable, 1 = enable
        ,UINT32 durClkCycles    // how long to run the experiment
        ,UINT32 flags           // Special handling flags
    )=0;

    // extract the 'raw count' bits from the ISM bit stream
    virtual UINT64 ExtractISMRawCount(UINT32 ismType, vector<UINT64>& ismBits)=0;

    // extract the 'raw DCD count' for AGING macros from the ISM bit stream.
    virtual UINT32 ExtractISMRawDcdCount(UINT32 ismType, vector<UINT64>& ismBits) { return 0; }

    // Dump ascii representation of the ISM bits.
    virtual RC DumpISMBits(UINT32 ismType, vector<UINT64>& ismBits){return OK;}
private:
    virtual RC ConfigureISMCtrl
    (
        const IsmChain *pChain          //The chain with the primary controller
        ,vector<UINT32>  &jtagArray     //vector of bits for the chain
        ,bool            bEnable        //enable/disable the controller
        ,UINT32          durClkCycles   // how long to run the experiment
        ,UINT32          delayClkCycles // how long to wait after the trigger
                                        // before taking samples.
        ,UINT32          trigger        // type of trigger to use.
        ,INT32           loopMode       // if true allows continuous looping of the experiments
        ,INT32           haltMode       // if true allows halting of a continuous experiments
        ,INT32           ctrlSrcSel     // 1 = jtag, 0 = priv access
    )=0;

    virtual UINT32           GetISMDurClkCycles() = 0;
    virtual INT32            GetISMMode(PART_TYPES type) = 0;
    virtual INT32            GetISMOutputDivider(PART_TYPES partType)=0;
    virtual INT32            GetISMOutDivForFreqCalc(PART_TYPES partType, INT32 outDiv)=0;
    virtual INT32            GetISMRefClkSel(PART_TYPES partType)=0;
    virtual INT32            GetOscIdx(PART_TYPES type)=0;

public:
    // -------------------------------------------------------------------------
    // Default implementations
    // -------------------------------------------------------------------------
    // Return the minimum size a vector needs to be to contain all of the bits 
    // for a given IsmType.
    inline UINT32 CallwlateVectorSize(IsmTypes ismType, UINT32 sizeTypeInBits)
    {
        return (GetISMSize(ismType) + sizeTypeInBits - 1) / sizeTypeInBits;
    }
    // Return the number of ISMs for a given part type
    virtual UINT32 GetNumISMs
    (
        PART_TYPES partType
    );

    // Return the number of ISM chains in the global ISM table.
    int GetNumISMChains()
    {
        return static_cast<int>(m_Table.size());
    }

    // Global enable for all ISMs (nothing necessary on dGPU)
    virtual RC EnableISMs(bool bEnable) { return OK; }
    virtual bool GetISMsEnabled() { return true; }

    //! \brief Read speedometers from an ISM Jtag chain.
    //!
    //! Reads all the specified oscillators of a given type from the ISM jtag
    //! chains. There are multiple ISM cirlwits connected throughout the part
    //! using jtag chains. Each chain may have 1 or more ISMs. This method will
    //! read all of the chains that have the specified speedo type of oscillator
    //! (COMP/BIN/METAL/MINI/etc) and return their values in pSpeedos
    virtual RC ReadISMSpeedometers
    (
        vector<UINT32>* pSpeedos    //!< (out) measured ring cycles
        ,vector<IsmInfo>* pInfo     //!< (out) uniquely identifies each ISM.
        ,PART_TYPES speedoType      //!< (in) what type of speedo to read
        ,vector<IsmInfo>*pSettings  //!< (in) Index of ring oscillators.
        ,UINT32 flags               //!< (in) flags for chains to process
        ,UINT32 durationClkCycles   //!< (in)  sample len in cl cycles.
    );
    virtual RC ReadISMSpeedometers
    (
        vector<UINT32>* pSpeedos    //!< (out) measured ring cycles
        ,vector<IsmInfo>* pInfo     //!< (out) uniquely identifies each ISM.
        ,PART_TYPES speedoType      //!< (in) what type of speedo to read
        ,vector<IsmInfo>*pSettings  //!< (in) Index of ring oscillators.
        ,UINT32 durationClkCycles=~0 //!< (in)  sample len in cl cycles.
    );

    // Reads the current MINI ISM counts on all of the chains.
    // Note: The primary controller is not programmed to start any experiements.
    // pSpeedos is an array of raw counts, one count for each MINI ISM read.
    // pInfo is an array of IsmInfo structs that describes the location of each
    // MINI ism. pInfo[0] describes the MINI ISM that refers to the count in
    // pSpeedos[0].
    virtual RC ReadMiniISMs
    (
        vector<UINT32> *pSpeedos
        ,vector<IsmInfo> *pInfo
        ,UINT32 chainflags      // Chain flags
        ,UINT32 expflags        // Experiment flags
    );

    // Reads the current MINI ISM counts on one chain.
    // Note: The master controller is not programmed to start any experiements.
    // pSpeedos is an array of raw counts, one count for each MINI ISM read.
    // pInfo is an array of IsmInfo structs that describes the location of each
    // MINI ism. pInfo[0] describes the MINI ISM that refers to the count in
    // pSpeedos[0].
    virtual RC ReadMiniISMsOnChain
    (
        INT32 chainId
        ,INT32 chiplet
        ,vector<UINT32> *pSpeedos
        ,vector<IsmInfo> *pInfo
        ,UINT32 expFlags
    );

    // This is only supported on GV100+
    // Reads the current NMEAS ISM counts on all of the chains.
    virtual RC ReadNoiseMeasLite
    (
        vector<UINT32> *pSpeedos
        ,vector<NoiseInfo> *pInfo
    );

    // This is only supported on GV100+
    // Reads the current NMEAS ISM counts on one chain.
    virtual RC ReadNoiseMeasLiteOnChain
    (
        INT32 chainId
        ,INT32 chiplet
        ,vector<UINT32> *pSpeedos
        ,vector<NoiseInfo> *pInfo
        ,bool bPollExperiment
    );

    // Setup the primary ISM controller to run an experiment
    // The experiment will run for dur clock cycles, the ISMs will start
    // counting after delay clock cycles from the trigger.
    virtual RC SetupISMController
    (
        UINT32 dur          // number of clock cycles to run the experiment
        ,UINT32 delay       // number of clock cycles to delay after the trigger
                            // before the ROs start counting.
        ,INT32 trigger      // trigger sourc to use
                            // (0=off, 1=priv write, 2=perf mon trigger, 3=now)
        ,INT32 loopMode     // 1= continue looping the experiment until the haltMode
                            //    has been set to 1.
                            // 0= perform a single experiment
        ,INT32 haltMode     // 1= halt the lwrrently running experiment. Used in
                            //    conjunction with loopMode to stop the repeating the
                            //    same experiment.
                            // 0= don't halt the current experiment.
        ,INT32 ctrlSrcSel   // 1 = jtag 0 = priv access
    );

    // Setup all the ISM sub-controllers to run an experiment
    // The experiment will run for dur clock cycles, the ISMs will start
    // counting after delay clock cycles from the trigger.
    virtual RC SetupISMSubController
    (
        UINT32 dur          // number of clock cycles to run the experiment
        ,UINT32 delay       // number of clock cycles to delay after the trigger
                            // before the ROs start counting.
        ,INT32 trigger      // trigger sourc to use
                            // (0=off, 1=priv write, 2=perf mon trigger, 3=now)
        ,INT32 loopMode     // 1= continue looping the experiment until the haltMode
                            //    has been set to 1.
                            // 0= perform a single experiment
        ,INT32 haltMode     // 1= halt the lwrrently running experiment. Used in
                            //    conjunction with loopMode to stop the repeating the
                            //    same experiment.
                            // 0= don't halt the current experiment.
        ,INT32 ctrlSrcSel   // 1 = jtag 0 = priv access
    );
    
    // Setup all the ISM sub-controllers on a specific chain to run an experiment
    // The experiment will run for dur clock cycles, the ISMs will start
    // counting after delay clock cycles from the trigger.
    virtual RC SetupISMSubControllerOnChain
    (
        INT32 chainId     // The Jtag chain
        ,INT32 chiplet    // The chiplet for this jtag chain.
        ,UINT32 dur       // number of clock cycles to run the experiment
        ,UINT32 delay     // number of clock cycles to delay after the trigger
                          // before the ROs start counting.
        ,INT32 trigger    // trigger sourc to use
                          // (0=off, 1=priv write, 2=perf mon trigger, 3=now)
        ,INT32 loopMode   // 1= continue looping the experiment until the haltMode
                          //    has been set to 1.
                          // 0= perform a single experiment
        ,INT32 haltMode   // 1= halt the lwrrently running experiment. Used in
                          //    conjunction with loopMode to stop the repeating the
                          //    same experiment.
                          // 0= don't halt the current experiment.
        ,INT32 ctrlSrcSel // 1 = jtag 0 = priv access
        ,bool bCacheController = true // true = save off the pChain info for subsequent use.
    );
    
    // Setup all the MINI ISMs on all chains to run an experiment.
    // Refer to dev_ism.ref for detailed information on how to setup a ISM.
    virtual RC SetupMiniISMs (
        INT32 oscIdx
      , INT32 outDiv
      , INT32 mode
      , INT32 init
      , INT32 finit
      , INT32 refClkSel
      , UINT32 chainFlags
    );

    // Setup all the MINI ISMs on a specific chain to run an experiment.
    // Refer to dev_ism.ref for detailed information on how to setup a ISM.
    virtual RC SetupMiniISMsOnChain (
        INT32 chainId
      , INT32 chiplet
      , INT32 oscIdx
      , INT32 outDiv
      , INT32 mode
      , INT32 init
      , INT32 finit
      , INT32 refClkSel
    );

    virtual RC SetupNoiseMeas //!< Setup all NMEAS macros to run an experiment.
    (
        Ism::NoiseMeasParam param      //!< structure with setup information
    );

    virtual RC SetupNoiseMeasLite   //!< Setup all NMEAS macros to run an experiment (Volta+)
    (
        const NoiseInfo& param      //!< structure with setup information
    );

    virtual RC SetupNoiseMeasLiteOnChain    //!< Setup NMEAS macros to run an experiment on a chain
    (
        INT32 chainId
        ,INT32 chiplet
        ,const NoiseInfo& param        //!< structure with setup information
    );

    virtual RC WriteCPMs
    (
        vector<CpmInfo> &params     // structure with setup information
    );


    virtual RC ReadCPMs
    (
        vector<CpmInfo> *pInfo     // structure with CPM information
        ,vector<CpmInfo> &params   // structure with setup information
    );
    
    virtual RC WriteISinks
    (
        vector<ISinkInfo> &params // structure with setup information
    );
    
    virtual RC ReadISinks
    (
         vector<ISinkInfo> *pInfo   // structure with LWRRENT_SINK information
         ,vector<ISinkInfo> &params // structure with setup information
    );

    virtual RC RunNoiseMeas //!< run the noise measurement experiment on all chains
    (
        NoiseMeasStage stage //!< bitmask of the experiment stages to perform.
        ,vector<UINT32>*pValues //!< vector to hold the raw counts
        ,vector<NoiseInfo>*pInfo //!< vector to hold the macro specific information
    );

    virtual RC RunNoiseMeasOnChain //!< run the noise measurement experiment on a specific chain
    (
        INT32 chainId       //!< JTag instruction Id to use
        ,INT32 chiplet       //!< JTag chiplet to use
        ,NoiseMeasStage stage //!< bitmask of the experiment statges to perform
        ,vector<UINT32>*pValues //!< vector to hold the raw counts
        ,vector<NoiseInfo>*pInfo //!< vector to hold the macro specific information
    );

    virtual RC ReadHoldISMs //!< Read all Hold ISMs
    (
        const HoldInfo   & settings  //!< settings for reading the hold ISMs
        ,vector<HoldInfo>* pInfo     //!< vector to hold the macro specific information
    );

    virtual RC ReadHoldISMsOnChain //!< Read Hold ISMs on a specific chain
    (
        INT32               chainId   //!< JTag instruction Id to use
        ,INT32              chiplet   //!< JTag chiplet to use
        ,const HoldInfo   & settings  //!< settings for reading the hold ISMs
        ,vector<HoldInfo> * pInfo     //!< vector to hold the macro specific information
    );
protected:
    virtual const IsmChain * GetISMCtrlChain();

    // Get all the ISM chains that have a specific ISM type
    virtual RC GetISMChains(vector<IsmChainFilter> *pIsmFilter, UINT32 type,
                            INT32 chainId, INT32 chiplet, UINT32 chainFlags);

    // Get all of the ISM chains that have a general ISM type.
    // There may be multiple specific ISM types for a general ISM type.
    RC GetChains
    (
        vector<IsmChainFilter> *pIsmFilter, //!< (out) list of chains
        PART_TYPES speedoType,  //!< (in) SpeedoType to search for
        INT32  chainId,         //!< (in) chain id to search for (-1 for all)
        INT32  chiplet,         //!< (in) chiplet to search for (-1 for all)
        UINT32 chainFlags       //!< (in) flags to use with the chain
    );

private:
    virtual RC ConfigureISMChain
    (
        const IsmChain *pChain
        ,vector<UINT32> &jtagArray              //!< Vector of bits for this chain.
        ,bool       bEnable                     //!< Enable any independant ISM in the chain.
        ,vector<IsmInfo>*pSettings              //!< (in) ISM specific settings to use
        ,const bitset<maxISMsPerChain>& ismMask //!< a mask of the ISMs you want to powerup.
        ,UINT32     durClkCycles                //!< how long you want the experiment to run
        ,UINT32   flags                         //!< Special handling flags
    );

    virtual const IsmChain * GetISMChain(UINT32 chainId);

    virtual RC GetISMChainCounts
    (
        const IsmChain *pChain
        ,vector<UINT32> &jtagArray              //!< Vector of bits for this chain.
        ,vector<UINT32> *pValues                //!< Where to put the counts
        ,vector<IsmInfo>*pInfo                  //!< unique info to identify this ISM.
        ,vector<IsmInfo>*pSettings              //!< (in) ISM specific settings to use
        ,const bitset<maxISMsPerChain>& ismMask //!< a mask of the ISMs count you want.
        ,UINT32     durClkCycles                //!< duration in Jtag Clk cycles
    );

    virtual RC GetISMSettings
    (
        vector<IsmInfo>*pSettings// (in) array of ISM specific settings
        ,const IsmChain *pChain  // (in) point to the chain info.
        ,UINT32 IsmIdx           // (in) index into ISMs[] for ISM to check
        ,IsmInfo *pFinalVals     // (out) location to store the final values.
    );

    RC ReadISMChailwiaJtag
    (
        vector<UINT32> *pValues // Vector of ROSC values to update.
        ,vector<IsmInfo>*pInfo  // unique info to identify this ISM.
        ,const IsmChain *pChain // What chain you want to run the experiment on
        ,vector<IsmInfo>*pSettings //!< (in) ISM specific settings to use
        ,const bitset<maxISMsPerChain>& ismMask // a mask of the ISMs you want to run.
        ,UINT32     durClkCycles// how long you want the experiment to run
    );

    RC ReadMiniISMChainsViaJtag
    (
        vector<UINT32> * pValues   // Vector of ROSC values to update.
        ,vector<IsmInfo>* pInfo    // Unique info to identify this ISM.
        ,vector<IsmChainFilter>*pChainFilter // vector of chains to read
        ,vector<IsmInfo>*pSettings //!< (in) ISM specific settings to use
        ,UINT32     durClkCycles   // how long you want the experiment to run
    );

    virtual bool AllowIsmTrailingBit() { return false; }

    // If true allow unused bits at the start/end/middle of the chain.
    virtual bool AllowIsmUnusedBits() { return false; }

    RC ValidateISMTable();

    // Vector of ISM chains
    vector<IsmChain>& m_Table;

    // ISM table validation
    bool m_TableIsValid;

    // Structures for running ISM experiments
    map<const IsmChainHdr, vector<IsmInfo> > m_IsmLocSettings;
    UINT32 m_IsmLocDurClkCycles;
    // Speedup const bitset
    const bitset<maxISMsPerChain> zeroIsmMask;
};

#endif

