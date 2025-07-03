/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef CRCCHECK_H
#define CRCCHECK_H

#include <string>
#include <vector>
#include <set>
#include "gpu/include/vgacrc.h"
#include "core/include/types.h"
#include "core/include/rc.h"

class VGA_CRC;

namespace LegacyVGA {

    class CRCProfile
    {
    public:
        CRCProfile(UINT32 chipid, bool active_raster, bool full_frame, UINT32 frame);
        CRCProfile(bool active_raster, bool full_frame, UINT32 frame);
        CRCProfile(const CRCProfile&, UINT32 new_chipid);
        static CRCProfile* CreateCRCProfile(const string&);

        string Profile() const;

        bool compare(const CRCProfile&) const;
        bool operator<(const CRCProfile&) const;
        bool ActiveRaster() const {return m_active_raster;}
        bool FullFrame() const {return m_full_frame;}
        UINT32 ChipID() const {return m_chipid;}

    private:
        static string delims;

        UINT32   m_chipid;
        bool     m_active_raster;
        bool     m_full_frame;
        UINT32   m_frame;
    };

    VGA_CRC* CRCManager();

    static set<int> s_last_match;
    typedef map<CRCProfile, UINT32> CrcMap;
    typedef vector<UINT32> CrcChain;
    CrcMap* ReadCRCs(const string& filename);
    unsigned SaveCRCs(const CrcMap&, const string& filename, bool approvinfo);
    unsigned CountCRCs(const CrcMap*, bool active_raster, bool full_frame, UINT32 chip_id);
    CrcMap::iterator SearchCRCChain(const CRCProfile &, CrcMap& ,const CrcChain&);
    const char* ExceptionFile(const string& efile = "");
    const char* SubTarget(const string& target = "");
    const FLOAT64 vga_timeout_ms (FLOAT64 timeoutMs = 0);

}

inline bool operator==(const LegacyVGA::CRCProfile& lhs, const LegacyVGA::CRCProfile& rhs)
{
    return lhs.compare(rhs);
}

class VGA_CRC
{
public:
    VGA_CRC(UINT32 head, UINT32 dac, UINT32 int_mask);
    VGA_CRC(UINT32 head, UINT32 dac, UINT32 int_mask, const string&);
    VGA_CRC();
    ~VGA_CRC();

    RC CaptureFrame(UINT32 wait, UINT32 update, UINT32 sync, FLOAT64 timeoutMs);
    RC WaitCapture(UINT32 last, UINT32 update, FLOAT64 timeoutMs);
    RC WaitBlinkState (UINT32 blink_request, FLOAT64 timeoutMs);
    RC ResetCRCValid();
    RC SetFrameState(bool cursor, bool attr);
    RC ClearFrameState ();
    RC SetClockChanged();
   RC ClearClockChanged (void);
    RC SetHeadDac(UINT32 head, UINT32 dac);
    RC SetHeadSor(UINT32 head, UINT32 sor);
    RC GetHeadDac(UINT32 *phead, UINT32 *pdac);
    RC SetInt(UINT32 int_mask);

    unsigned Check(const string& test_dir, bool active_raster, bool full_frame, bool dump, const string& ptrace);

private:
    //returns true if "p4 edit" succeded
    bool TryP4Edit(const string& file_name);
    RC WaitFrames (UINT32 count, FLOAT64 timeoutMs);

    static UINT08 m_prev_state[];

    UINT08 m_frame_state;
    bool m_frame_state_set;
    bool m_clock_changed;
    std::vector<UINT32> m_crc;
    UINT32 m_head_index;
    UINT32 m_or_index;
    UINT32 m_int_mask;
    string m_or_type;
};

#endif /* CRCCHECK_H */
