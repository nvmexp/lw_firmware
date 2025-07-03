#pragma once

//-----------------------------------------------------------------------------
// This file contains structures that represent all the floorsweepable regions in the chip
//-----------------------------------------------------------------------------

#include "fs_lib.h"
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <set>
#include <bitset>

// Enable these macros to turn on runtime optimizations
// Leave them default off to reduce risk

//#define USEBASEVECTORVIEW
//#define USEPOLYMORPHCONTAINER
//#define USESTATICCONTAINERS






//-----------------------------------------------------------------------------
// Generic exception for use in fslib
//-----------------------------------------------------------------------------

struct FSLibException : public std::exception {
    std::string description;
    FSLibException(const std::string& desc) : std::exception(), description(desc) {

    }

    const char * what () const throw () {
        return description.c_str();
    }
};

#ifdef USEBASEVECTORVIEW
#include <variant>
#endif

#include "fs_container_utils.h"

#ifdef USESTATICCONTAINERS
#include "etl/vector.h"
#endif



//-----------------------------------------------------------------------------
// This structure is a replica of the one defined in lwgpu/clib/include/chipPOR_config.h
// It's fields are populated by generated headers, whose values come from chipPOR_config.h
//-----------------------------------------------------------------------------

namespace fslib {

struct chipPOR_settings_t {
    std::string name;
    uint32_t num_gpcs;
    uint32_t num_tpc_per_gpc;
    uint32_t num_pes_per_gpc;
    uint32_t num_rop_per_gpc; // added for ROP_IN_GPC
    uint32_t num_fbps;
    uint32_t num_ltcs;
    uint32_t num_fbpas;
    uint32_t num_lwencs;
    uint32_t num_lwdecs;
    uint32_t num_lwjpgs;
    uint32_t num_ofas;
    uint32_t num_lwlinks;
    uint32_t lw_chip_pci_devid;
    uint32_t lw_chip_pci_devid_rebrand;
    uint32_t lw_chip_internal_devid;
    uint32_t lw_chip_implementation;
    uint32_t lw_chip_internal_arch;
    uint32_t lw_chip_internal_major_rev;
    uint32_t lw_chip_internal_minor_rev;
    uint32_t lw_chip_aza_pci_devid;
    uint32_t lw_chip_pmc_boot_0;
    uint32_t lw_chip_pmc_boot_0_self_host;
    uint32_t disp_no_link_ef;
};

struct chip_settings_t {
    Chip chip_type;
    chipPOR_settings_t chip_por_settings;
};

//-----------------------------------------------------------------------------
// Module ID type and utility functions
//-----------------------------------------------------------------------------

enum class module_type_id {
    fbp,
    gpc,
    rop,
    tpc,
    pes,
    cpc,
    ltc,
    fbio,
    l2slice,
    chip,
    halffbpa,
    invalid,
};

const std::map<std::string, module_type_id>& getStrToIdMap();
const std::map<module_type_id, std::string>& getIdToStrMap();
const std::string& idToStr(module_type_id module_id);
module_type_id strToId(const std::string& module_str);
std::ostream& operator<<(std::ostream& os, const module_type_id& module_id);

//-----------------------------------------------------------------------------
// typedefs
//-----------------------------------------------------------------------------

class FSModule_t;

// pick which type will be used to hold the modules, unique_ptr or PolyMorphContainer
#ifdef USEPOLYMORPHCONTAINER
template <class T, int n>
using ModuleHandle_t = PolyMorphContainer<T, n>;
template <class T, int n>
using PrimaryModuleList_t = StaticVector<T, n>;
#else
template <class T, int n>
using ModuleHandle_t = std::unique_ptr<T>;
template <class T, int n>
using PrimaryModuleList_t = std::vector<T>;
#endif

// forward declare base classes
class L2Slice_t;
class LTC_t;
class HalfFBPA_t;
class FBIO_t;
class FBP_t;
class TPC_t;
class ROP_t;
class PES_t;
class CPC_t;
class GPC_t;

// The numbers are the estimated sizeofs for each type and any potential derived classes
// There will be a compiler error if the size is too small
using L2SliceHandle_t = ModuleHandle_t<L2Slice_t, 16>;
using LTCHandle_t = ModuleHandle_t<LTC_t, 16>;
using HalfFBPAHandle_t = ModuleHandle_t<HalfFBPA_t, 16>;
using FBIOHandle_t = ModuleHandle_t<FBIO_t, 16>;
using FBPHandle_t = ModuleHandle_t<FBP_t, (1<<10)>;
using TPCHandle_t = ModuleHandle_t<TPC_t, 16>;
using ROPHandle_t = ModuleHandle_t<ROP_t, 16>;
using PESHandle_t = ModuleHandle_t<PES_t, 16>;
using CPCHandle_t = ModuleHandle_t<CPC_t, 16>;
using GPCHandle_t = ModuleHandle_t<GPC_t, (1<<10)>;

#ifdef USEBASEVECTORVIEW
// The variant type to use with BaseVectorView
using list_variant_type = 
    std::variant<
        const PrimaryModuleList_t<L2SliceHandle_t, 8>*,
        const PrimaryModuleList_t<LTCHandle_t, 2>*,
        const PrimaryModuleList_t<HalfFBPAHandle_t, 2>*,
        const PrimaryModuleList_t<FBIOHandle_t, 2>*,
        const PrimaryModuleList_t<FBPHandle_t, 12>*,
        const PrimaryModuleList_t<TPCHandle_t, 16>*,
        const PrimaryModuleList_t<ROPHandle_t, 2>*,
        const PrimaryModuleList_t<PESHandle_t, 3>*,
        const PrimaryModuleList_t<CPCHandle_t, 3>*,
        const PrimaryModuleList_t<GPCHandle_t, 12>*
    >;
using FSModuleVectorView = BaseVectorView<FSModule_t*, list_variant_type>;
using ConstFSModuleVectorView = BaseVectorView<const FSModule_t*, list_variant_type>;
#endif

#ifdef USESTATICCONTAINERS
// A static_vector class definition can be found online in various libraries (boost, ETL, etc)
// Using this type instead of std::vector can speed up fslib by 2-3x, but fslib is already pretty fast
// If there is a trusted implementation of static_vector, we could use it
// For now keep using std::vector
typedef etl::vector<FSModule_t*, 12> base_module_list_temp_t;
typedef etl::vector<const FSModule_t *, 12> const_base_module_list_temp_t;
typedef etl::vector<module_type_id, 3> hierarchy_list_t;
typedef etl::vector<uint32_t, 128> module_index_list_t;
typedef etl::vector<FSModule_t*, 128> flat_module_list_t;
typedef etl::vector<const FSModule_t *, 128> const_flat_module_list_t;
#else
typedef std::vector<FSModule_t*> base_module_list_temp_t;
typedef std::vector<const FSModule_t*> const_base_module_list_temp_t;
typedef std::vector<module_type_id> hierarchy_list_t;
typedef std::vector<uint32_t> module_index_list_t;
typedef std::vector<FSModule_t*> flat_module_list_t;
typedef std::vector<const FSModule_t *> const_flat_module_list_t;
#endif

#ifndef USEBASEVECTORVIEW
// FSModuleVectorView is based on BaseVectorView, which is implemented in fs_lib
// It can improve the runtime by around 2x, but it makes it more diffilwlt to debug
// Since the debugger has optimizations for std::vector that make it easier to explore the contents
// we should therefore keep both implementations alive.
typedef base_module_list_temp_t base_module_list_t;
typedef const_base_module_list_temp_t const_base_module_list_t;
#else
typedef FSModuleVectorView base_module_list_t;
typedef ConstFSModuleVectorView const_base_module_list_t;
#endif

// map module type to function pointer to get a list of modules of that type
typedef std::map<module_type_id, const_base_module_list_t (*)(const FSModule_t&)> module_map_t;

// map module type to hierarchy list
typedef std::map<module_type_id, hierarchy_list_t> hierarchy_map_t;

typedef std::set<module_type_id> module_set_t;
typedef std::bitset<32> mask32_t;


class FSChip_t;
class FSChipContainer_t : public PolyMorphContainer<FSChip_t, (1<<15)> {
    public:
    FSChipContainer_t() : PolyMorphContainer(){};
    FSChipContainer_t(Chip chip_enum);
    FSChipContainer_t(const FSChip_t* chip_to_clone);
};


//-----------------------------------------------------------------------------
// PESMapping_t
//-----------------------------------------------------------------------------

// This class has useful functions for mapping the TPCs to PESs
// It can also be used for TPC to CPC mapping
class PESMapping_t {
    public:
    class RangeIteratorView {

        int m_begin;
        int m_end;

        class iterator {
            private:
            uint32_t m_val;
            public:
            using iterator_category = std::forward_iterator_tag;
            iterator(uint32_t start);
            uint32_t operator*() const;
            iterator& operator++();
            iterator operator++(int);
            bool operator==(const iterator& other) const;
            bool operator!=(const iterator& other) const;
        };

        public:
        RangeIteratorView(int begin, int end);

        iterator begin() const;
        iterator end() const;
    };

public:
    typedef std::vector<int> tpc_list_t;
private:
    int m_num_tpcs;
    int m_num_pess;
public:
    PESMapping_t(int num_pess, int num_tpcs);
    int getTPCtoPESMapping(int tpc_id) const;
    RangeIteratorView getPEStoTPCMapping(int tpc_id) const;
    tpc_list_t getPEStoTPCMappingVector(int tpc_id) const;
    std::string printMapping() const;
};

//-----------------------------------------------------------------------------
// The following structures are used to represent floorsweepable modules in the chip
//-----------------------------------------------------------------------------

struct MaskComparer {
    bool operator() (const mask32_t &b1, const mask32_t &b2) const;
};

// Base class for all floorsweepable modules
class FSModule_t {
private:
    bool enabled{true};
    void getFlatList(const hierarchy_list_t& module_hierarchy, uint32_t hierarchy_level, const_flat_module_list_t& flat_list) const;
    uint32_t getModuleCountImpl(const hierarchy_list_t& module_hierarchy, uint32_t hierarchy_level) const;
    const FSModule_t* getModuleFlatIdxImpl(const hierarchy_list_t& module_hierarchy, uint32_t hierarchy_level, uint32_t flat_idx) const;
    bool findHierarchy(module_type_id type_id, hierarchy_list_t& hierarchy_list) const;
public:
    uint32_t instance_number{0};
    virtual bool isValid(FSmode fsmode, std::string& msg) const = 0;
    virtual bool getEnabled() const;
    virtual void setEnabled(bool en);
    virtual ~FSModule_t();
    //FSModule_t(const FSModule_t& other) = delete;
    std::string getTypeStr() const;
    virtual module_type_id getTypeEnum() const = 0;
    base_module_list_t returnModuleList(module_type_id module_type);
    const_base_module_list_t returnModuleList(module_type_id module_type) const;
    const_flat_module_list_t getFlatList(const hierarchy_list_t& module_hierarchy) const;
    flat_module_list_t getFlatList(const hierarchy_list_t& module_hierarchy);
    virtual module_index_list_t getPreferredDisableList(module_type_id module_type, const FUSESKUConfig::SkuConfig& potentialSKU) const = 0;
    module_index_list_t getPreferredDisableList(module_type_id module_type) const;
    module_index_list_t getSortedListByEnableCount(module_type_id module_type, module_type_id submodule_type) const;
    virtual uint32_t getEnableCount(module_type_id module_type) const = 0;
    void preSortDownBinPreference(const_flat_module_list_t& flat_list) const;
    virtual const std::vector<module_type_id>& getSubModuleTypeDownBinPriority() const = 0;
    virtual module_index_list_t getFSTiePreference(module_type_id module_type) const;
    virtual const module_map_t& getModuleMapRef() const = 0;
    virtual const module_set_t& getSubModuleTypes() const = 0;
    virtual void setup() = 0;
    module_set_t findAllSubUnitTypes() const;
    std::vector<module_type_id> getSortedDownBinTypes() const;
    virtual std::vector<module_type_id> getDownBinTypePriority() const;
    void findAllSubUnitTypes(module_set_t& modules_set) const;
    virtual void findHierarchyMap(hierarchy_list_t& breadcrumbs, hierarchy_map_t& hierarchy_map) const;
    bool moduleCanFitSKU(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
    bool moduleCanFitSKU(module_type_id submodule_id, const FUSESKUConfig::FsConfig& skuFSSettings, std::string& msg) const;
    bool moduleIsInSKU(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
    bool moduleIsInSKU(module_type_id submodule_id, const FUSESKUConfig::FsConfig& skuFSSettings, std::string& msg) const;
    virtual bool enableCountIsValid(module_type_id submodule_type, FSmode fsmode, std::string& msg) const;
    bool enableCountsAreValid(FSmode fsmode, std::string& msg) const;
    hierarchy_list_t findHierarchy(module_type_id type_id) const;
    uint32_t getModuleCount(module_type_id type_id) const;
    const FSModule_t* getModuleFlatIdx(module_type_id type_id, uint32_t flat_idx) const;
    FSModule_t* getModuleFlatIdx(module_type_id type_id, uint32_t flat_idx);
    virtual FSModule_t& operator=(const FSModule_t& other);
    virtual bool operator==(const FSModule_t& other) const;
    virtual bool operator!=(const FSModule_t& other) const;
    virtual void merge(const FSModule_t& other);
};

// Base class for all floorsweepable modules that do not contain other modules
class LeafFSModule_t : public FSModule_t {
public:
    virtual bool isValid(FSmode fsmode, std::string& msg) const;
    virtual uint32_t getEnableCount(module_type_id module_type) const;
    using FSModule_t::getPreferredDisableList;
    virtual module_index_list_t getPreferredDisableList(module_type_id module_type, const FUSESKUConfig::SkuConfig& potentialSKU) const;
    virtual const std::vector<module_type_id>& getSubModuleTypeDownBinPriority() const;
    virtual const module_map_t& getModuleMapRef() const;
    virtual const module_set_t& getSubModuleTypes() const;
    virtual void setup();
};

class ParentFSModule_t : public FSModule_t {
    public:
    uint32_t getEnableCount(module_type_id module_type) const;
    using FSModule_t::getPreferredDisableList;
    virtual module_index_list_t getPreferredDisableList(module_type_id module_type, const FUSESKUConfig::SkuConfig& potentialSKU) const;
    virtual module_index_list_t getPreferredDisableListSecondLevel(module_type_id module_type) const;
    virtual void setup();
    module_set_t makeChildModuleSet() const;
    hierarchy_map_t makeHierarchyMap() const;
    const_flat_module_list_t getFlatModuleList(module_type_id type_id) const;
    flat_module_list_t getFlatModuleList(module_type_id type_id);
    module_type_id findParentModuleTypeId(module_type_id type_id) const;
    virtual void sortPreferredMidLevelModules(const_flat_module_list_t& node_list, const std::vector<module_type_id>& priority) const;
};

// Leaf modules
class PES_t : public LeafFSModule_t {
public:
    virtual module_type_id getTypeEnum() const;
};

class TPC_t : public LeafFSModule_t {
public:
    virtual module_type_id getTypeEnum() const;
};

class ROP_t : public LeafFSModule_t {
public:
    virtual module_type_id getTypeEnum() const;
};

class CPC_t : public LeafFSModule_t {
public:
    virtual module_type_id getTypeEnum() const;
};

class L2Slice_t : public LeafFSModule_t {
public:
    virtual module_type_id getTypeEnum() const;
};

class LTC_t : public LeafFSModule_t {
public:
    virtual module_type_id getTypeEnum() const;
};

class FBIO_t : public LeafFSModule_t {
public:
    virtual module_type_id getTypeEnum() const;
};

class HalfFBPA_t : public LeafFSModule_t {
public:
    virtual module_type_id getTypeEnum() const;
};

// GPC base class
class GPC_t : public ParentFSModule_t{
    public:
    PrimaryModuleList_t<TPCHandle_t, 16> TPCs;

    virtual ~GPC_t();
    virtual void applyMask(const fslib::GpcMasks& mask, uint32_t index);
    virtual void getMask(fslib::GpcMasks& mask, uint32_t index) const;
    virtual bool isValid(FSmode fsmode, std::string& msg) const;
    void createTPC(TPCHandle_t& handle) const;
    void createPES(PESHandle_t& handle) const;
    void createROP(ROPHandle_t& handle) const;
    virtual void setup(const chipPOR_settings_t& settings);
    virtual module_type_id getTypeEnum() const;
    virtual void funcDownBin();
    virtual const std::vector<module_type_id>& getSubModuleTypeDownBinPriority() const;
    virtual const module_map_t& getModuleMapRef() const;
    virtual const module_set_t& getSubModuleTypes() const;
    virtual void killAnyTPC();
    virtual module_index_list_t getPreferredDisableList(module_type_id module_type, const FUSESKUConfig::SkuConfig& potentialSKU) const;
    virtual module_index_list_t getPreferredDisableListTPC(const FUSESKUConfig::SkuConfig& potentialSKU) const;
};

// GPC with GFX (enforces PES pairings)
class GPCWithGFX_t : public virtual GPC_t {
    public:
    PrimaryModuleList_t<PESHandle_t, 3> PESs;
    PrimaryModuleList_t<ROPHandle_t, 2> ROPs;
    virtual bool isValid(FSmode fsmode, std::string& msg) const;
    virtual void funcDownBin();
    virtual void setup(const chipPOR_settings_t& settings);
    virtual void applyMask(const fslib::GpcMasks& mask, uint32_t index);
    virtual void getMask(fslib::GpcMasks& mask, uint32_t index) const;
    virtual const module_map_t& getModuleMapRef() const;
    virtual const module_set_t& getSubModuleTypes() const;
    virtual module_index_list_t getPESIdxsByTPCCount() const;
    virtual module_index_list_t getPreferredDisableListTPC(const FUSESKUConfig::SkuConfig& potentialSKU) const;
};

// GPC with CPC
class GPCWithCPC_t : public virtual GPC_t {
    public:
    PrimaryModuleList_t<CPCHandle_t, 3> CPCs;
    void createCPC(CPCHandle_t& handle) const;
    virtual void applyMask(const fslib::GpcMasks& mask, uint32_t index);
    virtual void getMask(fslib::GpcMasks& mask, uint32_t index) const;
    virtual bool isValid(FSmode fsmode, std::string& msg) const;
    virtual void setup(const chipPOR_settings_t& settings);
    virtual void funcDownBin();
    virtual const module_map_t& getModuleMapRef() const;
    virtual const module_set_t& getSubModuleTypes() const;
    virtual module_index_list_t getCPCIdxsByTPCCount() const;
    virtual module_index_list_t getPreferredDisableListTPC(const FUSESKUConfig::SkuConfig& potentialSKU) const;
};

// FBP base class
class FBP_t : public ParentFSModule_t{
    public:
    PrimaryModuleList_t<L2SliceHandle_t, 8> L2Slices;
    PrimaryModuleList_t<LTCHandle_t, 2> LTCs;
    PrimaryModuleList_t<FBIOHandle_t, 2> FBIOs;

    static constexpr int slices_per_fbp = 8; // fixme
    virtual void applyMask(const fslib::FbpMasks& mask, uint32_t index);
    virtual void getMask(fslib::FbpMasks& mask, uint32_t index) const;
    virtual mask32_t getL2Mask() const;
    virtual void applyL2Mask(mask32_t mask);
    virtual void createLTC(LTCHandle_t& container) const;
    virtual void createL2Slice(L2SliceHandle_t& container) const;
    virtual void createFBIO(FBIOHandle_t& container) const;
    virtual uint32_t getNumEnabledLTCs() const;
    virtual uint32_t getNumEnabledSlices() const;
    virtual uint32_t getNumEnabledSlicesPerLTC(uint32_t ltc_idx) const;
    virtual uint32_t getNumDisabledSlicesPerLTC(uint32_t ltc_idx) const;
    virtual bool ltcHasPartialSlicesEnabled(uint32_t ltc_idx) const;
    virtual ~FBP_t();
    virtual bool isValid(FSmode fsmode, std::string& msg) const;
    virtual module_type_id getTypeEnum() const;
    virtual void setup(const chipPOR_settings_t& settings);
    virtual void funcDownBin();
    virtual bool ltcIsValid(FSmode fsmode, uint32_t ltc_idx, std::string& msg) const;
    virtual const std::vector<module_type_id>& getSubModuleTypeDownBinPriority() const;
    virtual const module_map_t& getModuleMapRef() const;
    virtual const module_set_t& getSubModuleTypes() const;
};

class HBMFBP_t : public FBP_t{
};

class DDRFBP_t : public FBP_t{
public:

    PrimaryModuleList_t<HalfFBPAHandle_t, 2> HalfFBPAs;

    virtual void applyMask(const fslib::FbpMasks& mask, uint32_t index);
    virtual void getMask(fslib::FbpMasks& mask, uint32_t index) const;
    virtual void createHalfFBPA(HalfFBPAHandle_t& handle) const;
    virtual uint32_t getNumEnabledHalfFBPAs() const;
    virtual bool isValid(FSmode fsmode, std::string& msg) const;
    virtual bool isValidSlice(FSmode fsmode, std::string& msg, mask32_t &expected_l2_mask) const;
    virtual int numSlicesPerLTC() const;
    virtual bool nextL2ProdMask(std::string& msg, mask32_t l2_mask, mask32_t &next_mask) const;
    virtual mask32_t getSliceStartMask(uint32_t num_slices_per_ltc) const;
    virtual bool canDo3SliceFS(FSmode fsmode, std::string& msg) const;
    virtual void setup(const chipPOR_settings_t& settings);
    virtual void funcDownBin();
    virtual module_index_list_t getPreferredDisableList(module_type_id module_type, const FUSESKUConfig::SkuConfig& potentialSKU) const;
    virtual module_index_list_t getPreferredDisableListLTC() const;
    const std::map<mask32_t, mask32_t, MaskComparer>& getSlicePatternMap() const;
    virtual bool ltcIsValid(FSmode fsmode, uint32_t ltc_idx, std::string& msg) const;
};

// Base class for all chips
class FSChip_t : public ParentFSModule_t {

friend class FSChipComponent_Test_clone_Test;
friend class FSChipComponent_Test_getHierarchyMap_Test;

public:
    PrimaryModuleList_t<GPCHandle_t, 12> GPCs;
    PrimaryModuleList_t<FBPHandle_t, 12> FBPs;
    fslib::GpcMasks spare_gpc_mask{ 0 };
protected:
    virtual void createGPC(GPCHandle_t& handle) const;
    virtual void createFBP(FBPHandle_t& handle) const;
    protected:
    virtual void getGPCType(GPCHandle_t& handle) const = 0;
    virtual void getFBPType(FBPHandle_t& handle) const = 0;
    virtual const std::vector<module_type_id>& getSubModuleTypeDownBinPriority() const;
    Chip chip_enum;
public:
    virtual const chip_settings_t& getChipSettings() const = 0;
    const chipPOR_settings_t& getChipPORSettings() const;
    Chip getChipEnum() const;
    std::unique_ptr<FSChip_t> clone() const;
    void cloneContainer(FSChipContainer_t& chip_container) const;
    FSChip_t& operator=(const FSChip_t& other);
    bool operator==(const FSChip_t& other) const;
    bool operator!=(const FSChip_t& other) const;
    virtual bool isValid(FSmode fsmode, std::string& msg) const;
    virtual void applyGPCMasks(const fslib::GpcMasks& mask);
    virtual void applyFBPMasks(const fslib::FbpMasks& mask);
    virtual void getGPCMasks(fslib::GpcMasks& mask) const;
    virtual void getFBPMasks(fslib::FbpMasks& mask) const;
    virtual uint32_t getNumEnabledSlices() const;
    virtual uint32_t getNumEnabledLTCs() const;
    virtual void setup();
    virtual void instanceSubModules();
    virtual std::string getString() const;
    virtual bool getEnabled() const;
    virtual void setEnabled(bool enabled);
    virtual module_type_id getTypeEnum() const;
    virtual bool isInSKU(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
    virtual bool canFitSKU(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
    virtual std::unique_ptr<FSChip_t> canFitSKUSearch(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
    virtual std::unique_ptr<FSChip_t> canFitSKUSearchWrap(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
    virtual bool downBin(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg);
    virtual void applyDownBinHeuristic(const FUSESKUConfig::SkuConfig& potentialSKU);
    virtual int compareConfig(const FSChip_t* candidate) const;
    virtual bool skuIsPossible(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
    virtual std::vector<FUSESKUConfig::SkuConfig> getSKUs(const std::vector<FUSESKUConfig::SkuConfig>& allPossibleSKUs) const;
    uint32_t getTotalEnableCount(module_type_id type_id) const;
    virtual void funcDownBin();
    void applyMustDisableLists(const FUSESKUConfig::SkuConfig& potentialSKU);
    void disableNonCompliantInstances(const FUSESKUConfig::SkuConfig& potentialSKU, std::string msg);
    virtual const module_map_t& getModuleMapRef() const;
    virtual const module_set_t& getSubModuleTypes() const;
    uint32_t getMaxAllowed(module_type_id module_id, const FUSESKUConfig::SkuConfig& potentialSKU) const;
    uint32_t getMinAllowed(module_type_id module_id, const FUSESKUConfig::SkuConfig& potentialSKU) const;
    bool isMustEnable(const FUSESKUConfig::SkuConfig& sku, module_type_id module_id, uint32_t module_idx) const;
    module_index_list_t getPreferredDisableListMustEnable(module_type_id module_type, const FUSESKUConfig::SkuConfig& sku) const;
    void killFBPs();
    virtual std::vector<module_type_id> getDownBinTypePriority() const;
    std::vector<uint32_t> getLogicalTPCCounts() const;
};


class DDRGPU_t : public FSChip_t {
public:
    virtual void funcDownBin();
    bool canDoSliceProduct(uint32_t num_slices_per_ltc, std::string& msg, mask32_t& first_mask) const;
    bool canDoSliceProduct(uint32_t num_slices_per_ltc, std::string& msg) const;
    bool applySliceProduct(uint32_t num_slices_per_ltc);
    void applySliceProduct(mask32_t start_mask, uint32_t start_fbp);
    void applyHalfFBPA();
    virtual std::unique_ptr<FSChip_t> canFitSKUSearch(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
    virtual bool downBin(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg);
    bool skuWantsSliceFS(const FUSESKUConfig::SkuConfig& potentialSKU) const;
    bool skuWants3SliceMode(const FUSESKUConfig::SkuConfig& potentialSKU) const;
    bool skuWants2SliceMode(const FUSESKUConfig::SkuConfig& potentialSKU) const;
};


// uGPU chips should derive from this class
class GPUWithuGPU_t : public virtual FSChip_t {
protected:
    const std::vector<int>& getUGPUMap() const;
    const LTC_t& getLTC(int ltc_idx) const;
    const L2Slice_t& getL2Slice(int l2slice_idx) const;
    int getPairFBP(int fbp_idx) const;
    int getPairLTC(int ltc_idx) const;
    int getPairL2Slice(int l2slice_idx) const;

    uint32_t getNumUnpairedFBPs() const;
    uint32_t getNumUnpairedLTCs() const;
    uint32_t getNumUnpairedL2Slices() const;
    void fixSlicesPerFBP();
    void fixSlicesPerChip();
    void fixuGPU();

public:
    virtual bool isValid(FSmode fsmode, std::string& msg) const;
    virtual void funcDownBin();
    virtual std::unique_ptr<FSChip_t> canFitSKUSearch(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
    virtual bool isInSKU(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
    virtual bool uGPUImbalanceIsInSKU(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
    int32_t getuGPUImbalance() const;
    virtual const std::vector<uint32_t>& getUGPUGPCMap() const = 0;
    virtual uint32_t getTPCCountPerUGPU(uint32_t ugpu_idx) const;
    virtual bool skuIsPossible(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
};


//-----------------------------------------------------------------------------
// Utility function templates to improve re-use
//-----------------------------------------------------------------------------

// Get the bit at a specific index from an integer type
template <class FieldType>
static bool getBit(int index, FieldType field){
    return ((field >> index) & 0x1) == 1;
}

// applies a two level mask to a list of modules
template <class unit_list_type, class bit_mask_type>
static void applyMaskToList(unit_list_type& unit_list, bit_mask_type mask){
    for (uint32_t i = 0; i < unit_list.size(); i++){
        bool disabled = getBit(i, mask);
        unit_list[i]->setEnabled(!disabled);
    }
}

// retreive a two level mask from a list of modules
template <class unit_list_type>
static uint32_t getMaskFromList(const unit_list_type& unit_list){
    uint32_t disable_mask = 0;
    for (uint32_t i = 0; i < unit_list.size(); i++){
        disable_mask |= (unit_list[i]->getEnabled() ? 0 : 1) << i;
    }
    return disable_mask;
}

// iterates over a list of modules and returns the number enabled
template <class unit_list_type>
static uint32_t getNumEnabled(const unit_list_type& unit_list){
    uint32_t count = 0;
    for (const auto& unit_instance : unit_list){
        if (unit_instance->getEnabled()){
            count += 1;
        }
    }
    return count;
}

// iterates over specific indexes in a list of modules and returns the number enabled
template <class unit_list_type>
static uint32_t getNumEnabledRange(const unit_list_type& unit_list, uint32_t begin_idx, uint32_t end_idx){
    uint32_t count = 0;
    for (uint32_t i = begin_idx; i < end_idx+1; i++){
        if (unit_list[i]->getEnabled()){
            count += 1;
        }
    }
    return count;
}

/**
 * @brief Helper function to make syntax cleaner when calling .reserve on vectors
 * The static vector classes do not implement reserve(size_t) so when USESTATICCONTAINERS is defined,
 * This function will be optimized away
 * 
 * @tparam list_t 
 * @param list_to_reserve 
 * @param amount 
 */
template <class list_t>
static void reserve(list_t& list_to_reserve, size_t amount){
    #ifndef USESTATICCONTAINERS
    list_to_reserve.reserve(amount);
    #endif
}

#ifndef USEBASEVECTORVIEW
// takes an arbitrary list of unique_ptrs to modules and copies the pointers into a const_base_module_list_t list
template <class any_module_list_t>
static void getBaseModuleList(const any_module_list_t& module_list, const_base_module_list_t& base_modules) {
    reserve(base_modules, module_list.size());
    for (const auto& module_inst : module_list){
        base_modules.push_back(module_inst.get());
    }
}
#endif

// takes an arbitrary list of unique_ptrs to modules and returns a const_base_module_list_t list
template <class any_module_list_t>
static const_base_module_list_t getBaseModuleList(const any_module_list_t& module_list) {
#ifdef USEBASEVECTORVIEW
    const_base_module_list_t base_modules(module_list);
#else
    const_base_module_list_t base_modules;
    getBaseModuleList(module_list, base_modules);
#endif
    return base_modules;
}

#ifdef USEPOLYMORPHCONTAINER
/**
 * @brief This is a helper function that was created so that PolyMorphContainer and 
 * unique_ptr can both be used via the same interface. For this version, handle is a
 * PolyMorphContainer reference, and dummy is the type that you want to construct inside
 * handle. dummy does not actually get passed in though. It is used only for selecting
 * the correct version of 
 * 
 * @tparam Base_t 
 * @tparam n 
 * @tparam Module_t 
 * @param handle 
 * @param dummy 
 */
template <class Base_t, int n, class Module_t>
static void makeModule(PolyMorphContainer<Base_t, n>& handle, Module_t&& dummy) {
    handle.template createModule<Module_t>();
}
#endif

/**
 * @brief This is a helper function that was created so that PolyMorphContainer and 
 * unique_ptr can both be used via the same interface. For this version, handle is a
 * std::unique_ptr reference, and dummy is the type that you want to construct inside
 * handle. dummy does not actually get passed in though. It is used only for selecting
 * the correct version of 
 * 
 * @tparam Base_t 
 * @tparam Module_t 
 * @param handle 
 * @param dummy 
 */
template <class Base_t, class Module_t>
static void makeModule(std::unique_ptr<Base_t>& handle, Module_t&& dummy) {
    handle = std::make_unique<Module_t>();
}

/**
 * @brief helper function for colwerting a module list view into a module list
 * 
 * @param view_in 
 * @return const_flat_module_list_t 
 */
static inline const_flat_module_list_t viewToList(const const_base_module_list_t& view_in){
    const_flat_module_list_t output_list;
    for (const FSModule_t* ptr : view_in){
        output_list.push_back(ptr);
    }
    return output_list;
}

//-----------------------------------------------------------------------------
// More util functions in global scope
//-----------------------------------------------------------------------------

bool isInMustEnableList(const FUSESKUConfig::SkuConfig& sku, module_type_id module_type, uint32_t module_idx);
void sortListByEnableCountAscending(const_flat_module_list_t& node_list, const std::vector<module_type_id>& submodule_type_priority);
void sortListByEnableCountDescending(const_flat_module_list_t& node_list, const std::vector<module_type_id>& submodule_type_priority);
void sortMajorUnitByMinorCount(const const_base_module_list_t& major_unit_list, const const_base_module_list_t& minor_unit_list,
                               module_index_list_t& idxs_to_sort, const PESMapping_t& mapping);
module_index_list_t sortMajorUnitByMinorCount(const const_base_module_list_t& major_unit_list, 
                                                const const_base_module_list_t& minor_unit_list);

std::ostream& operator<<(std::ostream& os, const FbpMasks& masks);
std::ostream& operator<<(std::ostream& os, const GpcMasks& masks);

}  // namespace fslib

