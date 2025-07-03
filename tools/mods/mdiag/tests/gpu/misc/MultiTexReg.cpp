/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2010, 2020-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Test to excercise multi-texture-pipe PRIV registers.  Access to registers
// in a specific texture pipeline is controlled by a routing register in
// the LW_PTPC_PRI_TEX_TRM_DBG register (Kepler) or LW_PTPC_PRI_TEX_M_ROUTING (Maxwell).
// This test reads/writes individual pipes to test this routing control.
//

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "MultiTexReg.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/sysspec.h"
#include "fermi/gf100/dev_master.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/massert.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/floorsweepimpl.h"
#include "mdiag/utils/cregctrl.h"

#include <string>
using std::string;

extern const ParamDecl multi_tex_reg_params[] =
{
    UNSIGNED_PARAM("-seed1", "Random stream seed for write of random value"),
    UNSIGNED_PARAM("-seed2", "Random stream seed for write of random value"),
    UNSIGNED_PARAM("-seed3", "Random stream seed for write of random value"),

    SIMPLE_PARAM("-verbose", "print out info as each register is written"),
    SIMPLE_PARAM("-check_floorsweep", "attempt to read the floorsweeping config to limit register testing"),

    PARAM_SUBDECL(lwgpu_single_params),

    LAST_PARAM
};

MultiTexReg::MultiTexReg(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
}

MultiTexReg::~MultiTexReg(void)
{
}

STD_TEST_FACTORY(multi_tex_reg, MultiTexReg)

int MultiTexReg::Setup(void)
{
    getStateReport()->enable();

    lwgpu = LWGpuResource::FindFirstResource();
    if (!lwgpu)
    {
        getStateReport()->setupFailed("Setup: Couldn't acquire a GPU!\n");
        return 0;
    }

    ch = lwgpu->CreateChannel();
    if (!ch)
    {
        ErrPrintf("MultiTexReg::Setup failed to create channel\n");
        return 0;
    }

    m_regMap = lwgpu->GetRegisterMap();
    if (!m_regMap)
    {
        ErrPrintf("MultiTexReg::Setup: can't get a register map from the GPU!\n");
        getStateReport()->setupFailed("Setup: can't get a register map from the GPU!\n");
        return 0;
    }

    m_seed1 = m_pParams->ParamUnsigned("-seed1", 0x1111);
    m_seed2 = m_pParams->ParamUnsigned("-seed2", 0x2222);
    m_seed3 = m_pParams->ParamUnsigned("-seed3", 0x3333);
    m_randomStream = new RandomStream(m_seed1, m_seed2, m_seed3);

    m_verbose = m_pParams->ParamPresent("-verbose");
    m_verbose = true;

    m_checkFloorsweep = m_pParams->ParamPresent("-check_floorsweep");

    // Create a list of texture registers to test - from manuals/pri_tex.ref
    // m_PerTpcRegisterNames.push_back("TEX_IN_DBG"); - ilwalidate trigger bits read as 0 after ilwalidate completes
    m_PerPipeRegisterNames.push_back("TEX_LOD_DBG");
    m_PerPipeRegisterNames.push_back("TEX_SAMP_DBG");
    m_PerPipeRegisterNames.push_back("TEX_SAMP_PM");
    m_PerPipeRegisterNames.push_back("TEX_X_DBG");
    m_PerPipeRegisterNames.push_back("TEX_W_DBG");
    m_PerPipeRegisterNames.push_back("TEX_T_DBG");
    m_PerPipeRegisterNames.push_back("TEX_T_PM");
    m_PerTpcRegisterNames.push_back("TEX_D_DBG");                   // shared across partitions in Turing L1 - bug 2041432
    m_PerPipeRegisterNames.push_back("TEX_M_ECC_CNT_TOTAL");
    m_PerPipeRegisterNames.push_back("TEX_M_ECC_CNT_UNIQUE");
    m_PerPipeRegisterNames.push_back("TEX_D_PM");
    // m_PerPipeRegisterNames.push_back("TEX_M_BLK_ACTIVITY_PRIV_LEVEL_MASK"); - won't behave same as any generic RW register, see bug 1673261
    m_PerTpcRegisterNames.push_back("TEX_M_DBG");                  // shared across partitions in Turing L1 - bug 2041432
    // m_PerTpcRegisterNames.push_back("TEX_M_DBG2"); - injection fields are write-only
    // m_PerTpcRegisterNames.push_back("TEX_T_DBG2_L1_DATA_ILWALIDATE"); - ilwalidate bits read as 0 after ilwalidate completes
    m_PerPipeRegisterNames.push_back("TEX_M_TEX_PIPE_STATUS");
    m_PerPipeRegisterNames.push_back("TEX_M_PM");
    m_PerPipeRegisterNames.push_back("TEX_M_HWW_ESR");
    m_PerPipeRegisterNames.push_back("TEX_M_HWW_ESR_REQ");
    m_PerPipeRegisterNames.push_back("TEX_M_HWW_ESR_ADDR");
    m_PerPipeRegisterNames.push_back("TEX_M_HWW_ESR_ADDR1");
    m_PerPipeRegisterNames.push_back("TEX_F_DBG");
    m_PerPipeRegisterNames.push_back("TEX_M_HWW_ESR_MMU");
    m_PerPipeRegisterNames.push_back("TEX_M_TEX_SUBUNITS_STATUS");
    m_PerPipeRegisterNames.push_back("TEX_PMISD");
    // m_PerPipeRegisterNames.push_back("TEX_M_BLK_ACT_INDEX"); - increments automatically and can only be written to 0
    // m_PerPipeRegisterNames.push_back("TEX_M_BLK_ACT_DATA"); - data reads incrementally after index written...
    m_PerTpcRegisterNames.push_back("TEX_RM_CB_0");            // shared across partitions in Turing L1 - bug 2041432
    m_PerTpcRegisterNames.push_back("TEX_RM_CB_1");            // shared across partitions in Turing L1 - bug 2041432

    // Clock gating - found in manuals/<litter>/pri_tex_blcg.ref
    m_PerPipeRegisterNames.push_back("TEX_IN_CG");
    m_PerPipeRegisterNames.push_back("TEX_IN_CG1");
    m_PerPipeRegisterNames.push_back("TEX_SAMP_CG");
    m_PerPipeRegisterNames.push_back("TEX_SAMP_CG1");
    m_PerPipeRegisterNames.push_back("TEX_X_CG");
    m_PerPipeRegisterNames.push_back("TEX_X_CG1");
    m_PerPipeRegisterNames.push_back("TEX_W_CG");
    m_PerPipeRegisterNames.push_back("TEX_W_CG1");
    m_PerPipeRegisterNames.push_back("TEX_D_CG");
    m_PerPipeRegisterNames.push_back("TEX_D_CG1");
    m_PerPipeRegisterNames.push_back("TEX_T_CG");
    m_PerPipeRegisterNames.push_back("TEX_T_CG1");
    m_PerPipeRegisterNames.push_back("TEX_M_CG");
    m_PerPipeRegisterNames.push_back("TEX_M_CG1");
    m_PerPipeRegisterNames.push_back("TEX_F_CG");
    m_PerPipeRegisterNames.push_back("TEX_F_CG1");
    m_PerPipeRegisterNames.push_back("TEX_MIPB_CG");
    m_PerPipeRegisterNames.push_back("TEX_MIPB_CG1");

    // New regs in Maxwell
    m_PerPipeRegisterNames.push_back("TEX_IN_POWER");
    m_PerPipeRegisterNames.push_back("TEX_X_POWER");
    m_PerPipeRegisterNames.push_back("TEX_D_POWER");

    // Compute the number of texture pipes per TPC in the chip by counting the values
    // in the TEX_TRM_DBG routing field (Kepler) or in the TEX_M_ROUTING field (Maxwell).
    //
    unique_ptr<IRegisterClass> reg1 = m_regMap->FindRegister("LW_PGRAPH_PRI_GPC0_TPC0_TEX_TRM_DBG");
    unique_ptr<IRegisterClass> reg2 = m_regMap->FindRegister("LW_PGRAPH_PRI_GPC0_TPC0_TEX_M_ROUTING");
    if (!reg1 && !reg2)
    {
        ErrPrintf("MultiTexReg::Setup: can't find register LW_PGRAPH_PRI_GPC0_TPC0_TEX_TRM_DBG and LW_PGRAPH_PRI_GPC0_TPC0_TEX_M_ROUTING!\n");
        getStateReport()->setupFailed("Setup: can't find register LW_PGRAPH_PRI_GPC0_TPC0_TEX_TRM_DBG and LW_PGRAPH_PRI_GPC0_TPC0_TEX_M_ROUTING!\n");
        return 0;
    }
    if (reg1 && reg2)
    {
        ErrPrintf("MultiTexReg::Setup: registers LW_PGRAPH_PRI_GPC0_TPC0_TEX_TRM_DBG and LW_PGRAPH_PRI_GPC0_TPC0_TEX_M_ROUTING can't both exist!\n");
        getStateReport()->setupFailed("Setup: registers LW_PGRAPH_PRI_GPC0_TPC0_TEX_TRM_DBG and LW_PGRAPH_PRI_GPC0_TPC0_TEX_M_ROUTING can't both exist!\n");
        return 0;
    }

    unique_ptr<IRegisterField> field;
    if (reg1)
    {
        field = reg1->FindField("LW_PGRAPH_PRI_GPC0_TPC0_TEX_TRM_DBG_ROUTING");
        m_TexPipeRoutingRegName = "TEX_TRM_DBG";
        m_TexPipeRoutingRegFieldName = "_ROUTING";
    }
    else
    {
        field = reg2->FindField("LW_PGRAPH_PRI_GPC0_TPC0_TEX_M_ROUTING_SEL");
        m_TexPipeRoutingRegName = "TEX_M_ROUTING";
        m_TexPipeRoutingRegFieldName = "_SEL";
    }
    if (!field)
    {
        ErrPrintf("MultiTexReg::Setup: can't find field LW_PGRAPH_PRI_GPC0_TPC0_TEX_TRM_DBG_ROUTING or LW_PGRAPH_PRI_GPC0_TPC0_TEX_M_ROUTING_SEL!\n");
        getStateReport()->setupFailed("Setup: can't find field LW_PGRAPH_PRI_GPC0_TPC0_TEX_TRM_DBG_ROUTING or LW_PGRAPH_PRI_GPC0_TPC0_TEX_M_ROUTING_SEL!\n");
        return 0;
    }
    m_NumTexPipesPerTpc = 0;
    for (unique_ptr<IRegisterValue> val = field->GetValueHead(); val; val = field->GetValueNext())
    {
        m_NumTexPipesPerTpc += 1;
    }
    m_NumTexPipesPerTpc -= 1;       // Subtract one for the DEFAULT (broadcast) value
    MASSERT(m_NumTexPipesPerTpc > 0);

    // Number of pipes per TPC should be even and paired 2:1 with TRMs
    MASSERT(! (m_NumTexPipesPerTpc & 0x1));
    m_NumTrmPerTpc = m_NumTexPipesPerTpc / 2;

    return 1;
}

void MultiTexReg::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("MultiTexReg::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }

    if (m_randomStream)
    {
        delete m_randomStream;
        m_randomStream = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

string MultiTexReg::MakeTexRegName(string base_name, int const gpc_id, int const tpc_id)
{
    return Utility::StrPrintf("LW_PGRAPH_PRI_GPC%d_TPC%d_%s", gpc_id, tpc_id, base_name.c_str());
}

UINT32 MultiTexReg::TestPerPipeRegs(void)
{
    UINT32 error_status = 0;
    UINT32 * const orig_vals = new UINT32[m_NumTexPipesPerTpc];
    UINT32 * const rand_vals = new UINT32[m_NumTexPipesPerTpc];

    for (vector<string>::iterator base_name = m_PerPipeRegisterNames.begin(); base_name != m_PerPipeRegisterNames.end(); base_name++)
    {
        for (INT32 gpc_id=0; gpc_id < Utility::CountBits(m_GpcMask); gpc_id++)
        {
            UINT32 phys_gpc_id;
            lwgpu->GetGpuSubdevice()->VirtualGpcToHwGpc(gpc_id, &phys_gpc_id);
            UINT32 tpc_mask = lwgpu->GetGpuSubdevice()->GetFsImpl()->TpcMask(phys_gpc_id);
            MASSERT(tpc_mask != 0);
            InfoPrintf("Testing virtual GPC %d with virtual TPC mask of 0x%x, register %s\n", gpc_id, tpc_mask, base_name->c_str());
            for (INT32 tpc_id=0; tpc_id < Utility::CountBits(tpc_mask); tpc_id++)
            {
                string test_reg_name = MakeTexRegName(*base_name, gpc_id, tpc_id);
                InfoPrintf("Testing register %s\n", test_reg_name.c_str());
                unique_ptr<IRegisterClass> test_reg = m_regMap->FindRegister(test_reg_name.c_str());

                if (! test_reg)
                {
                    InfoPrintf("Warning: unknown register %s\n", test_reg_name.c_str());
                    continue;
                }
                if (! test_reg->isReadable())
                {
                    InfoPrintf("Warning: register %s is not readable - skipping\n", test_reg->GetName());
                    continue;
                }
                if (! test_reg->isWriteable())
                {
                    InfoPrintf("Warning: register %s is not writeable - skipping\n", test_reg->GetName());
                    continue;
                }

                string route_reg_name = MakeTexRegName(m_TexPipeRoutingRegName.c_str(), gpc_id, tpc_id);
                unique_ptr<IRegisterClass> route_reg = m_regMap->FindRegister(route_reg_name.c_str());
                MASSERT(route_reg);

                // Save the current value of the register for all texture pipes
                SaveRegisters(test_reg.get(), route_reg.get(), orig_vals);

                // First broadcast a random value to all pipes
                SelectTexturePipe(route_reg.get(), -1);   // select broadcast mode
                rand_vals[0] = m_randomStream->RandomUnsigned(0x0, 0xFFFFFFFF);
                MyRegWr32(test_reg->GetAddress(), rand_vals[0]);
                if (m_verbose)
                    InfoPrintf("Broadcast test for register %s: 0x%.8x\n", test_reg->GetName(), rand_vals[0]);

                // Verify the results
                for (UINT32 tex_id=0; tex_id < m_NumTexPipesPerTpc; tex_id++)
                {
                    UINT32 expected = ComputeExpectedReadValue(test_reg.get(), orig_vals[tex_id], rand_vals[0]);
                    SelectTexturePipe(route_reg.get(), tex_id);
                    UINT32 actual = MyRegRd32(test_reg->GetAddress());
                    if (actual != expected)
                    {
                        InfoPrintf("Broadcast failure: pipe=%d original=0x%.8x write=0x%.8x expected=0x%.8x read=0x%.8x  %s\n",
                            tex_id, orig_vals[tex_id], rand_vals[0], expected, actual, test_reg->GetName());
                        error_status |= 1;
                    }
                }

                // Restore and save for next phase of testing
                RestoreRegisters(test_reg.get(), route_reg.get(), orig_vals);
                SaveRegisters(test_reg.get(), route_reg.get(), orig_vals);

                // Now write each register with a unique value
                for (UINT32 tex_id=0; tex_id < m_NumTexPipesPerTpc; tex_id++)
                {
                    SelectTexturePipe(route_reg.get(), tex_id);
                    rand_vals[tex_id] = m_randomStream->RandomUnsigned(0x0, 0xFFFFFFFF);
                    MyRegWr32(test_reg->GetAddress(), rand_vals[tex_id]);
                    if (m_verbose)
                        InfoPrintf("Unique test for register %s pipe %d: 0x%.8x\n", test_reg->GetName(), tex_id, rand_vals[tex_id]);
                }

                // Verify the results
                for (UINT32 tex_id=0; tex_id < m_NumTexPipesPerTpc; tex_id++)
                {
                    UINT32 expected = ComputeExpectedReadValue(test_reg.get(), orig_vals[tex_id], rand_vals[tex_id]);
                    SelectTexturePipe(route_reg.get(), tex_id);
                    UINT32 actual = MyRegRd32(test_reg->GetAddress());
                    if (actual != expected)
                    {
                        InfoPrintf("Unique failure: pipe=%d original=0x%.8x write=0x%.8x expected=0x%.8x read=0x%.8x  %s\n",
                            tex_id, orig_vals[tex_id], rand_vals[tex_id], expected, actual, test_reg->GetName());
                        error_status |= 1;
                    }
                }

                // Restore the original register values to each texture pipe
                RestoreRegisters(test_reg.get(), route_reg.get(), orig_vals);
            }
        }
    }

    delete rand_vals;
    delete orig_vals;

    return error_status;
}

UINT32 MultiTexReg::TestPerTpcRegs(void)
{
    UINT32 error_status = 0;
    UINT32 * const orig_vals = new UINT32[m_NumTexPipesPerTpc];
    UINT32 * const rand_vals = new UINT32[m_NumTexPipesPerTpc];

    for (vector<string>::iterator base_name = m_PerTpcRegisterNames.begin(); base_name != m_PerTpcRegisterNames.end(); base_name++)
    {
        for (INT32 gpc_id=0; gpc_id < Utility::CountBits(m_GpcMask); gpc_id++)
        {
            UINT32 phys_gpc_id;
            lwgpu->GetGpuSubdevice()->VirtualGpcToHwGpc(gpc_id, &phys_gpc_id);
            UINT32 tpc_mask = lwgpu->GetGpuSubdevice()->GetFsImpl()->TpcMask(phys_gpc_id);
            MASSERT(tpc_mask != 0);
            InfoPrintf("Testing virtual GPC %d with virtual TPC mask of 0x%x, register %s\n", gpc_id, tpc_mask, base_name->c_str());
            for (INT32 tpc_id=0; tpc_id < Utility::CountBits(tpc_mask); tpc_id++)
            {
                string test_reg_name = MakeTexRegName(*base_name, gpc_id, tpc_id);
                InfoPrintf("Testing register %s\n", test_reg_name.c_str());
                unique_ptr<IRegisterClass> test_reg = m_regMap->FindRegister(test_reg_name.c_str());

                if (! test_reg)
                {
                    InfoPrintf("Warning: unknown register %s\n", test_reg_name.c_str());
                    continue;
                }
                if (! test_reg->isReadable())
                {
                    InfoPrintf("Warning: register %s is not readable - skipping\n", test_reg->GetName());
                    continue;
                }
                if (! test_reg->isWriteable())
                {
                    InfoPrintf("Warning: register %s is not writeable - skipping\n", test_reg->GetName());
                    continue;
                }

                string route_reg_name = MakeTexRegName(m_TexPipeRoutingRegName.c_str(), gpc_id, tpc_id);
                unique_ptr<IRegisterClass> route_reg = m_regMap->FindRegister(route_reg_name.c_str());
                MASSERT(route_reg);

                // Save the current value of the register for all texture pipes
                SaveRegisters(test_reg.get(), route_reg.get(), orig_vals);

                // First broadcast a random value to all pipes
                SelectTexturePipe(route_reg.get(), -1);   // select broadcast mode
                rand_vals[0] = m_randomStream->RandomUnsigned(0x0, 0xFFFFFFFF);
                MyRegWr32(test_reg->GetAddress(), rand_vals[0]);
                if (m_verbose)
                    InfoPrintf("Broadcast test for register %s: 0x%.8x\n", test_reg->GetName(), rand_vals[0]);

                // Verify the results
                for (UINT32 trm_id=0; trm_id < m_NumTrmPerTpc; trm_id++)
                {
                    UINT32 tex_id = trm_id*2+1;     // Two pipes share a TRM, only check the odd pipes
                    UINT32 expected = ComputeExpectedReadValue(test_reg.get(), orig_vals[tex_id], rand_vals[0]);
                    SelectTexturePipe(route_reg.get(), tex_id);
                    UINT32 actual = MyRegRd32(test_reg->GetAddress());
                    if (actual != expected)
                    {
                        InfoPrintf("Broadcast failure: TRM=%d original=0x%.8x write=0x%.8x expected=0x%.8x read=0x%.8x  %s\n",
                            trm_id, orig_vals[tex_id], rand_vals[0], expected, actual, test_reg->GetName());
                        error_status |= 1;
                    }
                }

                //
                // We don't test writing unique values to per-TPC registers.  These registers are broadcast
                // to all TRMs, which was tested above.
                //

                // Restore the original register values to each texture pipe
                RestoreRegisters(test_reg.get(), route_reg.get(), orig_vals);
            }
        }
    }

    delete rand_vals;
    delete orig_vals;

    return error_status;
}

void MultiTexReg::Run(void)
{
    // This access makes sure the pipe is idle before testing the registers
    //
    unique_ptr<IRegisterClass> PmcEnableReg = m_regMap->FindRegister("LW_PMC_ENABLE");
    MASSERT(PmcEnableReg != 0);
    const UINT32 PmcEnable = MyRegRd32(PmcEnableReg->GetAddress());
    if (DRF_VAL(_PMC, _ENABLE, _PGRAPH, PmcEnable) == LW_PMC_ENABLE_PGRAPH_ENABLED)
    {
        unique_ptr<IRegisterClass> status_reg = m_regMap->FindRegister("LW_PGRAPH_STATUS");
        UINT32 tmpVal = MyRegRd32(status_reg->GetAddress());
        tmpVal = tmpVal & 0x00000001;
        while(tmpVal)
        {
            tmpVal = MyRegRd32(status_reg->GetAddress());
            tmpVal = tmpVal & 0x00000001;
            Platform::ClockSimulator(64);
        }
    }

    m_checkFloorsweep = 1;

    m_NumGpc = lwgpu->GetGpuSubdevice()->GetGpcCount();
    MASSERT(m_NumGpc > 0);
    m_GpcMask = (1<<m_NumGpc)-1;
    if (m_checkFloorsweep) m_GpcMask = lwgpu->GetGpuSubdevice()->GetFsImpl()->GpcMask();
    MASSERT(m_GpcMask != 0);

    m_NumTpc = lwgpu->GetGpuSubdevice()->GetTpcCount();
    MASSERT(m_NumTpc > 0);

    if (m_verbose) InfoPrintf("GPCs = %d, TPCs = %d, TEXs = %d\n", m_NumGpc, m_NumTpc, m_NumTexPipesPerTpc);

    UINT32 error_status = 0;
    error_status |= TestPerPipeRegs();
    error_status |= TestPerTpcRegs();

    if (error_status)
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

UINT32 MultiTexReg::ComputeExpectedReadValue(IRegisterClass * const reg, UINT32 old_val, UINT32 new_val)
{
    UINT32 expected = new_val;
    UINT32 write_mask = reg->GetWriteMask();
    UINT32 clear_write_one_mask = reg->GetW1ClrMask();
    UINT32 read_mask = reg->GetReadMask();
    UINT32 const_mask = reg->GetConstMask();

    if (m_verbose) InfoPrintf("ComputeExpectedReadValue: Initial expected: 0x%08x  %s\n",expected, reg->GetName());

    // apply the write mask from the manuals
    if (write_mask != ~(UINT32)0)
    {
        expected &= write_mask;
        expected |= old_val & ~write_mask;
        if (m_verbose)
            InfoPrintf("ComputeExpectedReadValue: Manuals Write Mask Adjustment: mask=0x%08x oldval=0x%08x newexp=0x%08x  %s\n",
                        write_mask, old_val, expected, reg->GetName());
    }

    // apply the clear-on-write1 mask
    if (clear_write_one_mask)
    {
        expected &= ~clear_write_one_mask;
        expected |= old_val & clear_write_one_mask;
        expected &= ~(new_val & clear_write_one_mask);
        if (m_verbose)
            InfoPrintf("ComputeExpectedReadValue: Manuals Clear on Write1 Mask Adjustment: mask=0x%08x oldval=0x%08x newval=0x%08x newexp=0x%08x  %s\n",
                        clear_write_one_mask, old_val, new_val, expected, reg->GetName());
    }

    // apply the read mask
    if (read_mask != ~(UINT32)0)
    {
        expected &= read_mask;
        if (m_verbose)
            InfoPrintf("ComputeExpectedReadValue: Manuals Read Mask Adjustment: mask=0x%08x newexp=0x%08x  %s\n",
                        read_mask, expected, reg->GetName());
    }

    // apply the constant mask and value from the manuals
    if (const_mask)
    {
        UINT32 const_val = reg->GetConstValue();
        expected &= ~const_mask;
        expected |= const_val;
        if (m_verbose)
            InfoPrintf("ComputeExpectedReadValue: Manuals Constant Mask Adjustment: cmask=0x%08x cval=0x%08x newexp=0x%08x  %s\n",
                        const_mask, const_val, expected, reg->GetName());
    }

    if (m_verbose) InfoPrintf("ComputeExpectedReadValue: Final expected value: 0x%08x  %s\n", expected, reg->GetName());

    return expected;
}

void MultiTexReg::SelectTexturePipe(IRegisterClass * const route_reg, int tex_id)
{
    string field_name(route_reg->GetName());
    field_name += m_TexPipeRoutingRegFieldName.c_str();
    unique_ptr<IRegisterField> field = route_reg->FindField(field_name.c_str());
    MASSERT(field);
    UINT32 lo_bit = field->GetStartBit();
    UINT32 hi_bit = field->GetEndBit();
    UINT32 mask = ((1<<(hi_bit-lo_bit+1))-1) << lo_bit;

    UINT32 tmp = MyRegRd32(route_reg->GetAddress());
    tmp &= ~mask;
    if (tex_id == -1)       // Broadcast to all texture pipes
    {
        // Do nothing - the zeroed field selects broadcast
        if (m_verbose)
            InfoPrintf("Selecting broadcast mode for register %s: 0x%.8x\n", route_reg->GetName(), tmp);
    }
    else
    {
        // Individual pipes are selected starting with value 1
        tmp |= (tex_id+1)<<(lo_bit);

        if (m_verbose)
            InfoPrintf("Selecting pipe %d for register %s: 0x%.8x\n", tex_id, route_reg->GetName(), tmp);
    }
    MyRegWr32(route_reg->GetAddress(), tmp);
}

void MultiTexReg::SaveRegisters(IRegisterClass * const test_reg, IRegisterClass * const route_reg, UINT32 * vals)
{
    for (UINT32 tex_id=0; tex_id < m_NumTexPipesPerTpc; tex_id++) {
        SelectTexturePipe(route_reg, tex_id);
        UINT32 addr = test_reg->GetAddress();
        vals[tex_id] = MyRegRd32(addr);

        if (m_verbose)
            InfoPrintf("Saving register %s for pipe %d: 0x%.8x\n", test_reg->GetName(), tex_id, vals[tex_id]);
    }
}

void MultiTexReg::RestoreRegisters(IRegisterClass * const test_reg, IRegisterClass * const route_reg, UINT32 * vals)
{
    for (UINT32 tex_id=0; tex_id < m_NumTexPipesPerTpc; tex_id++) {
        SelectTexturePipe(route_reg, tex_id);
        MyRegWr32(test_reg->GetAddress(), vals[tex_id]);

        if (m_verbose)
            InfoPrintf("Restoring register %s: 0x%.8x\n", test_reg->GetName(), vals[tex_id]);
    }
}

UINT32 MultiTexReg::MyRegRd32(UINT32 address)
{
    return lwgpu->GetGpuSubdevice()->RegRd32(address);
}

void MultiTexReg::MyRegWr32(UINT32 address, UINT32 data)
{
    lwgpu->GetGpuSubdevice()->RegWr32(address, data);
}
