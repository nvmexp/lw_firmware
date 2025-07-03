// need to make stdafx.h optional so this code can be shared by Fmodel as it, without change,
//  note that precompiled headers are turned off for this file's compile to make this conditionalization of stdafx.h work.
#ifdef FERMI_PERFSIM
#include "stdafx.h"
#endif

#define AMAP_TESTBENCH

#ifndef FERMI_PERFSIM
#include <iostream>
#include <string>
#include <sstream>
#ifndef AMAP_TESTBENCH
#include "csl.h"
#include "lwshared.h"
#endif
#endif

#include "FermiAddressMapping.h"
using namespace std;

//
// simplifiedMapping:
//     number of partitions
//     8     4     2     1
//   28:17 28:16 28:15 28:14  row (may include column bits)
//   16:14 15:13 14:12 13:11  bank
//   13:11 12:11 11:11 --:--  partition (when fewer partitions, fewer bits used here)
//   10:10 10:10 10:10 10:10  subpartition (xor'd with some higher-order bits to reduce clumping)
//   09:02 09:02 09:02 09:02  column (8 bits, others treated as "row" bits)
//
// non-simplified mapping is based on:
//   //hw/doc/gpu/fermi/fermi/design/Functional_Descriptions/Fermi_address_mapping.doc
//
// non-simplified mapping last verified for consistency with document:
//   8/28/06 by bhertzberg
//

#if defined(FERMI_PERFSIM) || defined(AMAP_TESTBENCH)
// this is for perfsim and testbench
bool FermiPhysicalAddress::m_simplified_mapping = false;                  // FBConfig::getFBConfig()->simplifiedMapping()
int FermiPhysicalAddress::m_number_fb_partitions=8;                 // FBConfig::getFBConfig()->partitions()
int FermiPhysicalAddress::m_number_xbar_l2_slices=4;                // FBConfig::getFBConfig()->slices()
#else
// this is for fmodel
bool FermiPhysicalAddress::m_simplified_mapping = true;                  // FBConfig::getFBConfig()->simplifiedMapping()
int FermiPhysicalAddress::m_number_fb_partitions=glw_scal_chip_num_fbps;                 // FBConfig::getFBConfig()->partitions()
int FermiPhysicalAddress::m_number_xbar_l2_slices=LW_SCAL_LITTER_NUM_MXBAR_SLICES;
#endif

int FermiPhysicalAddress::m_l2_sets=16;                           // *** Need to set m_l2_sets_bits as well, FBConfig::getFBConfig()->sets()
int FermiPhysicalAddress::m_l2_sets_bits=4;                       // Used in the creation of the xbar raw address field (padr) for fmodel
int FermiPhysicalAddress::m_l2_sectors=4;                         // FBConfig::getFBConfig()->sectors()
int FermiPhysicalAddress::m_l2_banks=4;                           // FBConfig::getFBConfig()->l2Banks()
int FermiPhysicalAddress::m_fb_slice_width=256;                   // FBConfig::SliceWidth expected to be 256, code not validated for other values
int FermiPhysicalAddress::m_fb_databus_width=32;                  // FBConfig::DataBusWidth expected to be 32, code not validated for other values
int FermiPhysicalAddress::m_dram_page_size=1024;                  // FBConfig::DRAMPageSize expected to be 1024, code not validated for other values
int FermiPhysicalAddress::m_number_dram_subpartitions=2;          // FBConfig::getFBConfig()->subpartitions(), should be 2...
int FermiPhysicalAddress::m_number_dram_banks=8;                  // FBConfig::getFBConfig()->banks()
int FermiPhysicalAddress::m_exists_extbank=0;                     // FBConfig::getFBConfig()->extbank()

// FIXME[Fung] change to correct value
int FermiPhysicalAddress::m_number_read_friend_trackers = 16;          // FBConfig::getFBConfig()->readFriendTrackers()
int FermiPhysicalAddress::m_number_read_friend_maker_entries = 64;     // FBConfig::getFBConfig()->readFriendMakerEntries()
int FermiPhysicalAddress::m_read_friend_make_associativity = 4;        // FBConfig::getFBConfig()->readFriendMakerAssociativity()
int FermiPhysicalAddress::m_number_dirty_friend_trackers = 16;         // FBConfig::getFBConfig()->dirtyFriendTrackers()
int FermiPhysicalAddress::m_number_dirty_friend_maker_entries = 32;    // FBConfig::getFBConfig()->dirtyFriendMakerEntries()
int FermiPhysicalAddress::m_dirty_firend_tracker_associativity = 4;    // FBConfig::getFBConfig()->dirtyFriendMakerAssociativity()

bool FermiPhysicalAddress::m_split_tile_mapping=false;                  // FBConfig::getFBConfig()->splitTileOverSubPartition(), mapping alternative
bool FermiPhysicalAddress::m_initialized = false;                   // guard against unitialized use
bool FermiPhysicalAddress::m_L2BankSwizzleSetting=false;

// stuff = SwizzleTable[ y[1:0] ][ x[1:0] ];
const UINT32 FermiPhysicalAddress::SwizzleTable[4][4] = {
    { 0, 1, 2, 3 },
    { 2, 3, 0, 1 },
    { 1, 2, 3, 0 },
    { 3, 0, 1, 2 }
};

// this constructor isn't typically used. the shell class in the particular model sets m_kind, m_page_size, and m_aperture according to the different model's implementations and calls computeRawAddress
FermiPhysicalAddress::FermiPhysicalAddress(PA_TYPE _adr,
                                           const enum FermiPhysicalAddress::KIND _kind,
                                           const enum FermiPhysicalAddress::PAGE_SIZE _page_size,
                                           const enum FermiPhysicalAddress::APERTURE _aperture) :
m_adr(_adr), m_kind(_kind), m_page_size(_page_size), m_aperture(_aperture)  {
    computeRawAddress();
}

void FermiPhysicalAddress::computeRawAddress() {
  PA_TYPE PA_lev1;
  UINT32 Quotient;
  UINT32 Offset;
  UINT32 Remainder;

  computePAKS(PA_lev1);
  UINT32 PartitionInterleave = computePartitionInterleave();
  computeQRO(PartitionInterleave, Quotient, Remainder, Offset);
  computePart(Quotient, Remainder);
  computeSlice(Quotient, Remainder, Offset);
  computeL2Address(Quotient, Offset, PartitionInterleave);

#ifndef FERMI_PERFSIM
#ifndef AMAP_TESTBENCH
    cslDebugGroup(("FermiAddressMapping", 80, "FermiAddressMapping: simplified_mapping = 0x%x\n", m_simplified_mapping));
    cslDebugGroup(("FermiAddressMapping", 80, "FermiAddressMapping: number_fb_partitions = 0x%x\n", m_number_fb_partitions));
    cslDebugGroup(("FermiAddressMapping", 80, "FermiAddressMapping: aperture = 0x%x\n", m_aperture));
    cslDebugGroup(("FermiAddressMapping", 80, "FermiAddressMapping: kind = 0x%x\n", m_kind));
    cslDebugGroup(("FermiAddressMapping", 80, "FermiAddressMapping: page_size = 0x%x\n", m_page_size));
    cslDebugGroup(("FermiAddressMapping", 80, "FermiAddressMapping: PAKS = 0x%x\n", PAKS));
    cslDebugGroup(("FermiAddressMapping", 80, "FermiAddressMapping: PA_lev1 = 0x%x\n", PA_lev1));
    cslDebugGroup(("FermiAddressMapping", 80, "FermiAddressMapping: m_partition = 0x%x\n", m_partition));
    cslDebugGroup(("FermiAddressMapping", 80, "FermiAddressMapping: m_l2set = 0x%x\n", m_l2set));
    cslDebugGroup(("FermiAddressMapping", 80, "FermiAddressMapping: m_l2tag = 0x%x\n", m_l2tag));
    cslDebugGroup(("FermiAddressMapping", 80, "FermiAddressMapping: m_slice = 0x%x\n", m_slice));
#endif
#endif

}

void FermiPhysicalAddress::computePAKS(PA_TYPE &PA_lev1) {
  std::string error_string;

#ifndef FERMI_PERFSIM
#ifndef AMAP_TESTBENCH
    cslDebugGroup(( "amap", 50, "addr: %011llx, aperture: %s, page_size: %s\n", m_adr, (m_aperture == LOCAL ? "LOCAL" : (m_aperture == SYSTEM ? "SYSTEM" : "PEER")), (m_page_size == PS4K ? "4K" : (m_page_size == PS64K ? "64K" : "128K")) ));
#endif
#endif

    PA_TYPE  PA_value = m_adr;

    PA_lev1 =0; // intermediate form of PA used in some equations

    // compute PAKS from PA_value
    PAKS = 0;
    if (m_simplified_mapping) {
      PAKS = PA_value;

    } else if ((m_aperture == SYSTEM) || (m_aperture == PEER)) {
      if ((m_aperture == SYSTEM) && ((m_page_size == PS64K) || (m_page_size == PS128K))) {
        std::stringstream ss;
        ss << "Sysmem cannot address large pages: " << m_page_size << std::endl;
        error_string = ss.str();
        PORTABLE_FAIL(error_string.c_str());
      }

      // 4 and 2 slice sysmem are the same
      switch (m_kind) {
      case PITCHLINEAR:
        switch (m_page_size) {
// SYSMEM PITCH
        case PS4K:
          // peer and sysmem
          calc_PAKS_system_pitchlinear_ps4k(PA_value);
          break;
        case PS64K:
        case PS128K:
          // peer mapping only
          calc_PAKS_system_pitchlinear_ps64k(PA_value);
          break;
        default:
          std::stringstream ss;
          ss << "Unsupported pagesize in address mapping. page size: " << m_page_size << std::endl;
          error_string = ss.str();
          PORTABLE_FAIL(error_string.c_str());
        }
        break;
      case BLOCKLINEAR_GENERIC:
      case BLOCKLINEAR_Z64:
// SYSMEM BLOCKLINEAR, Z64
        // peer and sysmem
        calc_PAKS_system_blocklinearZ64(PA_value);
        break;
      default:
        std::stringstream ss;
        ss << "Unsupported packing type. Kind: " << m_kind << std::endl;
        error_string = ss.str();
        PORTABLE_FAIL(error_string.c_str());
      }
    } else if (m_aperture == LOCAL) {
      switch (m_kind) {
      case PITCHLINEAR:
        switch (m_page_size) {

// VIDMEM PITCH 4K
        case PS4K:
          switch(m_number_fb_partitions) {
          case 8:
            calc_PAKS_local_pitchlinear_ps4k_parts8(PA_value, PA_lev1);
            break;
          case 4:
            calc_PAKS_local_pitchlinear_ps4k_parts4(PA_value, PA_lev1);
            break;
          case 2:
            calc_PAKS_local_pitchlinear_ps4k_parts2(PA_value);
            break;
          case 1:
            calc_PAKS_local_pitchlinear_ps4k_parts1(PA_value);
            break;
          case 6:
            calc_PAKS_local_pitchlinear_ps4k_parts6(PA_value);
            break;
          case 7:
            calc_PAKS_local_pitchlinear_ps4k_parts7(PA_value);
            break;
          case 5:
            calc_PAKS_local_pitchlinear_ps4k_parts5(PA_value, PA_lev1);
            break;
          case 3:
            calc_PAKS_local_pitchlinear_ps4k_parts3(PA_value);
            break;
          default:
            std::stringstream ss;
            ss << "Unsupported number of partitions in address mapping. Num partitions: " << m_number_fb_partitions << std::endl;
            error_string = ss.str();
            PORTABLE_FAIL(error_string.c_str());
          }
          break;

// VIDMEM PITCH 64K, 128K
        case PS64K:
        case PS128K:
          switch(m_number_fb_partitions) {
          case 8:
            calc_PAKS_local_pitchlinear_ps64k_parts8(PA_value);
            break;
          case 4:
            calc_PAKS_local_pitchlinear_ps64k_parts4(PA_value);
            break;
          case 2:
            calc_PAKS_local_pitchlinear_ps64k_parts2(PA_value);
            break;
          case 1:
            calc_PAKS_local_pitchlinear_ps64k_parts1(PA_value);
            break;
          case 6:
            calc_PAKS_local_pitchlinear_ps64k_parts6(PA_value);
            break;
          case 7:
            calc_PAKS_local_pitchlinear_ps64k_parts7(PA_value);
            break;
          case 5:
            calc_PAKS_local_pitchlinear_ps64k_parts5(PA_value);
            break;
          case 3:
            calc_PAKS_local_pitchlinear_ps64k_parts3(PA_value);
            break;
          default:
            std::stringstream ss;
            ss << "Unsupported number of partitions in address mapping. Num partitions: " << m_number_fb_partitions << std::endl;
            error_string = ss.str();
            PORTABLE_FAIL(error_string.c_str());
          }
          break;

        default:
          std::stringstream ss;
          ss << "Unsupported pagesize in address mapping. page size: " << m_page_size << std::endl;
          error_string = ss.str();
          PORTABLE_FAIL(error_string.c_str());
        }
        break;
        // all block linear kinds
      case BLOCKLINEAR_Z64:
      case BLOCKLINEAR_GENERIC:
        switch (m_page_size) {
          // VIDMEM 4K Block Linear
        case PS4K:
          switch(m_number_fb_partitions) {
          case 8:
            calc_PAKS_local_blocklinear_ps4k_parts8(PA_value, PA_lev1);
            break;
          case 4:
            calc_PAKS_local_blocklinear_ps4k_parts4(PA_value, PA_lev1);
            break;
          case 2:
            calc_PAKS_local_blocklinear_ps4k_parts2(PA_value);
            break;
          case 1:
            calc_PAKS_local_blocklinear_ps4k_parts1(PA_value);
            break;
          case 7:
            calc_PAKS_local_blocklinear_ps4k_parts7(PA_value);
            break;
          case 6:
            calc_PAKS_local_blocklinear_ps4k_parts6(PA_value);
            break;
          case 5:
            calc_PAKS_local_blocklinear_ps4k_parts5(PA_value, PA_lev1);
            break;
          case 3:
            calc_PAKS_local_blocklinear_ps4k_parts3(PA_value);
            break;
          default:
            std::stringstream ss;
            ss << "Unsupported number of partitions in address mapping. Num partitions: " << m_number_fb_partitions << std::endl;
            error_string = ss.str();
            PORTABLE_FAIL(error_string.c_str());
          }
          break;

// VIDMEM 64K, 128K  Block Linear
        case PS64K:
        case PS128K:
          switch(m_number_fb_partitions) {
          case 8:
            calc_PAKS_local_blocklinear_ps64k_parts8(PA_value);
            break;
          case 4:
            calc_PAKS_local_blocklinear_ps64k_parts4(PA_value);
            break;
          case 2:
            calc_PAKS_local_blocklinear_ps64k_parts2(PA_value);
            break;
          case 1:
            calc_PAKS_local_blocklinear_ps64k_parts1(PA_value);
            break;
          case 6:
            calc_PAKS_local_blocklinear_ps64k_parts6(PA_value);
            break;
          case 5:
            calc_PAKS_local_blocklinear_ps64k_parts5(PA_value);
            break;
          case 7:
            calc_PAKS_local_blocklinear_ps64k_parts7(PA_value);
            break;
          case 3:
            calc_PAKS_local_blocklinear_ps64k_parts3(PA_value);
            break;
          default:
            std::stringstream ss;
            ss << "Unsupported number of partitions in address mapping. Num partitions: " << m_number_fb_partitions << std::endl;
            error_string = ss.str();
            PORTABLE_FAIL(error_string.c_str());
          }
          break;

        default:
          std::stringstream ss;
          ss << "Unsupported pagesize in address mapping. Page size: " << m_page_size << std::endl;
          error_string = ss.str();
          PORTABLE_FAIL(error_string.c_str());
        }
        break;
      case PITCH_NO_SWIZZLE:
        PAKS = PA_value;
        break;
      default:
        std::stringstream ss;
        ss << "Unsupported packing type. Kind: " << m_kind << std::endl;
        error_string = ss.str();
        PORTABLE_FAIL(error_string.c_str());
      }

    } else {// junk
      error_string = "Not SYSTEM or LOCAL\n";
      PORTABLE_FAIL(error_string.c_str());
    }
}

UINT32 FermiPhysicalAddress::computePartitionInterleave() {
  std::string error_string;
  // UINT32 numPart = (UINT32)m_number_fb_partitions;

    UINT32 PartitionInterleave=0;  // set to some value because compiler complaining that this may be used uninitialized
    if ((m_aperture == SYSTEM) || (m_aperture == PEER)) {
      // should always be 2KB for 4 slices, 1KB for 2 slices
      PartitionInterleave =  512 * m_number_xbar_l2_slices; // 4 slice sysmem is 2KB, 2 slice sysmem is 1KB
      return PartitionInterleave;
    } else if (m_aperture == LOCAL) {
      // some non po2 partition configs are just too hard to map slice[1] above the partition bits
      switch (m_number_fb_partitions) {
      case 5:
      case 3:
        PartitionInterleave = 2048; // 2K for slice[1] below partition bits
        PORTABLE_ASSERT(PartitionInterleave == 2048);
        return PartitionInterleave;
        break;
      case 8:
      case 7:
      case 6:
      case 4:
      case 2:
      case 1:
        PartitionInterleave = 2048 / 2; // 2K/2 == 1K
        PORTABLE_ASSERT(PartitionInterleave == 1024);
        return PartitionInterleave;
        break;
      default:
        std::stringstream ss;
        ss << "Unsupported number of partitions in address mapping. Num partitions: " << m_number_fb_partitions << std::endl;
        error_string = ss.str();
        PORTABLE_FAIL(error_string.c_str());
      }
    } else {
        std::stringstream ss;
        ss << "Unsupported aperture : " << m_aperture << std::endl;
        error_string = ss.str();
        PORTABLE_FAIL(error_string.c_str());
    }
    std::stringstream ss;
    ss << "Should not be here!!! : " << m_aperture << std::endl;
    error_string = ss.str();
    PORTABLE_FAIL(error_string.c_str());
    return PartitionInterleave;  // this is just to quell the compiler warning, we should not hit this line
}

void FermiPhysicalAddress::computeQRO(const UINT32 PartitionInterleave, UINT32 &Q, UINT32 &R, UINT32 &O) {
  std::string error_string;
    // compute Q,R,O
    UINT32 numPart = (UINT32)m_number_fb_partitions;

    O = PAKS % PartitionInterleave;
    UINT32 tmp = PAKS / PartitionInterleave;
    Q = tmp / numPart;
    R = tmp % numPart;
}

void FermiPhysicalAddress::computePart(UINT32 Q, UINT32 R) {
  std::string error_string;
    // compute partition
    if ( m_simplified_mapping ) {
      m_partition = R;
    } else if ((m_aperture == SYSTEM) || (m_aperture == PEER)) {
      switch(m_number_fb_partitions) {
      case 8:
        calc_PART_system_parts8(Q, R);
        break;
      case 7:
      case 5:
      case 3:
        calc_PART_system_parts753(Q, R);
        break;
      case 6:
        calc_PART_system_parts6(Q, R);
        break;
      case 4:
        calc_PART_system_parts4(Q, R);
        break;
      case 2:
        calc_PART_system_parts2(Q, R);
        break;
      case 1:
        calc_PART_system_parts1();
        break;
      default:
        std::stringstream ss;
        ss << "Unsupported number of partitions in address mapping. Num partitions: " << m_number_fb_partitions << std::endl;
        error_string = ss.str();
        PORTABLE_FAIL(error_string.c_str());
        break;
      }

    } else if (m_aperture == LOCAL) {
      switch(m_number_fb_partitions) {
      case 8:
      case 5:
      case 4:
      case 3:
      case 2:
        calc_PART_local_parts85432(Q, R);
        break;
      case 7:
        calc_PART_local_parts7(Q, R);
        break;
      case 6:
        calc_PART_local_parts6(Q, R);
        break;
      case 1:
        calc_PART_local_parts1();
        break;
      default:
        std::stringstream ss;
        ss << "Unsupported number of partitions in address mapping. Num partitions: " << m_number_fb_partitions << std::endl;
        error_string = ss.str();
        PORTABLE_FAIL(error_string.c_str());
        break;
      }
    } else {// junk
      error_string = "Not SYSTEM or LOCAL\n";
      PORTABLE_FAIL(error_string.c_str());
    }
}

void FermiPhysicalAddress::computeSlice(UINT32 Q, UINT32 R, UINT32 O) {
  std::string error_string;
    // decode slice
    if ( m_simplified_mapping ) {
      // This simple mapping is generalized to allow some slight variations in parameters w/o
      // redoing the entire address mapping
      UINT32 tmp = O / m_fb_slice_width;
      m_slice = tmp % m_number_xbar_l2_slices;
// SYSMEM
    } else if ((m_aperture == SYSTEM) || (m_aperture == PEER)) {
      if (m_number_xbar_l2_slices == 4) {
        calc_SLICE_system_slices4(Q, R, O);
      } else if (m_number_xbar_l2_slices == 2) {
        calc_SLICE_system_slices2(Q, R, O);
      } else {
        std::stringstream ss;
        ss << "Unsupported number of slices in address mapping.: " << m_number_xbar_l2_slices << std::endl;
        error_string = ss.str();
        PORTABLE_FAIL(error_string.c_str());
      }
// VIDMEM
    } else if (m_aperture == LOCAL) {
      if (m_number_xbar_l2_slices == 4) {
        switch(m_number_fb_partitions) {
          //                m_slice =  ((bit<0>(Q) ^ bit<0>(m_partition)) << 1) | (bit<9>(O) ^ bit<1>(Q));
        case 7:
          calc_SLICE_local_slices4_parts7(Q, R, O);
          break;
        case 8:
        case 4:
        case 2:
        case 1:
          calc_SLICE_local_slices4_parts8421(Q, R, O);
          break;
        case 6:
          calc_SLICE_local_slices4_parts6(Q, R, O);
          break;
        case 5:
        case 3:
          calc_SLICE_local_slices4_parts53(Q, R, O);
          break;
        default:
          std::stringstream ss;
          ss << "Unsupported number of partitions in address mapping. Num partitions: " << m_number_fb_partitions << std::endl;
          error_string = ss.str();
          PORTABLE_FAIL(error_string.c_str());
          break;
        }
      } else if (m_number_xbar_l2_slices == 2) {
        switch(m_number_fb_partitions) {
        case 7:
          calc_SLICE_local_slices2_parts7(Q, R, O);
          break;
        case 8:
        case 4:
        case 2:
        case 1:
          calc_SLICE_local_slices2_parts8421(Q, R, O);
          break;
        case 6:
          calc_SLICE_local_slices2_parts6(Q, R, O);
          break;
        case 5:
        case 3:
          calc_SLICE_local_slices2_parts53(Q, R, O);
          break;
        default:
          std::stringstream ss;
          ss << "Unsupported number of partitions in address mapping. Num partitions: " << m_number_fb_partitions << std::endl;
          error_string = ss.str();
          PORTABLE_FAIL(error_string.c_str());
          break;
        }
      }  else  {  // junk
        error_string = "num_slices\n";
        PORTABLE_FAIL(error_string.c_str());
      }
    }  else  {  // junk
      error_string = "not SYSTEM or LOCAL\n";
      PORTABLE_FAIL(error_string.c_str());
    }
}

void FermiPhysicalAddress::computeL2Address(UINT32 Q, UINT32 O, const UINT32 PartitionInterleave) {
  std::string error_string;
      // L2 set/sector
    PORTABLE_ASSERT(m_l2_sectors == 4);
      if ( m_simplified_mapping ) {
        // This simple mapping is generalized to allow some slight variations in parameters w/o
        // redoing the entire address mapping
        UINT32 tmp = O / m_fb_slice_width;
        UINT32 O_lower = O % m_fb_slice_width;
        UINT32 O_upper = tmp / m_number_xbar_l2_slices;
        UINT32 l2addr = (Q * (PartitionInterleave / m_number_xbar_l2_slices)) |
                        (O_upper * m_fb_slice_width) |
                        O_lower;
        UINT32 index = (l2addr / (m_fb_databus_width * m_l2_sectors));
        m_l2set = index % m_l2_sets;
        m_l2tag = index / m_l2_sets;
        m_l2sector = (l2addr / m_fb_databus_width ) % m_l2_sectors;
        m_l2bank = m_l2sector % m_l2_banks;
        m_l2offset = l2addr % (m_fb_databus_width * m_l2_sectors);
// SYSMEM
    } else if ((m_aperture == SYSTEM) || (m_aperture == PEER)) {
      if (m_l2_sets == 16) {
        calc_L2SET_system_L2sets16(Q, O);
      } else if (m_l2_sets == 32) {
        calc_L2SET_system_L2sets32(Q, O);
      } else {
        std::stringstream ss;
        ss << "Sysmem illegal number of L2sets: " << m_l2_sets << std::endl;
        error_string = ss.str();
        PORTABLE_FAIL(error_string.c_str());
      }
// VIDMEM
    } else if (m_aperture == LOCAL) {

      UINT32 switch_key =
          m_number_xbar_l2_slices * 1000
        + m_l2_sets               * 10
        + m_number_fb_partitions;

        switch (switch_key) {

        // 4 slices, 16 L2 sets
        case 4168: case 4167: case 4166: case 4164: case 4162: case 4161:
          calc_L2SET_local_slices4_L2sets16_parts876421(Q, O);
          break;
        case 4165: case 4163:
          calc_L2SET_local_slices4_L2sets16_parts53(Q, O);
          break;

        // 4 slices, 32 L2 sets
        case 4328: case 4327: case 4326: case 4324: case 4322: case 4321:
          calc_L2SET_local_slices4_L2sets32_parts876421(Q, O);
          break;
        case 4325: case 4323:
          calc_L2SET_local_slices4_L2sets32_parts53(Q, O);
          break;

        // 2 slices, 16 L2 sets
        case 2168: case 2167: case 2166: case 2164: case 2162: case 2161:
          calc_L2SET_local_slices2_L2sets16_parts876421(Q, O);
          break;
        case 2165: case 2163:
          calc_L2SET_local_slices2_L2sets16_parts53(Q, O);
          break;

        // 2 slices, 32 L2 sets
        case 2328: case 2327: case 2326: case 2324: case 2322: case 2321:
          calc_L2SET_local_slices2_L2sets32_parts876421(Q, O);
          break;
        case 2325: case 2323:
          calc_L2SET_local_slices2_L2sets32_parts53(Q, O);
          break;

        default:
          std::stringstream ss;
          ss << "ERROR: Illegal configuration " << std::endl;
          ss << "Slices: " << m_number_xbar_l2_slices << std::endl;
          ss << "L2 sets: " << m_l2_sets << std::endl;
          ss << "Partitions: " << m_number_fb_partitions << std::endl;
          error_string = ss.str();
          PORTABLE_FAIL(error_string.c_str());
        }

    } else {
      std::stringstream ss;
      ss << "Not SYSTEM or LOCAL memory: " << m_aperture << std::endl;
      error_string = ss.str();
      PORTABLE_FAIL(error_string.c_str());
    }
}

FermiFBAddress::FermiFBAddress(PA_TYPE _adr, const enum KIND _kind, const enum PAGE_SIZE _page_size, const enum APERTURE _aperture) : FermiPhysicalAddress(_adr, _kind, _page_size, _aperture) {
    computeRBSC();
}

FermiFBAddress::FermiFBAddress(const FermiPhysicalAddress& _adr) : FermiPhysicalAddress(_adr) {
    computeRBSC();
}

void FermiFBAddress::computeRBSC()
{
    std::string error_string;
    // UINT32 num_col_bits = computeColumnBits();

    if ( m_simplified_mapping ) {
        // reconstruct L2 address, splice slice back in
        UINT32 l2addr = ((m_l2tag * m_l2_sets + m_l2set) * m_l2_sectors + m_l2sector) * m_fb_databus_width;
        UINT32 rbsc = ((l2addr / m_fb_slice_width) * m_number_xbar_l2_slices + m_slice) * m_fb_slice_width + (l2addr % m_fb_slice_width);
        m_col = (rbsc % m_dram_page_size)/sizeof(UINT32);
        UINT32 rbs = rbsc / m_dram_page_size;
        UINT32 rb = rbs / m_number_dram_subpartitions;
        // slight hashing necessary to balance streams across subpartitions (this may not be optimal, but seems good enough for this simple mapping)
        m_subpart = (rbs ^ (rb ^ (rb >> 1) ^ (rb >> 2) ^ (rb >> 3))) % m_number_dram_subpartitions;
        m_bank = rb % m_number_dram_banks;
        m_row = rb / m_number_dram_banks; // includes "column-as-row" bits

        if (m_split_tile_mapping) {
            error_string = "split tile mapping is needed, but not implemented right now\n";
            PORTABLE_FAIL(error_string.c_str());
        }
    } else {
// VIDMEM

      UINT32 switch_key =
          m_number_xbar_l2_slices * 100000
        + m_l2_sets               * 1000
        + m_number_dram_banks     * 10
        + m_exists_extbank;

        switch (switch_key) {

        // 4 slices, 16 L2 sets, 8 DRAM banks
        case 416080:
          calc_RBC_slices4_L2sets16_drambanks8_extbank0();
          break;
        case 416081:
          calc_RBC_slices4_L2sets16_drambanks8_extbank1();
          break;

        // 4 slices, 16 L2 sets, 16 DRAM banks
        case 416160:
          calc_RBC_slices4_L2sets16_drambanks16_extbank0();
          break;
        case 416161:
          calc_RBC_slices4_L2sets16_drambanks16_extbank1();
          break;

        // 4 slices, 32 L2 sets, 8 DRAM banks
        case 432080:
          calc_RBC_slices4_L2sets32_drambanks8_extbank0();
          break;
        case 432081:
          calc_RBC_slices4_L2sets32_drambanks8_extbank1();
          break;

        // 4 slices, 32 L2 sets, 16 DRAM banks
        case 432160:
          calc_RBC_slices4_L2sets32_drambanks16_extbank0();
          break;
        case 432161:
          calc_RBC_slices4_L2sets32_drambanks16_extbank1();
          break;

        // 2 slices, 16 L2 sets, 8 DRAM banks
        case 216080:
          calc_RBC_slices2_L2sets16_drambanks8_extbank0();
          break;
        case 216081:
          calc_RBC_slices2_L2sets16_drambanks8_extbank1();
          break;

        // 2 slices, 16 L2 sets, 16 DRAM banks
        case 216160:
          calc_RBC_slices2_L2sets16_drambanks16_extbank0();
          break;
        case 216161:
          calc_RBC_slices2_L2sets16_drambanks16_extbank1();
          break;

        // 2 slices, 32 L2 sets, 8 DRAM banks
        case 232080:
          calc_RBC_slices2_L2sets32_drambanks8_extbank0();
          break;
        case 232081:
          calc_RBC_slices2_L2sets32_drambanks8_extbank1();
          break;

        // 2 slices, 32 L2 sets, 16 DRAM banks
        case 232160:
          calc_RBC_slices2_L2sets32_drambanks16_extbank0();
          break;
        case 232161:
          calc_RBC_slices2_L2sets32_drambanks16_extbank1();
          break;

        default:
          std::stringstream ss;
          ss << "ERROR: Illegal configuration " << std::endl;
          ss << "Slices: " << m_number_xbar_l2_slices << std::endl;
          ss << "L2 sets: " << m_l2_sets << std::endl;
          ss << "DRAM banks: " << m_number_dram_banks << std::endl;
          ss << "DRAM_extbank: " << m_exists_extbank << std::endl;
          error_string = ss.str();
          PORTABLE_FAIL(error_string.c_str());
        }
    }
}

// If we shove expander DRAM select bits above row bits, power consumption is lower
UINT32 FermiFBAddress::computeColumnBits()
{
    UINT32 num_col_bits = 8;

    switch (m_dram_page_size) {
    case 1024:
        num_col_bits = 8;
        break;
    case 2048:
        num_col_bits = 9;
        break;
    case 4096:
        num_col_bits = 10;
        break;
    case 8192:
        num_col_bits = 11;
        break;
    default:
        break;
    }

    return num_col_bits;
}

UINT32 FermiFBAddress::ftTag() const
{
    // TODO: what's the new FT tag?
    return (row() * m_number_dram_banks + bank()) * m_number_dram_subpartitions + subPartition();
}

UINT32 FermiFBAddress::rdFtSelect() const
{
    UINT32 tag = ftTag();
    return  tag % m_number_read_friend_trackers;
}

UINT32 FermiFBAddress::rdFtSet() const
{
    UINT32 tag = ftTag();
    UINT32 simple_index = tag / m_number_read_friend_trackers;
    UINT32 num_sets = m_number_read_friend_maker_entries / m_read_friend_make_associativity;
    if (m_simplified_mapping) {
        // we perform some simple hashing to reduce associativity conflicts
        UINT32 hash_index = simple_index ^ (simple_index / num_sets);
        return hash_index % num_sets;
    } else {
        // the real address mapping performs enough swizzling at L2 level that additional swizzling here won't help
        return simple_index % num_sets;
    }
}

UINT32 FermiFBAddress::wrFtSelect() const
{
    UINT32 tag = ftTag();
    return  tag % m_number_dirty_friend_trackers;
}

UINT32 FermiFBAddress::wrFtSet() const
{
    UINT32 tag = ftTag();
    UINT32 simple_index = tag / m_number_dirty_friend_trackers;
    UINT32 num_sets = m_number_dirty_friend_maker_entries / m_dirty_firend_tracker_associativity;
    if (m_simplified_mapping) {
        // we perform some simple hashing to reduce associativity conflicts
        UINT32 hash_index = simple_index ^ (simple_index / num_sets);
        return hash_index % num_sets;
    } else {
        return (simple_index+((simple_index>>5)<<3)) % num_sets; // this assumes 5 bit set index for WR FT
    }
}

std::ostream& operator<<(std::ostream &os, const FermiPhysicalAddress &pa)
{
    os << "Adr= 0x" << std::hex << pa.address() << " Partition= " << std::dec << pa.partition() << " Slice= " << pa.slice();
    return os;
}

std::string FermiPhysicalAddress::getString() const
{
    std::ostringstream s;
    s << *this;
    return s.str();
}

std::string FermiFBAddress::getString() const
{
    std::ostringstream s;
    s<<FermiPhysicalAddress::getString()<<" bank= "<<bank()<<" SubPartition= "<<subPartition()<<" row= "<<row();
    return s.str();
}

// PAKS internal methods
// aperture_kind_pagesize_partitions
void FermiPhysicalAddress::calc_PAKS_local_pitchlinear_ps4k_parts8(PA_TYPE PA_value, PA_TYPE &PA_lev1){
            // Mapping with 4KB VM pages for vidmem is a galactic screw job.
            // part[2] and slice[1] are above the VM page.
            // Make it decent by PAKS[13] = PA_value[13] ^ PA_value[10] in both block linear and pitch

            // PAKS[6:0] = PA_value[6:0]                                                512B gob
            // PAKS[8:7] = PA_value[11:10]                                              512B gob
            // PAKS[9] = PA_value[7] ^ PA_value[14]                                     slice[0]
            // PAKS[11:10] = PA_value[9:8] + upper bit swizzle                          part[1:0]

            // Common mapping for both block linear and pitch
            // PAKS[12] = PA_value[12] ^ PA_value[13] ^ PA_value[16] ^ ...              part[2]
            // PAKS[13] = PA_value[13] ^ PA_lev1[10] ^ PA_value[15] ^ ...              slice[1]

              PA_lev1 = (bits<PA_MSB,12>(PA_value) << 12) |
              // part[1:0]
              // cannot swizzle PA_value[8] with PA_value[13] derivatives without aliasing
              (((bits<9,8>(PA_value)
                 + bits<11,10>(PA_value)
                 + bits<15,14>(PA_value) + bits<18,17>(PA_value) + bits<21,20>(PA_value)
                 + bits<24,23>(PA_value) + bits<27,26>(PA_value) + bits<30,29>(PA_value)
                 )%4) << 10) |

              // slice[0]
              // Won't help much to swizzle this bit.  Slice[1] has the bits all used.
              ((bit<7>(PA_value) ^ bit<14>(PA_value)) << 9) |

              // gob
              (bits<11,10>(PA_value) << 7) |
              bits<6,0>(PA_value);

            calc_PAKS_local_common_ps4k_parts8(PA_lev1);
}

void FermiPhysicalAddress::calc_PAKS_local_pitchlinear_ps4k_parts7(PA_TYPE PA_value){
              UINT32 swizzle1 = bit<11>(PA_value) ^  bit<12>(PA_value) ^  bit<13>(PA_value) ^ bit<14>(PA_value) ^ bit<15>(PA_value) ^
              bit<16>(PA_value) ^ bit<17>(PA_value) ^ bit<18>(PA_value) ^ bit<19>(PA_value) ^ bit<20>(PA_value) ^
              bit<21>(PA_value) ^ bit<22>(PA_value) ^ bit<23>(PA_value) ^ bit<24>(PA_value) ^ bit<25>(PA_value) ^
              bit<26>(PA_value) ^ bit<27>(PA_value) ^ bit<28>(PA_value) ^ bit<29>(PA_value) ^ bit<30>(PA_value) ^  bit<31>(PA_value);

              UINT32 swizzle = bits<15,14>(PA_value) + bits<17,16>(PA_value) + bits<19,18>(PA_value) + bits<21,20>(PA_value) +
              bits<23,22>(PA_value) +  bits<25,24>(PA_value) +  bits<27,26>(PA_value) +
              bits<29,28>(PA_value) +  bits<31,30>(PA_value);

            PAKS = (bits<PA_MSB,12>(PA_value) << 12) |

              (((bits<9,8>(PA_value) + bits<1,0>(swizzle) ) & 3) << 10) |

              ((bit<7>(PA_value) ^ bit<0>(swizzle1)) << 9) |
              (bits<11,10>(PA_value) << 7) |
              bits<6,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_pitchlinear_ps4k_parts6(PA_TYPE PA_value){
            PAKS = (bits<PA_MSB,12>(PA_value) << 12) |
              (bit<9>(PA_value) << 11) |

              // part[0]
              ((bit<8>(PA_value) ^ bit<9>(PA_value) ^ bit<10>(PA_value) ^ bit<13>(PA_value) ^ bit<15>(PA_value) ^ bit<17>(PA_value)
                ^ bit<19>(PA_value) ^ bit<21>(PA_value) ^ bit<23>(PA_value) ^ bit<25>(PA_value) ^ bit<27>(PA_value)
                ^ bit<29>(PA_value) ^ bit<31>(PA_value)) << 10) |

                // slice[0]
              ((bit<7>(PA_value) ^ bit<12>(PA_value) ^ bit<14>(PA_value) ^ bit<16>(PA_value)
                ^ bit<18>(PA_value) ^ bit<20>(PA_value) ^ bit<22>(PA_value)
                ^ bit<24>(PA_value) ^ bit<26>(PA_value) ^ bit<28>(PA_value)
                ^ bit<30>(PA_value)) << 9) |

              (bits<11,10>(PA_value) << 7) |
              bits<6,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_pitchlinear_ps4k_parts5(PA_TYPE PA_value, PA_TYPE &PA_lev1){
              UINT32 swizzle = bits<13,12>(PA_value) + bits<15,14>(PA_value) +  bits<17,16>(PA_value) +
              bits<19,18>(PA_value) + bits<21,20>(PA_value) +  bits<23,22>(PA_value) +
              bits<25,24>(PA_value) + bits<27,26>(PA_value) +  bits<29,28>(PA_value) +
              bits<31,30>(PA_value);

              PA_lev1 = (bits<PA_MSB,16>(PA_value) << 16) |
              (bits<14,11>(PA_value) << 12) |

              // 4KB portion
              // partition[0]
              (bit<9>(PA_value) << 11) |

              // slice[1:0]
              ((bit<8>(PA_value) ^ bit<1>(swizzle)) << 10) |
              ((bit<7>(PA_value) ^ bit<0>(swizzle)) << 9) |

              (bit<15>(PA_value) << 8) |
              (bit<10>(PA_value) << 7) |
              bits<6,0>(PA_value);

              calc_PAKS_local_common_ps4k_parts5(PA_lev1);
}

void FermiPhysicalAddress::calc_PAKS_local_pitchlinear_ps4k_parts4(PA_TYPE PA_value, PA_TYPE &PA_lev1){
              PA_lev1 = (bits<PA_MSB,12>(PA_value) << 12) |
              // part[1:0]
              // cannot swizzle PA_value[8] with PA_value[12] derivatives without aliasing
              (((bits<9,8>(PA_value)
                 + bits<11,10>(PA_value)
                 + (bit<13>(PA_value) <<1)
                 + bits<15,14>(PA_value) + bits<17,16>(PA_value) + bits<19,18>(PA_value)
                 + bits<21,20>(PA_value) + bits<23,22>(PA_value) + bits<25,24>(PA_value)
                 + bits<27,26>(PA_value) + bits<29,28>(PA_value) + bits<31,30>(PA_value)

                 )%4) << 10) |

              // slice[0]
              ((bit<7>(PA_value) ^ bit<13>(PA_value)) << 9) |

              // gob
              (bits<11,10>(PA_value) << 7) |
              bits<6,0>(PA_value);

              calc_PAKS_local_common_ps4k_parts4(PA_lev1);
}

void FermiPhysicalAddress::calc_PAKS_local_pitchlinear_ps4k_parts3(PA_TYPE PA_value){
            // both pitch and blocklinear 4KB mappings must swap bits 15, 11
              UINT32 swizzle = bits<12,11>(PA_value) + bits<14,13>(PA_value) +  bits<16,15>(PA_value) +
              bits<18,17>(PA_value) + bits<20,19>(PA_value) +  bits<22,21>(PA_value) +
              bits<24,23>(PA_value) + bits<26,25>(PA_value) +  bits<28,27>(PA_value) +
              bits<30,29>(PA_value);

            PAKS = (bits<PA_MSB,16>(PA_value) << 16) |
              (bits<14,11>(PA_value) << 12) |

              // 4KB
              // part[0]
              (bit<9>(PA_value) << 11) |

              // slice[1:0]
              ((bit<8>(PA_value) ^ bit<1>(swizzle)) << 10) |
              ((bit<7>(PA_value) ^ bit<0>(swizzle)) << 9) |

              (bit<15>(PA_value) << 8) |
              (bit<10>(PA_value) << 7) |
              bits<6,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_pitchlinear_ps4k_parts2(PA_TYPE PA_value){
            // looks a lot like 64KB, 128KB 2 partitions pitch
              UINT32 swizzle = bits<11,10>(PA_value) + bits<13,12>(PA_value) + bits<15,14>(PA_value) +
              bits<17,16>(PA_value) + bits<19,18>(PA_value) + bits<21,20>(PA_value) + bits<23,22>(PA_value) +
              bits<25,24>(PA_value) + bits<27,26>(PA_value) + bits<29,28>(PA_value) + bits<31,30>(PA_value)
              ;

            PAKS = (bits<PA_MSB,12>(PA_value) << 12) |
              ((bit<8>(PA_value) ^ bit<1>(swizzle)) << 11) |

              (((bit<9>(PA_value) + bit<10>(PA_value) + bit<11>(PA_value) + bit<12>(PA_value) + bit<13>(PA_value)
                + bit<14>(PA_value) + bit<15>(PA_value) + bit<16>(PA_value) + bit<17>(PA_value) + bit<18>(PA_value)
                + bit<19>(PA_value) + bit<20>(PA_value) + bit<21>(PA_value) + bit<22>(PA_value) + bit<23>(PA_value)
                + bit<24>(PA_value) + bit<25>(PA_value) + bit<26>(PA_value) + bit<27>(PA_value) + bit<28>(PA_value)
                + bit<29>(PA_value) + bit<30>(PA_value) + bit<31>(PA_value)) %2) << 10) |

              ((bit<7>(PA_value) ^ bit<0>(swizzle)) << 9) |

              (bits<11,10>(PA_value) << 7) |
              bits<6,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_pitchlinear_ps4k_parts1(PA_TYPE PA_value){
              UINT32 swizzle = bits<10,9>(PA_value) + bits<12,11>(PA_value) + bits<14,13>(PA_value) +
              bits<16,15>(PA_value) + bits<18,17>(PA_value) + bits<20,19>(PA_value) + bits<22,21>(PA_value) +
              bits<24,23>(PA_value) + bits<26,25>(PA_value) + bits<28,27>(PA_value) + bits<30,29>(PA_value) + bit<31>(PA_value)
              ;

            PAKS = (bits<PA_MSB,12>(PA_value) << 12) |

              (bit<9>(PA_value) << 11) |

              // slice[1:0]
              ((bit<8>(PA_value) ^ bit<1>(swizzle)) << 10) |
              ((bit<7>(PA_value) ^ bit<0>(swizzle)) << 9) |

              (bits<11,10>(PA_value) << 7) |
              bits<6,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_blocklinear_ps4k_parts8(PA_TYPE PA_value, PA_TYPE &PA_lev1){
            // Mapping with 4KB VM pages for vidmem is a galactic screw job.
            // part[2] and slice[1] are above the VM page.
            // Make it decent by PAKS[13] = PA_value[13] ^ PA_value[10] in both block linear and pitch

            // PAKS[8:0] = PA_value[8:0]                                                512B gob
            // PAKS[9] = PA_value[9] ^ PA_value[14]                                     slice[0]
            // PAKS[11:10] = PA_value[11:10] + PA_value[15:14] + PA_value[18:17] + ...  part[1:0]

            // Common mapping for both block linear and pitch
            // PAKS[12] = PA_value[12] ^ PA_value[13] ^ PA_value[16] ^ ...              part[2]
            // PAKS[13] = PA_value[13] ^ PA_lev1[10] ^ PA_value[15] ^ ...              slice[1]

             PA_lev1 = (bits<PA_MSB,12>(PA_value) << 12) |
              // part[1:0]
              // cannot swizzle PA_value[10] with PA_value[13] derivatives without aliasing
              (((bits<11,10>(PA_value)
                 + bits<15,14>(PA_value) + bits<18,17>(PA_value) + bits<21,20>(PA_value)
                 + bits<24,23>(PA_value) + bits<27,26>(PA_value) + bits<30,29>(PA_value)
                 )%4) << 10) |

              // slice[0]
              ((bit<9>(PA_value) ^ bit<14>(PA_value)) << 9) |

              // gob
              bits<8,0>(PA_value);

              calc_PAKS_local_common_ps4k_parts8(PA_lev1);
}

void FermiPhysicalAddress::calc_PAKS_local_blocklinear_ps4k_parts7(PA_TYPE PA_value){
            PAKS = (bits<PA_MSB,10>(PA_value) << 10) |
              // swizzle slice[0]
              ((bit<9>(PA_value)  ^ bit<14>(PA_value)  ^ bit<13>(PA_value) ) << 9) |
              bits<8,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_blocklinear_ps4k_parts6(PA_TYPE PA_value){
            PAKS = (bits<PA_MSB,14>(PA_value) << 14) |
              // slice[1]
              ((bit<13>(PA_value)) << 13) |

              // part[2:1] divide by 3 bits
              (((bit<12>(PA_value))&1) << 12) |
              (((bit<11>(PA_value))&1) << 11) |

              // part[0], power of 2 bit
               ((bit<10>(PA_value) ) << 10) | // post divide R swizzle

              // ((bit<10>(PA_value) ^ bit<14>(PA_value)) << 10) | // part = R

              // slice[0]
              ((bit<9>(PA_value)  ^ bit<13>(PA_value) ) << 9) |
              bits<8,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_blocklinear_ps4k_parts5(PA_TYPE PA_value, PA_TYPE &PA_lev1){
            // The constraint is to only swizzle 4KB of addressing.
            // But notice I'm swapping bits 11,15 relative to inside the 4KB region?
            // As long as I don't swizzle outside 64KB, and I use the same PA_lev1->PAKS
            // for both pitch and blocklinear, 4KB pages work.
            PA_lev1 = (bits<PA_MSB,16>(PA_value) << 16) |
              (bits<14,11>(PA_value) << 12) |

              // 4KB
              ((bit<10>(PA_value)&1 ) << 11) |
              // swizzle slices
              ((bit<15>(PA_value) ^ bit<10>(PA_value) ^ bit<14>(PA_value)) << 10) |
              ((bit<9>(PA_value) ^ bit<13>(PA_value)) << 9) |
              bits<8,0>(PA_value);

              calc_PAKS_local_common_ps4k_parts5(PA_lev1);
}

void FermiPhysicalAddress::calc_PAKS_local_blocklinear_ps4k_parts4(PA_TYPE PA_value, PA_TYPE &PA_lev1){
            PA_lev1 = (bits<PA_MSB,12>(PA_value) << 12) |
              // part[1:0]
              // cannot swizzle PA_value[10] with PA_value[12] derivatives without aliasing
              (((bits<11,10>(PA_value)
                 + (bit<13>(PA_value) <<1)
                 + bits<15,14>(PA_value)

                 )%4) << 10) |

              // slice[0]
              ((bit<9>(PA_value) ^ bit<13>(PA_value)) << 9) |

              // gob
              bits<8,0>(PA_value);

              calc_PAKS_local_common_ps4k_parts4(PA_lev1);
}

void FermiPhysicalAddress::calc_PAKS_local_blocklinear_ps4k_parts3(PA_TYPE PA_value){
            // both pitch and blocklinear 4KB mappings must swap bits 15, 11
            PAKS = (bits<PA_MSB,16>(PA_value) << 16) |
              (bits<14,11>(PA_value) << 12) |

              // 4KB
              ((bit<10>(PA_value)&1 ) << 11) |
              // swizzle slices
              ((bit<15>(PA_value) ^ bit<10>(PA_value) ) << 10) |
              ((bit<9>(PA_value) ^ bit<13>(PA_value)) << 9) |
              bits<8,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_blocklinear_ps4k_parts2(PA_TYPE PA_value){
              UINT32 swizzle = bit<10>(PA_value) ^ bit<13>(PA_value);

              PAKS = (bits<PA_MSB,12>(PA_value) << 12) |

                ((bit<11>(PA_value) ^ bit<0>(swizzle) ^ bit<14>(PA_value) ^ bit<15>(PA_value)
                  ) << 11) |

                (bit<0>(swizzle) << 10) |

                ((bit<9>(PA_value) ^ bit<13>(PA_value)) << 9) |
                bits<8,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_blocklinear_ps4k_parts1(PA_TYPE PA_value){
              UINT32 swizzle = bits<14,13>(PA_value) + bits<16,15>(PA_value);

              PAKS = (bits<PA_MSB,11>(PA_value) << 11) |
                // don't want diagonals
                // using ^ here so that carry doesn't propagate into bit10 from changing bit9, allows bit flipping in ROP
                ((bit<10>(PA_value) ^ bit<1>(swizzle)) << 10) |
                ((bit<9>(PA_value) ^ bit<0>(swizzle)) << 9) |

                bits<8,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_common_ps4k_parts8(PA_TYPE PA_lev1){
            // This section must be identical for pitch and block linear 8 partitions

            PAKS = (bits<PA_MSB,14>(PA_lev1) << 14) |

              // slice[1]
              // want to interleave slices vertically up to the top because same swizzling is used by pitch
              ((bit<13>(PA_lev1) ^ bit<10>(PA_lev1)

                ^ bit<15>(PA_lev1) ^ bit<16>(PA_lev1) ^ bit<17>(PA_lev1)
                ^ bit<18>(PA_lev1) ^ bit<19>(PA_lev1) ^ bit<20>(PA_lev1) ^ bit<21>(PA_lev1)
                ^ bit<22>(PA_lev1) ^ bit<23>(PA_lev1) ^ bit<24>(PA_lev1) ^ bit<25>(PA_lev1) ^ bit<26>(PA_lev1)
                ^ bit<27>(PA_lev1) ^ bit<28>(PA_lev1) ^ bit<29>(PA_lev1) ^ bit<30>(PA_lev1) ^ bit<31>(PA_lev1)

                ) << 13) |

              // part[2]
              ((bit<12>(PA_lev1) ^
                bit<13>(PA_lev1) ^ bit<16>(PA_lev1) ^ bit<19>(PA_lev1) ^ bit<22>(PA_lev1) ^ bit<25>(PA_lev1) ^
                bit<28>(PA_lev1) ^ bit<31>(PA_lev1)

                ) << 12) |

              bits<11,0>(PA_lev1);  // inside 4KB page

              // end section
}

void FermiPhysicalAddress::calc_PAKS_local_common_ps4k_parts5(PA_TYPE PA_lev1){
            // This section must be identical for pitch and block linear 5 partitions

            PAKS = (bits<PA_MSB,14>(PA_lev1) << 14) |
              (((~bit<13>(PA_lev1))&1 ) << 13) |
              (bit<12>(PA_lev1) << 12) |

              bits<11,0>(PA_lev1);  // inside 4KB page
}

void FermiPhysicalAddress::calc_PAKS_local_common_ps4k_parts4(PA_TYPE PA_lev1){
            // This section must be identical for pitch and block linear 4 partitions

            PAKS = (bits<PA_MSB,13>(PA_lev1) << 13) |

              // slice[1]
              // want to interleave slices vertically up to the top because same swizzling is used by pitch
              ((bit<12>(PA_lev1) ^ bit<10>(PA_lev1)

                ^ bit<15>(PA_lev1)

                ) << 12) |

              bits<11,0>(PA_lev1);  // inside 4KB page
}

void FermiPhysicalAddress::calc_PAKS_local_pitchlinear_ps64k_parts8(PA_TYPE PA_value){
              UINT32 swizzle = bits<10,9>(PA_value)  + bits<12,11>(PA_value) + bits<14,13>(PA_value) +
              bits<16,15>(PA_value) + bits<18,17>(PA_value) + bits<20,19>(PA_value) + bits<22,21>(PA_value) +
              bits<24,23>(PA_value) + bits<26,25>(PA_value) + bits<28,27>(PA_value) + bits<30,29>(PA_value)+
              bit<31>(PA_value)
              ;

            PAKS = (bits<PA_MSB,16>(PA_value) << 16) |
              (bits<13,12>(PA_value) << 14) |
              ((
                bit<8>(PA_value) ^ bit<1>(swizzle)
                ) << 13) |
              (((bits<11,9>(PA_value) + (bits<14,12>(PA_value)) + bits<17,15>(PA_value) + bits<20,18>(PA_value) + bits<23,21>(PA_value)
                 + bits<26,24>(PA_value) + bits<29,27>(PA_value) + bits<31,30>(PA_value)) %8) << 10) |

              ((bit<7>(PA_value) ^ bit<0>(swizzle)) << 9) |
              (bits<15,14>(PA_value) << 7) |
              bits<6,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_pitchlinear_ps64k_parts7(PA_TYPE PA_value){
              UINT32 swizzle1 = bit<11>(PA_value) ^  bit<12>(PA_value) ^  bit<13>(PA_value) ^ bit<14>(PA_value) ^ bit<15>(PA_value) ^
              bit<16>(PA_value) ^ bit<17>(PA_value) ^ bit<18>(PA_value) ^ bit<19>(PA_value) ^ bit<20>(PA_value) ^
              bit<21>(PA_value) ^ bit<22>(PA_value) ^ bit<23>(PA_value) ^ bit<24>(PA_value) ^ bit<25>(PA_value) ^
              bit<26>(PA_value) ^ bit<27>(PA_value) ^ bit<28>(PA_value) ^ bit<29>(PA_value) ^ bit<30>(PA_value) ^  bit<31>(PA_value);

              UINT32 swizzle = bits<16,14>(PA_value) + bits<19,17>(PA_value) + bits<22,20>(PA_value) +
              bits<25,23>(PA_value) + bits<28,26>(PA_value) + bits<31,29>(PA_value);

            PAKS = (bits<PA_MSB,16>(PA_value) << 16) |
              (bits<13,12>(PA_value) << 14) |
              ((bit<8>(PA_value)) << 13) |

              (((bits<11,9>(PA_value) + bits<2,0>(swizzle)) & 7) << 10) |

              ((bit<7>(PA_value) ^ bit<0>(swizzle1)) << 9) |
              (bits<15,14>(PA_value) << 7) |
              bits<6,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_pitchlinear_ps64k_parts6(PA_TYPE PA_value){
            PAKS = (bits<PA_MSB,16>(PA_value) << 16) |
              (bits<13,9>(PA_value) << 11) |

              // part[0]
              ((bit<8>(PA_value) ^ bit<9>(PA_value) ^ bit<10>(PA_value) ^ bit<13>(PA_value) ^ bit<15>(PA_value) ^ bit<17>(PA_value)
                ^ bit<19>(PA_value) ^ bit<21>(PA_value) ^ bit<23>(PA_value) ^ bit<25>(PA_value) ^ bit<27>(PA_value)
                ^ bit<29>(PA_value) ^ bit<31>(PA_value)) << 10) |

                // slice[0]
              ((bit<7>(PA_value) ^ bit<12>(PA_value) ^ bit<14>(PA_value) ^ bit<16>(PA_value)
                ^ bit<18>(PA_value) ^ bit<20>(PA_value) ^ bit<22>(PA_value)
                ^ bit<24>(PA_value) ^ bit<26>(PA_value) ^ bit<28>(PA_value)
                ^ bit<30>(PA_value)) << 9) |

              (bits<15,14>(PA_value) << 7) |
              bits<6,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_pitchlinear_ps64k_parts5(PA_TYPE PA_value){
              UINT32 swizzle = bits<13,12>(PA_value) + bits<15,14>(PA_value) +  bits<17,16>(PA_value) +
              bits<19,18>(PA_value) + bits<21,20>(PA_value) +  bits<23,22>(PA_value) +
              bits<25,24>(PA_value) + bits<27,26>(PA_value) +  bits<29,28>(PA_value) +
              bits<31,30>(PA_value);

            PAKS = (bits<PA_MSB,16>(PA_value) << 16) |
              (bits<13,12>(PA_value) << 14) |
              // partitions
              (bits<11,9>(PA_value) << 11) |

              // slice[1:0]
              ((bit<8>(PA_value) ^ bit<1>(swizzle)) << 10) |
              ((bit<7>(PA_value) ^ bit<0>(swizzle)) << 9) |

              (bits<15,14>(PA_value) << 7) |
              bits<6,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_pitchlinear_ps64k_parts4(PA_TYPE PA_value){
              UINT32 swizzle = bits<12,11>(PA_value) + bits<14,13>(PA_value) + bits<16,15>(PA_value) + bits<18,17>(PA_value) + bits<20,19>(PA_value) +
              bits<22,21>(PA_value) + bits<24,23>(PA_value) + bits<26,25>(PA_value) + bits<28,27>(PA_value) + bits<30,29>(PA_value) +
              bit<31>(PA_value);

            PAKS = (bits<PA_MSB,16>(PA_value) << 16) |
              (bits<13,11>(PA_value) << 13) |
              ((bit<8>(PA_value) ^ bit<1>(swizzle)) << 12) |

              (((bits<10,9>(PA_value)
                 + bits<1,0>(swizzle)  ) %4) << 10) |

              ((bit<7>(PA_value) ^ bit<0>(swizzle)) << 9) |
              (bits<15,14>(PA_value) << 7) |
              bits<6,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_pitchlinear_ps64k_parts3(PA_TYPE PA_value){
              UINT32 swizzle = bits<12,11>(PA_value) + bits<14,13>(PA_value) +  bits<16,15>(PA_value) +
              bits<18,17>(PA_value) + bits<20,19>(PA_value) +  bits<22,21>(PA_value) +
              bits<24,23>(PA_value) + bits<26,25>(PA_value) +  bits<28,27>(PA_value) +
              bits<30,29>(PA_value);

            PAKS = (bits<PA_MSB,16>(PA_value) << 16) |
              (bits<13,11>(PA_value) << 13) |
              // partitions
              (bits<10,9>(PA_value) << 11) |

              // slice[1:0]
              ((bit<8>(PA_value) ^ bit<1>(swizzle)) << 10) |
              ((bit<7>(PA_value) ^ bit<0>(swizzle)) << 9) |

              (bits<15,14>(PA_value) << 7) |
              bits<6,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_pitchlinear_ps64k_parts2(PA_TYPE PA_value){
              UINT32 swizzle = bits<11,10>(PA_value) + bits<13,12>(PA_value) + bits<15,14>(PA_value) +
              bits<17,16>(PA_value) + bits<19,18>(PA_value) + bits<21,20>(PA_value) + bits<23,22>(PA_value) +
              bits<25,24>(PA_value) + bits<27,26>(PA_value) + bits<29,28>(PA_value) + bits<31,30>(PA_value)
              ;

            PAKS = (bits<PA_MSB,16>(PA_value) << 16) |
              (bits<13,10>(PA_value) << 12) |
              ((bit<8>(PA_value) ^ bit<1>(swizzle)) << 11) |

              (((bit<9>(PA_value) + bit<10>(PA_value) + bit<11>(PA_value) + bit<12>(PA_value) + bit<13>(PA_value)
                + bit<14>(PA_value) + bit<15>(PA_value) + bit<16>(PA_value) + bit<17>(PA_value) + bit<18>(PA_value)
                + bit<19>(PA_value) + bit<20>(PA_value) + bit<21>(PA_value) + bit<22>(PA_value) + bit<23>(PA_value)
                + bit<24>(PA_value) + bit<25>(PA_value) + bit<26>(PA_value) + bit<27>(PA_value) + bit<28>(PA_value)
                + bit<29>(PA_value) + bit<30>(PA_value) + bit<31>(PA_value)) %2) << 10) |

              ((bit<7>(PA_value) ^ bit<0>(swizzle)) << 9) |

              (bits<15,14>(PA_value) << 7) |
              bits<6,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_pitchlinear_ps64k_parts1(PA_TYPE PA_value){
              UINT32 swizzle = bits<10,9>(PA_value) + bits<12,11>(PA_value) + bits<14,13>(PA_value) +
              bits<16,15>(PA_value) + bits<18,17>(PA_value) + bits<20,19>(PA_value) + bits<22,21>(PA_value) +
              bits<24,23>(PA_value) + bits<26,25>(PA_value) + bits<28,27>(PA_value) + bits<30,29>(PA_value) + bit<31>(PA_value)
              ;

            PAKS = (bits<PA_MSB,16>(PA_value) << 16) |
              (bits<13,9>(PA_value) << 11) |

              // slice[1:0]
              ((bit<8>(PA_value) ^ bit<1>(swizzle)) << 10) |
              ((bit<7>(PA_value) ^ bit<0>(swizzle)) << 9) |

              (bits<15,14>(PA_value) << 7) |
              bits<6,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_blocklinear_ps64k_parts8(PA_TYPE PA_value){
            /*
This looks better for the slices.
Needs slight tweaking.

              UINT32 swizzle = bits<12,10>(PA_value)
              + (bit<15>(PA_value) << 1)
              + (bit<14>(PA_value) << 0)
              + (bit<13>(PA_value) << 2)
              //              + bits<15,13>(PA_value)*5
              + bits<17,16>(PA_value);

            PAKS = (bits<PA_MSB,14>(PA_value) << 14) |
              // want to interleave slices vertically across all pitches up to 256MB
              ((bit<13>(PA_value) ^ bit<0>(swizzle)

                ^ bit<18>(PA_value)  ^ bit<19>(PA_value) ^ bit<20>(PA_value) ^ bit<21>(PA_value)
                ^ bit<22>(PA_value) ^ bit<23>(PA_value) ^ bit<24>(PA_value) ^ bit<25>(PA_value) ^ bit<26>(PA_value)
                ^ bit<27>(PA_value)

                ) << 13) |

              (bits<2,0>(swizzle) << 10) |

              ((bit<9>(PA_value) ^ bit<14>(PA_value)) << 9) |
              bits<8,0>(PA_value);
            break;
            */

            UINT32 swizzle = bits<12,10>(PA_value) + bits<15,13>(PA_value)*5 + bits<17,16>(PA_value);

            PAKS = (bits<PA_MSB,14>(PA_value) << 14) |
              // want to interleave slices vertically across all pitches up to 256MB
              ((bit<13>(PA_value) ^ bit<0>(swizzle)

                ^ bit<18>(PA_value)  ^ bit<19>(PA_value) ^ bit<20>(PA_value) ^ bit<21>(PA_value)
                ^ bit<22>(PA_value) ^ bit<23>(PA_value) ^ bit<24>(PA_value) ^ bit<25>(PA_value) ^ bit<26>(PA_value)
                ^ bit<27>(PA_value)

                ) << 13) |

              (bits<2,0>(swizzle) << 10) |

              ((bit<9>(PA_value) ^ bit<14>(PA_value)) << 9) |
              bits<8,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_blocklinear_ps64k_parts7(PA_TYPE PA_value){
            PAKS = (bits<PA_MSB,10>(PA_value) << 10) |
              // swizzle slice[0]
              ((bit<9>(PA_value)  ^ bit<14>(PA_value)  ^ bit<13>(PA_value) ) << 9) |
              bits<8,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_blocklinear_ps64k_parts6(PA_TYPE PA_value){
            PAKS = (bits<PA_MSB,14>(PA_value) << 14) |
              // slice[1]
              ((bit<13>(PA_value)) << 13) |

              // part[2:1] divide by 3 bits
              (((bit<12>(PA_value))&1) << 12) |
              (((bit<11>(PA_value))&1) << 11) |

              // part[0], power of 2 bit
               ((bit<10>(PA_value) ) << 10) | // post divide R swizzle

              // ((bit<10>(PA_value) ^ bit<14>(PA_value)) << 10) | // part = R

              // slice[0]
              ((bit<9>(PA_value)  ^ bit<13>(PA_value) ) << 9) |
              bits<8,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_blocklinear_ps64k_parts5(PA_TYPE PA_value){
            PAKS = (bits<PA_MSB,16>(PA_value) << 16) |
              (bits<14,13>(PA_value) << 14) |
              ((~bit<12>(PA_value)&1 ) << 13) |
              ((bit<11>(PA_value)&1 ) << 12) |
              ((bit<10>(PA_value)&1 ) << 11) |
              // swizzle slices
              ((bit<15>(PA_value) ^ bit<10>(PA_value)  ^ bit<14>(PA_value) ) << 10) |
              ((bit<9>(PA_value) ^ bit<13>(PA_value)) << 9) |
              bits<8,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_blocklinear_ps64k_parts4(PA_TYPE PA_value){
            // swizzle = SwizzleTable[ y[1:0] ][ x[1:0] ];
             UINT32 swizzle = SwizzleTable[ bits<14,13>(PA_value) ][ bits<11,10>(PA_value) ];

             //  swizzle = bits<11,10>(PA_value) + (bit<13>(PA_value) <<1) + bits<15,14>(PA_value);

              PAKS = (bits<PA_MSB,13>(PA_value) << 13) |

                ((bit<12>(PA_value) ^ bit<0>(swizzle) ^ bit<15>(PA_value)
                  ) << 12) |

                (bits<1,0>(swizzle) << 10) |

                ((bit<9>(PA_value) ^ bit<13>(PA_value)) << 9) |
                bits<8,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_blocklinear_ps64k_parts3(PA_TYPE PA_value){
            PAKS = (bits<PA_MSB,16>(PA_value) << 16) |
              (bits<14,12>(PA_value) << 13) |
              ((bit<11>(PA_value)&1 ) << 12) |
              ((bit<10>(PA_value)&1 ) << 11) |
              // swizzle slices
              ((bit<15>(PA_value) ^ bit<10>(PA_value) ) << 10) |
              ((bit<9>(PA_value) ^ bit<13>(PA_value)) << 9) |
              bits<8,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_blocklinear_ps64k_parts2(PA_TYPE PA_value){
              UINT32 swizzle = bit<10>(PA_value) ^ bit<13>(PA_value);

              PAKS = (bits<PA_MSB,12>(PA_value) << 12) |

                ((bit<11>(PA_value) ^ bit<0>(swizzle) ^ bit<14>(PA_value) ^ bit<15>(PA_value)
                  ) << 11) |

                (bit<0>(swizzle) << 10) |

                ((bit<9>(PA_value) ^ bit<13>(PA_value)) << 9) |
                bits<8,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_local_blocklinear_ps64k_parts1(PA_TYPE PA_value){
              UINT32 swizzle = bits<14,13>(PA_value) + bits<16,15>(PA_value);

              PAKS = (bits<PA_MSB,11>(PA_value) << 11) |
                // don't want diagonals
                // using ^ here so that carry doesn't propagate into bit10 from changing bit9, allows bit flipping in ROP
                ((bit<10>(PA_value) ^ bit<1>(swizzle)) << 10) |
                ((bit<9>(PA_value) ^ bit<0>(swizzle)) << 9) |

                bits<8,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_system_pitchlinear_ps4k(PA_TYPE PA_value){
          PAKS = (bits<PA_MSB,12>(PA_value) << 12) |
            (bits<9,7>(PA_value) << 9) |
            (bits<11,10>(PA_value) << 7) |
            bits<6,0>(PA_value);
}

void FermiPhysicalAddress::calc_PAKS_system_pitchlinear_ps64k(PA_TYPE PA_value){
  std::string error_string;
          PAKS = (bits<PA_MSB,16>(PA_value) << 16) |
            (bits<13,7>(PA_value) << 9) |
            (bits<15,14>(PA_value) << 7) |
            bits<6,0>(PA_value);
          if (m_aperture == SYSTEM) {
            std::stringstream ss;
            ss << "Sysmem cannot address large pitch pages: " << m_page_size << std::endl;
            error_string = ss.str();
            PORTABLE_FAIL(error_string.c_str());
          }
}

void FermiPhysicalAddress::calc_PAKS_system_blocklinearZ64(PA_TYPE PA_value){
        PAKS = PA_value;
}

// slice internal methods
void FermiPhysicalAddress::calc_SLICE_local_slices4_parts8421(UINT32 Q, UINT32 R, UINT32 O){
  m_slice =  (bit<0>(Q) << 1) | bit<9>(O);
}

void FermiPhysicalAddress::calc_SLICE_local_slices4_parts7(UINT32 Q, UINT32 R, UINT32 O){
  m_slice =  ((bit<3>(Q) ^ bit<2>(Q) ^ bit<0>(Q) ^ bit<0>(m_partition)) << 1) | bit<9>(O);
}

void FermiPhysicalAddress::calc_SLICE_local_slices4_parts6(UINT32 Q, UINT32 R, UINT32 O){
          //        m_slice =  ((bit<0>(Q) ^ bit<0>(R)  ^ bit<2>(Q)          // part = r
          m_slice =  ((bit<0>(Q)  ^ bit<0>(R) ^ bit<3>(Q)    // post divide R swizzle
                       ^ bit<4>(Q) ^ bit<5>(Q) ^ bit<6>(Q) ^ bit<7>(Q) ^ bit<8>(Q) ^ bit<9>(Q) ^ bit<10>(Q)
                       ^ bit<11>(Q) ^ bit<12>(Q) ^ bit<13>(Q) ^ bit<14>(Q) ^ bit<15>(Q) ^ bit<16>(Q) ^ bit<17>(Q)
                       ^ bit<18>(Q) ^ bit<19>(Q) ^ bit<20>(Q) ^ bit<21>(Q) ^ bit<22>(Q) ^ bit<23>(Q) ^ bit<24>(Q)
                       ^ bit<25>(Q) ^ bit<26>(Q) ^ bit<27>(Q) ^ bit<28>(Q) ^ bit<29>(Q) ^ bit<30>(Q) ^ bit<31>(Q)
                       ) << 1  ) | bit<9>(O);
}

void FermiPhysicalAddress::calc_SLICE_local_slices4_parts53(UINT32 Q, UINT32 R, UINT32 O){
  m_slice =  bits<10,9>(O);
}

void FermiPhysicalAddress::calc_SLICE_local_slices2_parts8421(UINT32 Q, UINT32 R, UINT32 O){
  m_slice = bit<0>(Q);
}

void FermiPhysicalAddress::calc_SLICE_local_slices2_parts7(UINT32 Q, UINT32 R, UINT32 O){
  m_slice = bit<3>(Q) ^ bit<2>(Q) ^ bit<0>(Q) ^ bit<0>(m_partition);
}

void FermiPhysicalAddress::calc_SLICE_local_slices2_parts6(UINT32 Q, UINT32 R, UINT32 O){
        m_slice = bit<0>(Q) ^ bit<0>(R) ^ bit<3>(Q)    // post divide R swizzle
          ^ bit<4>(Q)  ^ bit<5>(Q)  ^ bit<6>(Q)  ^ bit<7>(Q)  ^ bit<8>(Q)  ^ bit<9>(Q)  ^ bit<10>(Q)
          ^ bit<11>(Q) ^ bit<12>(Q) ^ bit<13>(Q) ^ bit<14>(Q) ^ bit<15>(Q) ^ bit<16>(Q) ^ bit<17>(Q)
          ^ bit<18>(Q) ^ bit<19>(Q) ^ bit<20>(Q) ^ bit<21>(Q) ^ bit<22>(Q) ^ bit<23>(Q) ^ bit<24>(Q)
          ^ bit<25>(Q) ^ bit<26>(Q) ^ bit<27>(Q) ^ bit<28>(Q) ^ bit<29>(Q) ^ bit<30>(Q) ^ bit<31>(Q);
}

void FermiPhysicalAddress::calc_SLICE_local_slices2_parts53(UINT32 Q, UINT32 R, UINT32 O){
        m_slice =  bit<10>(O);
}

void FermiPhysicalAddress::calc_SLICE_system_slices4(UINT32 Q, UINT32 R, UINT32 O){
          UINT32 swizzle = bits<12,11>(PAKS) + bits<14,13>(PAKS) + bits<16,15>(PAKS) + bits<18,17>(PAKS) + bits<20,19>(PAKS) +
          bits<22,21>(PAKS) + bits<24,23>(PAKS) + bits<26,25>(PAKS);

        m_slice = ((bit<10>(PAKS) ^ bit<1>(swizzle)) << 1) |
          (bit<9>(PAKS) ^ bit<0>(swizzle));
}

void FermiPhysicalAddress::calc_SLICE_system_slices2(UINT32 Q, UINT32 R, UINT32 O){
        m_slice = bit<9>(PAKS) ^ bit<10>(PAKS) ^ bit<11>(PAKS) ^ bit<12>(PAKS) ^ bit<13>(PAKS) ^ bit<14>(PAKS) ^ bit<15>(PAKS) ^
          bit<16>(PAKS) ^ bit<17>(PAKS) ^ bit<18>(PAKS) ^ bit<19>(PAKS) ^ bit<20>(PAKS) ^ bit<21>(PAKS) ^ bit<22>(PAKS) ^
          bit<23>(PAKS) ^ bit<24>(PAKS) ^ bit<25>(PAKS) ^ bit<26>(PAKS);
}

// partition internal methods
void FermiPhysicalAddress::calc_PART_local_parts85432(UINT32 Q, UINT32 R){
  m_partition = R;
}

void FermiPhysicalAddress::calc_PART_local_parts7(UINT32 Q, UINT32 R){
  m_partition = (R + bits<2,0>(Q) ) % 7;
}

void FermiPhysicalAddress::calc_PART_local_parts6(UINT32 Q, UINT32 R){
        m_partition = (bit<2>(Q) ^ bit<0>(R)) |
          (bits<2,1>(R) << 1);
}

void FermiPhysicalAddress::calc_PART_local_parts1(){
  m_partition = 0;
}

void FermiPhysicalAddress::calc_PART_system_parts8(UINT32 Q, UINT32 R){
  m_partition = bits<2,0>( bits<2,0>(R) + bits<2,0>(Q)*5 + bits<5,3>(Q) + bits<8,6>(Q) + bits<11,9>(Q) + bit<12>(Q) );
}

void FermiPhysicalAddress::calc_PART_system_parts753(UINT32 Q, UINT32 R){
  m_partition = R;
}

void FermiPhysicalAddress::calc_PART_system_parts6(UINT32 Q, UINT32 R){
        m_partition =
          (bits<2,1>(R) << 1) |
          (bit<0>(R) ^ bit<0>(Q) ^ bit<1>(Q) ^ bit<2>(Q) ^ bit<3>(Q) ^ bit<4>(Q) ^ bit<5>(Q) ^ bit<6>(Q) ^ bit<7>(Q) ^
          bit<8>(Q) ^ bit<9>(Q) ^ bit<10>(Q) ^ bit<11>(Q) ^ bit<12>(Q) ^ bit<13>(Q));
}

void FermiPhysicalAddress::calc_PART_system_parts4(UINT32 Q, UINT32 R){
        m_partition = (bits<1,0>(R) + bits<1,0>(Q) + bits<3,2>(Q) + bits<5,4>(Q) + bits<7,6>(Q) + bits<9,8>(Q)
                       + bits<11,10>(Q) + bits<13,12>(Q) ) %4;
}

void FermiPhysicalAddress::calc_PART_system_parts2(UINT32 Q, UINT32 R){
        m_partition = bit<0>(R) ^ bit<0>(Q) ^ bit<1>(Q) ^ bit<2>(Q) ^ bit<3>(Q)
          ^ bit<4>(Q) ^ bit<5>(Q) ^ bit<6>(Q) ^ bit<7>(Q) ^ bit<8>(Q) ^ bit<9>(Q)
          ^ bit<10>(Q) ^ bit<11>(Q) ^ bit<12>(Q) ^ bit<13>(Q) ^ bit<14>(Q);
}

void FermiPhysicalAddress::calc_PART_system_parts1(){
        m_partition = 0;
}

// L2_index, tag internal methods
void FermiPhysicalAddress::calc_L2SET_local_slices4_L2sets16_parts876421(UINT32 Q, UINT32 O){
            m_l2set = (bit<7>(O) ^ bit<3>(Q) ^ bit<7>(Q) ^ bit<11>(Q) ^ bit<15>(Q)) |
              ((bit<8>(O) ^ bit<4>(Q) ^ bit<8>(Q) ^ bit<12>(Q) ^ bit<16>(Q)) << 1) |
              (bits<1,0>(bits<3,1>(Q) + bits<7,5>(Q) + bits<11,9>(Q) + bits<15,13>(Q) + bit<17>(Q)) << 2);
            m_l2tag = bit<2>(bits<3,1>(Q) + bits<7,5>(Q) + bits<11,9>(Q) + bits<15,13>(Q) + bit<17>(Q));
                             // additional swizzling                             + ((bit<4>(Q) + bit<8>(Q) + bit<12>(Q) + bit<16>(Q)) << 2));
            m_l2tag |= ((bits<31,4>(Q)) << 1); // ors with bit zero prepared in case above

            calc_L2SECTOR_local(O);
}

void FermiPhysicalAddress::calc_L2SET_local_slices4_L2sets16_parts53(UINT32 Q, UINT32 O){
            m_l2set = (bit<7>(O) ^ bit<2>(Q) ^ bit<6>(Q) ^ bit<10>(Q) ^ bit<14>(Q)) |
              ((bit<8>(O) ^ bit<3>(Q) ^ bit<7>(Q) ^ bit<11>(Q) ^ bit<15>(Q)) << 1) |
              (bits<1,0>(bits<2,0>(Q) + bits<6,4>(Q) + bits<10,8>(Q) + bits<14,12>(Q) + bit<16>(Q)) << 2);
            m_l2tag = bit<2>(bits<2,0>(Q) + bits<6,4>(Q) + bits<10,8>(Q) + bits<14,12>(Q) + bit<16>(Q));
                             // additional swizzling                             + ((bit<3>(Q) + bit<7>(Q) + bit<11>(Q) + bit<15>(Q)) << 2));
            m_l2tag |= ((bits<31,3>(Q)) << 1); // ors with bit zero prepared in case above

            calc_L2SECTOR_local(O);
}

void FermiPhysicalAddress::calc_L2SET_local_slices4_L2sets32_parts876421(UINT32 Q, UINT32 O){
/*
L2_Index[0] = O[7] ^ Q[4] ^ Q[9] ^ Q[14]
L2_Index[1] = O[8] ^ Q[5] ^ Q[10] ^ Q[15]

bank[2:0]
L2_Index[4:2] = Q[3:1] + Q[8:6] + Q[13:11] + Q[17:16]
                        + don't know what to do here for banks

L2_tag[t-1:0] = Q[q-1:4]
*/
  calc_L2SECTOR_local(O);
}

void FermiPhysicalAddress::calc_L2SET_local_slices4_L2sets32_parts53(UINT32 Q, UINT32 O){
/*
L2_Index[0] = O[7] ^ Q[3] ^ Q[8] ^ Q[13]
L2_Index[1] = O[8] ^ Q[4] ^ Q[9] ^ Q[14]

L2_Index[4:2] = Q[2:0] + Q[7:5] + Q[12:10] + Q[16:15]
                        + don't know what to do here for banks

L2_tag[t-1:0] = Q[q-1:3]
*/
  calc_L2SECTOR_local(O);
}

void FermiPhysicalAddress::calc_L2SET_local_slices2_L2sets16_parts876421(UINT32 Q, UINT32 O){
          m_l2set =  (bit<7>(O)     ^ bit<2>(Q)     ^ bit<6>(Q)     ^ bit<10>(Q)     ^ bit<14>(Q)) |
            ((bit<8>(O)     ^ bit<4>(Q)     ^ bit<8>(Q)     ^ bit<12>(Q)     ^ bit<16>(Q)) << 1) |
            ((bit<9>(O)     ^ bit<3>(Q)     ^ bit<7>(Q)     ^ bit<11>(Q)     ^ bit<15>(Q)) << 2) |

            (bit<0>(bits<3,1>(Q) + bits<7,5>(Q) + bits<11,9>(Q) + bits<15,13>(Q) + bit<17>(Q)
                    + (bit<8>(Q) << 2) + (bit<4>(Q) << 1) + (bit<16>(Q) << 2) + (bit<12>(Q) << 1) ) << 3);

          m_l2tag = bits<2,1>(bits<3,1>(Q) + bits<7,5>(Q) + bits<11,9>(Q) + bits<15,13>(Q) + bit<17>(Q)
                              + (bit<8>(Q) << 2) + (bit<4>(Q) << 1) + (bit<16>(Q) << 2) + (bit<12>(Q) << 1) );

          m_l2tag |= ((bits<31,4>(Q)) << 2); // ors with bit zero prepared in case above

          calc_L2SECTOR_local(O);
}

void FermiPhysicalAddress::calc_L2SET_local_slices2_L2sets16_parts53(UINT32 Q, UINT32 O){
          m_l2set =  (bit<7>(O)     ^ bit<1>(Q)     ^ bit<5>(Q)     ^  bit<9>(Q)     ^ bit<13>(Q)) |
            ((bit<8>(O)     ^ bit<3>(Q)     ^ bit<7>(Q)     ^ bit<11>(Q)     ^ bit<15>(Q)) << 1) |
            ((bit<9>(O)     ^ bit<2>(Q)     ^ bit<6>(Q)     ^ bit<10>(Q)     ^ bit<14>(Q)) << 2) |

            (bit<0>(bits<2,0>(Q) + bits<6,4>(Q) + bits<10,8>(Q) + bits<14,12>(Q) + bit<16>(Q)
                    + (bit<7>(Q) << 2) + (bit<3>(Q) << 1) + (bit<15>(Q) << 2) + (bit<11>(Q) << 1) ) << 3);

          m_l2tag = bits<2,1>(bits<2,0>(Q) + bits<6,4>(Q) + bits<10,8>(Q) + bits<14,12>(Q) + bit<16>(Q)
                              + (bit<7>(Q) << 2) + (bit<3>(Q) << 1) + (bit<15>(Q) << 2) + (bit<11>(Q) << 1) );

          m_l2tag |= ((bits<31,3>(Q)) << 2); // ors with bit zero prepared in case above

          calc_L2SECTOR_local(O);
}

void FermiPhysicalAddress::calc_L2SET_local_slices2_L2sets32_parts876421(UINT32 Q, UINT32 O){
  UINT32 common = bits<3,1>(Q) + bits<8,6>(Q) + bits<13,11>(Q) + bits<17,16>(Q)
               + ((bit<4>(Q) ^ bit<5>(Q) ^ bit<9>(Q) ^ bit<10>(Q) ^ bit<14>(Q) ^ bit<15>(Q)) << 2);

  m_l2set =  (bit<7>(O) ^ bit<3>(Q) ^  bit<8>(Q) ^ bit<13>(Q)) |
            ((bit<8>(O) ^ bit<4>(Q) ^  bit<9>(Q) ^ bit<14>(Q)) << 1) |
            ((bit<9>(O) ^ bit<5>(Q) ^ bit<10>(Q) ^ bit<15>(Q)) << 2) |
             (bits<1,0>(common) << 3);

  m_l2tag = bit<2>(common) |
            (bits<31,4>(Q) << 1);

  calc_L2SECTOR_local(O);
}

void FermiPhysicalAddress::calc_L2SET_local_slices2_L2sets32_parts53(UINT32 Q, UINT32 O){
  UINT32 common = bits<2,0>(Q) + bits<7,5>(Q) + bits<12,10>(Q) + bits<16,15>(Q)
               + ((bit<3>(Q) ^ bit<4>(Q) ^ bit<8>(Q) ^ bit<9>(Q) ^ bit<13>(Q) ^ bit<14>(Q)) << 2);

  m_l2set =  (bit<7>(O) ^ bit<2>(Q) ^ bit<7>(Q) ^ bit<12>(Q)) |
            ((bit<8>(O) ^ bit<3>(Q) ^ bit<8>(Q) ^ bit<13>(Q)) << 1) |
            ((bit<9>(O) ^ bit<4>(Q) ^ bit<9>(Q) ^ bit<14>(Q)) << 2) |
             (bits<1,0>(common) << 3);

  m_l2tag = bit<2>(common) |
            (bits<31,3>(Q) << 1);

  calc_L2SECTOR_local(O);
}

void FermiPhysicalAddress::calc_L2SECTOR_local(UINT32 O){
    //sector mapping is just about address offset, not L2 bank.
    m_l2sector = (O / m_fb_databus_width ) % m_l2_sectors;
    if(m_L2BankSwizzleSetting)
      m_l2bank = (bits<2,1>(m_l2set) + bit<3>(m_l2set) + m_l2sector) % m_l2_banks;
    else
      m_l2bank = m_l2sector % m_l2_banks;  // without L2 bank swizzling

    m_l2offset = O % (m_fb_databus_width * m_l2_sectors);
}

void FermiPhysicalAddress::calc_L2SET_system_L2sets16(UINT32 Q, UINT32 O){
      UINT32 swizzle1 = bits<5,2>(Q) + bits<9,6>(Q) + bits<13,10>(Q) + bits<15,14>(Q);

      m_l2set = ((bit<1>(Q)    ^ bit<3>(swizzle1)) << 3) |
                ((bit<0>(Q)    ^ bit<2>(swizzle1)) << 2) |
                ((bit<8>(PAKS) ^ bit<1>(swizzle1)) << 1) |
                 (bit<7>(PAKS) ^ bit<0>(swizzle1));

      m_l2tag = bits<30,2>(Q);

      calc_L2SECTOR_system(O);
}

void FermiPhysicalAddress::calc_L2SET_system_L2sets32(UINT32 Q, UINT32 O){
      UINT32 swizzle1 = bits<7,3>(Q) + bits<12,8>(Q) + bits<16,13>(Q);

      m_l2set = ((3 & (bits<2,1>(Q) + bits<4,3>(swizzle1))) << 3) |

                ((bit<0>(Q)    ^ bit<2>(swizzle1)) << 2) |
                ((bit<8>(PAKS) ^ bit<1>(swizzle1)) << 1) |
                 (bit<7>(PAKS) ^ bit<0>(swizzle1));

      m_l2tag = bits<30,3>(Q);

      calc_L2SECTOR_system(O);
}

void FermiPhysicalAddress::calc_L2SECTOR_system(UINT32 O){
  calc_L2SECTOR_local(O);
}

void FermiFBAddress::calc_RBC_slices4_L2sets16_drambanks8_extbank0(){
  calc_RBC_slices4_common();
  UINT32 num_col_bits = computeColumnBits();
  UINT32 rb = (m_l2tag << 2) | (bits<3,2>(m_l2set));
  UINT32 tmp = rb % (m_number_dram_banks << m_exists_extbank);
  // swizzle banks for bank grouping

  // need to add cases here for bank groups
  m_bank =  bit<2>(tmp) |
  (bit<1>(tmp) << 1) |
  (bit<0>(tmp) << 2);

  m_extbank = 0;
  m_row = rb / (m_number_dram_banks << m_exists_extbank); // includes "column-as-row" bits

  // m_col[c-1:8] = m_row[c-9:0], where c > 8
  m_col = ((m_row << 8) | m_col) & ((1 << num_col_bits) - 1);
  m_row = m_row >> (num_col_bits - 8);
}

void FermiFBAddress::calc_RBC_slices4_L2sets16_drambanks8_extbank1(){
  calc_RBC_slices4_common();
  UINT32 num_col_bits = computeColumnBits();
  UINT32 rb = (m_l2tag << 2) | (bits<3,2>(m_l2set));
  UINT32 tmp = rb % (m_number_dram_banks << m_exists_extbank);
  // swizzle banks for bank grouping
  m_bank =  bit<3>(tmp) |
           (bit<1>(tmp) << 1) |
           (bit<0>(tmp) << 2);
  m_extbank = bit<2>(tmp);
  m_row = rb / (m_number_dram_banks << m_exists_extbank); // includes "column-as-row" bits

  // m_col[c-1:8] = m_row[c-9:0], where c > 8
  m_col = ((m_row << 8) | m_col) & ((1 << num_col_bits) - 1);
  m_row = m_row >> (num_col_bits - 8);
}

void FermiFBAddress::calc_RBC_slices4_L2sets16_drambanks16_extbank0(){
  calc_RBC_slices4_common();
  UINT32 num_col_bits = computeColumnBits();
  UINT32 rb = (m_l2tag << 2) | (bits<3,2>(m_l2set));
  UINT32 tmp = rb % (m_number_dram_banks << m_exists_extbank);
  // swizzle banks for bank grouping
  m_bank =  bit<3>(tmp) |
  (bit<2>(tmp) << 1) |
  (bit<0>(tmp) << 2) |
  (bit<1>(tmp) << 3);
  m_extbank = 0;
  m_row = rb / (m_number_dram_banks << m_exists_extbank); // includes "column-as-row" bits

  // m_col[c-1:8] = m_row[c-9:0], where c > 8
  m_col = ((m_row << 8) | m_col) & ((1 << num_col_bits) - 1);
  m_row = m_row >> (num_col_bits - 8);
}

void FermiFBAddress::calc_RBC_slices4_L2sets16_drambanks16_extbank1(){
  calc_RBC_slices4_common();
  UINT32 num_col_bits = computeColumnBits();
  UINT32 rb = (m_l2tag << 2) | (bits<3,2>(m_l2set));
  UINT32 tmp = rb % (m_number_dram_banks << m_exists_extbank);
  // swizzle banks for bank grouping
  m_bank =  bit<3>(tmp) |
           (bit<2>(tmp) << 1) |
           (bit<0>(tmp) << 2) |
           (bit<1>(tmp) << 3);
  m_extbank = bit<4>(tmp);
  m_row = rb / (m_number_dram_banks << m_exists_extbank); // includes "column-as-row" bits

  // m_col[c-1:8] = m_row[c-9:0], where c > 8
  m_col = ((m_row << 8) | m_col) & ((1 << num_col_bits) - 1);
  m_row = m_row >> (num_col_bits - 8);
}

void FermiFBAddress::calc_RBC_slices4_L2sets32_drambanks8_extbank0(){
}

void FermiFBAddress::calc_RBC_slices4_L2sets32_drambanks8_extbank1(){
}

void FermiFBAddress::calc_RBC_slices4_L2sets32_drambanks16_extbank0(){
}

void FermiFBAddress::calc_RBC_slices4_L2sets32_drambanks16_extbank1(){
}

void FermiFBAddress::calc_RBC_slices2_L2sets16_drambanks8_extbank0(){
  calc_RBC_slices2_common();
  UINT32 num_col_bits = computeColumnBits();
  UINT32 rb = (m_l2tag << 1) | bit<3>(m_l2set);
  UINT32 tmp = rb % (m_number_dram_banks << m_exists_extbank);
  // swizzle banks for bank grouping, 8 banks, 2 slice
  m_bank =  bit<2>(tmp) |
           (bit<1>(tmp) << 1) |
           (bit<0>(tmp) << 2);
  m_extbank = 0;
  m_row = rb / (m_number_dram_banks << m_exists_extbank); // includes "column-as-row" bits

  // m_col[c-1:8] = m_row[c-9:0], where c > 8
  m_col = ((m_row << 8) | m_col) & ((1 << num_col_bits) - 1);
  m_row = m_row >> (num_col_bits - 8);
}

void FermiFBAddress::calc_RBC_slices2_L2sets16_drambanks8_extbank1(){
  calc_RBC_slices2_common();
  UINT32 num_col_bits = computeColumnBits();
  UINT32 rb = (m_l2tag << 1) | bit<3>(m_l2set);
  UINT32 tmp = rb % (m_number_dram_banks << m_exists_extbank);
  // swizzle banks for bank grouping, 8 banks, 2 slice
  m_bank =  bit<3>(tmp) |
           (bit<1>(tmp) << 1) |
           (bit<0>(tmp) << 2);
  m_extbank = bit<2>(tmp);
  m_row = rb / (m_number_dram_banks << m_exists_extbank); // includes "column-as-row" bits

  // m_col[c-1:8] = m_row[c-9:0], where c > 8
  m_col = ((m_row << 8) | m_col) & ((1 << num_col_bits) - 1);
  m_row = m_row >> (num_col_bits - 8);
}

void FermiFBAddress::calc_RBC_slices2_L2sets16_drambanks16_extbank0(){
  calc_RBC_slices2_common();
  UINT32 num_col_bits = computeColumnBits();
  UINT32 rb = (m_l2tag << 1) | bit<3>(m_l2set);
  UINT32 tmp = rb % (m_number_dram_banks << m_exists_extbank);
  // swizzle banks for bank grouping, 8 banks, 2 slice
  m_bank =  bit<3>(tmp) |
           (bit<2>(tmp) << 1) |
           (bit<0>(tmp) << 2) |
           (bit<1>(tmp) << 3);
  m_extbank = 0;
  m_row = rb / (m_number_dram_banks << m_exists_extbank); // includes "column-as-row" bits

  // m_col[c-1:8] = m_row[c-9:0], where c > 8
  m_col = ((m_row << 8) | m_col) & ((1 << num_col_bits) - 1);
  m_row = m_row >> (num_col_bits - 8);
}

void FermiFBAddress::calc_RBC_slices2_L2sets16_drambanks16_extbank1(){
  calc_RBC_slices2_common();
  UINT32 num_col_bits = computeColumnBits();
  UINT32 rb = (m_l2tag << 1) | bit<3>(m_l2set);
  UINT32 tmp = rb % (m_number_dram_banks << m_exists_extbank);
  // swizzle banks for bank grouping, 8 banks, 2 slice
  m_bank =  bit<3>(tmp) |
           (bit<2>(tmp) << 1) |
           (bit<0>(tmp) << 2) |
           (bit<1>(tmp) << 3);
  m_extbank = bit<4>(tmp);
  m_row = rb / (m_number_dram_banks << m_exists_extbank); // includes "column-as-row" bits

  // m_col[c-1:8] = m_row[c-9:0], where c > 8
  m_col = ((m_row << 8) | m_col) & ((1 << num_col_bits) - 1);
  m_row = m_row >> (num_col_bits - 8);
}

void FermiFBAddress::calc_RBC_slices2_L2sets32_drambanks8_extbank0(){
  calc_RBC_slices2_common();
  UINT32 num_col_bits = computeColumnBits();
  UINT32 rb = (m_l2tag << 2) | bits<4,3>(m_l2set);
  UINT32 tmp = rb % (m_number_dram_banks << m_exists_extbank);
  // swizzle banks for bank grouping, 8 banks, 2 slice
  m_bank =  bit<2>(tmp) |
           (bit<1>(tmp) << 1) |
           (bit<0>(tmp) << 2);
  m_extbank = 0;
  m_row = rb / (m_number_dram_banks << m_exists_extbank); // includes "column-as-row" bits

  // m_col[c-1:8] = m_row[c-9:0], where c > 8
  m_col = ((m_row << 8) | m_col) & ((1 << num_col_bits) - 1);
  m_row = m_row >> (num_col_bits - 8);
}

void FermiFBAddress::calc_RBC_slices2_L2sets32_drambanks8_extbank1(){
  calc_RBC_slices2_common();
  UINT32 num_col_bits = computeColumnBits();
  UINT32 rb = (m_l2tag << 2) | bits<4,3>(m_l2set);
  UINT32 tmp = rb % (m_number_dram_banks << m_exists_extbank);
  // swizzle banks for bank grouping, 8 banks, 2 slice
  m_bank =  bit<3>(tmp) |
           (bit<1>(tmp) << 1) |
           (bit<0>(tmp) << 2);
  m_extbank = bit<2>(tmp);
  m_row = rb / (m_number_dram_banks << m_exists_extbank); // includes "column-as-row" bits

  // m_col[c-1:8] = m_row[c-9:0], where c > 8
  m_col = ((m_row << 8) | m_col) & ((1 << num_col_bits) - 1);
  m_row = m_row >> (num_col_bits - 8);
}

void FermiFBAddress::calc_RBC_slices2_L2sets32_drambanks16_extbank0(){
  calc_RBC_slices2_common();
  UINT32 num_col_bits = computeColumnBits();
  UINT32 rb = (m_l2tag << 2) | bits<4,3>(m_l2set);
  UINT32 tmp = rb % (m_number_dram_banks << m_exists_extbank);
  // swizzle banks for bank grouping, 8 banks, 2 slice
  m_bank =  bit<3>(tmp) |
           (bit<2>(tmp) << 1) |
           (bit<0>(tmp) << 2) |
           (bit<1>(tmp) << 3);
  m_extbank = 0;
  m_row = rb / (m_number_dram_banks << m_exists_extbank); // includes "column-as-row" bits

  // m_col[c-1:8] = m_row[c-9:0], where c > 8
  m_col = ((m_row << 8) | m_col) & ((1 << num_col_bits) - 1);
  m_row = m_row >> (num_col_bits - 8);
}

void FermiFBAddress::calc_RBC_slices2_L2sets32_drambanks16_extbank1(){
  calc_RBC_slices2_common();
  UINT32 num_col_bits = computeColumnBits();
  UINT32 rb = (m_l2tag << 2) | bits<4,3>(m_l2set);
  UINT32 tmp = rb % (m_number_dram_banks << m_exists_extbank);
  // swizzle banks for bank grouping, 8 banks, 2 slice
  m_bank =  bit<3>(tmp) |
           (bit<2>(tmp) << 1) |
           (bit<0>(tmp) << 2) |
           (bit<1>(tmp) << 3);
  m_extbank = bit<4>(tmp);
  m_row = rb / (m_number_dram_banks << m_exists_extbank); // includes "column-as-row" bits

  // m_col[c-1:8] = m_row[c-9:0], where c > 8
  m_col = ((m_row << 8) | m_col) & ((1 << num_col_bits) - 1);
  m_row = m_row >> (num_col_bits - 8);
}

void FermiFBAddress::calc_RBC_slices4_common(){
  m_subpart = bit<1>(m_slice);
  // m_col = (bit<0>(m_slice) << 7) | (bits<1,0>(m_l2set) << 5) | (m_l2sector << 3);
  m_col = (bit<0>(m_slice) << 7) | (bits<1,0>(m_l2set) << 5) | bits<6,2>(m_l2offset);
}

void FermiFBAddress::calc_RBC_slices2_common(){
  m_subpart = bit<0>(m_slice);
  // m_col = (bits<2,0>(m_l2set) << 5) | (m_l2sector << 3);
  m_col = (bits<2,0>(m_l2set) << 5) | bits<6,2>(m_l2offset);
}

//-------------------------
// AnchorMapping class methods
//
// these are masks used in to transform the padr or slice based on offset into a 1K FB tile
//   mask is indexed by offset/128
const UINT32 FermiAnchorMapping::LUT_padr_mask_4slice[8] = {
  0,1,2,3,0,1,2,3
};
const UINT32 FermiAnchorMapping::LUT_slice_mask_4slice[8] = {
  0,0,0,0,1,1,1,1
};
// masks for sysmem/peer block linear
const UINT32 FermiAnchorMapping::LUT_sysmem_padr_mask_4slice[8] = {
  0,1,2,3,0,1,2,3
};
const UINT32 FermiAnchorMapping::LUT_sysmem_slice_mask_4slice[8] = {
  0,0,0,0,1,1,1,1
};
const UINT32 FermiAnchorMapping::LUT_padr_mask_2slice[8] = {
  0,1,2,3,4,5,6,7
};
const UINT32 FermiAnchorMapping::LUT_slice_mask_2slice[8] = {
  0,0,0,0,0,0,0,0
};
// masks for sysmem/peer block linear
const UINT32 FermiAnchorMapping::LUT_sysmem_padr_mask_2slice[8] = {
  0,1,2,3,0,1,2,3
};
const UINT32 FermiAnchorMapping::LUT_sysmem_slice_mask_2slice[8] = {
  0,0,0,0,1,1,1,1
};

FermiAnchorMapping::FermiAnchorMapping(
                                       UINT64 _padr,
                                       UINT32 _slice,
                                       const enum FermiPhysicalAddress::KIND _kind,
                                       const enum FermiPhysicalAddress::APERTURE _aperture,
                                       const enum FermiPhysicalAddress::PAGE_SIZE _page_size,
                                       UINT32 _offset
                                       ) :
  m_padr(_padr), m_slice(_slice), m_kind(_kind), m_aperture(_aperture), m_page_size(_page_size), m_offset(_offset)
{
  PORTABLE_ASSERT(_offset<1024);
  if(FermiPhysicalAddress::m_simplified_mapping)
    m_newPadr = (m_padr<<8) + m_offset;  // for simplified mapping m_newPadr holds a byte aligned address
  else {
    if( m_kind == FermiPhysicalAddress::PITCHLINEAR ) {
      m_newPadr = m_padr;
      m_newSlice = m_slice;
    } else {
      switch ( FermiPhysicalAddress::m_number_xbar_l2_slices ) {
      case 4:
        if( m_aperture == FermiPhysicalAddress::LOCAL ) {
          // VIDMEM
          m_newPadr = m_padr ^ LUT_padr_mask_4slice[m_offset>>7];
          m_newSlice = m_slice ^ LUT_slice_mask_4slice[m_offset>>7];
        } else {
          // SYSMEM/PEER
          m_newPadr = m_padr ^ LUT_sysmem_padr_mask_4slice[m_offset>>7];
          m_newSlice = m_slice ^ LUT_sysmem_slice_mask_4slice[m_offset>>7];
        }
        break;
      case 2:
        if( m_aperture == FermiPhysicalAddress::LOCAL ) {
          // VIDMEM
          m_newPadr = m_padr ^ LUT_padr_mask_2slice[m_offset>>7];
          m_newSlice = m_slice ^ LUT_slice_mask_2slice[m_offset>>7];
        } else {
          // SYSMEM/PEER
          m_newPadr = m_padr ^ LUT_sysmem_padr_mask_2slice[m_offset>>7];
          m_newSlice = m_slice ^ LUT_sysmem_slice_mask_2slice[m_offset>>7];
        }
        break;
      default:
        std::string error_string;
        std::stringstream ss;
        ss << "ERROR:  unexpected number of slices = " << FermiPhysicalAddress::m_number_xbar_l2_slices << " should be 2 or 4." << std::endl;
        error_string = ss.str();
        PORTABLE_FAIL(error_string.c_str());
        break;
      }
    }
  }
}

// returns the slice the request should be sent to
UINT32 FermiAnchorMapping::slice() {
  // FB memory
  if(FermiPhysicalAddress::m_simplified_mapping)
    return (UINT32)((m_newPadr>>8) % FermiPhysicalAddress::m_number_xbar_l2_slices );
  else
    return m_newSlice;
}

// padr() returns a 128 byte aligned address ready to send to fb
UINT64  FermiAnchorMapping::padr( enum SIMPLIFIED_ALIGNMENT simplifiedAlignment ) {
  // fb memory
  if(FermiPhysicalAddress::m_simplified_mapping) {
    switch ( simplifiedAlignment ) {
    case SA_256B:
      return (m_newPadr >> 8);
    case SA_128B:
      return (m_newPadr >> 7);
    default:
      std::string error_string;
      std::stringstream ss;
      ss << "bad alignment " << simplifiedAlignment << " should be " <<  SA_128B << " or " << SA_256B << std::endl;
      error_string = ss.str();
      PORTABLE_FAIL(error_string.c_str());
    }
  }

  // POR mapping
  return m_newPadr;
}

// padr() returns a 256 byte aligned address ready to index into the comp bit table
UINT64  FermiAnchorMapping::compBitTableIndex() {
  // fb memory
  if(FermiPhysicalAddress::m_simplified_mapping) {
    return m_newPadr >> 10;
  } else {
    return m_newPadr >> 1;
  }
}

//-------------------------
// FermiSysmemPeerReverseMap class methods
//
FermiSysmemPeerReverseMap::FermiSysmemPeerReverseMap(
                                                     UINT64 _padr,
                                                     UINT32 _partition,
                                                     UINT32 _slice,
                                                     UINT32 _sector_msk,
                                                     const enum FermiPhysicalAddress::KIND _kind,
                                                     const enum FermiPhysicalAddress::APERTURE _aperture,
                                                     const enum FermiPhysicalAddress::PAGE_SIZE _page_size
                                                     )  :
  m_padr(_padr), m_partition(_partition), m_slice(_slice), m_sector_msk(_sector_msk), m_kind(_kind), m_aperture(_aperture), m_page_size(_page_size)
{
  if( FermiPhysicalAddress::m_simplified_mapping ) {
    m_pa = m_padr;
  } else {
    // POR Mapping
    // Recover Q
    UINT64 unswizzle;
    UINT64 Q;
    if (FermiPhysicalAddress::m_l2_sets == 16) {
      unswizzle = bits<7,4>(m_padr) + bits<11,8>(m_padr) + bits<15,12>(m_padr) + bits<17,16>(m_padr);
      Q = (bits<32,4>(m_padr) << 2) | ((bit<3>(m_padr) ^ bit<3>(unswizzle))<< 1) | (bit<2>(m_padr) ^ bit<2>(unswizzle));
    } else {
      unswizzle = bits<9,5>(m_padr) + bits<14,10>(m_padr) + bits<18,15>(m_padr);
      Q = (bits<33,5>(m_padr) << 3) | (bits<1,0>(bits<4,3>(m_padr) - bits<4,3>(unswizzle))<< 1) | (bit<2>(m_padr) ^ bit<2>(unswizzle));
    }

    UINT64 bit7paks = bit<0>(m_padr) ^ bit<0>(unswizzle);
    UINT64 bit8paks = bit<1>(m_padr) ^ bit<1>(unswizzle);

    UINT64 R = 0;
    switch (FermiPhysicalAddress::m_number_fb_partitions) {
    case 8:
      R = (m_partition - (bit<12>(Q) + bits<11,9>(Q) + bits<8,6>(Q) + bits<5,3>(Q) + bits<2,0>(Q) * 5)) & 0x7;
      break;
    case 7: case 5: case 3:
      R = m_partition;
      break;
    case 6:
      R = (bits<2,1>(m_partition) << 1) | (bit<0>(m_partition) ^ bit<0>(Q) ^ bit<1>(Q) ^ bit<2>(Q) ^ bit<3>(Q) ^ bit<4>(Q) ^ bit<5>(Q) ^ bit<6>(Q) ^  bit<7>(Q) ^ bit<8>(Q) ^ bit<9>(Q) ^ bit<10>(Q) ^ bit<11>(Q) ^ bit<12>(Q) ^ bit<13>(Q));
      break;
    case 4:
      R = (m_partition - (bits<1,0>(Q) + bits<3,2>(Q) + bits<5,4>(Q) + bits<7,6>(Q) + bits<9,8>(Q) + bits<11,10>(Q) + bits<13,12>(Q))) & 0x3;
      break;
    case 2:
      R = bit<0>(m_partition) ^ bit<0>(Q) ^ bit<1>(Q) ^ bit<2>(Q) ^ bit<3>(Q) ^ bit<4>(Q) ^ bit<5>(Q) ^bit<6>(Q) ^  bit<7>(Q) ^ bit<8>(Q) ^ bit<9>(Q) ^ bit<10>(Q) ^ bit<11>(Q) ^ bit<12>(Q) ^ bit<13>(Q) ^ bit<14>(Q);
      break;
    case 1:
      R = 0;
      break;
    }

    // temp variables
    UINT64 bits39_11paks;
    UINT64 bits39_10paks;
    UINT64 unswizzlep;
    UINT64 bit9paks;
    UINT64 bit10paks;
    switch ( FermiPhysicalAddress::m_number_xbar_l2_slices ) {
    case 4:
      bits39_11paks = bits<29,0>(Q) * FermiPhysicalAddress::m_number_fb_partitions + R;
      m_paks = bits39_11paks << 11;
      unswizzlep = (bits<12,11>(m_paks) + bits<14,13>(m_paks) + bits<16,15>(m_paks) + bits<18,17>(m_paks) + bits<20,19>(m_paks) + bits<22,21>(m_paks) + bits<24,23>(m_paks) + bits<26,25>(m_paks)) & 0x3;
      bit9paks = bit<0>(m_slice) ^ bit<0>(unswizzlep);
      bit10paks = bit<1>(m_slice) ^ bit<1>(unswizzlep);
      m_paks |= (bit10paks << 10) | (bit9paks << 9) | (bit8paks << 8) | (bit7paks << 7);
      break;
    case 2:
      bits39_10paks = bits<30,0>(Q) * FermiPhysicalAddress::m_number_fb_partitions + R;
      m_paks = bits39_10paks << 10;
      bit9paks = bit<0>(m_slice) ^ bit<10>(m_paks) ^ bit<11>(m_paks) ^ bit<12>(m_paks) ^ bit<13>(m_paks) ^
        bit<14>(m_paks) ^ bit<15>(m_paks) ^ bit<16>(m_paks) ^ bit<17>(m_paks) ^ bit<18>(m_paks) ^ bit<19>(m_paks) ^
        bit<20>(m_paks) ^ bit<21>(m_paks) ^ bit<22>(m_paks) ^ bit<23>(m_paks) ^ bit<24>(m_paks) ^ bit<25>(m_paks) ^ bit<26>(m_paks);
      m_paks |= (bit9paks << 9) | (bit8paks << 8) | (bit7paks << 7);
      break;
    default:
      PORTABLE_FAIL("ERROR:  unexpected number of slices.  Expected value 2 or 4.\n");
      break;
    }

    if( m_kind == FermiPhysicalAddress::PITCHLINEAR ) {
    // pitch only
      if( m_page_size == FermiPhysicalAddress::PS4K ) { // 4k page
        m_pa = (bits<39,12>(m_paks) << 12) | (bits<8,7>(m_paks) << 10) | (bits<11,9>(m_paks) << 7);
      } else { // 64k/128k page
        m_pa = (bits<39,16>(m_paks) << 16) | (bits<8,7>(m_paks) << 14) | (bits<15,9>(m_paks) << 7);
      }
    } else {
      // blocklinear/z64
      m_pa = m_paks;
    }
  }
  // Check sector mask and add in offset
  if(!(m_sector_msk&0x0ff)) {
    std::string error_string;
    std::stringstream ss;
    ss << "ERROR:  At least one bit of sector mask need to be set (sector mask=0x" << hex << m_sector_msk << std::endl;
    error_string = ss.str();
    PORTABLE_FAIL(error_string.c_str());
  }
  if( ( m_kind == FermiPhysicalAddress::PITCHLINEAR ) && !((m_sector_msk&0x0f) ^ (m_sector_msk&0xf0)) ) {
    std::string error_string;
    std::stringstream ss;
    ss << "ERROR:  Pitch kind sector mask use only half the mask, top or bottom 4 bits. (sector mask=0x" << hex << m_sector_msk << std::endl;
    error_string = ss.str();
    PORTABLE_FAIL(error_string.c_str());
  }

  int sector=0;
  unsigned int mask = m_sector_msk;
  if( ( m_kind == FermiPhysicalAddress::PITCHLINEAR ) && !FermiPhysicalAddress::m_simplified_mapping ) {
    // in POR mapping need to check if the pitch mask is in the top 4 bits of the mask.  If so, shift to bring it to the bottom 4 bits
    if( m_sector_msk & 0xf0 ) mask >>= 4;
  }
  while( !((1<<sector) & mask) ) sector++;  // find first one in sector mask
  m_pa += (sector*32);
}

// returns the original PA
UINT64 FermiSysmemPeerReverseMap::pa() {
  if( FermiPhysicalAddress::m_simplified_mapping ) {
    UINT32 sm=m_sector_msk;
    return (m_padr<<8)|((sm&0xf)?0:((sm&0xf0)?0x80:0));
  } else
    return m_pa;
}

// used in the ltc unit testbench (cachegen.cpp)
UINT64 FermiSysmemPeerReverseMap::ramif_addr()
{
  int i = 0;
  m_sector_msk >>= 1;
  while( m_sector_msk )
    {
      m_sector_msk >>= 1;
      i++;
    }
  return ((m_padr << 8) + 32*i);
}

//-------------------------
// Reverse Mapping to get PA bit 8 for Z64 kinds
// For details see section 7 in
// //hw/doc/gpu/fermi/fermi/design/Functional_Descriptions/Fermi_address_mapping.doc
// need to reverse map to get PA[8].  If this is a one, then we are on the right half of the gob and we need to
// use the anchor mapping to get the padr for the left side (gob anchor).

FermiZ64PA8ReverseMap::FermiZ64PA8ReverseMap(
                                             UINT64 _padr,
                                             UINT32 _slice,
                                             const enum FermiPhysicalAddress::APERTURE _aperture,
                                             const enum FermiPhysicalAddress::PAGE_SIZE _page_size
                                             ) :
  m_padr(_padr), m_slice(_slice), m_aperture(_aperture), m_page_size(_page_size)
{
  if(FermiPhysicalAddress::m_simplified_mapping) {
    m_pa8 = m_padr&1;
    m_alignedPadr = m_padr&~0x1;
    m_alignedSlice = m_alignedPadr % FermiPhysicalAddress::m_number_xbar_l2_slices;
  } else {
    // POR mapping
    // Need to reverse map to get PA[8]
    switch ( FermiPhysicalAddress::m_number_xbar_l2_slices ) {
    case 4:  // 4 SLICES
      if (FermiPhysicalAddress::m_l2_sets == 16) {
        if( m_aperture == FermiPhysicalAddress::LOCAL) {
          // VIDMEM 4 SLICE 16 SETS
          m_pa8 = bit<1>(m_padr) ^ bit<5>(m_padr) ^ bit<9>(m_padr) ^ bit<13>(m_padr) ^ bit<17>(m_padr);
        } else {
          // SYSMEM/PEER 4 SLICE 16 SETS
          m_pa8 = bit<0>(bit<1>(m_padr) ^ bit<1>(bits<7,4>(m_padr) + bits<11,8>(m_padr) + bits<15,12>(m_padr) + bits<17,16>(m_padr)));
        }
      } else {
        // VIDMEM/SYSMEM/PEER 4 SLICE 32 SETS
        m_pa8 = bit<1>(m_padr) ^ bit<5>(m_padr) ^ bit<10>(m_padr) ^ bit<15>(m_padr);
      }
      break;
    case 2:  // 2 SLICES
      if (FermiPhysicalAddress::m_l2_sets == 16) {
        if( m_aperture == FermiPhysicalAddress::LOCAL) {
          // VIDMEM 2 SLICE 16 SETS
          m_pa8 = bit<1>(m_padr) ^ bit<6>(m_padr) ^ bit<10>(m_padr) ^ bit<14>(m_padr) ^ bit<18>(m_padr);
        } else {
          // SYSMEM/PEER 2 SLICE 16 SETS
          m_pa8 = bit<0>(bit<1>(m_padr) ^ bit<1>(bits<7,4>(m_padr) + bits<11,8>(m_padr) + bits<15,12>(m_padr) + bits<17,16>(m_padr)));
        }
      } else {
        if( m_aperture == FermiPhysicalAddress::LOCAL) {
          // VIDMEM 2 SLICE 32 SETS
          m_pa8 = bit<1>(m_padr) ^ bit<6>(m_padr) ^ bit<11>(m_padr) ^ bit<16>(m_padr);
        } else {
          // SYSMEM/PEER 2 SLICE 32 SETS
          m_pa8 = bit<0>(bit<1>(m_padr) ^ bit<1>(bits<9,5>(m_padr) + bits<14,10>(m_padr) + bits<18,15>(m_padr)));
        }
       }
      break;
    default:
      std::string error_string;
      std::stringstream ss;
      ss << "ERROR:  unexpected number of slices = " << FermiPhysicalAddress::m_number_xbar_l2_slices << " should be 2 or 4." << std::endl;
      error_string = ss.str();
      PORTABLE_FAIL(error_string.c_str());
      break;
    }
    m_alignedPadr = m_padr ^ (m_pa8 << 1);
    m_alignedSlice = m_slice;      // right now z64 gob is always on the same slice
  }
}

// returns for padr for the anchor of the gob.
UINT64 FermiZ64PA8ReverseMap::padrGobAligned() {
  return m_alignedPadr;
}

// returns for slice for the anchor of the gob.
UINT32 FermiZ64PA8ReverseMap::sliceGobAligned() {
  return (UINT32)m_alignedSlice;
}
