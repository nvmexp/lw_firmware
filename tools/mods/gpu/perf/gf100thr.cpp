/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
*/

/*
 For reference //hw/fermi1_gf100/manuals/gf100/dev_pwr_therm.vpref.
 */
#include "gpu/perf/clockthrottle.h"
#include "fermi/gf100/dev_bus.h"
#include "fermi/gf100/dev_therm.h"
#include "fermi/gf100/dev_trim.h"
#include <list>
#include <algorithm>
#include <math.h>
#include <stdio.h>

typedef vector<UINT32>           vu32_t;
typedef vu32_t::iterator         vu32i_t;
typedef vector<LwU8>             vu8_t;
typedef vu8_t::iterator          vu8i_t;
typedef vector<float>            vf32_t;
typedef vf32_t::iterator         vf32i_t;
typedef vector<vu8_t>            vvu8_t;
typedef vvu8_t::iterator         vvu8i_t;
typedef map<float, UINT32>       mfu32_t;
typedef mfu32_t::iterator        mfu32i_t;
typedef map<UINT32, UINT32>      mu32u32_t;
typedef mu32u32_t::iterator      mu32u32i_t;
typedef map<UINT32, vu8_t>       mu32vu8_t ;
typedef mu32vu8_t::iterator      mu32vu8i_t ;
typedef map<vvu8_t, UINT32>      mvvu8u32_t;
typedef mvvu8u32_t::iterator     mvvu8u32i_t;
typedef list< vector< UINT32> >  lvu32_t;
typedef lvu32_t::iterator        lvu32i_t;
typedef map<UINT32, lvu32_t >    mu32lvu32_t ;
typedef mu32lvu32_t::iterator    mu32lvu32i_t;
typedef vector<vvu8_t>           vvvu8_t;
typedef vvvu8_t::iterator        vvvu8i_t;
typedef pair<UINT32, LwU8>       pu32u8_t;
typedef map< pu32u8_t, UINT32>   mpu32u8u32_t;
typedef mpu32u8u32_t::iterator   mpu32u8u32i_t;

typedef pair<FLOAT32, UINT32>     pf32u32_t ;
typedef map<UINT32, pf32u32_t >  mu32pf32u32_t ;

//helper macros for common ops
#define UPD(A,B,C,V)      m##B    = FLD_SET_DRF_NUM(A,B,C,V,m##B)
#define UPDIDX(A,B,C,V,I) m##B[I] = FLD_SET_DRF_NUM(A,B,C,V,m##B[I])

//------------------------------------------------------------------------------
class GF100ClockThrottle : public ClockThrottle
{
public:
    GF100ClockThrottle(GpuSubdevice * pGpuSub) : ClockThrottle(pGpuSub)
    {
        m_Debug = false;
        m_CLK_SLOWDOWN_0.resize(LW_THERM_CLK_COUNT);
        m_CLK_SLOWDOWN_1.resize(LW_THERM_CLK_COUNT);
        m_CLK_TIMING_0  .resize(LW_THERM_CLK_COUNT);
        //InitTables();
        m_Init = false;
        SetClockDomainMasks(1<<Gpu::ClkGpc2, &m_DomainMask, &m_HwDomainMask);
        SetClockDomainMasks(1<<Gpu::ClkGpc, &m_DomainMask, &m_HwDomainMask);
    }

    ~GF100ClockThrottle() override {}

    RC Grab() override;
    RC Set
    (
        const Params * pRequested,
        Params *       pActual
    ) override;
    RC Release() override;
    void SetDebug(bool dbg) override;

    void SetDomainMask(UINT32 newMask) override
    {
        SetClockDomainMasks(newMask, &m_DomainMask, &m_HwDomainMask);
    }
    UINT32 GetDomainMask() const override
    {
        return m_DomainMask;
    }

    // For the frequency shmoo mode
    string GetLwrrFreqLabel() override { return m_FreqLabel[GetLwrrFreqIdx()]; }
    UINT32 GetLwrrFreqIdx()   override { return m_FreqIdx  [m_ActFreq]; }
    UINT32 GetLwrrFreqInt()   override { return m_ActFreq; }
    UINT32 GetNumFreq()       override { return 22*16; } //16 octaves; 11 freqs

protected:
    void SetClockDomainMasks(UINT32 newSwMask, UINT32 * pSwMask, UINT32 * pHwMask);

private:
    //accounting
    bool          m_Init;
    bool          m_Debug;          //Debug Flag [0:ndebug 1:debug]
    void          PrintTable(vvu8_t);

    //main validity check structures
    vu32_t        m_Counts;         // vector[idx]    -> valid utilsclk4 counts
    mu32u32_t     m_CountMap;       // map[counts]    -> hw value
    vu32_t        m_Slews;          // vector[idx]    -> valid slew counts
    mu32u32_t     m_SlewMap;        // map[slew]      -> hw(idx) value
    vf32_t        m_Slows;          // vector[idx]    -> valid slow ratios
    mfu32_t       m_SlowMap;        // map[slowRatio] -> hw(idx) values
    vvvu8_t       m_TabList;        // tab[idx][SlowIdx][SlewIdx] -> SlowCorrection
                                    // SlowRatio is corrected to accomodate modes that
                                    // are not able to reach the given slow down with the
                                    // given slew.
    mpu32u8u32_t  m_Lut;            // map[pair(count,duty)] -> index for m_TabList
    mu32vu8_t     m_LutCnt;         // map[count] -> vector "meaningful duties"

    //helper structures
    mvvu8u32_t    m_TabToIdx;       // Util to uniquify tables
    UINT32        m_TabIdxCnt = 0;  // Util to uniquify tables

    //pulse width structures
    mu32lvu32_t   m_PulseMapClocks; // map[ns] -> list of vectors (count,duty,div)
                                    // where each setting produces pulse of
                                    // specified length

    //            These will hold the updated values of
    //            the LW_THERM privregs while we modify them
    vu32_t        m_CLK_SLOWDOWN_0;
    vu32_t        m_CLK_SLOWDOWN_1;
    vu32_t        m_CLK_TIMING_0  ;
    UINT32        m_CTRL_1  = 0;
    UINT32        m_CONFIG1 = 0;
    UINT32        m_CONFIG2 = 0;

    //            frequency manipulation structures
    UINT32        m_LowDivider  = 0;  //divider >= this value
    UINT32        m_HighDivider = 0;  //divider <  this value
    vu32_t        m_FreqVect;         //sorted list of achievable freqencies
    mu32pf32u32_t m_MakeFreq;         //map[freq] -> (UtilsClk4_cnt, div)
    mu32u32_t     m_FreqIdx ;         //map[freq] -> idx of freq
    UINT32        m_OneSource0  = 0;  //Stores Clock used in divider
    UINT32        m_ActFreq     = 0;
    vector<string> m_FreqLabel;

    //initialization routines for validity check and pulse widths
    RC       InitTables() ;

    // callwlates a vector[slewidx] indicating corrective factors
    // for the given (UtilsClk4_CNT, duty, slowfactor)
    vu8_t    IsModePossible (UINT32 count, LwU8 duty, float slow) ;

    // creates a table[slowidx][slewidx] for the specified (UtilsClk4_CNT, duty)
    UINT32    CalcTable      (UINT32 count, LwU8 duty);

    // creates a vector of "valid" or "interesting" duties for a given utilsclk4_cnt
    vu8_t    ValidDuties   (UINT32 count) ;

    // for a given divider, setup utilsclk to be OneSource0_CLK/div
    RC       SetupNewClock(FLOAT32 div);
    UINT32    RestoreClock();
    // edit the temp regs to reflect given settings
    RC       SetupNewValues(UINT32 count, UINT32 duty, UINT32 slowidx, UINT32 slewidx);

    // write back temp regs to HW
    RC SetRegs() ;

    // read HW regs into temp regs
    RC SaveRegs() ;

    //selection of nearest params
    pf32u32_t NearestDivCount(UINT32 Hz) ;
    LwU8      NearestDuty    (float count, float f_duty_255) ;
    LwU8      NearestSlowIdx (float slowf) ;
    LwU8      NearestSlewIdx (float slew) ;

    UINT32 m_DomainMask;     // bits numbered by Gpu::ClkDomain
    UINT32 m_HwDomainMask;   // translated to match the HW register layout

    void MyPrintf(const char* format, ...);
};

// Thanks cdavies for this impl.
void GF100ClockThrottle::MyPrintf(const char* format, ...)
{
    if (!m_Debug)
        return;

    va_list Arguments;
    va_start(Arguments, format);
    ModsExterlwAPrintf(Tee::PriNormal, ~0U, Tee::SPS_NORMAL, format, Arguments);
    va_end(Arguments);
}

void GF100ClockThrottle::SetDebug(bool dbg)
{
    m_Debug = dbg;
}

// this function dumps a table describing the Slow
// corrective factor for a given [slowidx][slewidx]
void GF100ClockThrottle::PrintTable(vvu8_t table)
{
    //

    vvu8i_t vvit = table.begin();
    vf32i_t slow = m_Slows.begin();
    MyPrintf("    {\n");
    MyPrintf("        /*             0    4    8   16   32   64  128  256  512   1k   2k   4k   8k  16k  32k  64k 128k*/\n");

    while(vvit != table.end())
    {
        MyPrintf("        /* %.7f */ {", 1.0f/ *slow );

        vu8i_t vit = vvit->begin();
        while(vit != vvit->end())
        {
            MyPrintf(" %3d,", (UINT32)*vit);
            vit++;
        }
        MyPrintf("},\n");
        vvit++;
        slow++;
    }
    MyPrintf("    },\n");
}

//for a given number of UtilSClk4s, there are only a certain number of ways we can
// divide them in order to acheive duty cycles.   For instance, 4 utilsclk4 can map
// to duties of 25,50,75,100 (1on,2on,3on,4on) (HW reg may not support 0on).
// TODO: determine effect of 0 in HW reg for counts less than 256.
vu8_t GF100ClockThrottle::ValidDuties(UINT32 count)
{
    vu8_t ret;
    UINT32 l_lwrr_N=0;
    for(UINT32 i = 0; i < UINT32(256); i++)
    {
        float factor = float(i+1)/256;
        if ( l_lwrr_N != UINT32(factor*count) )
        {
            ret.push_back(i);
            l_lwrr_N = UINT32(factor*count);
        }
    }
    return ret;
}

// for given slow,N,utilsclk4 return vector indicating
// slowdown corrective factor needed to meet various slews
vu8_t GF100ClockThrottle::IsModePossible( UINT32 count, LwU8 duty, float slow )
{
    vu8_t ret;

    //determine the minimum amount of utilsclk4 we have to transition
    float min_length = min(float(duty+1)/256.0f*count, float(256-(duty+1))/256*count);
    vu32i_t it = m_Slews.begin();

    while( it != m_Slews.end() )  //For each Slew
    {
        float temp   = min_length;
        LwU8 lutval = 0;
        while ( temp < *it/4.0f*m_SlowMap[slow] ) //determine corrective factor
        {
            temp += *it/4.0f;
            lutval++;
        }
        ret.push_back(lutval);
        it++;
    }

    if(m_Debug)
    {
        //for pulse length callwlations
        UINT32 high       = (UINT32)(float(duty+1)/256.0f * count);
        vu32_t tempvect(2);
        tempvect[0] = count;
        tempvect[1] = duty;
        m_PulseMapClocks[high].push_back( tempvect );
    }
    return ret;
}

// This function returns the index of a slowdown correction table for a given
// count, duty pair.  A hash table is used to uniquify the tables, as
// there is a lot of duplication.
UINT32 GF100ClockThrottle::CalcTable( UINT32 count, LwU8 duty )
{
    vvu8_t table;

    vf32i_t it = m_Slows.begin();

    //contruct table
    while(it!=m_Slows.end())
    {
        table.push_back( IsModePossible(count, duty, *it) );
        it++;
    }

    //seen this table? uniquify
    if(m_TabToIdx.find(table) == m_TabToIdx.end())
    {
        m_TabToIdx[table] = m_TabIdxCnt++;
        m_TabList.push_back(table);
    }

    return m_TabToIdx[table];
}

RC GF100ClockThrottle::InitTables()
{
    //FIXME: Obtain this from hardware
    m_OneSource0 = 1620000000;
    // 108 Mhz is the original XTAL4 frequency for UTILSCLK
    // m_OrigDivider = 15;//Divider for +0% 108 Mhz (ONESOURCE0/15 == 108 Mhz)
    m_LowDivider  = 11;  // Divider for +33% 108 Mhz (ONESOURCE0/11 == 147.2 Mhz)
    m_HighDivider = 22;  // Divider for -33% 108 Mhz (ONESOURCE0/22 ==  73.6 Mhz)

    //With  (lowdivider=11, highdivider=22] and onesource0 = 1620000000
    // the clocks(MHz)  we can hit are the following work pulse frequencies :
    // Note that Valid Duties must use the "real" count, and not the "priv reg" name count
//priv cnt ->        4        8      16      32      64     128    256    512   1024  2048  4096  8192  16k  32K  64k 128k
//real cnt ->        1        2       4       8      16      32     64    128    256   512  1024  2048   4k  8K   16k 32k
    //div 11  36818181 18409090 9204545 4602272 2301136 1150568 575284 287642 143821 71910 35955 17977 8988 4494 2247 1123
    // |  12  33750000 16875000 8437500 4218750 2109375 1054687 527343 263671 131835 65917 32958 16479 8239 4119 2059 1029
    // v  13  31153846 15576923 7788461 3894230 1947115  973557 486778 243389 121694 60847 30423 15211 7605 3802 1901  950
    //    14  28928571 14464285 7232142 3616071 1808035  904017 452008 226004 113002 56501 28250 14125 7062 3531 1765  882
    // *  15  27000000 13500000 6750000 3375000 1687500  843750 421875 210937 105468 52734 26367 13183 6591 3295 1647  823
    //    16  25312500 12656250 6328125 3164062 1582031  791015 395507 197753  98876 49438 24719 12359 6179 3089 1544  772
    //    17  23823529 11911764 5955882 2977941 1488970  744485 372242 186121  93060 46530 23265 11632 5816 2908 1454  727
    //    18  22500000 11250000 5625000 2812500 1406250  703125 351562 175781  87890 43945 21972 10986 5493 2746 1373  686
    //    19  21315789 10657894 5328947 2664473 1332236  666118 333059 166529  83264 41632 20816 10408 5204 2602 1301  650
    //    20  20250000 10125000 5062500 2531250 1265625  632812 316406 158203  79101 39550 19775  9887 4943 2471 1235  617
    //    21  19285714  9642857 4821428 2410714 1205357  602678 301339 150669  75334 37667 18833  9416 4708 2354 1177  588
    //

    // For instance, if we use a clock divider of 15 and allow cnt==4 utilsclk, we end up with
    // a pulse frequency of 27 Mhz
    //  FIXME:  Why is utilclk instead of utilsclk4 being used in this calc.  We have verified
    // on oscilliscope.

    // Count is the number of UtilsClk4 pulses the hardware supports
    // for GF1xx this 4 bit value maps from 4...128k utilsclk4 counts
    m_TabIdxCnt=0;
    for(unsigned int i=0; i<16; i++)
    {
        m_Counts.push_back( 1<<(i+2) );
        m_CountMap[1<<(i+2)] = i;
    }
    // Slew is the number of utilsclk4 pulses we spend at each slow level
    // before moving to the next slow level.
    // For GF1xx this 4 bit value maps from 4..128k utilsclk4 counts
    // FIXME: Slew of zero will be colwerted to 4.  There is not a use for
    // Slew 0 at this point beyond blowing stuff up, and it uses a different
    // HW path.  At some point we may add support for this.
    m_Slews.push_back(0);
    m_SlewMap[0] = 0;
    for(unsigned int i=0; i<16; i++)
    {
        m_Slews.push_back( 1<< (i+2) );
        m_SlewMap[1<<(i+2)] = i+1;
    }

    //extended mode slowdown callwlation.
    for(unsigned int i =0; i<64; i++)
    {
        float factor1 = 1.0f + 0.5f*i;
        m_Slows.push_back( factor1 );
        m_SlowMap[ factor1 ] = i;
    }

    //construct table for each count,duty pair
    vu32i_t count_it = m_Counts.begin();
    while(count_it != m_Counts.end())
    {
        vu8_t v = ValidDuties(*count_it);
        m_LutCnt[*count_it] = v;

        vu8i_t v_it = v.begin();
        while(v_it != v.end())
        {
            UINT32 idx = CalcTable(*count_it, *v_it);
            m_Lut[       pu32u8_t(*count_it, *v_it)] = idx;
            v_it++;
        }
        count_it++;
    }

    if (m_Debug)
    {
        vvvu8i_t it = m_TabList.begin();
        UINT32 tidx=0;
        while( it != m_TabList.end())
        {
            MyPrintf("Table %d\n", tidx);
            PrintTable(*it);
            it++;
            tidx++;
        }

    }
    //Clock section -
    // Warning UtilSClk is driven @108Mhz via xtalclk 4
    // OneSource1/15 happens to be 108Mhz and we will use this to vary UtilSClk
    // this may change on future chips
    char buff[128];
    for(float i=static_cast<float>(m_LowDivider); i<m_HighDivider; i+=0.5)
    {
        //LW_THERM_CONFIG2_GRAD_PWM_PERIOD_4 .. 128K
        for(unsigned j = 0; j<16; j++)
        {
            UINT32 utilsclk4_cnt = 1<<(j+2);
            //for some reason, we are getting x4 freqency out of this;
            //as if utilsclk is being used, not utilsclk4
            UINT32 freq = (UINT32)(m_OneSource0/(float)i/utilsclk4_cnt  /*/4*/) ;
            m_FreqVect.push_back(      freq );
            m_MakeFreq[                freq ] = pf32u32_t(i, utilsclk4_cnt);
        }
        MyPrintf("\n");

    }
    sort(m_FreqVect.begin(), m_FreqVect.end());
    for(unsigned int i =0; i<m_FreqVect.size();i++)
    {
        sprintf(buff,"%d", m_FreqVect[i]);
        m_FreqLabel.push_back(string(buff));
        m_FreqIdx[m_FreqVect[i]] = i;
    }

    MyPrintf("Freq List constructed\n");

    if(m_Debug)
    {
        mu32lvu32_t pulse_map_time;
        float round_factor = 1.0;
        for (unsigned int i = m_LowDivider; i < m_HighDivider; i++)
        {
            //for each UtilS4 clk divider
            //   ns/clk =  sec/clk * ns/sec
            // 4x inc in freq needed.
            float ns_per_utilsclk4 = 1.0f/m_OneSource0*i*1000000000.0f;

            mu32lvu32i_t pulses = m_PulseMapClocks.begin();
            while(pulses != m_PulseMapClocks.end())
            {
                //for each pulse length
                UINT32   utilsclk4_cnt = pulses->first;
                lvu32_t settings_list = pulses->second;
                settings_list.unique();

                UINT32 pulse_length_ns = (UINT32)(ns_per_utilsclk4*utilsclk4_cnt);
                pulse_length_ns       =  (UINT32)(pulse_length_ns/round_factor*round_factor);

                lvu32i_t settings_it = settings_list.begin();
                while(settings_it != settings_list.end() )
                {
                    settings_it->push_back( i ); //note the oncesource divider to use.
                    settings_it++;
                }

                pulse_map_time[pulse_length_ns].insert(
                    pulse_map_time[pulse_length_ns].end(),
                    settings_list.begin(),
                    settings_list.end());

                pulses++;
            }
        }
        MyPrintf("Pulse List constructed\n");

        mu32lvu32i_t pm_it = pulse_map_time.begin();
        while(pm_it != pulse_map_time.end())
        {
            UINT32 ns  = pm_it->first;
            lvu32_t l = pm_it->second;
            l.unique();
            MyPrintf("Settings that produce a %d nanosecond pulse\n", ns);

            lvu32i_t lit = l.begin();
            while(lit != l.end())
            {
                MyPrintf("    CNT: %10d N: %4d DIV: %4d\n",
                       lit->operator[](0),
                       lit->operator[](1),
                       lit->operator[](2));
                lit++;
            }
            pm_it++;
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
ClockThrottle * GF100ClockThrottleFactory (GpuSubdevice * pGpuSub)
{
    return new GF100ClockThrottle(pGpuSub);
}

//------------------------------------------------------------------------------
void GF100ClockThrottle::SetClockDomainMasks
(
    UINT32 newSwMask,
    UINT32 * pSwMask,
    UINT32 * pHwMask
)
{
    *pSwMask = newSwMask;
    *pHwMask = 0;

    MASSERT(Gpu::ClkDomain_NUM <= 32);
    for (UINT32 i = 0; i < Gpu::ClkDomain_NUM; i++)
    {
        if (0 == (newSwMask & (1<<i)))
            continue;

        switch (i)
        {
            case Gpu::ClkGpc2:
            case Gpu::ClkGpc:
                *pHwMask |= (1<<LW_THERM_CLK_GPCCLK);
                break;
            case Gpu::ClkSys2:
            case Gpu::ClkSys:
                *pHwMask |= (1<<LW_THERM_CLK_SYSCLK);
                break;
            case Gpu::ClkLeg:
                *pHwMask |= (1<<LW_THERM_CLK_LEGCLK);
                break;
            case Gpu::ClkLtc2:
            case Gpu::ClkLtc:
                *pHwMask |= (1<<LW_THERM_CLK_LTCCLK);
                break;
            default:
                Printf(Tee::PriWarn, "Unsupported domain %d.\n", i);
                *pSwMask &= ~(1<<i);
                break;
        }
    }
}

//------------------------------------------------------------------------------
/* virtual */
RC GF100ClockThrottle::Grab()
{
    for (int c = 0; c < LW_THERM_CLK_COUNT; c++)
    {
        SaveReg(LW_THERM_CLK_SLOWDOWN_0(c));
        SaveReg(LW_THERM_CLK_SLOWDOWN_1(c));
        SaveReg(LW_THERM_CLK_TIMING_0(c));
    }
    SaveReg(LW_THERM_CTRL_1);
    SaveReg(LW_THERM_CONFIG1);
    SaveReg(LW_THERM_CONFIG2);
    SaveReg(LW_PBUS_POWERCTRL_2);

    return ClockThrottle::Grab();
}

//------------------------------------------------------------------------------
RC GF100ClockThrottle::Release()
{
    StickyRC firstRc;

    RestoreClock();

    // Restore registers and RM thermal mgmt.
    firstRc = ClockThrottle::Release();

    return firstRc;
}

RC GF100ClockThrottle::SaveRegs()
{
    m_CTRL_1  = GpuSub()->RegRd32(LW_THERM_CTRL_1);
    m_CONFIG1 = GpuSub()->RegRd32(LW_THERM_CONFIG1);
    m_CONFIG2 = GpuSub()->RegRd32(LW_THERM_CONFIG2);

    MyPrintf("reading THERM_CTRL_1  [%08x] -> %08x\n", LW_THERM_CTRL_1, m_CTRL_1);
    MyPrintf("reading THERM_CONFIG1 [%08x] -> %08x\n", LW_THERM_CONFIG1, m_CONFIG1);
    MyPrintf("reading THERM_CONFIG2 [%08x] -> %08x\n", LW_THERM_CONFIG2, m_CONFIG2);

    for (int c = 0; c < LW_THERM_CLK_COUNT; c++)
    {
        // There are 4 sets of domain-slowdown regs, we will use only
        // the ones with set-bits in m_HwDomainMask unless user overrides us.
        //
        // Messing with clocks other than GPC seems to pretty frequently hang
        // the gpu.
        if (0 == (m_HwDomainMask & (1<<c)))
            continue;

        MyPrintf("reading THERM_CLK_SLOWDOWN_0[%d] [%08x] -> %08x\n",
                 c, LW_THERM_CLK_SLOWDOWN_0(c), m_CLK_SLOWDOWN_0[c]);
        MyPrintf("reading THERM_CLK_SLOWDOWN_1[%d] [%08x] -> %08x\n",
                 c, LW_THERM_CLK_SLOWDOWN_1(c), m_CLK_SLOWDOWN_1[c]);
        MyPrintf("reading THERM_CLK_TIMING_0[%d]   [%08x] -> %08x\n",
               c, LW_THERM_CLK_TIMING_0(c), m_CLK_TIMING_0[c]);

        m_CLK_SLOWDOWN_0[c] = GpuSub()->RegRd32(LW_THERM_CLK_SLOWDOWN_0(c));
        m_CLK_SLOWDOWN_1[c] = GpuSub()->RegRd32(LW_THERM_CLK_SLOWDOWN_1(c));
        m_CLK_TIMING_0[c]   = GpuSub()->RegRd32(LW_THERM_CLK_TIMING_0(c));
    }

    // Disable all automatic overtemp protection.
    m_CTRL_1 = FLD_SET_DRF(_THERM, _CTRL_1, _USE_EXT_OVERT, _OFF, m_CTRL_1);
    m_CTRL_1 = FLD_SET_DRF(_THERM, _CTRL_1, _USE_EXT_ALERT, _OFF, m_CTRL_1);
    m_CTRL_1 = FLD_SET_DRF(_THERM, _CTRL_1, _USE_POWER_ALERT, _OFF, m_CTRL_1);
    m_CTRL_1 = FLD_SET_DRF(_THERM, _CTRL_1, _USE_OVERT, _OFF, m_CTRL_1);
    m_CTRL_1 = FLD_SET_DRF(_THERM, _CTRL_1, _USE_RLHN, _OFF, m_CTRL_1);
    m_CTRL_1 = FLD_SET_DRF(_THERM, _CTRL_1, _USE_ALERT1H, _OFF, m_CTRL_1);
    m_CTRL_1 = FLD_SET_DRF(_THERM, _CTRL_1, _USE_ALERT2H, _OFF, m_CTRL_1);
    m_CTRL_1 = FLD_SET_DRF(_THERM, _CTRL_1, _USE_ALERT3H, _OFF, m_CTRL_1);
    m_CTRL_1 = FLD_SET_DRF(_THERM, _CTRL_1, _USE_ALERT4H, _OFF, m_CTRL_1);
    m_CTRL_1 = FLD_SET_DRF(_THERM, _CTRL_1, _OTOB_ENABLE, _OFF, m_CTRL_1);

    // Turn off idle slowdown.
    for (int c = 0; c < LW_THERM_CLK_COUNT; c++)
    {
        if (0 == (m_HwDomainMask & (1<<c)))
            continue;

        m_CLK_SLOWDOWN_0[c] = FLD_SET_DRF(_THERM, _CLK_SLOWDOWN_0,
                _IDLE_FACTOR, _DISABLED, m_CLK_SLOWDOWN_0[c]);
    }
    return OK;
}

RC GF100ClockThrottle::SetRegs()
{

    MyPrintf("writing THERM_CTRL_1   [%08x] <- %08x\n", LW_THERM_CTRL_1, m_CTRL_1);
    MyPrintf("writing THERM_CONFIG1 [%08x] <- %08x\n", LW_THERM_CONFIG1, m_CONFIG1);
    MyPrintf("writing THERM_CONFIG2 [%08x] <- %08x\n", LW_THERM_CONFIG2, m_CONFIG2);

    GpuSub()->RegWr32(LW_THERM_CTRL_1,  m_CTRL_1);
    GpuSub()->RegWr32(LW_THERM_CONFIG1, m_CONFIG1);
    GpuSub()->RegWr32(LW_THERM_CONFIG2, m_CONFIG2);

    for (int c = 0; c < LW_THERM_CLK_COUNT; c++)
    {
        if (0 == (m_HwDomainMask & (1<<c)))
            continue;

        MyPrintf("writing THERM_CLK_SLOWDOWN_0[%d] [%08x] <- %08x\n",
                 c, LW_THERM_CLK_SLOWDOWN_0(c), m_CLK_SLOWDOWN_0[c]);
        MyPrintf("writing THERM_CLK_SLOWDOWN_1[%d] [%08x] <- %08x\n",
                 c, LW_THERM_CLK_SLOWDOWN_1(c), m_CLK_SLOWDOWN_1[c]);
        MyPrintf("writing THERM_CLK_TIMING_0[%d]   [%08x] <- %08x\n",
               c, LW_THERM_CLK_TIMING_0(c), m_CLK_TIMING_0[c]);

        GpuSub()->RegWr32(LW_THERM_CLK_SLOWDOWN_0(c), m_CLK_SLOWDOWN_0[c]);
        GpuSub()->RegWr32(LW_THERM_CLK_SLOWDOWN_1(c), m_CLK_SLOWDOWN_1[c]);
        GpuSub()->RegWr32(LW_THERM_CLK_TIMING_0(c),   m_CLK_TIMING_0[c]);
    }

    return OK;
}

//------------------------------------------------------------------------------
//obtain nearest frequency via div_count pair
pair<FLOAT32,UINT32> GF100ClockThrottle::NearestDivCount  (UINT32 Hz)
{
    vu32i_t it = lower_bound(m_FreqVect.begin(), m_FreqVect.end(), Hz);
    MyPrintf("Hz: %d N: %d\n", Hz, *it);
    UINT32 new_hz = 0;
    if(      it == m_FreqVect.begin()) new_hz = *it;
    else if( it == m_FreqVect.end())   new_hz = *--it;
    else
    {
        UINT32 new_hz_delta_1 = *it - Hz;
        UINT32 new_hz_delta_2 = Hz  - *--it;
        MyPrintf("D1: %d D2: %d\n", new_hz_delta_1, new_hz_delta_2);
        if (new_hz_delta_1 >= new_hz_delta_2) new_hz = *it;
        else                                  new_hz = *++it;
    }

    return m_MakeFreq[new_hz];
}
//obtain nearest duty based on count, requested duty
LwU8  GF100ClockThrottle::NearestDuty   (float count, float f_duty_255)
{
    vu8_t available = ValidDuties(static_cast<UINT32>(count));
    vu8i_t it = lower_bound(available.begin(), available.end(), f_duty_255);

    if(      it == available.begin()) return *it;
    else if( it == available.end())   return *--it;
    else
    {
        float delta_1 =  *it - f_duty_255;
        float delta_2 =  f_duty_255 - *--it;
        if (delta_1 >= delta_2) return *it;
        else                    return *++it;
    }
}

//obtain nearest slowidx
LwU8  GF100ClockThrottle::NearestSlowIdx(float slowf)
{
    float slowR = 1.0f/slowf;
    vf32i_t it = lower_bound(m_Slows.begin(), m_Slows.end(), slowR);

    if(      it == m_Slows.begin()) return m_SlowMap[*it];
    else if( it == m_Slows.end())   return m_SlowMap[*--it];
    else
    {
        float delta_1 = *it   - slowR;
        float delta_2 = slowR - *--it;
        if (delta_1 >= delta_2) return m_SlowMap[*it];
        else                    return m_SlowMap[*++it];
    }
}

//obtain nearest slewidx
LwU8  GF100ClockThrottle::NearestSlewIdx(float slew)
{
    vu32i_t it = lower_bound(m_Slews.begin(), m_Slews.end(), slew);

    if(      it == m_Slews.begin()) return m_SlewMap[*it];
    else if( it == m_Slews.end())   return m_SlewMap[*--it];
    else
    {
        float delta_1 = *it   - slew;
        float delta_2 =  slew - *--it;
        if (delta_1 >= delta_2) return m_SlewMap[*it];
        else                    return m_SlewMap[*++it];
    }
}

/* virtual */
RC GF100ClockThrottle::Set
(
    const ClockThrottle::Params * pReq,
    ClockThrottle::Params *       pAct
)
{
    if (!m_Init)
    {
        InitTables();
        m_Init = true;
    }

    MASSERT(pReq);
    MASSERT(pAct);
    MASSERT(pReq->DutyPct <= 100.0);
    MASSERT(pReq->SlowFrac > 0.0 && pReq->SlowFrac <= 1.0);

    RC rc;

    //Setup SDIV14 mode early - this appears to prevent a glitch in a high power app situation.
    // TODO: is this delay enough?
    UINT32 clk_div =  GpuSub()->RegRd32(LW_PTRIM_SYS_UTILSCLK_OUT_LDIV);
    clk_div = FLD_SET_DRF_NUM(_PTRIM, _SYS_UTILSCLK_OUT_LDIV, _LOAD_CIP,   2, clk_div);//_INIT
    clk_div = FLD_SET_DRF_NUM(_PTRIM, _SYS_UTILSCLK_OUT_LDIV, _GATECLKDLY, 1, clk_div);//_INIT
    clk_div = FLD_SET_DRF_NUM(_PTRIM, _SYS_UTILSCLK_OUT_LDIV, _SDIV14,     1, clk_div);//_INDIV4_MODE
    GpuSub()->RegWr32(LW_PTRIM_SYS_UTILSCLK_OUT_LDIV,clk_div);

    CHECK_RC(SaveRegs());

    memset(pAct, 0, sizeof(*pAct));
    // HW accepts an 8 bit value for duty.  Determine exactly
    // what the user was requesting so we can match it as closely
    // as possible.
    //  TODO: Convention wisdom has 127 = 50%, 255 = 100%.
    // This would have 0 be some fraction of a percent (.392%)
    // 1.) is this the behavior for cnt>=256?
    // 2.) what happens when cnt<256?
    // value             0           127          255
    // utilsclk high     100/256%    50%          100%
    // cnt 256 high      1           128          256
    // cnt 64  high      ?           32           64

    float f_duty_255 = (256.0f * pReq->DutyPct / 100.0f) - 1.0f;

    pf32u32_t  div_count = NearestDivCount(pReq->PulseHz);
    FLOAT32 OneSourceDivider = div_count.first;
    UINT32  PulseCount       = div_count.second;

    LwU8            duty = NearestDuty    (static_cast<float>(PulseCount), f_duty_255);
    UINT32        slowidx = NearestSlowIdx (pReq->SlowFrac);
    UINT32        slewidx = NearestSlewIdx (static_cast<float>(pReq->GradStepDur));

    //FIXME: for now; Do not support Slew == 0, default to slew 4
    if (slewidx == 0) slewidx = 1;

    //access LUT to determine if the requested waveform can be produced,
    // or if a corrective factor is needed (in the case that there not enough
    // time during a high/low duty segment to satisfy the request slow/slew.)
    // We will correct by reducing the Slow Factor until the timing can be satisfied.
    // We had the option to correct via Slew, but this produces a more aggressive
    // waveform, while correcting through slow factor produces a less aggressive
    // waveform.
    pu32u8_t temp_p = pu32u8_t(PulseCount, duty);
    UINT32 temp      = m_Lut[temp_p];
    vvu8_t &table   = m_TabList[temp];

    //index LUT for corrective factor
    LwU8 corr =  table[ slowidx ][ slewidx ] ;

    // the actual settings we are going to use.
    // x4 multiplier needed for some unknown reason.
    pAct->PulseHz = m_ActFreq =  (UINT32)(m_OneSource0/OneSourceDivider/PulseCount)  /* /4 */;
    pAct->DutyPct     = (duty+1)/256.0f * 100.0f;
    pAct->SlowFrac    = 1.0f/m_Slows[slowidx-corr];
    pAct->GradStepDur = m_Slews[slewidx];

    CHECK_RC(SetupNewClock(OneSourceDivider));
    MyPrintf("CNT: %d CNTM: %d DTY: %d SLO: %d SLE: %d\n",
           PulseCount,
           m_CountMap[PulseCount],
           duty,
           slowidx-corr,
           slewidx);
    CHECK_RC(SetupNewValues(PulseCount, duty, slowidx-corr, slewidx));
    CHECK_RC(SetRegs());

    return rc;
}

RC GF100ClockThrottle::SetupNewValues(UINT32 count, UINT32 duty, UINT32 slowidx, UINT32 slewidx)
{
    UINT32 slew_enable = (slewidx !=0) ? 1 : 0;

    UPD(_THERM, _CONFIG2, _SLOWDOWN_FACTOR_EXTENDED, 1);
    UPD(_THERM, _CONFIG2, _LOAD_STEP_PWM_PERIOD,     7);

    UPD(_THERM, _CONFIG2, _GRAD_ENABLE,              slew_enable);
    UPD(_THERM, _CONFIG2, _GRAD_STEP_DURATION,       (slewidx==0)?0:slewidx-1);
    UPD(_THERM, _CONFIG2, _GRAD_PWM_PERIOD,          m_CountMap[count]);
    for (int c = 0; c < LW_THERM_CLK_COUNT; c++)
    {
        if (0 == (m_HwDomainMask & (1<<c)))
            continue;
        UPDIDX(_THERM, _CLK_TIMING_0,   _GRAD_SLOWDOWN,   slew_enable, c);
        UPDIDX(_THERM, _CLK_SLOWDOWN_1, _SW_THERM_FACTOR,     slowidx, c);
        UPDIDX(_THERM, _CLK_SLOWDOWN_1, _SW_THERM_PWM,           duty, c);
    }

    return OK;
}

UINT32 GF100ClockThrottle::RestoreClock()
{
    UINT32 clk_switch = GpuSub()->RegRd32(LW_PTRIM_SYS_UTILSCLK_OUT_SWITCH );
    MyPrintf("reading UTILSCLK_OUT_SWITCH [%08x] -> %08x\n", LW_PTRIM_SYS_UTILSCLK_OUT_SWITCH, clk_switch);

    clk_switch = FLD_SET_DRF_NUM(_PTRIM, _SYS_UTILSCLK_OUT_SWITCH,  _FINALSEL, 0, clk_switch);//_SLOWCL
    clk_switch = FLD_SET_DRF_NUM(_PTRIM, _SYS_UTILSCLK_OUT_SWITCH,  _ONESRCCLK,0, clk_switch);//onesrc0
    clk_switch = FLD_SET_DRF_NUM(_PTRIM, _SYS_UTILSCLK_OUT_SWITCH,  _SLOWCLK,  3, clk_switch);//_XTAL4X

    MyPrintf("writing UTILSCLK_OUT_SWITCH [%08x] <- %08x\n", LW_PTRIM_SYS_UTILSCLK_OUT_SWITCH, clk_switch);
    GpuSub()->RegWr32(LW_PTRIM_SYS_UTILSCLK_OUT_SWITCH, clk_switch );
    return clk_switch;
}

RC GF100ClockThrottle::SetupNewClock(FLOAT32 div)
{
    //default back to XTAL4 while we mess with multiplier
    UINT32 clk_switch = RestoreClock();

    UINT32 hwval = (UINT32)((div-1)*2);

    //setup divisor for ONESOURCE0
    UINT32 clk_div =  GpuSub()->RegRd32(LW_PTRIM_SYS_UTILSCLK_OUT_LDIV);
    MyPrintf("reading UTILSCLK_OUT_LDIV [%08x] -> %08x\n", LW_PTRIM_SYS_UTILSCLK_OUT_LDIV, clk_div);
    clk_div = FLD_SET_DRF_NUM(_PTRIM, _SYS_UTILSCLK_OUT_LDIV, _ONESRC0DIV, hwval, clk_div);
    clk_div = FLD_SET_DRF_NUM(_PTRIM, _SYS_UTILSCLK_OUT_LDIV, _ONESRC1DIV, hwval, clk_div);

    MyPrintf("writing UTILSCLK_OUT_LDIV [%08x] <- %08x\n", LW_PTRIM_SYS_UTILSCLK_OUT_LDIV, clk_div);
    GpuSub()->RegWr32(LW_PTRIM_SYS_UTILSCLK_OUT_LDIV, clk_div );

    //switch to ONESOURCE0
    clk_switch = FLD_SET_DRF_NUM(_PTRIM, _SYS_UTILSCLK_OUT_SWITCH,  _FINALSEL, 3, clk_switch);//ONESRCCLK
    clk_switch = FLD_SET_DRF_NUM(_PTRIM, _SYS_UTILSCLK_OUT_SWITCH,  _ONESRCCLK,0, clk_switch);//onesource0
    clk_switch = FLD_SET_DRF_NUM(_PTRIM, _SYS_UTILSCLK_OUT_SWITCH,  _SLOWCLK,  3, clk_switch);//_XTAL4X

    MyPrintf("writing UTILSCLK_OUT_SWITCH [%08x] <- %08x\n", LW_PTRIM_SYS_UTILSCLK_OUT_SWITCH, clk_switch);

    GpuSub()->RegWr32(LW_PTRIM_SYS_UTILSCLK_OUT_SWITCH, clk_switch );
    return OK;
}

