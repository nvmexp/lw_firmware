/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//
//
// The purpose of this test is to detect cases where the BIOS has disabled
// memory that is physically present.  It gets a list of board descriptions
// via the Parse() method.  Subsequent calls to Run() then determine if
// the DUT is one of those boards, and if so if it maches the expected memory
// size in that array.
//------------------------------------------------------------------------------

// This is a description of what memsize does...this is useful for cut/pasting
// into Outlook for people that ask what memsize is for & what it does.
//
/*
  The purpose of the memsize is to detect and fail cards that have been mis-sized
  by the vbios.

  When the system is first powered on, the vbios does a very simple "sizing
  algorithm" that tests the most-significant bits on the framebuffer bus.  If it
  can read what it wrote, then the vbios activates those bits.  If it cannot read
  what it wrote, it turns off those bits.

  The idea behind this is that with this "sizing algorithm," there can be a
  single vbios that supports multiple framebuffer widths.  On a 128-bit card, the
  vbios will find that all 128 bits are present and working, and activate all of
  them.  On a 64-bit card, the vbios will find that the upper 64 bits are not
  working, and only enable the lower 64-bits.

  However, this mechanism fails in these cases:

  CASE #1:  The card has all 128 bits worth of memory present, but there is some
  sort of manufacturing defect (like a solder short or open) on the most-
  significant 64 bits of the bus.  In this case, the sizing algorithm will
  determine it did not read what it wrote, and disable the most significant bits.

  In this case, the resulting 64-bit card will work fine and pass all the
  manufacturing diagnostics.  However, this is undesirable because the customer
  paid for and specified a 128-bit card.  Thus, there is a test escape.

  CASE #2:  The card has some sort of manufacturing defect that makes the memory
  work intermittently.  This can result in the following sequence of events:
  first, on the manufacturing floor, the vbios comes up, happens to fail the
  "bad" 64-bits of memory, and shut them off.  Then, the manufacturing
  diagnostics pass, because the least-significant 64 bits work fine.  Then, the
  card gets shipped to the customer.  This time, the sizing algorithm (which is a
  really simple test) happens to decide the memory is OK.  Then, it enables the
  upper 64 bits.  Then, the card fails severely because those bits do not work.

  The purpose of memsize is to catch these conditions.  By looking at the PCI
  device ID, memory strap, dram type, etc., memsize can figure out what specific
  board design is present in the system.  It does this by looking up boards with
  those attributes in the boards.js file.  Then, that same file tells memsize if
  that specific board design is a 128-bit or a 64-bit board.

*/

#include "core/include/framebuf.h"
#include "core/include/fusexml_parser.h"
#include "core/include/gpu.h"
#include "core/include/jscript.h"
#include "core/include/mgrmgr.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/script.h"
#include "core/include/utility.h"
#include "core/include/version.h"
#include "ctrl/ctrl2080.h"  // graphics caps
#include "device/interface/pcie.h"
#include "gpu/fuse/fuse.h"
#include "gpu/include/boarddef.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/perfsub.h"
#include "gpu/tests/gputest.h"
#include "cheetah/include/sysutil.h"

#ifdef INCLUDE_AZALIA
    #include "device/azalia/azactrl.h"
#endif

#include <algorithm>
#include <set>
#include <vector>

namespace
{
    //------------------------------------------------------------------
    //! This base class describes an N-bit bitfield, capable of
    //! holding up to 2**N values.  The static const RegBitField
    //! objects in this file are used to read/write/analyze the
    //! RegBits bit-vectors.  Value 0 is always DONT_CARE.
    //------------------------------------------------------------------
    class RegBitField
    {
    public:
        static const UINT32 DONT_CARE = 0;
        const string &GetName() const { return m_Name; }
        const string &GetWebsiteString() const { return m_WebsiteString; }
        const string &GetValueName(UINT32 value) const
            { return m_ValueName[value]; }
        UINT32 ReadValue(const vector<bool> &regBits) const;
        void WriteValue(vector<bool> *pRegBits, UINT32 value) const;
    protected:
        RegBitField(const string &name, const string &websiteString,
                    size_t numBits, ...);
        void InitValue(UINT32 value, const string &name)
            { m_ValueName[value] = name; }
    private:
        string m_Name;           //!< The bitfield's name
        string m_WebsiteString;  //!< String attached to this bitfield on the
                                 //!< boards.db website
        vector<UINT32> m_Bits;   //!< Index of the N bits in the bitfield
        UINT32 m_MaxBit;         //!< The highest-numbered bit in m_Bits
        vector<string> m_ValueName; //!< Names of the 2**N possible values
    };

    //------------------------------------------------------------------
    //! RegBitField subclasses
    //------------------------------------------------------------------
    class YesNoRegBitField : public RegBitField
    {
    public:
        enum Values
        {
            NO  = 0x1,
            YES = 0x2
        };
        YesNoRegBitField(const string &name, const string &websiteString,
                         UINT32 noBit, UINT32 yesBit) :
            RegBitField(name, websiteString, 2, noBit, yesBit)
        {
            InitValue(NO,  "No");
            InitValue(YES, "Yes");
        }
    };

    class InitGenRegBitField : public RegBitField
    {
    public:
        enum Values
        {
            GEN1 = 0x3,
            GEN2 = 0x2,
            GEN3 = 0x1
        };
        InitGenRegBitField(const string &name, const string &websiteString,
                           UINT32 bit0, UINT32 bit1) :
            RegBitField(name, websiteString, 2, bit0, bit1)
        {
            InitValue(GEN1, "GEN1");
            InitValue(GEN2, "GEN2");
            InitValue(GEN3, "GEN3");
        }
    };

    class PwmBitField : public RegBitField
    {
    public:
        enum Values
        {
            DISABLED = 0x1,
            PWM_66   = 0x2,
            PWM_33   = 0x3
        };
        PwmBitField(const string &name, const string &websiteString,
                    UINT32 bit0, UINT32 bit1) :
            RegBitField(name, websiteString, 2, bit0, bit1)
        {
            InitValue(DISABLED, "DISABLED");
            InitValue(PWM_66,   "66");
            InitValue(PWM_33,   "33");
        }
    };

    //------------------------------------------------------------------
    // Define the RegBits bitfields
    //------------------------------------------------------------------
    enum
    {
        BIT_RESMAN_NON_GL,                              //  0
        BIT_RESMAN_GL,                                  //  1
        BIT_RESMAN_ECC_DISABLED,                        //  2
        BIT_RESMAN_ECC_ENABLED,                         //  3
        BIT_RESMAN_PWRCAP_DISABLED,                     //  4
        BIT_RESMAN_PWRCAP_ENABLED,                      //  5
        BIT_RESMAN_GEN3_DISABLED,                       //  6
        BIT_RESMAN_GEN3_ENABLED,                        //  7
        BIT_RESMAN_GEN2_DISABLED,                       //  8
        BIT_RESMAN_GEN2_ENABLED,                        //  9
        BIT_RESMAN_INIT_GEN_BIT0,                       //  10
        BIT_UNUSED_0,                                   //  reuse after gm20x
        BIT_STRAPPED_XCLK_277_DISABLED,                 //  12
        BIT_STRAPPED_XCLK_277_ENABLED,                  //  13
        BIT_STRAPPED_SUB_VENDOR_DISABLED,               //  14
        BIT_STRAPPED_SUB_VENDOR_ENABLED,                //  15
        BIT_STRAPPED_SLOT_CLK_CFG_DISABLED,             //  16
        BIT_STRAPPED_SLOT_CLK_CFG_ENABLED,              //  17
        BIT_STRAPPED_PEX_PLL_EN_TERM100_DISABLED,       //  18
        BIT_STRAPPED_PEX_PLL_EN_TERM100_ENABLED,        //  19
        BIT_RESMAN_INIT_GEN_BIT1,                       //  20
        BIT_UNUSED_1,                                   //  reuse after gm20x
        BIT_STRAPPED_FAN_DEBUG_PWM_BIT0,                //  22
        BIT_STRAPPED_FAN_DEBUG_PWM_BIT1,                //  23
        BIT_AER_DISABLED,                               //  24
        BIT_AER_ENABLED,                                //  25
        BIT_PLX_DISABLED,                               //  26
        BIT_PLX_ENABLED,                                //  27
        BIT_GEMINI_DISABLED,                            //  28
        BIT_GEMINI_ENABLED,                             //  29

        NUM_REG_BITS // must be last
    };

    const YesNoRegBitField s_BitsResmanGl
    (
        "GL Board",
        "RM considers this a GL board",
        BIT_RESMAN_NON_GL,
        BIT_RESMAN_GL
    );
    const YesNoRegBitField s_BitsResmanEcc
    (
        "ECC enabled",
        "RM considers this an ECC-enabled board",
        BIT_RESMAN_ECC_DISABLED,
        BIT_RESMAN_ECC_ENABLED
    );
    const YesNoRegBitField s_BitsResmanPwrcap
    (
        "Power-capping",
        "RM considers power-capping enabled",
        BIT_RESMAN_PWRCAP_DISABLED,
        BIT_RESMAN_PWRCAP_ENABLED
    );
    const YesNoRegBitField s_BitsResmanGen3
    (
        "Gen3 enabled",
        "RM marks card as Gen3 enabled",
        BIT_RESMAN_GEN3_DISABLED,
        BIT_RESMAN_GEN3_ENABLED
    );
    const YesNoRegBitField s_BitsResmanGen2
    (
        "Gen2 enabled",
        "RM marks card as Gen2 enabled",
        BIT_RESMAN_GEN2_DISABLED,
        BIT_RESMAN_GEN2_ENABLED
    );
    const InitGenRegBitField s_BitsResmanInitGen
    (
        "Initial PCI speed",
        "RM initializes to PCIe speed",
        BIT_RESMAN_INIT_GEN_BIT0,
        BIT_RESMAN_INIT_GEN_BIT1
    );
    const YesNoRegBitField s_BitsStrappedXclk277
    (
        "XCLK_277",
        "Board strapped as XCLK_277 enabled",
        BIT_STRAPPED_XCLK_277_DISABLED,
        BIT_STRAPPED_XCLK_277_ENABLED
    );
    const YesNoRegBitField s_BitsStrappedSubVendor
    (
        "SUB_VENDOR",
        "Board strapped as SUB_VENDOR enabled",
        BIT_STRAPPED_SUB_VENDOR_DISABLED,
        BIT_STRAPPED_SUB_VENDOR_ENABLED
    );
    const YesNoRegBitField s_BitsStrappedSlotClkCfg
    (
        "SLOT_CLK_CFG",
        "Board strapped as SLOT_CLK_CFG enabled",
        BIT_STRAPPED_SLOT_CLK_CFG_DISABLED,
        BIT_STRAPPED_SLOT_CLK_CFG_ENABLED
    );
    const YesNoRegBitField s_BitsStrappedPexPllEnTerm100
    (
        "PEX_PLL_EN_TERM100",
        "Board strapped as PEX_PLL_EN_TERM100 enabled",
        BIT_STRAPPED_PEX_PLL_EN_TERM100_DISABLED,
        BIT_STRAPPED_PEX_PLL_EN_TERM100_ENABLED
    );
    const PwmBitField s_BitsStrappedFanDebugPwm
    (
        "FAN_DEBUG_PWM",
        "Board strapped as FAN_DEBUG_PWM",
        BIT_STRAPPED_FAN_DEBUG_PWM_BIT0,
        BIT_STRAPPED_FAN_DEBUG_PWM_BIT1
    );
    const YesNoRegBitField s_BitsAer
    (
        "AER enabled",
        "Board enables AER",
        BIT_AER_DISABLED,
        BIT_AER_ENABLED
    );
    const YesNoRegBitField s_BitsPlx
    (
        "PLX enabled",
        "Board has properly configured PLX switch",
        BIT_PLX_DISABLED,
        BIT_PLX_ENABLED
    );
    const YesNoRegBitField s_BitsGemini
    (
        "Gemini Board",
        "Board is a Gemini Board",
        BIT_GEMINI_DISABLED,
        BIT_GEMINI_ENABLED
    );

    // Array of all RegBitField objects
    const RegBitField  *s_RegBitFields[] =
    {
        &s_BitsResmanGl,
        &s_BitsResmanEcc,
        &s_BitsResmanPwrcap,
        &s_BitsResmanGen3,
        &s_BitsResmanGen2,
        &s_BitsResmanInitGen,
        &s_BitsStrappedXclk277,
        &s_BitsStrappedSubVendor,
        &s_BitsStrappedSlotClkCfg,
        &s_BitsStrappedPexPllEnTerm100,
        &s_BitsStrappedFanDebugPwm,
        &s_BitsAer,
        &s_BitsPlx,
        &s_BitsGemini,
    };
};

//! Read the value of a bitfield from a RegBits bit-vector.  If the
//! vector is too small to hold the bitfield, assume the remaining
//! bits are 0.
//!
UINT32 RegBitField::ReadValue(const vector<bool> &regBits) const
{
    UINT32 returlwalue = 0;
    for (UINT32 ii = 0; ii < m_Bits.size(); ++ii)
    {
        if ((m_Bits[ii] < regBits.size()) && regBits[m_Bits[ii]])
            returlwalue |= (1 << ii);
    }
    return returlwalue;
}

//! Write a value into a bitfield in RegBits bit-vector.  Auto-resize
//! the vector if it's too small.
//!
void RegBitField::WriteValue(vector<bool> *pRegBits, UINT32 value) const
{
    MASSERT(pRegBits);
    if (pRegBits->size() <= m_MaxBit)
        pRegBits->resize(m_MaxBit + 1, false);
    for (UINT32 ii = 0; ii < m_Bits.size(); ++ii)
        (*pRegBits)[m_Bits[ii]] = ((value & (1 << ii)) != 0);
}

//! RegBitField constructor; only called by subclasses.  The args
//! after numBits are the indices of the bits in the bitfield.
//!
RegBitField::RegBitField
(
    const string &name,
    const string &websiteString,
    size_t numBits, ...
)
{
    MASSERT(numBits > 0);
    m_Name = name;
    m_WebsiteString = websiteString;
    m_Bits.resize(numBits);
    m_ValueName.resize(static_cast<size_t>(1) << numBits);

    va_list ap;
    va_start(ap, numBits);
    for (size_t ii = 0; ii < numBits; ++ii)
        m_Bits[ii] = va_arg(ap, UINT32);
    va_end(ap);
    m_MaxBit = *max_element(m_Bits.begin(), m_Bits.end());

    for (UINT32 ii = 0; ii < m_ValueName.size(); ++ii)
    {
        m_ValueName[ii] = Utility::StrPrintf("Invalid[%d]", ii);
    }
    m_ValueName[0] = "Don't Care";
}

//--------------------------------------------------------------------
//! Helper class that contains the data read from the board
//--------------------------------------------------------------------
class BoardDef
{
public:
    BoardDef();
    BoardDef(const BoardDB::BoardEntry &boardEntry);
    void SetRegBits(const RegBitField &field, UINT32 value)
        { field.WriteValue(&m_RegBits, value); }
    void SetRegFlag(const YesNoRegBitField &field, bool value)
        { SetRegBits(field, value ? field.YES : field.NO); }
    void SetRegFlag(const YesNoRegBitField &fd,
                    Device::Feature ft, GpuSubdevice *pSb)
        { if (pSb->HasFeature(ft)) SetRegFlag(fd, pSb->IsFeatureEnabled(ft)); }
    UINT32 GetRegBits(const RegBitField &field) const
        { return field.ReadValue(m_RegBits); }
    void SetFbBus(UINT32 width) {m_FbBus = width;};
    UINT32 GetFbBus() const {return m_FbBus;};
    void SetPcieLinkWidth(UINT32 width) {m_PcieLinkWidth = width;};
    UINT32 GetPcieLinkWidth() const {return m_PcieLinkWidth;};
    void SetExtBanks(UINT32 numBanks) {m_ExtBanks = numBanks;};
    UINT32 GetExtBanks() const {return m_ExtBanks;};
    vector<bool> GetRegBits() const {return m_RegBits;};
    void AddChipSku(string sku){m_MatchedChipSku.push_back(sku);};
    string GetChipSku(UINT32 idx) const {return m_MatchedChipSku[idx];};
    UINT32 GetNumMatchedChipSkus() const
        {return static_cast<UINT32>(m_MatchedChipSku.size());};

    void SetSignature(const BoardDB::Signature &sig) {m_Sig = sig;};
    const BoardDB::Signature* GetSignature() const {return &m_Sig;};

    void PrintInfo(bool cooked, Tee::Priority pri) const;

private:
    vector<bool>   m_RegBits;
    vector<string> m_MatchedChipSku;
    UINT32         m_FbBus;
    UINT32         m_PcieLinkWidth;
    UINT32         m_ExtBanks;
    BoardDB::Signature m_Sig;
};

BoardDef::BoardDef()
: m_FbBus(0),
  m_PcieLinkWidth(0),
  m_ExtBanks(0)
{
    m_RegBits.resize(NUM_REG_BITS, false);
}

BoardDef::BoardDef(const BoardDB::BoardEntry &boardEntry) :
    m_RegBits(boardEntry.m_OtherInfo.m_RegBits),
    m_MatchedChipSku(boardEntry.m_OtherInfo.m_OfficialChipSKUs),
    m_FbBus(boardEntry.m_OtherInfo.m_FBBus),
    m_PcieLinkWidth(boardEntry.m_OtherInfo.m_PciExpressLanes),
    m_ExtBanks(boardEntry.m_OtherInfo.m_Extbanks),
    m_Sig(boardEntry.m_Signature)
{
    m_RegBits.resize(NUM_REG_BITS, false);
}

struct BoardDefEntries
{
    string entry;
    string comment;
};

void BoardDef::PrintInfo(bool cooked, Tee::Priority pri) const
{
    Printf(pri,
           "%s Definition:\n"
           "g_BoardsDB[\"BOARD_NAME\"] = \n"
           "{\n"
           "  BoardDefs: \n"
           "  [\n"
           "  {\n",
           cooked ? "Cooked" : "Raw");

    if (m_Sig.m_GpuId || (m_MatchedChipSku.size() > 0))
    {
        string pciDevIdEntry =
            m_Sig.m_Has5BitPciDevId ?
            Utility::StrPrintf("PCI_DID: 0x%x", 1 << (m_Sig.m_PciDevId&0x1f)) :
            Utility::StrPrintf("PciDevId: 0x%x", m_Sig.m_PciDevId);
        string pciDevIdComment = Utility::StrPrintf(
            m_Sig.m_Has5BitPciDevId ?
            "PCI Device ID = 0x?%02x (last 5 bits known)" :
            "PCI Device ID = 0x%x",
            m_Sig.m_PciDevId);

        UINT32 fbBarMB = m_Sig.m_FBBar * 32;
        string fbBarString;
        if (fbBarMB < 1024)
            fbBarString = Utility::StrPrintf("%d MB", fbBarMB);
        else
            fbBarString = Utility::StrPrintf("%d GB", fbBarMB / 1024);

        string chipSku;
        for (UINT32 ii = 0; ii < m_MatchedChipSku.size(); ++ii)
        {
            if (ii > 0)
                chipSku += " OR ";
            chipSku += '"' + m_MatchedChipSku[ii] + '"';
        }

        UINT32 effectiveSVID = (cooked ? 0 : m_Sig.m_SVID);
        UINT32 effectiveSSID = (cooked ? 0 : m_Sig.m_SSID);

        const BoardDefEntries boardDefEntries[] =
        {
            { Utility::StrPrintf("FBBus: %d", m_FbBus), "FB Bus Width" },
            { Utility::StrPrintf("EB: %d", m_ExtBanks), "External Banks" },
            { Utility::StrPrintf("PcieLanes: %d", m_PcieLinkWidth),
              "PCIe Lanes" },
            { Utility::StrPrintf("IB: %d", m_Sig.m_Intbanks),
              "Internal Banks" },
            { Utility::StrPrintf("Rows: 0x%x", m_Sig.m_Rows), "Memory Rows" },
            { Utility::StrPrintf("Columns: 0x%x", m_Sig.m_Columns),
              "Memory Columns" },
            { pciDevIdEntry, pciDevIdComment },
            { Utility::StrPrintf("GPU: 0x%x", m_Sig.m_GpuId),
              Utility::StrPrintf("GPU = %s", Gpu::DeviceIdToInternalString(
                              static_cast<Gpu::LwDeviceId>(m_Sig.m_GpuId)).c_str()) },
            { Utility::StrPrintf("Straps: 0x%x",
                                 cooked ? 0xFFFF : m_Sig.m_Straps),
              "Memory Straps" },
            { Utility::StrPrintf("FBBAR: 0x%02x", m_Sig.m_FBBar),
              "FB BAR Size = " + fbBarString },
            { Utility::StrPrintf("SVID: 0x%04x", effectiveSVID),
              Utility::StrPrintf("SVID = 0x%04x%s", effectiveSVID,
                                 (effectiveSVID == 0) ? " (don't care)" : "")},
            { Utility::StrPrintf("SSID: 0x%04x", effectiveSSID),
              Utility::StrPrintf("SSID = 0x%04x%s", effectiveSSID,
                                 (effectiveSSID == 0) ? " (don't care)" : "")},
            { Utility::StrPrintf("BoardID: 0x%04x", m_Sig.m_BoardId), "" },
            { "BoardProjectAndSku: \"" + m_Sig.m_BoardProjectAndSku + "\"",
              "" },
            { "VBIOSChipName: \"" + m_Sig.m_VBIOSChipName + "\"", "" },
            { "OfficialChipSKU: " + chipSku, "" },
            { "PCI Class Code: \"" + m_Sig.m_PciClassCode + "\"",
              "" },
        };

        for (UINT32 ii = 0; ii < NUMELEMS(boardDefEntries); ++ii)
        {
            Printf(pri, "    %-20s%s\n",
                   (boardDefEntries[ii].entry + ",").c_str(),
                   boardDefEntries[ii].comment.empty()
                   ? "" : (" // " + boardDefEntries[ii].comment).c_str());
        }

        set<const RegBitField*> fieldsToSkip;
        if (cooked)
        {
            // A "cooked" board description leaves out some reg bits.
            // I.e. it is more permissive, it allows a less-exact match.
            fieldsToSkip.insert(&s_BitsStrappedXclk277);
            fieldsToSkip.insert(&s_BitsStrappedSubVendor);
            fieldsToSkip.insert(&s_BitsStrappedSlotClkCfg);
            fieldsToSkip.insert(&s_BitsStrappedPexPllEnTerm100);
            fieldsToSkip.insert(&s_BitsStrappedFanDebugPwm);
            fieldsToSkip.insert(&s_BitsAer);
            fieldsToSkip.insert(&s_BitsPlx);
            fieldsToSkip.insert(&s_BitsGemini);
        }

        // Show RegBits
        //
        vector<bool> cookedRegBits = m_RegBits;
        for (set<const RegBitField*>::iterator ppField = fieldsToSkip.begin();
             ppField != fieldsToSkip.end(); ++ppField)
        {
            (*ppField)->WriteValue(&cookedRegBits, RegBitField::DONT_CARE);
        }
        string strRegBits;
        for (UINT32 ii = 0; ii < NUM_REG_BITS; ii += 32)
        {
            strRegBits += Utility::StrPrintf(" 0x%08x",
                                        Utility::PackBits(cookedRegBits, ii));
        }
        Printf(pri, "    RegBits:%s\n", strRegBits.c_str());

        for (UINT32 ii = 0; ii < NUMELEMS(s_RegBitFields); ++ii)
        {
            const RegBitField *pField = s_RegBitFields[ii];
            if (fieldsToSkip.count(pField))
                continue;
            UINT32 dutValue = pField->ReadValue(m_RegBits);
            Printf(pri, "    %20s // %s = %s\n",
                   "", pField->GetName().c_str(),
                   pField->GetValueName(dutValue).c_str());
        }
    }
    else
    {
        Printf(pri,
               "    BoardProjectAndSku: \"%s\",\n"
               "    VBIOSChipName: \"%s\"\n",
               m_Sig.m_BoardProjectAndSku.c_str(),
               m_Sig.m_VBIOSChipName.c_str());
    }

    Printf(pri,
           "  }\n"
           "  ]\n"
           "};\n");
}

namespace
{
    const string s_ErrorHeader =
        "BoardsDB Website Item                                   Device  "
        "%10s\n"
        "-----------------------------------------------------------------"
        "---------\n";
}

//-----------------------------------------------------------------------------
// FieldChecker -- a helper for ValidSkuCheck that factors out a bunch
//                 of repeated code for checking fields with overrides.
//-----------------------------------------------------------------------------
struct FieldChecker
{
    const char *    m_Name;
    RC              m_FailingRc;
    UINT32          m_OverrideMilwalue;
    UINT32          m_OverrideExactValue;

    FieldChecker
    (
        const char * name,
        RC failingRc
    )
    :   m_Name(name),
        m_FailingRc(failingRc),
        m_OverrideMilwalue(0),
        m_OverrideExactValue(0)
    { }

    RC CheckOverrides
    (
        UINT32 dutValue,
        Tee::Priority pri,
        bool bPrintHeader,
        GpuSubdevice * subdev
    );
    RC CheckBoardMatch
    (
        UINT32 dutValue,
        UINT32 boardMatchValue,
        Tee::Priority pri,
        bool bPrintHeader,
        GpuSubdevice * subdev
    );
};

//-----------------------------------------------------------------------------
RC FieldChecker::CheckOverrides
(
    UINT32 dutValue,
    Tee::Priority pri,
    bool bPrintHeader,
    GpuSubdevice * subdev
)
{
    if ((m_OverrideExactValue != 0) && (m_OverrideExactValue != dutValue))
    {
        if (bPrintHeader)
            Printf(pri, s_ErrorHeader.c_str(), "Overrides");
        Printf(pri, "%-50s  %10u  %10u\n",
               m_Name,
               dutValue,
               m_OverrideExactValue);
        return m_FailingRc;
    }
    if ((m_OverrideMilwalue != 0) && (m_OverrideMilwalue > dutValue))
    {
        if (bPrintHeader)
            Printf(pri, s_ErrorHeader.c_str(), "Overrides");

        string minString = Utility::StrPrintf("min %u", m_OverrideMilwalue);
        Printf(pri, "%-50s  %10u  %10s\n",
               m_Name,
               dutValue,
               minString.c_str());
        return m_FailingRc;
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC FieldChecker::CheckBoardMatch
(
    UINT32 dutValue,
    UINT32 boardMatchValue,
    Tee::Priority pri,
    bool bPrintHeader,
    GpuSubdevice * subdev
)
{
    if ((m_OverrideExactValue != 0) || (m_OverrideMilwalue != 0))
    {
        // We are not checking a boards.js match, the command-line override
        // takes priority.
        return OK;
    }
    if (dutValue != boardMatchValue)
    {
        if (bPrintHeader)
            Printf(pri, s_ErrorHeader.c_str(), "boards.db");
        Printf(pri, "%-50s  %10u  %10u\n",
               m_Name,
               dutValue,
               boardMatchValue);
        return m_FailingRc;
    }
    return OK;
}

//-----------------------------------------------------------------------------
// RegBitsChecker
//   a helper for ValidSkuCheck that looks a lot like FieldChecker, but
//   for the special-case of the RegBits bit-array.
//-----------------------------------------------------------------------------
struct RegBitsChecker
{
    vector<bool> m_Override;
    bool         m_OverrideValid;

    RegBitsChecker()
    :   m_Override(NUM_REG_BITS, false),
        m_OverrideValid(false)
    { }

    RC CheckOverrides
    (
        const vector<bool> & dutRegBits,
        Tee::Priority pri,
        bool bPrintHeader
    );
    RC CheckBoardMatch
    (
        const vector<bool> &dutRegBits,
        const vector<bool> &expectedRegBits,
        const set<const RegBitField*> &fieldsToSkip,
        Tee::Priority pri,
        bool bPrintHeader
    );
};

//-----------------------------------------------------------------------------
RC RegBitsChecker::CheckOverrides
(
    const vector<bool> &dutRegBits,
    Tee::Priority pri,
    bool bPrintHeader
)
{
    if (! m_OverrideValid)
        return OK;

    RC rc;
    bool bHeaderPrinted = false;
    for (UINT32 ii = 0; ii < NUMELEMS(s_RegBitFields); ++ii)
    {
        const RegBitField *pField = s_RegBitFields[ii];
        UINT32 dutValue = pField->ReadValue(dutRegBits);
        UINT32 expectedValue = pField->ReadValue(m_Override);
        if ((expectedValue != pField->DONT_CARE) && (expectedValue != dutValue))
        {
            if (bPrintHeader && !bHeaderPrinted)
            {
                Printf(pri, s_ErrorHeader.c_str(), "Overrides");
                bHeaderPrinted = true;
            }
            Printf(pri, "%-50s  %10s  %10s\n",
                   pField->GetWebsiteString().c_str(),
                   pField->GetValueName(dutValue).c_str(),
                   pField->GetValueName(expectedValue).c_str());
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC RegBitsChecker::CheckBoardMatch
(
    const vector<bool> &dutRegBits,
    const vector<bool> &expectedRegBits,
    const set<const RegBitField*> &fieldsToSkip,
    Tee::Priority pri,
    bool bPrintHeader
)
{
    RC rc;
    bool bHeaderPrinted = false;
    for (UINT32 ii = 0; ii < NUMELEMS(s_RegBitFields); ++ii)
    {
        const RegBitField *pField = s_RegBitFields[ii];
        if (fieldsToSkip.count(pField))
        {
            continue;
        }
        UINT32 dutValue = pField->ReadValue(dutRegBits);
        UINT32 expectedValue = (m_OverrideValid)? pField->ReadValue(m_Override) : pField->ReadValue(expectedRegBits);
        if ((expectedValue != pField->DONT_CARE) && (expectedValue != dutValue))
        {
            if (bPrintHeader && !bHeaderPrinted)
            {
                Printf(pri, s_ErrorHeader.c_str(), "boards.db");
                bHeaderPrinted = true;
            }
            Printf(pri, "%-50s  %10s  %10s\n",
                   pField->GetWebsiteString().c_str(),
                   pField->GetValueName(dutValue).c_str(),
                   pField->GetValueName(expectedValue).c_str());
            rc = RC::INCORRECT_FEATURE_SET;
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
// ValidSkuCheck -- this is the test itself.
//------------------------------------------------------------------------------
class ValidSkuCheck : public GpuTest
{
public:
    ValidSkuCheck();
    RC Setup();
    RC Cleanup();
    bool IsSupported();

    // Functions
    RC Run();
    RC Run(Tee::Priority outputPri);
    RC ExamineDut(BoardDef *pDutInfo);
    RC PrintFailureCauses(const BoardDB::BoardEntry & def,
                          const BoardDef * pDut,
                          Tee::Priority outputPri);

    RC InitFromJs();
    void PrintJsProperties(Tee::Priority pri);

    // runtime variables
    Tee::Priority  m_OutputPri;

    // FieldChecker helpers:
    FieldChecker m_ChkWidth;
    FieldChecker m_ChkExtbank;
    FieldChecker m_ChkPciExpressLanes;
    RegBitsChecker m_ChkRegBits;

    bool   m_SkipBoardDetect;
    bool   m_SkipPexLaneCheck;
    bool   m_SkipPexSpeedCheck;
    UINT32 m_PexSpeedCapOverride;
    UINT32 m_PexSpeedInitOverride;
    bool   m_SkipQualFuses;
    bool   m_SkipPlxCheck;
    bool   m_SkipGeminiCheck;
    string m_ChipSkuOverride;
    bool   m_SkipEccCheck;

    // Javascript properties
    SETGET_PROP(SkipBoardDetect, bool);
    SETGET_PROP(SkipPexLaneCheck, bool);
    SETGET_PROP(SkipPexSpeedCheck, bool);
    SETGET_PROP(PexSpeedCapOverride, UINT32);
    SETGET_PROP(PexSpeedInitOverride, UINT32);
    SETGET_PROP(SkipQualFuses, bool);
    SETGET_PROP(SkipPlxCheck, bool);
    SETGET_PROP(SkipGeminiCheck, bool);
    SETGET_PROP(ChipSkuOverride, string);
    SETGET_PROP(SkipEccCheck, bool);
  private:
    RC CalcMaxPexSpeedPState();
    RC InnerPlxRun(GpuSubdevice* pGpuSubdev, Tee::Priority errorPri);
    RC GeminiCheck(GpuSubdevice* pGpuSubdev, Tee::Priority errorPri);
    bool VerifyAerOffset(INT32 domainNumber,
                         INT32 busNumber,
                         INT32 deviceNumber,
                         INT32 functionNumber,
                         UINT16 expectedOffset) const;
    RC ConfirmBoard
    (
        Tee::Priority outputPri,
        const string &boardName,
        UINT32 index
    );
    UINT32 m_MaxSpeedPState;
    UINT32 m_ActualPState;
    Pcie *m_pGpuPcie;
};

//------------------------------------------------------------------------------
bool ValidSkuCheck::IsSupported()
{
    // ValidSkuCheck is not supported on Emulation/Simulation.
    if (GetBoundGpuSubdevice()->IsEmOrSim())
    {
        JavaScriptPtr pJs;
        bool sanityMode = false;
        pJs->GetProperty(pJs->GetGlobalObject(), "g_SanityMode", &sanityMode);
        if (!sanityMode)
            return false;
    }

    const BoardDB & boards = BoardDB::Get();
    UINT32 version = boards.GetDUTSupportedVersion(GetBoundGpuSubdevice());

    if ( version < 1 )
    {
        Printf(Tee::PriLow,
               "We don't have boards.js entries for pre G9x products. Skipping\n");
        return false;
    }

    if ( version > 1 )
    {
        Printf(Tee::PriLow,
               "We don't have boards.db version 1 entries for post GM20x products. Skipping\n");
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
ValidSkuCheck::ValidSkuCheck() :
    m_OutputPri(Tee::PriLow),
    // Checkers:              name                       failing rc                  feature
    m_ChkWidth(          "FB Bus Width (bits)", RC::MEMORY_SIZE_ERROR),
    m_ChkExtbank(        "External Banks",      RC::MEMORY_SIZE_ERROR),
    m_ChkPciExpressLanes("PCIe Lanes",          RC::INCORRECT_PCIX_LANES),
    m_ChkRegBits(),
    m_SkipBoardDetect(0),
    m_SkipPexLaneCheck(false),
    m_SkipPexSpeedCheck(false),
    m_PexSpeedCapOverride(Pci::SpeedUnknown),
    m_PexSpeedInitOverride(Pci::SpeedUnknown),
    m_SkipQualFuses(false),
    m_SkipPlxCheck(false),
    m_SkipGeminiCheck(false),
    m_ChipSkuOverride(""),
    m_SkipEccCheck(false),
    m_MaxSpeedPState(0),
    m_ActualPState(0),
    m_pGpuPcie(NULL)
{
    SetName("ValidSkuCheck");
}

JS_CLASS_INHERIT(ValidSkuCheck, GpuTest, "Memory size detection test");

//------------------------------------------------------------------------------
// object, methods, and tests.
//------------------------------------------------------------------------------
CLASS_PROP_READWRITE(ValidSkuCheck, SkipBoardDetect, bool,
        "If nonzero, skip board detection");
CLASS_PROP_READWRITE(ValidSkuCheck, SkipPexLaneCheck, bool,
        "if true, do not check number of PEX lanes");
CLASS_PROP_READWRITE(ValidSkuCheck, SkipPexSpeedCheck, bool,
        "If nonzero, test with Gen1 speed");
CLASS_PROP_READWRITE(ValidSkuCheck, PexSpeedCapOverride, UINT32,
        "Instead of testing for the board.js specified PEX Capability speed, use the override");
CLASS_PROP_READWRITE(ValidSkuCheck, PexSpeedInitOverride, UINT32,
        "Instead of testing for the board.js specified PEX Initialization speed, use the override");
CLASS_PROP_READWRITE(ValidSkuCheck, SkipQualFuses, bool,
        "If true, will bypass qual fuse when identifying chip SKUs");
CLASS_PROP_READWRITE(ValidSkuCheck, ChipSkuOverride, string,
        "check against a new Chip Sku");
CLASS_PROP_READWRITE(ValidSkuCheck, SkipPlxCheck, bool,
        "If true, will skip PLX test");
CLASS_PROP_READWRITE(ValidSkuCheck, SkipEccCheck, bool,
        "If true, will skip ECC test");
CLASS_PROP_READWRITE(ValidSkuCheck, SkipGeminiCheck, bool,
        "If true, will skip Gemini test");

PROP_DESC(ValidSkuCheck, BusOverride, 0,
        "If nonzero, test for this bus width (in bits)");
PROP_DESC(ValidSkuCheck, ExtOverride, 0,
          "If nonzero, test for this number of external banks");
PROP_DESC(ValidSkuCheck, PxlOverride, 0,
          "If nonzero, test for this number of PCI express lanes");

//------------------------------------------------------------------------------
RC ValidSkuCheck::Run()
{
    return (Run(Tee::PriNormal));
}

//------------------------------------------------------------------------------
RC ValidSkuCheck::Setup()
{
    RC rc = OK;
    GpuSubdevice *pGpuSubdev = GetBoundGpuSubdevice();

    CHECK_RC(GpuTest::Setup());
    m_pGpuPcie = pGpuSubdev->GetInterface<Pcie>();

    if (m_SkipPexSpeedCheck &&
        ((m_PexSpeedCapOverride != Pci::SpeedUnknown) ||
         (m_PexSpeedInitOverride != Pci::SpeedUnknown)))
    {
        Printf(Tee::PriHigh,
               "ValidSkuCheck : Setting PexSpeedCapOverride or PexSpeedInitOverride\n"
               "                SkipPexSpeedCheck=true is not a supported test configuration\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    Perf *pPerf = pGpuSubdev->GetPerf();
    UINT32 numPStates = 0;
    CHECK_RC(pPerf->GetNumPStates(&numPStates));
    if (numPStates)
    {
        CHECK_RC(CalcMaxPexSpeedPState());
        if (m_ActualPState != m_MaxSpeedPState)
        {
            CHECK_RC(pPerf->ForcePState(m_MaxSpeedPState,
                        LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));
        }
    }
    else
    {
        Printf(Tee::PriHigh,"ValidSkuCheck: No valid PState defined\n");
    }

    // Skip Gemini test if we are lwrrently using only_pci_dev
    set<UINT64> allowedPciDevices;
    GpuPtr()->GetAllowedPciDevices(&allowedPciDevices);
    if (!allowedPciDevices.empty())
    {
        Printf(Tee::PriLow, "Skipping Gemini Check since '-only_pci_dev' is in use\n");
        SetSkipGeminiCheck(true);
    }

    return rc;
}

//------------------------------------------------------------------------------
RC ValidSkuCheck::Cleanup()
{
    RC rc = OK;
    GpuSubdevice *pGpuSubdev = GetBoundGpuSubdevice();
    Perf *pPerf = pGpuSubdev->GetPerf();
    if (m_ActualPState != m_MaxSpeedPState)
    {
        //Restore actualPState
        CHECK_RC(pPerf->ForcePState(m_ActualPState,
                    LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));
    }
    CHECK_RC(GpuTest::Cleanup());
    return rc;
}

//------------------------------------------------------------------------------
RC ValidSkuCheck::CalcMaxPexSpeedPState()
{
    RC rc;
    GpuSubdevice *pGpuSubdev = GetBoundGpuSubdevice();
    Perf *pPerf = pGpuSubdev->GetPerf();
    vector <UINT32> availablePStates;
    UINT32 maxlnkSpeed = 0;
    UINT32 linkSpeed = 0;

    CHECK_RC(pPerf->GetLwrrentPState(&m_ActualPState));
    CHECK_RC(pPerf->GetAvailablePStates(&availablePStates));
    if (availablePStates.size() == 1)
    {
        m_MaxSpeedPState = m_ActualPState;
    }
    else
    {
        vector <UINT32>::const_iterator ii = availablePStates.begin();
        while (ii < availablePStates.end())
        {
            // Read expected link speed for all PStates
            CHECK_RC(pPerf->GetPexSpeedAtPState((*ii), &linkSpeed));
            if (linkSpeed > maxlnkSpeed)
            {
                maxlnkSpeed = linkSpeed;
                m_MaxSpeedPState = *ii;
            }
            ii++;
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
bool ValidSkuCheck::VerifyAerOffset(INT32 domainNumber,
                                    INT32 busNumber,
                                    INT32 deviceNumber,
                                    INT32 functionNumber,
                                    UINT16 expectedOffset) const
{
    UINT08 baseCapOffset;
    UINT16 extCapOffset;
    UINT16 extCapSize;

    Pci::FindCapBase(domainNumber,
                     busNumber,
                     deviceNumber,
                     functionNumber,
                     PCI_CAP_ID_PCIE,
                     &baseCapOffset);

    if (OK == Pci::GetExtendedCapInfo(domainNumber,
                                      busNumber,
                                      deviceNumber,
                                      functionNumber,
                                      Pci::AER_ECI,
                                      baseCapOffset,
                                      &extCapOffset,
                                      &extCapSize))
    {
        return extCapOffset == expectedOffset;
    }

    return false;
}

//------------------------------------------------------------------------------
RC ValidSkuCheck::ExamineDut(BoardDef *pDutInfo)
{
    FrameBuffer    *pFb;
    GpuSubdevice * pGpuSubdev;
    LwRm *pLwRm = GetBoundRmClient();

    RC rc = OK;

    pGpuSubdev = GetBoundGpuSubdevice();
    BoardDB::Signature sig;
    const BoardDB & boards = BoardDB::Get();
    CHECK_RC(boards.GetDUTSignature(pGpuSubdev, &sig));
    pDutInfo->SetSignature(sig);

    pFb = pGpuSubdev->GetFB();

    // Note that we're filling in the BoardDef for the Device Under Test,
    // as much as possible in the same order as the fields in boards.js.
    // This helps us notice our coding errors.
    //
    // Make sure all code paths provide at least a decent default value for
    // all fields, both in ExamineDut and in ParseInnerArray.

    pDutInfo->SetFbBus(pFb->GetBusWidth());
    pDutInfo->SetExtBanks(pFb->GetRankCount());
    pDutInfo->SetPcieLinkWidth(m_pGpuPcie->GetLinkWidth());

    // Fill in RegBits.
    //
    pDutInfo->SetRegFlag(s_BitsResmanGl,
            GetBoundGpuDevice()->CheckCapsBit(GpuDevice::QUADRO_GENERIC));

    pDutInfo->SetRegFlag(s_BitsStrappedXclk277, false);
    pDutInfo->SetRegFlag(s_BitsStrappedSubVendor,
                        Device::GPUSUB_STRAPS_SUB_VENDOR, pGpuSubdev);
    pDutInfo->SetRegFlag(s_BitsStrappedSlotClkCfg,
                        Device::GPUSUB_STRAPS_SLOT_CLK_CFG, pGpuSubdev);
    pDutInfo->SetRegFlag(s_BitsStrappedPexPllEnTerm100,
                        Device::GPUSUB_STRAPS_PEX_PLL_EN_TERM100, pGpuSubdev);

    // Test for AER
    bool gpuAerEnabled = pGpuSubdev->GetInterface<Pcie>()->AerEnabled();
    bool gpuAerOffsetCorrect = false;

    if (gpuAerEnabled)
    {
        // Verify AER offset in PCIE Extended Capabilities list
        gpuAerOffsetCorrect = VerifyAerOffset(pGpuSubdev->GetInterface<Pcie>()->DomainNumber(),
                                              pGpuSubdev->GetInterface<Pcie>()->BusNumber(),
                                              pGpuSubdev->GetInterface<Pcie>()->DeviceNumber(),
                                              pGpuSubdev->GetInterface<Pcie>()->FunctionNumber(),
                                              pGpuSubdev->GetInterface<Pcie>()->GetAerOffset());
    }

#ifdef INCLUDE_AZALIA
    // If we have a multifunctional SKU, both functions must support AER
    AzaliaController* const pAzalia = pGpuSubdev->GetAzaliaController();
    if (pAzalia != nullptr)
    {
        bool azaAerEnabled = pAzalia->AerEnabled();
        bool azaAerOffsetCorrect = false;

        if (azaAerEnabled)
        {
            // Find AER offset in PCIE Extended Capabilities list
            SysUtil::PciInfo azaPciInfo = {0};
            CHECK_RC(pAzalia->GetPciInfo(&azaPciInfo));

            azaAerOffsetCorrect = VerifyAerOffset(azaPciInfo.DomainNumber,
                                                  azaPciInfo.BusNumber,
                                                  azaPciInfo.DeviceNumber,
                                                  azaPciInfo.FunctionNumber,
                                                  pAzalia->GetAerOffset());
        }

        pDutInfo->SetRegFlag(s_BitsAer,
                            azaAerEnabled &&
                            azaAerOffsetCorrect &&
                            gpuAerEnabled &&
                            gpuAerOffsetCorrect);
    }
    else
    {
        pDutInfo->SetRegFlag(s_BitsAer, gpuAerEnabled && gpuAerOffsetCorrect);
    }
#else
    pDutInfo->SetRegFlag(s_BitsAer, gpuAerEnabled && gpuAerOffsetCorrect);
#endif

    if (pGpuSubdev->HasFeature(Device::GPUSUB_STRAPS_FAN_DEBUG_PWM_66) ||
        pGpuSubdev->HasFeature(Device::GPUSUB_STRAPS_FAN_DEBUG_PWM_33))
    {
        if (pGpuSubdev->IsFeatureEnabled(
                    Device::GPUSUB_STRAPS_FAN_DEBUG_PWM_66))
        {
            pDutInfo->SetRegBits(s_BitsStrappedFanDebugPwm,
                                 s_BitsStrappedFanDebugPwm.PWM_66);
        }
        else if (pGpuSubdev->IsFeatureEnabled(
                    Device::GPUSUB_STRAPS_FAN_DEBUG_PWM_33))
        {
            pDutInfo->SetRegBits(s_BitsStrappedFanDebugPwm,
                                 s_BitsStrappedFanDebugPwm.PWM_33);
        }
        else
        {
            pDutInfo->SetRegBits(s_BitsStrappedFanDebugPwm,
                                 s_BitsStrappedFanDebugPwm.DISABLED);
        }
    }

    if (pGpuSubdev->HasFeature(Device::GPUSUB_SUPPORTS_ECC))
    {
        bool eccSupported;
        UINT32 enableMask;
        CHECK_RC(pGpuSubdev->GetEccEnabled(&eccSupported, &enableMask));
        pDutInfo->SetRegFlag(s_BitsResmanEcc, eccSupported && enableMask);
    }

    LW2080_CTRL_PMGR_PWR_POLICY_INFO_PARAMS pwrPolicyParams = { 0 };
    CHECK_RC(pLwRm->ControlBySubdevice(pGpuSubdev,
                            LW2080_CTRL_CMD_PMGR_PWR_POLICY_GET_INFO,
                            &pwrPolicyParams,
                            sizeof(pwrPolicyParams)));
    pDutInfo->SetRegFlag(
            s_BitsResmanPwrcap,
            pwrPolicyParams.policyMask != 0);

    // Gpu Pci Capability
    bool gen3Capable = (m_pGpuPcie->LinkSpeedCapability() > Pci::Speed5000MBPS);
    bool gen2Capable = (gen3Capable ||
                        m_pGpuPcie->LinkSpeedCapability() > Pci::Speed2500MBPS);
    UINT32 lwrBusSpeed = m_pGpuPcie->GetLinkSpeed(Pci::SpeedUnknown);
    Printf(Tee::PriLow,
           "Gen3Capable = %d, Gen2Capable = %d, LwrBusSpeed = %d\n",
           gen3Capable, gen2Capable, lwrBusSpeed);

    Perf *pPerf = pGpuSubdev->GetPerf();
    UINT32 numPStates = 0;
    CHECK_RC(pPerf->GetNumPStates(&numPStates));
    if (numPStates)
    {
        if (m_SkipPexSpeedCheck)
        {
            Printf(Tee::PriHigh, "User requested to skip PEX speed test. \n");
            if (gen2Capable || gen3Capable)
            {
                Printf(Tee::PriHigh, "Detected Gen2 or Gen3 capable, "
                        "but we're skipping the Gen2 and Gen3 speed test.\n");
            }
        }

        pDutInfo->SetRegFlag(s_BitsResmanGen3, gen3Capable);
        pDutInfo->SetRegFlag(s_BitsResmanGen2, gen2Capable);
        if (lwrBusSpeed == 8000)
            pDutInfo->SetRegBits(s_BitsResmanInitGen, s_BitsResmanInitGen.GEN3);
        else if (lwrBusSpeed == 5000)
            pDutInfo->SetRegBits(s_BitsResmanInitGen, s_BitsResmanInitGen.GEN2);
        else
            pDutInfo->SetRegBits(s_BitsResmanInitGen, s_BitsResmanInitGen.GEN1);
    }

    bool passesPlxCheck = (OK == InnerPlxRun(pGpuSubdev, Tee::PriNone));
    pDutInfo->SetRegFlag(s_BitsPlx, passesPlxCheck);

    bool onGeminiBoard = (OK == GeminiCheck(pGpuSubdev, Tee::PriNone));
    pDutInfo->SetRegFlag(s_BitsGemini, onGeminiBoard);

    string fileName = Gpu::DeviceIdToString(pGpuSubdev->DeviceId());
    fileName = Utility::ToLowerCase(fileName) + "_f.xml";
    const FuseUtil::MiscInfo *pInfo;
    FuseUtil::GetP4Info(fileName, &pInfo);
    Printf(Tee::PriLow, "XML revision: %d\n", pInfo->P4Data.P4Revision);

    // bug 656960 - yuck. It sucks that MODS have to remember the name of the
    // qual fuses
    pGpuSubdev->GetFuse()->ClearIgnoreList();
    if (m_SkipQualFuses)
    {
        Printf(Tee::PriNormal, "Skipping the check for qual fuses\n");
        pGpuSubdev->GetFuse()->AppendIgnoreList("OPT_SAMPLE_BIN");
        pGpuSubdev->GetFuse()->AppendIgnoreList("OPT_SAMPLE_TYPE");
    }

    vector<string> chipSKUs;
    rc = pGpuSubdev->GetFuse()->FindMatchingSkus(&chipSKUs, FuseFilter::ALL_FUSES);

    // print the SKUs that matched:
    Printf(Tee::PriNormal, "Found SKU: ");
    for (UINT32 i = 0; i < chipSKUs.size(); i++)
    {
        Printf(Tee::PriNormal, " %s ", chipSKUs[i].c_str());
        pDutInfo->AddChipSku(chipSKUs[i]);
    }

    Printf(Tee::PriNormal, "\n");

    pGpuSubdev->GetFuse()->ClearIgnoreList();
    return rc;
}

// Checks if the GPU subdevice is attached to a properly configured PLX PCIe switch
//
// This test is very strict and is only required to work starting with Pascal.
// Older boards are likely to have outdated PLX firmware which is improperly configured.
// Most older boards will fail this test unless the PLX firmware is updated.
RC ValidSkuCheck::InnerPlxRun(GpuSubdevice* pGpuSubdev, Tee::Priority errorPri)
{
    RC rc;

    PexDevice* bridge;
    UINT32 parentPort;
    UINT32 readVal = 0;
    UINT16 readVal16 = 0;
    CHECK_RC(pGpuSubdev->GetInterface<Pcie>()->GetUpStreamInfo(&bridge, &parentPort));

    // Upstream PCIe device must exist
    if (bridge == nullptr)
    {
        Printf(errorPri, "Upstream PCIe device not present\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    // PCIe device must be a switch
    if (bridge->IsRoot())
    {
        Printf(errorPri, "Upstream PEX device is not a switch\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    PexDevice::PciDevice* upstreamPort = bridge->GetUpStreamPort();
    MASSERT(upstreamPort);

    // 1. Make sure the PCI Vendor ID == 0x10B5 which corresponds to PLX Technology
    CHECK_RC(Platform::PciRead16(upstreamPort->Domain, upstreamPort->Bus, upstreamPort->Dev,
                                 upstreamPort->Func, 0x00, &readVal16));

    if (readVal16 != Pci::Plx)
    {
        Printf(errorPri, "Upstream PEX device is not a PLX switch\n");
        return RC::PCI_ILWALID_VENDOR_IDENTIFICATION;
    }

    // 2. Make sure bits 26 and 3 of pci config register 0xF60 is set
    CHECK_RC(Platform::PciRead32(upstreamPort->Domain, upstreamPort->Bus, upstreamPort->Dev,
                    upstreamPort->Func, 0xF60, &readVal));

    if (~readVal & BIT(26))
    {
        Printf(errorPri,
               "The PLX switch at %02x:%02x.%x does not disable upstream BARs\n",
               upstreamPort->Bus, upstreamPort->Dev, upstreamPort->Func);
        return RC::PCI_FUNCTION_NOT_SUPPORTED;
    }
    if (~readVal & BIT(3))
    {
        Printf(errorPri,
               "The PLX switch at %02x:%02x.%x does not route TLPs correctly\n",
               upstreamPort->Bus, upstreamPort->Dev, upstreamPort->Func);
        return RC::PCI_FUNCTION_NOT_SUPPORTED;
    }

    // 3. PLX switch must have SVID == 0x10DE (LWPU)
    UINT08 ssvidCapOffset;
    CHECK_RC(Pci::FindCapBase(upstreamPort->Domain, upstreamPort->Bus, upstreamPort->Dev,
                              upstreamPort->Func, PCI_CAP_ID_SSVID, &ssvidCapOffset));

    CHECK_RC(Platform::PciRead16(upstreamPort->Domain, upstreamPort->Bus, upstreamPort->Dev,
                                 upstreamPort->Func, ssvidCapOffset + LW_PCI_CAP_SSVID_VENDOR,
                                 &readVal16));

    if (readVal16 != Pci::Lwpu)
    {
        Printf(errorPri,
            "Invalid SVID (0x%04x) on the PLX switch at %02x:%02x.%x. Expecting SVID to be 0x10de.\n",
            readVal16, upstreamPort->Bus, upstreamPort->Dev, upstreamPort->Func);
        return RC::PCI_ILWALID_VENDOR_IDENTIFICATION;
    }

    // 4. PLX switch must have same SSID as the GPU
    CHECK_RC(Platform::PciRead16(upstreamPort->Domain, upstreamPort->Bus, upstreamPort->Dev,
                                 upstreamPort->Func, ssvidCapOffset + LW_PCI_CAP_SSVID_DEVICE,
                                 &readVal16));

    const UINT32 gpuSSID = pGpuSubdev->GetInterface<Pcie>()->SubsystemDeviceId();
    if (readVal16 != gpuSSID)
    {
        Printf(errorPri,
            "Invalid SSID (0x%04x) on the PLX switch at %02x:%02x.%x. Expecting SSID to be 0x%04x\n",
            readVal16, upstreamPort->Bus, upstreamPort->Dev,
            upstreamPort->Func, gpuSSID);
        return RC::PCI_ILWALID_VENDOR_IDENTIFICATION;
    }

    return OK;
}

// Checks if the GPU subdevice is part of a Gemini board
//
// 1. The Gemini GPUs must be connected to a PLX brand PCIe switch
// 2. The board project, board SKU, and SSID/SVID of Gemini GPUs must match
// 3. The Gemini GPUs must be connected to the same PCIe switch
RC ValidSkuCheck::GeminiCheck(GpuSubdevice*  pMySubdev, Tee::Priority errorPri)
{
    RC rc;

    BoardDB::Signature mySig;
    BoardDB::Get().GetDUTSignature(pMySubdev, &mySig);

    PexDevice* myBridge;
    CHECK_RC(pMySubdev->GetInterface<Pcie>()->GetUpStreamInfo(&myBridge));

    // 1. Gemini GPUs must be connected to a PLX brand PCIe switch
    //
    // Use a subset of the PLX check, since that check is very strict about
    // the configuration of the PLX switch, and older PLX roms will not pass

    // Upstream PCIe device must exist
    if (myBridge == nullptr)
    {
        Printf(errorPri, "Upstream PCIe device not present\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    // PCIe device must be a switch
    if (myBridge->IsRoot())
    {
        Printf(errorPri, "Upstream PEX device is not a switch\n");
        return RC::INCORRECT_FEATURE_SET;
    }

    // Switch must be a PLX brand switch
    UINT16 readVal16 = 0;
    PexDevice::PciDevice* upstreamPort = myBridge->GetUpStreamPort();
    MASSERT(upstreamPort);
    CHECK_RC(Platform::PciRead16(upstreamPort->Domain, upstreamPort->Bus, upstreamPort->Dev,
                                 upstreamPort->Func, 0x00, &readVal16));
    if (readVal16 != Pci::Plx)
    {
        Printf(errorPri, "Upstream PEX device is not a PLX brand switch\n");
        return RC::PCI_ILWALID_VENDOR_IDENTIFICATION;
    }

    // Find all gemini matches, inclusive of the GpuSubDevice being tested
    vector<GpuSubdevice*> matches;
    GpuDevMgr* deviceMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
    for (GpuSubdevice* pLwrSubdev = deviceMgr->GetFirstGpu();
                       pLwrSubdev != nullptr;
                       pLwrSubdev = deviceMgr->GetNextGpu(pLwrSubdev))
    {
        BoardDB::Signature lwrSig;
        BoardDB::Get().GetDUTSignature(pLwrSubdev, &lwrSig);

        // 2. If the Board Project, Board SKU, and SSID/SVID match it may be Gemini
        if (lwrSig.m_BoardProjectAndSku == mySig.m_BoardProjectAndSku &&
            lwrSig.m_SSID == mySig.m_SSID &&
            lwrSig.m_SVID == mySig.m_SVID)
        {
            PexDevice* lwrBridge;
            CHECK_RC(pLwrSubdev->GetInterface<Pcie>()->GetUpStreamInfo(&lwrBridge));
            MASSERT(lwrBridge);

            // 3. If the bridges are the same (and both PLX), it is a Gemini match
            if (lwrBridge == myBridge)
            {
                matches.push_back(pLwrSubdev);
            }
        }
    }

    if (matches.size() == 2)
    {
        return OK;
    }
    else
    {
        Printf(errorPri, "Expected %u Gemini devices, found %zu\n", 2, matches.size());
        return RC::DEVICE_NOT_FOUND;
    }
}

//------------------------------------------------------------------------------
RC ValidSkuCheck::Run(Tee::Priority outputPri)
{
    StickyRC        rc;
    GpuSubdevice *  pGpuSubdev = GetBoundGpuSubdevice();
    m_OutputPri = outputPri;

    // Read this Device-Under-Test's properties.

    BoardDef dut;
    CHECK_RC(ExamineDut(&dut));

    // Before searching for a matching description, check any requirements
    // from the command-line arguments (overrides to boards.js basically).
    //
    // Specifically, check and print all override failures before returning
    rc = m_ChkWidth.CheckOverrides(dut.GetFbBus(),
                                   m_OutputPri,
                                   true,
                                   pGpuSubdev);
    rc = m_ChkExtbank.CheckOverrides(dut.GetExtBanks(),
                                     m_OutputPri,
                                     (OK == rc),
                                     pGpuSubdev);

    if (!m_SkipPexLaneCheck)
    {
        rc = m_ChkPciExpressLanes.CheckOverrides(dut.GetPcieLinkWidth(),
                                                 m_OutputPri,
                                                 (OK == rc),
                                                 pGpuSubdev);
    }
    rc = m_ChkRegBits.CheckOverrides(dut.GetRegBits(), m_OutputPri, (OK == rc));

    if (m_SkipBoardDetect)
    {
        return OK;
    }

    string boardName;
    UINT32 index;
    const BoardDB & boards = BoardDB::Get();
    CHECK_RC(pGpuSubdev->GetBoardInfoFromBoardDB(&boardName, &index, false));
    const BoardDB::BoardEntry *pGoldEntry =
        boards.GetBoardEntry(pGpuSubdev->DeviceId(), boardName, index);

    const BoardDB::BoardEntry *pWebsiteEntry = nullptr;
    CHECK_RC(boards.GetWebsiteEntry(&pWebsiteEntry));

    Printf(Tee::PriNormal,
         "\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!! BOARD SUMMARY !!!!!!!!!!!!!!!!!!!!!"
         "!!!!!!!!!\n");
    if (pGoldEntry)
    {
        Printf(Tee::PriNormal, "!! Found %s (%d)\n", boardName.c_str(), index);
        rc = PrintFailureCauses(*pGoldEntry, &dut, Tee::PriNone);
        if (OK != rc)
        {
            Printf(Tee::PriNormal, "!! Failure causes:\n");
            rc = PrintFailureCauses(*pGoldEntry, &dut, Tee::PriNormal);
        }
    }
    else
    {
        Printf(Tee::PriNormal,
           "!! BOARD MATCH NOT FOUND\n"
           "!! The probable cause of this is that you have an outdated\n"
           "!! boards.db file.\n");
        rc = RC::MEMORY_SIZE_ERROR;
    }
    Printf(Tee::PriNormal,
         "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
         "!!!!!!!\n\n");

    dut.PrintInfo(false, (rc == RC::OK) ? Tee::PriSecret : outputPri);
    if (rc == OK)
    {
        CHECK_RC(ConfirmBoard(outputPri, boardName, index));
    }
    else
    {
        Printf(outputPri,
             "************************ LWPU AE / FAEs *************************\n"
             "* Please update http://mods/boards.html using the following       *\n"
             "* information as well as the board POR.                           *\n"
             "* See https://wiki.lwpu.com/engwiki/index.php/MODS/Board_Editor *\n"
             "* for instructions.                                               *\n"
             "*******************************************************************\n");
        dut.PrintInfo(true, outputPri);     // print cooked info

        if (pWebsiteEntry)
        {
            Printf(outputPri,
                   "Here is the cooked info you entered on the website.\n"
                   "Compare to the cooked info for this board above.\n");
            BoardDef(*pWebsiteEntry).PrintInfo(true, outputPri);
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC ValidSkuCheck::InitFromJs()
{
    RC rc;
    JavaScriptPtr pJS;
    JSObject *    jsObject = GetJSObject();

    CHECK_RC(GpuTest::InitFromJs());

    CHECK_RC(pJS->GetProperty(jsObject, "BusOverride",
                              &m_ChkWidth.m_OverrideExactValue));
    CHECK_RC(pJS->GetProperty(jsObject, "ExtOverride",
                              &m_ChkExtbank.m_OverrideExactValue));
    CHECK_RC(pJS->GetProperty(jsObject, "PxlOverride",
                              &m_ChkPciExpressLanes.m_OverrideExactValue));

    vector<UINT32> featureBits;
    rc = pJS->GetProperty(jsObject, "FeatureBits", &featureBits);
    if (rc == OK)
    {
        m_ChkRegBits.m_OverrideValid = true;
        m_ChkRegBits.m_Override.clear();
        for (UINT32 ii = 0; ii < featureBits.size(); ++ii)
        {
            Utility::UnpackBits(&m_ChkRegBits.m_Override,
                                featureBits[ii], ii * 32);
        }
        m_ChkRegBits.m_Override.resize(NUM_REG_BITS, false);
    }
    else if (rc == RC::ILWALID_OBJECT_PROPERTY)
    {
        rc.Clear();
    }
    CHECK_RC(rc);

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ void ValidSkuCheck::PrintJsProperties(Tee::Priority pri)
{
    string featureBits;
    if (m_ChkRegBits.m_OverrideValid)
    {
        featureBits = "[";
        for (UINT32 ii = 0; ii < NUM_REG_BITS; ii += 32)
        {
            featureBits += Utility::StrPrintf(
                    "0x%08x,",
                    Utility::PackBits(m_ChkRegBits.m_Override, ii));
        }
        featureBits.pop_back();
        featureBits += "]";
    }
    else
    {
        featureBits = "undefined";
    }

    GpuTest::PrintJsProperties(pri);
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tSkipBoardDetect:\t\t%s\n",
           m_SkipBoardDetect ? "true" : "false");
    Printf(pri, "\tSkipPexLaneCheck:\t\t%s\n",
           m_SkipPexLaneCheck ? "true" : "false");
    Printf(pri, "\tSkipPexSpeedCheck:\t\t%s\n",
           m_SkipPexSpeedCheck ? "true" : "false");
    Printf(pri, "\tPexSpeedCapOverride:\t\t%u\n", m_PexSpeedCapOverride);
    Printf(pri, "\tPexSpeedInitOverride:\t\t%u\n", m_PexSpeedInitOverride);
    Printf(pri, "\tSkipQualFuses:\t\t\t%s\n",
           m_SkipQualFuses ? "true" : "false");
    Printf(pri, "\tSkipPlxCheck:\t\t\t%s\n",
           m_SkipPlxCheck ? "true" : "false");
    Printf(pri, "\tSkipGeminiCheck:\t\t\t%s\n",
           m_SkipGeminiCheck ? "true" : "false");
    Printf(pri, "\tChipSkuOverride:\t\t\"%s\"\n", m_ChipSkuOverride.c_str());
    Printf(pri, "\tSkipEccCheck\t\t\t\"%s\"\n", m_SkipEccCheck ? "true" : "false");
    Printf(pri, "\tFeatureBits:\t\t\t%s\n", featureBits.c_str());
    Printf(pri, "\tBusOverride:\t\t\t%u\n", m_ChkWidth.m_OverrideExactValue);
    Printf(pri, "\tExtOverride:\t\t\t%u\n", m_ChkExtbank.m_OverrideExactValue);
    Printf(pri, "\tPxlOverride:\t\t\t%u\n",
           m_ChkPciExpressLanes.m_OverrideExactValue);
}

//-----------------------------------------------------------------------------
RC ValidSkuCheck::PrintFailureCauses
(
    const BoardDB::BoardEntry & def,
    const BoardDef* dut,
    Tee::Priority outputPri
)
{
    StickyRC firstRc;
    GpuSubdevice *   pGpuSubdev = GetBoundGpuSubdevice();

    firstRc = m_ChkWidth.CheckBoardMatch(dut->GetFbBus(),
                                         def.m_OtherInfo.m_FBBus,
                                         outputPri,
                                         true,
                                         pGpuSubdev);

    firstRc = m_ChkExtbank.CheckBoardMatch(dut->GetExtBanks(),
                                           def.m_OtherInfo.m_Extbanks,
                                           outputPri,
                                           (OK == firstRc),
                                           pGpuSubdev);

    if (!m_SkipPexLaneCheck)
    {
        firstRc = m_ChkPciExpressLanes.CheckBoardMatch(dut->GetPcieLinkWidth(),
                                                       def.m_OtherInfo.m_PciExpressLanes,
                                                       outputPri,
                                                       (OK == firstRc),
                                                       pGpuSubdev);
    }

    vector<bool> expectedRegBits = def.m_OtherInfo.m_RegBits;
    set<const RegBitField*> fieldsToSkip;
    if (m_SkipPexSpeedCheck)
    {
        fieldsToSkip.insert(&s_BitsResmanGen2);
        fieldsToSkip.insert(&s_BitsResmanGen3);
        fieldsToSkip.insert(&s_BitsResmanInitGen);
    }

    if (m_SkipEccCheck)
    {
        fieldsToSkip.insert(&s_BitsResmanEcc);
    }

    if (m_SkipPlxCheck)
    {
        fieldsToSkip.insert(&s_BitsPlx);
    }
    else
    {
        // If the PLX regbit is set, run the PLX check again
        // so that any errors are logged
        if (def.m_OtherInfo.m_RegBits.size() > BIT_PLX_ENABLED &&
            def.m_OtherInfo.m_RegBits[BIT_PLX_ENABLED])
        {
            InnerPlxRun(pGpuSubdev, outputPri);
        }
    }

    if (m_SkipGeminiCheck)
    {
        fieldsToSkip.insert(&s_BitsGemini);
    }
    else
    {
        // If the Gemini regbit is set, run the Gemini check again
        // so that any errors are logged
        if (def.m_OtherInfo.m_RegBits.size() > BIT_GEMINI_ENABLED &&
            def.m_OtherInfo.m_RegBits[BIT_GEMINI_ENABLED])
        {
            GeminiCheck(pGpuSubdev, outputPri);
        }
    }

    switch (m_PexSpeedCapOverride)
    {
        default:
        case Pci::SpeedUnknown:
            break;
        case Pci::Speed8000MBPS:
            s_BitsResmanGen2.WriteValue(&expectedRegBits,
                                        YesNoRegBitField::YES);
            s_BitsResmanGen3.WriteValue(&expectedRegBits,
                                        YesNoRegBitField::YES);
            break;
        case Pci::Speed5000MBPS:
            s_BitsResmanGen2.WriteValue(&expectedRegBits,
                                        YesNoRegBitField::YES);
            s_BitsResmanGen3.WriteValue(&expectedRegBits,
                                        YesNoRegBitField::NO);
            break;
        case Pci::Speed2500MBPS:
            s_BitsResmanGen2.WriteValue(&expectedRegBits,
                                        YesNoRegBitField::NO);
            s_BitsResmanGen3.WriteValue(&expectedRegBits,
                                        YesNoRegBitField::NO);
            break;
    }
    switch (m_PexSpeedInitOverride)
    {
        default:
        case Pci::SpeedUnknown:
            break;
        case Pci::Speed8000MBPS:
            s_BitsResmanInitGen.WriteValue(&expectedRegBits,
                                           InitGenRegBitField::GEN3);
            break;
        case Pci::Speed5000MBPS:
            s_BitsResmanInitGen.WriteValue(&expectedRegBits,
                                           InitGenRegBitField::GEN2);
            break;
        case Pci::Speed2500MBPS:
            s_BitsResmanInitGen.WriteValue(&expectedRegBits,
                                           InitGenRegBitField::GEN1);
            break;
    }

    firstRc = m_ChkRegBits.CheckBoardMatch(dut->GetRegBits(),
                                           expectedRegBits,
                                           fieldsToSkip,
                                           outputPri,
                                           (OK == firstRc));
    if (OK != firstRc)
        Printf(outputPri, "\n");

    const UINT32 numExpectedSKUs =
        static_cast<UINT32>(def.m_OtherInfo.m_OfficialChipSKUs.size());
    const UINT32 numDetectedSKUs = dut->GetNumMatchedChipSkus();

    bool chipSkuFound = false;
    string expectedSKU = def.m_OtherInfo.m_OfficialChipSKUs[0];

    for (UINT32 i=0; i < numDetectedSKUs; i++)
    {
        for (UINT32 j=0; j < numExpectedSKUs; j++)
        {
            if (m_ChipSkuOverride == "")
                expectedSKU = def.m_OtherInfo.m_OfficialChipSKUs[j];
            else
                expectedSKU = m_ChipSkuOverride;

            const string detectedSKU = dut->GetChipSku(i);
            if (detectedSKU == expectedSKU)
            {
                chipSkuFound = true;
                break;
            }
        }
        if (chipSkuFound)
        {
            break;
        }
    }

    if ((!chipSkuFound) &&
        (expectedSKU != "unknown_chip_sku") &&
        (m_ChipSkuOverride != "unknown_chip_sku"))
    {
        // print some fuse info:
        if (Tee::PriNone != outputPri)
        {
            Printf(Tee::PriNormal, "\nShould be SKU: ");
            if (m_ChipSkuOverride == "")
            {
                for (UINT32 j=0; j < numExpectedSKUs; j++)
                {
                    Printf(Tee::PriNormal, " %s ",
                           def.m_OtherInfo.m_OfficialChipSKUs[j].c_str());
                }
            }
            else
            {
                Printf(Tee::PriNormal, " %s ", m_ChipSkuOverride.c_str());
            }

            Printf(Tee::PriNormal, "\n");

            if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
            {
                Fuse *pFuse = GetBoundGpuSubdevice()->GetFuse();
                // print reasons why each of the expected SKU mismatched:
                if (m_ChipSkuOverride == "")
                {
                    for (UINT32 j=0; j < numExpectedSKUs; j++)
                    {
                        Printf(Tee::PriNormal,
                               "Failed to match SKU %s because \n",
                               def.m_OtherInfo.m_OfficialChipSKUs[j].c_str());
                        pFuse->PrintMismatchingFuses(
                                def.m_OtherInfo.m_OfficialChipSKUs[j].c_str(),
                                FuseFilter::ALL_FUSES);
                    }
                }
                else
                {
                    Printf(Tee::PriNormal, "Failed to match SKU %s because \n",
                           m_ChipSkuOverride.c_str());
                    pFuse->PrintMismatchingFuses(m_ChipSkuOverride,
                                           FuseFilter::ALL_FUSES);
                }
            }
        }
        firstRc = RC::INCORRECT_FEATURE_SET;
    }

    return firstRc;
}

//--------------------------------------------------------------------
//! Confirm the board if being run to do so.  Also ensure that no test
//! arguments or command line options are present that curtail a full
//! validation of the new boards.db entry
//!
//! Also print the confirmation string if this is an old-style board
//! change request without a specific list of tests to run
//!
//! \param outputPri Priority to print output at
//! \param BoardName Board name that was found.
//! \param Index Index for the board that was found
//!
//! \return OK if board was confirmed (or no confirmation necessary), not OK if
//!     confirmation is required but test argumetns or command line options
//!     skipped validation of some portion of the new entry.
//!
RC ValidSkuCheck::ConfirmBoard
(
    Tee::Priority outputPri,
    const string &boardName,
    UINT32 index
)
{
    const BoardDB & boards = BoardDB::Get();
    if (!boards.BoardChangeRequested())
        return OK;

    string testParams = "";
    string cmdLineArgs = "";
    if (m_SkipBoardDetect)
    {
        testParams  +=
           "!!      SkipBoardDetect                                       !!\n";
        cmdLineArgs +=
           "!!      -skip_board_detect                                    !!\n";
    }
    if (m_SkipPexLaneCheck)
    {
        testParams  +=
           "!!      SkipPexLaneCheck                                      !!\n";
    }
    if (m_SkipPexSpeedCheck)
    {
        testParams  +=
           "!!      SkipPexSpeedCheck                                     !!\n";
        cmdLineArgs +=
           "!!      -test_for_gen1                                        !!\n";
    }
    if (m_PexSpeedCapOverride != Pci::SpeedUnknown)
    {
        testParams  +=
           "!!      PexSpeedCapOverride                                   !!\n";
    }
    if (m_PexSpeedInitOverride != Pci::SpeedUnknown)
    {
        testParams  +=
           "!!      PexSpeedInitOverride                                  !!\n";
        cmdLineArgs  +=
           "!!      -test_for_gen2                                        !!\n";
    }
    if (m_SkipQualFuses)
    {
        testParams  +=
           "!!      SkipQualFuses                                         !!\n";
    }
    if (m_SkipPlxCheck)
    {
        testParams  +=
           "!!      SkipPlxCheck                                          !!\n";
    }
    if (m_SkipEccCheck)
    {
        testParams  +=
           "!!      SkipEccCheck                                          !!\n";
    }
    if (m_SkipGeminiCheck)
    {
        testParams  +=
           "!!      SkipGeminiCheck                                       !!\n";
    }
    if (m_ChipSkuOverride != "")
    {
        testParams  +=
           "!!      ChipSkuOverride                                       !!\n";
    }
    if (0 != m_ChkWidth.m_OverrideExactValue)
    {
        testParams  +=
           "!!      BusOverride                                           !!\n";
        cmdLineArgs +=
           "!!      -bus_width                                            !!\n";
    }
    if (0 != m_ChkExtbank.m_OverrideExactValue)
    {
        testParams  +=
           "!!      ExtOverride                                           !!\n";
        cmdLineArgs +=
           "!!      -ext_banks                                            !!\n";
    }
    if (0 != m_ChkPciExpressLanes.m_OverrideExactValue)
    {
        testParams  +=
           "!!      PxlOverride                                           !!\n";
        cmdLineArgs +=
           "!!      -check_pxl                                            !!\n";
    }
    if (m_ChkRegBits.m_OverrideValid)
    {
        testParams  +=
           "!!      FeatureBits                                           !!\n";
        cmdLineArgs +=
           "!!      -check_features                                       !!\n";
        cmdLineArgs +=
           "!!      -check_features3                                      !!\n";
    }
    if (testParams != "")
    {
        testParams.insert(0,
            "!!   Test Parameters (from \"-testarg\" or .spc file):"
            "          !!\n");
        testParams +=
           "!!                                                            !!\n";
    }
    if (cmdLineArgs != "")
    {
        cmdLineArgs.insert(0,
            "!!   Command Line Arguments:"
            "                                  !!\n");
        cmdLineArgs +=
           "!!                                                            !!\n";
    }

    if ((testParams != "") || (cmdLineArgs != ""))
    {
        Printf(outputPri,
            "\n\n"
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
            "!!     USER ERROR   USER ERROR    USER ERROR   USER ERROR     !!\n"
            "!!             CANNOT GENERATE CONFIRMATION STRING            !!\n"
            "!!                                                            !!\n"
            "!! Generating a confirmation string must be done on a POR GPU !!\n"
            "!! board including POR VBIOS on a motherboard that supports   !!\n"
            "!! the POR.  Remove any of the following test parameters or   !!\n"
            "!! command line options that are present and re-run           !!\n"
            "!! %s to generate the confirmation string:         !!\n"
            "!!                                                            !!\n"
            "%s%s"
            "!!                                                            !!\n"
            "!! Running MODS with a boards.dbe from the Board Editor       !!\n"
            "!! Website that contains an unconfirmed entry for any purpose !!\n"
            "!! other than generation of a confirmation string is not      !!\n"
            "!! supported                                                  !!\n"
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",
            GetName().c_str(), testParams.c_str(), cmdLineArgs.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    else
    {
        BoardDB::ConfirmBoard(boardName, index);

        bool bAnyTestsRequired;
        RC rc;

        // If no specific tests were required to confirm an entry, then that
        // means we are confirming an entry for an older GPU in a newer branch
        // and test 17 rather than boarddb.spc is the confirmation vehicle
        CHECK_RC(boards.ConfirmTestRequired("", &bAnyTestsRequired));
        if (!bAnyTestsRequired)
        {
            CHECK_RC(boards.ConfirmEntry());
        }
    }
    return OK;
}
