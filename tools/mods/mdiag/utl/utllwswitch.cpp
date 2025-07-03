/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/mgrmgr.h"
#include "core/include/rc.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_lwlink_libif_user.h"
#include "mdiag/IRegisterMap.h"
#include "core/include/refparse.h"

#include "mdiag/sysspec.h"

#include "utl.h"
#include "utlutil.h"

#include "utllwswitch.h"

#if LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
#define LW_PMC_BOOT_42_CHIP_ID_S000  0x4
#define LW_PMC_BOOT_42_CHIP_ID_LS10  0x7
#endif
unique_ptr<IRegisterMap> CreateRegisterMap(const RefManual *Manual);

vector<UtlLwSwitch::LwSwitchPtr> UtlLwSwitch::s_LwSwitches;
vector<UtlLwSwitch*> UtlLwSwitch::s_PythonLwSwitches;

void UtlLwSwitch::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("LwSwitch.Control", "command", true, "command to be exelwted");
    kwargs->RegisterKwarg("LwSwitch.Control", "params", true, "parameters for the exelwted command");
    kwargs->RegisterKwarg("LwSwitch.GetRegCtrl", "instance", true, "target instance");
    kwargs->RegisterKwarg("LwSwitch.GetRegCtrl", "engine", true, "target engine");
    kwargs->RegisterKwarg("LwSwitchRegCtrl.SetEngine", "engine", true, "target engine");
    kwargs->RegisterKwarg("LwSwitchRegCtrl.SetEngine", "instance", true, "target instance");

    UtlLwSwitch::PythonLwSwitch lwSwitch(module, "LwSwitch");
    lwSwitch.def_static("GetLwSwitches", &UtlLwSwitch::GetLwSwitches,
        "This function returns a list of all LwSwitch objects.");
    lwSwitch.def("Control", &UtlLwSwitch::ControlPy,
        UTL_DOCSTRING("LwSwitch.Control", "This function makes a control call to the LwSwitch driver."));
    lwSwitch.def("GetRegCtrl", &UtlLwSwitch::GetRegCtrl,
        UTL_DOCSTRING("LwSwitch.GetRegCtrl", "This function returns the Register controller associated with the LwSwitch."),
        py::return_value_policy::take_ownership);

    py::enum_<UtlLwSwitch::ControlCommand>(lwSwitch, "ControlCommand",
        "An enum containing a selection of LwSwitch control command structs supported by UTL")
        .value("GET_INFO", UtlLwSwitch::ControlCommand::GET_INFO)
        .value("SET_SWITCH_PORT_CONFIG", UtlLwSwitch::ControlCommand::SET_SWITCH_PORT_CONFIG)
        .value("GET_INTERNAL_LATENCY", UtlLwSwitch::ControlCommand::GET_INTERNAL_LATENCY)
        .value("SET_LATENCY_BINS", UtlLwSwitch::ControlCommand::SET_LATENCY_BINS)
        .value("GET_LWLIPT_COUNTERS", UtlLwSwitch::ControlCommand::GET_LWLIPT_COUNTERS)
        .value("SET_LWLIPT_COUNTER_CONFIG", UtlLwSwitch::ControlCommand::SET_LWLIPT_COUNTER_CONFIG)
        .value("GET_LWLIPT_COUNTER_CONFIG", UtlLwSwitch::ControlCommand::GET_LWLIPT_COUNTER_CONFIG)
        .value("SET_REMAP_POLICY", UtlLwSwitch::ControlCommand::SET_REMAP_POLICY)
        .value("SET_ROUTING_ID", UtlLwSwitch::ControlCommand::SET_ROUTING_ID)
        .value("SET_ROUTING_LAN", UtlLwSwitch::ControlCommand::SET_ROUTING_LAN)
        .value("SET_MC_RID_TABLE", UtlLwSwitch::ControlCommand::SET_MC_RID_TABLE);

    UtlLwSwitch::PythonControlParams controlParams(lwSwitch, "ControlParams");

    UtlLwSwitch::PythonControlStruct(controlParams, "ControlStruct")
        .def("__str__", &UtlLwSwitch::ControlStruct::ToString,
            "This function returns the string representation of this struct.");

    // BindIntegralArray<LwU8> covers both LwU8 and LwBool. LwBool is an alias of LwU8.
    UtlLwSwitch::BindIntegralArray<LwU8>(controlParams, "LwU8");
    UtlLwSwitch::BindIntegralArray<LwU32>(controlParams, "LwU32");
    UtlLwSwitch::BindStructArray<LWSWITCH_REMAP_POLICY_ENTRY>(controlParams, "REMAP_POLICY_ENTRY");
    UtlLwSwitch::BindStructArray<LWSWITCH_ROUTING_ID_DEST_PORT_LIST>(controlParams, "ROUTING_ID_DEST_PORT_LIST");
    UtlLwSwitch::BindStructArray<LWSWITCH_ROUTING_ID_ENTRY>(controlParams, "ROUTING_ID_ENTRY");
    UtlLwSwitch::BindStructArray<LWSWITCH_ROUTING_LAN_PORT_SELECT>(controlParams, "ROUTING_LAN_PORT_SELECT");
    UtlLwSwitch::BindStructArray<LWSWITCH_ROUTING_LAN_ENTRY>(controlParams, "ROUTING_LAN_ENTRY");
    UtlLwSwitch::BindStructArray<LWSWITCH_INTERNAL_LATENCY_BINS>(controlParams, "INTERNAL_LATENCY_BINS");
    UtlLwSwitch::BindStructArray<LWSWITCH_LATENCY_BIN>(controlParams, "LATENCY_BIN");
    UtlLwSwitch::BindStructArray<LWSWITCH_LWLIPT_COUNTER>(controlParams, "LWLIPT_COUNTER");

    UtlLwSwitch::BindStruct<LWSWITCH_GET_INFO>(controlParams, "GET_INFO");
    UtlLwSwitch::BindStruct<LWSWITCH_REMAP_POLICY_ENTRY>(controlParams, "REMAP_POLICY_ENTRY");
    UtlLwSwitch::BindStruct<LWSWITCH_SET_REMAP_POLICY>(controlParams, "SET_REMAP_POLICY");
    UtlLwSwitch::BindStruct<LWSWITCH_ROUTING_ID_DEST_PORT_LIST>(controlParams, "ROUTING_ID_DEST_PORT_LIST");
    UtlLwSwitch::BindStruct<LWSWITCH_ROUTING_ID_ENTRY>(controlParams, "ROUTING_ID_ENTRY");
    UtlLwSwitch::BindStruct<LWSWITCH_SET_ROUTING_ID>(controlParams, "SET_ROUTING_ID");
    UtlLwSwitch::BindStruct<LWSWITCH_ROUTING_LAN_PORT_SELECT>(controlParams, "ROUTING_LAN_PORT_SELECT");
    UtlLwSwitch::BindStruct<LWSWITCH_ROUTING_LAN_ENTRY>(controlParams, "ROUTING_LAN_ENTRY");
    UtlLwSwitch::BindStruct<LWSWITCH_SET_ROUTING_LAN>(controlParams, "SET_ROUTING_LAN");
    UtlLwSwitch::BindStruct<LWSWITCH_INTERNAL_LATENCY_BINS>(controlParams, "INTERNAL_LATENCY_BINS");
    UtlLwSwitch::BindStruct<LWSWITCH_GET_INTERNAL_LATENCY>(controlParams, "GET_INTERNAL_LATENCY");
    UtlLwSwitch::BindStruct<LWSWITCH_LATENCY_BIN>(controlParams, "LATENCY_BIN");
    UtlLwSwitch::BindStruct<LWSWITCH_SET_LATENCY_BINS>(controlParams, "SET_LATENCY_BINS");
    UtlLwSwitch::BindStruct<LWSWITCH_SET_SWITCH_PORT_CONFIG>(controlParams, "SET_SWITCH_PORT_CONFIG");
    UtlLwSwitch::BindStruct<LWSWITCH_LWLIPT_COUNTER>(controlParams, "LWLIPT_COUNTER");
    UtlLwSwitch::BindStruct<LWSWITCH_GET_LWLIPT_COUNTERS>(controlParams, "GET_LWLIPT_COUNTERS");
    UtlLwSwitch::BindStruct<LWLIPT_COUNTER_CONFIG>(controlParams, "LWLIPT_COUNTER_CONFIG");
    UtlLwSwitch::BindStruct<LWSWITCH_SET_LWLIPT_COUNTER_CONFIG>(controlParams, "SET_LWLIPT_COUNTER_CONFIG");
    UtlLwSwitch::BindStruct<LWSWITCH_GET_LWLIPT_COUNTER_CONFIG>(controlParams, "GET_LWLIPT_COUNTER_CONFIG");
    UtlLwSwitch::BindStruct<LWSWITCH_SET_MC_RID_TABLE_PARAMS>(controlParams, "SET_MC_RID_TABLE_PARAMS");

    // LwSwitch defines
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_COUNT_MAX, "GET_INFO_COUNT_MAX");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_ARCH_SV10, "GET_INFO_INDEX_ARCH_SV10");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_IMPL_SV10, "GET_INFO_INDEX_IMPL_SV10");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_ARCH_LR10, "GET_INFO_INDEX_ARCH_LR10");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_IMPL_LR10, "GET_INFO_INDEX_IMPL_LR10");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_REMAP_POLICY_ENTRIES_MAX, "REMAP_POLICY_ENTRIES_MAX");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_REMAP_POLICY_FLAGS_REMAP_ADDR, "REMAP_POLICY_FLAGS_REMAP_ADDR");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_REMAP_POLICY_FLAGS_REQCTXT_CHECK, "REMAP_POLICY_FLAGS_REQCTXT_CHECK");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_REMAP_POLICY_FLAGS_REQCTXT_REPLACE, "REMAP_POLICY_FLAGS_REQCTXT_REPLACE");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_REMAP_POLICY_FLAGS_ADR_BASE, "REMAP_POLICY_FLAGS_ADR_BASE");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_REMAP_POLICY_FLAGS_ADR_OFFSET, "REMAP_POLICY_FLAGS_ADR_OFFSET");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_ROUTING_ID_DEST_PORT_LIST_MAX, "ROUTING_ID_DEST_PORT_LIST_MAX");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_ROUTING_ID_ENTRIES_MAX, "ROUTING_ID_ENTRIES_MAX");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_ROUTING_LAN_GROUP_SEL_MAX, "ROUTING_LAN_GROUP_SEL_MAX");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_ROUTING_LAN_ENTRIES_MAX, "ROUTING_LAN_ENTRIES_MAX");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_MAX_PORTS, "MAX_PORTS");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_MAX_VCS, "MAX_VCS");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_MC_MAX_PORTS, "MC_MAX_PORTS");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_MC_MAX_SPRAYGROUPS, "MC_MAX_SPRAYGROUPS");
    // LwSwitch RW engine defines
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_RAW, "REGISTER_RW_ENGINE_RAW");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_CLKS, "REGISTER_RW_ENGINE_CLKS");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_FUSE, "REGISTER_RW_ENGINE_FUSE");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_JTAG, "REGISTER_RW_ENGINE_JTAG");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_PMGR, "REGISTER_RW_ENGINE_PMGR");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_SAW, "REGISTER_RW_ENGINE_SAW");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_XP3G, "REGISTER_RW_ENGINE_XP3G");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_XVE, "REGISTER_RW_ENGINE_XVE");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_SOE, "REGISTER_RW_ENGINE_SOE");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_SE, "REGISTER_RW_ENGINE_SE");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_SIOCTRL, "REGISTER_RW_ENGINE_SIOCTRL");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_MINION, "REGISTER_RW_ENGINE_MINION");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_LWLIPT, "REGISTER_RW_ENGINE_LWLIPT");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_LWLTLC, "REGISTER_RW_ENGINE_LWLTLC");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_DLPL, "REGISTER_RW_ENGINE_DLPL");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_TX_PERFMON, "REGISTER_RW_ENGINE_TX_PERFMON");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_RX_PERFMON, "REGISTER_RW_ENGINE_RX_PERFMON");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_LWLW, "REGISTER_RW_ENGINE_LWLW");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_LWLIPT_LNK, "REGISTER_RW_ENGINE_LWLIPT_LNK");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_LWLDL, "REGISTER_RW_ENGINE_LWLDL");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_PLL, "REGISTER_RW_ENGINE_PLL");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_NPG, "REGISTER_RW_ENGINE_NPG");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_NPORT, "REGISTER_RW_ENGINE_NPORT");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_NPG_PERFMON, "REGISTER_RW_ENGINE_NPG_PERFMON");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_NPORT_PERFMON, "REGISTER_RW_ENGINE_NPORT_PERFMON");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_SWX, "REGISTER_RW_ENGINE_SWX");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_SWX_PERFMON, "REGISTER_RW_ENGINE_SWX_PERFMON");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_AFS, "REGISTER_RW_ENGINE_AFS");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_AFS_PERFMON, "REGISTER_RW_ENGINE_AFS_PERFMON");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_NXBAR, "REGISTER_RW_ENGINE_NXBAR");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_NXBAR_PERFMON, "REGISTER_RW_ENGINE_NXBAR_PERFMON");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_TILE, "REGISTER_RW_ENGINE_TILE");
    UTL_BIND_CONSTANT(controlParams, REGISTER_RW_ENGINE_TILE_PERFMON, "REGISTER_RW_ENGINE_TILE_PERFMON");

    // LwSwitch enums
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_ARCH, "GET_INFO_INDEX_ARCH");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_IMPL, "GET_INFO_INDEX_IMPL");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_CHIPID, "GET_INFO_INDEX_CHIPID");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_REVISION_MAJOR, "GET_INFO_INDEX_REVISION_MAJOR");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_REVISION_MINOR, "GET_INFO_INDEX_REVISION_MINOR");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_REVISION_MINOR_EXT, "GET_INFO_INDEX_REVISION_MINOR_EXT");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_FOUNDRY, "GET_INFO_INDEX_FOUNDRY");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_FAB, "GET_INFO_INDEX_FAB");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_LOT_CODE_0, "GET_INFO_INDEX_LOT_CODE_0");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_LOT_CODE_1, "GET_INFO_INDEX_LOT_CODE_1");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_WAFER, "GET_INFO_INDEX_WAFER");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_XCOORD, "GET_INFO_INDEX_XCOORD");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_YCOORD, "GET_INFO_INDEX_YCOORD");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_SPEEDO_REV, "GET_INFO_INDEX_SPEEDO_REV");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_SPEEDO0, "GET_INFO_INDEX_SPEEDO0");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_SPEEDO1, "GET_INFO_INDEX_SPEEDO1");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_SPEEDO2, "GET_INFO_INDEX_SPEEDO2");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_IDDQ, "GET_INFO_INDEX_IDDQ");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_IDDQ_REV, "GET_INFO_INDEX_IDDQ_REV");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_ATE_REV, "GET_INFO_INDEX_ATE_REV");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_VENDOR_CODE, "GET_INFO_INDEX_VENDOR_CODE");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_OPS_RESERVED, "GET_INFO_INDEX_OPS_RESERVED");

    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_NUM_PORTS, "GET_INFO_INDEX_NUM_PORTS");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_ENABLED_PORTS_MASK_31_0, "GET_INFO_INDEX_ENABLED_PORTS_MASK_31_0");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_ENABLED_PORTS_MASK_63_32, "GET_INFO_INDEX_ENABLED_PORTS_MASK_63_32");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_NUM_VCS, "GET_INFO_INDEX_NUM_VCS");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_REMAP_POLICY_TABLE_SIZE, "GET_INFO_INDEX_REMAP_POLICY_TABLE_SIZE");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_ROUTING_ID_TABLE_SIZE, "GET_INFO_INDEX_ROUTING_ID_TABLE_SIZE");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_ROUTING_LAN_TABLE_SIZE, "GET_INFO_INDEX_ROUTING_LAN_TABLE_SIZE");

    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_FREQ_KHZ, "GET_INFO_INDEX_FREQ_KHZ");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_VCOFREQ_KHZ, "GET_INFO_INDEX_VCOFREQ_KHZ");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_VOLTAGE_MVOLT, "GET_INFO_INDEX_VOLTAGE_MVOLT");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_PHYSICAL_ID, "GET_INFO_INDEX_PHYSICAL_ID");

    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_PCI_DOMAIN, "GET_INFO_INDEX_PCI_DOMAIN");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_PCI_BUS, "GET_INFO_INDEX_PCI_BUS");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_PCI_DEVICE, "GET_INFO_INDEX_PCI_DEVICE");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_GET_INFO_INDEX_PCI_FUNCTION, "GET_INFO_INDEX_PCI_FUNCTION");

    UTL_BIND_CONSTANT(controlParams, LWSWITCH_ROUTING_ID_VCMAP_SAME, "ROUTING_ID_VCMAP_SAME");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_ROUTING_ID_VCMAP_ILWERT, "ROUTING_ID_VCMAP_ILWERT");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_ROUTING_ID_VCMAP_ZERO, "ROUTING_ID_VCMAP_ZERO");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_ROUTING_ID_VCMAP_ONE, "ROUTING_ID_VCMAP_ONE");

    UTL_BIND_CONSTANT(controlParams, CONNECT_ACCESS_GPU, "CONNECT_ACCESS_GPU");
    UTL_BIND_CONSTANT(controlParams, CONNECT_ACCESS_CPU, "CONNECT_ACCESS_CPU");
    UTL_BIND_CONSTANT(controlParams, CONNECT_TRUNK_SWITCH, "CONNECT_TRUNK_SWITCH");
    UTL_BIND_CONSTANT(controlParams, CONNECT_ACCESS_SWITCH, "CONNECT_ACCESS_SWITCH");

    UTL_BIND_CONSTANT(controlParams, LWSWITCH_TABLE_SELECT_REMAP_PRIMARY, "TABLE_SELECT_REMAP_PRIMARY");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_TABLE_SELECT_REMAP_EXTA, "TABLE_SELECT_REMAP_EXTA");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_TABLE_SELECT_REMAP_EXTB, "TABLE_SELECT_REMAP_EXTB");
    UTL_BIND_CONSTANT(controlParams, LWSWITCH_TABLE_SELECT_REMAP_MULTICAST, "TABLE_SELECT_REMAP_MULTICAST");
}

// This function is called by the Utl object to create all LwSwitch objects
// before any scripts are run.
//
void UtlLwSwitch::CreateLwSwitches()
{
    // When running with -no_gpu for stand-alone switch tests, the switch
    // devices also may not get created or initialized. So first look for an
    // existing library interface that has already been set up. If one doesn't
    // exist, create one and initialize it.
    LwLinkDevIf::LibIfPtr pLibIf;
    auto pTestDeviceMgr =
        static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    if (pTestDeviceMgr != nullptr)
    {
        TestDevicePtr pLwSwitchDevice;

        pTestDeviceMgr->GetDevice(
            TestDevice::TYPE_LWIDIA_LWSWITCH, 0, &pLwSwitchDevice);

        if (pLwSwitchDevice != nullptr)
        {
            pLibIf = pLwSwitchDevice->GetLibIf();
        }
    }

    if (pLibIf == nullptr)
    {
        // Create LwLink core library interface and initialize.
        LwLinkDevIf::LwLinkLibIfPtr pLwLinkLibIf =
            make_shared<LwLinkDevIf::LwLinkLibIfUser>();
        pLwLinkLibIf->Initialize();

        // Create LwSwitch library interface and initialize.
        pLibIf = LwLinkDevIf::LibInterface::CreateLwSwitchLibIf();
        pLibIf->Initialize();
        pLibIf->InitializeAllDevices();
    }

    vector<LwLinkDevIf::LibInterface::LibDeviceHandle> libDeviceHandles;
    pLibIf->FindDevices(&libDeviceHandles);

    for (const auto& libDeviceHandle : libDeviceHandles)
    {
        LwSwitchPtr pLwSwitch(
            new UtlLwSwitch(pLibIf, libDeviceHandle));
        DebugPrintf("UTL: Adding LwSwitch 0x%x.\n", libDeviceHandle);
        s_LwSwitches.push_back(move(pLwSwitch));
        s_PythonLwSwitches.push_back(s_LwSwitches.back().get());
    }
}

UtlLwSwitch::UtlLwSwitch
(
    LwLinkDevIf::LibIfPtr pLibIf,
    LwLinkDevIf::LibInterface::LibDeviceHandle libDeviceHandle
) :
    m_pLibIf(pLibIf),
    m_LibDeviceHandle(libDeviceHandle)
{
}

void UtlLwSwitch::FreeLwSwitches()
{
    s_PythonLwSwitches.clear();
    s_LwSwitches.clear();
}

// This function can be called from a UTL script to get
// the list of LwSwitch objects.
//
py::list UtlLwSwitch::GetLwSwitches()
{
    return UtlUtility::ColwertPtrVectorToPyList(s_PythonLwSwitches);
}

void UtlLwSwitch::Control
(
    LwLinkDevIf::LibInterface::LibControlId libControlId,
    void* controlParams,
    UINT32 paramSize
)
{
    RC rc;

    rc = m_pLibIf->Control(m_LibDeviceHandle, libControlId, controlParams, paramSize);

    if (rc != OK)
    {
        UtlUtility::PyError("Control call to LwSwitch driver from function "
            "LwSwitch.Control failed.");
    }
}

void UtlLwSwitch::ControlPy
(
    py::kwargs kwargs
)
{
    auto command = UtlUtility::GetRequiredKwarg<ControlCommand>(
        kwargs, "command", "LwSwitch.Control");
    auto pParams = UtlUtility::GetRequiredKwarg<UtlLwSwitch::ControlStruct*>(
        kwargs, "params", "LwSwitch.Control");

    LwLinkDevIf::LibInterface::LibControlId libControlId =
        LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_GET_INFO;
    switch (command)
    {
        case ControlCommand::GET_INFO:
            libControlId = LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_GET_INFO;
            break;
        case ControlCommand::SET_SWITCH_PORT_CONFIG:
            libControlId = LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_SET_SWITCH_PORT_CONFIG;
            break;
        case ControlCommand::GET_INTERNAL_LATENCY:
            libControlId = LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_GET_INTERNAL_LATENCY;
            break;
        case ControlCommand::SET_LATENCY_BINS:
            libControlId = LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_SET_LATENCY_BINS;
            break;
        case ControlCommand::GET_LWLIPT_COUNTERS:
            libControlId = LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_GET_LWLIPT_COUNTERS;
            break;
        case ControlCommand::SET_LWLIPT_COUNTER_CONFIG:
            libControlId = LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_SET_LWLIPT_COUNTER_CONFIG;
            break;
        case ControlCommand::GET_LWLIPT_COUNTER_CONFIG:
            libControlId = LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_GET_LWLIPT_COUNTER_CONFIG;
            break;
        case ControlCommand::SET_REMAP_POLICY:
            libControlId = LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_SET_REMAP_POLICY;
            break;
        case ControlCommand::SET_ROUTING_ID:
            libControlId = LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_SET_ROUTING_ID;
            break;
        case ControlCommand::SET_ROUTING_LAN:
            libControlId = LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_SET_ROUTING_LAN;
            break;
        case ControlCommand::SET_MC_RID_TABLE:
            libControlId = LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_SET_MC_RID_TABLE;
            break;
        default:
            UtlUtility::PyError("Invalid command argument passed to "
                "function LwSwitch.Control.");
    }

    DebugPrintf("UTL: LwSwitch Control libControlId=0x%x\n", libControlId);

    // Copy values from Python field objects to source struct.
    pParams->CopyToSource();

    {
        UtlGil::Release gil;

        Control(libControlId, pParams->Get(), pParams->GetSize());
    }

    // Copy values back to Python field objects.
    pParams->CopyFromSource();
}

UtlLwSwitchRegCtrl* UtlLwSwitch::GetRegCtrl(py::kwargs kwargs)
{
    auto instance = UtlUtility::GetRequiredKwarg<UINT32>(
        kwargs, "instance", "LwSwitch.GetRegCtrl");
    auto engine = UtlUtility::GetRequiredKwarg<UINT32>(
        kwargs, "engine", "LwSwitch.GetRegCtrl");
    return new UtlLwSwitchRegCtrl(this, instance, engine);
}

std::unique_ptr<IRegisterClass> UtlLwSwitch::FindRegister(string regName)
{
    ParseRefManual();
    std::unique_ptr<IRegisterClass> reg = m_RegMap->FindRegister(regName.c_str());
    if (!reg)
    {
        UtlUtility::PyError("Can't find register with name: %s",
            regName.c_str());
    }
    return reg;
}

std::unique_ptr<IRegisterClass> UtlLwSwitch::FindRegister(UINT32 regAddress)
{
    ParseRefManual();
    return m_RegMap->FindRegister(regAddress);
}

void UtlLwSwitch::ParseRefManual()
{
    // Parse the manuals register map only when requested since this is
    // an expensive process.
    if (m_RegMap == nullptr)
    {
        string filename = GetRefManualFile();
        if (filename.empty())
        {
            UtlUtility::PyError("Invalid manuals filename: %s",
                filename.c_str());
        }
        m_RefMan = GetRefManual(filename);
        m_RegMap = CreateRegisterMap(m_RefMan.get());
    }
}

string UtlLwSwitch::GetRefManualFile()
{
    LWSWITCH_GET_INFO infoParams;
    infoParams.index[0] = LWSWITCH_GET_INFO_INDEX_ARCH;
    infoParams.index[1] = LWSWITCH_GET_INFO_INDEX_IMPL;
    infoParams.index[2] = LWSWITCH_GET_INFO_INDEX_CHIPID;
    infoParams.count = 3;
    Control(LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_GET_INFO, &infoParams, sizeof(infoParams));
    UINT32 arch = infoParams.info[0];
    UINT32 impl = infoParams.info[1];
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
    UINT32 chipId = infoParams.info[2];
#endif

    switch (arch)
    {
        case LWSWITCH_GET_INFO_INDEX_ARCH_LR10:
            switch (impl)
            {
                case LWSWITCH_GET_INFO_INDEX_IMPL_LR10:
                    return "lr10_ref.txt";
                default:
                    break;
            }
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
        case LWSWITCH_GET_INFO_INDEX_ARCH_LS10:
            switch (impl)
            {
                case LWSWITCH_GET_INFO_INDEX_IMPL_LS10:
                    switch (chipId)
                    {
                        case LW_PMC_BOOT_42_CHIP_ID_S000:
                            return "s000_ref.txt";
                        case LW_PMC_BOOT_42_CHIP_ID_LS10:
                            return "ls10_ref.txt";
                    }
                default:
                    break;
            }
#endif
        default:
            break;
    }

    UtlUtility::PyError("Can't find the ref manual for LWSwitch");
    return "";
}

std::unique_ptr<RefManual> UtlLwSwitch::GetRefManual(string filename) const
{
    auto manuals = std::make_unique<RefManual>();
    if (manuals->Parse(filename.c_str()) != OK)
    {
        UtlUtility::PyError("Failed to parse manuals file: %s",
            filename.c_str());
    }
    return manuals;
}
