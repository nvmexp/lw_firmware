#ifndef _PHYSICALADDRESS__H_
#define _PHYSICALADDRESS__H_

// Address Mapping Utilities
// cpp source file is in clib/lwshared/Memory/PhysicalAddress.cpp

#define AMAP_TESTBENCH
#define LWR_U64 UINT64

#include "memory_enum_types.h"
#include "mdiag/utils/types.h"

mmu_big_page_size_t_ENUM_DECL
mmu_page_size_t_ENUM_DECL

enum amap_override_t { OVERRIDE_NONE = 0, OVERRIDE_WITH_VIDMEM_4K = 1, OVERRIDE_WITH_PEERMEM_4K = 2, OVERRIDE_WITH_PEERMEM_64K = 3, OVERRIDE_WITH_PEERMEM_128K = 4 };

// Called from MemoryAccessPLI
//part doesn't work, only works with 3 bank bits, and 9 column bits
#define MemAccPLI2RAMIF_ADDR(part, row, extBank, intBank, subpa, column) \
  (LWR_U64)(((row)<<15)|(((column)&0x100)<<6)|((intBank)<<11)|(((column)&0x40)<<4)|((subpa)<<9)|(((column)&0x80)<<1)|(((column)&0x3f)<<2)|(((part)&0x7)<<29))

// Called from MemoryAccessPLI_Backdoor
// part doesn't work with this right now inserts sub above the first 3 col bits (padr[1:0],slice[0])
#define MemAccPLI2RAMIF_ADDR_BACKDOOR(part, padr, slice,  sector, position) \
  (LWR_U64)((((padr)&0xfffffffe)<<9) | (((slice)&0x3)<<8) | (((padr)&0x1)<<7) | ((sector)<<5) | ((position)<<2))

// Reverse mappings from RAMIF address, only used for debug
#define RAMIF2MemAccPLI_PADR_BACKDOOR(addr) (LWR_U32)((((addr)>>9) & 0xfffc) | (((addr)>>8) & 0x3))
#define RAMIF2MemAccPLI_SLICE_BACKDOOR(addr) (LWR_U32)((((addr)>>7) & 0x1) | (((addr)>>9) & 0x2))
#define RAMIF2MemAccPLI_SECTOR_BACKDOOR(addr) (LWR_U32)((((addr)>>5) & 0x3))
#define RAMIF2MemAccPLI_POSITION_BACKDOOR(addr) (LWR_U32)((((addr)>>2) & 0x7))

#ifdef __cplusplus

// #include "lwshared.h"

#include "FBConfig.h"
#include <iostream>
#include <string>
#include <sstream>
#include "CommonUtils.h"
#include "PTEKind.h"
#include "memory_enum_types.h"
// #include "csl.h"
#include "FermiAddressMapping.h"

// L2 to FB address mapping
#define OLDSTYLE 0
#define SLICE_APPENDED 1
#define POR_ADDR_CALC 2
#define RAMIF_ADDR_CALC POR_ADDR_CALC

#define RAM_ADDR(part,padr,noop,slice,sector)  \
(PhysicalAddress(padr, part, slice, sector).fb_addr())

//-------------------------
// PhysicalAddress - Forward Mapping
// Inherits from FermiPhysicalAddress in clib/include/FermiAddressMapping.h and
//   clib/lwShared/Memory/FermiAddressMapping.cpp
// For details see
// //hw/doc/gpu/fermi/fermi/design/Functional_Descriptions/Fermi_address_mapping.doc
struct PhysicalAddress : FermiPhysicalAddress {
    PhysicalAddress() : FermiPhysicalAddress() {}
    PhysicalAddress(UINT64 _adr, const PTEKind& _pte_kind, UINT32 aperture=ENUM_aperture_t_APERTURE_LOCAL_MEM, mmu_page_size_t page_size=PAGE_SIZE_BIG, int source=999, const char *calledFromFile="", const unsigned int calledFromLine=0);
    PhysicalAddress(UINT32 _adr, const PTEKind& _pte_kind, UINT32 aperture=ENUM_aperture_t_APERTURE_LOCAL_MEM, mmu_page_size_t page_size=PAGE_SIZE_BIG, int source=999, const char *calledFromFile="", const unsigned int calledFromLine=0);
    PhysicalAddress(UINT32 _padr, UINT32 node_id, UINT32 port_id, UINT32 sector, UINT32 kind);
    PhysicalAddress(UINT32 _padr, UINT32 partition, UINT32 slice, UINT32 sector);
    PhysicalAddress(const PhysicalAddress& _pa) : FermiPhysicalAddress(_pa) {}

    static int SLICE_EMBEDDED;
    static amap_override_t m_amap_override;

    // Common routine called by the constructor taking a PA argument
    void commonConstructor(const PTEKind& _pte_kind, UINT32 aperture, mmu_page_size_t page_size, int source, const char *calledFromFile, const unsigned int calledFromLine);

    // Setup configuration variables
    void setSliceEmbedded( int i);
    void setSimplifiedMapping( bool val );
    bool isSimplifiedMapping() { return FermiPhysicalAddress::m_simplified_mapping; }
    void overrideVidmemMapping( amap_override_t override ) { m_amap_override = override;}
    void set_partitions( UINT32 p );
    void set_slices( UINT32 s );
    void set_l2_sets( UINT32 s );
    UINT32 get_partitions();

    // Call to get the xbar padr and node_id (partition)
    UINT32 xbar_raw_padr(int simple_to_por=0) const;
    UINT32 node_id() const;

    UINT32 xbar_raw_256_byte_aligned(int simple_to_por=0) const;  // legacy call

    // backdoor call
    UINT64 fb_addr() const;

    bool operator==(const PhysicalAddress & _adr) const {return _adr.address() == m_adr;};
    std::string getString() const;

    PTEKind ptekind;

    // look up tables for the slice swizzling
    static const int LUT_slice_swizzle_4slice[2][4];
    static const int LUT_slice_swizzle_2slice[2][2];
    static const int swizzle_bits[8];    // temporary: should be replaced by Yongxiang's global

};
std::ostream& operator<<(std::ostream &os, const PhysicalAddress &pa);

//-------------------------
// Anchor mapping
// For details see section 7.1 in
// //hw/doc/gpu/fermi/fermi/design/Functional_Descriptions/Fermi_address_mapping.doc
struct AnchorMapping {

    AnchorMapping( UINT64 _padr,
                   int _slice,
                   int _kind,
                   int _aperture,
                   mmu_page_size_t _pagesize,
                   int _offset,
                   const char *calledFromFile="",
                   const unsigned int calledFromLine=0 );
    ~AnchorMapping();

    // returns the slice the request should be sent to
    UINT32 slice();
    // padr() returns a 128 byte aligned address ready to send to fb
    UINT64 padr( int POR=0 );

    UINT64 padrGobAligned();
    UINT32 sliceGobAligned();

    UINT64 m_padr;
    UINT32 m_slice;
    FermiPhysicalAddress::KIND m_kind;
    FermiPhysicalAddress::APERTURE m_aperture;
    FermiPhysicalAddress::PAGE_SIZE m_pagesize;
    UINT32 m_offset;

    FermiAnchorMapping *m_anchorMapping;
    FermiZ64PA8ReverseMap *m_z64pa8reverseMap;

    static int SLICE_EMBEDDED;
    void setSliceEmbedded( int i) { SLICE_EMBEDDED = i;}

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
struct SysmemReverseMap {

    SysmemReverseMap( UINT64 _padr,
                      UINT32 _node_id,
                      UINT32 _port_id,
                      UINT32 _sector_msk,
                      int _kind = 0,  // PITCH
                      int _pagesize = ENUM_mmu_page_size_t_PAGE_SIZE_SMALL,
                      int _aperture = ENUM_aperture_t_APERTURE_SYS_MEM_COHERENT,
                      const char *calledFromFile="",
                      const unsigned int calledFromLine=0 );
    ~SysmemReverseMap();

    UINT64 pa();
    UINT64 ramif_addr();

    FermiPhysicalAddress::KIND m_kind;
    FermiPhysicalAddress::APERTURE m_aperture;
    FermiPhysicalAddress::PAGE_SIZE m_pagesize;
    FermiSysmemPeerReverseMap *m_revMap;

    void setSliceEmbedded( int i) { }
};

//-------------------------
// FBAddress - Mapping to row, bank, subpartition, column
// Inherits from FermiFBAddress in clib/include/FermiAddressMapping.h and
//   clib/lwShared/Memory/FermiAddressMapping.cpp
// For details see
// //hw/doc/gpu/fermi/fermi/design/Functional_Descriptions/Fermi_address_mapping.doc
struct FBAddress : FermiFBAddress {
    FBAddress() : FermiFBAddress() {}
    FBAddress(const FBAddress& _adr) : FermiFBAddress(_adr) {};
    FBAddress(const FermiFBAddress& _adr) : FermiFBAddress(_adr) { computeRBSC(); }
    FBAddress(const PhysicalAddress& _adr);
    FBAddress(UINT32 row, UINT32 bank, UINT32 subpartition, UINT32 column, UINT32 partition);

    UINT64 fb_addr() const {
        std::string error_string;
        //if( FermiPhysicalAddress::m_simplified_mapping ) return ((UINT64)partition()<<29) | m_adr;
        //UINT32 padr = (row()<<5) | (bits<2,0>(bank()) << 2) | bits<6,5>(column());
        UINT32 bankbits = 0, colbits = 0;
        switch (m_number_dram_banks) {
        case 4: bankbits = 2; break;
        case 8: bankbits = 3; break;
        case 16: bankbits = 4; break;
        default:
            error_string = "Invalid number of banks\n";
            break;
        }
        switch (m_dram_page_size) {
        case 1024: colbits = 0; break;
        case 2048: colbits = 1; break;
        case 4096: colbits = 2; break;
        case 8192: colbits = 3; break;
        case 16384: colbits = 4; break;
        default:
            error_string = "Invalid DRAM page size\n";
            break;
        }
        // UINT32 padr = ((((row()<<colbits) | bits<10,8>(column())) << bankbits) | (bank())) << 2 | bits<6,5>(column());
        // UINT32 slice = (subPartition() << 1) | bit<7>(column());
        //return ((UINT64)(RAM_ADDR(partition(), padr, 0, slice, (bits<4,3>(column())) )));
        //printf("%x %x %08x %08x %08x %08x %08x\n",bankbits, colbits, partition(), row(), bank(), (subPartition()), column());

        return (partition() << 29)| (row()<<(11+bankbits+colbits)) | ((column()>>8)<<(11+bankbits)) | (bank()<< 11) | (bit<6>(column())<< 10) | (subPartition()<< 9)| (bit<7>(column())<< 8) | (bits<5,3>(column())<<5);
        //return (partition() << 29)| (row() << 14) | (bits<2,0>(bank()) << 11) | (bit<6>(column())<< 10) | (subPartition()<< 9)| (bit<7>(column())<< 8) | (bits<5,3>(column())<<5);
    }

    void set_num_banks( UINT32 b );
    void set_dram_page_size( UINT32 p );
    void set_extbank_exists( UINT32 e );

    UINT64 ramifAddr();
    UINT32 m_fba_row;
    UINT32 m_fba_bank;
    UINT32 m_fba_subpartition;
    UINT32 m_fba_column;
    UINT32 m_fba_partition;
    static UINT32 m_fba_colbitsAboveBit8;  // column bits above bit 8
    static UINT32 m_fba_drambankbits;
};

//-------------------------
// Colwert an xbar raw address to an ramif address - not lwrrently supported
struct Xbar2FBAddress {
    Xbar2FBAddress() : m_adr(0), m_partition(~0), m_slice(~0) {ptekind = PTEKind(PTEKind::PITCH, PTEKind::VM64K);}
    Xbar2FBAddress( UINT64 _adr, const PTEKind& _pte_kind, UINT32 _partition, UINT32 _slice ) :
      m_adr(_adr), m_partition(_partition), m_slice(_slice) {ptekind = PTEKind(PTEKind::PITCH, PTEKind::VM64K);}

    UINT64 fb_addr() const { return ((UINT64)m_partition << 29) |m_adr; }

    PTEKind ptekind;
    UINT64 m_adr; // actual physical address, should only be used for debugging or tags
    UINT32 m_partition;
    UINT32 m_slice;
};

// utility functions to translate from fmod kinds, apertures, pagesize to address mapping types
FermiPhysicalAddress::KIND fmod2amap_kind( int _kind );
FermiPhysicalAddress::APERTURE fmod2amap_aperture( int _aperture );
FermiPhysicalAddress::PAGE_SIZE fmod2amap_pagesize( int _pagesize, int _big_pagesize );
const char* kind_string( FermiPhysicalAddress::KIND );
const char* aperture_string( FermiPhysicalAddress::APERTURE );
const char* pagesize_string( FermiPhysicalAddress::PAGE_SIZE );

#endif   // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif

LWR_U64 MemAccPLI2RAMIF_ADDR_NEW( int part, int row, int extBank, int intBank, int subpa, int column );
LWR_U64 MemAccPLI2RAMIF_ADDR_BACKDOOR_NEW( int part, int padr, int slice, int sector, int position);
void set_num_dram_banks( int b );
void set_dram_page_size( int p );
void set_extbank_exists( bool e );
void set_simplified_mapping( bool s );
#ifdef __cplusplus
}
#endif

#endif
