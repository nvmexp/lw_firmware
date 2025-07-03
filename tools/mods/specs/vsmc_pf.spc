/*
* LWIDIA_COPYRIGHT_BEGIN
* 
* Copyright 2019 by LWPU Corporation.All rights reserved.
* All information contained herein is proprietary and confidential to LWPU
* Corporation.Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
* 
* LWIDIA_COPYRIGHT_END
*/


/*
	Id: 45
	Revision: 3
		Name: SMC_SRIOV_PVS
		Description: Spec for running SMC in parallel using SR-IOV in PVS
		Created By: Anand Nahar
	URL: https://cftt.lwpu.com/mods/#/specFiles/45/3
	Filter: PF
*/

#include "arghndlr.js" // NOTE this is required for "ARG_STATE_VARIABLE"

var g_SpecDescriptor =
{
	Id: "45",
		Branch: "Rbringup_g",
		Release: "bringup_g.48",
		Description: "Spec for running SMC in parallel using SR-IOV in PVS",
		Revision: "3",
	URL: "https://cftt.lwpu.com/mods/#/specFiles/45/3",
	Filter: "PF"
};


g_TestMode = 64; // TM_MFG_BOARD : 64

/*Global Variables*/

/*Global Function*/

/*Spec Tests*/

function userSpec(testList, perfPoints, testDevice)
{
	testList.TestMode = 64;
	testList.AddTests(
	[
		["SetPState",{InfPts:{"PStateNum":0,"LocationStr":"max"},"VirtualTestId":"PF_0_max"}],
		["SriovSync",{"SyncPf": true,"VirtualTestId":"PF_Release_VFs"}],
		["SriovSync",{"VirtualTestId":"PF_VF_Sync"}],
		["SetPState",{InfPts:{"PStateNum":0,"LocationStr":"intersect"},"VirtualTestId":"0_intersect"}],
		["SriovSync",{"SyncPf": true,"VirtualTestId":"PF_Release_VFs"}],
		["PerfPunish",{"FgTestNum": 309,"VirtualTestId":"PF_PerfPunish"}],
		["SriovSync",{"VirtualTestId":"PF_VF_Sync"}],
		["SetPState",{InfPts:{"PStateNum":0,"LocationStr":"max"},"VirtualTestId":"PF_0_max"}],
		["SriovSync",{"SyncPf": true,"VirtualTestId":"PF_Release_VFs"}],
		["PerfSwitch",{"FgTestNum": 309,"VirtualTestId":"PF_PerfSwitch"}]
	]);
};
