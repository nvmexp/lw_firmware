//
// PhysicalAddress.cpp
//
// Physical address mapping functions
#define LWSHARED_LIB_CODE
#include "fermi/gf100/kind_macros.h"
#include "fermi/gf100/dev_mmu.h"
#include "PhysicalAddress.h"
// #include "lw_ref_generated.h"

#define CHECK(x)
#define cslDebugGroup(x)
#define cslAssertMsg(x, y)
#define cslInfo(x)

// static char cslNameAmap[] = "amap";

FBConfig fbConfig;
int PhysicalAddress::SLICE_EMBEDDED=1;
amap_override_t PhysicalAddress::m_amap_override=OVERRIDE_NONE;
int AnchorMapping::SLICE_EMBEDDED=1;

#undef LWSHAREDDATA
#ifdef _WIN32
    #define LWSHAREDDATA  __declspec(dllexport)
#else
    #define LWSHAREDDATA  extern
#endif
LWSHAREDDATA int glw_partition_hem[];

//-------------------------
// PhysicalAddress class methods
//
PhysicalAddress::PhysicalAddress(UINT64 _adr, const PTEKind& _pte_kind, UINT32 aperture, mmu_page_size_t page_size, int source, const char *calledFromFile, const unsigned int calledFromLine) {
  m_adr =_adr;
  commonConstructor( _pte_kind, aperture, page_size, source, calledFromFile, calledFromLine);
}

PhysicalAddress::PhysicalAddress(UINT32 _adr, const PTEKind& _pte_kind, UINT32 aperture, mmu_page_size_t page_size, int source, const char *calledFromFile, const unsigned int calledFromLine) {
  m_adr =_adr;
  commonConstructor( _pte_kind, aperture, page_size, source, calledFromFile, calledFromLine);
}

void PhysicalAddress::commonConstructor(const PTEKind& _pte_kind, UINT32 aperture, mmu_page_size_t page_size, int source, const char *calledFromFile, const unsigned int calledFromLine) {
  m_page_size = ((page_size == PAGE_SIZE_SMALL) ? FermiPhysicalAddress::PS4K :
                 (_pte_kind.is64kPage() ? FermiPhysicalAddress::PS64K : FermiPhysicalAddress::PS128K));
  m_aperture = (aperture == ENUM_aperture_t_APERTURE_LOCAL_MEM ? FermiPhysicalAddress::LOCAL :
                (aperture == ENUM_aperture_t_APERTURE_P2P ? FermiPhysicalAddress::PEER : FermiPhysicalAddress::SYSTEM));
  m_kind = (_pte_kind.isZ64() ? FermiPhysicalAddress::BLOCKLINEAR_Z64 :
            (_pte_kind.isBlockLinear() ? FermiPhysicalAddress::BLOCKLINEAR_GENERIC : FermiPhysicalAddress::PITCHLINEAR)) ;
  if( m_aperture == FermiPhysicalAddress::LOCAL ) {
    switch ( m_amap_override ) {
    case OVERRIDE_NONE: break;
    case OVERRIDE_WITH_VIDMEM_4K: m_page_size = FermiPhysicalAddress::PS4K; break;
    case OVERRIDE_WITH_PEERMEM_4K: m_aperture = FermiPhysicalAddress::PEER; m_page_size = FermiPhysicalAddress::PS4K;break;
    case OVERRIDE_WITH_PEERMEM_64K:
      if( m_page_size == FermiPhysicalAddress::PS64K || m_page_size == FermiPhysicalAddress::PS128K ) {
        m_aperture = FermiPhysicalAddress::PEER;
        m_page_size = FermiPhysicalAddress::PS64K;
      }
      break;
    case OVERRIDE_WITH_PEERMEM_128K:
      if( m_page_size == FermiPhysicalAddress::PS128K ) {
        m_aperture = FermiPhysicalAddress::PEER;
        m_page_size = FermiPhysicalAddress::PS128K;
      }
      break;
    }
  }

  bool saveMapping = FermiPhysicalAddress::m_simplified_mapping;
  if( source == 10) {
    FermiPhysicalAddress::m_simplified_mapping = 0;
  }
  computeRawAddress();
  cslDebugGroup(( cslNameAmap, 50, "UINT64: %s %012llx -> %d:%d:%08x -> %012llx; ap=%s; page=%s; ",
                  (FermiPhysicalAddress::m_simplified_mapping ? "Simple" : "POR"),
                  m_adr,
                  slice(),
                  node_id(),
                  xbar_raw_256_byte_aligned(1),
                  fb_addr(),
                  (m_aperture == LOCAL ? "vidmem" : (m_aperture == SYSTEM ? "sysmem" : "p2p")),
                  (m_page_size == PS4K ? "4k" : (m_page_size == PS64K ? "64k" : "128k")) )
                << _pte_kind.getString()  // prints format=PITCH:VM128K
                << "; " << CSLN(source)
                << "file=" << calledFromFile
                << ":" << calledFromLine
                << "\n");

  FermiPhysicalAddress::m_simplified_mapping = saveMapping;
}

// constructor to xbar raw to ramif address
PhysicalAddress::PhysicalAddress(UINT32 _padr, UINT32 node_id, UINT32 port_id, UINT32 sector, UINT32 kind) {
  m_adr = isSimplifiedMapping() ? (_padr<<8)|(sector<<5) : ((sector&3) << 5);
  m_l2set = _padr & (m_l2_sets-1);
  m_l2tag = _padr >> m_l2_sets_bits;
  m_partition = node_id >> 1;
  m_slice = port_id;
  m_l2sector = sector;

  if( KIND_PITCH(kind)) {
    m_kind = FermiPhysicalAddress::PITCHLINEAR;
  } else if( KIND_BLOCKLINEAR(kind) && KIND_64BPP_Z(kind) ) {
    m_kind = FermiPhysicalAddress::BLOCKLINEAR_Z64;
    if( sector > 3 ) m_l2set ^= 1;
  } else if( KIND_BLOCKLINEAR(kind) ){
    m_kind = FermiPhysicalAddress::BLOCKLINEAR_GENERIC;
    if( sector > 3 ) m_l2set ^= 1;
  }
  cslDebugGroup(( cslNameAmap, 50,"PhysicalAddress(xbar_raw->ramif)(padr=%08x, l2tag=%08x, l2set=%04x, slice=%d, sector=%d, partition=%d)->%012llx\n",
                  _padr, m_l2tag, m_l2set, m_slice, sector, m_partition, fb_addr()
                  ));
}

// constructor to l2 address to ramif address (partition is not a node_id)
PhysicalAddress::PhysicalAddress(UINT32 _padr, UINT32 partition, UINT32 slice, UINT32 sector) {
  //  PhysicalAddress( _padr, partition<<1, slice, sector, LW_MMU_PTE_KIND_PITCH );
  m_adr = isSimplifiedMapping() ? (_padr<<7)|(sector<<5) : ((sector&3) << 5);
  m_l2set = _padr & (m_l2_sets-1);
  m_l2tag = _padr >> m_l2_sets_bits;
  m_partition = partition;
  m_slice = slice;
  m_l2sector = sector;

  cslDebugGroup(( cslNameAmap, 50,"PhysicalAddress(L2->ramif)(padr=%08x, l2tag=%08x, l2set=%04x, slice=%d, sector=%d, partition=%d)->%012llx\n\n",
                  _padr, m_l2tag, m_l2set, m_slice, sector, m_partition, fb_addr()
                  ));
}

// return xbar padr (Name is misleading due to legacy simpified mapping)
UINT32 PhysicalAddress::xbar_raw_256_byte_aligned(int simple_to_por) const {
  return xbar_raw_padr(simple_to_por);
}

// return xbar padr
UINT32 PhysicalAddress::xbar_raw_padr(int simple_to_por) const {
  if(simple_to_por) {
    return ((l2tag()<<m_l2_sets_bits)|l2set());
  }
  if( SLICE_EMBEDDED ) {
    return (UINT32)(m_adr>>8);
  } else if( !SLICE_EMBEDDED && FermiPhysicalAddress::m_simplified_mapping ) {
    return (UINT32)(m_adr>>10);
  } else {
    return ((l2tag()<<m_l2_sets_bits)|l2set());
  }
}

// return the xbar node id (partition)
UINT32 PhysicalAddress::node_id() const {
  UINT32 nodeID;

  switch (m_partition) {
  case 0: nodeID =  ENUM_node_id_t_NODE_FBP_0; break;
  case 1: nodeID =  ENUM_node_id_t_NODE_FBP_1; break;
  case 2: nodeID =  ENUM_node_id_t_NODE_FBP_2; break;
  case 3: nodeID =  ENUM_node_id_t_NODE_FBP_3; break;
  case 4: nodeID =  ENUM_node_id_t_NODE_FBP_4; break;
  case 5: nodeID =  ENUM_node_id_t_NODE_FBP_5; break;
  case 6: nodeID =  ENUM_node_id_t_NODE_FBP_6; break;
  case 7: nodeID =  ENUM_node_id_t_NODE_FBP_7; break;
  default:
    cslAssertMsg((false), ("ERROR:  unexpected partition ID = %d.  Expected value between 0 and 7.\n", m_partition));
    nodeID =  ENUM_node_id_t_NODE_FBP_0;
    break;
  }
  return nodeID;
}

const int PhysicalAddress::LUT_slice_swizzle_4slice[2][4] = {
  {0, 1, 2, 3},  // no swizzle
  {3, 2, 1, 0}   // swizzle
};
const int PhysicalAddress::LUT_slice_swizzle_2slice[2][2] = {
  {0, 1},  // no swizzle
  {1, 0}   // swizzle
};

// Backdoor call to get ramif address
UINT64 PhysicalAddress::fb_addr() const {
  UINT64 rval=0;

  if( !FermiPhysicalAddress::m_simplified_mapping && (m_aperture == ENUM_aperture_t_APERTURE_LOCAL_MEM) ) {
    // POR VIDMEM
    // UINT32 padr = (l2tag()<<m_l2_sets_bits)|l2set();
    switch ( FermiPhysicalAddress::m_number_xbar_l2_slices ) {
    case 4:
      // rval = ((UINT64)partition() << 29) | (bits<31,1>(padr)<<10) | (LUT_slice_swizzle_4slice[glw_partition_hem[partition()]][slice()]<<8) | (bit<0>(padr)<< 7) | bits<6,0>(m_adr);
      break;
    case 2:
      // rval = ((UINT64)partition() << 29) | (bits<31,1>(padr)<<9) | (LUT_slice_swizzle_2slice[glw_partition_hem[partition()]][slice()]<<8) | (bit<0>(padr)<< 7) | bits<6,0>(m_adr);
      break;
    default:
      cslAssertMsg((false), ("ERROR:  unexpected number of slices = %d.  Expected value 2 or 4.\n", FermiPhysicalAddress::m_number_xbar_l2_slices));
    }

    cslDebugGroup((cslNameAmap, 50, "PhysicalAddress::fb_addr: POR vidmem m_adr:%012llx -> sl/part/padr %d:%d:%08x -> fb_addr:%012llx; ap=%s; page=%s;\n",
                   m_adr,
                   slice(),
                   partition(),
                   padr,
                   rval,
                   (m_aperture == LOCAL ? "vidmem" : (m_aperture == SYSTEM ? "sysmem" : "p2p")),
                   (m_page_size == PS4K ? "4k" : (m_page_size == PS64K ? "64k" : "128k"))
                   ));
  } else if( !FermiPhysicalAddress::m_simplified_mapping &&
             (m_aperture == ENUM_aperture_t_APERTURE_SYS_MEM_COHERENT || m_aperture == ENUM_aperture_t_APERTURE_SYS_MEM_NONCOHERENT ) ) {
    // POR SYSMEM
    rval = m_adr;
    cslDebugGroup((cslNameAmap, 50, "PhysicalAddress::fb_addr: POR sysmem  m_adr:%012llx -> fb_addr:%012llx; ap=%s; page=%s;\n",
                   m_adr,
                   rval,
                   (m_aperture == LOCAL ? "vidmem" : (m_aperture == SYSTEM ? "sysmem" : "p2p")),
                   (m_page_size == PS4K ? "4k" : (m_page_size == PS64K ? "64k" : "128k"))
                   ));

  } else if( !FermiPhysicalAddress::m_simplified_mapping && m_aperture == ENUM_aperture_t_APERTURE_P2P ) {
    // POR P2P
    // cslAssertMsg((false), ("Should not be using P2P aperture through backdoor!!!\n"));
    rval = m_adr;
  } else {
    // SIMPLIFIED MAPPING
    rval = ((UINT64)partition() << 29) | m_adr;
    cslDebugGroup((cslNameAmap, 50, "PhysicalAddress::fb_addr: Simplified  fb_addr:%012llx\n", rval ));
  }
  return rval;
}

void PhysicalAddress::setSliceEmbedded( int i) { SLICE_EMBEDDED = i;}

void PhysicalAddress::setSimplifiedMapping( bool val ) {
  cslDebugGroup(( cslNameAmap, 50,"setSimplifiedMapping(%d)\n", val));
  FermiPhysicalAddress::m_simplified_mapping = val;
  if( val ) {
    SLICE_EMBEDDED = 1;
    AnchorMapping::SLICE_EMBEDDED = 1;
  } else {
    SLICE_EMBEDDED = 0;
    AnchorMapping::SLICE_EMBEDDED = 0;
  }
}

void PhysicalAddress::set_partitions( UINT32 p ) {
  cslDebugGroup(( cslNameAmap, 50,"Default number of partitions is %d and will change to %d\n",(int)m_number_fb_partitions,(int)p));
  FBConfig::getFBConfig()->m_partitions = p; m_number_fb_partitions = p;
}

void PhysicalAddress::set_slices( UINT32 s ) {
  cslDebugGroup(( cslNameAmap, 50,"Default number of slices is %d and will change to %d\n",(int)m_number_xbar_l2_slices,(int)s));
  FBConfig::getFBConfig()->m_slices = s; m_number_xbar_l2_slices = s;
}

void PhysicalAddress::set_l2_sets( UINT32 s ) {
  cslDebugGroup(( cslNameAmap, 50,"Default number of l2 sets is %d and will change to %d\n",(int)m_l2_sets,(int)s));
  FBConfig::getFBConfig()->m_sets = s; m_l2_sets = s;
  switch ( m_l2_sets ) {
  case 16: m_l2_sets_bits = 4; break;
  case 32: m_l2_sets_bits = 5; break;
  default: cslAssertMsg((false), ("ERROR:  unexpected number of l2 sets = %d.  Expected value 16 or 32.\n", m_l2_sets));
  }
}

UINT32 PhysicalAddress::get_partitions() {
  return m_number_fb_partitions;
}

std::string PhysicalAddress::getString() const
{
    std::ostringstream s;
    s << *this;
    return s.str();
}
std::ostream& operator<<(std::ostream &os, const PhysicalAddress &pa)
{
    os << "Adr= 0x" << std::hex << pa.address() << " Partition= " << std::dec << pa.partition() << " Slice= " << pa.slice();
    return os;
}

//-------------------------
// AnchorMapping class methods
//
// these are masks used in to transform the padr or slice based on offset into a 1K FB tile
//   mask is indexed by offset/128
const UINT32 AnchorMapping::LUT_padr_mask_4slice[8] = {
  0,1,2,3,0,1,2,3
};
const UINT32 AnchorMapping::LUT_slice_mask_4slice[8] = {
  0,0,0,0,1,1,1,1
};
// masks for sysmem/peer block linear
const UINT32 AnchorMapping::LUT_sysmem_padr_mask_4slice[8] = {
  0,1,2,3,0,1,2,3
};
const UINT32 AnchorMapping::LUT_sysmem_slice_mask_4slice[8] = {
  0,0,0,0,1,1,1,1
};
const UINT32 AnchorMapping::LUT_padr_mask_2slice[8] = {
  0,1,2,3,4,5,6,7
};
const UINT32 AnchorMapping::LUT_slice_mask_2slice[8] = {
  0,0,0,0,0,0,0,0
};
// masks for sysmem/peer block linear
const UINT32 AnchorMapping::LUT_sysmem_padr_mask_2slice[8] = {
  0,1,2,3,0,1,2,3
};
const UINT32 AnchorMapping::LUT_sysmem_slice_mask_2slice[8] = {
  0,0,0,0,1,1,1,1
};

AnchorMapping::AnchorMapping( UINT64 _padr,
                              int _slice,
                              int _kind,
                              int _aperture,
                              mmu_page_size_t _pagesize,
                              int _offset,
                              const char *calledFromFile,
                              const unsigned int calledFromLine ) :
  m_padr(_padr), m_slice(_slice), m_offset(_offset)
{
  cslAssertMsg((m_offset<1024), ("ERROR:  offset greater than 1024 = %d\n", m_offset));

  m_kind = fmod2amap_kind( _kind );
  m_aperture = fmod2amap_aperture( _aperture );
  m_pagesize = fmod2amap_pagesize( _pagesize, ENUM_mmu_big_page_size_t_BIG_PAGE_SIZE_64K);

  m_anchorMapping = new FermiAnchorMapping(m_padr, m_slice, m_kind, m_aperture, m_pagesize, m_offset);
  m_z64pa8reverseMap = new FermiZ64PA8ReverseMap(m_padr, m_slice, m_aperture, m_pagesize);
  cslDebugGroup(( cslNameAmap, 50, "AnchorMapping: %s offset:slice:padr=%d:%d:%08llx -> slice:padr=%d:%012llx; kind=%s; ap=%s; page=%s; ",
                  (FermiPhysicalAddress::m_simplified_mapping ? "Simple" : "POR"),
                  m_offset,
                  m_slice,
                  m_padr,
                  slice(),
                  padr(),
                  kind_string(m_kind),
                  aperture_string(m_aperture),
                  (m_pagesize == FermiPhysicalAddress::PS4K ? "4k" : "64k/128k"))
                << "file=" << calledFromFile
                << ":" << calledFromLine
                << "\n");
}

AnchorMapping::~AnchorMapping() {
  delete m_anchorMapping;
  delete m_z64pa8reverseMap;
}

// returns the slice the request should be sent to
UINT32 AnchorMapping::slice() {
  return m_anchorMapping->slice();
}

// padr() returns a 128 byte aligned address ready to send to fb
UINT64  AnchorMapping::padr( int POR ) {
  switch( POR ) {
  case 0: return m_anchorMapping->padr(FermiAnchorMapping::SA_128B);
  case 1: return m_anchorMapping->padr(FermiAnchorMapping::SA_256B);
  case 2: return m_anchorMapping->compBitTableIndex();
  default:
    cslAssertMsg((false), ("ERROR: invalid argument = %d, expect 0, 1, or 2\n", POR));
  }
  return m_anchorMapping->padr(FermiAnchorMapping::SA_128B);
}

// returns for padr for the anchor of the gob.
// need to reverse map to get PA[8].  If this is a one, then we are on the right half of the gob and we need to
// use the anchor mapping to get the padr for the left side (gob anchor).
UINT64 AnchorMapping::padrGobAligned() {
  // fb memory
  if(!FermiPhysicalAddress::m_simplified_mapping) {
    // POR MAPPING
    if(m_kind == FermiPhysicalAddress::BLOCKLINEAR_Z64) {
      // BLOCKLINEAR Z64
      return m_z64pa8reverseMap->padrGobAligned();
    } else {
      // PITCH/BLOCKLINEAR
      return m_padr;
    }
  }
  // SIMPLIFIED MAPPING
  return m_z64pa8reverseMap->padrGobAligned();
}

// returns for slice for the anchor of the gob.
UINT32 AnchorMapping::sliceGobAligned() {
  return m_z64pa8reverseMap->sliceGobAligned();
}

//-------------------------
// SysmemReverseMap class methods
//
SysmemReverseMap::SysmemReverseMap(
                                   UINT64 _padr,
                                   UINT32 _node_id,
                                   UINT32 _port_id,
                                   UINT32 _sector_msk,
                                   int _kind,
                                   int _pagesize,
                                   int _aperture,
                                   const char *calledFromFile,
                                   const unsigned int calledFromLine )
{
  m_kind = fmod2amap_kind( _kind );
  m_aperture = fmod2amap_aperture( _aperture );
  m_pagesize = fmod2amap_pagesize( _pagesize, ENUM_mmu_big_page_size_t_BIG_PAGE_SIZE_64K);

  m_revMap = new FermiSysmemPeerReverseMap(_padr, _node_id>>1, _port_id, _sector_msk, m_kind, m_aperture, m_pagesize);

  cslDebugGroup(( cslNameAmap, 50, "SysmemReverseMap: %s slice:part:padr=%d:%d:%08llx -> PA=%012llx; kind=%s; ap=%s; page=%s; ",
                  (FermiPhysicalAddress::m_simplified_mapping ? "Simple" : "POR"),
                  _port_id,
                  _node_id>>1,
                  _padr,
                  m_revMap->pa(),
                  kind_string(m_kind),
                  aperture_string(m_aperture),
                  (m_pagesize == FermiPhysicalAddress::PS4K ? "4k" : "64k/128k"))
                << "file=" << calledFromFile
                << ":" << calledFromLine
                << "\n");
}

SysmemReverseMap::~SysmemReverseMap() {
  delete m_revMap;
}

// returns the original PA
UINT64 SysmemReverseMap::pa() {
  return m_revMap->pa();
}

// used in the ltc unit testbench (cachegen.cpp)
UINT64 SysmemReverseMap::ramif_addr()
{
  return m_revMap->ramif_addr();
}

//-------------------------
// FBAddress class methods
//

UINT32 FBAddress::m_fba_colbitsAboveBit8=0;
UINT32 FBAddress::m_fba_drambankbits=3;

FBAddress::FBAddress(const PhysicalAddress& _adr) : FermiFBAddress(FermiPhysicalAddress(_adr)) {
    computeRBSC();
    m_fba_row = row();
    m_fba_bank = bank();
    m_fba_subpartition = subPartition();
    m_fba_column = column();
    m_fba_partition = partition();
}

FBAddress::FBAddress(UINT32 row, UINT32 bank, UINT32 subpartition, UINT32 column, UINT32 partition) {
     m_fba_row = row;
     m_fba_bank = bank;
     m_fba_subpartition = subpartition;
     m_fba_column = column;
     m_fba_partition = partition;
}

// Only works for POR mapping
UINT64 FBAddress::ramifAddr() {
    UINT64 rval =
      (m_fba_partition << 29) |
      (m_fba_row<<(11+m_fba_drambankbits+m_fba_colbitsAboveBit8)) |
      ((m_fba_column>>8)<<(11+m_fba_drambankbits)) |
      (m_fba_bank<< 11) | (bit<6>(m_fba_column)<< 10) |
      (m_fba_subpartition<< 9) |
      (bit<7>(m_fba_column)<< 8) |
      (bits<5,3>(m_fba_column)<<5);
    cslDebugGroup((cslNameAmap, 50, "FBAddress::ramifAddr: cfg(m_fba_drambankbits:%d m_fba_colbitsAboveBit8:%d) m_fba_partition:%d m_fba_subpartition:%d m_fba_bank:%d m_fba_row:0x%x m_fba_column:0x%x ramifAddr:0x%llx\n", m_fba_drambankbits, m_fba_colbitsAboveBit8, m_fba_partition, m_fba_subpartition, m_fba_bank, m_fba_row, m_fba_column, rval));
    return rval;
}

void FBAddress::set_num_banks( UINT32 b ) {
  cslInfo(("Changing DRAM Banks from %d to %d\n",m_number_dram_banks,b));
  m_number_dram_banks = b;
  switch (m_number_dram_banks) {
  case 4: m_fba_drambankbits = 2; break;
  case 8: m_fba_drambankbits = 3; break;
  case 16: m_fba_drambankbits = 4; break;
  default:
    cslAssertMsg((false), ("ERROR: Invalid number of banks( %d ).  Expected value=4,8,16.\n",m_number_dram_banks ));
    break;
  }
}

void FBAddress::set_dram_page_size( UINT32 p ) {
  cslInfo(("Changing DRAM Page size from %d to %d\n",m_dram_page_size,p));
  m_dram_page_size = p;
  switch (m_dram_page_size) {
    case 256: m_fba_colbitsAboveBit8 = 0; break;
    case 512: m_fba_colbitsAboveBit8 = 0; break;
    case 1024: m_fba_colbitsAboveBit8 = 0; break;
    case 2048: m_fba_colbitsAboveBit8 = 1; break;
    case 4096: m_fba_colbitsAboveBit8 = 2; break;
    case 8192: m_fba_colbitsAboveBit8 = 3; break;
    case 16384: m_fba_colbitsAboveBit8 = 4; break;
    case 32768: m_fba_colbitsAboveBit8 = 5; break;
  default:
   cslAssertMsg((false), ("ERROR: Invalid DRAM page size( %d ).  Expected value=256, 512, 1024, 2048, 4096, 8192, 16384, 32768.\n",m_dram_page_size ));
    break;
  }
}
void FBAddress::set_extbank_exists( UINT32 e ) {
  cslInfo(("external bank is %s\n",(e ? "ON" : "OFF")));
  m_exists_extbank = e;
}

extern "C"   LWR_U64  MemAccPLI2RAMIF_ADDR_NEW( int part, int row, int extBank, int intBank, int subpa, int column ) {
  if (PhysicalAddress().isSimplifiedMapping())
    return MemAccPLI2RAMIF_ADDR (part, row, extBank, intBank, subpa, column);
  else
    return FBAddress (row, intBank, subpa, column, part).ramifAddr() |
           // since the previous MemAccPLI2RAMIF_ADDR macro had the "position" info encoded in column, retain that functionality here
           (bits<2,0>((UINT32)column)<<2);
}
extern "C"   LWR_U64  MemAccPLI2RAMIF_ADDR_BACKDOOR_NEW( int part, int padr, int slice, int sector, int position) {
  if (PhysicalAddress().isSimplifiedMapping())
    return MemAccPLI2RAMIF_ADDR_BACKDOOR (part, padr, slice, sector, position);
  else
    return PhysicalAddress (padr, part, slice, sector).fb_addr() | ((position)<<2);
}
extern "C"   void  set_num_dram_banks( int b ) {
  FBAddress().set_num_banks(  b );
}
extern "C"   void  set_dram_page_size( int p ) {
  FBAddress().set_dram_page_size( p );
}
extern "C"   void  set_extbank_exists( bool e ) {
  FBAddress().set_extbank_exists( e );
}
extern "C"   void  set_simplified_mapping( bool s ) {
  PhysicalAddress().setSimplifiedMapping( s );
}

//----------------------------------------
// utility functions to translate from fmod kinds, apertures, pagesize to address mapping types
FermiPhysicalAddress::KIND fmod2amap_kind( int _kind ) {
  if( KIND_PITCH(_kind)) {
    return FermiPhysicalAddress::PITCHLINEAR;
  } else if( KIND_64BPP_Z(_kind) ){
    return FermiPhysicalAddress::BLOCKLINEAR_Z64;
  } else if( KIND_BLOCKLINEAR(_kind) ){
    return FermiPhysicalAddress::BLOCKLINEAR_GENERIC;
  //  } else if( KIND_NO_SWIZZLE(_kind) ){
  // return FermiPhysicalAddress::NO_SWIZZLE;
  }
  cslAssertMsg((false), ("ERROR:  invalid kind = %d\n", _kind));
  return FermiPhysicalAddress::PITCHLINEAR;
}
FermiPhysicalAddress::APERTURE fmod2amap_aperture( int _aperture ) {
  switch (_aperture) {
  case ENUM_aperture_t_APERTURE_LOCAL_MEM: return FermiPhysicalAddress::LOCAL; break;
  case ENUM_aperture_t_APERTURE_P2P: return FermiPhysicalAddress::PEER; break;
  case ENUM_aperture_t_APERTURE_SYS_MEM_COHERENT:
  case ENUM_aperture_t_APERTURE_SYS_MEM_NONCOHERENT: return FermiPhysicalAddress::SYSTEM; break;
  default:
    cslAssertMsg((false), ("ERROR:  invalid aperture = %d\n", _aperture));
  }
  return FermiPhysicalAddress::LOCAL;
}

FermiPhysicalAddress::PAGE_SIZE fmod2amap_pagesize( int _pagesize, int _big_pagesize ) {
  switch ( _pagesize ) {
  case ENUM_mmu_page_size_t_PAGE_SIZE_SMALL: return FermiPhysicalAddress::PS4K; break;
  case ENUM_mmu_page_size_t_PAGE_SIZE_BIG:
    switch (_big_pagesize) {
    case ENUM_mmu_big_page_size_t_BIG_PAGE_SIZE_64K: return FermiPhysicalAddress::PS64K; break;
    case ENUM_mmu_big_page_size_t_BIG_PAGE_SIZE_128K: return FermiPhysicalAddress::PS128K; break;
    default: cslAssertMsg((false), ("ERROR:  invalid big page size = %d\n", _big_pagesize));
    }
    break;
  default:
    cslAssertMsg((false), ("ERROR:  invalid page size = %d\n", _pagesize));
  }
  return FermiPhysicalAddress::PS128K;
}
const char* kind_string( FermiPhysicalAddress::KIND _kind ) {
  switch (_kind) {
  case FermiPhysicalAddress::PITCHLINEAR: return "PITCH"; break;
  case FermiPhysicalAddress::BLOCKLINEAR_Z64: return "Z64"; break;
  case FermiPhysicalAddress::BLOCKLINEAR_GENERIC: return "BLOCKLINEAR"; break;
  case FermiPhysicalAddress::PITCH_NO_SWIZZLE: return "PITCH_NO_SWIZZLE"; break;
  default:
    cslAssertMsg((false), ("ERROR:  invalid kind = %d\n", _kind));
  }
  return "KIND_ERROR";
}
const char* aperture_string( FermiPhysicalAddress::APERTURE _aperture ) {
  switch (_aperture) {
  case FermiPhysicalAddress::LOCAL: return "VIDMEM"; break;
  case FermiPhysicalAddress::SYSTEM: return "SYSMEM"; break;
  case FermiPhysicalAddress::PEER: return "PEER"; break;
  default:
    cslAssertMsg((false), ("ERROR:  invalid aperture = %d\n", _aperture));
  }
  return "APERTURE_ERROR";
}
const char* aperture_string( FermiPhysicalAddress::PAGE_SIZE _pagesize ) {
  switch (_pagesize) {
  case FermiPhysicalAddress::PS4K: return "4K"; break;
  case FermiPhysicalAddress::PS64K: return "64K"; break;
  case FermiPhysicalAddress::PS128K: return "128K"; break;
  default:
    cslAssertMsg((false), ("ERROR:  invalid pagesize = %d\n", _pagesize));
  }
  return "PAGESIZE_ERROR";
}
