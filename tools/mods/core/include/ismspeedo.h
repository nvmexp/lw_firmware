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

#ifndef INCLUDED_ISM_SPEEDO_H
#define INCLUDED_ISM_SPEEDO_H
// refer to the following links for more detailed info on ISMs:
// HW Links: https://wiki.lwpu.com/gpuhwdept/index.php/System-ASIC/In-Silicon_Measurement/Gen_ISM_Manual
// Mods Links: https://wiki.lwpu.com/engwiki/index.php/MODS/ISMs
// Perforce links: https://p4viewer/get//hw/lwgpu_gmlit1/manuals/dev_ism.ref

namespace IsmSpeedo
{
    static const INT32 NO_OSCILLATOR = -1;
    static const INT32 NO_OUTPUT_DIVIDER = -1;
    static const INT32 NO_MODE = -1;
    static const UINT32 NO_CLK_CYCLES = ~0;
    static const UINT32 NO_DELAY = ~0;
    static const INT32 NO_TRIGGER = -1;
    // General class of speedo types (including legacy speedos)
    enum PART_TYPES
    {   // new protocol for reading speedos below are independant from
        // floorsweeping block partitions.
        COMP,                 // LW_ISM_ROSC_comp
        BIN,                  // LW_ISM_ROSC_bin
        METAL,                // LW_ISM_ROSC_bin_metal
        MINI,                 // LW_ISM_MINI_*
        TSOSC,                // LW_ISM_TSOSC_a, LW_ISM_TSOSC_a2 (Pascal+) &
                              // LW_ISM_TSOSC_a3 (Volta+)
        VNSENSE,              // LW_ISM_VNSENSE_ddly
        NMEAS,                // LW_ISM_NMEAS_v2 & LW_ISM_NMEAS_v3 (Pascal+) &
                              // LW_ISM_NMEAS_lite (Volta+)
        HOLD,                 // LW_ISM_HOLD
        AGING,                // LW_ISM_ROSC_bin_aging & LW_ISM_ROSC_bin_aging_v1 (Volta+)
        BIN_AGING,            // LW_ISM_ROSC_bin & LW_ISM_ROSC_bin_aging (Pascal+) &
                              // LW_ISM_ROSC_bin_aging_v1 (Volta+)
        AGING2,               // LW_ISM_aging & LW_ISM_aging_v1 (Volta+) LW_ISM_aging_v2 (CheetAh+),
                              // LW_ISM_aging_v3 & LW_ISM_aging_v4 (Turing+)
        CPM,                  // LW_ISM_cpm, LW_ISM_cpm_v1, LW_ISM_cpm_v2, LW_ISM_cpm_v3
        IMON,                 // LW_ISM_imon
        CTRL,                 // LW_ISM_ctrl, LW_ISM_ctrl_v2 ... LW_ISM_ctrl_v4
        ISINK,                // LW_ISM_lwrrent_sink
        ALL                   // does not include TSOSC, NMEAS, NMEAS_3, HOLD or ROSC_bin_aging
    };

    enum HoldMode
    {
        HOLD_AUTO,
        HOLD_MANUAL,
        HOLD_OSC
    };
    struct HoldInfo
    {
        UINT32   chainId;        //!< [in] Chain Id
        UINT32   chiplet;        //!< [in] Chiplet
        INT32    startBit;       //!< [in] Start bit
        HoldMode mode;           //!< [in] Mode to run the Hold ISM in
        bool     bDoubleClk;     //!< [in] Enable double clock (AUTO mode only)
        UINT32   modeSel;        //!< [in] Mode sel value for hold ISM
        UINT32   t1Value;        //!< [in] Input T1 value
        UINT32   t2Value;        //!< [in] Input T2 value
        UINT32   oscClkCount;    //!< [in] clock count to run OSC mode for
        UINT32   t1RawCount;     //!< [out] T1 raw reading/osc_count
        UINT32   t2RawCount;     //!< [out] T2 raw reading/osc_count
        UINT64   t1OscPs;        //!< [out] T1 time in ps (OSC mode only)
        UINT64   t2OscPs;        //!< [out] T2 time in ps (OSC mode only)
        bool     bMismatchFound; //!< [out] true if mismatch was found
                                 //!< (AUTO mode only)
        HoldInfo() :
            chainId(~0), chiplet(~0), startBit(-1), mode(HOLD_AUTO),
            bDoubleClk(false), modeSel(0), t1Value(0), t2Value(0),
            oscClkCount(10000), t1RawCount(0), t2RawCount(0), t1OscPs(0),
            t2OscPs(0),bMismatchFound(false) {}
        HoldInfo(UINT32 chain, UINT32 chip, INT32 sb, HoldMode m, bool clk,
                 INT32 ms, INT32 t1v, INT32 t2v, INT32 osc) :
            chainId(chain), chiplet(chip), startBit(sb), mode(m),
            bDoubleClk(clk), modeSel(ms), t1Value(t1v), t2Value(t2v),
            oscClkCount(osc), t1RawCount(0), t2RawCount(0), t1OscPs(0),
            t2OscPs(0),bMismatchFound(false) {}
    };
    // Structure to describe version 2 (Maxwell) of the noise measurement macro
    struct NoiseInfo2
    {
        UINT32 chainId;
        UINT32 chiplet;
        INT32  startBit;
        INT32  tap;
        INT32  mode;
        INT32  outSel;
        INT32  thresh;
        INT32  trimInit;
        INT32  measLen;
        UINT32 count;   // raw counts from the Jtag chain
        UINT32 data;    // raw value from the priv reg VST_DATA
    };
    // Structure to describe version 3 (Pascal+) of the noise measurement macro
    struct NoiseInfo3
    {
        UINT32 chainId;
        UINT32 chiplet;
        INT32  startBit;
        INT32  hmDiv;
        INT32  hmFineDlySel;
        INT32  hmTapDlySel;
        INT32  hmTapSel;
        INT32  hmTrim;
        INT32  lockPt;
        INT32  mode;
        INT32  outSel;
        INT32  outClamp;
        INT32  sat;
        INT32  thresh;
        INT32  winLen;
        INT32  hmAvgWin;
        INT32  fdUpdateOff;
        UINT32 count;   // raw counts from the Jtag chain
        UINT32 hmOut;   // raw value from the hard macro output
        INT32 rdy;      // ready status of the circuit, used for debug.
    };

    // Structure to describe version 4 and 5 (Volta+) of the noise measurement macro
    struct NoiseInfoLite
    {
        UINT32 chainId;
        UINT32 chiplet;
        INT32  startBit;
        INT32  trim;
        INT32  tap;
        INT32  iddq;
        INT32  mode;
        INT32  sel;
        INT32  div;
        INT32  divClkIn;
        INT32  fixedDlySel;
        INT32  count;
        INT32  refClkSel; // version 5 only
    };

    // Structure to describe version 6 (Ampere+) of the noise measurement macro
    struct NoiseInfoLite3
    {
        UINT32 chainId;         //
        UINT32 chiplet;         //
        INT32  startBit;        //
        INT32  trim;            // NMEAS_LITE_V3_TRIM
        INT32  tap;             // NMEAS_LITE_V3_TAP_SEL
        INT32  iddq;            // NMEAS_LITE_V3_IDDQ
        INT32  mode;            // NMEAS_LITE_V3_MODE
        INT32  clkSel;          // NMEAS_LITE_V3_CLK_SEL
        INT32  outSel;          // NMEAS_LITE_V3_OUT_SEL
        INT32  div;             // NMEAS_LITE_V3_DIV
        INT32  divClkIn;        // NMEAS_LITE_V3_DIV_CLK_IN
        INT32  fixedDlySel;     // NMEAS_LITE_V3_FIXED_DLY_SEL
        INT32  refClkSel;       // NMEAS_LITE_V3_REFCLK_SEL
        INT32  jtagOvr;         // NMEAS_LITE_V3_JTAG_OVERRIDE
        INT32  winLen;          // NMEAS_LITE_V3_WINDOW_LENGTH
        INT32  cntrEn;          // NMEAS_LITE_V3_COUNTER_ENABLE
        INT32  cntrClr;         // NMEAS_LITE_V3_COUNTER_CLEAR
        INT32  cntrDir;         // NMEAS_LITE_V3_COUNTER_DIRECTION
        INT32  cntrToRead;      // NMEAS_LITE_V3_COUNTER_TO_READ
        INT32  cntrOffset;      // NMEAS_LITE_V3_COUNTER_OFFSET
        INT32  errEn;           // NMEAS_LITE_V3_ERROR_ENABLE
        INT32  spareIn;         // NMEAS_LITE_V3_SPARE_IN
        INT32  thresh;          // NMEAS_LITE_V3_THRESHOLD
        INT32  mnMxWinEn;       // NMEAS_LITE_V3_MNMX_WIN_ENABLE
        INT32  mnMxWinDur;      // NMEAS_LITE_V3_MNMX_WIN_DURATION
        INT32  threshDir;       // NMEAS_LITE_V3_THRESHOLD_DIRECTION
        INT32  roEn;            // NMEAS_LITE_V3_RO_ENABLE
        INT32  errOut;          // NMEAS_LITE_V3_ERROR_OUT
        INT32  spareOut;        // NMEAS_LITE_V3_SPARE_OUT
        UINT64 count;           // NMEAS_LITE_V3_COUNT_HI << 28 + NMEAS_LITE_V3_COUNT_LOW
        UINT32 countLow;        // NMEAS_LITE_V3_COUNT_LOW
        UINT32 countHi;         // NMEAS_LITE_V3_COUNT_HI
        INT32  ready;           // NMEAS_LITE_V3_READY
    };

    // Structure to support LW_ISM_NMEAS_lite_v4-7 (GA10x) of the noise measurement macro
    struct NoiseInfoLite4
    {
        UINT32 chainId;         //
        UINT32 chiplet;         //
        INT32  startBit;        //
        INT32  trim;            // NMEAS_LITE_V 4 5 6 7  _TRIM
        INT32  tap;             // NMEAS_LITE_V 4 5 6 7  _TAP_SEL
        INT32  iddq;            // NMEAS_LITE_V 4 5 6 7  _IDDQ
        INT32  mode;            // NMEAS_LITE_V 4 5 6 7  _MODE
        INT32  clkSel;          // NMEAS_LITE_V 4 5 6 7  _CLK_SEL
        INT32  outSel;          // NMEAS_LITE_V 4 5 6 7  _OUT_SEL
        INT32  div;             // NMEAS_LITE_V 4 5 6 7  _DIV
        INT32  divClkIn;        // NMEAS_LITE_V 4 5 6 7  _DIV_CLK_IN
        INT32  fixedDlySel;     // NMEAS_LITE_V 4 5 6 7  _FIXED_DLY_SEL
        INT32  refClkSel;       // NMEAS_LITE_V 4 5 6 7  _REFCLK_SEL
        INT32  jtagOvr;         // NMEAS_LITE_V 4 5 6 7  _JTAG_OVERRIDE
        INT32  winLen;          // NMEAS_LITE_V 4 5 - -  _WINDOW_LENGTH
        INT32  cntrEn;          // NMEAS_LITE_V 4 5 - -  _COUNTER_ENABLE
        INT32  cntrClr;         // NMEAS_LITE_V 4 5 - -  _COUNTER_CLEAR
        INT32  cntrDir;         // NMEAS_LITE_V 4 5 - -  _COUNTER_DIRECTION
        INT32  cntrToRead;      // NMEAS_LITE_V 4 5 - -  _COUNTER_TO_READ
        INT32  cntrOffset;      // NMEAS_LITE_V 4 5 - -  _COUNTER_OFFSET
        INT32  errEn;           // NMEAS_LITE_V 4 5 6 7  _ERROR_ENABLE
        INT32  spareIn;         // NMEAS_LITE_V 4 5 6 7  _SPARE_IN
        INT32  thresh;          // NMEAS_LITE_V 4 5 6 7  _THRESHOLD
        INT32  threshDir;       // NMEAS_LITE_V 4 5 6 7  _THRESHOLD_DIRECTION
        INT32  roEn;            // NMEAS_LITE_V 4 5 6 7  _RO_ENABLE
        INT32  fabRoEn;         // NMEAS_LITE_V - 5 - -  _FABRO_ENABLE
        INT32  fabRoSBcast;     // NMEAS_LITE_V - 5 - -  _FABRO_S_BCAST
        INT32  fabRoSwPol;      // NMEAS_LITE_V - 5 - -  _FABRO_SW_POLARITY
        INT32  fabRoIdxBcast;   // NMEAS_LITE_V - 5 - -  _FABRO_IDX_BCAST
        INT32  fabRoSel;        // NMEAS_LITE_V - 5 - -  _FABRO_RO_SEL
        INT32  fabRoSpareIn;    // NMEAS_LITE_V - 5 - -  _FABRO_SPARE_IN
        INT32  streamSel;       // NMEAS_LITE_V - - 6 7  _STREAM_SEL
        INT32  streamMode;      // NMEAS_LITE_V - - 6 7  _STREAMING_MODE
        INT32  selStreamTrigSrc;// NMEAS_LITE_V - - 6 7  _SEL_STREAMING_TRIGGER_SRC
        INT32  samplePeriod;    // NMEAS_LITE_V - - 6 7  _SAMPLING_PERIOD
        INT32  streamTrigSrc;   // NMEAS_LITE_V - - 6 7  _STREAMING_TRIGGER_SRC
        INT32  readyBypass;     // NMEAS_LITE_V - - - 7  _READY_BYPASS
        INT32  errOut;          // NMEAS_LITE_V 4 5 6 7  _ERROR_OUT
        INT32  spareOut;        // NMEAS_LITE_V 4 5 6 7  _SPARE_OUT or
                                //                       _SPARE_OUT_LOW + _SPARE_OUT_HI
        UINT64 count;           // NMEAS_LITE_V 4 5 6 7  _COUNT / _COUNT_HI + _COUNT_LOW
        UINT32 countLow;        // NMEAS_LITE_V 4 5 - -  _COUNT_LOW
        UINT32 countHi;         // NMEAS_LITE_V 4 5 - -  _COUNT_HI
        INT32  ready;           // NMEAS_LITE_V 4 5 6 7  _READY
    };

    // Structure to support LW_ISM_NMEAS_lite_v8-9 (GH100) of the noise measurement macro
    struct NoiseInfoLite8
    {
        UINT32 chainId;         //
        UINT32 chiplet;         //
        INT32  startBit;        //
        INT32  iddq;            // NMEAS_LITE_V 8 9  _IDDQ
        INT32  mode;            // NMEAS_LITE_V 8 9  _MODE
        INT32  outSel;          // NMEAS_LITE_V 8 9  _OUT_SEL
        INT32  clkSel;          // NMEAS_LITE_V 8 9  _REFCLK_SEL
        INT32  div;             // NMEAS_LITE_V 8 9  _DIV
        INT32  clkPhaseIlw;     // NMEAS_LITE_V 8 9  _CLK_PHASE_ILWERT
        INT32  divClkIn;        // NMEAS_LITE_V 8 9  _DIV_CLK_IN
        INT32  trimInitA;       // NMEAS_LITE_V 8 9  _TRIM_INIT_A
        INT32  trimInitB;       // NMEAS_LITE_V 8 9  _TRIM_INIT_B
        INT32  trimTypeSel;     // NMEAS_LITE_V 8 9  _TRIM_TYPE_SEL
        INT32  tapTypeSel;      // NMEAS_LITE_V 8 9  _TAP_TYPE_SEL
        INT32  trimInitC;       // NMEAS_LITE_V 8 9  _TRIM_INIT_C
        INT32  fixedDlySel;     // NMEAS_LITE_V 8 9  _FIXED_DLY_SEL
        INT32  tapSel;          // NMEAS_LITE_V 8 9  _TAP_SEL
        INT32  jtagOvr;         // NMEAS_LITE_V 8 9  _JTAG_OVERRIDE
        INT32  threshMax;       // NMEAS_LITE_V 8 9  _THRESHOLD_MAX
        INT32  threshMin;       // NMEAS_LITE_V 8 9  _THRESHOLD_MIN
        INT32  threshEn;        // NMEAS_LITE_V 8 9  _THRESHOLD_ENABLE
        INT32  spareIn;         // NMEAS_LITE_V 8 9  _SPARE_IN
        INT32  streamSel;       // NMEAS_LITE_V - 9  _STREAM_SEL
        INT32  streamMode;      // NMEAS_LITE_V - 9  _STREAMING_MODE
        INT32  selStreamTrigSrc;// NMEAS_LITE_V - 9  _SEL_STREAMING_TRIGGER_SRC
        INT32  samplePeriod;    // NMEAS_LITE_V - 9  _SAMPLING_PERIOD
        INT32  streamTrigSrc;   // NMEAS_LITE_V - 9  _STREAMING_TRIGGER_SRC
        INT32  readyBypass;     // NMEAS_LITE_V - 9  _READY_BYPASS
        INT32  cntrEn;          // NMEAS_LITE_V 8 -  _COUNTER_ENABLE
        INT32  winLen;          // NMEAS_LITE_V 8 -  _WINDOW_LENGTH
        INT32  cntrClr;         // NMEAS_LITE_V 8 -  _COUNTER_CLEAR
        INT32  cntrOffsetDir;   // NMEAS_LITE_V 8 -  _COUNTER_OFFSET_DIRECTION
        INT32  cntrToRead;      // NMEAS_LITE_V 8 -  _COUNTER_TO_READ
        INT32  cntrTrimOffset;  // NMEAS_LITE_V 8 -  _COUNTER_TRIM_OFFSET
        UINT64 count;           // NMEAS_LITE_V 8 9  _COUNT / _COUNT_HI + _COUNT_LOW
        UINT32 countLow;        // NMEAS_LITE_V 8 -  _COUNT_LOW
        UINT32 countHi;         // NMEAS_LITE_V 8 -  _COUNT_HI
        INT32  ready;           // NMEAS_LITE_V 8 9  _READY
        INT32  threshOut;       // NMEAS_LITE_V 8 9  _THRESHOLD_OUT
        INT32  threshMaxOut;    // NMEAS_LITE_V 8 9  _THRESHOLD_MAX_OUT
        INT32  threshMinOut;    // NMEAS_LITE_V 8 9  _THRESHOLD_MIN_OUT
        INT32  spareOut;        // NMEAS_LITE_V 8 9  _SPARE_OUT
    };

    // structure to contain any version of the noise measurement macro.
    struct NoiseInfo
    {
        UINT32 version; // used to indicate what structure version was populated.
        union {                                                           //$ GMxxx   GPxxx   GVxx   TUxxxx   GAxxx   GHxxx   ADxxx  GBxxx
            struct NoiseInfo2 ni2;                                        //$   x                 
            struct NoiseInfo3 ni3;                                        //$            x       x        
            struct NoiseInfoLite niLite;    // version 1&2 data           //$                          x
            struct NoiseInfoLite3 niLite3;  // version 3 data             //$                                    x
            struct NoiseInfoLite4 niLite4;  // LW_ISM_NMEAS_lite_v4-7     //$                    x               x
            struct NoiseInfoLite8 niLite8;  // LW_ISM_NMEAS_lite_v8-9     //$                                            x
        };
    };

    // Structure to describe Critical Path Measurement (CPM) macro.
    // Each field from LW_ISM_CPM is covered by a dedicated parameter.
    // Please refer to dev_ism.ref for detailed information on this.
    struct Cpm1 // version 0 & 1
    {                                   
        UINT32  enable;
        UINT32  iddq;
        UINT32  resetFunc;
        UINT32  resetMask;
        UINT32  launchSkew;
        UINT32  captureSkew;
        UINT32  tuneDelay;
        UINT32  spareIn;
        UINT32  resetPath;
        UINT32  maskId;
        UINT32  maskEn;
        UINT32  maskEdge;
        UINT32  synthPathConfig;
        UINT32  calibrationModeSel;
        UINT32  pgDis;
        UINT32  treeOut;
        UINT64  status0;
        UINT64  status1;
        UINT32  status2;
        UINT32  miscOut;
        UINT32  garyCntOut;
    };
    struct Cpm2 // version 2 & 3
    {
        UINT32  grayCnt;
        UINT32  macroFailureCnt;
        UINT32  miscOut;
        UINT32  pathTimingStatus;
        UINT32  hardMacroCtrlOverride;
        UINT32  hardMacroCtrl;
        UINT32  instanceIdOverride;
        UINT32  instanceId;
        UINT32  samplingPeriod;
        UINT32  skewCharEn;
        UINT32  spareIn;
    };
    struct Cpm4                             // Used by version
    {                                       //  4   5
        UINT32  grayCnt;                    //  x   x
        UINT32  macroFailureCnt;            //  x   x
        UINT32  miscOut;                    //  x   x
        UINT32  pathTimingStatus;           //  x   x
        UINT32  hardMacroCtrlOverride;      //  x   -
        UINT32  hardMacroCtrl;              //  x   -
        UINT32  instanceIdOverride;         //  x   -
        UINT32  instanceId;                 //  x   x
        UINT32  samplingPeriod;             //  x   x
        UINT32  skewCharEn;                 //  x   x
        UINT32  spareIn;                    //  x   x
        UINT32  pgEn;                       //  x   -
        UINT32  pgEnOverride;               //  x   -
        UINT32  enable;                     //  -   x
        UINT32  resetFunc;                  //  -   x
        UINT32  resetPath;                  //  -   x
        UINT32  configData;                 //  -   x
        UINT32  dataTypeId;                 //  -   x
        UINT32  dataValid;                  //  -   x
    };

    struct Cpm6 // version 6
    {
        UINT32 grayCnt;
        UINT32 macroFailureCnt;
        UINT32 miscOut;
        UINT32 pathTimingStatus;
        UINT32 instanceId;
        UINT32 samplingPeriod;
        UINT32 skewCharEn;
        UINT32 spareIn;
        UINT32 hardMacroCtrl;
        UINT32 hardMacroCtrlOverride;
        UINT32 instanceIdOverride;
    };

    // Structure containing the minimum properties that will uniquely identify a specific ISM
    // circuit on a given JTag chain
    struct IsmHeader
    {
        UINT32      version;
        INT32       chainId;
        INT32       chiplet;
        INT32       offset;
    };
    // We keep 1 copy of each version so the user can specify different parameters for each
    // version of the macro.
    struct CpmInfo
    {   
        IsmHeader   hdr;
        union {
            Cpm1        cpm1;
            Cpm2        cpm2; // version 2 & 3
            Cpm4        cpm4; // version 4 & 5 T234
            Cpm6        cpm6; // version 6 GH100+
        };
    };
    
    struct ISink1
    {
        UINT32  iOff;
        UINT32  iSel;
        UINT32  selDbg;
    };
    // We keep 1 copy of each version so the user can specify different parameters for each
    // version of the macro.
    struct ISinkInfo
    {
        IsmHeader   hdr;
        union 
        {
            ISink1  isink1;     // initial version
        };
    };

    // Structure to hold enough ISM data to identify each ISM as unique.
    struct IsmInfo
    {
        // version 1 specific fields
        UINT32 version; // default = 1
        UINT32 chainId;
        UINT32 chiplet;
        INT32  startBit;
        INT32  oscIdx;
        INT32  outDiv;
        INT32  mode;
        INT32  duration;    // if < 0 use the default value
        UINT32 clkKHz;      // frequency of the Jtag clk
        UINT32 count;
        INT32  adj;         // used on TSOSC and VNSENSE
        INT32  init;        // used on MINI and VNSENSE
        INT32  finit;       // used on MINI and VNSENSE
        UINT32 flags;       // process flags for the chain, see Ism::IsmFlags
        UINT32 roEnable;    // used on AGING, pascal+
        INT32  refClkSel;   // used on MINI and NMEAS_LITE
        UINT32 dcdEn;       // used on AGING (Volta+)
        UINT32 outSel;      // used on AGING (Volta+)
        UINT32 pactSel;     // used on AGING (Volta+)
        UINT32 nactSel;     // used on AGING (Volta+)
        UINT32 srcClkSel;   // used on AGING (Volta+)
        UINT32 dcdCount;    // used on AGING (Turing+)

        // Version 2 specific fields
        UINT32 delaySel;    // used on ROSC_BIN_v4 (Hopper+)
        UINT32 analogSel;   // used on _IMON (Hopper+)
        // Version 3 specific fields
        UINT32 spareIn;     // used on _IMON_v2 (Ada+)
        UINT32 spareOut;    // used on _IMON_v2 (Ada+)
        IsmInfo()
          : version(1)
          , chainId(~0)
          , chiplet(~0)
          , startBit(-1)
          , oscIdx(-1)
          , outDiv(-1)
          , mode(-1)
          , duration(-1)
          , clkKHz(0)
          , count(0)
          , adj(0)
          , init(0)
          , finit(0)
          , flags(0)
          , roEnable(0)
          , refClkSel(0)
          , dcdEn(0)
          , outSel(0)
          , pactSel(0)
          , nactSel(0)
          , srcClkSel(0)
          , dcdCount(0)
          , delaySel(0)  // version 2
          , analogSel(0) // version 2
          , spareIn(0)   // version 3
          , spareOut(0)  // version 3
        {}

        IsmInfo(UINT32 v, UINT32 chain, UINT32 chip, INT32 sb, INT32 oi,
                INT32 od, INT32 m, INT32 d, UINT32 clk, INT32 ad = 0, INT32 i = 0,
                INT32 fi = 0, UINT32 f = 0, UINT32 en = 0, INT32 refClkSel = 0,
                UINT32 dcdEn = 0, UINT32 outSel = 0, UINT32 pactSel = 0,
                UINT32 nactSel = 0, UINT32 srcClkSel = 0, UINT32 dcdCnt = 0, UINT32 ds = 0,
                UINT32 as = 0, UINT32 sIn = 0, UINT32 sOut = 0)
          : version(v)
          , chainId(chain)
          , chiplet(chip)
          , startBit(sb)
          , oscIdx(oi)
          , outDiv(od)
          , mode(m)
          , duration(d)
          , clkKHz(clk)
          , count(0)
          , adj(ad)
          , init(i)
          , finit(fi)
          , flags(f)
          , roEnable(en)
          , refClkSel(refClkSel)
          , dcdEn(dcdEn)
          , outSel(outSel)
          , pactSel(pactSel)
          , nactSel(nactSel)
          , srcClkSel(srcClkSel)
          , dcdCount(dcdCnt)
          , delaySel(ds)
          , analogSel(as)
          , spareIn(sIn)
          , spareOut(sOut)
        {}
    };
    // Implemented in ism.cpp
    const char * SpeedoPartTypeToString(IsmSpeedo::PART_TYPES speedoType);
};
#endif // INCLUDED_ISM_SPEEDO_H

