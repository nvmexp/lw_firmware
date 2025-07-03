/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015,2018, 2020-2021 by LWPU Corporation.  All rights 
 * reserved. All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "RegTestGpu.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/sysspec.h"
#include "fermi/gf100/dev_disp.h"
#include "fermi/gf100/dev_master.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/massert.h"
#include "gpu/include/gpusbdev.h"
#include "mdiag/utils/cregctrl.h"

#include <string>
using std::string;

// FIXME: // these should come from the manuals parser
// defines needed for Clock Counter Test
#define LW_PTRIM_CLK_CNTR_DISP_CFG                       0x00004800 /* RW-4R */
#define LW_PTRIM_CLK_CNTR_DISP_CFG_NOOFIPCLKS                  13:0 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_DISP_CFG_NOOFIPCLKS_INIT       0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_DISP_CFG_ENABLE                     20:20 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_DISP_CFG_ENABLE_INIT           0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_DISP_CFG_ENABLE_DEASSERTED     0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_DISP_CFG_ENABLE_ASSERTED       0x00000001 /* RW--V */
#define LW_PTRIM_CLK_CNTR_DISP_CFG_RESET                      24:24 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_DISP_CFG_RESET_INIT            0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_DISP_CFG_RESET_DEASSERTED      0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_DISP_CFG_RESET_ASSERTED        0x00000001 /* RW--V */
#define LW_PTRIM_CLK_CNTR_DISP_CFG_SOURCE                     31:28 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_DISP_CFG_SOURCE_INIT           0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_DISP_CFG_SOURCE_DISPCLK             0x00000000 /* RW--V */
#define LW_PTRIM_CLK_CNTR_DISP_CFG_SOURCE_DISPCLK_FULL_SPEED  0x00000001 /* RW--V */
#define LW_PTRIM_CLK_CNTR_DISP_CFG_SOURCE_DISPCLK_HEAD0       0x00000002 /* RW--V */
#define LW_PTRIM_CLK_CNTR_DISP_CFG_SOURCE_DISPCLK_HEAD1       0x00000003 /* RW--V */
#define LW_PTRIM_CLK_CNTR_DISP_CNT                       0x00004804 /* R--4R */
#define LW_PTRIM_CLK_CNTR_DISP_CNT_VALUE                       19:0 /* R--UF */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG                       0x00004810 /* RW-4R */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG_NOOFIPCLKS                  13:0 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG_NOOFIPCLKS_INIT       0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG_ENABLE                     20:20 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG_ENABLE_INIT           0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG_ENABLE_DEASSERTED     0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG_ENABLE_ASSERTED       0x00000001 /* RW--V */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG_RESET                      24:24 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG_RESET_INIT            0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG_RESET_DEASSERTED      0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG_RESET_ASSERTED        0x00000001 /* RW--V */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG_SOURCE                     31:28 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG_SOURCE_INIT           0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG_SOURCE_M2CLK_p0       0x00000000 /* RW--V */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG_SOURCE_M2CLK_GR_P0    0x00000001 /* RW--V */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG_SOURCE_MCLK_P0        0x00000002 /* RW--V */
#define LW_PTRIM_CLK_CNTR_MEMA_CFG_SOURCE_MCLK_GR_P0     0x00000003 /* RW--V */
#define LW_PTRIM_CLK_CNTR_MEMA_CNT                       0x00004814 /* R--4R */
#define LW_PTRIM_CLK_CNTR_MEMA_CNT_VALUE                       19:0 /* R--UF */
#define LW_PTRIM_CLK_CNTR_LW_CFG                         0x00004820 /* RW-4R */
#define LW_PTRIM_CLK_CNTR_LW_CFG_NOOFIPCLKS                    13:0 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_LW_CFG_NOOFIPCLKS_INIT         0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_LW_CFG_ENABLE                       20:20 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_LW_CFG_ENABLE_INIT             0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_LW_CFG_RESET                        24:24 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_LW_CFG_RESET_INIT              0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_LW_CFG_SOURCE                       31:28 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_LW_CFG_SOURCE_INIT             0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_LW_CNT                         0x00004824 /* R--4R */
#define LW_PTRIM_CLK_CNTR_LW_CNT_VALUE                         19:0 /* R--UF */
#define LW_PTRIM_CLK_CNTR_PEX_CFG                        0x00004830 /* RW-4R */
#define LW_PTRIM_CLK_CNTR_PEX_CFG_NOOFIPCLKS                   13:0 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_PEX_CFG_NOOFIPCLKS_INIT        0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_PEX_CFG_ENABLE                      20:20 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_PEX_CFG_ENABLE_INIT            0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_PEX_CFG_ENABLE_DEASSERTED      0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_PEX_CFG_ENABLE_ASSERTED        0x00000001 /* RW--V */
#define LW_PTRIM_CLK_CNTR_PEX_CFG_RESET                       24:24 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_PEX_CFG_RESET_INIT             0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_PEX_CFG_RESET_DEASSERTED       0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_PEX_CFG_RESET_ASSERTED         0x00000001 /* RW--V */
#define LW_PTRIM_CLK_CNTR_PEX_CFG_SOURCE                      31:28 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_PEX_CFG_SOURCE_INIT            0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_PEX_CFG_SOURCE_HOSTCLK             0x00000000 /* RW--V */
#define LW_PTRIM_CLK_CNTR_PEX_CFG_SOURCE_HOSTCLK_FULL_SPEED  0x00000001 /* RW--V */
#define LW_PTRIM_CLK_CNTR_PEX_CFG_SOURCE_XCLK                0x00000002 /* RW--V */
#define LW_PTRIM_CLK_CNTR_PEX_CFG_SOURCE_PEX_XTXCLK1X        0x00000003 /* RW--V */
#define LW_PTRIM_CLK_CNTR_PEX_CNT                        0x00004834 /* R--4R */
#define LW_PTRIM_CLK_CNTR_PEX_CNT_VALUE                        19:0 /* R--UF */
#define LW_PTRIM_CLK_CNTR_VP2_CFG                        0x00004840 /* RW-4R */
#define LW_PTRIM_CLK_CNTR_VP2_CFG_NOOFIPCLKS                   13:0 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_VP2_CFG_NOOFIPCLKS_INIT        0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_VP2_CFG_ENABLE                      20:20 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_VP2_CFG_ENABLE_INIT            0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_VP2_CFG_RESET                       24:24 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_VP2_CFG_RESET_INIT             0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_VP2_CFG_SOURCE                      31:28 /* RWIUF */
#define LW_PTRIM_CLK_CNTR_VP2_CFG_SOURCE_INIT            0x00000000 /* RWI-V */
#define LW_PTRIM_CLK_CNTR_VP2_CNT                        0x00004844 /* R--4R */
#define LW_PTRIM_CLK_CNTR_VP2_CNT_VALUE                        19:0 /* R--UF */

extern const ParamDecl RegTestGpu_params[] = {

    {"-group", "t", ParamDecl::PARAM_MULTI_OK, 0, 0,"name of register group to test"},

    {"-range", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "specify an address range to restrict testing (start:stop)"},

    SIMPLE_PARAM("-A5", "write As and 5s for each register"),
    SIMPLE_PARAM("-walk1", "do walking 1 test for each register"),
    SIMPLE_PARAM("-walk0", "do walking 0 test for each register"),

    SIMPLE_PARAM("-hwInitChk", "read each register and check against HW init value"),
    SIMPLE_PARAM("-bootInitOnly", "only take GPU through boot init, skip all other setup"),
    SIMPLE_PARAM("-clkCntrChk", "Do a few checks on the clock counters"),
    SIMPLE_PARAM("-check_init_values", "check initial values, if any"),
    SIMPLE_PARAM("-check_actual_init_values", "check actual HW init values and takes PROD settings into consideration, if any"),
    SIMPLE_PARAM("-only_init_values", "don't check more than the initial values"),
    SIMPLE_PARAM("-only_prod_values", "don't check more than the __PROD values"),

    SIMPLE_PARAM("-random", "write a random value to each register"),
    UNSIGNED_PARAM("-seed1", "Random stream seed for write of random value"),
    UNSIGNED_PARAM("-seed2", "Random stream seed for write of random value"),
    UNSIGNED_PARAM("-seed3", "Random stream seed for write of random value"),
    UNSIGNED_PARAM("-random_count", "Number of random writes to generate for each register"),

    SIMPLE_PARAM("-verbose", "print out info as each register is written"),
    SIMPLE_PARAM("-ptvo_special", "run PTVO_SPECIAL test for indirect PTVO registers"),
    SIMPLE_PARAM("-tmds_special", "run TMDS_SPECIAL test for indirect TMDS registers"),

    {"-regFileTest", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "run a test using the register manipulation input file"},

    UNSIGNED_PARAM("-print", "print out the register map (0=addr only, 1=full layout)"),
    UNSIGNED_PARAM("-printClass", "print out the given class"),
    UNSIGNED_PARAM("-printArrayLwtoff", "print out the register arrays to max index"),

    SIMPLE_PARAM("-printExceptions", "print out all the exceptions as read in from files"),

    SIMPLE_PARAM("-rma", "use RMA mode"),
    SIMPLE_PARAM("-xve", "add LW_PCFG offset for xve registers"),
    SIMPLE_PARAM("-ixve_c11", "add LW_PCFG offset for ixve_c11 registers"),
    SIMPLE_PARAM("-xve1", "add 0x8a000 offset for xve1 registers"),

    SIMPLE_PARAM("-trim_bug", "fix for trim register write bug"),

    {"-skipFile", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "supply a filename of registers to skip"},
    {"-skipfile", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "supply a filename of registers to skip"},

    {"-ArrayRange1", "uu", 0, 0, 0, "Test range limits for first  index of register arrays (-ArrayRange1 <lowest index> <highest index>)"},
    {"-ArrayRange2", "uu", 0, 0, 0, "Test range limits for second index of register arrays (-ArrayRange2 <lowest index> <highest index>)"},

    {"-fbTestRange", "uu", 0, 0, 0, "enter begin and end FB address to test (as words, -fbTestRange 1 100)"},

    // Special case of allowing "swallowed" options since this test is used for ISO/display testing.
    // These two options are used for display testing but should be ignored here since no surface is created.
    // This was authorized by bmartinusen and, for ISO, it is much preferred over maintaining parallel levels files
    // that only differ by these higher level "mode" options. ISO already has many dimensions to it's levels structure.
    // Bug 212871  fjohnson
    SIMPLE_PARAM("-pitch", "unused ISO option; allowed but discarded"),
    SIMPLE_PARAM("-dumpImages", "unused ISO option; allowed but discarded"),
    SIMPLE_PARAM("-enable_rtl_monitors", "unused rtl option; allowed but discarded"),

    PARAM_SUBDECL(lwgpu_single_params),

    LAST_PARAM
};

CRegTestGpu::CRegTestGpu(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    m_printLevel = 0;
    m_printClass = -1;
    m_printArrayLwtoff = 20;
}

CRegTestGpu::~CRegTestGpu(void)
{
}

STD_TEST_FACTORY(RegTestGpu,CRegTestGpu)

#if 0
Test *CRegTestGpu::Factory(ArgDatabase *args)
{
    ArgReader *reader;

    reader = new ArgReader(RegTestGpuParams);
    if ( reader == NULL )
    {
        ErrPrintf("CRegTestGpu::CRegTestGpu: ArgReader create failed\n");
        return 0;
    }

    if( !reader->ParseArgs(args) )
    {
        ErrPrintf("RegTestGpu: error parsing args!\n");
        return 0;
    }

    return(new CRegTestGpu(reader));
}

#endif

UINT32 CRegTestGpu::ProcessExceptionFile(const char* skipFile)
{
    if(!skipFile)
        return 0;

    FileIO *fh = Sys::GetFileIO(skipFile, "r");
    if (fh == NULL)
    {
        ErrPrintf("can't open skip file %s , fh= %p\n", skipFile, fh);
        getStateReport()->setupFailed("Setup: can't open skip file");
        return 1;
    }

    char line[8192];
    int  lcnt = 0;
    while(fh->FileGetStr(line, sizeof(line)-1))
    {
        lcnt ++;

        if (m_verbose)
            DebugPrintf ("ProcessExceptionFile %s:%d raw line = %s", skipFile, lcnt, line);

        // remove comments and whitespace
        if (char *p = strchr (line, '#'))
            *p = 0;
        while (char *p = strpbrk (line, " \t\r\n"))
            for (; *p; p++)
                *p = *(p+1);
        if (m_verbose)
            DebugPrintf ("ProcessExceptionFile %s:%d stripped line = %s\n", skipFile, lcnt, line);

        // skip empty lines
        if (!*line)
            continue;
        string orig_line = line;

        // check if should attempt to parse a register array line
        if(line[0] == 'A')
        {
            if(m_exclusions.MapArrayReg(line, m_regMap) == false)
            {
                ErrPrintf("Invalid register array exclusion line: %s\n", line);
                getStateReport()->setupFailed("Invalid register array exclusion line detected");
                return 2;
            }
            continue;
        }

        // parse up to 4 parts separated by ":"
        const int maxparts = 4;
        string  sparts     [maxparts]; // char version
        UINT32  uparts     [maxparts]; // unsigned version
        bool    uparts_vld [maxparts]; // true if corresponding upart is valid
        for (int i = 0; i < maxparts; i++)
        {
            // extract the parts as strings first
            char *token = NULL;
            if (i == 0)
                token = strtok(line, ":");
            else if (sparts[i-1].size())
                token = strtok(NULL, ":");
            if (token)
                sparts[i] = token;
            else
                sparts[i] = string("");

            // decode the part's hex value, if possible
            uparts_vld[i] = sscanf (sparts[i].c_str(), "%x", uparts+i) == 1;
        }

        // get easier names for the parts
        string type     = sparts [0];
        int    idxoff   = type.size() == 1 ? 1 : 0;  // for now all special ops are one char and all regs are > 1 char
        string regname  = sparts     [0+idxoff];
        string valname  = sparts     [1+idxoff];
        string fldname  = sparts     [2+idxoff];
        UINT32 &val     = uparts     [1+idxoff];
        UINT32 &msk     = uparts     [2+idxoff];
        bool   &val_vld = uparts_vld [1+idxoff];
        bool   &msk_vld = uparts_vld [2+idxoff];

        // handle array indices and ranges.
        // reg(idx) or reg(hi_idx..lo_idx) or reg(lo_idx..hi_idx)
        // only handles single-dimensional arrays for now
        // TODO: handle two-dimensional indices
        bool indexed_exclude = 0;
        size_t paren_open = regname.find("(", 0);
        if (paren_open != string::npos)
        {
            // parse the index (or range)
            size_t paren_close = regname.find(")", paren_open);
            if (paren_close != (regname.size() - 1))
            {
                ErrPrintf("ProcessExceptionFile %s:%d Unrecognized register specification: %s: %s\n", skipFile, lcnt, regname.c_str(), orig_line.c_str());
                return 2;
            }
            string       idx_str = regname.substr (paren_open+1, paren_close - paren_open - 1);
            string       idx1_str = "", idx2_str = "";
            unsigned int idx1, idx2;
            size_t sep_pos = idx_str.find("..",0);
            if (sep_pos == string::npos)
            {
                idx1_str = idx2_str = idx_str;
            }
            else
            {
                idx1_str = idx_str.substr(0        , sep_pos);
                idx2_str = idx_str.substr(sep_pos+2, idx_str.size()-sep_pos-2+1);
            }
            if (sscanf (idx1_str.c_str(), "%i", &idx1) != 1)
            {
                ErrPrintf("ProcessExceptionFile %s:%d Unrecognized register index specification: %s: %s\n", skipFile, lcnt, regname.c_str(), orig_line.c_str());
                return 2;
            }
            if (sscanf (idx2_str.c_str(), "%i", &idx2) != 1)
            {
                ErrPrintf("ProcessExceptionFile %s:%d Unrecognized register index specification: %s: %s\n", skipFile, lcnt, regname.c_str(), orig_line.c_str());
                return 2;
            }
            if (idx1 > idx2) {
                unsigned int tmp = idx2;
                idx2 = idx1;
                idx1 = tmp;
            }

            // restore the real register name
            regname = regname.substr (0, paren_open);

            // mark each address of the range to be skipped
            for (unsigned int idx = idx1; idx <= idx2; idx++) {
                m_skipidxs[regname][idx] = 1;
            }
            indexed_exclude = 1;
        }

        // the register must exist (and be specified symbolically)
        // lookup the register
        unique_ptr<IRegisterClass> reg = m_regMap->FindRegister(regname.c_str());
        if(!reg)
        {
            WarnPrintf("ProcessExceptionFile %s:%d register %s not found\n", skipFile, lcnt, regname.c_str());
            continue;
        }
        m_skipRegsAll[regname] = 1;

        // if the field part is given, concatenate it onto the register name
        // if defined, use it
        unique_ptr<IRegisterField> fld;
        if (valname.size() && !fldname.size())
        {
            // handle records like "E:reg:fieldname"
            vector <string> trylist;
            trylist.push_back(                valname);  // try the raw field name first
            trylist.push_back(regname + "_" + valname);  // try using the value name (needed for "E:" record)
            trylist.push_back(regname +       valname);  // try again w/o "_"
            for (unsigned i = 0; i < trylist.size(); i++)
                if ((fld = reg->FindField (trylist[i].c_str())))
                {
                    msk     = fld->GetMask();
                    msk_vld = 1;
                    valname = "";  // just a mask
                    val_vld = 0;
                    break;
                }

            // handle records like "E:reg:mask"
            // WARNING: this needs to handle "I:reg:value" records.  In this case,
            // msk and val must not be swapped like they are for E records since
            // val is correct as is.  See bug 463829.
            if (!msk_vld && val_vld && (type != "I"))
            {
                msk     = val;
                msk_vld = 1;
                val_vld = 0;
            }
        }
        else if (fldname.size())
        {
            vector <string> trylist;
            trylist.push_back(                fldname);  // try the raw field name first
            trylist.push_back(regname + "_" + fldname);  // try prepending the register name with "_"
            trylist.push_back(regname +       fldname);  // try prepending the register name without "_"
            for (unsigned i = 0; i < trylist.size(); i++)
                if ((fld = reg->FindField (trylist[i].c_str())))
                {
                    msk     = fld->GetMask();
                    msk_vld = 1;
                    break;
                }
        }

        // if the value part is given, concatenate it onto the field name
        // if defined, use it
        unique_ptr<IRegisterValue> r_val;
        if (valname.size() && fld)
        {
            vector <string> trylist;
            trylist.push_back(                                valname);  // try the raw value name first
            trylist.push_back(regname + "_" + fldname + "_" + valname);  // try prepending the register and field names with "_"
            trylist.push_back(regname +       fldname +       valname);  // try prepending the register and field names without "_"
            for (unsigned i = 0; i < trylist.size(); i++)
                if ((r_val = fld->FindValue (trylist[i].c_str())))
                {
                    val     = r_val->GetValue() << fld->GetStartBit();
                    val_vld = 1;
                    break;
                }
        }

        // parse the fields
        if (type == "C")  // constant definition
        {
            if (val_vld && msk_vld)
            {
                if(m_constBits.find(regname) == m_constBits.end())
                    m_constBits[regname] = new CConstVal(0, 0);
                *m_constBits[regname] |= CConstVal(val, msk);
                if (m_verbose)
                    InfoPrintf("ProcessExceptionFile Constant %s:0x%x:0x%x\n", regname.c_str(), val, msk);
                continue;
            }
            else
            {
                getStateReport()->setupFailed("Unrecognized constant specification");
                ErrPrintf("ProcessExceptionFile %s:%d Unrecognized constant specification %s: %s\n", skipFile, lcnt, regname.c_str(), orig_line.c_str());
                return 2;
            }
        }

        if (type == "F")  // field value force definition
        {
            if (val_vld && msk_vld)
            {
                if(m_forceBits.find(regname) == m_forceBits.end())
                    m_forceBits[regname] = new CConstVal(0, 0);
                *m_forceBits[regname] |= CConstVal(val, msk);
                if (m_verbose)
                    InfoPrintf("ProcessExceptionFile Force %s:0x%x:0x%x\n", regname.c_str(), val, msk);
                continue;
            }
            else
            {
                getStateReport()->setupFailed("Unrecognized constant specification");
                ErrPrintf("ProcessExceptionFile %s:%d Unrecognized constant specification %s: %s\n", skipFile, lcnt, regname.c_str(), orig_line.c_str());
                return 2;
            }
        }

        if (type == "E")  // exclusion definition
        {
            if (msk_vld & !val_vld)
            {
                if(m_excludeBits.find(regname) == m_excludeBits.end())
                    m_excludeBits[regname] = 0;
                m_excludeBits[regname] |= msk;
                if (m_verbose)
                    InfoPrintf("ProcessExceptionFile E Exception (mask) %s:0x%x\n", regname.c_str(), msk);
                continue;
            }
            else
            {
                getStateReport()->setupFailed("Unrecognized excluded field specification");
                ErrPrintf("ProcessExceptionFile %s:%d Unrecognized excluded field specification for %s: %s\n", skipFile, lcnt, regname.c_str(), orig_line.c_str());
                return 2;
            }
        }

        if (type == "T")  // tolerance definition
        {
            if (msk_vld & !val_vld)
            {
                m_Tolerances[regname] = msk;
                if (m_verbose)
                    InfoPrintf("ProcessExceptionFile Tolerance %s:0x%x\n", regname.c_str(), msk);
                continue;
            }
            else
            {
                getStateReport()->setupFailed("Unrecognized tolerance specification");
                ErrPrintf("ProcessExceptionFile %s:%d Unrecognized tolerance specification for %s: %s\n", skipFile, lcnt, regname.c_str(), orig_line.c_str());
                return 2;
            }

        }

        if (type == "I")  // initialization definition
        {
            // default mask
            if (!msk_vld) {
                msk = 0xffffffff;
                msk_vld = 1;
            }

            if (val_vld)
            {
                if(m_InitialVals.find(regname) == m_InitialVals.end())
                    m_InitialVals[regname] = 0;
                m_InitialVals[regname] &= ~msk;
                m_InitialVals[regname] |=  msk & val;
                if (m_verbose)
                    InfoPrintf("ProcessExceptionFile Initialization %s:0x%x\n", regname.c_str(), val);
                continue;
            }
            else
            {
                getStateReport()->setupFailed("Unrecognized initialization field specification");
                ErrPrintf("ProcessExceptionFile %s:%d Unrecognized initialization field specification for %s: %s\n", skipFile, lcnt, regname.c_str(), orig_line.c_str());
                return 2;
            }
        }

        if (type == "O")  // initial value override definition
        {
            if (msk_vld & val_vld)
            {
                if (m_InitialOverride.find(regname) == m_InitialOverride.end())
                    m_InitialOverride[regname] = new CConstVal(0, 0);
                *m_InitialOverride[regname] |= CConstVal(val, msk);
                if (m_verbose)
                    InfoPrintf( "ProcessExceptionFile O Exception (override) %s:0x%x:0x%x\n", regname.c_str(), val, msk);
                continue;
            }
            else
            {
                getStateReport()->setupFailed( "Unrecognized initialization field specification" );
                ErrPrintf( "ProcessExceptionFile %s:%d Unrecognized initialization field specification for %s: %s\n", skipFile, lcnt, regname.c_str(), orig_line.c_str());
                return 2;
            }
        }

        // exclude an entire register
        if (!indexed_exclude)
            m_skipRegs[regname] = 1;
    }

    fh->FileClose();
    Sys::ReleaseFileIO(fh);

    return 0;
}

int CRegTestGpu::Setup(void)
{
    unique_ptr<IRegisterClass> reg;

    getStateReport()->enable();

    lwgpu = LWGpuResource::FindFirstResource();
    if(!lwgpu)
    {
        getStateReport()->setupFailed("Setup: Couldn't acquire a GPU!\n");
        return 0;
    }

    m_only_prod = m_pParams->ParamPresent("-only_prod_values");

    // if(m_pParams->ParamPresent("-check_init_values")) ResetDisplay();
    if(!m_only_prod) ResetDisplay();

    if (m_pParams->ParamPresent("-bootInitOnly"))
        MASSERT(!"-bootInitOnly not supported");

    if (m_pParams->ParamPresent("-pitch"))
        InfoPrintf("NOTE from RegTestGpu: the -pitch option will be ignored.\n");

    if (m_pParams->ParamPresent("-dumpImages"))
        InfoPrintf("NOTE from RegTestGpu: the -dumpImages option will be ignored.\n");

    if (m_pParams->ParamPresent("-enable_rtl_monitors"))
        InfoPrintf("NOTE from RegTestGpu: the -enable_rtl_monitors option will be ignored.\n");

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("RegTestGpu::Setup failed to create channel\n");
        return 0;
    }

    m_regGroups = m_pParams->ParamStrList("-group", NULL);

    if(m_pParams->ParamPresent("-range"))
    {
        const vector<string>* rangeList = m_pParams->ParamStrList("-range", NULL);
        if ( rangeList && !rangeList->empty() )
        {
            auto rangeListLwr = rangeList->begin();
            while ( rangeListLwr != rangeList->end() )
            {
                char * tmp = strdup(rangeListLwr->c_str());
                char *token = strtok(tmp, ":\n\r");
                char *token2 = strtok(NULL, ":\n\r");

                if(token && token2)
                {
                    UINT32 r1,r2;
                    r1 = strtol(token, NULL, 0);
                    r2 = strtol(token2, NULL, 0);

                    m_regRanges.push_back(new CRange(r1, r2));
                }
                else
                {
                    WarnPrintf("Bad range specified, not in start:stop format (%s)\n", rangeListLwr->c_str());
                }

                rangeListLwr++;
            }
        }
    }

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("CRegTestGpu::Setup: can't get a register map from the GPU!\n");
        getStateReport()->setupFailed("Setup: can't get a register map from the GPU!\n");
        return 0;
    }

    m_seed1 = m_pParams->ParamUnsigned("-seed1", 0x1111);
    m_seed2 = m_pParams->ParamUnsigned("-seed2", 0x2222);
    m_seed3 = m_pParams->ParamUnsigned("-seed3", 0x3333);
    m_rndStream = new RandomStream(m_seed1, m_seed2, m_seed3);

    m_verbose = m_pParams->ParamPresent("-verbose");

    // allow multiple skip files
    // allow both skipfile argument names -- klunky but needed for backward compatibility
    bool skip_seen = false;
    for (int t=0; t<2; t++) {
        const char *skip_argname = t ? "-skipfile" : "-skipFile";
        const vector<string>* skipFileLines = m_pParams->ParamStrList(skip_argname, NULL);
        if ( skipFileLines && !skipFileLines->empty() )
        {
            skip_seen = true;
            auto skipFileLwr = skipFileLines->begin();
            while ( skipFileLwr != skipFileLines->end() )
            {
                if(ProcessExceptionFile((const char*)skipFileLwr->c_str()))
                    return 0;

                skipFileLwr++;
            }
        }
    }
    if (skip_seen && m_pParams->ParamPresent("-printExceptions"))
        PrintExceptions();

    if(m_pParams->ParamPresent("-xve") || m_pParams->ParamPresent("-ixve_c11"))
    {
        AddOffset = true;
        RefManual *pRefManual = lwgpu->GetGpuSubdevice()->GetRefManual();
        const RefManualRegisterGroup *pPcfgGroup =
            pRefManual->FindGroup("LW_PCFG");
        if (pPcfgGroup != NULL)
        {
            a_offset = pPcfgGroup->GetRangeLo();
            InfoPrintf("Found -xve or -ixve_c11 param"
                       " - will add offset 0x%08x to addresses\n",
                       a_offset);
        }
        else
        {
            a_offset = 0x0;
            ErrPrintf("Don't know set offset for XVE or IXVE regs, setting to 0\n");
        }
    }
    else if(m_pParams->ParamPresent("-xve1"))
    {
        InfoPrintf("Found -xve1 param - will add 0x8a000 offset to addresses\n");
        AddOffset = true;
        a_offset = 0x08a000;
    }
    else
    {
        DebugPrintf("Did not find -xve or -xve1 or -ixve_c11 param\n");
        AddOffset = false;
        a_offset = 0x0;
    }

    if(m_pParams->ParamPresent("-rma"))
    {
        GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

        UINT32 arch = lwgpu->GetArchitecture();
        if (!(reg = m_regMap->FindRegister("LW_PBUS_PCI_LW_5")))
        {
            ErrPrintf("CRegTestGpu::Setup: Can't find register: LW_PBUS_PCI_LW_5\n");
            getStateReport()->setupFailed("Setup: ");
            return 0;
        }
        pSubdev->RegWr32(reg->GetAddress(),pSubdev->RegRd32(reg->GetAddress())|0x1);

        if (!(reg = m_regMap->FindRegister("LW_PBUS_PCI_LW_21")))
        {
            ErrPrintf("CRegTestGpu::Setup: Can't find register: LW_PBUS_PCI_LW_21\n");
            getStateReport()->setupFailed("Setup: ");
            return 0;
        }
        pSubdev->RegWr32(reg->GetAddress(),pSubdev->RegRd32(reg->GetAddress())|0x1);

        InfoPrintf("Turn on i/o access at 3c3 for RMA testing...");
        if (!(reg = m_regMap->FindRegister("LW_VIO_VSE2")))
        {
            ErrPrintf("CRegTestGpu::Setup: Can't find register: LW_VIO_VSE2\n");
            getStateReport()->setupFailed("Setup: ");
            return 0;
        }
        Platform::PioWrite08(reg->GetAddress(), 1);  // turn on i/o accesses at 3c3
        Platform::PioWrite08(LW_VIO_MISC__WRITE, 3); // allow host access  to display ram and

        Platform::PioWrite08(0x3c2, 0xe3);

        InfoPrintf("Unlocking the extended VGA registers for RMA testing...\n");
        if (arch == 0x50)
        {
            reg = m_regMap->FindRegister("LW_CIO_CRX__COLOR");
            pSubdev->RegWr08(reg->GetAddress(),LW_CIO_CRE_LOCK__INDEX);

            reg = m_regMap->FindRegister("LW_CIO_CR_HDT__COLOR");
            pSubdev->RegWr08(reg->GetAddress(),LW_CIO_CRE_LOCK_EXT_REG_ACCESS_UNLOCK_RW );

        }
        else
        {
            reg = m_regMap->FindRegister("LW_PRMCIO_CRX__COLOR");
            pSubdev->RegWr08(reg->GetAddress(), 0x1f);

            reg = m_regMap->FindRegister("LW_PRMCIO_CR__COLOR");
            pSubdev->RegWr08(reg->GetAddress(), 0x57);

        }

        // FIXME: This should be in the skip file
        m_skipRegs["LW_PBUS_PCI_LW_5"] = 1;     // add this reg to the skip list for RMA mode
        m_skipRegs["LW_PBUS_PCI_LW_21"] = 1;    // add this reg to the skip list for RMA mode
        m_skipRegs["LW_PMC_ENABLE"] = 1;                // add this reg to the skip list for RMA mode
        m_skipRegs["LW_PBUS_POWERCTRL_2"] = 1;          // add this reg to the skip list for RMA mode
        m_skipRegs["LW_PEXTDEV_BOOT_3"] = 1;    // add this reg to the skip list for RMA mode
        m_skipRegs["LW_PEXTDEV_BOOT_4"] = 1;    // add this reg to the skip list for RMA mode
        m_skipRegs["LW_PFIFO_CACHE1_DMA_CTL"] = 1; // add this reg to the skip list for RMA mode
        m_skipRegs["LW_PBUS_THERMALCTRL_0"] = 1;   // add this reg to the skip list for RMA mode
    }

    if(m_pParams->ParamPresent("-ArrayRange1"))
    {
        m_ArrayRange1 = new CRange(m_pParams->ParamNUnsigned("-ArrayRange1", 0),
                                   m_pParams->ParamNUnsigned("-ArrayRange1", 1)
                                  );
    }
    else
    {
        m_ArrayRange1 = NULL;
    }

    if(m_pParams->ParamPresent("-ArrayRange2"))
    {
        m_ArrayRange2 = new CRange(m_pParams->ParamNUnsigned("-ArrayRange2", 0),
                                   m_pParams->ParamNUnsigned("-ArrayRange2", 1)
                                  );
    }
    else
    {
        m_ArrayRange2 = NULL;
    }

    if(m_pParams->ParamPresent("-fbTestRange"))
    {
        m_fbTest = true;
        m_fbTestBegin = m_pParams->ParamNUnsigned("-fbTestRange", 0);
        m_fbTestEnd = m_pParams->ParamNUnsigned("-fbTestRange", 1);
    }
    else
    {
        m_fbTest = false;
    }

    return 1;
}

void CRegTestGpu::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("CRegTestGpu::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }

    if(m_rndStream)
    {
        delete m_rndStream;
        m_rndStream = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

const char* CRegTestGpu::PassGroupCheck(IRegisterClass* reg, bool allowNoGroups)
{
    const char* passchk = NULL;

    if ( m_regGroups && !m_regGroups->empty() )
    {
        string regName;
        auto regGroupsLwr = m_regGroups->begin();

        while ( regGroupsLwr != m_regGroups->end() )
        {
            regName = "LW_";
            regName += regGroupsLwr->c_str();
            regName += "_";

            if(strncmp(reg->GetName(), regName.c_str(), regName.size()) == 0)
            {
                passchk = regGroupsLwr->c_str();
                regName.erase();
                break;
            }

            regGroupsLwr++;
            regName.erase();
        }
    }
    else if(allowNoGroups)
    {
        passchk = "NO GROUP";
    }

    return passchk;
}

void CRegTestGpu::Run(void)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterClass> status_reg;

    UINT32 regCnt = 0;
    UINT32 failCnt = 0;
    UINT32 skipCnt = 0;
    UINT32 runStatus = 0;
    UINT32 fbCnt = 0;
    UINT32 fbFail = 0;
    UINT32 dims;
    UINT32 limit1, vsize1;
    UINT32 limit2, vsize2;
    UINT32 tmpVal = 0;

    bool useFalconExtendedAddr = 0;

    if(m_pParams->ParamPresent("-print"))
    {
        m_printLevel = m_pParams->ParamUnsigned("-print", 0);
        if(m_pParams->ParamPresent("-printClass"))
            m_printClass = m_pParams->ParamUnsigned("-printClass", 0);
        m_printArrayLwtoff = m_pParams->ParamUnsigned("-printArrayLwtoff", 20);
        PrintRegisterMap();
        SetStatus(TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
        return;
    }

    if(m_pParams->ParamPresent("-hwInitChk"))
    {
        InfoPrintf("Begin check of HW init values\n");
        runStatus |= DoHwInitTest();

        if(runStatus)
        {
            SetStatus(Test::TEST_FAILED_CRC);
            getStateReport()->runFailed("run failed, see errors/stats printed!\n");
        }
        else
        {
            SetStatus(TEST_SUCCEEDED);
            getStateReport()->crcCheckPassedGold();
        }

        return;
    }

    if(m_pParams->ParamPresent("-clkCntrChk"))
    {
        InfoPrintf("Begin check of Clock Counters \n") ;
        runStatus |= DoClkCntrTest();

        if(runStatus)
        {
            SetStatus(TEST_FAILED_CRC);
            getStateReport()->runFailed("run failed, see errors/stats printed!\n");
        }
        else
        {
            SetStatus(TEST_SUCCEEDED);
            getStateReport()->crcCheckPassedGold();
        }

        return;
    }

    // perform register initializations before starting the tests
    for (map<string, UINT32>::iterator initValIter = m_InitialVals.begin();
         initValIter != m_InitialVals.end();
         initValIter++)
    {
        UINT32 initVal = initValIter->second;
        unique_ptr<IRegisterClass> reg = m_regMap->FindRegister(initValIter->first.c_str());
        if (!reg) continue;  // shouldn't be any way for this to be undefined

        // Hack: ignore xve offset for registers without XVE in their names
        // Note that this will fail if someone starts using the "I:" exception for an register w/ LW_XVE in the name that doesn't use the offset
        UINT32 xve_offset = strstr(reg->GetName(), "LW_XVE") ? a_offset : 0;
        UINT32 ixve_c11_offset = strstr(reg->GetName(), "LW_IXVE_C11") ? a_offset : 0;

        reg->GetFormula2(limit1, limit2, vsize1, vsize2);
        for (UINT32 idim = 0; idim < limit1; idim++)
            for (UINT32 jdim = 0; jdim < limit2; jdim++)
            {
                UINT32 theAddr = reg->GetAddress() + xve_offset + ixve_c11_offset + (idim * vsize1) + (jdim * vsize2);
                InfoPrintf("CRegTestGpu::Run Initializing Register 0x%08x->0x%08x  %s\n", theAddr, initVal, reg->GetName());
                LocalRegWr32(theAddr, initVal);
            }
    }

    // run all the input file register tests now
    if(m_pParams->ParamPresent("-regFileTest"))
    {
        string testFile;

        const vector<string>* regTestLines = m_pParams->ParamStrList("-regFileTest", NULL);
        if ( regTestLines && !regTestLines->empty() )
        {
           auto regTestLwr = regTestLines->begin();

            while ( regTestLwr != regTestLines->end() )
            {
                testFile = "F:";
                testFile += regTestLwr->c_str();
                vector<CRegisterControl> regFileTest;
                if(lwgpu->ProcessOptionalRegisters(&regFileTest, testFile.c_str()))
                {
                    SetStatus(Test::TEST_FAILED_CRC);
                    getStateReport()->runFailed("error processing input register test file!\n");
                    return;
                }

                if(!regFileTest.empty())
                    lwgpu->HandleOptionalRegisters(&regFileTest);

                regTestLwr++;
            }
        }
    }

    // this access is added to make sure the pipe is idle before testing other registers
    unique_ptr<IRegisterClass> PmcEnableReg = m_regMap->FindRegister("LW_PMC_ENABLE");
    MASSERT(PmcEnableReg != 0);
    const UINT32 PmcEnable = LocalRegRd32(PmcEnableReg->GetAddress());
    if (DRF_VAL(_PMC, _ENABLE, _PGRAPH, PmcEnable) == LW_PMC_ENABLE_PGRAPH_ENABLED)
    {
        status_reg = m_regMap->FindRegister("LW_PGRAPH_STATUS");
        tmpVal = LocalRegRd32(status_reg->GetAddress());
        tmpVal = tmpVal & 0x00000001;
        while(tmpVal)
        {
            tmpVal = LocalRegRd32(status_reg->GetAddress());
            tmpVal = tmpVal & 0x00000001;
            Platform::ClockSimulator(64);
        }
    }

    // This will allow PFIFO_DMA to be tested by disableing caches_reassign
    // FIXME: this can (and should) be done in the skip file now
    if ((reg = m_regMap->FindRegister("LW_PFIFO_CACHES")) && !m_only_prod)
        lwgpu->RegWr32(reg->GetAddress(),0);

    unique_ptr<IRegisterClass> PpwrPriAddrReg = m_regMap->FindRegister("LW_PPWR_PRI_ADDR");
    bool FALCON_3_0 = (PpwrPriAddrReg != 0);

    if (m_verbose && FALCON_3_0)
        InfoPrintf ("This is falcon 3.0. We are eventually going to use falcon extended register to access few of the pwr registers.\n");

    for (reg = m_regMap->GetRegisterHead(); reg; reg = m_regMap->GetRegisterNext())
    {
        bool doskip = 0;

        const char* grpName;

        // only process registers from the selected group (however group name is not required to be present if checking just PROD values as we need only 1 test for prod check)
        if (m_only_prod)
            grpName = PassGroupCheck(reg.get(), true);
        else
            grpName = PassGroupCheck(reg.get(), false);
        if (!grpName)
            continue;

        const char * sReg = reg->GetName();
        if (FALCON_3_0 &&
            ((!strcmp(sReg, "LW_PPWR_PMU_ELPG_IDLEFILTH_2")) ||
             (!strcmp(sReg, "LW_PPWR_PMU_ELPG_PPUIDLEFILTH_2")) ||
             (!strcmp(sReg, "LW_PPWR_PMU_ELPG_ZONELUT_2")) ||
             (!strcmp(sReg, "LW_PPWR_PMU_ELPG_DELINT_20")) ||
             (!strcmp(sReg, "LW_PPWR_PMU_ELPG_DELINT_21")) ||
             (!strcmp(sReg, "LW_PPWR_PMU_ELPG_PSW_MASK2")) ||
             (!strcmp(sReg, "LW_PPWR_PMU_ELPG_SWCTRL2")) ||
             (!strcmp(sReg, "LW_PPWR_PMU_ELPG_PSORDER_20")) ||
             (!strcmp(sReg, "LW_PPWR_PMU_ELPG_PSORDER_21")) ||
             (!strcmp(sReg, "LW_PPWR_PMU_ELPG_PDELAY2")) ||
             (!strcmp(sReg, "LW_PPWR_PMU_ELPG_FORCE_VICPT_OFF")) ||
             (!strcmp(sReg, "LW_PPWR_PMU_ELPG_IDLE_CNT2"))))
         {
             useFalconExtendedAddr = 1;
             if (m_verbose)
                 InfoPrintf ("Regsiter %s lie in the Falcon Extended Address Space\n", sReg);
        }
        else
             useFalconExtendedAddr = 0;

        if (m_skipRegs.find(reg->GetName()) != m_skipRegs.end())
        {
            InfoPrintf("Register %s being skipped because it's listed in the skip file(s)\n", reg->GetName());
            doskip=1;
        }

        if (!doskip && !reg->isReadable() && !m_skipRegsAll[reg->GetName()]) {
            InfoPrintf("Register %s being skipped because the manual says it's unreadable\n", reg->GetName());
            doskip=1;
        }

        // these tests all read registers before they issue writes
        // therefor, registers which return x's on the first reasd must be avoided to prevent causing problems in the pcie bfm
        for (unique_ptr<IRegisterField> field = reg->GetFieldHead(); !doskip && field; field = reg->GetFieldNext())
            if (field->isUnknown()) {
                if (m_skipRegsAll[reg->GetName()])
                {
                    InfoPrintf("Register %s with unknown(X) fields not being skipped due to presence of skipfile entries\n", reg->GetName());
                }
                else
                {
                    InfoPrintf("Register %s being skipped because it has unknown(X) fields\n", reg->GetName());
                    doskip = 1;
                }
                break;
            }

        // get register array limits
        dims = reg->GetArrayDimensions();
        reg->GetFormula2(limit1, limit2, vsize1, vsize2);
        if (!doskip)
            DebugPrintf("register %s, dims=%d, limit1=%d, limit2=%d, vsize1=%d vsize2=%d\n", reg->GetName(),dims,limit1,limit2,vsize1,vsize2);

        for (UINT32 idim = 0; idim < limit1; idim++)
        {
            for (UINT32 jdim = 0; jdim < limit2; jdim++)
            {
                ++regCnt;

                // above skips defered to here to allow the counts to work
                if (doskip) {
                    ++skipCnt;
                    continue;
                }

                if(m_exclusions.SkipArrayReg(reg->GetName(), idim, jdim) == true)
                {
                    ++skipCnt;
                    continue;
                }

                if(dims == 0)
                    DebugPrintf("Found register in group %s: %s\n", grpName, reg->GetName());
                else if(dims == 1)
                    DebugPrintf("Found register in group %s: %s(%d)\n", grpName, reg->GetName(), idim);
                else if(dims == 2)
                    DebugPrintf("Found register in group %s: %s(%d,%d)\n", grpName, reg->GetName(), idim, jdim);

                // see if the outer index is listed in the skip file(s)
                if (m_skipidxs                .find(reg->GetName()) != m_skipidxs                .end() &&
                    m_skipidxs[reg->GetName()].find(idim)           != m_skipidxs[reg->GetName()].end())
                {
                    if(dims <= 1)
                        InfoPrintf("Register %s(%d) being skipped because the index is listed in the skipfile(s)\n", reg->GetName(), idim);
                    else
                        InfoPrintf("Register %s(%d,%d) being skipped because the first index is listed in the skipfile(s)\n", reg->GetName(), idim, jdim);
                    ++skipCnt;
                    continue;
                }

                // apply index ranges if given
                if ((dims     && m_ArrayRange1 && !m_ArrayRange1->InRangeInclusive(idim)) ||
                    (dims > 1 && m_ArrayRange2 && !m_ArrayRange2->InRangeInclusive(jdim)))
                {
                    if(m_verbose)
                    {
                        if (dims > 1)
                            InfoPrintf("Register %s(%d:%d) being skipped, index out of range\n", reg->GetName(), jdim, idim);
                        else
                            InfoPrintf("Register %s(%d) being skipped, index out of range\n", reg->GetName(), idim);
                    }
                    ++skipCnt;
                    continue;
                }

                // compute the register's address and apply limits if given
                UINT32 theAddr = reg->GetAddress() + (idim * vsize1) + (jdim * vsize2);
                if(!AddrInRange(theAddr))
                {
                    if(m_verbose)
                        InfoPrintf("Register %s being skipped, not in requested ranges\n", reg->GetName());
                    ++skipCnt;
                    continue;
                }

                // ssaraogi: Added hack for pwr registers which uses falcon extended address (bug 497992)
                if (useFalconExtendedAddr)
                {
                    tmpVal = LocalRegRd32(PpwrPriAddrReg->GetAddress());
                    if (m_verbose)
                        InfoPrintf ("Value read for register LW_PPWR_PRI_ADDR = 0x%x\n", tmpVal);
                    LocalRegWr32(PpwrPriAddrReg->GetAddress(), 1);
                    if (m_verbose)
                        InfoPrintf ("(bug 497992): Writing LW_PPWR_PRI_ADDR (addr = 0x%x) with value 0x1 for accessing falcon extended address for register %s.\nWill write back the register LW_PPWR_PRI_ADDR with the read value 0x%x after register check is complete\n", PpwrPriAddrReg->GetAddress(), reg->GetName(), tmpVal);
                }

                // test the initial value if enabled
                UINT32 testStatus = 0;
                if(m_pParams->ParamPresent("-check_init_values"))
                    testStatus |= DoReadInitValuesTest(reg.get(), theAddr);
                if(m_pParams->ParamPresent("-check_actual_init_values"))
                    testStatus |= DoReadActualInitValuesTest(reg.get(), theAddr);
                if(m_only_prod)
                    testStatus |= DoReadProdValuesTest(reg.get(), theAddr);
                if (!m_pParams->ParamPresent("-only_init_values") && !m_only_prod)
                {
                  // if the register isn't writable, we're done
                  if(!reg->isWriteable())
                  {
                      InfoPrintf("Register %s can't be written (per manual)\n", reg->GetName());
                      skipCnt += !(m_pParams->ParamPresent("-check_init_values") || m_pParams->ParamPresent("-check_actual_init_values") ) ;
                  } else {

                      // always test writes of all 0's and 1's
                      testStatus |= DoWrite0Test(reg.get(), theAddr);
                      testStatus |= DoWrite1Test(reg.get(), theAddr);

                      if(m_pParams->ParamPresent("-walk1"))
                          testStatus |= DoWalkTest(reg.get(), theAddr, 1, "Walk1");

                      if(m_pParams->ParamPresent("-walk0"))
                          testStatus |= DoWalkTest(reg.get(), theAddr, 0, "Walk0");

                      if(m_pParams->ParamPresent("-A5"))
                          testStatus |= DoA5Test(reg.get(), theAddr);

                      if(m_pParams->ParamPresent("-random"))
                      {
                          unsigned count = m_pParams->ParamUnsigned("-random_count", 1);
                          testStatus |= DoRandomTest(reg.get(), theAddr, count);
                      }
                   }
                }

                if (useFalconExtendedAddr)
                {
                    LocalRegWr32(PpwrPriAddrReg->GetAddress(), tmpVal);
                    if (m_verbose)
                        InfoPrintf ("Wrote back register LW_PPWR_PRI_ADDR with value 0x%x\n", tmpVal);
                }

                runStatus |= testStatus;

                if(testStatus != 0)
                    ++failCnt;
            }
        }
    }

    InfoPrintf("Total registers processed     : %d\n", regCnt);
    InfoPrintf("Total registers skipped       : %d\n", skipCnt);
    InfoPrintf("Total registers failed        : %d\n", failCnt);
    InfoPrintf("Total registers passed        : %d\n", regCnt - failCnt - skipCnt);

    if(m_fbTest)
    {
        m_fbTestBegin *= 4;
        m_fbTestEnd *= 4;

        if((UINT64)m_fbTestBegin >= lwgpu->GetFBMemSize())
        {
            m_fbTestBegin = 0;
        }

        if((m_fbTestEnd == 0) || ((UINT64)m_fbTestEnd >= lwgpu->GetFBMemSize()))
        {
            m_fbTestEnd = (UINT32)(lwgpu->GetFBMemSize()) - 4;
        }

        UINT32 wrVal, rdVal, addr;

        for(addr=m_fbTestBegin; addr<=m_fbTestEnd; addr+=4)
        {
            wrVal = m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF);

            MEM_WR32(addr, wrVal);
            rdVal = MEM_RD32(addr);
            ++fbCnt;
            if(rdVal != wrVal)
            {
                ErrPrintf("Write to FB at addr 0x%08x failed: 0x%08x != 0x%08x\n", addr, wrVal, rdVal);
                ++fbFail;
                runStatus = 1;
            }
            else if(m_verbose)
            {
                InfoPrintf("Write to FB at addr 0x%08x passed: 0x%08x == 0x%08x\n", addr, wrVal, rdVal);
            }
        }
    }

    InfoPrintf("Total FB locations processed  : %d\n", fbCnt);
    InfoPrintf("Total FB locations failed     : %d\n", fbFail);

    if(m_pParams->ParamPresent("-ptvo_special")) {
        runStatus |= PTVOSpecialTest();
    }
    if(m_pParams->ParamPresent("-tmds_special")) {
        runStatus |= TMDSSpecialTest();
    }

    if(runStatus)
    {
        SetStatus(Test::TEST_FAILED_CRC);
        getStateReport()->crcCheckFailed("run failed, see errors/stats printed!\n");
    }
    else
    {
        SetStatus(TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    }
}

bool CRegTestGpu::PrintExceptions(void)
{

    InfoPrintf("Constant exceptions!\n");
    map<string, CConstVal *>::iterator constBitsIter = m_constBits.begin();
    while(constBitsIter != m_constBits.end())
    {
        CConstVal *constVal = constBitsIter->second;
        InfoPrintf("  %s: value=0x%08x mask=0x%08x\n", constBitsIter->first.c_str(), constVal->GetConstVal(), constVal->GetConstMask());
        ++constBitsIter;
    }

    InfoPrintf("Force exceptions!\n");
    map<string, CConstVal *>::iterator forceBitsIter = m_forceBits.begin();
    while(forceBitsIter != m_forceBits.end())
    {
        CConstVal *constVal = forceBitsIter->second;
        InfoPrintf("  %s: value=0x%08x mask=0x%08x\n", forceBitsIter->first.c_str(), constVal->GetConstVal(), constVal->GetConstMask());
        ++forceBitsIter;
    }

    InfoPrintf("Intialization exceptions!\n");
    map<string, UINT32>::iterator initIter = m_InitialVals.begin();
    while(initIter != m_InitialVals.end())
    {
        InfoPrintf("  %s: value=0x%08x\n", initIter->first.c_str(), initIter->second);
        ++initIter;
    }

    InfoPrintf("Initial Value Override exceptions!\n");
    map<string, CConstVal *>::iterator overIter = m_InitialOverride.begin();
    while(overIter != m_InitialOverride.end())
    {
        CConstVal *overVal = overIter->second;
        InfoPrintf("  %s: value=0x%08x mask=0x%08x\n", overIter->first.c_str(), overVal->GetConstVal(), overVal->GetConstMask());
        ++overIter;
    }

    InfoPrintf("Bit Exclusion exceptions!\n");
    map<string, UINT32>::iterator excludeBitsIter = m_excludeBits.begin();
    while(excludeBitsIter != m_excludeBits.end())
    {
        InfoPrintf("  %s: 0x%08x\n", excludeBitsIter->first.c_str(), excludeBitsIter->second);
        ++excludeBitsIter;
    }

    InfoPrintf("Skip register exceptions!\n");
    map<string, int>::iterator skipRegIter = m_skipRegs.begin();
    while(skipRegIter != m_skipRegs.end())
    {
        InfoPrintf("  %s\n", skipRegIter->first.c_str());
        ++skipRegIter;
    }

    return true;
}

UINT32 CRegTestGpu::ApplyWriteMasks(IRegisterClass* reg, UINT32 rdVal, UINT32 wrVal)
{
    UINT32 oldVal    = wrVal;
    UINT32 maskedVal = wrVal;
    UINT32 taskMask  = reg->GetTaskMask  ();

    // for fields of "task" type, use the first non-task value (or zero if all values ilwoke tasks)
    if (taskMask)
    {
        UINT32 nontaskVal = 0;
        for (unique_ptr<IRegisterField> fld = reg->GetFieldHead(); fld; fld = reg->GetFieldNext())
        {
            if (!fld->isTask())
                continue;
            for (unique_ptr<IRegisterValue> val = fld->GetValueHead(); val; val = fld->GetValueNext())
            {
                if (val->IsTask())
                    continue;
                nontaskVal |= val->GetValue() << fld->GetStartBit();
                break;
            }
        }
        maskedVal &=             ~taskMask;
        maskedVal |= nontaskVal & taskMask;
        if (m_verbose)
            DebugPrintf("WriteMASK: Manuals Task Write Adjustment: mask=0x%08x prvwrt=0x%08x newwrt=0x%08x  %s\n",
                        taskMask, oldVal, maskedVal, reg->GetName());

        oldVal = maskedVal;
    }

    // don't change excluded bits listed in exceptions file
    if (m_excludeBits.find(reg->GetName()) != m_excludeBits.end())
    {
        UINT32 excludeMask = m_excludeBits[reg->GetName()];

        maskedVal &=        ~excludeMask;
        maskedVal |= rdVal & excludeMask;
        if (m_verbose)
            DebugPrintf("WriteMASK: SkipFile Exclude Write Exclude Adjustment: mask=0x%08x prvwrt=0x%08x newwrt=0x%08x  %s\n",
                        excludeMask, oldVal, maskedVal, reg->GetName());

        oldVal = maskedVal;
    }

    // apply forced fields
    if (m_forceBits.find(reg->GetName()) != m_forceBits.end())
    {
        CConstVal *force = m_forceBits[reg->GetName()];

        maskedVal &= ~force->GetConstMask();
        maskedVal |=  force->GetConstVal ();
        if (m_verbose)
            DebugPrintf("WriteMASK: SkipFile Force Write Adjustment: mask=0x%08x val=0x%08x prvwrt=0x%08x newwrt=0x%08x  %s\n",
                        force->GetConstMask(), force->GetConstVal(), oldVal, maskedVal, reg->GetName());

        oldVal = maskedVal;
    }

    return maskedVal;
}

UINT32 CRegTestGpu::ComputeExpectedReadValue(IRegisterClass* reg, UINT32 rdValOld, UINT32 wrVal, UINT32 rdValNew)
{
    UINT32 expVal = wrVal;
    UINT32 oldVal = wrVal;
    UINT32 wrMask  = reg->GetWriteMask ();
    UINT32 cw1Mask = reg->GetW1ClrMask ();
    UINT32 rdMask  = reg->GetReadMask  ();
    UINT32 cMask   = reg->GetConstMask ();

    // apply the write mask from the manuals
    if (wrMask != 0xFFFFFFFF)
    {
        expVal &=             wrMask;
        expVal |= rdValOld & ~wrMask;
        if (m_verbose)
            DebugPrintf("ReadExpVal: Manuals Write Mask Adjustment: mask=0x%08x oldval=0x%08x prvexp=0x%08x newexp=0x%08x  %s\n",
                        wrMask, rdValOld, oldVal, expVal, reg->GetName());
        oldVal = expVal;
    }

    // apply the clear-on-write1 mask
    if (cw1Mask)
    {
        expVal &=             ~cw1Mask;
        expVal |=   rdValOld & cw1Mask;
        expVal &= ~(wrVal    & cw1Mask);
        if (m_verbose)
            DebugPrintf("ReadExpVal: Manuals Clear on Write1 Mask Adjustment: mask=0x%08x oldval=0x%08x prvexp=0x%08x newexp=0x%08x  %s\n",
                        cw1Mask, rdValOld, oldVal, expVal, reg->GetName());
        oldVal = expVal;
    }

    // apply the read mask
    if (rdMask != 0xFFFFFFFF)
    {
        expVal &= rdMask;
        if (m_verbose)
            DebugPrintf("ReadExpVal: Manuals Read Mask Adjustment: mask=0x%08x prvexp=0x%08x newexp=0x%08x  %s\n",
                        rdMask, oldVal, expVal, reg->GetName());
        oldVal = expVal;
    }

    // apply the constant mask and value from the manuals
    if (cMask)
    {
        UINT32 cVal = reg->GetConstValue();
        expVal &= ~cMask;
        expVal |=  cVal;
        if (m_verbose)
            DebugPrintf("ReadExpVal: Manuals Constant Mask Adjustment: cmask=0x%08x cval=0x%08x prvexp=0x%08x newexp=0x%08x  %s\n",
                        cMask, cVal, oldVal, expVal, reg->GetName());
        oldVal = expVal;
    }

    // apply exeptions from the skip file
    if (m_excludeBits.find(reg->GetName()) != m_excludeBits.end())
    {
        UINT32 excMask = m_excludeBits[reg->GetName()];

        expVal &=           ~excMask;
        expVal |= rdValNew & excMask;
        if (m_verbose)
            DebugPrintf("ReadExpVal: SkipFile Exclude Mask Adjustment: mask=0x%08x prvexp=0x%08x newread=0x%08x newexp=0x%08x  %s\n",
                        excMask, oldVal, rdValNew, expVal, reg->GetName());
        oldVal = expVal;
    }

    // apply constants from the skip file
    if (m_constBits.find(reg->GetName()) != m_constBits.end())
    {
        CConstVal *constVal = m_constBits[reg->GetName()];
        UINT32     cMask    = constVal->GetConstMask();
        UINT32     cVal     = constVal->GetConstVal ();

        expVal &= ~cMask;
        expVal |=  cVal;
        if (m_verbose)
            DebugPrintf("ReadExpVal: SkipFile Constant Mask Adjustment: mask=0x%08x cval=0x%08x prvexp=0x%08x newexp=0x%08x  %s\n",
                        cMask, cVal, oldVal, expVal, reg->GetName());
        oldVal = expVal;
    }

    return expVal;
}

// NOTE:  Deprecated.  Should use ComputeExpectedReadValue instead
bool CRegTestGpu::DoSpecialAdjustments(IRegisterClass* reg, UINT32 which, UINT32* expVal, UINT32* readVal)
{
    bool adjustmentMade = false;

    // first adjust the expected value by the user defined constant mask/data
    // this is used when a reg returns constant values in certain bits but those
    // values are not properly reflected in the manuals...
    // note we do NOT adjust the read value, as it should reflect the constant bits properly, and is what we are
    // testing for here....
    if(which & SADJ_CONST)
    {
        if(m_constBits.find(reg->GetName()) != m_constBits.end())
        {
            CConstVal *constVal = m_constBits[reg->GetName()];;
            UINT32 newVal = *expVal;
            newVal = (newVal & ~constVal->GetConstMask()) | (constVal->GetConstVal() & constVal->GetConstMask());
            DebugPrintf("%s Constant Adjustment: value=0x%08x mask=0x%08x\n", reg->GetName(),
                constVal->GetConstVal(), constVal->GetConstMask());
            DebugPrintf("%s Constant Adjustment: orig expected=0x%08x new expected=0x%08x\n", reg->GetName(), *expVal, newVal);
            *expVal = newVal;
            adjustmentMade = true;
        }
    }

    // Now adjust for any bits the user asked to exclude from the checking as an override
    // this is applied to BOTH the read and expected value to throw the bits out of the check
    if(which & SADJ_EXCL)
    {
        if(m_excludeBits.find(reg->GetName()) != m_excludeBits.end())
        {
            UINT32 excludeMask = m_excludeBits[reg->GetName()];
            DebugPrintf("%s Exclude Adjustment: exclude mask=0x%08x\n", reg->GetName(), excludeMask);
            DebugPrintf("%s Exclude Adjustment: orig read=0x%08x new read=0x%08x\n", reg->GetName(),
                *readVal, *readVal & ~excludeMask);
            *readVal &= ~excludeMask;
            DebugPrintf("%s Exclude Adjustment: orig expected=0x%08x new expected=0x%08x\n", reg->GetName(),
                *expVal, *expVal & ~excludeMask);
            *expVal &= ~excludeMask;
            adjustmentMade = true;
        }
    }

    return adjustmentMade;
}

// NOTE:  Deprecated.  Should use ApplyWriteMasks instead
bool CRegTestGpu::DoWriteAdjustments(IRegisterClass* reg, UINT32 which, UINT32 initialValue, UINT32* writeVal)
{
    bool adjustmentMade = false;

    // first adjust the expected value by the user defined constant mask/data
    // this is used when a reg returns constant values in certain bits but those
    // values are not properly reflected in the manuals...
    // note we do NOT adjust the read value, as it should reflect the constant bits properly, and is what we are
    // testing for here....
    if(which & SADJ_CONST)
    {
#if 0
        if(m_constBits.find(reg->GetName()) != m_excludeBits.end())
        {
            CConstVal *constVal = m_constBits[reg->GetName()];;
            UINT32 newVal = *expVal;
            newVal = (newVal & ~constVal->GetConstMask()) | (constVal->GetConstVal() & constVal->GetConstMask());
            DebugPrintf("%s Constant Adjustment: value=0x%08x mask=0x%08x\n", reg->GetName(),
                constVal->GetConstVal(), constVal->GetConstMask());
            DebugPrintf("%s Constant Adjustment: orig expected=0x%08x new expected=0x%08x\n", reg->GetName(), *expVal, newVal);
            *expVal = newVal;
            adjustmentMade = true;
        }
#endif
    }

    // Now adjust for any bits the user asked to exclude from the checking as an override
    // this is applied to BOTH the read and expected value to throw the bits out of the check
    if(which & SADJ_EXCL)
    {
        if(m_excludeBits.find(reg->GetName()) != m_excludeBits.end())
        {
            UINT32 excludeMask = m_excludeBits[reg->GetName()];
            UINT32 origWrite = *writeVal;

            DebugPrintf("%s Exclude Write Adjustment: exclude mask=0x%08x\n", reg->GetName(), excludeMask);

            *writeVal &= ~excludeMask;  // clear out the exclude bits
            *writeVal |= (initialValue & excludeMask); // add in the initial register bits for everything we are excluding
                                                       // this will cause the regiter to get hit with identical bits for
                                                       // for everything we have been told to exclude testing

            DebugPrintf("%s Exclude Write Adjustment: orig write=0x%08x new write=0x%08x\n", reg->GetName(),
                origWrite, *writeVal);

            adjustmentMade = true;
        }
    }

    return adjustmentMade;
}

//getting init value from manual
bool CRegTestGpu::GetInitValue(IRegisterClass* reg, UINT32 testAddr, UINT32 *initVal,UINT32 *initMask)
{
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;

    UINT32 tmpVal, bit, startBit, endBit, one = 1;
    bool hasInitValue=false;
    DebugPrintf("CRegTestGpu::GetInitValue: addr = 0x%08x\n", testAddr);

    *initVal=0;
    *initMask=0;
    field = reg->GetFieldHead();
    while(field)
    {
         startBit = field->GetStartBit();
         endBit   = field->GetEndBit();
         DebugPrintf("  Field %s access=%s start_bit=%d end_bit=%d\n", field->GetName(), field->GetAccess(), startBit, endBit);
         value = field->GetValueHead();
         while(value)
         {
             string name(value->GetName());
             tmpVal=value->GetValue();
             DebugPrintf("Value %s val=0x%08x\n", name.c_str(), tmpVal);
             string::size_type pos=name.rfind("_INIT");
             if(pos==name.size()-5)
             {
                 *initVal = *initVal | (tmpVal << startBit);
                 for (bit=startBit; bit<=endBit; bit++)
                 {
                    *initMask =*initMask | (one << bit);
                 }
                 DebugPrintf("accumulate init value =0x%08x\n",  *initVal);
                 DebugPrintf("accumulate init mask  =0x%08x\n",  *initMask);
                 hasInitValue=true;
             }
             value = field->GetValueNext();
         }
         field = reg->GetFieldNext();
    }
    return hasInitValue;
}

bool CRegTestGpu::GetActualInitValue(IRegisterClass* reg, UINT32 testAddr, UINT32 *initVal,UINT32 *initMask)
{
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;

    UINT32 tmpVal, bit, startBit, endBit, one = 1;
    UINT32 temp1 = 0;                       // Defining a New Variable
    bool hasInitValue=false;
    bool temp2;                             // Varible to confirm presence of 'I' or '__PROD'
    DebugPrintf("CRegTestGpu::GetActualInitValue: addr = 0x%08x\n", testAddr);

    *initVal=0;
    *initMask=0;
    field = reg->GetFieldHead();
    while(field)
    {
         startBit = field->GetStartBit();
         endBit   = field->GetEndBit();
         temp2 = false;
         DebugPrintf("  Field %s access=%s start_bit=%d end_bit=%d\n", field->GetName(), field->GetAccess(), startBit, endBit);
         value = field->GetValueHead();
         while(value)
         {
             string name(value->GetName());
             tmpVal=value->GetValue();
             DebugPrintf("Value %s val=0x%08x\n", name.c_str(), tmpVal);

             //Searching For 'I' field
             if(value->IsHWInit())
             {
                 temp2 = true;              //Register has initial value
                 temp1 = (tmpVal << startBit);
             }
             //Searching for '__PROD' field
             string::size_type pos=name.rfind("__PROD");
             if(pos==name.size()-6)
             {
                 temp2 = true;              //Register has __PROD value
                 temp1 = (tmpVal << startBit);  //overwriting temp1 if PROD field is present
                 break;
             }
             value = field->GetValueNext();
         }
         if (temp2)
         {
             *initVal = *initVal | (temp1);
             for (bit=startBit; bit<=endBit; bit++)
             {
                 *initMask =*initMask | (one << bit);
             }
             DebugPrintf("accumulate init value =0x%08x\n",  *initVal);
             DebugPrintf("accumulate init mask  =0x%08x\n",  *initMask);
             hasInitValue=true;
         }
         field = reg->GetFieldNext();
    }
    return hasInitValue;
}

//getting prod value from manual
bool CRegTestGpu::GetProdValue(IRegisterClass* reg, UINT32 testAddr, UINT32 *prodVal,UINT32 *prodMask)
{
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;

    UINT32 tmpVal, bit, startBit, endBit, one = 1;
    bool hasProdValue=false;
    DebugPrintf("CRegTestGpu::GetProdValue: addr = 0x%08x\n", testAddr);

    *prodVal=0;
    *prodMask=0;
    field = reg->GetFieldHead();
    while(field)
    {
         startBit = field->GetStartBit();
         endBit   = field->GetEndBit();
         DebugPrintf("  Field %s access=%s start_bit=%d end_bit=%d\n", field->GetName(), field->GetAccess(), startBit, endBit);
         value = field->GetValueHead();
         while(value)
         {
             string name(value->GetName());
             tmpVal=value->GetValue();
             DebugPrintf("Value %s val=0x%08x\n", name.c_str(), tmpVal);
             string::size_type pos=name.rfind("__PROD");
             if(pos==name.size()-6)
             {
                 *prodVal = *prodVal | (tmpVal << startBit);
                 for (bit=startBit; bit<=endBit; bit++)
                 {
                    *prodMask =*prodMask | (one << bit);
                 }
                 DebugPrintf("accumulate prod value =0x%08x\n",  *prodVal);
                 DebugPrintf("accumulate prod mask  =0x%08x\n",  *prodMask);
                 hasProdValue=true;
             }
             value = field->GetValueNext();
         }
         field = reg->GetFieldNext();
    }
    return hasProdValue;
}

//reset display before reading init values
void CRegTestGpu::ResetDisplay()
{
    UINT32 tmp = LocalRegRd32(LW_PMC_ENABLE);
    tmp &= ~(DRF_MASK(LW_PMC_ENABLE_PDISP)<<(DRF_SHIFT(LW_PMC_ENABLE_PDISP)));
    tmp |= DRF_DEF(_PMC, _ENABLE, _PDISP, _DISABLED);
    LocalRegWr32(LW_PMC_ENABLE, tmp);
   // do three reads of this register
    tmp = LocalRegRd32(LW_PMC_ENABLE);
    tmp = LocalRegRd32(LW_PMC_ENABLE);
    tmp = LocalRegRd32(LW_PMC_ENABLE);
    // pull the register out of reset
    tmp &= ~(DRF_MASK(LW_PMC_ENABLE_PDISP)<<(DRF_SHIFT(LW_PMC_ENABLE_PDISP)));
    tmp |= DRF_DEF(_PMC, _ENABLE, _PDISP, _ENABLED);
    LocalRegWr32(LW_PMC_ENABLE, tmp);
    return;
}

UINT32 CRegTestGpu::DoReadInitValuesTest(IRegisterClass* reg, UINT32 testAddr)
{
    UINT32 readVal;
    UINT32 initVal;
    UINT32 initMask;
    UINT32 err = 0;

    if (!GetInitValue(reg, testAddr, &initVal, &initMask))
    {
        InfoPrintf("Register %s has no initial value, skipping\n",reg->GetName());
        return 0;
    }

    if (m_InitialOverride.find(reg->GetName()) != m_InitialOverride.end())
    {
        CConstVal *overVal = m_InitialOverride[reg->GetName()];
        UINT32 oMask = overVal->GetConstMask();
        UINT32 oVal  = overVal->GetConstVal();

        initVal = (initVal & ~oMask) | (oVal & oMask);
        InfoPrintf("Register %s initial value override found!, mask=0x%08x value=0x%08x init=0x%08x\n", reg->GetName(), oMask, oVal, initVal );
    }

    // for dedugging only...
    UINT32 rmask = reg->GetReadMask  (); if (m_verbose) DebugPrintf("CRegTestGpu::DoReadInitValuesTest: read mask = 0x%08x\n", rmask);
    UINT32 cmask = reg->GetConstMask (); if (m_verbose) DebugPrintf("CRegTestGpu::DoReadInitValuesTest: const mask = 0x%08x\n", cmask);
    testAddr += a_offset;

    readVal = LocalRegRd32(testAddr);
    DebugPrintf("ReadInitValues: readVal=0x%08x initVal=0x%08x initMask=0x%08x \n", readVal, initVal, initMask);
    readVal &= initMask & rmask;
    initVal &= rmask;
    DoSpecialAdjustments(reg, SADJ_ALL, &readVal, &initVal);

    // This abs call was originally abs(initVal - readVal), which is pointless
    // because the difference of two unsigned integers is always positive.
    // I have changed it to be what I think the author intended even though
    // it may have a change in behavior
    if(UINT32(abs((INT32)initVal - (INT32)readVal)) > Tolerance(reg))
    {
        ErrPrintf("Read init value from register addr=0x%08x expect=0x%08x actual=0x%08x  %s\n", testAddr, initVal, readVal, reg->GetName());
        err = 1;
    }
    else if(m_verbose)
    {
        InfoPrintf("Read init value from register addr=0x%08x expect=0x%08x actual=0x%08x  %s\n", testAddr, initVal, readVal, reg->GetName());
    }
    return err;
}

UINT32 CRegTestGpu::DoReadActualInitValuesTest(IRegisterClass* reg, UINT32 testAddr)
{
    UINT32 readVal;
    UINT32 initVal;
    UINT32 initMask;
    UINT32 err = 0;

    if (!GetActualInitValue(reg, testAddr, &initVal, &initMask))
    {
        InfoPrintf("Register %s has no initial value, skipping\n",reg->GetName());
        return 0;
    }

    if (m_InitialOverride.find(reg->GetName()) != m_InitialOverride.end())
    {
        CConstVal *overVal = m_InitialOverride[reg->GetName()];
        UINT32 oMask = overVal->GetConstMask();
        UINT32 oVal  = overVal->GetConstVal();

        initVal = (initVal & ~oMask) | (oVal & oMask);
        InfoPrintf("Register %s initial value override found!, mask=0x%08x value=0x%08x init=0x%08x\n", reg->GetName(), oMask, oVal, initVal );
    }

    // for dedugging only...
    UINT32 rmask = reg->GetReadMask  (); if (m_verbose) DebugPrintf("CRegTestGpu::DoReadActualInitValuesTest: read mask = 0x%08x\n", rmask);
    UINT32 cmask = reg->GetConstMask (); if (m_verbose) DebugPrintf("CRegTestGpu::DoReadActualInitValuesTest: const mask = 0x%08x\n", cmask);
    testAddr += a_offset;

    readVal = LocalRegRd32(testAddr);
    DebugPrintf("ReadActualInitValues: readVal=0x%08x initVal=0x%08x initMask=0x%08x \n", readVal, initVal, initMask);
    readVal &= initMask & rmask;
    initVal &= rmask;
    DoSpecialAdjustments(reg, SADJ_ALL, &readVal, &initVal);

   // This abs call was originally abs(initVal - readVal), which is pointless
    // because the difference of two unsigned integers is always positive.
    // I have changed it to be what I think the author intended even though
    // it may have a change in behavior
    if(UINT32(abs((INT32)initVal - (INT32)readVal)) > Tolerance(reg))
    {
        ErrPrintf("Read init value from register addr=0x%08x expect=0x%08x actual=0x%08x  %s\n", testAddr, initVal, readVal, reg->GetName());
        err = 1;
    }
    else if(m_verbose)
    {
        InfoPrintf("Read init value from register addr=0x%08x expect=0x%08x actual=0x%08x  %s\n", testAddr, initVal, readVal, reg->GetName());
    }
    return err;
}

UINT32 CRegTestGpu::DoReadProdValuesTest(IRegisterClass* reg, UINT32 testAddr)
{
    UINT32 readVal;
    UINT32 prodVal;
    UINT32 prodMask;
    UINT32 err = 0;

    if (!GetProdValue(reg, testAddr, &prodVal, &prodMask))
    {
        // InfoPrintf("Register %s has no __PROD value, skipping\n",reg->GetName());
        return 0;
    }

    // for dedugging only...
    UINT32 rmask = reg->GetReadMask  (); if (m_verbose) DebugPrintf("CRegTestGpu::DoReadProdValuesTest: read mask = 0x%08x\n", rmask);
    testAddr += a_offset;

    readVal = LocalRegRd32(testAddr);
    DebugPrintf("ReadProdValues: readVal=0x%08x prodVal=0x%08x prodMask=0x%08x \n", readVal, prodVal, prodMask);
    readVal &= prodMask & rmask;
    prodVal &= rmask;

   // This abs call was originally abs(initVal - readVal), which is pointless
    // because the difference of two unsigned integers is always positive.
    // I have changed it to be what I think the author intended even though
    // it may have a change in behavior
    if(prodVal != readVal)
    {
        ErrPrintf("Read prod value from register addr=0x%08x expect=0x%08x actual=0x%08x  %s\n", testAddr, prodVal, readVal, reg->GetName());
        err = 1;
    }
    else if(m_verbose)
    {
        InfoPrintf("Read prod value from register addr=0x%08x expect=0x%08x actual=0x%08x  %s\n", testAddr, prodVal, readVal, reg->GetName());
    }
    return err;
}

UINT32 CRegTestGpu::Tolerance(IRegisterClass* reg)
{
    map<string, UINT32>::const_iterator iter = m_Tolerances.find(reg->GetName());

    if (iter == m_Tolerances.end())
        return 0;
    else
        return m_Tolerances[reg->GetName()];
}

UINT32 CRegTestGpu::SaveRegValue(IRegisterClass* reg, UINT32 testAddr, UINT32 &lwrVal)
{
    UINT32 rdMask  = reg->GetReadMask    (); if (m_verbose) DebugPrintf("CRegTestGpu::SaveRegValue: read   mask = 0x%08x  %s\n", rdMask , reg->GetName());
    UINT32 cmask   = reg->GetConstMask   (); if (m_verbose) DebugPrintf("CRegTestGpu::SaveRegValue: const  mask = 0x%08x  %s\n", cmask  , reg->GetName());
    UINT32 cval    = reg->GetConstValue  (); if (m_verbose) DebugPrintf("CRegTestGpu::SaveRegValue: const value = 0x%08x  %s\n", cval   , reg->GetName());
    UINT32 offAddr = testAddr + a_offset   ; if (m_verbose) DebugPrintf("CRegTestGpu::SaveRegValue: offset addr = 0x%08x  %s\n", offAddr, reg->GetName());
    lwrVal = LocalRegRd32(offAddr);
    if(m_verbose)
        InfoPrintf("Read current value from register addr=0x%08x data=0x%08x  %s\n", testAddr, lwrVal, reg->GetName());

    if (lwrVal & ~rdMask) {
        ErrPrintf("SaveRegValue: Register bits are marked unreadable but return 1: addr=0x%08x actual=0x%08x expect=0x%08x mask=0x%08x  %s\n",
                  offAddr, lwrVal, lwrVal & rdMask, rdMask, reg->GetName());
        return 1;
    }

    if ((lwrVal & cmask) != cval) {
        ErrPrintf("Constant bits wrong in read value from register addr=0x%08x actual=0x%08x expect=0x%08x mask=0x%08x  %s\n",
                  offAddr, lwrVal, cval, cmask, reg->GetName());
        return 1;
    }
    return 0;
}

UINT32 CRegTestGpu::RestoreRegValue(IRegisterClass* reg, UINT32 testAddr, UINT32 lwrVal)
{
    UINT32 offAddr = testAddr + a_offset; if (m_verbose) DebugPrintf("CRegTestGpu::RestoreRegValue: offset addr = 0x%08x data=0x%08x %s\n", offAddr, lwrVal, reg->GetName());
    LocalRegWr32(offAddr, lwrVal);
    return 0;
}

UINT32 CRegTestGpu::DoWriteTest(IRegisterClass* reg, UINT32 testAddr, UINT32 &lwrVal, UINT32 testWrVal, const char *tType)
{
    UINT32 err = 0;
    DebugPrintf("CRegTestGpu::DoWriteTest: type:%s testAddr:0x%08x testWrVal:0x%08x %s\n", tType, testAddr, testWrVal, reg->GetName());

    UINT32 offAddr = testAddr + a_offset;
    if (m_verbose)
        DebugPrintf("CRegTestGpu::DoWriteTest: offset addr = 0x%08x\n", offAddr);

    // write the test value to the register (after masking)
    INT32 wrVal = ApplyWriteMasks (reg, lwrVal, testWrVal);
    LocalRegWr32(offAddr, wrVal);

    // read the register's new value
    UINT32 newVal = LocalRegRd32(offAddr);

    // compute expected value
    UINT32 expVal = ComputeExpectedReadValue (reg, lwrVal, wrVal, newVal);

    // This abs call was originally abs(newVal - expVal), which is pointless
    // because the difference of two unsigned integers is always positive.
    // I have changed it to be what I think the author intended even though
    // it may have a change in behavior
    if ((UINT32(abs((INT32)newVal - (INT32)expVal))) > Tolerance(reg))
    {
        ErrPrintf("Wrote 0x%08x to register addr=0x%08x expect=0x%08x actual=0x%08x  %s\n", wrVal, offAddr, expVal, newVal, reg->GetName());
        err = 1;
    }
    else if(m_verbose)
    {
        InfoPrintf(" Wrote 0x%08x to register addr=0x%08x expect=0x%08x actual=0x%08x  %s\n", wrVal, offAddr, expVal, newVal, reg->GetName());
    }

    return err;
}

UINT32 CRegTestGpu::DoWrite0Test(IRegisterClass* reg, UINT32 testAddr)
{
    UINT32 lwrVal;
    UINT32 err = 0;

    err |= SaveRegValue    (reg, testAddr, lwrVal);
    err |= DoWriteTest     (reg, testAddr, lwrVal, 0x0, "Write0");
    err |= RestoreRegValue (reg, testAddr, lwrVal);

    return err;
}

UINT32 CRegTestGpu::DoWrite1Test(IRegisterClass* reg, UINT32 testAddr)
{
    UINT32 lwrVal;
    UINT32 err = 0;

    err |= SaveRegValue    (reg, testAddr, lwrVal);
    err |= DoWriteTest     (reg, testAddr, lwrVal, 0xffffffff, "Write1");
    err |= RestoreRegValue (reg, testAddr, lwrVal);

    return err;
}

UINT32 CRegTestGpu::DoA5Test(IRegisterClass* reg, UINT32 testAddr)
{
    UINT32 lwrVal;
    UINT32 err = 0;

    err |= SaveRegValue (reg, testAddr, lwrVal);
    err |= DoWriteTest (reg, testAddr, lwrVal, 0xAAAAAAAA, "WriteA");
    err |= DoWriteTest (reg, testAddr, lwrVal, 0x55555555, "Write5");
    err |= RestoreRegValue (reg, testAddr, lwrVal);

    return err;
}

UINT32 CRegTestGpu::DoRandomTest(IRegisterClass* reg, UINT32 testAddr, UINT32 count)
{
    UINT32 lwrVal;
    UINT32 err = 0;

    err |= SaveRegValue (reg, testAddr, lwrVal);

    for (unsigned i = 0; i < count; i++)
    {
        // create a random value and test it
        UINT32 randVal = m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF);
        err |= DoWriteTest (reg, testAddr, lwrVal, randVal, "WriteRandom");
    }

    err |= RestoreRegValue (reg, testAddr, lwrVal);

    return err;
}

UINT32 CRegTestGpu::DoWalkTest(IRegisterClass* reg, UINT32 testAddr, UINT32 val, const char *tType)
{
    int i;
    UINT32 lwrVal;
    UINT32 err = 0;

    err |= SaveRegValue    (reg, testAddr, lwrVal);

    for(i=0; i<32; i++)
    {
        UINT32 newData = (1<<i) ^ (val ? 0 : 0xffffffff);
        err |= DoWriteTest (reg, testAddr, lwrVal, newData, tType);
    }

    err |= RestoreRegValue (reg, testAddr, lwrVal);

    return err;
}

UINT32 CRegTestGpu::DoHwInitTest()
{
    unique_ptr<IRegisterClass> reg;
    UINT32 status = 0;
    UINT32 value;
    UINT32 rdVal;
    UINT32 expVal;

    int regCnt = 0;
    int failCnt = 0;
    int noinitCnt = 0;
    int skipCnt = 0;

    UINT32 limit1, limit2, vsize1, vsize2;
    UINT32 dims;

    reg = m_regMap->GetRegisterHead();
    while(reg)
    {
        // only process registers from the selected group
        const char* grpName = PassGroupCheck(reg.get(), true);
        if (!grpName)
        {
            reg = m_regMap->GetRegisterNext();
            continue;
        }

        if(m_skipRegs.find(reg->GetName()) != m_skipRegs.end())
        {
            ++skipCnt;
            InfoPrintf("Register %s being skipped\n", reg->GetName());
            reg = m_regMap->GetRegisterNext();
            continue;
        }

        dims = reg->GetArrayDimensions();
        reg->GetFormula2(limit1, limit2, vsize1, vsize2);

        UINT32 idim = 0, jdim = 0;
        UINT32 theAddr = 0;

        if(!reg->GetHWInitValue(&value))
        {
            do
            {
                do
                {
                    if(m_exclusions.SkipArrayReg(reg->GetName(), idim, jdim) == false)
                    {
                        if(dims == 0)
                            DebugPrintf("Found register in group %s: %s\n", grpName, reg->GetName());
                        else if(dims == 1)
                            DebugPrintf("Found register in group %s: %s(%d)\n", grpName, reg->GetName(), idim);
                        else if(dims == 2)
                            DebugPrintf("Found register in group %s: %s(%d,%d)\n", grpName, reg->GetName(), idim, jdim);

                        theAddr = reg->GetAddress() + (idim * vsize1) + (jdim * vsize2);

                        rdVal = lwgpu->GetGpuSubdevice()->RegRd32(theAddr + a_offset);

                        expVal = ((value & reg->GetReadMask()) | reg->GetConstMask());

                        DoSpecialAdjustments(reg.get(), SADJ_ALL, &expVal, &rdVal);

                        if(rdVal != expVal)
                        {
                            ++failCnt;

                            //                            ErrPrintf("%s failed init test:\n", reg->GetName());
                            if(dims == 0)
                                ErrPrintf("%s failed init test:    expect=0x%08x   actual=0x%08x\n",
                                    reg->GetName(), expVal, rdVal);
                            else if(dims == 1)
                                ErrPrintf("%s(%d) failed init test:    expect=0x%08x   actual=0x%08x\n",
                                    reg->GetName(), idim, expVal, rdVal);
                            else if(dims == 2)
                                ErrPrintf("%s(%d,%d) failed init test:    expect=0x%08x   actual=0x%08x\n",
                                    reg->GetName(), idim, jdim, expVal, rdVal);

                            status = 1;
                        }
                        else if(m_verbose)
                        {
                            if(dims == 0)
                                InfoPrintf("%s passed init test : 0x%08x\n", reg->GetName(), rdVal);
                            else if(dims == 1)
                                InfoPrintf("%s(%d) passed init test : 0x%08x\n", reg->GetName(), idim, rdVal);
                            else if(dims == 2)
                                InfoPrintf("%s(%d,%d) passed init test : 0x%08x\n", reg->GetName(), idim, jdim, rdVal);
                        }
                        ++regCnt;
                    }
                    else
                    {
                        // skipping register array index
                        DebugPrintf("Skipping register: %s index %d:%d\n", reg->GetName(), idim, jdim);
                    }
                    ++jdim;
                }
                while(jdim < limit2);

                jdim = 0;
                ++idim;
            }
            while(idim < limit1);
        }
        else
        {
            ++noinitCnt;

            if(m_verbose)
                InfoPrintf("%s has no HW init value defined\n", reg->GetName());
        }

        reg = m_regMap->GetRegisterNext();
    }

    InfoPrintf("Total registers processed     : %d\n", regCnt);
    InfoPrintf("Total registers failed        : %d\n", failCnt);
    InfoPrintf("Total registers passed        : %d\n", regCnt - noinitCnt - failCnt);
    InfoPrintf("Total registers with no init  : %d\n", noinitCnt);

    return status;
}

UINT32 CRegTestGpu::DoClkCntrTest()
{
    unique_ptr<IRegisterClass> reg;
    UINT32 status = 0;
    // UINT32 value;
    UINT32 rdVal;
    // UINT32 expVal;

    int regCnt = 0;
    int failCnt = 0;
    int noinitCnt = 0;
    // int skipCnt = 0;

    // UINT32 limit1, limit2, vsize1, vsize2;
    // UINT32 dims;

    // const char* grpName = NULL;

    // data = 0;
    // data = FLD_SET_DRF_NUM(_PRAMDAC,_HA_FP_TMDS_CONTROL,_ADDRESS,illegal_tmds_register,data);
    // lwgpu->RegWr32(LW_PRAMDAC_HA_FP_TMDS_CONTROL,data);

    reg = m_regMap->FindRegister("LW_PTRIM_CLK_CNTR_VP2_CFG");
    lwgpu->RegWr32(reg->GetAddress(), 0x31000000);  // Reset Counter
    lwgpu->RegWr32(reg->GetAddress(), 0x301000ff);  // Enable Counting, Count for 0xff xtal clocks

    Platform::ClockSimulator(1000);

    reg = m_regMap->FindRegister("LW_PTRIM_CLK_CNTR_VP2_CNT");
    rdVal = lwgpu->RegRd32(reg->GetAddress());
    InfoPrintf("Clock Counter LW_VP count     : %d\n", rdVal);

    reg = m_regMap->FindRegister("LW_PTRIM_CLK_CNTR_VP2_CFG");
    lwgpu->RegWr32(reg->GetAddress(), 0x31000000);  // Reset Counter
    lwgpu->RegWr32(reg->GetAddress(), 0x3010000f);  // Enable Counting, Count for 0x0f xtal clocks

    Platform::ClockSimulator(1000);

    reg = m_regMap->FindRegister("LW_PTRIM_CLK_CNTR_VP2_CNT");
    rdVal = lwgpu->RegRd32(reg->GetAddress());
    InfoPrintf("Clock Counter LW_VP count     : %d\n", rdVal);

    InfoPrintf("Total registers processed     : %d\n", regCnt);
    InfoPrintf("Total registers failed        : %d\n", failCnt);
    InfoPrintf("Total registers passed        : %d\n", regCnt - noinitCnt - failCnt);
    InfoPrintf("Total registers with no init  : %d\n", noinitCnt);

    return status;
}

void CRegTestGpu::PrintRegister(IRegisterClass* reg)
{
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;

    InfoPrintf("Register %s addr=0x%08x access=%s rmask=0x%08x wmask=0x%08x cmask=0x%08x\n",
        reg->GetName(), reg->GetAddress(), reg->GetAccess(), reg->GetReadMask(),
        reg->GetWriteMask(), reg->GetConstMask());

    field = reg->GetFieldHead();
    while(field)
    {
        InfoPrintf("  Field %s access=%s start_bit=%d end_bit=%d\n",
            field->GetName(), field->GetAccess(), field->GetStartBit(), field->GetEndBit());

        value = field->GetValueHead();
        while(value)
        {
            InfoPrintf("      Value %s val=0x%08x\n", value->GetName(), value->GetValue());
            value = field->GetValueNext();
        }

        field = reg->GetFieldNext();
    }
}

void CRegTestGpu::DecodeRegister(IRegisterClass* ireg, UINT32 numIdx, UINT32 idx1, UINT32 idx2)
{
    // return error code 1 if we don't have support for register maps
    if(!ireg)
        return;

    if(numIdx == 0)
        RawPrintf("%s 0x%08x\n", ireg->GetName(), ireg->GetAddress());
    else if(numIdx == 1)
        RawPrintf("%s(%d) 0x%08x\n", ireg->GetName(), idx1, ireg->GetAddress(idx1));
    else if(numIdx == 2)
        RawPrintf("%s(%d,%d) 0x%08x\n", ireg->GetName(), idx1, idx2,
            ireg->GetAddress(idx1,idx2));
    else
    {
        ErrPrintf("Asked to decode register with invalid idx count : %d\n", numIdx);
        return;
    }

    if(m_printLevel >= 1)
    {
        unique_ptr<IRegisterField> field;
        unique_ptr<IRegisterValue> value;

        field = ireg->GetFieldHead();
        while(field)
        {
            RawPrintf("  Field %s access=%s start_bit=%d end_bit=%d\n",
                field->GetName(), field->GetAccess(), field->GetStartBit(), field->GetEndBit());

            if(m_printLevel == 2)
            {
                value = field->GetValueHead();
                while(value)
                {
                    RawPrintf("      Value %s val=0x%08x\n", value->GetName(), value->GetValue());

                    value = field->GetValueNext();
                }
            }

            field = ireg->GetFieldNext();
        }
    }
}

void CRegTestGpu::PrintRegisterMap()
{
    unique_ptr<IRegisterClass> reg;
    string regName;
    UINT32 i, j;
    UINT32 limit1, vsize1;
    UINT32 limit2, vsize2;
    UINT32 dims;

    if ( m_regGroups && !m_regGroups->empty() )
    {
        auto regGroupsLwr = m_regGroups->begin();

        while ( regGroupsLwr != m_regGroups->end() )
        {
            regName = "LW_";
            regName += regGroupsLwr->c_str();
            regName += "_";

            reg = m_regMap->GetRegisterHead();
            while(reg)
            {
                if(m_printClass != -1)
                {
                    if(reg->GetClassId() != (UINT32)m_printClass)
                    {
                        reg = m_regMap->GetRegisterNext();
                        continue;
                    }
                }

                if(strncmp(reg->GetName(), regName.c_str(), regName.size()) == 0)
                {
                    dims = reg->GetArrayDimensions();
                    if(dims == 0)
                    {
                        DecodeRegister(reg.get(), 0, 0, 0);
                    }
                    else if(dims == 1)
                    {
                        reg->GetFormula1(limit1, vsize1);

                        if(limit1 > m_printArrayLwtoff)
                        {
                            WarnPrintf("Limiting array to print first 20 entries\n");
                            limit1 = m_printArrayLwtoff;
                        }

                        for(i=0; i<limit1; i++)
                        {
                            DecodeRegister(reg.get(), 1, i, 0);
                        }
                    }
                    else if(dims == 2)
                    {
                        reg->GetFormula2(limit1, limit2, vsize1, vsize2);

                        if(limit1 > m_printArrayLwtoff)
                        {
                            WarnPrintf("Limiting array to print first 20 entries\n");
                            limit1 = m_printArrayLwtoff;
                        }

                        if(limit2 > m_printArrayLwtoff)
                        {
                            WarnPrintf("Limiting array to print first 20 entries\n");
                            limit2 = m_printArrayLwtoff;
                        }

                        for(j=0; j<limit2; j++)
                            for(i=0; i<limit1; i++)
                            {
                                DecodeRegister(reg.get(), 2, i, j);
                            }
                    }
                }

                reg = m_regMap->GetRegisterNext();
            }

            regGroupsLwr++;

            regName.erase();
        }
    }
    else
    {
        reg = m_regMap->GetRegisterHead();
        while(reg)
        {
            if(m_printClass != -1)
            {
                if(reg->GetClassId() != (UINT32)m_printClass)
                {
                    reg = m_regMap->GetRegisterNext();
                    continue;
                }
            }

            dims = reg->GetArrayDimensions();
            if(dims == 0)
            {
                DecodeRegister(reg.get(), 0, 0, 0);
            }
            else if(dims == 1)
            {
                reg->GetFormula1(limit1, vsize1);

                if(limit1 > m_printArrayLwtoff)
                {
                    WarnPrintf("Limiting array to print first 20 entries\n");
                    limit1 = m_printArrayLwtoff;
                }

                for(i=0; i<limit1; i++)
                {
                    DecodeRegister(reg.get(), 1, i, 0);
                }
            }
            else if(dims == 2)
            {
                reg->GetFormula2(limit1, limit2, vsize1, vsize2);

                if(limit1 > m_printArrayLwtoff)
                {
                    WarnPrintf("Limiting array to print first 20 entries\n");
                    limit1 = m_printArrayLwtoff;
                }

                if(limit2 > m_printArrayLwtoff)
                {
                    WarnPrintf("Limiting array to print first 20 entries\n");
                    limit2 = m_printArrayLwtoff;
                }

                for(j=0; j<limit2; j++)
                    for(i=0; i<limit1; i++)
                    {
                        DecodeRegister(reg.get(), 2, i, j);
                    }
            }

            reg = m_regMap->GetRegisterNext();
        }
    }
}

UINT32 CRegTestGpu::LocalRegRd32(UINT32 address)
{
    UINT32 read_val;

    if(m_pParams->ParamPresent("-rma"))
    {
        InfoPrintf("RMA read address 0x%08x\n", address);
        read_val = RMARegRd32(address);
        InfoPrintf("RMA read address 0x%08x returned data 0x%08x\n", address, read_val);
    }
    else
    {
        read_val = lwgpu->GetGpuSubdevice()->RegRd32(address);
    }
    return(read_val);
}

void CRegTestGpu::LocalRegWr32(UINT32 address, UINT32 data)
{
    UINT32 read_val;
    DebugPrintf ("Enter LocalRegWr32\n");
    if(m_pParams->ParamPresent("-rma"))
    {
        if(m_pParams->ParamPresent("-trim_bug"))
        {
            DebugPrintf("RMA (trim_bug) write data 0x%08x to address 0x%08x\n", data, address);
            RMARegWr32(address, 0);
            read_val = RMARegRd32(address);
            RMARegWr32(address, read_val ^ data);
            DebugPrintf("RMA (trim_bug) write to address 0x%08x returned\n", address);
        }
        else
        {
            DebugPrintf("RMA write data 0x%08x to address 0x%08x\n", data, address);
            RMARegWr32(address, data);
            DebugPrintf("RMA write to address 0x%08x returned\n", address);
        }
    }
    else
    {
        GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
        if(m_pParams->ParamPresent("-trim_bug"))
        {
            pSubdev->RegWr32(address, 0);
            read_val = pSubdev->RegRd32(address);
            pSubdev->RegWr32(address, read_val ^ data);
            DebugPrintf ("### test goes in trim_bug way\n");
        }
        else
        {
            lwgpu->RegWr32(address, data);
            DebugPrintf ("write data 0x%08x to address 0x%08x\n", data, address);
        }
    }
}

UINT32 CRegTestGpu::RMARegRd32(UINT32 address)
{
    UINT32 readVal = 0;

    SaveLWAccessState(); // leaves RMA in addr mode
    Platform::PioWrite16(0x3d2, address >> 16); // set addr
    Platform::PioWrite16(0x3d0, address & 0xffff);
    Platform::PioWrite16(0x3d4, 0x0538); // put cr38 into data mode
    UINT16 dataLow = 0;
    UINT16 dataHigh = 0;
    Platform::PioRead16(0x3d2, &dataHigh); // read MSW first
    Platform::PioRead16(0x3d0, &dataLow); 
    readVal = dataLow | (dataHigh << 16);
    RestoreLWAccessState();

    return readVal;
}

void CRegTestGpu::RMARegWr32(UINT32 address, UINT32 data)
{
    SaveLWAccessState(); // leaves RMA in addr mode
    Platform::PioWrite16(0x3d2, address >> 16); // set addr
    Platform::PioWrite16(0x3d0, address & 0xffff);
    Platform::PioWrite16(0x3d4, 0x0738); // put cr38 into data mode
    Platform::PioWrite16(0x3d0, data & 0xffff);
    Platform::PioWrite16(0x3d2, data >> 16);
    RestoreLWAccessState();
}

void CRegTestGpu::SaveLWAccessState()
{
    // note: don't save value of 0x3d4 pointer
    Platform::PioWrite08(0x3d4, 0x38);
    Platform::PioRead08(0x3d5, &cr38Val); // save cr38 flags

    Platform::PioWrite16(0x3d4, 0x0738); // put cr38 into data mode
    Platform::PioRead16(0x3d0, &cr38LswOfWrite); // save LSW of data

    Platform::PioWrite16(0x3d4, 0x0338); // put cr38 into addr mode
    UINT16 dataLow = 0;
    UINT16 dataHigh = 0;
    Platform::PioRead16(0x3d0, &dataLow);  // save low word of addr
    Platform::PioRead16(0x3d2, &dataHigh); // save high word of addr
    cr38Addr = dataLow | (dataHigh << 16);
}

void CRegTestGpu::RestoreLWAccessState()
{
    Platform::PioWrite16(0x3d4, 0x0338); // put cr38 into addr mode
    Platform::PioWrite16(0x3d2, cr38Addr>>16); // write MSW of addr
    Platform::PioWrite16(0x3d0, cr38Addr & 0xffff); // write LSW of addr

    Platform::PioWrite16(0x3d4, 0x0738); // put cr38 into data mode
    Platform::PioWrite16(0x3d0, cr38LswOfWrite); // restore LSW of 32bit write data

    Platform::PioWrite16(0x3d4, (cr38Val << 8) | 0x38); // restore cr38 value
}

bool CRegTestGpu::AddrInRange(UINT32 addr)
{
    bool inr = true;

    vector<CRange*>::iterator regRangesLwr = m_regRanges.begin();
    while ( regRangesLwr != m_regRanges.end() )
    {
        inr = false;
        if((*regRangesLwr)->InRangeInclusive(addr))
        {
            inr = true;
            break;
        }
        ++regRangesLwr;
    }

    return inr;
}

UINT32 CRegTestGpu::PTVOSpecialTest()
{
    MASSERT(!"PTVOSpecialTest() is no longer supported");
    InfoPrintf("PTVOSpecialTest() is no longer supported");
    return 1;
}

UINT32 CRegTestGpu::TMDSSpecialTest()
{
    MASSERT(!"TMDSSpecialTest() is no longer supported");
    InfoPrintf("TMDSSpecialTest() is no longer supported");
    return 1;
}
