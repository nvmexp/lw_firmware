/*
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soediscovery.c
 * @brief  Read the bases addresses from PTOP
 *
 * Store the Base addresses of various engines.
 */
/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"
#include "dev_soe_csb.h"
#include "dev_lws_top.h"
#include "soe_bar0.h"
#include "lwlinkip_discovery.h"
#include "npgip_discovery.h"
#include "nxbar_discovery.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objsoe.h"
#include "soe_objdiscovery.h"
#include "config/g_soe_private.h"

/* ------------------------- Type Definitions ------------------------------- */
typedef struct
{
    void (*parseEntry)(LwU32 entry, LwU32 *entryType, LwBool *entryChain);
    void (*parseEnum)(LwU32 entry, LwU32 *entryDevice, LwU32 *entryId, LwU32 *entryVersion);
    void (*handleData1)(LwU32 entry, struct OBJENGINELIST *engine, LwU32 entryDevice, LwU32 *discoveryListSize);
    void (*handleData2)(LwU32 entry, struct OBJENGINELIST *engine, LwU32 entryDevice);
}
SOE_DISCOVERY_HANDLERS_LR10;
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
// Level1 PTOP table
static struct ENGINE_TABLE engineLookUp_LR10 [] =
{
    { LW_SWPTOP_ENUM_DEVICE_SAW, LW_DISCOVERY_ENGINE_NAME_SAW, INSTANCE0 },
    { LW_SWPTOP_ENUM_DEVICE_LWLW, LW_DISCOVERY_ENGINE_NAME_LWLW_PTOP, INSTANCE0 },
    { LW_SWPTOP_ENUM_DEVICE_LWLW, LW_DISCOVERY_ENGINE_NAME_LWLW_PTOP, INSTANCE1 },
    { LW_SWPTOP_ENUM_DEVICE_LWLW, LW_DISCOVERY_ENGINE_NAME_LWLW_PTOP, INSTANCE2 },
    { LW_SWPTOP_ENUM_DEVICE_LWLW, LW_DISCOVERY_ENGINE_NAME_LWLW_PTOP, INSTANCE3 },
    { LW_SWPTOP_ENUM_DEVICE_LWLW, LW_DISCOVERY_ENGINE_NAME_LWLW_PTOP, INSTANCE4 },
    { LW_SWPTOP_ENUM_DEVICE_LWLW, LW_DISCOVERY_ENGINE_NAME_LWLW_PTOP, INSTANCE5 },
    { LW_SWPTOP_ENUM_DEVICE_LWLW, LW_DISCOVERY_ENGINE_NAME_LWLW_PTOP, INSTANCE6 },
    { LW_SWPTOP_ENUM_DEVICE_LWLW, LW_DISCOVERY_ENGINE_NAME_LWLW_PTOP, INSTANCE7 },
    { LW_SWPTOP_ENUM_DEVICE_LWLW, LW_DISCOVERY_ENGINE_NAME_LWLW_PTOP, INSTANCE8 },
    { LW_SWPTOP_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG_PTOP, INSTANCE0 },
    { LW_SWPTOP_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG_PTOP, INSTANCE1 },
    { LW_SWPTOP_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG_PTOP, INSTANCE2 },
    { LW_SWPTOP_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG_PTOP, INSTANCE3 },
    { LW_SWPTOP_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG_PTOP, INSTANCE4 },
    { LW_SWPTOP_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG_PTOP, INSTANCE5 },
    { LW_SWPTOP_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG_PTOP, INSTANCE6 },
    { LW_SWPTOP_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG_PTOP, INSTANCE7 },
    { LW_SWPTOP_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG_PTOP, INSTANCE8 },
    { LW_SWPTOP_ENUM_DEVICE_NPG_BCAST, LW_DISCOVERY_ENGINE_NAME_NPG_BCAST_PTOP, INSTANCE0 },
    { LW_SWPTOP_ENUM_DEVICE_NXBAR, LW_DISCOVERY_ENGINE_NAME_NXBAR_PTOP, INSTANCE0 },
    { LW_SWPTOP_ENUM_DEVICE_NXBAR, LW_DISCOVERY_ENGINE_NAME_NXBAR_PTOP, INSTANCE1 },
    { LW_SWPTOP_ENUM_DEVICE_NXBAR, LW_DISCOVERY_ENGINE_NAME_NXBAR_PTOP, INSTANCE2 },
    { LW_SWPTOP_ENUM_DEVICE_NXBAR, LW_DISCOVERY_ENGINE_NAME_NXBAR_PTOP, INSTANCE3 },
    { LW_SWPTOP_ENUM_DEVICE_NXBAR_BCAST, LW_DISCOVERY_ENGINE_NAME_NXBAR_BCAST_PTOP, INSTANCE0 },
    { LW_SWPTOP_ENUM_DEVICE_XVE, LW_DISCOVERY_ENGINE_NAME_XVE, INSTANCE0 },
};

static struct ENGINE_TABLE engineLwlwLookUp_LR10 [] =
{
    { LW_LWLINKIP_DISCOVERY_COMMON_DEVICE_LWLTLC, LW_DISCOVERY_ENGINE_NAME_LWLTLC, INSTANCE0 },
    { LW_LWLINKIP_DISCOVERY_COMMON_DEVICE_LWLTLC, LW_DISCOVERY_ENGINE_NAME_LWLTLC, INSTANCE1 },
    { LW_LWLINKIP_DISCOVERY_COMMON_DEVICE_LWLTLC, LW_DISCOVERY_ENGINE_NAME_LWLTLC, INSTANCE2 },
    { LW_LWLINKIP_DISCOVERY_COMMON_DEVICE_LWLTLC, LW_DISCOVERY_ENGINE_NAME_LWLTLC, INSTANCE3 },
    { LW_LWLINKIP_DISCOVERY_COMMON_DEVICE_LWLIPT_LNK, LW_DISCOVERY_ENGINE_NAME_LWLIPT_LINK, INSTANCE0 },
    { LW_LWLINKIP_DISCOVERY_COMMON_DEVICE_LWLIPT_LNK, LW_DISCOVERY_ENGINE_NAME_LWLIPT_LINK, INSTANCE1 },
    { LW_LWLINKIP_DISCOVERY_COMMON_DEVICE_LWLIPT_LNK, LW_DISCOVERY_ENGINE_NAME_LWLIPT_LINK, INSTANCE2 },
    { LW_LWLINKIP_DISCOVERY_COMMON_DEVICE_LWLIPT_LNK, LW_DISCOVERY_ENGINE_NAME_LWLIPT_LINK, INSTANCE3 },
    { LW_LWLINKIP_DISCOVERY_COMMON_DEVICE_MINION, LW_DISCOVERY_ENGINE_NAME_MINION, INSTANCE0},
};

static struct ENGINE_TABLE engineNportLookUp_LR10 [] =
{
    { LW_NPG_DISCOVERY_ENUM_DEVICE_NPORT, LW_DISCOVERY_ENGINE_NAME_NPORT, INSTANCE0 },
    { LW_NPG_DISCOVERY_ENUM_DEVICE_NPORT, LW_DISCOVERY_ENGINE_NAME_NPORT, INSTANCE1 },
    { LW_NPG_DISCOVERY_ENUM_DEVICE_NPORT, LW_DISCOVERY_ENGINE_NAME_NPORT, INSTANCE2 },
    { LW_NPG_DISCOVERY_ENUM_DEVICE_NPORT, LW_DISCOVERY_ENGINE_NAME_NPORT, INSTANCE3 },
};

static struct ENGINE_TABLE engineNpgBcastLookUp_LR10 [] =
{
    { LW_NPG_DISCOVERY_ENUM_DEVICE_NPORT_MULTICAST, LW_DISCOVERY_ENGINE_NAME_NPORT_MULTICAST, INSTANCE0 },
    { LW_NPG_DISCOVERY_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG_BCAST, INSTANCE0 },
};

static struct ENGINE_TABLE engineNpgLookUp_LR10 [] =
{
    { LW_NPG_DISCOVERY_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG, INSTANCE0 },
    { LW_NPG_DISCOVERY_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG, INSTANCE1 },
    { LW_NPG_DISCOVERY_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG, INSTANCE2 },
    { LW_NPG_DISCOVERY_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG, INSTANCE3 },
    { LW_NPG_DISCOVERY_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG, INSTANCE4 },
    { LW_NPG_DISCOVERY_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG, INSTANCE5 },
    { LW_NPG_DISCOVERY_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG, INSTANCE6 },
    { LW_NPG_DISCOVERY_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG, INSTANCE7 },
    { LW_NPG_DISCOVERY_ENUM_DEVICE_NPG, LW_DISCOVERY_ENGINE_NAME_NPG, INSTANCE8 },
};

static struct ENGINE_TABLE engineNxbarLookUp_LR10 [] =
{
    { LW_NXBAR_DISCOVERY_ENUM_DEVICE_NXBAR, LW_DISCOVERY_ENGINE_NAME_NXBAR, INSTANCE0 },
    { LW_NXBAR_DISCOVERY_ENUM_DEVICE_NXBAR, LW_DISCOVERY_ENGINE_NAME_NXBAR, INSTANCE1 },
    { LW_NXBAR_DISCOVERY_ENUM_DEVICE_NXBAR, LW_DISCOVERY_ENGINE_NAME_NXBAR, INSTANCE2 },
    { LW_NXBAR_DISCOVERY_ENUM_DEVICE_NXBAR, LW_DISCOVERY_ENGINE_NAME_NXBAR, INSTANCE3 },
};

static struct ENGINE_TABLE engineTileLookUp_LR10 [] =
{
    { LW_NXBAR_DISCOVERY_ENUM_DEVICE_TILE, LW_DISCOVERY_ENGINE_NAME_TILE, INSTANCE0 },
    { LW_NXBAR_DISCOVERY_ENUM_DEVICE_TILE, LW_DISCOVERY_ENGINE_NAME_TILE, INSTANCE1 },
    { LW_NXBAR_DISCOVERY_ENUM_DEVICE_TILE, LW_DISCOVERY_ENGINE_NAME_TILE, INSTANCE2 },
    { LW_NXBAR_DISCOVERY_ENUM_DEVICE_TILE, LW_DISCOVERY_ENGINE_NAME_TILE, INSTANCE3 },
};

static struct ENGINE_TABLE engineNxbarBcastLookUp_LR10 [] =
{
    { LW_NXBAR_DISCOVERY_ENUM_DEVICE_TILE_MULTICAST, LW_DISCOVERY_ENGINE_NAME_TILE_MULTICAST, INSTANCE0 },
    { LW_NXBAR_DISCOVERY_ENUM_DEVICE_NXBAR, LW_DISCOVERY_ENGINE_NAME_NXBAR_BCAST, INSTANCE0 },
};

static ENGINE_TABLES engineTables_LR10 = {
    .engineLookUp=engineLookUp_LR10,
    .sizeEngineLookUp=sizeof(engineLookUp_LR10)/sizeof((engineLookUp_LR10)[0]),
    .engineLwlwLookUp=engineLwlwLookUp_LR10,
    .sizeEngineLwlwLookUp=sizeof(engineLwlwLookUp_LR10)/sizeof((engineLwlwLookUp_LR10)[0]),
    .engineNportLookUp=engineNportLookUp_LR10,
    .sizeEngineNportLookUp=sizeof(engineNportLookUp_LR10)/sizeof((engineNportLookUp_LR10)[0]),
    .engineNpgBcastLookUp=engineNpgBcastLookUp_LR10,
    .sizeEngineNpgBcastLookUp=sizeof(engineNpgBcastLookUp_LR10)/sizeof((engineNpgBcastLookUp_LR10)[0]),
    .engineNpgLookUp=engineNpgLookUp_LR10,
    .sizeEngineNpgLookUp=sizeof(engineNpgLookUp_LR10)/sizeof((engineNpgLookUp_LR10)[0]),
    .engineNxbarLookUp=engineNxbarLookUp_LR10,
    .sizeEngineNxbarLookUp=sizeof(engineNxbarLookUp_LR10)/sizeof((engineNxbarLookUp_LR10)[0]),
    .engineTileLookUp=engineTileLookUp_LR10,
    .sizeEngineTileLookUp=sizeof(engineTileLookUp_LR10)/sizeof((engineTileLookUp_LR10)[0]),
    .engineNxbarBcastLookUp=engineNxbarBcastLookUp_LR10,
    .sizeEngineNxbarBcastLookUp=sizeof(engineNxbarBcastLookUp_LR10)/sizeof((engineNxbarBcastLookUp_LR10)[0]),
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJDISCOVERY Discovery;
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static void
_discoverEngineAddress(LwU32 discoveryOffset, SOE_DISCOVERY_HANDLERS_LR10 *discoveryHandler,
                      struct ENGINE_TABLE *engineLookUp, LwU32  engineLookupTableSize,
                      LwBool level2Ptop, LwU32 level1Instance)
                    GCC_ATTRIB_SECTION("imem_resident", "discoverEngineAddress");

// L1 PTOP parsing functions
static void
_soePtopParseEntryLr10(LwU32 entry, LwU32 *entryType, LwBool *entryChain)
                    GCC_ATTRIB_SECTION("imem_resident", "soePtopParseEntryLr10");
static void
_soePtopParseEnumLr10(LwU32 entry, LwU32 *entryDevice, LwU32 *entryId, LwU32 *entryVersion)
                    GCC_ATTRIB_SECTION("imem_resident", "soePtopParseEnumLr10");
static void
_soePtopHandleData1Lr10(LwU32 entry, struct OBJENGINELIST *engine, LwU32 entryDevice, LwU32 *discoveryListSize)
                    GCC_ATTRIB_SECTION("imem_resident", "soePtopHandleData1Lr10");
static void
_soePtopHandleData2Lr10(LwU32 entry, struct OBJENGINELIST *engine, LwU32 entryDevice)
                    GCC_ATTRIB_SECTION("imem_resident", "soePtopHandleData2Lr10");

// L2 LWLW parsing functions
static void
_soeLwlwParseEntryLr10(LwU32 entry, LwU32 *entryType, LwBool *entryChain)
                    GCC_ATTRIB_SECTION("imem_resident", "soeLwlwParseEntryLr10");
static void
_soeLwlwParseEnumLr10(LwU32 entry, LwU32 *entryDevice, LwU32 *entryId, LwU32 *entryVersion)
                    GCC_ATTRIB_SECTION("imem_resident", "soeLwlwParseEnumLr10");
static void
_soeLwlwHandleData1Lr10(LwU32 entry, struct OBJENGINELIST *engine, LwU32 entryDevice, LwU32 *discoveryListSize)
                    GCC_ATTRIB_SECTION("imem_resident", "soeLwlwHandleData1Lr10");
static void
_soeLwlwHandleData2Lr10(LwU32 entry, struct OBJENGINELIST *engine, LwU32 entryDevice)
                    GCC_ATTRIB_SECTION("imem_resident", "soeLwlwHandleData2Lr10");

// L2 NPG parsing functions
static void
_soeNpgParseEntryLr10(LwU32 entry, LwU32 *entryType, LwBool *entryChain)
                    GCC_ATTRIB_SECTION("imem_resident", "soeNpgParseEntryLr10");
static void
_soeNpgParseEnumLr10(LwU32 entry, LwU32 *entryDevice, LwU32 *entryId, LwU32 *entryVersion)
                    GCC_ATTRIB_SECTION("imem_resident", "soeNpgParseEnumLr10");
static void
_soeNpgHandleData1Lr10(LwU32 entry, struct OBJENGINELIST *engine, LwU32 entryDevice, LwU32 *discoveryListSize)
                    GCC_ATTRIB_SECTION("imem_resident", "soeNpgHandleData1Lr10");
static void
_soeNpgHandleData2Lr10(LwU32 entry, struct OBJENGINELIST *engine, LwU32 entryDevice)
                    GCC_ATTRIB_SECTION("imem_resident", "soeNpgHandleData2Lr10");

// L2 NXBAR parsing functions
static void
_soeNxbarParseEntryLr10(LwU32 entry, LwU32 *entryType, LwBool *entryChain)
                    GCC_ATTRIB_SECTION("imem_resident", "soeNxbarParseEntryLr10");
static void
_soeNxbarParseEnumLr10(LwU32 entry, LwU32 *entryDevice, LwU32 *entryId, LwU32 *entryVersion)
                    GCC_ATTRIB_SECTION("imem_resident", "soeNxbarParseEnumLr10");
static void
_soeNxbarHandleData1Lr10(LwU32 entry, struct OBJENGINELIST *engine, LwU32 entryDevice, LwU32 *discoveryListSize)
                    GCC_ATTRIB_SECTION("imem_resident", "soeNxbarHandleData1Lr10");
static void
_soeNxbarHandleData2Lr10(LwU32 entry, struct OBJENGINELIST *engine, LwU32 entryDevice)
                    GCC_ATTRIB_SECTION("imem_resident", "soeNxbarHandleData2Lr10");

/*
 *  Parses the PTOP Entry
 *
 *  @param[in]  entry      - Data from the PTOP
 *  @param[out] entryType  - Type of the given data
 *  @param[out] entryChain - Tells if the given entry is last or has a chain
 *
 */
static void
_soePtopParseEntryLr10
(
    LwU32   entry,
    LwU32   *entryType,
    LwBool  *entryChain
)
{
    LwU32 entryTypeLwlw;

    entryTypeLwlw = DRF_VAL(_LWLINKIP, _DISCOVERY_COMMON, _ENTRY, entry);
    *entryChain = FLD_TEST_DRF(_LWLINKIP, _DISCOVERY_COMMON, _CHAIN, _ENABLE, entry);

    switch (entryTypeLwlw)
    {
        case LW_LWLINKIP_DISCOVERY_COMMON_ENTRY_ENUM:
            *entryType = LWSWITCH_DISCOVERY_ENTRY_ENUM;
            break;
        case LW_LWLINKIP_DISCOVERY_COMMON_ENTRY_DATA1:
            *entryType = LWSWITCH_DISCOVERY_ENTRY_DATA1;
            break;
        case LW_LWLINKIP_DISCOVERY_COMMON_ENTRY_DATA2:
            *entryType = LWSWITCH_DISCOVERY_ENTRY_DATA2;
            break;
        default:
            *entryType = LWSWITCH_DISCOVERY_ENTRY_ILWALID;
            break;
    }
}

/*
 *  Parses the PTOP Enum to get device,version and entryid
 *
 *  @param[in]  entry        - Data from the PTOP
 *  @param[out] entryDevice  - Device enum value
 *  @param[out] entryId      - Instance of the device
 *  @param[out] entryVersion - Version of the device
 *
 */
static void
_soePtopParseEnumLr10
(
    LwU32   entry,
    LwU32   *entryDevice,
    LwU32   *entryId,
    LwU32   *entryVersion
)
{
    *entryDevice  = DRF_VAL(_SWPTOP, _, ENUM_DEVICE, entry);
    *entryId      = DRF_VAL(_SWPTOP, _, ENUM_ID, entry);
    *entryVersion = DRF_VAL(_SWPTOP, _, ENUM_VERSION, entry);
}

/*
 *  Parses the PTOP Data1
 *
 *  @param[in]  entry        - Data from the PTOP
 *  @param[out] engine       - Updates the engine values
 *  @param[in]  entryDevice  - Device enum value
 *  @param[out] discoveryListSize - Size of L1 PTOP
 *
 */
static void
_soePtopHandleData1Lr10
(
    LwU32 entry,
    struct OBJENGINELIST *engine,
    LwU32 entryDevice,
    LwU32 *discoveryListSize
)
{
    if (LW_SWPTOP_ENUM_DEVICE_PTOP == entryDevice)
    {
        *discoveryListSize = DRF_VAL(_SWPTOP, _DATA1, _PTOP_LENGTH, entry);
        return;
    }
    else
    {
        if(!(DRF_VAL(_SWPTOP, _DATA1, _RESERVED, entry) == 0))
        {
            REG_WR32_STALL(CSB, SOE_STATUS_SCRATCH_REGISTER,
                           SOE_ERROR_LS_FATAL_DISCOVERY_FAILED);
            SOE_HALT();
        }
    }

    if (engine == NULL)
    {
        return;
    }
}

/*
 *  Parses the PTOP Data2
 *
 *  @param[in]  entry        - Data from the PTOP
 *  @param[out] engine       - Updates the engine values
 *  @param[in]  entryDevice  - Device enum value
 *
 */
static void
_soePtopHandleData2Lr10
(
    LwU32 entry,
    struct OBJENGINELIST *engine,
    LwU32 entryDevice
)
{
    LwU32 data2Type = DRF_VAL(_SWPTOP, _DATA2, _TYPE, entry);
    LwU32 data2Addr = DRF_VAL(_SWPTOP, _DATA2, _ADDR, entry);

    switch(data2Type)
    {
        case LW_SWPTOP_DATA2_TYPE_DISCOVERY:
            // Parse sub-discovery table
            engine->info.top.discovery = data2Addr*sizeof(LwU32);
            break;
        case LW_SWPTOP_DATA2_TYPE_UNICAST:
            engine->info.uc.ucAddr = data2Addr*sizeof(LwU32);
            break;
        case LW_SWPTOP_DATA2_TYPE_BROADCAST:
            engine->info.bc.bcAddr = data2Addr*sizeof(LwU32);
            break;
        case LW_SWPTOP_DATA2_TYPE_MULTICAST0:
        case LW_SWPTOP_DATA2_TYPE_MULTICAST1:
        case LW_SWPTOP_DATA2_TYPE_MULTICAST2:
            {
                LwU32 mcIdx = data2Type - LW_SWPTOP_DATA2_TYPE_MULTICAST0;
                engine->info.bc.mcAddr[mcIdx] = data2Addr*sizeof(LwU32);
            }
            break;
        case LW_SWPTOP_DATA2_TYPE_ILWALID:
            // Do Nothing
            break;
        default:
            // Do Nothing
            break;
    }
}

SOE_DISCOVERY_HANDLERS_LR10 discoveryHandlersLwlwLr10 =
{
    &_soeLwlwParseEntryLr10,
    &_soeLwlwParseEnumLr10,
    &_soeLwlwHandleData1Lr10,
    &_soeLwlwHandleData2Lr10
};

/*
 *  Parses the NPG Entry
 *
 *  @param[in]  entry      - Data from the PTOP
 *  @param[out] entryType  - Type of the given data
 *  @param[out] entryChain - Tells if the given entry is last or has a chain
 *
 */
static void
_soeNpgParseEntryLr10
(
    LwU32  entry,
    LwU32  *entryType,
    LwBool *entryChain
)
{
    LwU32 entryTypeNpg;

    entryTypeNpg = DRF_VAL(_NPG, _DISCOVERY, _ENTRY, entry);
    *entryChain = FLD_TEST_DRF(_NPG, _DISCOVERY, _CHAIN, _ENABLE, entry);

    switch (entryTypeNpg)
    {
        case LW_NPG_DISCOVERY_ENTRY_ENUM:
            *entryType = LWSWITCH_DISCOVERY_ENTRY_ENUM;
            break;
        case LW_NPG_DISCOVERY_ENTRY_DATA1:
            *entryType = LWSWITCH_DISCOVERY_ENTRY_DATA1;
            break;
        case LW_NPG_DISCOVERY_ENTRY_DATA2:
            *entryType = LWSWITCH_DISCOVERY_ENTRY_DATA2;
            break;
        default:
            *entryType = LWSWITCH_DISCOVERY_ENTRY_ILWALID;
            break;
    }
}

/*
 *  Parses the NPG Enum to get device,version and entryid
 *
 *  @param[in]  entry        - Data from the PTOP
 *  @param[out] entryDevice  - Device enum value
 *  @param[out] entryId      - Instance of the device
 *  @param[out] entryVersion - Version of the device
 *
 */
static void
_soeNpgParseEnumLr10
(
    LwU32 entry,
    LwU32 *entryDevice,
    LwU32 *entryId,
    LwU32 *entryVersion
)
{
    *entryDevice  = DRF_VAL(_NPG, _DISCOVERY, _ENUM_DEVICE, entry);
    *entryId      = DRF_VAL(_NPG, _DISCOVERY, _ENUM_ID, entry);
    *entryVersion = DRF_VAL(_NPG, _DISCOVERY, _ENUM_VERSION, entry);
}

/*
 *  Parses the NPG Data1
 *
 *  @param[in]  entry        - Data from the PTOP
 *  @param[out] engine       - Updates the engine values
 *  @param[in]  entryDevice  - Device enum value
 *  @param[out] discoveryListSize - Size of L1 PTOP
 *
 */
static void
_soeNpgHandleData1Lr10
(
    LwU32 entry,
    struct OBJENGINELIST *engine,
    LwU32 entryDevice,
    LwU32 *discoveryListSize
)
{
    if (LW_NPG_DISCOVERY_ENUM_DEVICE_NPG == entryDevice)
    {
        *discoveryListSize = DRF_VAL(_NPG, _DISCOVERY, _DATA1_NPG_LENGTH, entry);
    }
    else
    {
        if (DRF_VAL(_NPG, _DISCOVERY, _DATA1_RESERVED, entry) != 0)
        {
            REG_WR32_STALL(CSB, SOE_STATUS_SCRATCH_REGISTER,
                           SOE_ERROR_LS_FATAL_DISCOVERY_FAILED);
            SOE_HALT();
        }
    }

    if (engine == NULL)
    {
        return;
    }
}

/*
 *  Parses the NPG Data2
 *
 *  @param[in]  entry        - Data from the PTOP
 *  @param[out] engine       - Updates the engine values
 *  @param[in]  entryDevice  - Device enum value
 *
 */
static void
_soeNpgHandleData2Lr10
(
    LwU32 entry,
    struct OBJENGINELIST *engine,
    LwU32 entryDevice
)
{
    LwU32 data2Type = DRF_VAL(_NPG, _DISCOVERY_DATA2, _TYPE, entry);
    LwU32 data2Addr = DRF_VAL(_NPG, _DISCOVERY_DATA2, _ADDR, entry);

    switch(data2Type)
    {

        case  LW_NPG_DISCOVERY_DATA2_TYPE_DISCOVERY:
            // Parse sub-discovery table

            //
            // Lwrrently _DISCOVERY is not used in the second
            // level discovery.
            //
            REG_WR32_STALL(CSB, SOE_STATUS_SCRATCH_REGISTER,
                           SOE_ERROR_LS_FATAL_DISCOVERY_FAILED);
            SOE_HALT();
            break;

        case LW_NPG_DISCOVERY_DATA2_TYPE_UNICAST:
            engine->info.uc.ucAddr = data2Addr*sizeof(LwU32);
            break;

        case LW_NPG_DISCOVERY_DATA2_TYPE_BROADCAST:
            engine->info.bc.bcAddr = data2Addr*sizeof(LwU32);
            break;

        case LW_NPG_DISCOVERY_DATA2_TYPE_MULTICAST0:
        case LW_NPG_DISCOVERY_DATA2_TYPE_MULTICAST1:
        case LW_NPG_DISCOVERY_DATA2_TYPE_MULTICAST2:
            {
                LwU32 mcIdx = data2Type - LW_NPG_DISCOVERY_DATA2_TYPE_MULTICAST0;
                engine->info.bc.mcAddr[mcIdx] = data2Addr*sizeof(LwU32);
            }
            break;
        case LW_NPG_DISCOVERY_DATA2_TYPE_ILWALID:
            break;

        default:
            REG_WR32_STALL(CSB, SOE_STATUS_SCRATCH_REGISTER,
                           SOE_ERROR_LS_FATAL_DISCOVERY_FAILED);
            SOE_HALT();
            break;
    }
}

SOE_DISCOVERY_HANDLERS_LR10 discoveryHandlersNpgLr10 =
{
    &_soeNpgParseEntryLr10,
    &_soeNpgParseEnumLr10,
    &_soeNpgHandleData1Lr10,
    &_soeNpgHandleData2Lr10
};

/*
 *  Parses the NXBAR Entry
 *
 *  @param[in]  entry      - Data from the PTOP
 *  @param[out] entryType  - Type of the given data
 *  @param[out] entryChain - Tells if the given entry is last or has a chain
 *
 */
static void
_soeNxbarParseEntryLr10
(
    LwU32  entry,
    LwU32  *entryType,
    LwBool *entryChain
)
{
    LwU32 entryTypeNxbar;

    entryTypeNxbar = DRF_VAL(_NXBAR, _DISCOVERY, _ENTRY, entry);
    *entryChain = FLD_TEST_DRF(_NXBAR, _DISCOVERY, _CHAIN, _ENABLE, entry);

    switch (entryTypeNxbar)
    {
        case LW_NXBAR_DISCOVERY_ENTRY_ENUM:
            *entryType = LWSWITCH_DISCOVERY_ENTRY_ENUM;
            break;
        case LW_NXBAR_DISCOVERY_ENTRY_DATA1:
            *entryType = LWSWITCH_DISCOVERY_ENTRY_DATA1;
            break;
        case LW_NXBAR_DISCOVERY_ENTRY_DATA2:
            *entryType = LWSWITCH_DISCOVERY_ENTRY_DATA2;
            break;
        default:
            *entryType = LWSWITCH_DISCOVERY_ENTRY_ILWALID;
            break;
    }
}

/*
 *  Parses the NXBAR Enum to get device,version and entryid
 *
 *  @param[in]  entry        - Data from the PTOP
 *  @param[out] entryDevice  - Device enum value
 *  @param[out] entryId      - Instance of the device
 *  @param[out] entryVersion - Version of the device
 *
 */
static void
_soeNxbarParseEnumLr10
(
    LwU32 entry,
    LwU32 *entryDevice,
    LwU32 *entryId,
    LwU32 *entryVersion
)
{
    LwU32 entry_reserved;

    *entryDevice  = DRF_VAL(_NXBAR, _DISCOVERY, _ENUM_DEVICE, entry);
    *entryId      = DRF_VAL(_NXBAR, _DISCOVERY, _ENUM_ID, entry);
    *entryVersion = DRF_VAL(_NXBAR, _DISCOVERY, _ENUM_VERSION, entry);

    entry_reserved = DRF_VAL(_NXBAR, _DISCOVERY, _ENUM_RESERVED, entry);
    if (entry_reserved != 0)
    {
        REG_WR32_STALL(CSB, SOE_STATUS_SCRATCH_REGISTER,
                       SOE_ERROR_LS_FATAL_DISCOVERY_FAILED);
        SOE_HALT();
    }
}

/*
 *  Parses the NXBAR Data1
 *
 *  @param[in]  entry        - Data from the PTOP
 *  @param[out] engine       - Updates the engine values
 *  @param[in]  entryDevice  - Device enum value
 *  @param[out] discoveryListSize - Size of L1 PTOP
 *
 */
static void
_soeNxbarHandleData1Lr10
(
    LwU32 entry,
    struct OBJENGINELIST *engine,
    LwU32 entryDevice,
    LwU32 *discoveryListSize
)
{
    if (LW_NXBAR_DISCOVERY_ENUM_DEVICE_NXBAR == entryDevice)
    {
        *discoveryListSize = DRF_VAL(_NXBAR, _DISCOVERY, _DATA1_NXBAR_LENGTH, entry);
    }
    else
    {
        if (DRF_VAL(_NXBAR, _DISCOVERY, _DATA1_RESERVED, entry) != 0)
        {
            REG_WR32_STALL(CSB, SOE_STATUS_SCRATCH_REGISTER,
                           SOE_ERROR_LS_FATAL_DISCOVERY_FAILED);
            SOE_HALT();
        }
    }

    if (engine == NULL)
    {
        return;
    }
}

/*
 *  Parses the NXBAR Data2
 *
 *  @param[in]  entry        - Data from the PTOP
 *  @param[out] engine       - Updates the engine values
 *  @param[in]  entryDevice  - Device enum value
 *
 */
static void
_soeNxbarHandleData2Lr10
(
    LwU32 entry,
    struct OBJENGINELIST *engine,
    LwU32 entryDevice
)
{
    LwU32 data2Type = DRF_VAL(_NXBAR, _DISCOVERY_DATA2, _TYPE, entry);
    LwU32 data2Addr = DRF_VAL(_NXBAR, _DISCOVERY_DATA2, _ADDR, entry);

    switch(data2Type)
    {

        case  LW_NXBAR_DISCOVERY_DATA2_TYPE_DISCOVERY:
            // Parse sub-discovery table

            //
            // Lwrrently _DISCOVERY is not used in the second
            // level discovery.
            //
            REG_WR32_STALL(CSB, SOE_STATUS_SCRATCH_REGISTER,
                           SOE_ERROR_LS_FATAL_DISCOVERY_FAILED);
            SOE_HALT();
            break;

        case LW_NXBAR_DISCOVERY_DATA2_TYPE_UNICAST:
            engine->info.uc.ucAddr = data2Addr*sizeof(LwU32);
            break;

        case LW_NXBAR_DISCOVERY_DATA2_TYPE_BROADCAST:
            engine->info.bc.bcAddr = data2Addr*sizeof(LwU32);
            break;

        case LW_NXBAR_DISCOVERY_DATA2_TYPE_MULTICAST0:
        case LW_NXBAR_DISCOVERY_DATA2_TYPE_MULTICAST1:
        case LW_NXBAR_DISCOVERY_DATA2_TYPE_MULTICAST2:
            {
                LwU32 mcIdx = data2Type - LW_NXBAR_DISCOVERY_DATA2_TYPE_MULTICAST0;
                engine->info.bc.mcAddr[mcIdx] = data2Addr*sizeof(LwU32);
            }
            break;
        case LW_NXBAR_DISCOVERY_DATA2_TYPE_ILWALID:
            break;

        default:
            REG_WR32_STALL(CSB, SOE_STATUS_SCRATCH_REGISTER,
                           SOE_ERROR_LS_FATAL_DISCOVERY_FAILED);
            SOE_HALT();
            break;
    }
}

SOE_DISCOVERY_HANDLERS_LR10 discoveryHandlersNxbarLr10 =
{
    &_soeNxbarParseEntryLr10,
    &_soeNxbarParseEnumLr10,
    &_soeNxbarHandleData1Lr10,
    &_soeNxbarHandleData2Lr10
};

/*
 *  Stores the BAR0 addresses for the required Engines
 *
 *  @param[in] discoveryOffset          Base address of the L1 PTOP.
 *  @param[in] discoveryHandler         Discovery handlers of the Engine.
 *  @param[in] engineLookUp             Lookup table of the engine.
 *  @param[in] engineLookupTableSize    Size of lookup table.
 *  @param[in] bLevel2Ptop              Ptop of the engine (L1 or L2)
 *  @param[in] level1Instance           Instance of the L1 PTOP.
 */
static void
_discoverEngineAddress
(
    LwU32 discoveryOffset,
    SOE_DISCOVERY_HANDLERS_LR10 *discoveryHandler,
    struct ENGINE_TABLE *engineLookUp,
    LwU32  engineLookupTableSize,
    LwBool bLevel2Ptop,
    LwU32  level1Instance
)
{
    LwU32                   entryType = LWSWITCH_DISCOVERY_ENTRY_ILWALID;
    LwBool                  entryChain = LW_FALSE;
    LwU32                   entry = 0;
    LwU32                   entryDevice = 0;
    LwU32                   engineName = 0;
    LwU32                   entryId = 0;
    LwU32                   entryVersion = 0;
    LwU32                   entryCount = 0;
    LwBool                  done = LW_FALSE;
    LwU32                   discoveryListSize = 2;
    LwU32                   discoveryTableSize = engineLookupTableSize;
    struct OBJENGINELIST    *engine = NULL;

    while ((!done) && (entryCount < discoveryListSize))
    {
        entry = REG_RD32(BAR0, discoveryOffset);

        discoveryHandler->parseEntry(entry, &entryType, &entryChain);

        switch(entryType)
        {
            case LWSWITCH_DISCOVERY_ENTRY_ENUM:
                discoveryHandler->parseEnum(entry, &entryDevice, &entryId, &entryVersion);
                {
                    LwU32 i;

                    for (i = 0; i < discoveryTableSize; i++)
                    {
                        if (entryDevice == engineLookUp[i].engineEnum)
                        {
                            engineName = engineLookUp[i].engineName;

                            if (entryId == (bLevel2Ptop ? (level1Instance*4 + engineLookUp[i].instance) : engineLookUp[i].instance))
                            {
                                LwU32 j;
                                for (j = 0; j < TOTAL_ENGINES; j++)
                                {
                                    if (Discovery.EngineList[j].valid == LW_FALSE)
                                    {
                                        engine = &Discovery.EngineList[j];
                                    }
                                }
                            }
                        }

                        if (engine != NULL)
                        {
                            engine->valid      = LW_TRUE;
                            engine->engineEnum = entryDevice;
                            engine->engineName = engineName;
                            engine->instance   = entryId;
                        }
                    }
                }
                break;
            case LWSWITCH_DISCOVERY_ENTRY_DATA1:
                discoveryHandler->handleData1(entry, engine, entryDevice, &discoveryListSize);
                break;
            case LWSWITCH_DISCOVERY_ENTRY_DATA2:
                if (engine == NULL)
                {
                   break;
                }
                discoveryHandler->handleData2(entry, engine, entryDevice);
                break;
            default:
            case LWSWITCH_DISCOVERY_ENTRY_ILWALID:
                // Invalid entry.  Just ignore it
                break;
        }

        if (!entryChain)
        {
            // End of chain.  Close the active engine
            engine = NULL;
            entryDevice  = 0;      // Mark invalid
            entryId      = ~0;
            entryVersion = ~0;
        }
        entryCount++;
        discoveryOffset += sizeof(LwU32);
    }
}

/*
 *  Parses the PTOP Entry
 *
 *  @param[in]  entry      - Data from the PTOP
 *  @param[out] entryType  - Type of the given data
 *  @param[out] entryChain - Tells if the given entry is last or has a chain
 *
 */
static void
_soeLwlwParseEntryLr10
(
    LwU32 entry,
    LwU32 *entryType,
    LwBool *entryChain
)
{
    LwU32 entryTypeLwlw;

    entryTypeLwlw = DRF_VAL(_LWLINKIP, _DISCOVERY_COMMON, _ENTRY, entry);
    *entryChain = FLD_TEST_DRF(_LWLINKIP, _DISCOVERY_COMMON, _CHAIN, _ENABLE, entry);

    switch (entryTypeLwlw)
    {
        case LW_LWLINKIP_DISCOVERY_COMMON_ENTRY_ENUM:
            *entryType = LWSWITCH_DISCOVERY_ENTRY_ENUM;
            break;
        case LW_LWLINKIP_DISCOVERY_COMMON_ENTRY_DATA1:
            *entryType = LWSWITCH_DISCOVERY_ENTRY_DATA1;
            break;
        case LW_LWLINKIP_DISCOVERY_COMMON_ENTRY_DATA2:
            *entryType = LWSWITCH_DISCOVERY_ENTRY_DATA2;
            break;
        default:
            *entryType = LWSWITCH_DISCOVERY_ENTRY_ILWALID;
            break;
    }
}

/*
 *  Parses the PTOP Enum to get device,version and entryid
 *
 *  @param[in]  entry        - Data from the PTOP
 *  @param[out] entryDevice  - Device enum value
 *  @param[out] entryId      - Instance of the device
 *  @param[out] entryVersion - Version of the device
 *
 */
static void
_soeLwlwParseEnumLr10
(
    LwU32 entry,
    LwU32 *entryDevice,
    LwU32 *entryId,
    LwU32 *entryVersion
)
{
    *entryDevice  = DRF_VAL(_LWLINKIP, _DISCOVERY_COMMON, _DEVICE, entry);
    *entryId      = DRF_VAL(_LWLINKIP, _DISCOVERY_COMMON, _ID, entry);
    *entryVersion = DRF_VAL(_LWLINKIP, _DISCOVERY_COMMON, _VERSION, entry);
}

/*
 *  Parses the PTOP Data1
 *
 *  @param[in]  entry        - Data from the PTOP
 *  @param[out] engine       - Updates the engine values
 *  @param[in]  entryDevice  - Device enum value
 *  @param[out] discoveryListSize - Size of L1 PTOP
 *
 */
static void
_soeLwlwHandleData1Lr10
(
    LwU32 entry,
    struct OBJENGINELIST *engine,
    LwU32 entryDevice,
    LwU32 *discoveryListSize
)
{
    if ((LW_LWLINKIP_DISCOVERY_COMMON_DEVICE_IOCTRL == entryDevice)  ||
        (LW_LWLINKIP_DISCOVERY_COMMON_DEVICE_SIOCTRL == entryDevice) ||
        (LW_LWLINKIP_DISCOVERY_COMMON_DEVICE_TIOCTRL == entryDevice) ||
        (LW_LWLINKIP_DISCOVERY_COMMON_DEVICE_LWLW == entryDevice))
    {
        *discoveryListSize = DRF_VAL(_LWLINKIP, _DISCOVERY_COMMON, _DATA1_IOCTRL_LENGTH, entry);
    }

    if (engine == NULL)
    {
        return;
    }
}

/*
 *  Parses the PTOP Data2
 *
 *  @param[in]  entry        - Data from the PTOP
 *  @param[out] engine       - Updates the engine values
 *  @param[in]  entryDevice  - Device enum value
 *
 */
static void
_soeLwlwHandleData2Lr10
(
    LwU32 entry,
    struct OBJENGINELIST *engine,
    LwU32 entryDevice
)
{
    LwU32 data2Type = DRF_VAL(_LWLINKIP, _DISCOVERY_COMMON_DATA2, _TYPE, entry);
    LwU32 data2Addr = DRF_VAL(_LWLINKIP, _DISCOVERY_COMMON_DATA2, _ADDR, entry);

    switch(data2Type)
    {

        case LW_LWLINKIP_DISCOVERY_COMMON_DATA2_TYPE_DISCOVERY:
            // Parse sub-discovery table

            //
            // Lwrrently _DISCOVERY is not used in the second
            // level discovery.
            //
            REG_WR32_STALL(CSB, SOE_STATUS_SCRATCH_REGISTER,
                           SOE_ERROR_LS_FATAL_DISCOVERY_FAILED);
            SOE_HALT();
            break;

        case LW_LWLINKIP_DISCOVERY_COMMON_DATA2_TYPE_UNICAST:
            engine->info.uc.ucAddr = data2Addr*sizeof(LwU32);
            break;

        case LW_LWLINKIP_DISCOVERY_COMMON_DATA2_TYPE_BROADCAST:
            engine->info.bc.bcAddr = data2Addr*sizeof(LwU32);
            break;

        case LW_LWLINKIP_DISCOVERY_COMMON_DATA2_TYPE_MULTICAST0:
        case LW_LWLINKIP_DISCOVERY_COMMON_DATA2_TYPE_MULTICAST1:
        case LW_LWLINKIP_DISCOVERY_COMMON_DATA2_TYPE_MULTICAST2:
            {
                LwU32 mcIdx = data2Type - LW_LWLINKIP_DISCOVERY_COMMON_DATA2_TYPE_MULTICAST0;
                engine->info.bc.mcAddr[mcIdx] = data2Addr*sizeof(LwU32);
            }
            break;
        case LW_LWLINKIP_DISCOVERY_COMMON_DATA2_TYPE_ILWALID:
            break;

        default:
            break;
    }
}

SOE_DISCOVERY_HANDLERS_LR10 discoveryHandlersPtopLr10 =
{
    &_soePtopParseEntryLr10,
    &_soePtopParseEnumLr10,
    &_soePtopHandleData1Lr10,
    &_soePtopHandleData2Lr10
};

/*
 * Returns the chip specific engine lookup tables.
 *
 *  return ENGINE_TABLES*           Retuns the chip specific engine lookup tables.
 */
PENGINE_TABLES
soeGetEngineLookup_LR10(void)
{
    return &engineTables_LR10;
}

/*
 * Retuns the Base address of the requested engine.
 *
 * @param[in] engineEnum   ENUM of the requested engines Base address
 * @param[in] instance     Instance of the requested engine
 * @param[in] addressType  Type of address requested (Unicast, BroadCast, Multicast)
 * @param[in] mcNum        In case of multicast , MultiCast number for that engine.
 *                         This value is irrelavent in case if the address is not Multicast
 *
 *  return LwU32           Returns the base address of the requested engine.
 */
LwU32
getEngineBaseAddress
(
    LwU32 engineName,
    LwU32 instance,
    LwU32 addressType,
    LwU32 mcNum
)
{
    ENGINE_TABLES *engineLookUps = soeGetEngineLookup_HAL();
    LwU32 i;
    LwU32 discoveryTableSize;
    LwU32 address = ILWALID_ADDRESS;

    if (!Discovery.bIsDiscoveryDone)
    {
        memset(&Discovery, 0, sizeof(OBJDISCOVERY));

        discoveryTableSize = engineLookUps->sizeEngineLookUp;

        _discoverEngineAddress(LW_SWPTOP_TABLE_BASE_ADDRESS_OFFSET, &discoveryHandlersPtopLr10,
                               engineLookUps->engineLookUp, discoveryTableSize, LW_FALSE, 0);
        for ( i = 0 ; i < TOTAL_ENGINES; i++)
        {
            if (Discovery.EngineList[i].engineEnum == LW_SWPTOP_ENUM_DEVICE_LWLW)
            {
                discoveryTableSize = engineLookUps->sizeEngineLwlwLookUp;
                _discoverEngineAddress(Discovery.EngineList[i].info.top.discovery, &discoveryHandlersLwlwLr10,
                                       engineLookUps->engineLwlwLookUp, discoveryTableSize, LW_TRUE , Discovery.EngineList[i].instance);
            }
            if (Discovery.EngineList[i].engineEnum == LW_SWPTOP_ENUM_DEVICE_NPG)
            {
                discoveryTableSize = engineLookUps->sizeEngineNportLookUp;
                _discoverEngineAddress(Discovery.EngineList[i].info.top.discovery, &discoveryHandlersNpgLr10,
                                       engineLookUps->engineNportLookUp, discoveryTableSize, LW_TRUE, Discovery.EngineList[i].instance);
                discoveryTableSize = engineLookUps->sizeEngineNpgLookUp;
                _discoverEngineAddress(Discovery.EngineList[i].info.top.discovery, &discoveryHandlersNpgLr10,
                                       engineLookUps->engineNpgLookUp, discoveryTableSize, LW_FALSE, 0);
            }
            if (Discovery.EngineList[i].engineEnum == LW_SWPTOP_ENUM_DEVICE_NPG_BCAST)
            {
                discoveryTableSize = engineLookUps->sizeEngineNpgBcastLookUp;
                _discoverEngineAddress(Discovery.EngineList[i].info.top.discovery, &discoveryHandlersNpgLr10,
                                       engineLookUps->engineNpgBcastLookUp, discoveryTableSize, LW_FALSE, 0);
            }
            if (Discovery.EngineList[i].engineEnum == LW_SWPTOP_ENUM_DEVICE_NXBAR)
            {
                discoveryTableSize = engineLookUps->sizeEngineTileLookUp;
                _discoverEngineAddress(Discovery.EngineList[i].info.top.discovery, &discoveryHandlersNxbarLr10,
                                       engineLookUps->engineTileLookUp, discoveryTableSize, LW_TRUE, Discovery.EngineList[i].instance);
                discoveryTableSize = engineLookUps->sizeEngineNxbarLookUp;
                _discoverEngineAddress(Discovery.EngineList[i].info.top.discovery, &discoveryHandlersNxbarLr10,
                                       engineLookUps->engineNxbarLookUp, discoveryTableSize, LW_FALSE, 0);
            }
            if (Discovery.EngineList[i].engineEnum == LW_SWPTOP_ENUM_DEVICE_NXBAR_BCAST)
            {
                discoveryTableSize = engineLookUps->sizeEngineNxbarBcastLookUp;
                _discoverEngineAddress(Discovery.EngineList[i].info.top.discovery, &discoveryHandlersNxbarLr10,
                                       engineLookUps->engineNxbarBcastLookUp, discoveryTableSize, LW_FALSE, 0);
            }
        }
        Discovery.bIsDiscoveryDone = LW_TRUE;
    }

    for (i = 0; i < TOTAL_ENGINES; i++)
    {
        if (engineName == Discovery.EngineList[i].engineName)
        {
            if ((instance == Discovery.EngineList[i].instance) && (Discovery.EngineList[i].valid == LW_TRUE))
            {
                switch (addressType)
                {
                    case ADDRESS_UNICAST:
                        address = Discovery.EngineList[i].info.uc.ucAddr;
                        break;
                    case ADDRESS_BROADCAST:
                        address = Discovery.EngineList[i].info.bc.bcAddr;
                        break;
                    case ADDRESS_MULTICAST:
                        address = Discovery.EngineList[i].info.bc.mcAddr[mcNum];
                        break;
                    default:
                        address = ILWALID_ADDRESS;
                        break;
                }
            }
        }
    }

    if (address == ILWALID_ADDRESS)
    {
        REG_WR32_STALL(CSB, SOE_STATUS_SCRATCH_REGISTER,
                       SOE_ERROR_LS_FATAL_DISCOVERY_FAILED);
        SOE_HALT();
    }
    return address;
}
