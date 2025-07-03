/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012  by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _REGMAP_HELPER_H_
#define _REGMAP_HELPER_H_

// Include Standard Libraries
#include <string>
#include <utility>
#include <vector>

// Include Libraries for LW specific defines.
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/IRegisterMap.h"

class regmap_helper{
    LWGpuResource *lwgpu;
    IRegisterMap *m_regMap;
public:
    regmap_helper(LWGpuResource *, IRegisterMap *);

    // Register Write functions.
    int drf_def(string unit, string, string, string, UINT32 baseAddr = 0);
    int drf_def(string unit, string, vector<pair<string,string>>, UINT32 baseAddr = 0);
    int drf_num(string, string , string, int, UINT32 baseAddr = 0);
    int drf_num(string, string, UINT32, UINT32 baseAddr = 0);

    // Register Read function.
    UINT32 regRd(string unit, string reg_n, bool &valid, UINT32 baseAddr = 0);
    UINT32 regRd(string unit, string reg_n, string field_n, bool &valid, UINT32 baseAddr = 0);
    
    // Register Read and Check value function.
    int regChk(string unit, string reg_n, UINT32 val, UINT32 baseAddr = 0);
    int regChk(string unit, string reg_n, string field_n, string val_n, UINT32 baseAddr = 0);
    int regChk(string unit, string reg_n, vector<pair<string, string>> field_val_pairs, UINT32 baseAddr = 0);
    int regChk(string unit, string reg_n, string field_n, UINT32 val, UINT32 baseAddr = 0);
    int regMaskedChk(string unit, string reg_n, UINT32 val, UINT32 mask, UINT32 baseAddr = 0);

    // Register Poll function.
    int regPoll(string unit, string reg_n, UINT32 val, UINT32 interPollWait, int maxPoll, bool equal = true, UINT32 baseAddr = 0);
    int regPoll(string unit, string reg_n, UINT32 val, UINT32 mask, UINT32 interPollWait, int maxPoll, bool equal = true, UINT32 baseAddr = 0);
    int regPoll(string unit, string reg_n, string field_n, string val_n, UINT32 interPollWait, int maxPoll, bool equal = true, UINT32 baseAddr = 0);
    int regPoll(string unit, string reg_n, vector<pair<string,string>> field_val_pairs, UINT32 interPollWait, int maxPoll, bool equal = true, UINT32 baseAddr = 0);
    int regPoll(string unit, string reg_n, string field_n, UINT32 val, UINT32 interPollWait, int maxPoll, bool equal = true, UINT32 baseAddr = 0);

    // Polymorphed Write function
    int regWr ( string unit, string reg, string field, string val, UINT32 baseAddr = 0) {
        return drf_def(unit, reg, field, val, baseAddr);
    }
    int regWr ( string unit, string reg, vector<pair<string,string>> field_val_pairs, UINT32 baseAddr = 0) {
        return drf_def(unit, reg, field_val_pairs, baseAddr);
    }
    int regWr ( string unit, string reg, string field, int val, UINT32 baseAddr = 0 ) {
        return drf_num(unit, reg, field, val, baseAddr);
    }
    int regWr ( string unit, string reg, UINT32 val, UINT32 baseAddr = 0 ) {
        return drf_num(unit, reg, val, baseAddr);
    }
};

#endif // _REGMAP_HELPER_H_
