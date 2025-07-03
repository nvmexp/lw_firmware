#ifndef _FermiAddressMapping__H_
#define _FermiAddressMapping__H_
#include "CommonUtils.h"

// FIXME Fung should be replaced with mods routines

#if defined(FERMI_PERFSIM)
#define PA_MSB 31
typedef UINT32 PA_TYPE;
#define PORTABLE_FAIL(x) PERFSIM_CHECK(0)(x)
#define PORTABLE_ASSERT(x) PERFSIM_CHECK(x)
#elif defined(AMAP_TESTBENCH)
#define PA_MSB 31
typedef UINT64 PA_TYPE;
#define PORTABLE_FAIL(x)
#define PORTABLE_ASSERT(x)
#else
#define PA_MSB 39
typedef UINT64 PA_TYPE;
#define PORTABLE_FAIL(x) cslAssertMsg((0),(x))
#define PORTABLE_ASSERT(x) cslAssert((x))
#endif

struct FermiPhysicalAddress {

    enum KIND {PITCHLINEAR=0, BLOCKLINEAR_GENERIC, BLOCKLINEAR_Z64, PITCH_NO_SWIZZLE};
    enum PAGE_SIZE {PS4K=0, PS64K, PS128K};
    enum APERTURE {LOCAL=0, PEER, SYSTEM};

    FermiPhysicalAddress() : m_adr(0), m_partition(~0), m_slice(~0), m_l2set(~0), m_l2tag(~0), m_l2sector(~0), m_l2bank(~0), m_l2offset(~0),
         m_kind(PITCHLINEAR), m_page_size(PS128K), m_aperture(LOCAL) {}
    FermiPhysicalAddress(PA_TYPE _adr,
        const enum KIND _kind,
        const enum PAGE_SIZE _page_size,
        const enum APERTURE _aperture);
    // this is not different from the default copy constructor
    FermiPhysicalAddress(const FermiPhysicalAddress& _pa) : m_adr(_pa.m_adr), m_partition(_pa.m_partition), m_slice(_pa.m_slice), m_l2set(_pa.m_l2set), m_l2tag(_pa.m_l2tag),
        m_l2sector(_pa.m_l2sector), m_l2bank(_pa.m_l2bank), m_l2offset(_pa.m_l2offset), m_kind(_pa.m_kind), m_page_size(_pa.m_page_size), m_aperture(_pa.m_aperture), PAKS(_pa.PAKS) {}

    PA_TYPE paks() const { return PAKS; }

    UINT32 partition() const { return m_partition; }
    UINT32 slice() const { return m_slice; }
    PA_TYPE address() const { return m_adr; }
    bool operator==(const FermiPhysicalAddress & _adr) const {return (_adr.m_adr == m_adr && _adr.m_aperture == m_aperture && _adr.m_kind == m_kind && _adr.m_page_size == m_page_size);};

    UINT32  l2tag() const { return m_l2tag; }
    UINT32  l2offset() const { return m_l2offset; }
    UINT32  l2set() const { return m_l2set; }
    UINT32  l2sector() const { return m_l2sector; }
    UINT32  l2bank() const { return m_l2bank; }

    std::string getString() const;

  private:
    // PAKS internal methods
    // aperture_kind_pagesize_partitions
    void calc_PAKS_local_pitchlinear_ps4k_parts8(PA_TYPE PA_value, PA_TYPE &PA_lev1);
    void calc_PAKS_local_pitchlinear_ps4k_parts7(PA_TYPE PA_value);
    void calc_PAKS_local_pitchlinear_ps4k_parts6(PA_TYPE PA_value);
    void calc_PAKS_local_pitchlinear_ps4k_parts5(PA_TYPE PA_value, PA_TYPE &PA_lev1);
    void calc_PAKS_local_pitchlinear_ps4k_parts4(PA_TYPE PA_value, PA_TYPE &PA_lev1);
    void calc_PAKS_local_pitchlinear_ps4k_parts3(PA_TYPE PA_value);
    void calc_PAKS_local_pitchlinear_ps4k_parts2(PA_TYPE PA_value);
    void calc_PAKS_local_pitchlinear_ps4k_parts1(PA_TYPE PA_value);

    void calc_PAKS_local_blocklinear_ps4k_parts8(PA_TYPE PA_value, PA_TYPE &PA_lev1);
    void calc_PAKS_local_blocklinear_ps4k_parts7(PA_TYPE PA_value);
    void calc_PAKS_local_blocklinear_ps4k_parts6(PA_TYPE PA_value);
    void calc_PAKS_local_blocklinear_ps4k_parts5(PA_TYPE PA_value, PA_TYPE &PA_lev1);
    void calc_PAKS_local_blocklinear_ps4k_parts4(PA_TYPE PA_value, PA_TYPE &PA_lev1);
    void calc_PAKS_local_blocklinear_ps4k_parts3(PA_TYPE PA_value);
    void calc_PAKS_local_blocklinear_ps4k_parts2(PA_TYPE PA_value);
    void calc_PAKS_local_blocklinear_ps4k_parts1(PA_TYPE PA_value);

    void calc_PAKS_local_common_ps4k_parts8(PA_TYPE PA_lev1);
    void calc_PAKS_local_common_ps4k_parts5(PA_TYPE PA_lev1);
    void calc_PAKS_local_common_ps4k_parts4(PA_TYPE PA_lev1);

    void calc_PAKS_local_pitchlinear_ps64k_parts8(PA_TYPE PA_value);
    void calc_PAKS_local_pitchlinear_ps64k_parts7(PA_TYPE PA_value);
    void calc_PAKS_local_pitchlinear_ps64k_parts6(PA_TYPE PA_value);
    void calc_PAKS_local_pitchlinear_ps64k_parts5(PA_TYPE PA_value);
    void calc_PAKS_local_pitchlinear_ps64k_parts4(PA_TYPE PA_value);
    void calc_PAKS_local_pitchlinear_ps64k_parts3(PA_TYPE PA_value);
    void calc_PAKS_local_pitchlinear_ps64k_parts2(PA_TYPE PA_value);
    void calc_PAKS_local_pitchlinear_ps64k_parts1(PA_TYPE PA_value);

    void calc_PAKS_local_blocklinear_ps64k_parts8(PA_TYPE PA_value);
    void calc_PAKS_local_blocklinear_ps64k_parts7(PA_TYPE PA_value);
    void calc_PAKS_local_blocklinear_ps64k_parts6(PA_TYPE PA_value);
    void calc_PAKS_local_blocklinear_ps64k_parts5(PA_TYPE PA_value);
    void calc_PAKS_local_blocklinear_ps64k_parts4(PA_TYPE PA_value);
    void calc_PAKS_local_blocklinear_ps64k_parts3(PA_TYPE PA_value);
    void calc_PAKS_local_blocklinear_ps64k_parts2(PA_TYPE PA_value);
    void calc_PAKS_local_blocklinear_ps64k_parts1(PA_TYPE PA_value);

    void calc_PAKS_system_pitchlinear_ps4k(PA_TYPE PA_value);
    void calc_PAKS_system_pitchlinear_ps64k(PA_TYPE PA_value);
    void calc_PAKS_system_blocklinearZ64(PA_TYPE PA_value);

    // slice internal methods
    void calc_SLICE_local_slices4_parts8421(UINT32 Q, UINT32 R, UINT32 O);
    void calc_SLICE_local_slices4_parts7(UINT32 Q, UINT32 R, UINT32 O);
    void calc_SLICE_local_slices4_parts6(UINT32 Q, UINT32 R, UINT32 O);
    void calc_SLICE_local_slices4_parts53(UINT32 Q, UINT32 R, UINT32 O);

    void calc_SLICE_local_slices2_parts8421(UINT32 Q, UINT32 R, UINT32 O);
    void calc_SLICE_local_slices2_parts7(UINT32 Q, UINT32 R, UINT32 O);
    void calc_SLICE_local_slices2_parts6(UINT32 Q, UINT32 R, UINT32 O);
    void calc_SLICE_local_slices2_parts53(UINT32 Q, UINT32 R, UINT32 O);

    void calc_SLICE_system_slices4(UINT32 Q, UINT32 R, UINT32 O);
    void calc_SLICE_system_slices2(UINT32 Q, UINT32 R, UINT32 O);

    // partition internal methods
    void calc_PART_local_parts85432(UINT32 Q, UINT32 R);
    void calc_PART_local_parts7(UINT32 Q, UINT32 R);
    void calc_PART_local_parts6(UINT32 Q, UINT32 R);
    void calc_PART_local_parts1();

    void calc_PART_system_parts8(UINT32 Q, UINT32 R);
    void calc_PART_system_parts753(UINT32 Q, UINT32 R);
    void calc_PART_system_parts6(UINT32 Q, UINT32 R);
    void calc_PART_system_parts4(UINT32 Q, UINT32 R);
    void calc_PART_system_parts2(UINT32 Q, UINT32 R);
    void calc_PART_system_parts1();

    // L2_index, tag internal methods
    void calc_L2SET_local_slices4_L2sets16_parts876421(UINT32 Q, UINT32 O);
    void calc_L2SET_local_slices4_L2sets16_parts53(UINT32 Q, UINT32 O);

    void calc_L2SET_local_slices4_L2sets32_parts876421(UINT32 Q, UINT32 O);
    void calc_L2SET_local_slices4_L2sets32_parts53(UINT32 Q, UINT32 O);

    void calc_L2SET_local_slices2_L2sets16_parts876421(UINT32 Q, UINT32 O);
    void calc_L2SET_local_slices2_L2sets16_parts53(UINT32 Q, UINT32 O);

    void calc_L2SET_local_slices2_L2sets32_parts876421(UINT32 Q, UINT32 O);
    void calc_L2SET_local_slices2_L2sets32_parts53(UINT32 Q, UINT32 O);

    void calc_L2SET_system_L2sets16(UINT32 Q, UINT32 O);
    void calc_L2SET_system_L2sets32(UINT32 Q, UINT32 O);

    void calc_L2SECTOR_local(UINT32 O);
    void calc_L2SECTOR_system(UINT32 O);

protected:
    void computeRawAddress();
    void computePAKS(PA_TYPE &PA_lev1);
    UINT32 computePartitionInterleave();
    void computeQRO(const UINT32 PartitionInterleave, UINT32 &Q, UINT32 &R, UINT32 &O);
    void computePart(UINT32 Q, UINT32 R);
    void computeSlice(UINT32 Q, UINT32 R, UINT32 O);
    void computeL2Address(UINT32 Q, UINT32 O, const UINT32 PartitionInterleave);

    PA_TYPE m_adr; // actual physical address, should only be used for debugging or tags
    UINT32  m_partition;
    UINT32  m_slice;
    UINT32  m_l2set;
    UINT32  m_l2tag;
    UINT32  m_l2sector;
    UINT32  m_l2bank;
    UINT32  m_l2offset;

    enum KIND m_kind;
    enum PAGE_SIZE m_page_size;
    enum APERTURE  m_aperture;
    PA_TYPE PAKS;

  static const UINT32 SwizzleTable[4][4];

public:
    // config options
    static bool m_simplified_mapping;                  // FBConfig::getFBConfig()->simplifiedMapping()

    static int m_l2_sets;                              // *** Need to set m_l2_sets_bits as well, FBConfig::getFBConfig()->sets()
    static int m_l2_sets_bits;                         // Used in the creation of the xbar raw address field (padr) for fmodel

    static int m_l2_sectors;                           // FBConfig::getFBConfig()->sectors()
    static int m_l2_banks;                             // FBConfig::getFBConfig()->l2Banks()

    static int m_fb_slice_width;                       // FBConfig::SliceWidth expected to be 256, code not validated for other values
    static int m_fb_databus_width;                     // FBConfig::DataBusWidth expected to be 32, code not validated for other values
    static int m_dram_page_size;                       // FBConfig::DRAMPageSize expected to be 1024, code not validated for other values

    static int m_number_dram_subpartitions;            // FBConfig::getFBConfig()->subpartitions(), should be 2...
    static int m_number_dram_banks;                    // FBConfig::getFBConfig()->banks()
    static int m_exists_extbank;                       // FBConfig::getFBConfig()->extbank()
    static int m_number_xbar_l2_slices;                // FBConfig::getFBConfig()->slices()
    static int m_number_fb_partitions;                 // FBConfig::getFBConfig()->partitions()

    static int m_number_read_friend_trackers;          // FBConfig::getFBConfig()->readFriendTrackers()
    static int m_number_read_friend_maker_entries;     // FBConfig::getFBConfig()->readFriendMakerEntries()
    static int m_read_friend_make_associativity;       // FBConfig::getFBConfig()->readFriendMakerAssociativity()
    static int m_number_dirty_friend_trackers;         // FBConfig::getFBConfig()->dirtyFriendTrackers()
    static int m_number_dirty_friend_maker_entries;    // FBConfig::getFBConfig()->dirtyFriendMakerEntries()
    static int m_dirty_firend_tracker_associativity;   // FBConfig::getFBConfig()->dirtyFriendMakerAssociativity()

    static bool m_split_tile_mapping;                  // FBConfig::getFBConfig()->splitTileOverSubPartition(), mapping alternative

    static bool m_initialized;                         // guard against unitialized use

    static bool m_L2BankSwizzleSetting;

};
std::ostream& operator<<(std::ostream &os, const FermiPhysicalAddress &pa);

struct FermiFBAddress : FermiPhysicalAddress {
    FermiFBAddress() : FermiPhysicalAddress(), m_row(~0), m_bank(~0), m_extbank(~0), m_subpart(~0), m_col(~0) {}
    FermiFBAddress(const FermiFBAddress& _adr) : FermiPhysicalAddress(_adr), m_row(_adr.m_row), m_bank(_adr.m_bank), m_extbank(_adr.m_extbank), m_subpart(_adr.m_subpart), m_col(_adr.m_col) {};
    FermiFBAddress(PA_TYPE  _adr, const enum KIND _kind, const enum PAGE_SIZE _page_size, const enum APERTURE _aperture);
    FermiFBAddress(const FermiPhysicalAddress& _adr);
    UINT32  bank() const { return m_bank; }
    UINT32  extbank() const { return m_extbank; }
    UINT32  row() const { return m_row; }
    UINT32  column() const { return m_col; }
    UINT32  subPartition() const { return m_subpart; }
    UINT32  ftTag() const;

    UINT32  rdFtSelect() const;
    UINT32  rdFtSet() const;
    UINT32  wrFtSelect() const;
    UINT32  wrFtSet() const;

    std::string getString() const;

  private:
    // FB RBC methods
    void calc_RBC_slices4_L2sets16_drambanks8_extbank0();
    void calc_RBC_slices4_L2sets16_drambanks8_extbank1();

    void calc_RBC_slices4_L2sets16_drambanks16_extbank0();
    void calc_RBC_slices4_L2sets16_drambanks16_extbank1();

    void calc_RBC_slices4_L2sets32_drambanks8_extbank0();
    void calc_RBC_slices4_L2sets32_drambanks8_extbank1();

    void calc_RBC_slices4_L2sets32_drambanks16_extbank0();
    void calc_RBC_slices4_L2sets32_drambanks16_extbank1();

    void calc_RBC_slices2_L2sets16_drambanks8_extbank0();
    void calc_RBC_slices2_L2sets16_drambanks8_extbank1();

    void calc_RBC_slices2_L2sets16_drambanks16_extbank0();
    void calc_RBC_slices2_L2sets16_drambanks16_extbank1();

    void calc_RBC_slices2_L2sets32_drambanks8_extbank0();
    void calc_RBC_slices2_L2sets32_drambanks8_extbank1();

    void calc_RBC_slices2_L2sets32_drambanks16_extbank0();
    void calc_RBC_slices2_L2sets32_drambanks16_extbank1();

    void calc_RBC_slices4_common();
    void calc_RBC_slices2_common();

protected:
    void computeRBSC();
    UINT32 computeColumnBits();

    UINT32  m_row;
    UINT32  m_bank;
    UINT32  m_extbank;
    UINT32  m_subpart;
    UINT32  m_col;
};

//-------------------------
// Anchor mapping
// For details see section 7.1 in
// //hw/doc/gpu/fermi/fermi/design/Functional_Descriptions/Fermi_address_mapping.doc
struct FermiAnchorMapping {

    enum SIMPLIFIED_ALIGNMENT { SA_128B, SA_256B };

    FermiAnchorMapping(
                       UINT64 _padr,
                       UINT32 _slice,
                       const enum FermiPhysicalAddress::KIND _kind,
                       const enum FermiPhysicalAddress::APERTURE _aperture,
                       const enum FermiPhysicalAddress::PAGE_SIZE _page_size,
                       UINT32 _offset
                       );

    // returns the slice the request should be sent to
    UINT32 slice();
    // padr() returns a 128 byte aligned address ready to send to fb
    UINT64 padr( enum SIMPLIFIED_ALIGNMENT simplifiedAlignment=SA_256B );

    UINT64 compBitTableIndex();

    UINT64 m_newPadr;
    UINT32 m_newSlice;

    UINT64 m_padr;
    UINT32 m_slice;
    enum FermiPhysicalAddress::KIND m_kind;
    enum FermiPhysicalAddress::APERTURE  m_aperture;
    enum FermiPhysicalAddress::PAGE_SIZE m_page_size;
    UINT32 m_offset;

    // Look up tables used in anchor mapping
    static const UINT32 LUT_padr_mask_4slice[8];
    static const UINT32 LUT_slice_mask_4slice[8];
    static const UINT32 LUT_sysmem_padr_mask_4slice[8];
    static const UINT32 LUT_sysmem_slice_mask_4slice[8];
    static const UINT32 LUT_padr_mask_2slice[8];
    static const UINT32 LUT_slice_mask_2slice[8];
    static const UINT32 LUT_sysmem_padr_mask_2slice[8];
    static const UINT32 LUT_sysmem_slice_mask_2slice[8];
};

//-------------------------
// Reverse Mapping for both sysmem and peer mem
// For details see section 12 in
// //hw/doc/gpu/fermi/fermi/design/Functional_Descriptions/Fermi_address_mapping.doc
struct FermiSysmemPeerReverseMap {

    FermiSysmemPeerReverseMap(
                              UINT64 _padr,
                              UINT32 _node_id,
                              UINT32 _port_id,
                              UINT32 _sector_msk,
                              const enum FermiPhysicalAddress::KIND _kind,
                              const enum FermiPhysicalAddress::APERTURE _aperture,
                              const enum FermiPhysicalAddress::PAGE_SIZE _page_size
                              );

    UINT64 pa();
    UINT64 ramif_addr();
    UINT64 paks() {return m_paks;}   // for debugging

    UINT64 m_padr; // actual physical address
    UINT32 m_partition;
    UINT32 m_slice;
    UINT32 m_sector_msk;
    UINT64 m_pa;
    UINT64 m_paks;
    enum FermiPhysicalAddress::KIND m_kind;
    enum FermiPhysicalAddress::APERTURE  m_aperture;
    enum FermiPhysicalAddress::PAGE_SIZE m_page_size;
};

//-------------------------
// Reverse Mapping to get PA bit 8 for Z64 kinds
// For details see section 7 in
// //hw/doc/gpu/fermi/fermi/design/Functional_Descriptions/Fermi_address_mapping.doc
struct FermiZ64PA8ReverseMap {

    FermiZ64PA8ReverseMap(
                          UINT64 _padr,
                          UINT32 _slice,
                          const enum FermiPhysicalAddress::APERTURE _aperture,
                          const enum FermiPhysicalAddress::PAGE_SIZE _page_size
                          );

    UINT64 padrGobAligned();
    UINT32 sliceGobAligned();

    UINT64 m_alignedPadr;
    UINT64 m_alignedSlice;

    UINT64 m_padr;
    UINT32 m_slice;
    UINT64 m_pa8;
    enum FermiPhysicalAddress::APERTURE  m_aperture;
    enum FermiPhysicalAddress::PAGE_SIZE m_page_size;
};

#endif
