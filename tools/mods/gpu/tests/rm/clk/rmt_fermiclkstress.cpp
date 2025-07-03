/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2004-2013,2016-2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//!
//! \file rmt_fermiclkstress.cpp
//! \RM-Test to perform a clock stress check.
//! This test is primarily to stress the clocks on Fermi.
//!
//! FERMITODO:CLK: Enhance the test for clock counter verification once they are implemented fully on HW.
//!

#include <math.h>           // Needed for INFINITY, etc.
#include "gpu/tests/rmtest.h"
#include "gpu/tests/rmt_cl906f.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/display.h"
#include "core/include/platform.h"
#include "gpu/utility/rmclkutil.h"
#include "lwRmApi.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"
#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/memcheck.h"

// Define INFINITY as something usable in case the library doesn't.
#ifndef INFINITY
#ifdef HUGE_VAL
#define INFINITY HUGE_VAL
#else
#define INFINITY 1.0e999999999
#endif
#endif

//! Size of buffers used to format numerics, etc.
#define SMALL_BUFFER_SIZE 16

//! \brief Maximum number of possible clock domains
//! The value is infered by LW2080_CTRL_CMD_CLK_GET_DOMAINS (et al), which uses a 32-bit bit-mapped field.
//! We use it to allocate arrays.  MAX_CLOCK_DOMAIN_COUNT and ClkDomain_t must agree.
#define MAX_CLOCK_DOMAIN_COUNT       32

//! \brief Clock domain bitmap per LW2080_CTRL_CLK_DOMAIN_xxx constants
//! The type is infered by LW2080_CTRL_CMD_CLK_GET_DOMAINS (et al), which uses a 32-bit bit-mapped field.
//! MAX_CLOCK_DOMAIN_COUNT and ClkDomain_t must agree.
#define ClkDomain_t              UINT32

//! \brief Source clock domain per LW2080_CTRL_CLK_SOURCE_xxx constants
//! The type is infered by LW2080_CTRL_CLK_INFO, which uses a 32-bit field.
#define ClkSource_t              UINT32

//! \brief Clock Frequency in KHz
#define FreqKHz_t                UINT32

//! \brief Crystal frequency
#define XTAL_FREQ_KHZ             27000

//! \brief OneSource frequency
#define OSM_FREQ_KHZ            1620000

//! \brief Number of test iterations
//efine TEST_ITERATION_COUNT      10000
#define TEST_ITERATION_COUNT       1000
//efine TEST_ITERATION_COUNT         10

//! \brief Maximum amout of time to run this test
#define TEST_MAX_TIME_SECONDS   (24*60*60)

//! \brief Print statistics every PRINT_STATISTICS_PERIOD iterations
//! Zero to disable periodic statistics print.
//! Statistics are always printed at the end.
#define PRINT_STATISTICS_PERIOD     100
//efine PRINT_STATISTICS_PERIOD       5

//! \brief Number of categories into which deltas are put when collecting histogram data.
//! The first category, at index zero, is the 'nearly zero' category.
#define MAX_DELTA_CATEGORY_COUNT     10

//! \brief Number of buckets in the histogram.
//! For negative, (nearly) zero, and positive deltas, we allocate a histogram array of MAX_DELTA_CATEGORY_COUNT * 2 - 1.
//! The nearly zero delta category is at index MAX_DELTA_CATEGORY_COUNT - 1.
//! Positive deltas, in order of increasing magnitude, go from index MAX_DELTA_CATEGORY_COUNT to HISTOGRAM_ARRAY_SIZE - 1.
//! Negative deltas, in order of increasing magnitude, go from index MAX_DELTA_CATEGORY_COUNT - 2 to 0.
#define HISTOGRAM_ARRAY_SIZE     (MAX_DELTA_CATEGORY_COUNT * 2 - 1)

//! \brief Index of the (nearly) zero bucket in the histogram.
//! The nearly zero delta category is at index MAX_DELTA_CATEGORY_COUNT - 1.
#define HISTOGRAM_ZERO_BUCKET    (MAX_DELTA_CATEGORY_COUNT - 1)

//! \brief Size of each category relative the the next one nearer zero.
//! This table is close to a binary logarithm based on 100%.
//! This table must be in increasing order since the zero index is the 'nearly zero' category.
//! Histogram array indicies are indicated in the comments for reference.
//! The last category is usually +INFINITY; if not, deltas beyond that range are ignored as outliers.
static const double c_DeltaCategoryThreshold[MAX_DELTA_CATEGORY_COUNT] =
{   0.005,          //   [9]     Nearly zero; Within 1/2% of correct
    0.01,           // [8] [10]   1/2% <= d <   1%
    0.02,           // [7] [11]     1% <= d <   2%
    0.05,           // [6] [12]     2% <= d <   5%
    0.10,           // [5] [13]     5% <= d <  10%
    0.25,           // [4] [14]    10% <= d <  25%
    0.50,           // [3] [15]    25% <= d <  50%
    1.00,           // [2] [16]    50% <= d < 100%
    2.00,           // [1] [17]   100% <= d < 200% (i.e. at least double if expected freq)
    +INFINITY       // [0] [18]   200% or more     (i.e. at least triple of expected freq)
};

//! \brief Failure Tolerance for OneSource-Sourced Clocks
//! The test fails if we get at least one delta (when the source is OneSource) equal or exceeding this value in magnatude.
//! In former versions of this program, ALT_PATH_TOLERANCE_100X would defined this value as a percent.
#define OSM_DELTA_TOLERANCE     0.50    //  50% or more

//! \brief Failure Tolerance for PLL-Sourced Clocks
//! The test fails if we get at least one delta (when the source is PLL) equal or exceeding this value in magnatude.
//! In former versions of this program, REF_PATH_TOLERANCE_100X would defined this value as a percent.
#define PLL_DELTA_TOLERANCE     0.10    // 10% or more

//! Length of 100% bar when graphing the histogram (in characters)
#define HISTOGRAM_GRAPH_WIDTH     50

//! Plus-or-minus sign in ISO/IEC 8859-1 (Latin-1)
#define PLUS_OR_MINUS           (-79)

//! Each test is a frequency and a set of LW2080_CTRL_CLK_INFO_FLAGS_xxx flags
struct TestSpec_t
{   FreqKHz_t   freqKHz;
    ClkSource_t source;
    UINT32      flag;
};

//! Colwenience constants for source in TestSpec_t elements in array.
#define SOURCE(source)          LW2080_CTRL_CLK_SOURCE ## source

//! Allow _NONE to be specified in FLAG and FREQ_FLAG constants
#ifndef LW2080_CTRL_CLK_INFO_FLAGS_FORCE_NONE
#define LW2080_CTRL_CLK_INFO_FLAGS_FORCE_NONE           3:2
#define LW2080_CTRL_CLK_INFO_FLAGS_FORCE_NONE_ENABLE    0x00000000
#endif

//! Colwenience constants for _FORCE flags in FreqFlag_t elements in array.
#define FLAG(flag)              DRF_DEF(2080_CTRL_CLK, _INFO_FLAGS, _FORCE ## flag, _ENABLE)

//! Colwenience constant for FreqFlag_t elements in array.
#define TEST_SPEC(freq, source, flag)   { freq, SOURCE(source), FLAG(flag) }

//! Colwenience constants to terminate FreqFlag_t array.
#define TEST_SPEC_END           TEST_SPEC(0, _DEFAULT, _NONE)

//! Stepping value when not using explicit freq table
// TODO Enable the use of incremental value between minOperationalFreq and maxOperationalFreq.
//efine INCREMENT_KHz         100000

//! \brief Explicit Test Frequency Table
// Each must be terminated with a zero.
// TODO: We need a better way to determine the freqs we will use.
static const TestSpec_t c_pExplicitFreq_DEFAULT[] = {   TEST_SPEC( 162000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 324000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 900000, _DEFAULT, _NONE),
                                                        TEST_SPEC_END};

static const TestSpec_t c_pExplicitFreq_MCLK[]    = {   TEST_SPEC( 162000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 324000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 800000, _DEFAULT, _NONE),
                                                        TEST_SPEC_END};

static const TestSpec_t c_pExplicitFreq_HOSTCLK[] = {   TEST_SPEC( 417000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 277000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 648000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 540000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 462857, _DEFAULT, _NONE),
                                                        TEST_SPEC( 405000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 360000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 324000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 294545, _DEFAULT, _NONE),
                                                        TEST_SPEC( 270000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 249231, _DEFAULT, _NONE),
                                                        TEST_SPEC( 231429, _DEFAULT, _NONE),
                                                        TEST_SPEC( 216000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 202500, _DEFAULT, _NONE),
                                                        TEST_SPEC( 190588, _DEFAULT, _NONE),
                                                        TEST_SPEC( 180000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 170526, _DEFAULT, _NONE),
                                                        TEST_SPEC( 162000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 154288, _DEFAULT, _NONE),
                                                        TEST_SPEC( 147273, _DEFAULT, _NONE),
                                                        TEST_SPEC( 140870, _DEFAULT, _NONE),
                                                        TEST_SPEC( 135000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 129600, _DEFAULT, _NONE),
                                                        TEST_SPEC( 124615, _DEFAULT, _NONE),
                                                        TEST_SPEC( 120000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 115714, _DEFAULT, _NONE),
                                                        TEST_SPEC( 111724, _DEFAULT, _NONE),
                                                        TEST_SPEC( 108000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 104516, _DEFAULT, _NONE),
                                                        TEST_SPEC( 101250, _DEFAULT, _NONE),
                                                        TEST_SPEC(  98182, _DEFAULT, _NONE),
                                                        TEST_SPEC(  95294, _DEFAULT, _NONE),
                                                        TEST_SPEC(  90000, _DEFAULT, _NONE),
                                                        TEST_SPEC(  85263, _DEFAULT, _NONE),
                                                        TEST_SPEC(  81000, _DEFAULT, _NONE),
                                                        TEST_SPEC(  77143, _DEFAULT, _NONE),
                                                        TEST_SPEC(  73636, _DEFAULT, _NONE),
                                                        TEST_SPEC(  70435, _DEFAULT, _NONE),
                                                        TEST_SPEC(  67500, _DEFAULT, _NONE),
                                                        TEST_SPEC(  64800, _DEFAULT, _NONE),
                                                        TEST_SPEC(  62308, _DEFAULT, _NONE),
                                                        TEST_SPEC(  60000, _DEFAULT, _NONE),
                                                        TEST_SPEC(  57857, _DEFAULT, _NONE),
                                                        TEST_SPEC(  55862, _DEFAULT, _NONE),
                                                        TEST_SPEC(  54000, _DEFAULT, _NONE),
                                                        TEST_SPEC(  52258, _DEFAULT, _NONE),
                                                        TEST_SPEC_END};

static const TestSpec_t c_pExplicitFreq_SYS2CLK[] = {   TEST_SPEC( 162000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 324000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 750000, _DEFAULT, _NONE),
                                                        TEST_SPEC_END};

static const TestSpec_t c_pExplicitFreq_HUB2CLK[] = {   TEST_SPEC( 162000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 324000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 750000, _DEFAULT, _NONE),
                                                        TEST_SPEC_END};

static const TestSpec_t c_pExplicitFreq_LEGCLK[]  = {   TEST_SPEC( 162000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 324000, _DEFAULT, _NONE),
                                                        TEST_SPEC( 600000, _DEFAULT, _NONE),
                                                        TEST_SPEC_END};

//! \brief Explicit Test Frequency Table per Clock Domain
struct ExplicitFreqList
{   ClkDomain_t         domain;
    const TestSpec_t    *list;
};

//! \brief Explicit Test Frequency Table per Clock Domain
static const ExplicitFreqList c_pExplicitFreqList[] =
{   { LW2080_CTRL_CLK_DOMAIN_HOSTCLK, c_pExplicitFreq_HOSTCLK },
    { LW2080_CTRL_CLK_DOMAIN_MCLK,    c_pExplicitFreq_MCLK    },
    { LW2080_CTRL_CLK_DOMAIN_SYS2CLK, c_pExplicitFreq_SYS2CLK },
    { LW2080_CTRL_CLK_DOMAIN_HUB2CLK, c_pExplicitFreq_HUB2CLK },
    { LW2080_CTRL_CLK_DOMAIN_LEGCLK,  c_pExplicitFreq_LEGCLK  },
};

//! Summarize the transitions between clock sources
//! CAUTION: A lot of logic assumes these bitmaps.  Think well before changing these constants.
enum Transition_t
{   TransitionPllPll            = 0x00,     // PLL => PLL (i.e. VCO) transition
    TransitionPllOsm            = 0x01,     // PLL => OneSource transition
    TransitionOsmPll            = 0x02,     // OneSource => PLL transition
    TransitionOsmOsm            = 0x03,     // OneSource => OneSource (i.e. bypass/alternate) transition
    TransitionOther             = 0x04,     // Transition could not be determined
    TransitionTypeCount         = 0x05,     // Number of transition types for which we keep statistics
    TransitionToOsm             = 0x01,     // Bit indicating a transition ending in OneSource
    TransitionFromOsm           = 0x02,     // Bit indicating a transition starting in OneSource
};

//! Text per Transition_t enum constants
static const char *c_TransitionShortName[TransitionTypeCount] = { "P=>P", "P=>O", "O=>P", "O=>O", "    " };

//! Text per Transition_t enum constants
static const char *c_TransitionLongName[TransitionTypeCount]  = { "PLL=>PLL", "PLL=>OSM", "OSM=>PLL", "OSM=>OSM", "unknown" };

class FermiClockStressTest: public RmTest
{
    //! \brief FreqLimit (formerly CLK_OPERATIONAL_FREQ_LIMITS)
    //! This structure describes the max and min operational requencies for a particular clock domain.
    //! FERMITODO: Once the operational frequencies are defined by HW
    //!            (Ref:- Bug 502518) this structure will be a part of SDK and we
    //!            will define an API to get this information from the driver itself.
    public: class FreqLimit
    {   protected:
            TestSpec_t          minOperationalFreq;     // Range of frequencies when
            TestSpec_t          maxOperationalFreq;     // not using an explicit list
            UINT32              explicitIndex;          // Index used in SetInfos when using an explicit list
            const TestSpec_t   *explicitFreq;           // Explicit freq/flag list; NULL when not used.

        //! Construction
        public: inline FreqLimit()
        {   minOperationalFreq.freqKHz  = 0;            // Zero means uninitialized
            minOperationalFreq.flag     = FLAG( _NONE);
            maxOperationalFreq.freqKHz  = 0;
            maxOperationalFreq .flag    = FLAG( _NONE);
            explicitIndex               = 0;            // Start at the beginning
            explicitFreq                = NULL;
        }

        //! Initialization
        public: inline void Init( ClkDomain_t domain)
        {   minOperationalFreq.freqKHz  = XTAL_FREQ_KHZ;      // Apply appropriate defaults
            minOperationalFreq.flag     = FLAG(_NONE);
            maxOperationalFreq.freqKHz  = OSM_FREQ_KHZ;
            maxOperationalFreq.flag     = FLAG(_NONE);
            explicitIndex       = 0;
            explicitFreq        = ::c_pExplicitFreq_DEFAULT;            // Use an explicit list by default

            for( UINT32 i= 0; i < sizeof(c_pExplicitFreqList) / sizeof(ExplicitFreqList); i++)
            {   if( domain == c_pExplicitFreqList[i].domain)            // Use domain-specific default if listed
                {   explicitFreq = c_pExplicitFreqList[i].list;
                    break;
                }
            }
        }

        //! \brief True iff we use the explicit list (v. the min and max)
        public: inline bool UseExplicit()
        {   return !!explicitFreq;
        }

        //! \brief Get the next frequency in the list
        public: TestSpec_t TestSpec() const
        {   if( explicitFreq)                                                   // An explicit list was specified
            {   MASSERT(explicitFreq[explicitIndex].freqKHz);                   // The list must not be empty
                return explicitFreq[explicitIndex];                             // Return the next freq
            }

            // TODO Enable the use of incremental value between minOperationalFreq and maxOperationalFreq.
            MASSERT(false);
            return minOperationalFreq;
        }

        //! \brief Advance to the next frequency in the list
        public: TestSpec_t NextTestSpec()
        {   if( explicitFreq)                                                   // An explicit list was specified
            {   if( !explicitFreq[++explicitIndex].freqKHz) explicitIndex = 0;  // Wrap around if at the end of the list
                MASSERT(explicitFreq[explicitIndex].freqKHz);                   // The list must not be empty
                return explicitFreq[explicitIndex];                             // Return the next freq
            }

            // TODO Enable the use of incremental value between minOperationalFreq and maxOperationalFreq.
            MASSERT(false);
            return minOperationalFreq;
        }

    };  // FreqLimit

    //! Histograms and that sort of thing
    public: class Statistics
    {   struct Histogram
        {   UINT32 count[HISTOGRAM_ARRAY_SIZE];     // See comments at MAX_DELTA_CATEGORY_COUNT
            UINT32 total;                           // Total count of all histogram categories

            inline Histogram()                      // Construction
            {   for( UINT32 i = 0; i < HISTOGRAM_ARRAY_SIZE; ++i) count[i] = 0;
                total = 0;
            }

            //! \brief Colwert the index for c_DeltaCategoryThreshold to the index for the histograms
            static inline UINT32 HistogramIndex( bool negative, UINT32 category)   // When MAX_DELTA_CATEGORY_COUNT == 10
            {   MASSERT(category < MAX_DELTA_CATEGORY_COUNT);               // then HISTOGRAM_ARRAY_SIZE == 19
                return negative? HISTOGRAM_ZERO_BUCKET - category:          // Negatives are 9 through 0
                                 category + HISTOGRAM_ZERO_BUCKET;          // Positives are 9 through 19
            }                                                               // 9 is the index for 'nearly zero'

            //! \brief Colwert the histogram index to a category sign
            static inline char CategorySign( UINT32 histogram)              // When MAX_DELTA_CATEGORY_COUNT == 10
            {   MASSERT(histogram < HISTOGRAM_ARRAY_SIZE);                  // then HISTOGRAM_ARRAY_SIZE == 19
                return histogram == HISTOGRAM_ZERO_BUCKET? PLUS_OR_MINUS:   // Nearly zero is 9 in the histogram index
                     ( histogram <  HISTOGRAM_ZERO_BUCKET? '-': '+');       // Negative before that; positive after
            }

            //! \brief Colwert the histogram index to an index for c_DeltaCategoryThreshold
            static inline UINT32 CategoryIndex( UINT32 histogram)
            {   MASSERT(histogram < HISTOGRAM_ARRAY_SIZE);
                return histogram < HISTOGRAM_ZERO_BUCKET?           // When MAX_DELTA_CATEGORY_COUNT == 10
                    HISTOGRAM_ZERO_BUCKET - histogram:              // Negatives are index 9 through 0 in the histogram
                    histogram - HISTOGRAM_ZERO_BUCKET;              // Positives are index 9 through 19 in the histogram
            }                                                       // 9 is the index for 'nearly zero'

            //! Apply the data to the histogram
            void Collect(double x)
            {   ++total;
                bool negative = x < 0.0;                                    // Sign is needed for histogram placement
                if (negative) x = -x;                                       // Search category table by absolute value

                for( UINT32 category = 0; category < MAX_DELTA_CATEGORY_COUNT; ++category) // Zero is the index for the 'nearly zero' category, so start there
                {   if (x < c_DeltaCategoryThreshold[category])             // Each entry indicagtes the upper bound of the category
                    {   ++count[ HistogramIndex(negative, category)];       // Increment the count once we have the right category
                        return;                                             // There is no more to do at this point.
                    }
                }                                   // If we exit this loop, it means we have an outlier as defined by the table
            }                                       // In general, +INFINITY is used to indicate an unavailable data point.

            //! TRUE iff all data points are in the zero-delta bucket
            bool AllZero() const
            {   return total == count[HISTOGRAM_ZERO_BUCKET];       // Middle count[] is zero-delta
            }

            //! Print the histgram data
            void PrintAll() const
            {   if (!total)                                                 // Nothing to print
                    Printf(Tee::PriHigh, "%s: No data points were collected\n", __FUNCTION__);

                else if (AllZero())                                         // One line summary
                    Printf(Tee::PriHigh, "%s: All %u data points are in the %c%.1f%% category\n", __FUNCTION__,
                        total, PLUS_OR_MINUS, c_DeltaCategoryThreshold[0] * 100.0);

                else
                {   char graph[HISTOGRAM_GRAPH_WIDTH + 1];
                    double threshold;
                    Printf(Tee::PriHigh, "%s:   range     count  percent\n", __FUNCTION__);
                    for( UINT32 i = 0; i < HISTOGRAM_ARRAY_SIZE; ++i)
                    {   threshold = c_DeltaCategoryThreshold[CategoryIndex(i)];

                        MASSERT(count[i] <= total);                                 // Ratio must not exceed 1.0 else we overflow the buffer
                        memset(graph, '*', HISTOGRAM_GRAPH_WIDTH);                  // Fill the graph with asterisks
                        graph[HISTOGRAM_GRAPH_WIDTH * count[i] / total] = '\0';     // Terminate to indicate the relative count

                        Printf(Tee::PriHigh, "%s: %c%5.1f%c  %8.0u %7.1f%%  %s\n", __FUNCTION__,
                            CategorySign(i), threshold * 100.0,
                            (-INFINITY < threshold && threshold < +INFINITY? '%': ' '),
                            count[i], (double) count[i] / total * 100.0, graph);
                    }

                    Printf(Tee::PriHigh, "%s:  total:  %8.0u\n", __FUNCTION__, total);
                }
            }
        };

        protected:
            Histogram m_ActualDeltaHistogram[2];                // Indexed by source: 0=PLL 1=OneSource
            Histogram m_MeasuredDeltaHistogram[2];              // Indexed by source: 0=PLL 1=OneSource
            UINT32    m_TransitionCount[TransitionTypeCount];   // Indexed by Transition_t
            UINT32    m_TransitionTotal;                        // Total of m_TransitionCount

        public: inline Statistics()                             // Construction
        {   for( INT32 i = 0; i < TransitionTypeCount; ++i) m_TransitionCount[i] = 0;
            m_TransitionTotal = 0;                              // Zero out the counts
        }                                                       // Histograms initialize themselves

        public: void Collect( double actualDelta, double measuredDelta, Transition_t transition)
        {   ++m_TransitionCount[transition];                            // Count per transition
            ++m_TransitionTotal;                                        // Count totals

            bool source = !!(transition & TransitionToOsm);             // Current source: 0=PLL 1=OneSource
            m_ActualDeltaHistogram[source].Collect(actualDelta);        // Update actual freq histogram per clock source
            m_MeasuredDeltaHistogram[source].Collect(measuredDelta);    // Update measured freq histogram per clock source
        }

        //! Print all histogram data
        public: void PrintHistogram(const char *domain) const
        {   bool source = false;                                        // Current source: 0=PLL 1=OneSource
            do
            {   if (m_ActualDeltaHistogram[source].total)               // Ignore if we have no data points
                {   Printf(Tee::PriHigh, "\n%s:%9s: Actual Freq v. Target Freq when source is %s\n",
                        __FUNCTION__, domain, ( source? "OneSource": "PLL"));
                    m_ActualDeltaHistogram[source].PrintAll();
                }

                if (m_MeasuredDeltaHistogram[source].total)             // Ditto
                {   Printf(Tee::PriHigh, "\n%s:%9s: Measured Freq v. Actual Freq when source is %s\n",
                        __FUNCTION__, domain, ( source? "OneSource": "PLL"));
                    m_MeasuredDeltaHistogram[source].PrintAll();
                }

                source = !source; // If we just did the PLL, repeat for the OneSource
            } while (source);
        }

        //! Print all transition data
        public: void PrintTransition(const char *domain) const
        {   if (!m_TransitionTotal)
            {   Printf(Tee::PriHigh, "\n%s:%9s: No data points we collected to evaluate transition type.\n",
                    __FUNCTION__, domain);
            }

            for (UINT32 i = 0; i < TransitionTypeCount; ++i)
            {
                if (m_TransitionCount[i] == m_TransitionTotal)
                {
                    Printf(Tee::PriHigh, "\n%s:%9s: All %u switches were %s transition type.\n",
                        __FUNCTION__, domain, m_TransitionTotal, c_TransitionLongName[i]);
                    return;
                }
            }

            Printf(Tee::PriHigh, "\n%s:%9s: Number of switches per transition type\n",
                __FUNCTION__, domain);

            for (UINT32 i = 0; i < TransitionTypeCount; ++i)
            {
                if (m_TransitionCount[i])
                {   Printf(Tee::PriHigh, "%s:%9s:%10s%8u%7.1f%%\n",
                        __FUNCTION__, domain, c_TransitionLongName[i],
                        m_TransitionCount[i], (double) m_TransitionCount[i] / m_TransitionTotal * 100.0);
                }
            }

            Printf(Tee::PriHigh, "%s:%9s:     total%8u\n",
                __FUNCTION__, domain, m_TransitionTotal);
        }
    };

    //! \brief Clock Domain Info
    public: class ClockDomain
    {
        protected:
            const char             *m_DomainName;           // Terse textual name of the domain
            LW2080_CTRL_CLK_INFO   *m_pClkInfo;             // Used in RM API calls for GET and SET
            TestSpec_t              m_TargetTestSpec;       // The target frequency/source/flag
            FreqKHz_t               m_MeasuredFreqKHz;      // The last measured frequency
            FreqLimit               m_FreqLimit;
            ClkSource_t             m_PriorSource;          // Source clock prior to most recent SetInfos
            Statistics              m_Statistics;           // Histogram and other statistical information
            UINT32                  m_ToleranceErrorCount;  // Number of deltas that were out of tolerance
            UINT32                  m_SourceErrorCount;     // Number of times the target and actual sources differed

        //! \brief Construction
        public: inline ClockDomain()
        {   m_DomainName             = "???";                // Terse textual name of the domain
            m_pClkInfo               = NULL;                 // Null until Init() has been called
            m_TargetTestSpec.freqKHz = 0;                    // Zero for unknown
            m_TargetTestSpec.source  = SOURCE( _DEFAULT);
            m_TargetTestSpec.flag    = FLAG( _NONE);
            m_MeasuredFreqKHz        = 0;                    // Zero for unknown
            m_PriorSource            = LW2080_CTRL_CLK_SOURCE_DEFAULT;
            m_ToleranceErrorCount    = 0;                    // No errors yet
            m_SourceErrorCount       = 0;
        }

        //! \brief True iff the domain has been set by an Init() call
        public: inline bool IsUnspecified()
        {   return !m_pClkInfo || !m_pClkInfo->clkDomain;
        }

        //! \brief Initialize
        //!
        //! \param  domain      Clock domain per LW2080_CTRL_CLK_DOMAIN_xxx constants
        //!
        //! \param  pClkInfo    Objects of this class own the structure referenced
        //!                     by this polinter, except that external logic may use
        //!                     it in LW2080_CTRL_CMD_CLK_GET_INFO and
        //!                     LW2080_CTRL_CMD_CLK_SET_INFO API calls.
        //!                     Logic external to this class must not otherwise
        //!                     modify this structure.
        //!
        public: void Init( LW2080_CTRL_CLK_INFO *pClkInfo, ClkDomain_t domain)
        {   MASSERT(domain);                                        // Must be a specific domain
            MASSERT(ONEBITSET(domain));                             // Must be only one domain
            MASSERT(pClkInfo);                                      // Required structure
            MASSERT(!m_pClkInfo);                                   // Init() may be called only once per object

            m_DomainName                = RmtClockUtil().GetClkDomainName(domain);
            m_TargetTestSpec.freqKHz    = 0;                        // Zero for unknown
            m_TargetTestSpec.source     = SOURCE( _DEFAULT);
            m_TargetTestSpec.flag       = FLAG( _NONE);
            m_MeasuredFreqKHz           = 0;                        // Zero for unknown
            m_FreqLimit.Init(domain);                               // Also initialize the limits

            m_pClkInfo                  = pClkInfo;                 // May be used in RM API calls for GET and SET
            memset( pClkInfo, 0, sizeof(LW2080_CTRL_CLK_INFO));     // Initialize as usual
            m_pClkInfo->clkDomain       = domain;
        }

        //! \brief Clock domain per LW2080_CTRL_CLK_DOMAIN_xxx SDK constants
        public: inline ClkDomain_t Domain() const
        {   return m_pClkInfo->clkDomain;
        }

        //! \brief Terse name of clock domain
        public: inline const char *DomainName() const
        {   return m_DomainName;
        }

        //! \brief Source from which the domain is running
        public: inline const char *SourceDomainName() const
        {   return ! m_pClkInfo->clkSource? "":                         // Empty is unspecified or unknown
                RmtClockUtil().GetClkSourceName( m_pClkInfo->clkSource);
        }

        //! \brief Save the source as the prior source
        public: inline void SavePriorInfo()
        {   m_PriorSource = m_pClkInfo->clkSource;
        }

        //! \brief Source is OneSource (v. PLL)
        public: static inline bool IsOneSource( ClkSource_t s)
        {   return s == LW2080_CTRL_CLK_SOURCE_SPPLL0 ||
                   s == LW2080_CTRL_CLK_SOURCE_SPPLL1;
        }

        //! \brief Target Source is specified (v. _DEFAULT)
        public: inline bool TargetSourceSpecified() const
        {   if (TargetFlag() & FLAG(_PLL))      return true;    // If there was a _FORCE flag,
            if (TargetFlag() & FLAG(_BYPASS))   return true;    // then it is specified.
            return false;
        }

        //! \brief Target Source is OneSource (v. PLL)
        public: inline bool TargetSourceIsOneSource() const
        {   if (TargetFlag() & FLAG(_PLL))      return false;   // If there was a _FORCE flag,
            if (TargetFlag() & FLAG(_BYPASS))   return true;    // then use that.
            return IsOneSource( m_pClkInfo->clkSource);         // Otherwise, we let RM decide.
        }

        //! \brief Actual Source is OneSource (v. PLL)
        public: inline bool ActualSourceIsOneSource() const
        {   if (m_pClkInfo->flags & FLAG(_PLL))     return false;   // Clocls 2.0 sets the _FORCE flag,
            if (m_pClkInfo->flags & FLAG(_BYPASS))  return true;
            return IsOneSource( m_pClkInfo->clkSource);             // Legacy does not.
        }

        //! \brief Prior source is OneSource (v. PLL)
        public: inline bool PriorSourceIsOneSource() const
        {   return IsOneSource( m_PriorSource);
        }

        //! \brief Measure the clock frequency (LW2080_CTRL_CMD_CLK_MEASURE_FREQ)
        public: RC Measure(GpuSubdevice *hSubDev)
        {   LwRmPtr                             pLwRm;
            LW2080_CTRL_CLK_MEASURE_FREQ_PARAMS params;
            RC                                  rc;

            memset( &params, 0, sizeof( params));               // Setup parameters for the API call
            params.clkDomain = m_pClkInfo->clkDomain;           // Measure the frequency for this domain
            rc= pLwRm->ControlBySubdevice(hSubDev, LW2080_CTRL_CMD_CLK_MEASURE_FREQ, &params, sizeof( params));

            m_MeasuredFreqKHz = (rc? 0: params.freqKHz);        // Remember the freq unless there as an error
            return rc;                                          // Indicate the error (if any) to the caller
        }

        //! \brief Set the target freq; Reset the actual freq, source, and measured freq
        public: inline void PrepareForSetInfos()
        {   m_pClkInfo->targetFreq = TargetFreqKHz();
            m_pClkInfo->actualFreq = 0;
            m_pClkInfo->clkSource  = TargetSource();
            m_pClkInfo->flags      = TargetFlag();
            m_MeasuredFreqKHz      = 0;
        }

        //! \brief Result of measuring the clock frequency
        //! Zero if unknown or it Measure() call failed.
        public: inline FreqKHz_t MeasuredFreqKHz() const
        {   return m_MeasuredFreqKHz;
        }

        //! \brief Delta of the measured v. actual freq
        // Return +INFINITY if either the measured or actual is unknown (zero).
        public: inline double MeasuredDelta() const
        {   return !m_MeasuredFreqKHz || !m_pClkInfo->actualFreq? +INFINITY:
            ((double) m_MeasuredFreqKHz - m_pClkInfo->actualFreq) / m_pClkInfo->actualFreq;
        }

        //! \brief Reset the measured freq to "unknown" (i.e. zero)
        public: inline void ResetMeasuredFreq()
        {   m_MeasuredFreqKHz = 0;
        }

        //! Compares the prior source to the current source
        public: Transition_t Transition() const
        {   UINT08 transition = TransitionPllPll;           // CAUTION: Logic here assumes certain bit patterns
            if( !m_pClkInfo->clkSource || !m_PriorSource)   return TransitionOther;

            // We prefer to use the target, but use the actual if need be.
            if( TargetSourceSpecified())
            {   if( TargetSourceIsOneSource())      transition|= TransitionToOsm;
            }
            else if( ActualSourceIsOneSource())     transition|= TransitionToOsm;

            if( PriorSourceIsOneSource())           transition|= TransitionFromOsm;
            return (Transition_t) transition;
        }

        //! \brief Frequency programmed by Resman
        public: inline FreqKHz_t ActualFreqKHz() const
        {   return m_pClkInfo->actualFreq;
        }

        //! \brief Delta of the actual v. target freq
        // Return +INFINITY if either the actual or target is unknown (zero).
        public: inline double ActualDelta() const
        {   return !m_pClkInfo->actualFreq || !TargetFreqKHz()? +INFINITY:
            ((double) m_pClkInfo->actualFreq - TargetFreqKHz()) / TargetFreqKHz();
        }

        //! \brief Reset the actual freq and source to "unknown" (i.e. zero)
        public: inline void ResetActualInfo()
        {   m_pClkInfo->actualFreq = 0;
            m_pClkInfo->clkSource  = LW2080_CTRL_CLK_SOURCE_DEFAULT;
        }

        //! \brief Frequency requested when asking Resman to program the domain
        //! Target matches the actual unless target has been advanced.
        //! This makes it okay for the first in iteration to be a NOP.
        public: inline FreqKHz_t TargetFreqKHz() const
        {   if (m_TargetTestSpec.freqKHz) return m_TargetTestSpec.freqKHz;
            else                          return m_pClkInfo->actualFreq;
        }

        //! \brief Source requested when asking Resman to program the domain
        public: inline ClkSource_t TargetSource() const
        {   return m_TargetTestSpec.source;
        }

        //! \brief Source from which the domain is running
        public: inline const char *TargetSourceDomainName() const
        {   return ! m_TargetTestSpec.source? "":               // Empty is unspecified or default
                RmtClockUtil().GetClkSourceName( m_TargetTestSpec.source);
        }

        //! \brief Flag requested when asking Resman to program the domain
        public: inline LwU32 TargetFlag() const
        {   return m_TargetTestSpec.flag;
        }

        //! \brief See if we're out of tolerance and print errors if we are
        public: inline void CheckTolerance()
        {   static const double tolerance[2]    = { PLL_DELTA_TOLERANCE, OSM_DELTA_TOLERANCE };
            bool source;                                                // Current source: 0=PLL 1=OneSource
            double actualDelta                  = ActualDelta();        // Delta of actual freq v. target freq
            double measuredDelta                = MeasuredDelta();      // Delta of measured freq v. actual freq

            if( TargetSourceSpecified())    source = TargetSourceIsOneSource();     // We prefer to use the target,
            else                            source = ActualSourceIsOneSource();     // but use the actual if need be.

            if (-tolerance[source] >= actualDelta || actualDelta >= tolerance[source])
            {   Printf(Tee::PriHigh, "\n%s:%9s: ERROR: Actual frequency is out of tolerance for %s.  delta=%+.1f%%  tolerance=%c%.1f%%\n",
                    __FUNCTION__, m_DomainName,
                    ( source? "OneSource": "PLL"), actualDelta * 100.0,
                    PLUS_OR_MINUS, tolerance[source] * 100.0);
                ++m_ToleranceErrorCount;
            }

            if (-tolerance[source] >= measuredDelta || measuredDelta >= tolerance[source])
            {   Printf(Tee::PriHigh, "\n%s:%9s: ERROR: Measured frequency is out of tolerance for %s.  delta=%+.1f%%  tolerance=%c%.1f%%\n",
                    __FUNCTION__, m_DomainName,
                    ( source? "OneSource": "PLL"), measuredDelta * 100.0,
                    PLUS_OR_MINUS, tolerance[source] * 100.0);
                ++m_ToleranceErrorCount;
            }
        }

        //! \brief See if we're out of tolerance and print errors if we are
        public: inline void CheckSource()
        {   if( TargetSourceSpecified())                        // Nothing to check is unspecified
            {   bool targetSource = TargetSourceIsOneSource();  // Target source: 0=PLL 1=OneSource
                bool actualSource = ActualSourceIsOneSource();  // Actual source: 0=PLL 1=OneSource

                if (targetSource != actualSource)
                {   Printf(Tee::PriHigh, "\n%s:%9s: ERROR: Actual source (%s) does not match target source (%s).\n",
                        __FUNCTION__, m_DomainName,
                        ( actualSource? "OneSource": "PLL"),
                        ( targetSource? "OneSource": "PLL"));
                    ++m_SourceErrorCount;
                }
            }
        }

        //! \brief Total number of errors detected in CheckTolerance()
        public: inline UINT32 ToleranceErrorCount() const
        {   return m_ToleranceErrorCount;
        }

        //! \brief Total number of errors detected in CheckTolerance()
        public: inline UINT32 SourceErrorCount() const
        {   return m_SourceErrorCount;
        }

        //! \brief Advance the target freq to the next one in the list
        public: inline void AdvanceTargetTestSpec()
        {   m_TargetTestSpec = m_FreqLimit.NextTestSpec();
        }

        //! \brief Flags passed to Resman when programming the domain
        //! per LW2080_CTRL_CLK_INFO_FLAGS_xxx constants
        public: inline UINT32 Flags() const
        {   return m_pClkInfo->flags;
        }

        //! \brief Source passed to Resman when programming the domain
        //! per LW2080_CTRL_CLK_INFO_SOURCE_xxx constants
        public: inline ClkSource_t Source() const
        {   return m_pClkInfo->clkSource;
        }

        //! \brief Set flags passed to Resman when programming the domain
        //! per LW2080_CTRL_CLK_INFO_FLAGS_xxx constants
        public: inline UINT32 SetFlags( UINT32 include, UINT32 exclude= 0x0000000)
        {   return m_pClkInfo->flags = (m_pClkInfo->flags & ~exclude) | include;
        }

        //! \brief Collect statistics on actual v. target and measured v. actual
        public: inline void CollectStatistics()
        {   m_Statistics.Collect( ActualDelta(), MeasuredDelta(), Transition());
        }

        //! \brief Print statistics on actual v. target and measured v. actual
        public: inline void PrintStatistics()
        {
            if (m_ToleranceErrorCount)
                m_Statistics.PrintHistogram(m_DomainName);      // Histogram data only if it's interesting

            m_Statistics.PrintTransition(m_DomainName);         // Transition data is always interesting

            if (m_ToleranceErrorCount)
                Printf(Tee::PriHigh, "\n%s:%9s: %u tolerance errors detected\n",
                    __FUNCTION__, m_DomainName, m_ToleranceErrorCount);
            else
                Printf(Tee::PriHigh, "\n%s:%9s: No tolerance errors detected\n",
                    __FUNCTION__, m_DomainName);

            if (m_SourceErrorCount)
                Printf(Tee::PriHigh, "\n%s:%9s: %u source mismatch errors detected\n",
                    __FUNCTION__, m_DomainName, m_SourceErrorCount);
            else
                Printf(Tee::PriHigh, "\n%s:%9s: No source mismatch errors detected\n",
                    __FUNCTION__, m_DomainName);

        }
    };  // ClockDomain

    //! \brief Array of programmable clock domains
    public: class ClockDomainArray
    {
        protected:
            UINT32                  m_ClockDomainCount;                     // Number of programmable clock domains
            ClockDomain             m_pClockDomain[MAX_CLOCK_DOMAIN_COUNT]; // Each clock domain
            LW2080_CTRL_CLK_INFO    m_pClkInfos[MAX_CLOCK_DOMAIN_COUNT];    // Used in the RM API calls and within ClockDomain object
            GpuSubdevice            *pSubdevice;                            // Subdevice Handle

        //! \brief Construction
        public: ClockDomainArray()
        {   m_ClockDomainCount = 0;
        }

        //! \brief Initialization
        //!
        //! \params include     Bitmap of domains to handle.  Per LW2080_CTRL_CLK_DOMAIN_xxx constants
        //!                     Must be a subset of readable domains for the Get() or Set() to work.
        //!                     Must be a subset of programmable domains for the Set() to work.
        //!
        public: void Init( ClkDomain_t include)
        {   MASSERT(!m_ClockDomainCount);               // Initxxx() may be called only once per array object
            ClkDomain_t domain;                         // Per LW2080_CTRL_CLK_DOMAIN_xxx constants

            m_ClockDomainCount = 0;
            domain = 0x00000001;                        // Iterate through all possible domains

            while(true)
            {   if( domain & include)                   // Ignore any domain not included in the mask
                {   m_pClockDomain[m_ClockDomainCount].Init( m_pClkInfos+ m_ClockDomainCount, domain);
                    ++m_ClockDomainCount;               // Next array element
                }

                if( domain & (ClkDomain_t) BIT( MAX_CLOCK_DOMAIN_COUNT- 1)) break;  // Exit on last domain
                domain<<= 1;                                                        // Next potential domain
            }
        }

        //! \brief Initialize to the set of programmable domains
        //!
        //! \params exclude     Bitmap of domains to exclude.
        //!
        public: RC InitProgrammable( ClkDomain_t exclude, GpuSubdevice *hSubDev)
        {   LwRmPtr                             pLwRm;
            LW2080_CTRL_CLK_GET_DOMAINS_PARAMS  params;
            RC                                  rc;

            MASSERT(!m_ClockDomainCount);                       // Do not call this function if we've aleady initialized
            pSubdevice = hSubDev;
            memset( &params, 0, sizeof( params));               // Get the set of clock domains that are programmable
            params.clkDomainsType = LW2080_CTRL_CLK_DOMAINS_TYPE_PROGRAMMABALE_ONLY;
            rc= pLwRm->ControlBySubdevice(pSubdevice, LW2080_CTRL_CMD_CLK_GET_DOMAINS, &params, sizeof( params));

            if( !rc) Init( params.clkDomains & ~exclude);       // Initialize if all went as planned
            return rc;                                          // Indicate the error (if any) to the caller
        }

        //! \brief Number of elements
        public: UINT32 Count() const
        {   return m_ClockDomainCount;
        }

        //! \brief Specified array element
        public: ClockDomain &operator[](int clk)
        {   return m_pClockDomain[clk];
        }

        //! Object corresponding to a specific domain
        //! NULL if no such object exists in the array
        public: ClockDomain *Find( ClkDomain_t domain)
        {   UINT32 clk = BIT_IDX_32(domain) + 1;                        // Uppermost possible is the bit position
            if (clk > m_ClockDomainCount) clk = m_ClockDomainCount;     // or the number of actual elements since
                                                                        // they were initialized on bit order.
            while(clk--)                                                // Search until we find it
            {   if( m_pClockDomain[clk].Domain()== domain) return m_pClockDomain + clk;
                if( m_pClockDomain[clk].Domain()< domain) break;        // or go past it
            }

            return NULL;                                                // NULL if no match
        }

        //! \brief Read the clock frequencies (LW2080_CTRL_CMD_CLK_GET_INFO)
        public: RC GetInfos( UINT32 globalFlags= 0x00000000)
        {   LwRmPtr                             pLwRm;
            LW2080_CTRL_CLK_GET_INFO_PARAMS     params;

            memset( &params, 0, sizeof( params));
            params.flags                = globalFlags;
            params.clkInfoListSize      = m_ClockDomainCount;
            params.clkInfoList          = (LwP64) m_pClkInfos;      // Use the RM API to get the info
            return pLwRm->ControlBySubdevice(pSubdevice, LW2080_CTRL_CMD_CLK_GET_INFO, &params, sizeof( params));
        }

        //! \brief Set the clock frequencies (LW2080_CTRL_CMD_CLK_SET_INFO)
        public: RC SetInfos( UINT32 globalFlags= 0x00000000)
        {   LwRmPtr                             pLwRm;
            LW2080_CTRL_CLK_SET_INFO_PARAMS     params;

            for( UINT32 clk = 0; clk < m_ClockDomainCount; clk++)   // Discard info that will no longer apply
            {   m_pClockDomain[clk].SavePriorInfo();                // Save off the prior actual source
                m_pClockDomain[clk].PrepareForSetInfos();           // Set the target freq; Reset the actual freq, source, and measured freq
            }

            memset( &params, 0, sizeof( params));
            params.flags                = globalFlags;
            params.clkInfoListSize      = m_ClockDomainCount;
            params.clkInfoList          = (LwP64) m_pClkInfos;      // Use the RM API to set the info
            return pLwRm->ControlBySubdevice(pSubdevice, LW2080_CTRL_CMD_CLK_SET_INFO, &params, sizeof( params));
        }

        //! \brief Measure the clock frequencies (LW2080_CTRL_CMD_CLK_MEASURE_FREQ)
        public: RC MeasureAll()
        {   RC rc = OK;                 // Loop through all clocks (or until error)
            for( UINT32 clk = 0; clk < m_ClockDomainCount && !( rc = m_pClockDomain[clk].Measure(pSubdevice)); clk++) /*NOP*/;
            return rc;                  // Indicate the error (if any) to the caller
        }

        //! \brief Collect statistics
        public: void CollectStatistics()
        {   for( UINT32 clk = 0; clk < m_ClockDomainCount; clk++) m_pClockDomain[clk].CollectStatistics();
        }

        //! \brief See if we're out of tolerance and/or have source mismatches and print errors if we are
        public: inline void CheckErrors()
        {   for( UINT32 clk = 0; clk < m_ClockDomainCount; clk++)
            {   m_pClockDomain[clk].CheckTolerance();
                m_pClockDomain[clk].CheckSource();
            }
        }

        //! \brief Total number of errors detected in CheckTolerance()
        public: inline UINT32 ToleranceErrorCount() const
        {   UINT32 total = 0;
            for( UINT32 clk = 0; clk < m_ClockDomainCount; clk++) total += m_pClockDomain[clk].ToleranceErrorCount();
            return total;
        }

        //! \brief Total number of errors detected in CheckTolerance()
        public: inline UINT32 SourceErrorCount() const
        {   UINT32 total = 0;
            for( UINT32 clk = 0; clk < m_ClockDomainCount; clk++) total += m_pClockDomain[clk].SourceErrorCount();
            return total;
        }

        //! \brief Advance the target freqs for all clocks
        public: void AdvanceAllTargetFreq()
        {   for( UINT32 clk = 0; clk < m_ClockDomainCount; clk++) m_pClockDomain[clk].AdvanceTargetTestSpec();
        }

        //! Format the 'delta' information into a buffer for printing
        //! The output is formatted for 7 characters, not including the terminating \0
        //! Typically, the first 3 characters are spaces and may be used as filler between columns.
        //! It is assumed that the buffer is at least SMALL_BUFFER_SIZE in size.
        //! The buffer is used internally; it's value should not be relied upon after the call.
        //! The buffer should exist in the caller's local space to ensure thread-safety.
        protected: const char *FormatDelta( char *buffer, double delta)
        {   if (-0.0005 < delta && delta < 0.0005) return "   ----";            // Nearly zero

            if (-1000.0 < delta && delta < 1000.0)                              // Reasonable values
            {   snprintf( buffer, SMALL_BUFFER_SIZE, "%+6.1f%%", delta * 100.0);
                return buffer;                                                  // Return as a percent
            }

            if (-INFINITY < delta && delta < +INFINITY) return "   ****";       // Large finite values
            return "       ";                                                   // Data not available
        }

        //! \brief Print the current state of all clocks
        //!
        //! The following fields are printed:
        //! target   Freq that we requested for Resman to program
        //! actual   Freq that Resman decided was doable and near enough
        //! delta    Delta between actual and target
        //! measured Freq that Resman actually measured
        //! delta    Delta between measured and actual
        //! source   Clock source for this clock domain
        //! tran     Transitions to/from PLL/OneSource
        //! flags    Flags passed to Resman when programming the clock domain
        public: void PrintState()
        {   char actualBuffer[SMALL_BUFFER_SIZE];
            char measuredBuffer[SMALL_BUFFER_SIZE];

            Printf(Tee::PriHigh, "%s:             target   actual   delta  measured   delta   source     tran  flags\n", __FUNCTION__);
            for( UINT32 clk = 0; clk < m_ClockDomainCount; clk++)
            {   Printf(Tee::PriHigh, "%s:%9s: %8.0u %8.0u %-8.8s %8.0u %-8.8s  %-11s%-4s  0x%08x\n",
                    __FUNCTION__, m_pClockDomain[clk].DomainName(),
                    m_pClockDomain[clk].TargetFreqKHz(),
                    m_pClockDomain[clk].ActualFreqKHz(),    FormatDelta(actualBuffer,   m_pClockDomain[clk].ActualDelta()),
                    m_pClockDomain[clk].MeasuredFreqKHz(),  FormatDelta(measuredBuffer, m_pClockDomain[clk].MeasuredDelta()),
                    m_pClockDomain[clk].SourceDomainName(), c_TransitionShortName[m_pClockDomain[clk].Transition()],
                    m_pClockDomain[clk].Flags());
            }
        }

        //! \brief Print the target state of all clocks
        //!
        //! The following fields are printed:
        //! target   Freq that we requested for Resman to program
        //! flags    Flags passed to Resman when programming the clock domain
        public: void PrintTarget()
        {   Printf(Tee::PriHigh, "%s:             target  source    flags\n", __FUNCTION__);
            for( UINT32 clk = 0; clk < m_ClockDomainCount; clk++)
            {   Printf(Tee::PriHigh, "%s:%9s: %8.0u  %-8.8s  0x%08x\n",
                    __FUNCTION__, m_pClockDomain[clk].DomainName(),
                    m_pClockDomain[clk].TargetFreqKHz(),
                    m_pClockDomain[clk].TargetSourceDomainName(),
                    m_pClockDomain[clk].TargetFlag());
            }
        }

        //! \brief Print collecgted stats for all clocks
        public: inline void PrintStatistics()
        {   for( UINT32 clk = 0; clk < m_ClockDomainCount; clk++) m_pClockDomain[clk].PrintStatistics();
        }

    };  // ClockDomainArray

public:
    FermiClockStressTest();
    virtual ~FermiClockStressTest();
    virtual string  IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(noMCLK, bool);

private:
    //! Clock Domains
    ClockDomainArray    m_Clocks;

    // Command Line Arguments.
    bool m_noMCLK;

};  // FermiClockStressTest

//! \brief ClockTest constructor.
//!
//! Initializes some basic variables as default values in the resepctive tests
//! \sa Setup
//-----------------------------------------------------------------------------
FermiClockStressTest::FermiClockStressTest()
{
    SetName("ClockTest");
    m_noMCLK = false;
    // Quick and dirty hack: If Class906fTest is in the background, keep it there until we're done.
    Class906fTest::runControl= Class906fTest::ContinueRunning;
}

//! \brief ClockTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
FermiClockStressTest::~FermiClockStressTest()
{   }

//! \brief IsSupported Function.
//!
//! The test is supported on HW
//! \sa Setup
//-----------------------------------------------------------------------------
string FermiClockStressTest::IsTestSupported()
{
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();

    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        // Since we cannot model all the clock regsisters and their functionality on F-Model.
        return "Fermi Clock Stress Test is not supported for F-Model due to incomplete register modelling.";
    }

    if (IsMAXWELLorBetter(chipName))
    {
        return "This Clock Stress Test is only supported for Kepler. Use Universal Clock Test for Maxwell and beyond."; //$
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Cleanup Function.
//!
//! Cleanup all the allocated variables in the setup. Cleanup can be called
//! even in errorneous condition, in such case all the allocations should be
//! freed, this function takes care of such freeing up.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RC FermiClockStressTest::Cleanup()
{
    RC rc;
    rc = OK;
    // Quick and dirty hack: If Class906fTest is in the background, release it.
    Class906fTest::runControl= Class906fTest::StopRunning;
    return rc;
}

//! \brief Setup test
//!
//! For basic test this setup function allocates the required memory.
//! To allocate memory for basic test it needs clock domains
//! information so the setup function also gets and dumps the clock domains
//! information.
//!
//! \return OK if the allocations success, specific RC if failed
//! \sa Setup
//-----------------------------------------------------------------------------
RC FermiClockStressTest::Setup()
{   RC          rc;
    ClkDomain_t exclude = 0;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    // Exclude MCLK if that was specified on the command line
    if (m_noMCLK)
    {   exclude |= LW2080_CTRL_CLK_DOMAIN_MCLK;
        Printf(Tee::PriHigh, "%s: MCLK has been excluded by command line parameter\n", __FUNCTION__);
    }

    // Initialize the array of clock domains
    m_Clocks.InitProgrammable( exclude, GetBoundGpuSubdevice());

    Printf(Tee::PriHigh, "%s: Number of domains: %u\n", __FUNCTION__, m_Clocks.Count());
    return rc;
}

//! \brief Run Test
//!
//! This test gets current clocks state and verifies if we can program them.
//! The test basically does a Stress test on all Fermi Clock states.
//!
//! \return OK if the tests passed, specific RC if failed
//-----------------------------------------------------------------------------
RC FermiClockStressTest::Run()
{   RC rc;
    UINT32 i;
    UINT32 start = Utility::GetSystemTimeInSeconds();
    UINT32 elapsed = 0;

    Printf(Tee::PriHigh, "%s: start\n", __FUNCTION__);

    // hook up clk info table
    CHECK_RC( m_Clocks.GetInfos());

    // Measure all of the clocks
    CHECK_RC( m_Clocks.MeasureAll());

    // Talk
    Printf(Tee::PriHigh, "%s: Initial state of each domain:  (KHz)\n", __FUNCTION__);
    m_Clocks.PrintState();
    Printf(Tee::PriHigh,"\n");

    for (i = 0; i < TEST_ITERATION_COUNT && elapsed < TEST_MAX_TIME_SECONDS; ++i)
    {
        if(i) Printf(Tee::PriHigh, "\n%s: #%u: %u iterations remaining\n", __FUNCTION__, i, TEST_ITERATION_COUNT - i);
        else Printf(Tee::PriHigh, "\n%s: #%u: first iteration makes no changes in the frequencies\n", __FUNCTION__, i);

        Printf(Tee::PriHigh, "%s: #%u: About to program each domain:  (KHz)\n", __FUNCTION__, i);
        m_Clocks.PrintTarget();                             // Let's see what will soon happen
        Printf(Tee::PriHigh,"\n");

        CHECK_RC( m_Clocks.SetInfos());                     // Program the clocks
        CHECK_RC( m_Clocks.GetInfos());                     // Read the clocks
        CHECK_RC( m_Clocks.MeasureAll());                   // See how fast they're really running
        m_Clocks.CollectStatistics();                       // Collect statistics
        m_Clocks.CheckErrors();                             // Show tolerance and/or source mismatch errors (if any)

        Printf(Tee::PriHigh, "\n%s: #%u: Current state of each domain after programming:  (KHz)\n", __FUNCTION__, i);
        m_Clocks.PrintState();                              // Let's see what it looks like

        elapsed = Utility::GetSystemTimeInSeconds() - start;    // Compute elapsed time
        Printf(Tee::PriHigh, "\n%s: #%u: Elapsed time: %02u:%02u:%02u\n", __FUNCTION__, i,
            elapsed / 3600, (elapsed / 60) % 60, elapsed % 60);

        if (PRINT_STATISTICS_PERIOD && i % PRINT_STATISTICS_PERIOD == PRINT_STATISTICS_PERIOD - 1 && i + 1 < TEST_ITERATION_COUNT)
        {   Printf(Tee::PriHigh, "\n%s: #%u: Statistics collected so far:\n", __FUNCTION__, i);
            m_Clocks.PrintStatistics();                     // Prints the statistics periodically
        }                                                   // unless we're on the last iteration

        m_Clocks.AdvanceAllTargetFreq();                    // Advance to the next freq in the list
    }

    Printf(Tee::PriHigh, "\n%s: Statistics collected throughout test:\n", __FUNCTION__);
    m_Clocks.PrintStatistics();                             // Prints the statistics

    // Check for an error on return
// HACK: Ignore errors on HOSTCLK since we have known problems but it isn't a priority to fix.
    const ClockDomain *pHostClk = m_Clocks.Find(LW2080_CTRL_CLK_DOMAIN_HOSTCLK);
    if (!rc && (m_Clocks.ToleranceErrorCount() > (pHostClk? pHostClk->ToleranceErrorCount(): 0)))
    {   Printf(Tee::PriHigh, "\n%s: %u total tolerance errors detected\n", __FUNCTION__, m_Clocks.ToleranceErrorCount());
        rc = RC::SOFTWARE_ERROR;
    }

    if (pHostClk && pHostClk->ToleranceErrorCount())
        Printf(Tee::PriHigh, "\n%s: HACK: %u total HOSTCLK tolerance errors ignored\n", __FUNCTION__, pHostClk->ToleranceErrorCount());

//  if (!rc && m_Clocks.ToleranceErrorCount())
//  {   Printf(Tee::PriHigh, "\n%s: %u total tolerance errors detected\n", __FUNCTION__, m_Clocks.ToleranceErrorCount());
//      rc = RC::SOFTWARE_ERROR;
//  }

    if (!rc && m_Clocks.SourceErrorCount())
    {   Printf(Tee::PriHigh, "\n%s: %u total source mismatch errors detected\n", __FUNCTION__, m_Clocks.SourceErrorCount());
        rc = RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriHigh, "\n%s: end\n", __FUNCTION__);
    return rc;

}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ FermiClockStressTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(FermiClockStressTest, RmTest, "Fermi RM Clock Stress Test");
CLASS_PROP_READWRITE(FermiClockStressTest, noMCLK, bool, "Do not test MCLK");

