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
	Filter: VF
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
	Filter: "VF"
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
		["SriovSync",{"SyncPf": true,"VirtualTestId":"Sync_VFs"}],
		["LwdaLinpackDgemm",{"CheckLoops":4,"RuntimeMs":500,"VirtualTestId":"VF_LwdaLinpackDgemm"}],
		["LwdaLinpackPulseDP",{"CheckLoops":4,"RuntimeMs":500, "Ksize":256,"VirtualTestId":"VF_LwdaLinpackDgemm"}],
		["LwdaLinpackSgemm",{"CheckLoops":10,TestConfiguration:{Loops:100},"VirtualTestId":"VF_LwdaLinpackDgemm"}],
		["LwdaLinpackIgemm",{"CheckLoops":10,"RuntimeMs":500,"VirtualTestId":"VF_LwdaLinpackDgemm"}],
		["LwdaLinpackIMMAgemm",{"CheckLoops":10,"RuntimeMs":500,"VirtualTestId":"VF_LwdaLinpackDgemm"}],
		["LwdaLinpackHgemm",{"CheckLoops":10,"RuntimeMs":500,"VirtualTestId":"VF_LwdaLinpackDgemm"}],
		["LwdaLinpackHMMAgemm",{"CheckLoops":10,"RuntimeMs":500,"VirtualTestId":"VF_LwdaLinpackDgemm"}],
		["CaskLinpackSgemm",{"RuntimeMs":500,"VirtualTestId":"VF_LwdaLinpackDgemm"}],
		["CaskLinpackDgemm",{"RuntimeMs":500,"VirtualTestId":"VF_LwdaLinpackDgemm"}],
		["CaskLinpackDMMAgemm",{"RuntimeMs":500,"VirtualTestId":"VF_LwdaLinpackDgemm"}],
		["CaskLinpackE8M10",{"RuntimeMs":500,"VirtualTestId":"VF_LwdaLinpackDgemm"}],
		["CaskLinpackE8M7",{"RuntimeMs":500,"VirtualTestId":"VF_LwdaLinpackDgemm"}],
		["CaskLinpackSparseHMMA",{"RuntimeMs":500,"VirtualTestId":"VF_LwdaLinpackDgemm"}],
		["CaskLinpackSparseIMMA",{"RuntimeMs":500,"VirtualTestId":"VF_LwdaLinpackDgemm"}],
		["SriovSync",{"SyncPf": true,"VirtualTestId":"Sync_VFs"}],
		["NewLwdaMatsRandom",{"Iterations":1,"MaxFbMb":0,"VirtualTestId":"VF_NewLwdaMatsRandom","RuntimeMs":5000}],
		["SriovSync",{"SyncPf": true,"VirtualTestId":"Sync_VFs"}],
		["LwdaRandom",{"TestConfiguration":{"Loops":6*75},"VirtualTestId":"VF_LwdaRandom"}],
		["SriovSync",{"SyncPf": true,"VirtualTestId":"Sync_VFs"}]
	]);
};
