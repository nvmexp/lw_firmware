/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_OBJDISCOVERY_H
#define SOE_OBJDISCOVERY_H

/* ------------------------ System includes -------------------------------- */
#include "dev_lws_top.h"
#include "dev_soe_ip_addendum.h"
/* ------------------------ Application includes --------------------------- */
/* ------------------------ Defines ---------------------------------------- */
#define LWSWITCH_DISCOVERY_ENTRY_ILWALID    LW_SWPTOP_ENTRY_ILWALID
#define LWSWITCH_DISCOVERY_ENTRY_ENUM       LW_SWPTOP_ENTRY_ENUM
#define LWSWITCH_DISCOVERY_ENTRY_DATA1      LW_SWPTOP_ENTRY_DATA1
#define LWSWITCH_DISCOVERY_ENTRY_DATA2      LW_SWPTOP_ENTRY_DATA2


#define NUM_L2_NPG_ENGINES    NUM_NPG_ENGINE   + NUM_NPG_BCAST_ENGINE + \
                              NUM_NPORT_ENGINE + NUM_NPORT_MULTICAST_BCAST_ENGINE
#define NUM_L2_NXBAR_ENGINES  NUM_NXBAR_ENGINE + NUM_NXBAR_BCAST_ENGINE + \
                              NUM_TILE_ENGINE  + NUM_TILE_MULTICAST_BCAST_ENGINE

#define NUM_L1_NPG_ENGINES    NUM_NPG_ENGINE   + NUM_NPG_BCAST_ENGINE
#define NUM_L1_NXBAR_ENGINES  NUM_NXBAR_ENGINE + NUM_NXBAR_BCAST_ENGINE

#define NUM_L1_PTOP_ENGINES   NUM_SAW_ENGINE     + NUM_LWLW_ENGINE + \
                              NUM_L1_NPG_ENGINES + NUM_L1_NXBAR_ENGINES + \
                              NUM_BUS_ENGINE     + NUM_GIN_ENGINE + \
                              NUM_SYS_PRI_HUB    + NUM_PRI_MASTER_RS

#define NUM_L2_PTOP_ENGINES   NUM_TLC_ENGINE     + NUM_LWLIPT_LNK_ENGINE + \
                              NUM_L2_NPG_ENGINES + NUM_L2_NXBAR_ENGINES + \
                              NUM_MINION_ENGINE

// Sum of all the L1 and L2 engines
#define TOTAL_ENGINES NUM_L1_PTOP_ENGINES + NUM_L2_PTOP_ENGINES

#define ILWALID_ADDRESS 0xFFFFFFFF

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
struct OBJENGINELIST
{
    LwU32 engineEnum;
    LwU32 engineName;
    LwU32 instance;
    LwBool valid;

    union
    {
        struct
        {
            LwU32 discovery;                // Used for top level only
        } top;
        struct
        {
            LwU32 ucAddr;
        } uc;
        struct
        {
            LwU32 bcAddr;
            LwU32 mcAddr[3];
        } bc;
    } info;
};

enum AddressTypes
{
    ADDRESS_UNICAST = 0,
    ADDRESS_BROADCAST,
    ADDRESS_MULTICAST,
    ADDRESS_DISCOVERY
};

enum InstanceNumber
{
    INSTANCE0 = 0,
    INSTANCE1,
    INSTANCE2,
    INSTANCE3,
    INSTANCE4,
    INSTANCE5,
    INSTANCE6,
    INSTANCE7,
    INSTANCE8,
    INSTANCE9,
    INSTANCE10,
    INSTANCE11,
    INSTANCE12,
    INSTANCE13,
    INSTANCE14,
    INSTANCE15,
};

enum EngineName
{
    LW_DISCOVERY_ENGINE_NAME_SAW = 0,
    LW_DISCOVERY_ENGINE_NAME_LWLW_PTOP,
    LW_DISCOVERY_ENGINE_NAME_NPG_PTOP,
    LW_DISCOVERY_ENGINE_NAME_NPG_BCAST_PTOP,
    LW_DISCOVERY_ENGINE_NAME_NXBAR_PTOP,
    LW_DISCOVERY_ENGINE_NAME_NXBAR_BCAST_PTOP,
    LW_DISCOVERY_ENGINE_NAME_LWLW,
    LW_DISCOVERY_ENGINE_NAME_LWLTLC,
    LW_DISCOVERY_ENGINE_NAME_LWLIPT_LINK,
    LW_DISCOVERY_ENGINE_NAME_NPG,
    LW_DISCOVERY_ENGINE_NAME_NPG_BCAST,
    LW_DISCOVERY_ENGINE_NAME_NPORT,
    LW_DISCOVERY_ENGINE_NAME_NPORT_MULTICAST,
    LW_DISCOVERY_ENGINE_NAME_NXBAR,
    LW_DISCOVERY_ENGINE_NAME_NXBAR_BCAST,
    LW_DISCOVERY_ENGINE_NAME_TILE,
    LW_DISCOVERY_ENGINE_NAME_TILE_MULTICAST,
    LW_DISCOVERY_ENGINE_NAME_XAL,
    LW_DISCOVERY_ENGINE_NAME_XVE,
    LW_DISCOVERY_ENGINE_NAME_SYS_PRI_HUB,
    LW_DISCOVERY_ENGINE_NAME_PRI_MASTER_RS,
    LW_DISCOVERY_ENGINE_NAME_MINION,
    LW_DISCOVERY_ENGINE_NAME_GIN,
    LW_DISCOVERY_ENGINE_NUM,
};

typedef struct
{
    struct OBJENGINELIST EngineList[TOTAL_ENGINES];
    LwBool               bIsDiscoveryDone;
}OBJDISCOVERY;

struct ENGINE_TABLE
{
    LwU32 engineEnum;
    LwU32 engineName;
    LwU32 instance;
};

typedef struct engine_tables
{
    struct ENGINE_TABLE *engineLookUp;
    LwU32 sizeEngineLookUp;
    struct ENGINE_TABLE  *engineLwlwLookUp;
    LwU32 sizeEngineLwlwLookUp;
    struct ENGINE_TABLE  *engineNportLookUp;
    LwU32 sizeEngineNportLookUp;
    struct ENGINE_TABLE  *engineNpgBcastLookUp;
    LwU32 sizeEngineNpgBcastLookUp;
    struct ENGINE_TABLE  *engineNpgLookUp;
    LwU32 sizeEngineNpgLookUp;
    struct ENGINE_TABLE  *engineNxbarLookUp;
    LwU32 sizeEngineNxbarLookUp;
    struct ENGINE_TABLE  *engineTileLookUp;
    LwU32 sizeEngineTileLookUp;
    struct ENGINE_TABLE  *engineNxbarBcastLookUp;
    LwU32 sizeEngineNxbarBcastLookUp;
}ENGINE_TABLES, *PENGINE_TABLES;


extern OBJDISCOVERY Discovery;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

sysSYSLIB_CODE LwU32 getEngineBaseAddress(LwU32 engineName, LwU32 instance, LwU32 addressType, LwU32 mcNum);
/* ------------------------ Misc macro definitions ------------------------- */

#endif // SOE_OBJDISCOVERY_H
