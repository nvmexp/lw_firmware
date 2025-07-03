#ifndef _FBCONFIG__H_
#define _FBCONFIG__H_
#include <iostream>

#define STANDALONE
#ifndef STANDALONE
#include "PerfSim.h"
#include "DRAMParams.h"

// Note: FBConfig is not really a packet, but an independent object that wants to access knobs.  This is not allowed, so
// we pretend that it's a packet for now.
struct FBConfig : public PerfSimPacket<FBConfig> {
#else
struct FBConfig {
    void printConfig() {
//       printf("PA/FB Parameters\n");
//       printf("partitions : %d\n", m_partitions);
//       printf("subpartitions : %d\n", m_subpartitions);
//       printf("banks : %d\n", m_banks);
//       printf("sets : %d\n", m_sets);
//       printf("slices : %d\n", m_slices);
//       printf("L2Banks : %d\n", m_L2Banks);
    }
#endif
    // these parameters are immutable
    enum {
        DataBusWidth = 32,
        SliceWidth = 256,
        DRAMPageSize = 1024,
        PartitionInterleave = 2048,
    };
#ifndef STANDALONE
    static std::string name() { return "FBConfig"; };
    static bool initialized;
    static void init();
#endif
    static FBConfig *getFBConfig();
#ifndef STANDALONE
    std::string getString() const { return "FBConfig"; };   // HACK

    // Define FB Clients
    enum Client {
        // Clients in GPC
        CLIENT_TEXTURE,
        CLIENT_SHADER,
        CLIENT_PRIMITIVE,
        // Clients in Raster
        CLIENT_CROP,
        CLIENT_ZROP,
        // Clients in Hub
        CLIENT_DISPLAY,
        CLIENT_VIDEO,
        CLIENT_FRONTEND,
        CLIENT_HOST,
        // Client unknown or not defined
        CLIENT_OTHER
    };
    static std::string client_to_string( Client client );
#endif
    enum aperture {
      VIDMEM, SYSMEM, PEER
    };

    FBConfig();
    int rows() const { return m_rows; }
    int columns() const { return m_columns; }
    int banks() const { return m_banks; }
    int subpartitions() const { return m_subpartitions; }
    int partitions() const { return m_partitions; }
    int ways() const { return m_ways; }
    int sets() const { return m_sets; }
    int slices() const { return m_slices; }
    int sectors() const { return m_sectors; }
    int l2Banks() const { return m_L2Banks; }
    bool gobSwizzling() const { return m_gobSwizzling; };

    bool simplifiedMapping() const { return m_simplifiedMapping; };
    int getAperture() const { return m_aperture; };
    void setAperture( int a ) { m_aperture = a; };
    bool experimentalMapping() const { return m_experimentalMapping; };
    int  experimentalMappingYRotation() const { return m_experimentalMappingYRotation; };
    int  experimentalMappingXRotation() const { return m_experimentalMappingXRotation; };
    int  randomAddressMapping() const { return m_randomAddressMapping; };
    int  splitTileOverSubPartition() const { return m_splitTileOverSubPartition; };
    int  mapSliceToSubPartition() const { return m_mapSliceToSubPartition; };

#ifndef STANDALONE
    int readFriendTrackers() const { return m_readFriendTrackers; }
    int readFriendMakerEntries() const { return m_readFriendMakerEntries; }
    int readFriendMakerAssociativity() const { return m_readFriendMakerAssociativity; }
    int readFriendLists() const { return m_readFriendLists; }
    int readFriendListEntries() const { return m_readFriendListEntries; }
    int dirtyFriendTrackers()  const { return m_dirtyFriendTrackers; }
    int dirtyFriendMakerEntries() const { return m_dirtyFriendMakerEntries; }
    int dirtyFriendMakerAssociativity() const { return m_dirtyFriendMakerAssociativity; }
    int dirtyFriendMakerAssociativeThreshold() const { return m_dirtyFriendMakerAssociativeThreshold; }
    int dirtyFriendLists() const { return m_dirtyFriendLists; }
    int dirtyFriendListEntries() const { return m_dirtyFriendListEntries; }

    int maxReadFriendListChain() const { return m_maxReadFriendListChain; }
    int maxReadFriendListRuntChain() const { return m_maxReadFriendListRuntChain; }
    double desiredRWTurnEfficiency() const { return m_desiredRWTurnEfficiency; }
    int minSectorsPerTurnForEfficiency() const { return m_minSectorsPerTurnForEfficiency; }
    DRAMParamsPtr dram_params() const { return m_dram_params; }

    double getDramclkFreq() { return m_dramclkFreqKnob.getValue(); }
    double getLwclkFreq() { return m_lwclkFreqKnob.getValue(); }
    int fbioLatency() const { return m_fbioLatency; }
    int cleanLineOnFriendCount() const { return m_cleanLineOnFriendCount; }
    int cleanLineOnFriendMakerWayCount() const { return m_cleanLineOnFriendMakerWayCount; }
    int dirArbSectorQuotaIncrement() const { return m_dirArbSectorQuotaIncrement; }

private:
#endif
    static FBConfig* m_fbconfig;

#ifndef STANDALONE
    static PerfSimKnob<double> m_dramclkFreqKnob;
    static PerfSimKnob<double> m_lwclkFreqKnob;

    static PerfSimKnob<int> m_numFBsKnob;
    static PerfSimKnob<int> m_numSubPartitionsKnob;

    static PerfSimKnob<bool> m_gobSwizzlingKnob;
    static PerfSimKnob<int> m_readFriendTrackersKnob;
    static PerfSimKnob<int> m_readFriendMakerEntriesKnob;
    static PerfSimKnob<int> m_readFriendMakerAssociativityKnob;
    static PerfSimKnob<int> m_readFriendListEntriesKnob;
    static PerfSimKnob<int> m_readFriendListsKnob;
    static PerfSimKnob<int> m_dirtyFriendTrackersKnob;
    static PerfSimKnob<int> m_dirtyFriendMakerEntriesKnob;
    static PerfSimKnob<int> m_dirtyFriendMakerAssociativityKnob;
    static PerfSimKnob<int> m_dirtyFriendMakerAssociativeThresholdKnob;
    static PerfSimKnob<int> m_dirtyFriendListsKnob;
    static PerfSimKnob<int> m_dirtyFriendListEntriesKnob;
    static PerfSimKnob<int> m_maxReadFriendListChainKnob;
    static PerfSimKnob<int> m_maxReadFriendListRuntChainKnob;
    static PerfSimKnob<bool> m_debugStringKnob;
    static PerfSimKnob<int> m_statTimelineIntervalKnob;
    static PerfSimKnob<int> m_numL2SlicesKnob;
    static PerfSimKnob<int> m_numL2SetsKnob;
    static PerfSimKnob<int> m_numL2WaysKnob;
    static PerfSimKnob<int> m_numL2SectorsKnob;
    static PerfSimKnob<int> m_numL2BanksKnob;
    static PerfSimKnob<int> m_numDRAMRowsKnob;
    static PerfSimKnob<int> m_numDRAMColumnsKnob;
    static PerfSimKnob<int> m_numDRAMBanksKnob;
    static PerfSimKnob<double> m_desiredRWTurnEfficiencyKnob;
    static PerfSimKnob<int> m_fbioLatencyKnob;
    static PerfSimKnob<int> FBConfig::m_cleanLineOnFriendCountKnob;
    static PerfSimKnob<int> FBConfig::m_cleanLineOnFriendMakerWayCountKnob;
    static PerfSimKnob<int> FBConfig::m_dirArbSectorQuotaIncrementKnob;

    static PerfSimKnob<bool> m_simplifiedMappingKnob;
    static PerfSimKnob<bool> m_sysmemMappingKnob;
    static PerfSimKnob<bool> m_experimentalMappingKnob;
    static PerfSimKnob<int>  m_experimentalMappingYRotationKnob;
    static PerfSimKnob<int>  m_experimentalMappingXRotationKnob;
    static PerfSimKnob<bool>  m_randomAddressMappingKnob;
    static PerfSimKnob<bool>  m_splitTileOverSubPartitionKnob;
    static PerfSimKnob<bool>  m_mapSliceToSubPartitionKnob;

    DRAMParamsPtr m_dram_params;
#endif
    int m_rows;
    int m_columns;
    int m_banks;
    int m_subpartitions;
    int m_partitions;
    bool m_gobSwizzling;
    int m_ways;
    int m_sets;
    int m_slices;
    int m_sectors;
    int m_L2Banks;
    int m_readFriendTrackers;
    int m_readFriendMakerEntries;
    int m_readFriendMakerAssociativity;
    int m_readFriendLists;
    int m_readFriendListEntries;
    int m_dirtyFriendTrackers;
    int m_dirtyFriendMakerEntries;
    int m_dirtyFriendMakerAssociativity;
    int m_dirtyFriendMakerAssociativeThreshold;
    int m_dirtyFriendLists;
    int m_dirtyFriendListEntries;
    int m_maxReadFriendListChain;
    int m_maxReadFriendListRuntChain;
    double m_desiredRWTurnEfficiency;
    int m_minSectorsPerTurnForEfficiency;
    int m_fbioLatency;
    int m_cleanLineOnFriendCount;
    int m_cleanLineOnFriendMakerWayCount;
    int m_dirArbSectorQuotaIncrement;

    int m_aperture;
    bool m_simplifiedMapping;
    bool m_experimentalMapping;
    int  m_experimentalMappingYRotation;
    int  m_experimentalMappingXRotation;
    bool m_randomAddressMapping;
    bool m_splitTileOverSubPartition;
    bool m_mapSliceToSubPartition;

};

#endif
