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

#ifndef INCLUDED_GPUISM_H
#define INCLUDED_GPUISM_H

#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

#ifndef MASSERT_H__
#include "core/include/massert.h"
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

#ifndef INCLUDED_ISM_H
#include "core/include/ism.h"
#endif

class RegHal;
class GpuSubdevice;
//------------------------------------------------------------------------------
// Derived class for implementing access to the In-Silicon Measurement(ISM)
// macros on a GPU.
//------------------------------------------------------------------------------
#ifndef INCLUDED_ISM_SPEEDO_H
#include "core/include/ismspeedo.h"
#endif

using namespace IsmSpeedo;

class GpuIsm : public Ism
{
private:
    GpuSubdevice * m_pGpu;
    UINT32 m_ClkSrcFreqKHz = 0;

    // Structure describing the noise measurement circuit version 2
    struct NoiseMeas2 //!< Bits in the macro (version 2)
    {         
        bool    bEnable;        //!<  0:0
        UINT08  tap;            //!<  5:1
        bool    bJtagOverwrite; //!<  6:6
        bool    bMeas;          //!<  7:7
        UINT08  mode;           //!< 10:8
        UINT08  outSel;         //!< 14:11
        bool    bOutClamp;      //!< 15:15
        UINT08  thresh;         //!< 20:16
        UINT16  trimInit;       //!< 30:21
        UINT16  winLen;         //!< 46:31
        UINT32  cnt;            //!< 67:47
        bool    bRdy;           //!< 68:68
    };
    // Structure describing the noise measurement circuit version 3
    struct NoiseMeas3         //!< Bits in the macro
    {
        bool    bEnable;        //!<  0:0
        UINT08  hmDiv;          //!<  3:1  Hard macro divider
        UINT08  hmFineDlySel;   //!<  5:4
        UINT08  hmTapDlySel;    //!<  7:6
        UINT08  hmTapSel;       //!< 10:8
        UINT08  hmTrim;         //!< 19:11
        bool    bJtagOverwrite; //!< 20:20
        UINT08  lockPt;         //!< 25:21 ability to lock at diff points of sample
        UINT08  mode;           //!< 28:26
        UINT08  outSel;         //!< 31:29
        bool    bOutClamp;      //!< 32:32
        bool    bSat;           //!< 33:33 saturate the count
        UINT08  thresh;         //!< 38:34
        UINT16  winLen;         //!< 49:39
        UINT16  hmAvgWin;       //!< 53:50
        bool    fdUpdateOff;    //!< 54:54
        UINT32  cnt;            //!< 78:55
        UINT32  hmOut;          //!<110:79
        bool    bRdy;           //!<111:111
    };
    Ism::NoiseMeasParam m_NoiseParam = {};

public:

    struct PollHoldIsmData
    {
        GpuIsm         * pIsm;
        RegHal         * pRegs;
        PollHoldMode     mode;
        const IsmChain * pChain;
        RC               pollRc;
    };

    //--------------------------------------------------------------------------
    // APIs
    //--------------------------------------------------------------------------
    // Create the appropriate GpuIsm subclass based on the Gpu DeviceId.
    virtual ~GpuIsm(){};

    virtual UINT32 IsExpComplete
    (
        UINT32 ismType, //Ism::IsmTypes
        INT32 offset,
        const vector<UINT32> &JtagArray
    );

    //Configure the ismBits for the LW_ISM_NMEAS2 macro
    RC ConfigureNoiseBits
    (
        vector<UINT64> &nBits
        ,NoiseMeas2  nMeas      //!< structure containing the new values
    );

    //Configure the ismBits for the LW_ISM_NMEAS3 macro
    RC ConfigureNoiseBits
    (
        vector<UINT64> &nBits
        ,NoiseMeas3  nMeas      //!< structure containing the new values
    );

    virtual RC ConfigureNoiseLiteBits
    (
        IsmTypes          ismType
        ,vector<UINT64>&  nBits
        ,const NoiseInfo& nMeas
    );

    virtual RC GetISMClkFreqKhz
    (
        UINT32* clkSrcFreqKHz
    );

    RC GetNoiseChainCounts
    (
        const IsmChain *pChain      //!< the chain configuration
        ,vector<UINT32>&jtagArray   //!< vector of raw bits for this chain
        ,vector<UINT32>*pValues      //!< where to put the counts
        ,vector<NoiseInfo>*pInfo    //!< vector to hold the macro specific data
    );

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

    virtual RC PollISMCtrlComplete
    (
        UINT32 *pComplete
    );

    virtual RC ReadCPMs
    (
        vector<CpmInfo> *pInfo
        ,vector<CpmInfo> &cpmParams      // structure with setup information
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

    virtual RC ReadISinks
    (
        vector<ISinkInfo> *pInfo
        ,vector<ISinkInfo> &params      // structure with setup information
    );
    // Reads the current NMEAS_lite ISM counts on all of the chains.
    virtual RC ReadNoiseMeasLite
    (
        vector<UINT32> *pSpeedos
        , vector<NoiseInfo> *pInfo
        , bool bPollExperiment = true
    );

    // Reads the current NMEAS_lite ISM counts on one chain
    virtual RC ReadNoiseMeasLiteOnChain
    (
        INT32 chainId
        , INT32 chiplet
        , vector<UINT32> *pSpeedos
        , vector<NoiseInfo> *pInfo
        , bool bPollExperiment
    );

    virtual RC RunNoiseMeas //!< run the noise measurement experiment on all chains
    (
        Ism::NoiseMeasStage stage //!< bitmask of the experiment stages to perform.
        ,vector<UINT32>*pValues //!< vector to hold the raw counts
        ,vector<NoiseInfo>*pInfo //!< vector to hold the macro specific information
    );

    virtual RC RunNoiseMeasOnChain //!< run the noise measurement experiment on a specific chain
    (
        INT32 chainId           //!< JTag instruction Id to use
        ,INT32 chiplet          //!< JTag chiplet to use
        ,Ism::NoiseMeasStage stage   //!< bitmask of the experiment statges to perform
        ,vector<UINT32>*pValues  //!< vector to hold the raw counts
        ,vector<NoiseInfo>*pInfo//!< vector to hold the macro specific information
    );

    RC RunNoiseMeasV2OnChain          //!< Run the noise measurement exp. on a specific chain.
    (
        const IsmChain *pChain      //!< the chain configuration
        ,const bitset<Ism::maxISMsPerChain>& ismMask //!< bitmask of the MACROs to program
        ,vector<UINT32> &jtagArray  //!< Vector of raw bits for this chain.
        ,NoiseMeasParam noiseParm   //!< structure containing the experiment parameters
        ,vector<UINT32> *pValues     //!< vector of the raw counts
        ,vector<NoiseInfo> *pInfo   //!< vector of macro specific information
        ,UINT32 stageMask           //!< a bitmask of what stages should be performed
    );

    RC RunNoiseMeasV3OnChain
    (
        const IsmChain *pChain      //!< the chain configuration
        ,const bitset<Ism::maxISMsPerChain>& ismMask //!< bit mask to specify stages to perform
        ,vector<UINT32> &jtagArray  //!< Vector of raw bits for this chain.
        ,NoiseMeasParam noiseParm  //!< structure containing the experiment parameters
        ,vector<UINT32> *pValues    //!< vector of the raw counts
        ,vector<NoiseInfo> *pInfo   //!< vector of macro specific information
        ,UINT32 stageMask           //!< a bitmask of what stages should be performed
    );

    // Setup all NMEAS macros to run an experiment (Legacy function)
    virtual RC SetupNoiseMeas
    (
        NoiseMeasParam param    // structure with setup information
    );

    virtual RC SetupNoiseMeasLite   //!< Setup all NMEAS_lite macros to run an experiment (Volta+)
    (
        const NoiseInfo& param      //!< structure with setup information
    );

    // Setup all the NMEAS_lite macros on a specific chain to run an experiment
    virtual RC SetupNoiseMeasLiteOnChain
    (
        INT32 chainId
        , INT32 chiplet
        , const NoiseInfo& param   // structure with setup information
    );

    // Trigger an ISM experiment when the ISM trigger source = priv write.
    virtual RC TriggerISMExperiment();

    virtual RC WriteCPMs
    (
        vector<CpmInfo> &params     // structure with setup information
    );

    virtual RC WriteISinks
    (
        vector<ISinkInfo> &params // structure with setup information
    );

protected:
    // Force user to go through the CreateISM() API.
    GpuIsm
    (
        GpuSubdevice *pGpu,
        vector<Ism::IsmChain> *pTable
    );

    virtual RC ConfigureCPMBits
    (
        IsmTypes          ismType
        , vector<UINT64>& cpmBits
        , const CpmInfo&  param
    );

    virtual RC ConfigureISinkBits
    (
        IsmTypes        ismType
        ,vector<UINT64> &iSinkBits
        ,const ISinkInfo  &param
    );

    virtual RC ConfigureISMBits
    (
        vector<UINT64> &IsmBits
        ,IsmInfo &info
        ,IsmTypes ismType
        ,UINT32 iddq            // 0 = powerUp, 1 = powerDown
        ,UINT32 enable          // 0 = disable, 1 = enable
        ,UINT32  durClkCycles   // how long to run the experiment
        ,UINT32 flags           // Special handling flags
    );

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
        ,INT32           ctrlSrcSel     // 1 = jtag 0 = priv access
    );

    RC DumpISMBits(UINT32 ismType, vector<UINT64>& ismBits);

    virtual UINT64 ExtractISMRawCount
    (
        UINT32 ismType,
        vector<UINT64>& ismBits
    );

    virtual UINT32 ExtractISMRawDcdCount
    (
        UINT32 ismType,
        vector<UINT64>& ismBits
    );
    
    virtual RC GetCPMChainCounts
    (
        const IsmChain *pChain        // The chain configuration
        , vector<UINT32> &jtagArray   // Vector of raw bits for this chain
        , vector<CpmInfo> *pInfo      // Vector to hold the macro specific data
        , vector<CpmInfo> &cpmParams  // Vector to hold the filter paramters of the chains to read.
    );

    //TODO: Make this a template
    INT32 GetCpmParamIdx
    (
        vector<CpmInfo> &params,
        UINT32  chiplet,
        UINT32  instrId,
        INT32   offset
    );
    
    virtual RC GetISinkChainCounts
    (
        const IsmChain *pChain        // The chain configuration
        , vector<UINT32> &jtagArray   // Vector of raw bits for this chain
        , vector<ISinkInfo> *pInfo    // Vector to hold the macro specific data
        , vector<ISinkInfo> &params   // Vector to hold the filter paramters of the chains to read.
    );

    INT32 GetISinkParamIdx
    (
        vector<ISinkInfo> &params,
        UINT32  chiplet,
        UINT32  instrId,
        INT32   offset
    );

    bool ParamsMatch
    (
        IsmHeader &header,
        UINT32 chiplet,
        UINT32 instrId,
        INT32 offset
    );

    UINT32 GetDelayTimeUs
    (
        const IsmChain * pChain
        ,UINT32 durClkCycles
    );

    virtual UINT32 GetISMSize
    (
        IsmTypes ismType
    );

    virtual INT32 GetISMMode
    (
        PART_TYPES type
    );

    GpuSubdevice * GetGpuSub() { return m_pGpu; }

    virtual RC ReadHost2Jtag
    (
        UINT32 Chiplet,
        UINT32 Instruction,
        UINT32 ChainLength,
        vector<UINT32> *pResult
    );

    virtual RC WriteHost2Jtag
    (
        UINT32 Chiplet,
        UINT32 Instruction,
        UINT32 ChainLength,
        const vector<UINT32> &InputData
    );

    virtual INT32 GetISMOutputDivider
    (
        PART_TYPES PartType
    );
    virtual INT32 GetISMOutDivForFreqCalc
    (
        PART_TYPES PartType,
        INT32 outDiv
    );

private:
    virtual UINT32 GetISMDurClkCycles();


    virtual INT32 GetISMRefClkSel
    (
        PART_TYPES partType
    );

    // Return the proper frequency for callwlating measurement time.
    virtual RC GetNoiseMeasClk
    (
        UINT32 *pClkKhz
    );

    virtual INT32 GetOscIdx
    (
        PART_TYPES Type
    );
   
    static bool PollHoldIsm(void *pvPollData);

    RC ReadHoldIsm
    (
        const IsmChain * pChain,
        vector<UINT64> &holdBits
    );

    RC WriteHoldIsm
    (
        const IsmChain * pChain,
        vector<UINT64> &holdBits
    );

};

#define CREATE_GPU_ISM_FUNCTION(Class)                  \
    GpuIsm *Create##Class(GpuSubdevice *pGpu)           \
    {                                                   \
        return new Class(pGpu, Class::GetTable(pGpu));  \
    }

#endif

