#include "fs_chip_core.h"
#include "fs_chip_factory.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <set>
#include <numeric>
#include <math.h>
#include <assert.h>
#ifdef _MSC_VER
#pragma warning(disable : 4068 4267) //disable warnings about gcc pragmas and size_t to uint32_t colwersion
#endif
#pragma GCC diagnostic ignored "-Wunused-value"

namespace fslib {

//-----------------------------------------------------------------------------
// ID to string mapping functions
//-----------------------------------------------------------------------------

/**
 * @brief Return a map of str to enum for FSable module names
 * 
 * @return std::map<std::string, module_type_id> 
 */
static std::map<std::string, module_type_id> makeStrToIdMap() {
    std::map<std::string, module_type_id> str_to_id_map;
    str_to_id_map["fbp"]      = module_type_id::fbp;
    str_to_id_map["gpc"]      = module_type_id::gpc;
    str_to_id_map["rop"]      = module_type_id::rop;
    str_to_id_map["tpc"]      = module_type_id::tpc;
    str_to_id_map["pes"]      = module_type_id::pes;
    str_to_id_map["cpc"]      = module_type_id::cpc;
    str_to_id_map["ltc"]      = module_type_id::ltc;
    str_to_id_map["fbio"]     = module_type_id::fbio;
    str_to_id_map["l2slice"]  = module_type_id::l2slice;
    str_to_id_map["halffbpa"] = module_type_id::halffbpa;
    str_to_id_map["chip"]     = module_type_id::chip;
    str_to_id_map["invalid"]  = module_type_id::invalid;
    return str_to_id_map;
}

/**
 * @brief Get a const reference to the Str To Id Map object
 * 
 * @return const std::map<std::string, module_type_id>& 
 */
const std::map<std::string, module_type_id>& getStrToIdMap() {
    static std::map<std::string, module_type_id> str_to_id_map = makeStrToIdMap();
    return str_to_id_map;
}

/**
 * @brief create a map of enum to string. ilwerts the map created by makeStrToIdMap
 * 
 * @return std::map<module_type_id, std::string> 
 */
static std::map<module_type_id, std::string> makeIdToStrMap() {
    std::map<module_type_id, std::string> id_to_str_map;
    for (const auto& kv : getStrToIdMap()){
        // Check to make sure there wasn't a typo in the map above. There should be no duplicates
        if (id_to_str_map.find(kv.second) != id_to_str_map.end()) { throw FSLibException("found duplicate entry in makegetStrToIdMap()!");}

        // ilwert the map iteratively
        id_to_str_map[kv.second] = kv.first;
    }
    return id_to_str_map;
}

/**
 * @brief Get a const reference to the Id To Str Map object
 * 
 * @return const std::map<module_type_id, std::string>& 
 */
const std::map<module_type_id, std::string>& getIdToStrMap() {
    static std::map<module_type_id, std::string> id_to_str_map = makeIdToStrMap();
    return id_to_str_map;
}

/**
 * @brief Map instance name to enum
 * 
 * @param module_id 
 * @return const std::string& 
 */
const std::string& idToStr(module_type_id module_id){
    const auto& map_it = getIdToStrMap().find(module_id);
    return map_it->second;
}

/**
 * @brief map str instance name to enum
 * 
 * @param module_str 
 * @return module_type_id 
 */
module_type_id strToId(const std::string& module_str){
    const auto& map_it = getStrToIdMap().find(module_str);
    return map_it->second;
}

std::ostream& operator<<(std::ostream& os, const module_type_id& module_id) {
    os << idToStr(module_id);
    return os;
}

/**
 * @brief colwert a const_base_module_list_t to base_module_list_t.
 * It's only appropriate use is for code reuse in getFlatList and returnModuleList
 * 
 * @param module_list 
 * @return base_module_list_t 
 */
static base_module_list_t make_nonconst_module_list(const const_base_module_list_t& module_list){
#ifdef USEBASEVECTORVIEW
    // if it's a FSModuleVectorView, then colwert it to non-const with removeConst
    return module_list.removeConst();
#else
    // otherwise, copy all the pointers to a new vector and make them non-const
    base_module_list_t return_list;
    reserve(return_list, module_list.size());
    for(const FSModule_t* ptr : module_list){
        return_list.push_back(const_cast<FSModule_t*>(ptr));
    }
    return return_list;
#endif
}

static flat_module_list_t make_nonconst_flat_module_list(const const_flat_module_list_t& module_list){
    flat_module_list_t return_list;
    reserve(return_list, module_list.size());
    for(const FSModule_t* ptr : module_list){
        return_list.push_back(const_cast<FSModule_t*>(ptr));
    }
    return return_list;
}

/**
 * @brief Return true if this type and index is explicity specified in the SKU's mustEnableList
 * 
 * @param sku 
 * @param module_id 
 * @param module_idx 
 * @return true 
 * @return false 
 */
bool isInMustEnableList(const FUSESKUConfig::SkuConfig& sku, module_type_id module_id, uint32_t module_idx){
    const auto& sku_fs_config_it = sku.fsSettings.find(idToStr(module_id));
    if (sku_fs_config_it == sku.fsSettings.end()){
        return false;
    }
    return std::find(sku_fs_config_it->second.mustEnableList.begin(), sku_fs_config_it->second.mustEnableList.end(), module_idx) != sku_fs_config_it->second.mustEnableList.end();
}

/**
 * @brief Return true if this type is implicitly specified in the SKU's mustEnableList
 * An example of implicit mustEnable would be a GPC whose TPC is in the mustEnableList
 * 
 * @param sku 
 * @param module_id 
 * @param module_idx 
 * @return true 
 * @return false 
 */
bool FSChip_t::isMustEnable(const FUSESKUConfig::SkuConfig& sku, module_type_id module_id, uint32_t module_idx) const {

    if (isInMustEnableList(sku, module_id, module_idx)){
        return true;
    }

    // check if this module has child or dependent modules that are must enable. If they are, then this module is also must enable
    // try checking by disabling this module and checking if any mustEnable modules get disabled
    // pretty heavy handed, but leverages the funcDownBin code, so worth it for now
    std::unique_ptr<FSChip_t> fresh_chip = createChip(getChipEnum());

    fresh_chip->getModuleFlatIdx(module_id, module_idx)->setEnabled(false);
    fresh_chip->funcDownBin();

    //check the mustEnables and if any are disabled after funcDownBin, then we know this module is a mustEnable
    for (const auto& fsconfig_it : sku.fsSettings){
        const_flat_module_list_t must_enable_module_flat_list = static_cast<const FSChip_t*>(fresh_chip.get())->getFlatModuleList(strToId(fsconfig_it.first));
        for (uint32_t must_enable_idx : fsconfig_it.second.mustEnableList){
            if (!must_enable_module_flat_list[must_enable_idx]->getEnabled()){
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief Get the preferred disable list after removing instances that would violate the mustEnableList
 * This is a wrapper around getPreferredDisableList
 * 
 * @param module_type 
 * @param sku 
 * @return module_index_list_t 
 */
module_index_list_t FSChip_t::getPreferredDisableListMustEnable(module_type_id module_type, const FUSESKUConfig::SkuConfig& sku) const {
    module_index_list_t disable_list = getPreferredDisableList(module_type, sku);

    const auto& sku_fs_config_it = sku.fsSettings.find(idToStr(module_type));
    if (sku_fs_config_it == sku.fsSettings.end()){
        return disable_list;
    }

    module_index_list_t return_list;

    for (uint32_t module_idx : disable_list){
        if (!isMustEnable(sku, module_type, module_idx)){
            return_list.push_back(module_idx);
        }
    }
    
    return return_list;
}

/**
 * @brief Implementation for module_index_list_t sortMajorUnitByMinorCount(const const_flat_module_list_t& major_unit_list, 
                                                const const_flat_module_list_t& minor_unit_list)
 * 
 * @param major_unit_list 
 * @param minor_unit_list 
 * @param idxs_to_sort 
 * @param mapping  PESMapping_t is being used generically here. todo: rename it to be generic
 */
void sortMajorUnitByMinorCount(const const_base_module_list_t& major_unit_list, const const_base_module_list_t& minor_unit_list,
                               module_index_list_t& idxs_to_sort, const PESMapping_t& mapping) {
    
    // sort major unit idxs_to_sort by number of enabled minor units connected
    auto sortComparison = [&mapping, &idxs_to_sort, &minor_unit_list](uint32_t left, uint32_t right) -> bool {
        auto getNumEnabledMinorUnits = [](const PESMapping_t::RangeIteratorView& minor_idx_list, const const_base_module_list_t& minor_unit_list) -> uint32_t {
            uint32_t count = 0;
            for (int minor_idx : minor_idx_list) {
                if (minor_unit_list[minor_idx]->getEnabled()) {
                    count += 1;
                }
            }
            return count;
        };

        auto minor_idxs_for_left = mapping.getPEStoTPCMapping(left);
        auto minor_idxs_for_right = mapping.getPEStoTPCMapping(right);
        return getNumEnabledMinorUnits(minor_idxs_for_left, minor_unit_list) > getNumEnabledMinorUnits(minor_idxs_for_right, minor_unit_list); 
    };
    std::stable_sort(idxs_to_sort.begin(), idxs_to_sort.end(), sortComparison);
}

bool MaskComparer::operator() (const mask32_t &b1, const mask32_t &b2) const {
    return b1.to_ulong() < b2.to_ulong();
}

/**
 * @brief Order a list of major units based on number of enabled minor units per major unit
 * For example, sort a list of PESs based on the number of enabled TPCs for each PES
 * This requires the typical mapping between major and minor unit
 * The return type is a list of major unit indexes
 * 
 * @param major_unit_list 
 * @param minor_unit_list 
 * @return module_index_list_t 
 */
module_index_list_t sortMajorUnitByMinorCount(const const_base_module_list_t& major_unit_list, 
                                                const const_base_module_list_t& minor_unit_list) {

    module_index_list_t idxs;
    idxs.resize(major_unit_list.size());
    std::iota(idxs.begin(), idxs.end(), 0);

    PESMapping_t mapping(major_unit_list.size(), minor_unit_list.size());
    sortMajorUnitByMinorCount(major_unit_list, minor_unit_list, idxs, mapping);
    return idxs;
}

//-----------------------------------------------------------------------------
// PESMapping_t
//-----------------------------------------------------------------------------

/**
 * @brief Construct a new PESMapping_t object
 *
 * The algorithm works as follows:
 * Assign TPCs to the PESs starting with PES0 and moving up to PESn
 * Wrap back to PES0 and continue assigning TPCs. 
 * This determines how many TPCs each PES has.
 * Next, assign the specific TPCs to the PESs in order of TPCs
 * Don't move to the next PES until all the TPCs are assigned to the first one
 * For example, a GPC with 5 TPCs and 2 PESs would have the following mapping:
 * PES0 -> TPC0, TPC1, TPC2
 * PES1 -> TPC3, TPC4
 * A GPC with 8 TPCs, 3 PESs would have the following mapping:
 * PES0 -> TPC0, TPC1, TPC2
 * PES1 -> TPC3, TPC4, TPC5
 * PES2 -> TPC6, TPC7
 * 
 * @param num_pess 
 * @param num_tpcs 
 */
PESMapping_t::PESMapping_t(int num_pess, int num_tpcs) : m_num_tpcs(num_tpcs), m_num_pess(num_pess) {

}

/**
 * @brief Get the PES IDX that the TPC at this IDX connects to
 * 
 * @param tpc_id 
 * @return int 
 */
int PESMapping_t::getTPCtoPESMapping(int tpc_id) const {
    int tpc_to_pes_ratio = static_cast<int>(ceil(static_cast<float>(m_num_tpcs) / static_cast<float>(m_num_pess)));
    return tpc_id / tpc_to_pes_ratio;
}

/**
 * @brief Get a const ref to a list of TPC IDXs that the PES at this IDX connects to
 * 
 * @param tpc_id 
 * @return const tpc_list_t& 
 */
PESMapping_t::tpc_list_t PESMapping_t::getPEStoTPCMappingVector(int pes_id) const {
    tpc_list_t tpc_list;
    for (int i : getPEStoTPCMapping(pes_id)){
        tpc_list.push_back(i);
    }
    return tpc_list;
}

/**
 * @brief Get a RangeIteratorView, which can iterate over the TPC IDXs that connect to this PES
 * 
 * @param pes_id 
 * @return PESMapping_t::RangeIteratorView 
 */
PESMapping_t::RangeIteratorView PESMapping_t::getPEStoTPCMapping(int pes_id) const {
    int tpc_to_pes_ratio = static_cast<int>(ceil(static_cast<float>(m_num_tpcs) / static_cast<float>(m_num_pess)));
    int begin_tpc = pes_id * tpc_to_pes_ratio;
    int end_tpc = begin_tpc + tpc_to_pes_ratio; //one past end tpc actually
    if (end_tpc > m_num_tpcs){
        end_tpc = m_num_tpcs;
    }
    RangeIteratorView range_view(begin_tpc, end_tpc);
    return range_view;
}

/**
 * @brief For debugging only. Print out the mapping
 * 
 * @return std::string 
 */
std::string PESMapping_t::printMapping() const {
    std::stringstream map_sstr;
    for (int tpc_id = 0; tpc_id < m_num_tpcs; tpc_id++){
        map_sstr << "tpc[" << tpc_id << "]=>" << getTPCtoPESMapping(tpc_id) << "\n";
    }
    for (int pes_id = 0; pes_id < m_num_pess; pes_id++){
        map_sstr << "pes[" << pes_id << "]=>";
        for (int tpc_id : getPEStoTPCMapping(pes_id)){
            map_sstr << tpc_id << ", ";
        }
        map_sstr << "\n";
    }

    return map_sstr.str();
}


/**
 * @brief begin iterator, points to the first TPC in the group
 * 
 * @return PESMapping_t::RangeIteratorView::iterator 
 */
PESMapping_t::RangeIteratorView::iterator PESMapping_t::RangeIteratorView::begin() const {
    return iterator(m_begin);
}

/**
 * @brief end iterator, points 1 past the end of the last TPC in the group
 * 
 * @return PESMapping_t::RangeIteratorView::iterator 
 */
PESMapping_t::RangeIteratorView::iterator PESMapping_t::RangeIteratorView::end() const {
    return iterator(m_end);
}

/**
 * @brief Construct a new RangeIteratorView object
 * begin and end are TPC idxs
 * end should be 1 past the last TPC in the group
 * 
 * @param begin 
 * @param end 
 */
PESMapping_t::RangeIteratorView::RangeIteratorView(int begin, int end) : m_begin(begin), m_end(end) {
}

/**
 * @brief Construct a new pesmapping t::rangeiteratorview::iterator::iterator object
 * 
 * @param start 
 */
PESMapping_t::RangeIteratorView::iterator::iterator(uint32_t start) : m_val(start) {
}

/**
 * @brief Get the module idx
 * 
 * @return uint32_t 
 */
uint32_t PESMapping_t::RangeIteratorView::iterator::operator*() const {
    return m_val;
}

/**
 * @brief pre increment
 * 
 * @return PESMapping_t::RangeIteratorView::iterator& 
 */
PESMapping_t::RangeIteratorView::iterator& PESMapping_t::RangeIteratorView::iterator::operator++() {
    m_val++;
    return *this;
}

/**
 * @brief post increment
 * 
 * @return PESMapping_t::RangeIteratorView::iterator 
 */
PESMapping_t::RangeIteratorView::iterator PESMapping_t::RangeIteratorView::iterator::operator++(int) {
    iterator tmp = *this;
    ++(*this);
    return tmp;
}

/**
 * @brief == operator
 * 
 * @param other 
 * @return true 
 * @return false 
 */
bool PESMapping_t::RangeIteratorView::iterator::operator==(const iterator& other) const {
    return m_val == other.m_val;
}

/**
 * @brief != operator
 * 
 * @param other 
 * @return true 
 * @return false 
 */
bool PESMapping_t::RangeIteratorView::iterator::operator!=(const iterator& other) const {
    return !(*this == other);
}


//-----------------------------------------------------------------------------
// FSModule_t
//-----------------------------------------------------------------------------

/**
 * @brief Destroy the fsmodule t::fsmodule t object
 * 
 */
FSModule_t::~FSModule_t() {

}

/**
 * @brief get the enable status of this module
 * 
 * @return true if enabled
 * @return false if disabled
 */
bool FSModule_t::getEnabled() const {
    return enabled;
}

/**
 * @brief Set the enable status of this module
 * 
 * @param en true for enabled, false for disabled
 */
void FSModule_t::setEnabled(bool en) {
    enabled = en;
}

/**
 * @brief Get the name of this module type
 * 
 * @return std::string 
 */
std::string FSModule_t::getTypeStr() const {
    return idToStr(getTypeEnum());
}

/**
 * @brief Return a flattened list of all modules of this type.
 * 
 * @param module_hierarchy A list of module types. This is used to select the module type at each hierarchy
 * @return const_flat_module_list_t 
 */
const_flat_module_list_t FSModule_t::getFlatList(const hierarchy_list_t& module_hierarchy) const {
    const_flat_module_list_t flat_list;
    getFlatList(module_hierarchy, 0, flat_list);
    return flat_list;
}

/**
 * @brief Return a flattened list of all modules of this type.
 * This non-const version does a const cast and calls the const version to avoid copy paste
 * @param module_hierarchy A list of module types. This is used to select the module type at each hierarchy
 * @return flat_module_list_t 
 */
flat_module_list_t FSModule_t::getFlatList(const hierarchy_list_t& module_hierarchy) {
    return make_nonconst_flat_module_list(static_cast<const FSModule_t*>(this)->getFlatList(module_hierarchy));
}

/**
 * @brief get a flattened list of all modules of this type.
 * 
 * @param module_hierarchy A list of module types. This is used to select the module type at each hierarchy
 * @param flat_list this vector will be populated with pointers to the modules
 */
void FSModule_t::getFlatList(const hierarchy_list_t& module_hierarchy, uint32_t hierarchy_level, const_flat_module_list_t& flat_list) const {
    
    const_base_module_list_t child_unit_list = returnModuleList(module_hierarchy[hierarchy_level]);
    for(const auto& child_unit : child_unit_list) {
        if (hierarchy_level + 1 < module_hierarchy.size()){
            child_unit->getFlatList(module_hierarchy, hierarchy_level + 1, flat_list);
        } else {
            flat_list.push_back(child_unit);
        }
    }
}

/**
 * @brief Return a pointer to the module of this type at this index.
 * The index used is the global flat index for all modules of this type within the chip
 * This is a private worker function for getModuleFlatIdx
 * 
 * @param module_hierarchy 
 * @param hierarchy_level 
 * @param flat_idx 
 * @return const FSModule_t* 
 */
const FSModule_t* FSModule_t::getModuleFlatIdxImpl(const hierarchy_list_t& module_hierarchy, uint32_t hierarchy_level, uint32_t flat_idx) const {
    const_base_module_list_t child_unit_list = returnModuleList(module_hierarchy[hierarchy_level]);
    if (hierarchy_level + 1 < module_hierarchy.size()){
        uint32_t module_count = getModuleCount(module_hierarchy.back());
        uint32_t modules_per_group = module_count / child_unit_list.size();
        uint32_t child_idx = flat_idx / modules_per_group;
        uint32_t child_offset = flat_idx % modules_per_group;
        return child_unit_list[child_idx]->getModuleFlatIdxImpl(module_hierarchy, hierarchy_level + 1, child_offset);
    } else {
        return child_unit_list[flat_idx];
    }
}

/**
 * @brief Return a pointer to the module of this type at this index.
 * The index used is the global flat index for all modules of this type within the chip
 * 
 * @param type_id 
 * @param flat_idx 
 * @return const FSModule_t* 
 */
const FSModule_t* FSModule_t::getModuleFlatIdx(module_type_id type_id, uint32_t flat_idx) const {
    return getModuleFlatIdxImpl(findHierarchy(type_id), 0, flat_idx);
}

/**
 * @brief non-const version of getModuleFlatIdx
 * 
 * @param type_id 
 * @param flat_idx 
 * @return FSModule_t* 
 */
FSModule_t* FSModule_t::getModuleFlatIdx(module_type_id type_id, uint32_t flat_idx) {
    return const_cast<FSModule_t*>(static_cast<const FSModule_t*>(this)->getModuleFlatIdx(type_id, flat_idx));
}

/**
 * @brief Get the total number of modules of this type in the chip.
 * This is a private helper function for getModuleCount.
 * 
 * @param module_hierarchy 
 * @param hierarchy_level 
 * @return uint32_t 
 */
uint32_t FSModule_t::getModuleCountImpl(const hierarchy_list_t& module_hierarchy, uint32_t hierarchy_level) const {
    const_base_module_list_t child_unit_list = returnModuleList(module_hierarchy[hierarchy_level]);

    if (child_unit_list[0]->getTypeEnum() == module_hierarchy.back()){
        return child_unit_list.size();
    } else {
        return child_unit_list.size() * child_unit_list[0]->getModuleCountImpl(module_hierarchy, hierarchy_level + 1);
    }
}

/**
 * @brief Get the total number of modules of this type in the chip.
 * 
 * @param type_id 
 * @return uint32_t 
 */
uint32_t FSModule_t::getModuleCount(module_type_id type_id) const {
    return getModuleCountImpl(findHierarchy(type_id), 0);
}


/**
 * @brief Return a base_module_list_t containing FSModule_t pointers to the modules of this type
 * Return an empty list if the requested module type is not a child module of this type
 * 
 * @param module_type 
 * @return base_module_list_t 
 */
base_module_list_t FSModule_t::returnModuleList(module_type_id module_type) {
    return make_nonconst_module_list(static_cast<const FSModule_t*>(this)->returnModuleList(module_type));
}

/**
 * @brief Return a const_base_module_list_t containing FSModule_t pointers to the modules of this type
 * Return an empty list if the requested module type is not a child module of this type
 * 
 * @param module_type 
 * @return const_base_module_list_t 
 */
const_base_module_list_t FSModule_t::returnModuleList(module_type_id module_type) const {
    const module_map_t& map_ref = getModuleMapRef();
    const auto& map_it = map_ref.find(module_type);
    
    if (map_it != map_ref.end()){
        auto& map_func = map_it->second;
        return map_func(*this);
    }

    // return an empty list if the type is not child module
    return {};
}

/**
 * @brief Sort a list of modules by the number of good submodules. 
 * 
 * @param node_list list of FSModule_t*
 * @param submodule_type_priority a list of submodule types to choose the priority
 */
void sortListByEnableCountAscending(const_flat_module_list_t& node_list, const std::vector<module_type_id>& submodule_type_priority){
    struct compare_subunit_counts {
        const std::vector<module_type_id>& sub_module_priorities;
        compare_subunit_counts(const std::vector<module_type_id>& submodule_priority) : sub_module_priorities(submodule_priority) {
        }
        bool operator()(const FSModule_t* l, const FSModule_t* r) const {
            for (module_type_id id : sub_module_priorities) {
                if (l->getEnableCount(id) != r->getEnableCount(id)){
                    return l->getEnableCount(id) < r->getEnableCount(id);
                }
            }
            return false; // both have identical instance counts, so not less than
        }
    };
    std::stable_sort(node_list.begin(), node_list.end(), compare_subunit_counts(submodule_type_priority));
}

/**
 * @brief Sort a list of modules by the number of good submodules.
 * TODO make this not a copy paste of sortListByEnableCountAscending with one thing changed
 * @param node_list list of FSModule_t*
 * @param submodule_type_priority a list of submodule types to choose the priority
 */
void sortListByEnableCountDescending(const_flat_module_list_t& node_list, const std::vector<module_type_id>& submodule_type_priority){
    struct compare_subunit_counts {
        const std::vector<module_type_id>& sub_module_priorities;
        compare_subunit_counts(const std::vector<module_type_id>& submodule_priority) : sub_module_priorities(submodule_priority) {
        }
        bool operator()(const FSModule_t* l, const FSModule_t* r) const {
            for (module_type_id id : sub_module_priorities) {
                if (l->getEnableCount(id) != r->getEnableCount(id)){
                    return l->getEnableCount(id) > r->getEnableCount(id);
                }
            }
            return false; // both have identical instance counts, so not less than
        }
    };
    std::stable_sort(node_list.begin(), node_list.end(), compare_subunit_counts(submodule_type_priority));
}

/**
 * @brief Get a list of module indexes sorted by floorswepeing preference. If we had to floorsweep one of them, this is the order to do it in
 * 
 * @param module_type 
 * @param submodule_type 
 * @return module_index_list_t 
 */
module_index_list_t FSModule_t::getSortedListByEnableCount(module_type_id module_type, module_type_id submodule_type) const {
    const_flat_module_list_t node_list = viewToList(returnModuleList(module_type));
    sortListByEnableCountAscending(node_list, {submodule_type});
    module_index_list_t sorted_indexes;

    for (const FSModule_t* module_ptr : node_list){
        sorted_indexes.push_back(module_ptr->instance_number);
    }

    return sorted_indexes;
}

/**
 * @brief If there is a tie in enable counts, pick submodules in this order
 * 
 * @param flat_list 
 */
void FSModule_t::preSortDownBinPreference(const_flat_module_list_t& module_list) const {
    module_index_list_t tie_preference = getFSTiePreference(module_list.at(0)->getTypeEnum());
    const_flat_module_list_t reorder_module_ptrs;

    // first pick out the prioritized pointers
    for (uint32_t idx : tie_preference){
        reorder_module_ptrs.push_back(module_list[idx]);
    }

    // then copy over the pointers that weren't specified
    for (uint32_t i = 0; i < module_list.size(); i++){
        if (std::find(tie_preference.begin(), tie_preference.end(), i) == tie_preference.end()){
            reorder_module_ptrs.push_back(module_list[i]);
        }
    }
    module_list = reorder_module_ptrs;
}

/**
 * @brief Return a list of module indexes to floorsweep first in the event that they have the same good submodule counts
 * The default case is that there is no preference
 * 
 * @param module_type what module type we are floorsweeping
 * @return module_index_list_t 
 */
module_index_list_t FSModule_t::getFSTiePreference(module_type_id module_type) const {
    return {};
}

/**
 * @brief Return a set containing all the submodule types contained in this module as well as submodules
 * 
 * @return module_set_t 
 */
module_set_t FSModule_t::findAllSubUnitTypes() const {
    module_set_t all_modules;
    findAllSubUnitTypes(all_modules);
    return all_modules;
}

/**
 * @brief return a vector of module types to iterate over while downbinning.
 * This is not used, but it may be useful if we decide we need to pick gpcs to floorsweep before tpcs for example.
 * 
 * @return std::vector<module_type_id> 
 */
std::vector<module_type_id> FSModule_t::getSortedDownBinTypes() const {
    std::vector<module_type_id> output_list;
    module_set_t priority_modules;

    for (module_type_id priority_type : getDownBinTypePriority()){
        output_list.push_back(priority_type);
        priority_modules.insert(priority_type);
    }

    for (module_type_id other_type : findAllSubUnitTypes()){
        if (priority_modules.find(other_type) == priority_modules.end()){
            output_list.push_back(other_type);
        }
    }

    return output_list;
}

/**
 * @brief Default is no preference. Chips can override this and pick their own order preference.
 * 
 * @return std::vector<module_type_id> 
 */
std::vector<module_type_id> FSModule_t::getDownBinTypePriority() const {
    return {};
}

/**
 * @brief Relwrsively iterate over all submodules of this module type and populate
 * a std::set with all the module types
 * 
 * @param modules_set 
 */
void FSModule_t::findAllSubUnitTypes(module_set_t& modules_set) const {
    for (module_type_id submodule_id : getSubModuleTypes()){
        modules_set.insert(submodule_id);
        for (const FSModule_t* child_module : returnModuleList(submodule_id)){
            child_module->findAllSubUnitTypes(modules_set);
        }
    }
}

/**
 * @brief Traverse all submodules of this module and create a map of the hierarchy
 * 
 * @param breadcrumbs 
 * @param hierarchy_map 
 */
void FSModule_t::findHierarchyMap(hierarchy_list_t& breadcrumbs, hierarchy_map_t& hierarchy_map) const {
    for (module_type_id submodule_id : getSubModuleTypes()){
        breadcrumbs.push_back(submodule_id);
        if (hierarchy_map.find(submodule_id) == hierarchy_map.end()){
            hierarchy_map[submodule_id] = breadcrumbs;
        }
        for (const FSModule_t* child_module : returnModuleList(submodule_id)){
            child_module->findHierarchyMap(breadcrumbs, hierarchy_map);
        }
        breadcrumbs.pop_back();
    }
}

/**
 * @brief Check all the submodules of this module with the sku spec to make sure the group count attributes are satisfied
 * 
 * @param potentialSKU 
 * @param msg 
 * @return true 
 * @return false 
 */
bool FSModule_t::moduleCanFitSKU(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {
    for (module_type_id submodule_id : getSubModuleTypes()){
        const auto& sku_fs_config_it = potentialSKU.fsSettings.find(idToStr(submodule_id));
        if (sku_fs_config_it != potentialSKU.fsSettings.end()){
            if (!moduleCanFitSKU(submodule_id, sku_fs_config_it->second, msg)){
                return false;
            }
            if (!isValid(FSmode::FUNC_VALID, msg)) {
                return false;
            }
        }
    }
    return true;
}

/**
 * @brief Check if specific submodules can meet the group requirements of the SKU
 * 
 * @param submodule_id 
 * @param skuFSSettings 
 * @param msg 
 * @return true 
 * @return false 
 */
bool FSModule_t::moduleCanFitSKU(module_type_id submodule_id, const FUSESKUConfig::FsConfig& skuFSSettings, std::string& msg) const {

    uint32_t must_disable_count = 0;
    for(auto must_disable_idx : skuFSSettings.mustDisableList){
        // check if the child's must_disable is inside this module
        const_base_module_list_t submodule_list = returnModuleList(submodule_id);
        uint32_t must_disable_parent_idx = must_disable_idx / submodule_list.size();
        uint32_t must_disable_local_idx = must_disable_idx % submodule_list.size();
        if (must_disable_parent_idx == instance_number){
            // now also check if the must disable is not already defective
            if (submodule_list[must_disable_local_idx]->getEnabled()){
                must_disable_count += 1;
            }
        }
    }

    uint32_t enable_count_after_applying_must_disable = getEnableCount(submodule_id) - must_disable_count;
    if (enable_count_after_applying_must_disable < skuFSSettings.getMinGroupCount()){
        std::stringstream ss;
        ss << "The SKU asks for at least " << skuFSSettings.getMinGroupCount() << " ";
        ss << submodule_id << "s per " << getTypeEnum() << ", but " << getTypeEnum() << instance_number;
        ss << " has only " << enable_count_after_applying_must_disable << " " << submodule_id << "s enabled";
        ss << " after disabling an additional " << must_disable_count << " " << submodule_id << "s";
        ss << " according to the mustDisableList";
        msg = ss.str();
        return false;
    }
    return true;
}

/**
 * @brief Check if the module's submodules fit the SKU requirements
 * different from moduleCanFitSKU, which checks if it's possible to downbin into the SKU
 * 
 * @param potentialSKU 
 * @param msg 
 * @return true 
 * @return false 
 */
bool FSModule_t::moduleIsInSKU(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {
    for (module_type_id submodule_id : getSubModuleTypes()){
        const auto& sku_fs_config_it = potentialSKU.fsSettings.find(idToStr(submodule_id));
        if (sku_fs_config_it != potentialSKU.fsSettings.end()){
            if (!moduleIsInSKU(submodule_id, sku_fs_config_it->second, msg)){
                return false;
            }
        }
    }
    return true;
}

/**
 * @brief Check specific submodules of this module are in the sku
 * 
 * @param submodule_id 
 * @param skuFSSettings 
 * @param msg 
 * @return true 
 * @return false 
 */
bool FSModule_t::moduleIsInSKU(module_type_id submodule_id, const FUSESKUConfig::FsConfig& skuFSSettings, std::string& msg) const {

    if (skuFSSettings.minEnablePerGroup != UNKNOWN_ENABLE_COUNT){
        if (getEnableCount(submodule_id) < skuFSSettings.minEnablePerGroup){
            std::stringstream ss;
            ss << "The SKU asks for at least " << skuFSSettings.minEnablePerGroup << " ";
            ss << submodule_id << "s per " << getTypeEnum() << ", but " << getTypeEnum() << instance_number;
            ss << " has only " << getEnableCount(submodule_id) << " " << submodule_id << "s enabled!";
            msg = ss.str();
            return false;
        }
    }

    if (skuFSSettings.enableCountPerGroup != UNKNOWN_ENABLE_COUNT){
        if (getEnableCount(submodule_id) != skuFSSettings.enableCountPerGroup){
            std::stringstream ss;
            ss << "The SKU asks for " << skuFSSettings.enableCountPerGroup << " ";
            ss << submodule_id << "s per " << getTypeEnum() << ", but " << getTypeEnum() << instance_number;
            ss << " has only " << getEnableCount(submodule_id) << " " << submodule_id << "s enabled!";
            msg = ss.str();
            return false;
        }
    }

    return true;
}

/**
 * @brief Check if the enable count for this submodule type is valid
 * 
 * @param submodule_type 
 * @param fsmode 
 * @param msg 
 * @return true 
 * @return false 
 */
bool FSModule_t::enableCountIsValid(module_type_id submodule_type, FSmode fsmode, std::string& msg) const {
    return true;
}

/**
 * @brief Check if the total enable counts for all submodules is valid
 * 
 * @param fsmode 
 * @param msg 
 * @return true 
 * @return false 
 */
bool FSModule_t::enableCountsAreValid(FSmode fsmode, std::string& msg) const {
    for (module_type_id submodule_id : findAllSubUnitTypes()){
        if(!enableCountIsValid(submodule_id, fsmode, msg)){
            return false;
        }
    }
    return true;
}

/**
 * @brief Copy assignment operator for all modules.
 * Copy the enabled member, then copy all the submodules over
 * 
 * @param other 
 * @return FSModule_t& 
 */
FSModule_t& FSModule_t::operator=(const FSModule_t& other) {
    setEnabled(other.getEnabled());

    for (module_type_id submodule_id : getSubModuleTypes()) {
        base_module_list_t our_modules = returnModuleList(submodule_id);
        const_base_module_list_t their_modules = other.returnModuleList(submodule_id);
        for (uint32_t i = 0; i < our_modules.size(); i++){
            *our_modules[i] = *their_modules[i];
        }
    }

    return *this;
}

/**
 * @brief equality operator for all modules
 * Check if the enables are equal and check if all submodules are equal
 * 
 * @param other 
 * @return true 
 * @return false 
 */
bool FSModule_t::operator==(const FSModule_t& other) const {
    if (getEnabled() != other.getEnabled()){
        return false;
    }

    for (module_type_id submodule_id : getSubModuleTypes()) {
        const_base_module_list_t our_modules = returnModuleList(submodule_id);
        const_base_module_list_t their_modules = other.returnModuleList(submodule_id);
        for (uint32_t i = 0; i < our_modules.size(); i++){
            if(*our_modules[i] != *their_modules[i]) {
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief Ilwerse of == operator
 * 
 * @param other 
 * @return true 
 * @return false 
 */
bool FSModule_t::operator!=(const FSModule_t& other) const {
    return !(*this == other);
}

/**
 * @brief Disable all the submodules in this module that are disabled in the other submodule.
 * 
 * @param other 
 */
void FSModule_t::merge(const FSModule_t& other) {
    if (!other.getEnabled()){
        setEnabled(false);
    }

    for (module_type_id submodule_id : getSubModuleTypes()) {
        base_module_list_t our_modules = returnModuleList(submodule_id);
        const_base_module_list_t their_modules = other.returnModuleList(submodule_id);
        for (uint32_t i = 0; i < our_modules.size(); i++){
            our_modules[i]->merge(*their_modules[i]);
        }
    }
}

module_index_list_t FSModule_t::getPreferredDisableList(module_type_id module_type) const {
    return getPreferredDisableList(module_type, FUSESKUConfig::SkuConfig());
}


//-----------------------------------------------------------------------------
// ParentFSModule_t 
//-----------------------------------------------------------------------------

/**
 * @brief Get the number of enabled submodules of module_type directly beneath this module
 * If module_type is not a submodule of this module, then return 0
 * 
 * @param module_type 
 * @return uint32_t 
 */
uint32_t ParentFSModule_t::getEnableCount(module_type_id module_type) const {
    return getNumEnabled(returnModuleList(module_type));
}

/**
 * @brief Sort the list based on submodule priority. priority is a list of types in preferred order.
 * The module with the most enabled submodules of the specified priority will be chosen first.
 * 
 * @param node_list 
 * @param priority 
 */
void ParentFSModule_t::sortPreferredMidLevelModules(const_flat_module_list_t& node_list, const std::vector<module_type_id>& priority) const {
    sortListByEnableCountDescending(node_list, priority);
}

/**
 * @brief Get a list of modules to floorsweep if the type is two levels down.
 * 
 * If the type we are downbinning is not directly beneath this type, for example, if "this" is a chip and
 * module_type is a TPC, then we want to kill off TPCs in the healthiest GPC
 * 
 * @param module_type 
 * @return module_index_list_t 
 */
module_index_list_t ParentFSModule_t::getPreferredDisableListSecondLevel(module_type_id module_type) const {
    // This will not be called unless the module_type is two levels below this type
    module_type_id mid_level_type = findParentModuleTypeId(module_type);

    // pick the "healthiest" submodule to balance out
    const_flat_module_list_t node_list = getFlatModuleList(mid_level_type);

    preSortDownBinPreference(node_list);

    const_flat_module_list_t submodule_flat_list = getFlatModuleList(module_type);

    // the list will be sorted greatest to least
    sortPreferredMidLevelModules(node_list, {module_type});

    // now we have a list of mid level modules to weaken
    // now pick the order of preference for submodules within each one

    uint32_t max_local_count = node_list[0]->returnModuleList(module_type).size();

    // make it into a flat list
    module_index_list_t flat_preference;
    for(const FSModule_t* midlevel_module : node_list){
        module_index_list_t local_preference = midlevel_module->getPreferredDisableList(module_type, FUSESKUConfig::SkuConfig());
        for(uint32_t local_idx : local_preference){
            uint32_t flat_idx = (midlevel_module->instance_number * max_local_count) + local_idx;
            if (submodule_flat_list[flat_idx]->getEnabled()){
                flat_preference.push_back(flat_idx);
            }
        }

    }

    return flat_preference;
}

/**
 * @brief get a list of submodule indexes to disable in preferred order for this type of module
 * don't recommend already disabled modules
 * 
 * @param module_type what type of submodule do we want to get the order for?
 * @return module_index_list_t 
 */
module_index_list_t ParentFSModule_t::getPreferredDisableList(module_type_id module_type, const FUSESKUConfig::SkuConfig& potentialSKU) const {

    // if the submodule is two levels down, call getPreferredDisableListSecondLevel
    if (findParentModuleTypeId(findParentModuleTypeId(module_type)) == getTypeEnum()){
        return getPreferredDisableListSecondLevel(module_type);
    }

    const_flat_module_list_t node_list = getFlatModuleList(module_type);
    preSortDownBinPreference(node_list);
    if (node_list[0]->getSubModuleTypes().size() != 0){
        sortListByEnableCountAscending(node_list, node_list[0]->getSubModuleTypeDownBinPriority());
    }
    module_index_list_t sorted_indexes;
    for(const auto& node : node_list){
        if (node->getEnabled()){
            sorted_indexes.push_back(node->instance_number);
        }
    }
    return sorted_indexes;
}

/**
 * @brief initialize maps
 * 
 */
void ParentFSModule_t::setup() {

    // set the instance numbers of submodules
    for (module_type_id module_type : getSubModuleTypes()){
        base_module_list_t module_list = returnModuleList(module_type);
        for (uint32_t i = 0; i < module_list.size(); i++){
            module_list[i]->instance_number = i;
        }

    }
}

/**
 * @brief Iterate over the submodule types given by getModuleMapRef and produce a std::set
 * which can be efficiently used in range based for loops
 * 
 * @return module_set_t 
 */
module_set_t ParentFSModule_t::makeChildModuleSet() const {
    module_set_t child_set;
    for (const auto& kv : getModuleMapRef()) {
        child_set.insert(kv.first);
    }
    return child_set;
}

/**
 * @brief generate and return a mapping of submodule hierarchies
 * 
 * @return hierarchy_map_t
 */
hierarchy_map_t ParentFSModule_t::makeHierarchyMap() const {
    hierarchy_map_t hierarchy_map_out;
    hierarchy_list_t breadcrumbs;
    findHierarchyMap(breadcrumbs, hierarchy_map_out);

    return hierarchy_map_out;
}

/**
 * @brief Get a list of pointers to modules all of the type specified in type_id.
 * This list spans parent modules
 * 
 * @param type_id What type of module do we want added to the list
 * @param flat_modules list to populate with module pointers
 */
const_flat_module_list_t ParentFSModule_t::getFlatModuleList(module_type_id type_id) const {
    return getFlatList(findHierarchy(type_id));
}

/**
 * @brief return a hierarchy_list_t which shows the module hierarchy order
 * 
 * @param type_id 
 * @return hierarchy_list_t 
 */
hierarchy_list_t FSModule_t::findHierarchy(module_type_id type_id) const {
    hierarchy_list_t list;
    findHierarchy(type_id, list);
    list.pop_back();
    std::reverse(list.begin(), list.end());
    return list;
}

/**
 * @brief To be used by findHierarchy(module_type_id type_id)
 * 
 * @param type_id 
 * @param hierarchy_list 
 * @return true 
 * @return false 
 */
bool FSModule_t::findHierarchy(module_type_id type_id, hierarchy_list_t& hierarchy_list) const {

    if (getTypeEnum() == type_id){
        hierarchy_list.push_back(getTypeEnum());
        return true;
    }

    // this block is an optimization. check all module types before checking replicated instances
    for (module_type_id submodule_type : getSubModuleTypes()){
        const_base_module_list_t module_list = returnModuleList(submodule_type);
        if (module_list[0]->findHierarchy(type_id, hierarchy_list)){
            hierarchy_list.push_back(getTypeEnum());
            return true;
        }
    }

    for (module_type_id submodule_type : getSubModuleTypes()){
        const_base_module_list_t module_list = returnModuleList(submodule_type);
        for (uint32_t i = 1; i < module_list.size(); i++){
            if (module_list[i]->findHierarchy(type_id, hierarchy_list)){
                hierarchy_list.push_back(getTypeEnum());
                return true;
            }
        }
    }

    return false;
}

/**
 * @brief Get a list of pointers to modules all of the type specified in type_id.
 * This list spans parent modules
 * 
 * @param type_id What type of module do we want added to the list
 * @param flat_modules list to populate with module pointers
 */
flat_module_list_t ParentFSModule_t::getFlatModuleList(module_type_id type_id) {
    return make_nonconst_flat_module_list(static_cast<const FSChip_t*>(this)->getFlatModuleList(type_id));
}

/**
 * @brief return the parent ID of the type_id. based on the list returned by findHierarchy
 * 
 * @param type_id 
 * @return module_type_id 
 */
module_type_id ParentFSModule_t::findParentModuleTypeId(module_type_id type_id) const {
    hierarchy_list_t hierarchy = findHierarchy(type_id);
    if (hierarchy.size() > 1){
        return *(hierarchy.end() - 2);
    } else if (hierarchy.size() == 1) {
        return getTypeEnum();
    }
    return module_type_id::invalid;
}

//-----------------------------------------------------------------------------
// LeafFSModule_t 
//-----------------------------------------------------------------------------

/**
 * @brief All leaf modules are valid.
 * 
 * @param fsmode ignored
 * @param msg not set
 * @return true always
 * @return false never
 */
bool LeafFSModule_t::isValid(FSmode fsmode, std::string& msg) const {
    return true;
}

/**
 * @brief All leaf modules return 0, since they don't have any submodules
 * 
 * @param module_type 
 * @return uint32_t 
 */
uint32_t LeafFSModule_t::getEnableCount(module_type_id module_type) const {
    return 0;
}

/**
 * @brief get a list of submodule indexes to disable in preferred order. don't call on leaf modules
 * 
 * @param module_type what type of submodule do we want to get the order for?
 * @return module_index_list_t 
 */
module_index_list_t LeafFSModule_t::getPreferredDisableList(module_type_id module_type, const FUSESKUConfig::SkuConfig& potentialSKU) const {
    throw FSLibException("cannot call getPreferredDisableList on leaf modules!");
    std::vector<module_type_id> priority;
    return {};
}

/**
 * @brief Don't call this function on a leaf module
 * 
 * @return std::vector<module_type_id> 
 */
const std::vector<module_type_id>& LeafFSModule_t::getSubModuleTypeDownBinPriority() const {
    throw FSLibException("cannot call getSubModuleTypeDownBinPriority on leaf modules!");
    static const std::vector<module_type_id> downbin_priority{};
    return downbin_priority;
}

/**
 * @brief nothing to do for leaf modules
 * 
 */
void LeafFSModule_t::setup() {
}

/**
 * @brief Leaf modules will return a const ref to an empty map. They don't need to hold their own copy
 * 
 * @return const module_map_t& 
 */
const module_map_t& LeafFSModule_t::getModuleMapRef() const {
    // const static to avoid construction every time, while also not requiring every instance to maintain its own copy
    const static module_map_t empty_module_map;
    return empty_module_map;
}

/**
 * @brief Leaf modules will return a const ref to an empty set. They don't need to hold their own copy
 * 
 * @return const module_map_t& 
 */
const module_set_t& LeafFSModule_t::getSubModuleTypes() const {
    // const static to avoid construction every time, while also not requiring every instance to maintain its own copy
    const static module_set_t empty_module_set;
    return empty_module_set;
}

//-----------------------------------------------------------------------------
// GPC_t
//-----------------------------------------------------------------------------
/**
 * @brief Destroy the gpc t::gpc t object
 * 
 */
GPC_t::~GPC_t() {
}

/**
 * @brief Apply the fslib.h masks to this GPC
 * 
 * @param mask defined in fslib.h
 * @param index Which GPC this is
 */
void GPC_t::applyMask(const fslib::GpcMasks& mask, uint32_t index) {
    applyMaskToList(TPCs, mask.tpcPerGpcMasks[index]);
}

/**
 * @brief Get the fslib.h masks for this GPC
 * 
 * @param mask defined in fslib.h
 * @param index Which GPC this is
 */
void GPC_t::getMask(fslib::GpcMasks& mask, uint32_t index) const {
    mask.tpcPerGpcMasks[index] = getMaskFromList(TPCs);
}

/**
 * @brief Is this GPC valid? Only considering stuff within this GPC
 * 
 * @param fsmode degree of valid to check for 
 * @param msg if it's not valid. This will be set to a string that explains why
 * @return true if valid
 * @return false if not valid
 */
bool GPC_t::isValid(FSmode fsmode, std::string& msg) const {
    if (getEnabled()){
        if (getNumEnabled(TPCs) == 0) {
            std::stringstream ss;
            ss << getTypeStr() << instance_number << " cannot be enabled while having no enabled TPCs!";
            msg = ss.str();
            return false;
        }
    } else {
        if (getNumEnabled(TPCs) != 0) {
            std::stringstream ss;
            ss << getTypeStr() << instance_number << " cannot be disabled while having enabled TPCs!";
            msg = ss.str();
            return false;
        }
    }
    return true;
}

/**
 * @brief This function populates its submodule lists, like a constructor, except we can call virtual methods here.
 * 
 * @param settings 
 */
void GPC_t::setup(const chipPOR_settings_t& settings) {
    TPCs.resize(settings.num_tpc_per_gpc);
    for (auto& TPC : TPCs){
        createTPC(TPC);
    }
    ParentFSModule_t::setup();
}

/**
 * @brief construct a new TPC instance that is appropriate for this GPC type.
 * 
 * @param handle 
 */
void GPC_t::createTPC(TPCHandle_t& handle) const {
    makeModule(handle, TPC_t());
}

/**
 * @brief Construct a new ROP instance that is appropriate for this GPC type.
 * 
 * @return std::unique_ptr<ROP_t> 
 */
void GPC_t::createROP(ROPHandle_t& handle) const {
    makeModule(handle, ROP_t());
}

/**
 * @brief Construct a new PES instance that is appropriate for this GPC type.
 * 
 * @return std::unique_ptr<PES_t> 
 */
void GPC_t::createPES(PESHandle_t& handle) const {
    makeModule(handle, PES_t());
}


/**
 * @brief Return the module type
 * 
 * @return module_type_id 
 */
module_type_id GPC_t::getTypeEnum() const {
    return module_type_id::gpc;
} 

static const_base_module_list_t getTPCs(const FSModule_t& ref) {return getBaseModuleList(static_cast<const GPC_t&>(ref).TPCs); }

/**
 * @brief Creates a module_map_t that can be used to access its submodule lists.
 * Return a const ref. The map is in global memory
 * 
 * @return const module_map_t& 
 */
const module_map_t& GPC_t::getModuleMapRef() const {
    auto makeMap = [this]() -> module_map_t {
        module_map_t module_map;
        module_map[module_type_id::tpc] = getTPCs;
        return module_map;
    };
    const static module_map_t module_map = makeMap();
    return module_map;
}

/**
 * @brief Return a const ref set of all the submodules of this type
 * 
 * @return const module_set_t& 
 */
const module_set_t& GPC_t::getSubModuleTypes() const {
    // const static to avoid construction every time, while also not requiring every instance to maintain its own copy
    const static module_set_t child_set = makeChildModuleSet();
    return child_set;
}

/**
 * @brief Apply extra floorsweeping to attempt to make the config valid
 * 
 */
void GPC_t::funcDownBin() {
    if (getEnabled()) {
        // if the GPC has no available TPCsdisable the GPC
        if (getNumEnabled(TPCs) == 0){
            setEnabled(false);
        }
    }

    // if the GPC is disabled, disable all the TPCs
    if (!getEnabled()) {
        for(auto& TPC : TPCs){
            TPC->setEnabled(false);
        }
    }
}

/**
 * @brief This calls an optimized disable list for TPCs
 * 
 * @param module_type 
 * @param potentialSKU 
 * @return module_index_list_t 
 */
module_index_list_t GPC_t::getPreferredDisableList(module_type_id module_type, const FUSESKUConfig::SkuConfig& potentialSKU) const {
    switch(module_type){
        case module_type_id::tpc: return getPreferredDisableListTPC(potentialSKU);
        default: {
            break;
        }
    }
    return ParentFSModule_t::getPreferredDisableList(module_type, potentialSKU);
}

/**
 * @brief Only return first enabled TPC per GPC.
 * 
 * @return module_index_list_t 
 */
module_index_list_t GPC_t::getPreferredDisableListTPC(const FUSESKUConfig::SkuConfig& potentialSKU) const {
    // Just pick the lowest index TPC, since we don't have CPCs or PESs.
    module_index_list_t sorted_indexes = {};
    for (const auto& tpc : TPCs) {
        if (tpc->getEnabled()) {
            sorted_indexes.push_back(tpc->instance_number);
            break;
        }
    }
    return sorted_indexes;
}

/**
 * @brief GPC quality is based on number of good TPCs
 * 
 * @return const std::vector<module_type_id>& 
 */
const std::vector<module_type_id>& GPC_t::getSubModuleTypeDownBinPriority() const {
    static const std::vector<module_type_id> downbin_priority{module_type_id::tpc};
    return downbin_priority;
}

/**
 * @brief Kill the first TPC chosen by getPreferredDisableList
 * 
 */
void GPC_t::killAnyTPC() {
    // get the idx of the last enabled tpc
    uint32_t tpc_to_kill = getPreferredDisableList(module_type_id::tpc, FUSESKUConfig::SkuConfig())[0];
    TPCs[tpc_to_kill]->setEnabled(false);
    funcDownBin();
}

//-----------------------------------------------------------------------------
// GPCWithCPC_t
//-----------------------------------------------------------------------------

/**
 * @brief This function populates its submodule lists, like a constructor, except we can call virtual methods here.
 * 
 * @param settings 
 */
void GPCWithCPC_t::setup(const chipPOR_settings_t& settings) {
    CPCs.resize(settings.num_pes_per_gpc); //fixme
    for (auto& CPC : CPCs){
        createCPC(CPC);
    }
    GPC_t::setup(settings);
}

/**
 * @brief Create CPC
 * 
 * @return std::unique_ptr<CPC_t> 
 */
void GPCWithCPC_t::createCPC(CPCHandle_t& handle) const {
    makeModule(handle, CPC_t());
}

/**
 * @brief Apply the fslib.h masks to this GPC
 * 
 * @param mask defined in fslib.h
 * @param index Which GPC this is this
 */
void GPCWithCPC_t::applyMask(const fslib::GpcMasks& mask, uint32_t index){
    GPC_t::applyMask(mask, index);
    applyMaskToList(CPCs, mask.cpcPerGpcMasks[index]);
}

/**
 * @brief Get the fslib.h masks for this GPC
 * 
 * @param mask defined in fslib.h
 * @param index Which GPC this is
 */
void GPCWithCPC_t::getMask(fslib::GpcMasks& mask, uint32_t index) const {
    GPC_t::getMask(mask, index);
    mask.cpcPerGpcMasks[index] = getMaskFromList(CPCs);
}


/**
 * @brief Is this GPC valid? Only considering stuff within this GPC
 * 
 * @param fsmode degree of valid to check for 
 * @param msg if it's not valid. This will be set to a string that explains why
 * @return true if valid
 * @return false if not valid
 */
bool GPCWithCPC_t::isValid(FSmode fsmode, std::string& msg) const {
    PESMapping_t pes_mapping(CPCs.size(), TPCs.size());

    // check that all enabled TPCs have an enabled CPC
    for(uint32_t tpc_id = 0; tpc_id < TPCs.size(); tpc_id++){
        int cpc_id = pes_mapping.getTPCtoPESMapping(tpc_id);
        const TPC_t& tpc = *TPCs[tpc_id];
        const CPC_t& cpc = *CPCs[cpc_id];

        if (tpc.getEnabled() && !cpc.getEnabled()){
            std::stringstream ss;
            ss << "can't enable physical TPC" << tpc_id << " without CPC" << cpc_id << " enabled in GPC" << instance_number << "!";
            msg = ss.str();
            return false;
        }
    }

    // check that all disabled CPCs have no enabled TPCs
    for(uint32_t cpc_id = 0; cpc_id < CPCs.size(); cpc_id++){
        bool at_least_one_tpc_enabled = false;
        const CPC_t& cpc = *CPCs[cpc_id];
        for (int tpc_id : pes_mapping.getPEStoTPCMapping(cpc_id)){
            const TPC_t& tpc = *TPCs[tpc_id];
            at_least_one_tpc_enabled |= tpc.getEnabled();
            if (tpc.getEnabled() && !cpc.getEnabled()){
                std::stringstream ss;
                ss << "can't disable physical CPC" << cpc_id << " while TPC" << tpc_id <<  "is enabled in GPC" << instance_number << "!";
                msg = ss.str();
                return false;
            }
        }

        // check that the enabled CPCs have some enabled TPCs
        if (cpc.getEnabled() && !at_least_one_tpc_enabled){
            std::stringstream ss;
            ss << "can't enable physical CPC" << cpc_id << " in physical GPC" << instance_number << " without any TPCs enabled!";
            msg = ss.str();
            return false;
        }
    }

    return GPC_t::isValid(fsmode, msg);
}

static const_base_module_list_t getCPCs(const FSModule_t& ref) {return getBaseModuleList(dynamic_cast<const GPCWithCPC_t&>(ref).CPCs); }

/**
 * @brief Creates a module_map_t that can be used to access its submodule lists
 * 
 * @return module_map_t 
 */
const module_map_t& GPCWithCPC_t::getModuleMapRef() const {
    auto makeMap = [this]() -> module_map_t {
        module_map_t module_map = GPC_t::getModuleMapRef();
        module_map[module_type_id::cpc] = getCPCs;
        return module_map;
    };
    const static module_map_t module_map = makeMap();
    return module_map;
}

/**
 * @brief Return a const ref set of all the submodules of this type
 * 
 * @return const module_set_t& 
 */
const module_set_t& GPCWithCPC_t::getSubModuleTypes() const {
    // const static to avoid construction every time, while also not requiring every instance to maintain its own copy
    const static module_set_t child_set = makeChildModuleSet();
    return child_set;
}

/**
 * @brief Apply extra floorsweeping to attempt to make the config valid
 * 
 */
void GPCWithCPC_t::funcDownBin() {
    GPC_t::funcDownBin();

    //Disable the TPCs for every disabled CPC
    PESMapping_t pes_mapping(CPCs.size(), TPCs.size());
    for(uint32_t cpc_id = 0; cpc_id < CPCs.size(); cpc_id++){
        bool at_least_one_tpc_enabled = false;
        CPC_t* cpc = CPCs[cpc_id].get();
        for (int tpc_id : pes_mapping.getPEStoTPCMapping(cpc_id)){
            TPC_t* tpc = TPCs[tpc_id].get();
            at_least_one_tpc_enabled |= tpc->getEnabled();
            if (tpc->getEnabled() && !cpc->getEnabled()){
                tpc->setEnabled(false);
            }
        }

        //Disable the CPC if all the TPCs are disabled
        if (cpc->getEnabled() && !at_least_one_tpc_enabled){
            cpc->setEnabled(false);
        }
    }

    if (getEnabled()) {
        // if the GPC has no available CPCs or TPCs, disable the GPC
        if (getNumEnabled(CPCs) == 0 || getNumEnabled(TPCs) == 0){
            setEnabled(false);
        }
    }

    if (!getEnabled()) {
        //Disable the CPCs if the GPC is disabled
        for(auto& CPC : CPCs){
            CPC->setEnabled(false);
        }
    }
}

/**
 * @brief Return a list of CPC idxs sorted based on enabled TPC count per CPC
 * 
 * @return module_index_list_t 
 */
module_index_list_t GPCWithCPC_t::getCPCIdxsByTPCCount() const {
    return sortMajorUnitByMinorCount(getBaseModuleList(CPCs), getBaseModuleList(TPCs));
}



/**
 * @brief Balance out the TPCs per CPC.  Only return first TPC per GPC.
 * 
 * @return module_index_list_t 
 */
module_index_list_t GPCWithCPC_t::getPreferredDisableListTPC(const FUSESKUConfig::SkuConfig& potentialSKU) const {

    module_index_list_t cpc_idxs = getCPCIdxsByTPCCount();
    const PESMapping_t cpc_map(CPCs.size(), TPCs.size());

    // now we have a list of CPCs ordered by tpc count, highest to lowest
    // so next we need to pick a TPC inside the first CPC in this list
    // just pick the first TPC in each CPC
    module_index_list_t tpcs_to_floorsweep;
    for (auto cpc_idx : cpc_idxs){
        for(int tpc_idx : cpc_map.getPEStoTPCMapping(cpc_idx)){
            if (TPCs[tpc_idx]->getEnabled()){
                tpcs_to_floorsweep.push_back(tpc_idx);
            }
        }
    }

    return tpcs_to_floorsweep;
}

//-----------------------------------------------------------------------------
// GPCWithGFX_t
//-----------------------------------------------------------------------------

/**
 * @brief Is this GPC valid? Only considering stuff within this GPC
 * 
 * @param fsmode degree of valid to check for 
 * @param msg if it's not valid. This will be set to a string that explains why
 * @return true if valid
 * @return false if not valid
 */
bool GPCWithGFX_t::isValid(FSmode fsmode, std::string& msg) const {
    if (!getEnabled()) {
        if (getNumEnabled(ROPs) > 0 ) {
            std::stringstream ss;
            ss << "GPC" << instance_number << " cannot be disabled while having enabled ROPs!";
            msg = ss.str();
            return false;
        }
        if (getNumEnabled(PESs) > 0 ) {
            std::stringstream ss;
            ss << "GPC" << instance_number << " cannot be disabled while having enabled PESs!";
            msg = ss.str();
            return false;
        }
    } else {
        PESMapping_t pes_mapping(PESs.size(), TPCs.size());

        // check that all enabled TPCs have an enabled PES
        for(uint32_t tpc_id = 0; tpc_id < TPCs.size(); tpc_id++){
            int pes_id = pes_mapping.getTPCtoPESMapping(tpc_id);
            const TPC_t& tpc = *TPCs[tpc_id];
            const PES_t& pes = *PESs[pes_id];

            if (tpc.getEnabled() && !pes.getEnabled()){
                std::stringstream ss;
                ss << "can't enable physical TPC" << tpc_id << " without conencted PES" << pes_id << " enabled in GPC" << instance_number << "!";
                msg = ss.str();
                return false;
            }
        }

        // check that all disabled PESs have no enabled TPCs
        for(uint32_t pes_id = 0; pes_id < PESs.size(); pes_id++){
            bool at_least_one_tpc_enabled = false;
            const PES_t& pes = *PESs[pes_id];
            for (int tpc_id : pes_mapping.getPEStoTPCMapping(pes_id)){
                const TPC_t& tpc = *TPCs[tpc_id];
                at_least_one_tpc_enabled |= tpc.getEnabled();
                if (tpc.getEnabled() && !pes.getEnabled()){
                    std::stringstream ss;
                    ss << "can't disable physical PES" << pes_id << " while connected TPC" << tpc_id <<  "is enabled in GPC" << instance_number << "!";
                    msg = ss.str();
                    return false;
                }
            }
            
            // Disallow configurations with PES connected to no TPCs
            if (pes.getEnabled() && !at_least_one_tpc_enabled){
                std::stringstream ss;
                ss << "can't enable physical PES" << pes_id << " in physical GPC" << instance_number << " without any connected TPCs enabled!";
                msg = ss.str();
                return false;
            }
        }

        // Make sure there are at least 1 ROPs
        if (getNumEnabled(ROPs) < 1) {
            std::stringstream ss;
            ss << "At least one ROP must be enabled in physical GPC " << instance_number << "!";
            msg = ss.str();
            return false;    
        }
    }

    return GPC_t::isValid(fsmode, msg);
}

/**
 * @brief Apply extra floorsweeping to attempt to make the config valid
 * 
 */
void GPCWithGFX_t::funcDownBin() {
    GPC_t::funcDownBin();

    if (getEnabled()) {
        // if the GPC has no available TPCs, ROPs, or PESs, disable the GPC
        if (getNumEnabled(PESs) == 0 || getNumEnabled(ROPs) == 0){
            setEnabled(false);
        }

        //Disable the TPCs for every disabled PES
        PESMapping_t pes_mapping(PESs.size(), TPCs.size());
        for(uint32_t pes_id = 0; pes_id < PESs.size(); pes_id++){
            bool at_least_one_tpc_enabled = false;
            PES_t* pes = PESs[pes_id].get();
            for (int tpc_id : pes_mapping.getPEStoTPCMapping(pes_id)){
                TPC_t* tpc = TPCs[tpc_id].get();
                at_least_one_tpc_enabled |= tpc->getEnabled();
                if (tpc->getEnabled() && !pes->getEnabled()){
                    tpc->setEnabled(false);
                }
            }

            //Disable the CPC if all the TPCs are disabled
            if (pes->getEnabled() && !at_least_one_tpc_enabled){
                pes->setEnabled(false);
            }
        }
    }

    // if the GPC is disabled, disable all the TPCs, PESs, ROPs
    if (!getEnabled()) {
        for(auto& PES : PESs){
            PES->setEnabled(false);
        }
        for(auto& ROP : ROPs){
            ROP->setEnabled(false);
        }
    }

    // it needs to be called twice, once at the beginning and end
    GPC_t::funcDownBin();
}

/**
 * @brief This function populates its submodule lists, like a constructor, except we can call virtual methods here.
 * 
 * @param settings 
 */
void GPCWithGFX_t::setup(const chipPOR_settings_t& settings) {
    GPC_t::setup(settings);

    PESs.resize(settings.num_pes_per_gpc);
    for (auto& PES : PESs){
        createPES(PES);
    }

    ROPs.resize(settings.num_rop_per_gpc);
    for (auto& ROP : ROPs){
        createROP(ROP);
    }
}

/**
 * @brief Apply the fslib.h masks to this GPC
 * 
 * @param mask defined in fslib.h
 * @param index Which GPC this is this
 */
void GPCWithGFX_t::applyMask(const fslib::GpcMasks& mask, uint32_t index){
    GPC_t::applyMask(mask, index);
    applyMaskToList(PESs, mask.pesPerGpcMasks[index]);
    applyMaskToList(ROPs, mask.ropPerGpcMasks[index]);
}

/**
 * @brief Get the fslib.h masks for this GPC
 * 
 * @param mask defined in fslib.h
 * @param index Which GPC this is
 */
void GPCWithGFX_t::getMask(fslib::GpcMasks& mask, uint32_t index) const {
    GPC_t::getMask(mask, index);
    mask.pesPerGpcMasks[index] = getMaskFromList(PESs);
    mask.ropPerGpcMasks[index] = getMaskFromList(ROPs);
}

static const_base_module_list_t getPESs(const FSModule_t& ref) {return getBaseModuleList(dynamic_cast<const GPCWithGFX_t&>(ref).PESs); }
static const_base_module_list_t getROPs(const FSModule_t& ref) {return getBaseModuleList(dynamic_cast<const GPCWithGFX_t&>(ref).ROPs); }

/**
 * @brief Creates a module_map_t that can be used to access its submodule lists
 * 
 * @return const module_map_t&
 */
const module_map_t& GPCWithGFX_t::getModuleMapRef() const {
    auto makeMap = [this]() -> module_map_t {
        module_map_t module_map = GPC_t::getModuleMapRef();
        module_map[module_type_id::pes] = getPESs;
        module_map[module_type_id::rop] = getROPs;
        return module_map;
    };
    const static module_map_t module_map = makeMap();
    return module_map;
}

/**
 * @brief Return a const ref set of all the submodules of this type
 * 
 * @return const module_set_t& 
 */
const module_set_t& GPCWithGFX_t::getSubModuleTypes() const {
    // const static to avoid construction every time, while also not requiring every instance to maintain its own copy
    const static module_set_t child_set = makeChildModuleSet();
    return child_set;
}

/**
 * @brief Return a list of PES idxs sorted based on enabled TPC count per PES
 * 
 * @return module_index_list_t 
 */
module_index_list_t GPCWithGFX_t::getPESIdxsByTPCCount() const {
    return sortMajorUnitByMinorCount(getBaseModuleList(PESs), getBaseModuleList(TPCs));
}

/**
 * @brief Balance out the TPCs per PES.  Only return first TPC per GPC.
 * 
 * @return module_index_list_t 
 */
module_index_list_t GPCWithGFX_t::getPreferredDisableListTPC(const FUSESKUConfig::SkuConfig& potentialSKU) const {

    module_index_list_t pes_idxs = getPESIdxsByTPCCount();
    const PESMapping_t pes_map(PESs.size(), TPCs.size());

    // now we have a list of PESs ordered by tpc count, highest to lowest
    // so next we need to pick a TPC inside the first PES in this list
    // just pick the first TPC in each PES
    module_index_list_t tpcs_to_floorsweep;
    for (auto pes_idx : pes_idxs){
        for(int tpc_idx : pes_map.getPEStoTPCMapping(pes_idx)){
            if (TPCs[tpc_idx]->getEnabled()){
                tpcs_to_floorsweep.push_back(tpc_idx);
                return tpcs_to_floorsweep;
            }
        }
    }

    return tpcs_to_floorsweep;
}

//-----------------------------------------------------------------------------
// FBP_t
//-----------------------------------------------------------------------------
/**
 * @brief This function populates its submodule lists, like a constructor, except we can call virtual methods here.
 * 
 * @param settings 
 */
void FBP_t::setup(const chipPOR_settings_t& settings) {
    LTCs.resize(settings.num_ltcs / settings.num_fbps);
    for (auto& LTC : LTCs){
        createLTC(LTC);
    }

    L2Slices.resize(slices_per_fbp);
    for (auto& L2Slice : L2Slices){
        createL2Slice(L2Slice);
    }

    FBIOs.resize(settings.num_fbpas / settings.num_fbps);
    for (auto& FBIO : FBIOs){
        createFBIO(FBIO);
    }
    ParentFSModule_t::setup();
}

FBP_t::~FBP_t() {
}

/**
 * @brief Apply the fslib.h masks to this FBP
 * 
 * @param mask defined in fslib.h
 * @param index Which FBP this is this
 */
void FBP_t::applyMask(const fslib::FbpMasks& mask, uint32_t index){
    applyMaskToList(LTCs,     mask.ltcPerFbpMasks[index]);
    applyMaskToList(L2Slices, mask.l2SlicePerFbpMasks[index]);
    applyMaskToList(FBIOs,    mask.fbioPerFbpMasks[index]);
}

/**
 * @brief Get the fslib.h masks for this FBP
 * 
 * @param mask defined in fslib.h
 * @param index Which FBP this is
 */
void FBP_t::getMask(fslib::FbpMasks& mask, uint32_t index) const {
    mask.ltcPerFbpMasks[index] = getMaskFromList(LTCs);
    mask.l2SlicePerFbpMasks[index] = getMaskFromList(L2Slices);
    mask.fbioPerFbpMasks[index] = getMaskFromList(FBIOs);
}

/**
 * @brief Return the L2 slice bitmask for this FBP
 * 
 * The L2 slice bitmask is used to check patterns in PRODUCT modei
 * 
 * @return uint32_t The L2 slice bitmask for a FBP
 */
mask32_t FBP_t::getL2Mask() const {
    mask32_t mask = 0;
    for (uint32_t i = 0; i < L2Slices.size(); i++) {
        mask[i] = !(L2Slices[i]->getEnabled());
    }
    return mask;
};

/**
 * @brief apply an l2 slice disable mask to the FBP
 * 
 * @return uint32_t 
 */
void FBP_t::applyL2Mask(mask32_t mask) {
    for (uint32_t i = 0; i < L2Slices.size(); i++) {
        if (mask[i] == 1){
            L2Slices[i]->setEnabled(false);
        }
    }
    funcDownBin();
};

/**
 * @brief Create a new LTC appropriate for this FBP type
 * 
 * @return std::unique_ptr<LTC_t> 
 */
void FBP_t::createLTC(LTCHandle_t& handle) const {
    makeModule(handle, LTC_t());
}

/**
 * @brief Create a new L2Slice appropriate for this FBP type
 * 
 * @return std::unique_ptr<L2Slice_t> 
 */
void FBP_t::createL2Slice(L2SliceHandle_t& handle) const {
    makeModule(handle, L2Slice_t());
}

/**
 * @brief Create a new FBIO appropriate for this FBP type
 * 
 * @return std::unique_ptr<FBIO_t> 
 */
void FBP_t::createFBIO(FBIOHandle_t& handle) const {
    makeModule(handle, FBIO_t());
}

/**
 * @brief return the number of enabled LTCs within this FBP
 * 
 * @return uint32_t 
 */
uint32_t FBP_t::getNumEnabledLTCs() const {
    return getNumEnabled(LTCs);
}

/**
 * @brief return the number of enabled slices within this FBP
 * 
 * @return uint32_t 
 */
uint32_t FBP_t::getNumEnabledSlices() const {
    return getNumEnabled(L2Slices);
}

/**
 * @brief Get the number of enabled slices for an LTC within this FBP
 * 
 * @param ltc_idx which LTC we want to callwlate num enabled slices for
 * @return uint32_t 
 */
uint32_t FBP_t::getNumEnabledSlicesPerLTC(uint32_t ltc_idx) const{
    uint32_t num_slices_per_ltc = L2Slices.size() / LTCs.size();
    uint32_t begin_idx = ltc_idx * num_slices_per_ltc;
    uint32_t end_idx = begin_idx + num_slices_per_ltc - 1;
    return getNumEnabledRange(L2Slices, begin_idx, end_idx);
}

/**
 * @brief Get the number of disabled slices for an LTC within this FBP
 * 
 * @param ltc_idx which LTC we want to callwlate num disabled slices for
 * @return uint32_t 
 */
uint32_t FBP_t::getNumDisabledSlicesPerLTC(uint32_t ltc_idx) const{
    uint32_t num_slices_per_ltc = L2Slices.size() / LTCs.size();
    return num_slices_per_ltc - getNumEnabledSlicesPerLTC(ltc_idx);
}

/**
 * @brief Are there some slices floorswept in this LTC?
 * 
 * @param ltc_idx 
 * @return true if the LTC is partially floorswept
 * @return false if the LTC has no slices enabled, or all the slices enabled
 */
bool FBP_t::ltcHasPartialSlicesEnabled(uint32_t ltc_idx) const {
    uint32_t num_slices_per_ltc = L2Slices.size() / LTCs.size();
    uint32_t enabled_slice_count = getNumEnabledSlicesPerLTC(ltc_idx);
    return (enabled_slice_count != num_slices_per_ltc) && (enabled_slice_count != 0);
}

/**
 * @brief Is this FBP valid? Only considering stuff within this FBP
 * 
 * @param fsmode degree of valid to check for 
 * @param msg if it's not valid. This will be set to a string that explains why
 * @return true if valid
 * @return false if not valid
 */
bool FBP_t::isValid(FSmode fsmode, std::string& msg) const {
    // Check all LTCs and make sure they are valid
    for (uint32_t i = 0; i < LTCs.size(); i++){
        const LTC_t& LTC = *LTCs[i];
        if (!LTC.isValid(fsmode, msg)){
            return false;
        }

        // make sure each LTC has some slices enabled
        if (!ltcIsValid(fsmode, i, msg)){
            return false;
        }
    }

    // an enabled FBP must have LTCs enabled
    if (getEnabled() && (getNumEnabledLTCs() == 0)){
        std::stringstream ss;
        ss << "can't enable FBP" << instance_number << " with no LTCs enabled!";
        msg = ss.str();
        return false;
    }

    // a disabled FBP cannot have LTCs enabled
    if (!getEnabled() && (getNumEnabledLTCs() != 0)){
        std::stringstream ss;
        ss << "can't disable FBP" << instance_number << " with LTCs enabled!";
        msg = ss.str();
        return false;
    }

    // an enabled FBP must have slices enabled
    if (getEnabled() && (getNumEnabledSlices() == 0)){
        std::stringstream ss;
        ss << "can't enable FBP" << instance_number << " with no L2Slices enabled!";
        msg = ss.str();
        return false;
    }

    // a disabled FBP cannot have slices enabled
    if (!getEnabled() && (getNumEnabledSlices() != 0)){
        std::stringstream ss;
        ss << "can't disable FBP" << instance_number << " with L2Slices enabled!";
        msg = ss.str();
        return false;
    }

    return true;
}

/**
 * @brief Check if the LTC at ltc_idx is valid with respect to slices or other parts of this FBP
 * 
 * @param fsmode check for func valid or product valid
 * @param ltc_idx which LTC within the FBP to check
 * @param msg reason for why its not valid
 * @return true if valid
 * @return false if not valid
 */
bool FBP_t::ltcIsValid(FSmode fsmode, uint32_t ltc_idx, std::string& msg) const {
    // make sure each LTC has some slices enabled
    if (LTCs[ltc_idx]->getEnabled()){
        if (getNumEnabledSlicesPerLTC(ltc_idx) == 0){
            std::stringstream ss;
            ss << "can't enable LTC" << ltc_idx << " in FBP" << instance_number << " with no L2Slices enabled!";
            msg = ss.str();
            return false;
        }
    } else { // make sure each disabled LTC has no slices enabled
        if (getNumEnabledSlicesPerLTC(ltc_idx) != 0){
            std::stringstream ss;
            ss << "can't disable LTC" << ltc_idx << " in FBP" << instance_number << " with L2Slices enabled!";
            msg = ss.str();
            return false;
        }
    }
    return true;
}

/**
 * @brief Return the module type
 * 
 * @return module_type_id 
 */
module_type_id FBP_t::getTypeEnum() const {
    return module_type_id::fbp;
}

/**
 * @brief FBP quality is based on number of l2slices
 * 
 * @return std::vector<module_type_id> 
 */
const std::vector<module_type_id>& FBP_t::getSubModuleTypeDownBinPriority() const {
    static const std::vector<module_type_id> downbin_priority{module_type_id::l2slice};
    return downbin_priority;
}

static const_base_module_list_t getLTCs(const FSModule_t& ref) {return getBaseModuleList(static_cast<const FBP_t&>(ref).LTCs); }
static const_base_module_list_t getL2Slices(const FSModule_t& ref) {return getBaseModuleList(static_cast<const FBP_t&>(ref).L2Slices); }
static const_base_module_list_t getFBIOs(const FSModule_t& ref) {return getBaseModuleList(static_cast<const FBP_t&>(ref).FBIOs); }

/**
 * @brief Creates a module_map_t that can be used to access its submodule lists
 * 
 * @return const module_map_t&
 */
const module_map_t& FBP_t::getModuleMapRef() const {
    auto makeMap = [this]() -> module_map_t {
        module_map_t module_map;
        module_map[module_type_id::ltc] = getLTCs;
        module_map[module_type_id::l2slice] = getL2Slices;
        module_map[module_type_id::fbio] = getFBIOs;
        return module_map;
    };
    const static module_map_t module_map = makeMap();
    return module_map;
}

/**
 * @brief Return a const ref set of all the submodules of this type
 * 
 * @return const module_set_t& 
 */
const module_set_t& FBP_t::getSubModuleTypes() const {
    // const static to avoid construction every time, while also not requiring every instance to maintain its own copy
    const static module_set_t child_set = makeChildModuleSet();
    return child_set;
}

/**
 * @brief Apply extra floorsweeping to attempt to make the config valid
 * 
 */
void FBP_t::funcDownBin() {
    // Disable the FBP if all slices are disabled
    // Disable the FBP if all LTCs are disabled
    // Disable the FBP if all FBIOs are disabled
    if (getEnabled()) {
        if (getNumEnabled(LTCs) == 0 || getNumEnabled(L2Slices) == 0 || getNumEnabled(FBIOs) == 0){
            setEnabled(false);
        }

        // LTC/Slice interaction
        for (uint32_t i = 0; i < LTCs.size(); i++){
            // if the LTC is invalid for any other reason
            std::string msg;
            if (!ltcIsValid(FSmode::FUNC_VALID, i, msg)){
                LTCs[i]->setEnabled(false);
            }

            // If the LTC is diabled and slices are enabled, disable the slices
            if (getNumEnabledSlicesPerLTC(i) != 0 && !LTCs[i]->getEnabled()) {
                uint32_t num_slices_per_ltc = L2Slices.size() / LTCs.size();
                uint32_t begin_idx = i * num_slices_per_ltc;
                uint32_t end_idx = begin_idx + num_slices_per_ltc - 1;
                for(uint32_t slice_idx = begin_idx; slice_idx <= end_idx; slice_idx+=1){
                    L2Slices[slice_idx]->setEnabled(false);
                }
            }
        }
    }

    if (getNumEnabled(LTCs) == 0 || getNumEnabled(L2Slices) == 0 || getNumEnabled(FBIOs) == 0){
        setEnabled(false);
    }

    // Disable the slices, LTCs, FBIOs if the FBP is disabled
    if (!getEnabled()) {
        for (auto& LTC : LTCs){
            LTC->setEnabled(false);
        }
        for (auto& L2Slice : L2Slices){
            L2Slice->setEnabled(false);
        }
        for (auto& FBIO : FBIOs){
            FBIO->setEnabled(false);
        }
    }
}

//-----------------------------------------------------------------------------
// DDRFBP_t
//-----------------------------------------------------------------------------

/**
 * @brief Constructor for FBP connected to DDR memory
 * 
 * @param settings A struct containing the number of floorsweepable componenets
 */
void DDRFBP_t::setup(const chipPOR_settings_t& settings) {
    HalfFBPAs.resize(2 * settings.num_fbpas / settings.num_fbps);
    for (auto& HalfFBPA : HalfFBPAs){
        createHalfFBPA(HalfFBPA);
    }
    FBP_t::setup(settings);
}

/**
 * @brief Applies fslib mask to FBP
 * 
 * @param mask[in] Struct containing masks for the floorsweepable regions of the FBP
 * @param index[in] The physical index of this FBP
 */
void DDRFBP_t::applyMask(const fslib::FbpMasks& mask, uint32_t index){
    applyMaskToList(HalfFBPAs, mask.halfFbpaPerFbpMasks[index]);
    FBP_t::applyMask(mask, index);
}

/**
 * @brief Get the fslib.h masks for this FBP
 * 
 * @param mask defined in fslib.h
 * @param index Which FBP this is
 */
void DDRFBP_t::getMask(fslib::FbpMasks& mask, uint32_t index) const {
    FBP_t::getMask(mask, index);
    mask.halfFbpaPerFbpMasks[index] = getMaskFromList(HalfFBPAs);    
}

/**
 * @brief Creates Half FBPA instance for a DDR FBP
 * 
 * @return std::unique_ptr<HalfFBPA_t> Instance of Half FBPA
 */
void DDRFBP_t::createHalfFBPA(HalfFBPAHandle_t& handle) const {
    makeModule(handle, HalfFBPA_t());
}

/**
 * @brief Returns the number of enabled Half FBPAs
 * 
 * @return int The number of enabled Half FBPAs
 */
uint32_t DDRFBP_t::getNumEnabledHalfFBPAs() const {
    return getNumEnabled(HalfFBPAs);
}

/**
 * @brief Checks validity of FBP connected to DDR memory
 *
 * - Checks that FBIOs are floorswept if FBP is floorswept
 * 
 * @param[in] fsmode The mode to use: PRODUCT, FUNC_VALID, IGNORE_FSLIB
 * @param[out] msg An error message describing why a configuration is invalid
 * @return true This floorsweeping configuration is valid for a generic FBP connected to DDR memory
 * @return false This floorsweeping configuration is invalid for a generic FBP connected to DDR memory
 */
bool DDRFBP_t::isValid(FSmode fsmode, std::string& msg) const {
    // FBIOs must be floorswept if the FBP is floorswept, and both must be enabled if the FBP is enabled
    if (getEnabled()){
        if (getNumEnabled(FBIOs) != FBIOs.size()) {
            std::stringstream ss;
            ss << "FBIOs in FBP" << instance_number << " cannot be floorswept while the FBP is enabled!";
            msg = ss.str();
            return false;
        }
    } else {
        if (getNumEnabled(FBIOs) != 0) {
            std::stringstream ss;
            ss << "FBIOs in FBP" << instance_number << " cannot be enabled if the FBP is floorswept!";
            msg = ss.str();
            return false;
        }
    }

    return FBP_t::isValid(fsmode, msg);
}

/**
 * @brief Checks slice configuration in DDR-based FBP
 * 
 * All LTCs must have the same number of L2 slices.  If mode is PRODUCT, slice masks
 * are restricted to a subset of possible configurations.
 * In PRODUCT mode expected_l2_mask is used as an input and an output - it will contain the expected mask
 * for the next FBP if given a valid PRODUCT mask.
 * 
 * @param[in] fsmode The mode to use: PRODUCT, FUNC_VALID, IGNORE_FSLIB
 * @param[out] msg An error message describing why a configuration is invalid
 * @param[in,out] expected_l2_mask Input the expected L2 mask for PRODUCT mode. If PRODUCT mode this is changed to the next expected mask.
 * @return true This slice configuration is valid for a generic DDR-based FBP.
 * @return false This slice configuration is invalid for a generic DDR-based FBP.
 */
bool DDRFBP_t::isValidSlice(FSmode fsmode, std::string& msg, mask32_t &expected_l2_mask) const {
    // FBIOs must be floorswept if the FBP is floorswept, and both must be enabled if the FBP is enabled
    if (getEnabled()) {
        bool first_ltc = true;
        uint32_t i = 0;
        // Handle case where no L2 slices are floorswept
        if (getNumEnabledSlices() == L2Slices.size()) {
            return true;
        }
        int num_slices = -1;
        switch(fsmode) {
            case FSmode::PRODUCT:
                if (getL2Mask() == expected_l2_mask) {
                    // If this is a valid mask, the number of slices per LTC must be equal
                    if (getNumEnabledLTCs() != 0) {
                        // nextL2ProdMask returns the validity of the current mask for PRODUCT mode
                        return nextL2ProdMask(msg, expected_l2_mask, expected_l2_mask);
                    } else {
                        std::stringstream ss;
                        ss << "No LTCs enabled in FBP" << instance_number << ", but L2 slices are enabled!";
                        msg = ss.str();
                        return false;                    
                    }
                } else {
                    for (i = 0; i < LTCs.size(); i++) {
                        const LTC_t& LTC = *LTCs[i];
                        if (LTC.getEnabled()) {
                            if (getNumEnabledSlicesPerLTC(i) != (L2Slices.size()/LTCs.size())){
                                std::stringstream ss;
                                ss << "FBP" << instance_number << " LTC" << i << " has floorswept slices that are not in the expected pattern!";
                                ss << " the expected mask is " << expected_l2_mask << ", but the actual mask was " << getL2Mask();
                                msg = ss.str();
                                expected_l2_mask = 0xffffffff;
                                return false;
                            }
                        }
                    }
                } break;
            case FSmode::FUNC_VALID:
                for (i = 0; i < LTCs.size(); i++) {
                    const LTC_t& LTC = *LTCs[i];
                    if (LTC.getEnabled()) {
                        int slices_enabled = getNumEnabledSlicesPerLTC(i);
                        if (slices_enabled == 1) {
                            std::stringstream ss;
                            ss << "LTC " << i << " has 1 slice enabled!";
                            msg = ss.str();
                            return false;
                        }
                        if (first_ltc) {
                            num_slices = slices_enabled;
                            first_ltc = false;
                        } else if (num_slices != slices_enabled) {
                            std::stringstream ss;
                            ss << "The number of L2 slices are not the same across LTCs! LTC " 
                            << i << " has " << slices_enabled << " slices, but other LTCs have " 
                            << num_slices <<"!";
                            msg = ss.str();
                            return false;
                        }
                    }
                }
                return true;
            case FSmode::IGNORE_FSLIB:
                // Provide a negative value since the number of l2 slices per LTC may not be equal
                return true;
            default:
                std::stringstream ss;
                ss << "Invalid fsmode! Options are PRODUCT, FUNC_VALID, and IGNORE_FSLIB!";
                msg = ss.str();
                return false;

        }
    } else {
        // isValid for DDRFPB checks that all l2 slices are disabled for a disabled FBP.  This configuration is valid.
        return true;
    }
    return true;
}

/**
 * @brief Get the number of L2 slices per LTC (assumes `isValidSlice` returned true)
 * 
 * @return int The number of L2 slices per LTC in this FBP
 */
int DDRFBP_t::numSlicesPerLTC() const {
    for (uint32_t i = 0; i < LTCs.size(); i++) {
        if(LTCs[i]->getEnabled()) {
            return getNumEnabledSlicesPerLTC(i);
        }
    }
    return 0;
}

/**
 * @brief Return a map for determining the next l2 mask in the pattern
 * 
 * @return const std::map<mask32_t, mask32_t, MaskComparer>& 
 */
const std::map<mask32_t, mask32_t, MaskComparer>& DDRFBP_t::getSlicePatternMap() const {
    const static std::map<mask32_t, mask32_t, MaskComparer> product_l2_pattern {
        {0x81, 0x42}, {0x42, 0x24},
        {0x24, 0x18}, {0x18, 0x81},
        {0xc3, 0xc3}, {0x3c, 0x3c}
    };
    return product_l2_pattern;
}

/**
 * @brief The next valid per-FBP L2 mask
 *
 * Provides the next per-FBP L2 mask in `next_mask`, returns the validity of the input `l2_mask`
 * `next_mask` is undefined if `l2_mask` is invalid.
 * 
 * @param[in] l2_mask The current L2 mask for a FBP
 * @param[out] next_mask The mask the next FBP must have to be PRODUCT valid
 * @return true The input `l2_mask` is valid
 * @return false The input `l2_mask` is not PRODUCT valid
 */
bool DDRFBP_t::nextL2ProdMask(std::string& msg, mask32_t l2_mask, mask32_t &next_mask) const { 
    const auto& map_it = getSlicePatternMap().find(l2_mask);
    if (map_it != getSlicePatternMap().end()) {
        next_mask = map_it->second;
        return true;
    } else {
        if (getNumEnabledLTCs() < LTCs.size()) {
            mask32_t valid_mask = 0;
            for (uint32_t ltc = 0; ltc < LTCs.size(); ltc++) {
                valid_mask |= LTCs[ltc]->getEnabled() ? 0 : 0xf << (4*ltc);
            }
            if (l2_mask == valid_mask) {
                next_mask = 0;
                return true;
            }
            std::stringstream ss;
            ss << "FBP" << instance_number << " has an LTC floorswept, but L2 mask " << l2_mask << " has slices floorswept!";
            msg = ss.str();
            return false;
        }
        std::stringstream ss;
        ss << "L2 mask 0x" << std::hex << l2_mask.to_ulong() << " is not a PRODUCT valid mask!";
        msg = ss.str();
        return false;
    }
}

mask32_t DDRFBP_t::getSliceStartMask(uint32_t num_slices_per_ltc) const {
    for (const auto& map_it : getSlicePatternMap()){
        uint32_t num_slices_disabled_per_fbp = L2Slices.size() - num_slices_per_ltc*LTCs.size();
        if (map_it.first.count() == num_slices_disabled_per_fbp){
            return map_it.first;
        }
    }
    throw FSLibException("no 3slice patterns found!");
    return 0;
}

bool DDRFBP_t::canDo3SliceFS(FSmode fsmode, std::string& msg) const {

    // if only one slice is dead, don't bother doing any more checks
    if (getNumEnabledSlices() >= L2Slices.size() - 1){
        return true;
    }

    // if more than 1 slice per LTC is dead, then we can't do it
    for (uint32_t ltc_idx = 0; ltc_idx < LTCs.size(); ltc_idx++){
        if (getNumEnabledSlicesPerLTC(ltc_idx) < 3){
            std::stringstream ss;
            ss << "Can't do 3 slice mode on FBP " << instance_number << "! it only has ";
            ss << getNumEnabledSlicesPerLTC(ltc_idx) << " slices enabled in LTC " << ltc_idx;
            msg = ss.str();
            return false;
        }
    }

    if (fsmode == FSmode::PRODUCT){
        // if 2 slices are disabled, then check if they are in the allowed mask list
        const auto& pattern_map_it = getSlicePatternMap().find(getL2Mask());
        if (pattern_map_it == getSlicePatternMap().end()) {
            std::stringstream ss;
            ss << "Can't do 3 slice mode on FBP " << instance_number << "! it's slice mask is not a valid pattern!";
            msg = ss.str();
            return false;
        } else {

        }
    }

    return true;
}

/**
 * @brief Downbin the memory system to make it valid
 * 
 */
void DDRFBP_t::funcDownBin() {
    // call parent class method
    FBP_t::funcDownBin();

    // kill ltcs that have invalid slice configurations
    for(uint32_t ltc_idx = 0; ltc_idx < LTCs.size(); ltc_idx++){
        if (getNumEnabledSlicesPerLTC(ltc_idx) < 2){
            LTCs[ltc_idx]->setEnabled(false);
        }
    }

    // call funcDownBin again to disable any leftover sliees
    FBP_t::funcDownBin();

    // a disabled fbp cannot be in halffbpa mode
    if (!getEnabled()){
        HalfFBPAs[0]->setEnabled(false);
    }
}

/**
 * @brief Prefer to floorsweep the LTC with the least enabled L2Slices
 * 
 * @return module_index_list_t 
 */
module_index_list_t DDRFBP_t::getPreferredDisableListLTC() const {
    return sortMajorUnitByMinorCount(getBaseModuleList(LTCs), getBaseModuleList(L2Slices));
}

/**
 * @brief Specific disable preferences for DDR FBP components
 * 
 * @param module_type 
 * @param potentialSKU 
 * @return module_index_list_t 
 */
module_index_list_t DDRFBP_t::getPreferredDisableList(module_type_id module_type, const FUSESKUConfig::SkuConfig& potentialSKU) const {
    switch(module_type){
        case module_type_id::ltc: return getPreferredDisableListLTC();
        default: {
            break;
        }
    }
    return FBP_t::getPreferredDisableList(module_type, potentialSKU);
}

/**
 * @brief Is the LTC valid?
 * 
 * @param fsmode 
 * @param ltc_idx 
 * @param msg 
 * @return true 
 * @return false 
 */
bool DDRFBP_t::ltcIsValid(FSmode fsmode, uint32_t ltc_idx, std::string& msg) const {
    if (!FBP_t::ltcIsValid(fsmode, ltc_idx, msg)){
        return false;
    }

    if (LTCs[ltc_idx]->getEnabled() && getNumEnabledSlicesPerLTC(ltc_idx) < 2){
        std::stringstream ss;
        ss << "FBP" << instance_number << " LTC" << ltc_idx << " is not valid because it has";
        ss << getNumEnabledSlicesPerLTC(ltc_idx) << " slices enabled";
        msg = ss.str();
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
// SKU definition checking
//-----------------------------------------------------------------------------

/**
 * @brief Check if this SKU is actually possible, regardless of what chip it is being applied to
 * 
 * @param msg An explanation for why it's not possible
 * @return true if possible
 * @return false if not possible
 */
bool FUSESKUConfig::SkuConfig::skuIsPossible(std::string& msg) const {

    for (const auto& fsconfig_it : fsSettings) {
        const std::string& module_type = fsconfig_it.first;
        const FUSESKUConfig::FsConfig& module_fsconfig = fsconfig_it.second;

        // min > max check
        if (module_fsconfig.minEnableCount > module_fsconfig.maxEnableCount) {
            std::stringstream ss;
            ss << "The SKU wants at least " << module_fsconfig.minEnableCount << " " << module_type << "s, but at most ";
            ss << module_fsconfig.maxEnableCount << " " << module_type << "s. Not possible!";
            msg = ss.str();
            return false;
        }

        // mustEnableList cannot overlap with mustDisableList
        auto must_enable_copy = module_fsconfig.mustEnableList;
        auto must_disable_copy = module_fsconfig.mustDisableList;

        std::sort(must_enable_copy.begin(), must_enable_copy.end());
        std::sort(must_disable_copy.begin(), must_disable_copy.end());

        module_index_list_t intersection;
        std::set_intersection(must_enable_copy.begin(), must_enable_copy.end(),
                              must_disable_copy.begin(), must_disable_copy.end(),
                              std::back_inserter(intersection));
        if (intersection.size() != 0) {
            std::stringstream ss;
            ss << "The SKU has " << module_type << intersection[0] << " in both the mustEnableList and mustDisableList!";
            msg = ss.str();
            return false;
        }

        // minEnablePerGroup cannot be more than maxEnableCount
        if (module_fsconfig.minEnablePerGroup != UNKNOWN_ENABLE_COUNT || module_fsconfig.enableCountPerGroup != UNKNOWN_ENABLE_COUNT){
            uint32_t enable_count_per_group = module_fsconfig.minEnablePerGroup != UNKNOWN_ENABLE_COUNT ? module_fsconfig.minEnablePerGroup : module_fsconfig.enableCountPerGroup;
            if (enable_count_per_group > module_fsconfig.maxEnableCount) {
                std::stringstream ss;
                ss << "The SKU wants " << enable_count_per_group << " " << module_type << "s per group, but it also wants max ";
                ss << module_fsconfig.maxEnableCount << " " << module_type << "s!";
                msg = ss.str();
                return false;
            }
        }
    }
    return true;
}

/**
 * @brief If both minEnablePerGroup and enableCountPerGroup are not defined, then return 1
 * if minEnablePerGroup is not defined, then use enableCountPerGroup.
 * If enableCountPerGroup is not defined, then return minEnablePerGroup
 * Both are not allowed to be defined
 * 
 * @return uint32_t 
 */
uint32_t FUSESKUConfig::FsConfig::getMinGroupCount() const {
    if (minEnablePerGroup == UNKNOWN_ENABLE_COUNT && enableCountPerGroup == UNKNOWN_ENABLE_COUNT) {
        return 1;
    }
    if (minEnablePerGroup == UNKNOWN_ENABLE_COUNT) {
        return enableCountPerGroup;
    }
    if (enableCountPerGroup == UNKNOWN_ENABLE_COUNT) {
        return minEnablePerGroup;
    }
    throw FSLibException("both minEnablePerGroup and enableCountPerGroup cannot be defined!");
    return 1;
}


//-----------------------------------------------------------------------------
// FSChip_t
//-----------------------------------------------------------------------------

/**
 * @brief instantiate a new GPC
 * 
 * @return std::unique_ptr<GPC_t> 
 */
void FSChip_t::createGPC(GPCHandle_t& gpc_handle) const {
    getGPCType(gpc_handle);
    gpc_handle->setup(getChipPORSettings());
}

/**
 * @brief Instantiate a new FBP
 * 
 * @return std::unique_ptr<FBP_t> 
 */
void FSChip_t::createFBP(FBPHandle_t& fbp_handle) const {
    getFBPType(fbp_handle);
    fbp_handle->setup(getChipPORSettings());
}

/**
 * @brief get the enum representing this chip type
 * 
 * @return Chip 
 */
Chip FSChip_t::getChipEnum() const {
    return getChipSettings().chip_type;
}

/**
 * @brief Return the chip settings that originate in the fmodel build
 * 
 * @return const chipPOR_settings_t& 
 */
const chipPOR_settings_t& FSChip_t::getChipPORSettings() const {
    return getChipSettings().chip_por_settings;
}

/**
 * @brief Is this chip functionally valid?
 * 
 * @param fsmode degree of valid to check for 
 * @param msg if it's not valid. This will be set to a string that explains why
 * @return true if valid
 * @return false if not valid
 */
bool FSChip_t::isValid(FSmode fsmode, std::string& msg) const {
    // call isValid on all subunits
    for (const auto& FBP : FBPs){
        if (!FBP->isValid(fsmode, msg)){
            return false;
        }
    }

    for (const auto& GPC : GPCs){
        if (!GPC->isValid(fsmode, msg)){
            return false;
        }
    }

    // Make sure there are enabled subunits
    if (getNumEnabled(FBPs) == 0) {
        std::stringstream ss;
        ss << "All FBPs are floorswept!";
        msg = ss.str();
        return false; 
    }
    if (getNumEnabled(GPCs) == 0) {
        std::stringstream ss;
        ss << "All GPCs are floorswept!";
        msg = ss.str();
        return false; 
    }

    if (!enableCountsAreValid(fsmode, msg)){
        return false;
    }

    // then check top level config
    return true;
}

/**
 * @brief Don't call this on chip modules
 * 
 * @return std::vector<module_type_id> 
 */
const std::vector<module_type_id>& FSChip_t::getSubModuleTypeDownBinPriority() const {
    throw FSLibException("cannot call this on chip modules!");
    static const std::vector<module_type_id> downbin_priority{};
    return downbin_priority;
}

/**
 * @brief Apply the fslib.h masks to all the GPCs in this chip
 * 
 * @param mask 
 */
void FSChip_t::applyGPCMasks(const fslib::GpcMasks& mask){
    applyMaskToList(GPCs, mask.gpcMask);

    for (uint32_t i = 0; i < GPCs.size(); i++){
        GPCs[i]->applyMask(mask, i);
    }
}

/**
 * @brief Apply the fslib.h masks to all the FBPs in this chip
 * 
 * @param mask 
 */
void FSChip_t::applyFBPMasks(const fslib::FbpMasks& mask){
    applyMaskToList(FBPs, mask.fbpMask);

    for (uint32_t i = 0; i < FBPs.size(); i++){
        FBPs[i]->applyMask(mask, i);
    }
}

/**
 * @brief Get the fslib.h GpcMasks masks for this chip config
 * 
 * @param mask 
 */
void FSChip_t::getGPCMasks(fslib::GpcMasks& mask) const {
    mask.gpcMask = getMaskFromList(GPCs);

    for (uint32_t i = 0; i < GPCs.size(); i++){
        GPCs[i]->getMask(mask, i);
    }
}

/**
 * @brief Get the fslib.h FbpMasks masks for this chip config
 * 
 * @param mask 
 */
void FSChip_t::getFBPMasks(fslib::FbpMasks& mask) const {
    mask.fbpMask = getMaskFromList(FBPs);

    for (uint32_t i = 0; i < FBPs.size(); i++){
        FBPs[i]->getMask(mask, i);
    }
}

/**
 * @brief Get the number of enabled slices for the entire chip
 * 
 * @return uint32_t 
 */
uint32_t FSChip_t::getNumEnabledSlices() const {
    uint32_t slices = 0;
    for (const auto& FBP : FBPs){
        slices += FBP->getNumEnabledSlices();
    }
    return slices;
}

/**
 * @brief Get the number of enabled LTCs for the entire chip
 * 
 * @return uint32_t 
 */
uint32_t FSChip_t::getNumEnabledLTCs() const {
    uint32_t slices = 0;
    for (const auto& FBP : FBPs){
        slices += FBP->getNumEnabledLTCs();
    }
    return slices;
}

/**
 * @brief Get a logical "skyline" for the current tpc config
 * 
 * @return std::vector<uint32_t> 
 */
std::vector<uint32_t> FSChip_t::getLogicalTPCCounts() const {
    std::vector<uint32_t> aliveTPCs;
    aliveTPCs.reserve(GPCs.size());
    for (const auto& gpc_ptr : GPCs) {
        uint32_t num_tpcs = getNumEnabled(gpc_ptr->TPCs);
        if (!gpc_ptr->getEnabled()) {
            num_tpcs = 0;
        }
        aliveTPCs.push_back(num_tpcs);
    }
    std::sort(aliveTPCs.begin(), aliveTPCs.end());
    return aliveTPCs;
}

/**
 * @brief This function populates its submodule lists, like a constructor, except we can call virtual methods here.
 * 
 */
void FSChip_t::instanceSubModules() {
    GPCs.resize(getChipPORSettings().num_gpcs);
    for (uint32_t i = 0; i < GPCs.size(); i++){
        createGPC(GPCs[i]);
    }

    FBPs.resize(getChipPORSettings().num_fbps);
    for (uint32_t i = 0; i < FBPs.size(); i++){
        createFBP(FBPs[i]);
    }
}

/**
 * @brief This function populates its submodule lists, like a constructor, except we can call virtual methods here.
 * 
 */
void FSChip_t::setup() {
    instanceSubModules();
    ParentFSModule_t::setup();
}

/**
 * @brief Get the enable count for all modules of this type in the entire chip
 * 
 * @param type_id 
 * @return uint32_t 
 */
uint32_t FSChip_t::getTotalEnableCount(module_type_id type_id) const {
    return getNumEnabled(getFlatModuleList(type_id));
}

/**
 * @brief Return a pointer to a cloned copy of this chip
 * 
 * @return std::unique_ptr<FSChip_t> 
 */
std::unique_ptr<FSChip_t> FSChip_t::clone() const {
    std::unique_ptr<FSChip_t> fresh_chip = createChip(getChipEnum());
    *fresh_chip = *this;
    return fresh_chip;
}

void FSChip_t::cloneContainer(FSChipContainer_t& chip_container) const {
    createChipContainer(getChipEnum(), chip_container);
    *chip_container = *this;
}

/**
 * @brief Do a deep copy to copy all the FS settings from one chip instance to the other
 * 
 * @param other copy from this instance
 * @return FSChip_t& a reference to this
 */
FSChip_t& FSChip_t::operator=(const FSChip_t& other){
    ParentFSModule_t::operator = (other);
    spare_gpc_mask = other.spare_gpc_mask;
    return *this;
}

/**
 * @brief Check if two chip instances are equal. It iterates over all submodules
 * 
 * @param other Check if equal to this instance
 * @return true if equal
 * @return false if not equal
 */
bool FSChip_t::operator==(const FSChip_t& other) const {
    if (this->getChipEnum() != other.getChipEnum()){
        return false;
    }

    if (!(ParentFSModule_t::operator == (other))){
        return false;
    }

    if (!(spare_gpc_mask == other.spare_gpc_mask)) {
        return false;
    }

    return true;
}

/**
 * @brief Check if two chip instances are not equal. It is just the ilwerse of ==
 * 
 * @param other 
 * @return true if not equal
 * @return false if equal
 */
bool FSChip_t::operator!=(const FSChip_t& other) const {
    return ! (*this == other);
}

/**
 * @brief Return the module type
 * 
 * @return module_type_id 
 */
module_type_id FSChip_t::getTypeEnum() const {
    return module_type_id::chip;
}

std::string FSChip_t::getString() const {
    std::stringstream ss;
    GpcMasks gpcSettings = { 0 };
    FbpMasks fbpSettings = { 0 };

    getGPCMasks(gpcSettings);
    getFBPMasks(fbpSettings);

    ss << gpcSettings << " " << fbpSettings;
    return ss.str();
}

/**
 * @brief The chip is always enabled.
 * 
 * @return true 
 * @return false 
 */
bool FSChip_t::getEnabled() const {
    return true;
}

/**
 * @brief Don't do anything, chips can't be disabled.
 * This is here only to make other relwrsive functions cleaner.
 * 
 * @param enabled 
 */
void FSChip_t::setEnabled(bool enabled) {
}


static const_base_module_list_t getGPCs(const FSModule_t& ref) {return getBaseModuleList(dynamic_cast<const FSChip_t&>(ref).GPCs); }
static const_base_module_list_t getFBPs(const FSModule_t& ref) {return getBaseModuleList(dynamic_cast<const FSChip_t&>(ref).FBPs); }

/**
 * @brief 
 * 
 * @return module_map_t 
 */
const module_map_t& FSChip_t::getModuleMapRef() const {
    auto makeMap = [this]() -> module_map_t {
        module_map_t module_map;
        module_map[module_type_id::gpc] = getGPCs;
        module_map[module_type_id::fbp] = getFBPs;
        return module_map;
    };
    const static module_map_t module_map = makeMap();
    return module_map;
}

/**
 * @brief Return a const ref set of all the submodules of this type
 * 
 * @return const module_set_t& 
 */
const module_set_t& FSChip_t::getSubModuleTypes() const {
    // const static to avoid construction every time, while also not requiring every instance to maintain its own copy
    const static module_set_t child_set = makeChildModuleSet();
    return child_set;
}

/**
 * @brief This function checks a SkU definition object and a list of corresponding modules, and returns false if they do not comply with the SKU definition 
 * 
 * @param type_id The type of module being checked
 * @param skuconfig The SKU definition
 * @param modules flat list of modules to check against the definition
 * @param msg Explanation for why the SKU doesn't match the list of modules
 * @return true if the list complies with the SKU
 * @return false if the list does not comply with the SKU
 */
static bool listIsInSKU(const FUSESKUConfig::FsConfig& skuconfig, const const_flat_module_list_t& modules, std::string& msg){

    module_type_id type_id = modules[0]->getTypeEnum();
    // check min units
    if (getNumEnabled(modules) < skuconfig.minEnableCount) {
        std::stringstream ss;
        ss << "The SKU requires minimum " << skuconfig.minEnableCount << " " << type_id << "s but " << getNumEnabled(modules) << " are enabled!";
        msg = ss.str();
        return false;
    }
    // check max units
    if (getNumEnabled(modules) > skuconfig.maxEnableCount) {
        std::stringstream ss;
        ss << "The SKU requires maximum " << skuconfig.minEnableCount << " " << type_id << "s but " << getNumEnabled(modules) << " are enabled!";
        msg = ss.str();
        return false;
    }

    // check the mustEnableList
    for(auto instance_id : skuconfig.mustEnableList) {
        if (!modules[instance_id]->getEnabled()) {
            std::stringstream ss;
            ss << "The SKU requires instance " << type_id << instance_id << " to be enabled, but it is not enabled!";
            msg = ss.str();
            return false;
        }
    }

    // check the mustDisableList
    for(auto instance_id : skuconfig.mustDisableList) {
        if (modules[instance_id]->getEnabled()) {
            std::stringstream ss;
            ss << "The SKU requires instance " << type_id << instance_id << " to be disabled, but it is enabled!";
            msg = ss.str();
            return false;
        }
    }

    return true;
}

/**
 * @brief check if this chip matches this sku definition
 * 
 * @param potentialSKU The SKU definition
 * @param msg Explanation for why the SKU doesn't match the chip instance
 * @return true if the chip instance complies with the SKU definition
 * @return false if the chip instance does not comply with the SKU definition
 */
bool FSChip_t::isInSKU(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {

    for (const auto& fsconfig_it : potentialSKU.fsSettings) {
        module_type_id module_id = strToId(fsconfig_it.first);
        const FUSESKUConfig::FsConfig& skuconfig = fsconfig_it.second;

        const_flat_module_list_t module_vector = getFlatModuleList(module_id);
        if (!listIsInSKU(skuconfig, module_vector, msg)){
            return false;
        }
    }

    for (module_type_id module_id : findAllSubUnitTypes()){
        const_flat_module_list_t flat_module_list = getFlatModuleList(module_id);
        for (const FSModule_t* module : flat_module_list){
            if (module->getEnabled()){
                if (!module->moduleIsInSKU(potentialSKU, msg)){
                    return false;
                }
            }
        }
    }
    return true;
}


/**
 * @brief Check if the flat list of modules can be floorswept further to fit the SKU definition
 * 
 * @param skuconfig The SKU definition for this module type
 * @param modules A flat list of modules
 * @param msg This reference will be set to an explanation for why the chip instance cannot fit the SKu
 * @return true if the modules could be further floorswept to match the SKU
 * @return false if the modules can not be further floorswept to match the SKU
 */
static bool listCanFitSKU(const FUSESKUConfig::FsConfig& skuconfig, const const_flat_module_list_t& modules, std::string& msg){

    module_type_id type_id = modules[0]->getTypeEnum();
    // check min units
    if (getNumEnabled(modules) < skuconfig.minEnableCount) {
        std::stringstream ss;
        ss << "This chip cannot fit the SKU because the SKU requires " << skuconfig.minEnableCount << " " << type_id << "s, but " << getNumEnabled(modules) << " are enabled!";
        msg = ss.str();
        return false;
    }

    // check the mustEnableList
    for(auto instance_id : skuconfig.mustEnableList) {
        if (!modules[instance_id]->getEnabled()) {
            std::stringstream ss;
            ss << "This chip cannot fit the SKU because the SKU requires instance " << type_id << instance_id << " to be enabled, but it is not enabled!";
            msg = ss.str();
            return false;
        }
    }

    // check the mustDisableList
    uint32_t must_disable_additions = 0;
    for(auto instance_id : skuconfig.mustDisableList) {
        if (modules[instance_id]->getEnabled()) {
            must_disable_additions += 1;
        }
    }

    if (getNumEnabled(modules) - must_disable_additions < skuconfig.minEnableCount) {
        std::stringstream ss;
        ss << "This chip cannot fit the SKU because there are not enough good " << type_id << "s to meet the minimum requirement of " << skuconfig.minEnableCount;
        ss << " after disabling those in the mustDisableList";
        msg = ss.str();
        return false;
    }

    return true;
}

/**
 * @brief Floorsweep all the module instances that are specified in the mustDisableList in the SKU definition
 * 
 * @param potentialSKU The SKU definition
 */
void FSChip_t::applyMustDisableLists(const FUSESKUConfig::SkuConfig& potentialSKU){
    for (const auto& fsconfig_it : potentialSKU.fsSettings) {
        module_type_id module_id = strToId(fsconfig_it.first);
        const FUSESKUConfig::FsConfig& skuconfig = fsconfig_it.second;
        flat_module_list_t module_vector = getFlatModuleList(module_id);
        for(auto instance_id : skuconfig.mustDisableList) {
            if (module_vector[instance_id]->getEnabled()){
                module_vector[instance_id]->setEnabled(false);
            }
        }
    }
}

/**
 * @brief this is only to be used if the sku config is not specified
 * 
 * @param module_list 
 * @return FUSESKUConfig::FsConfig 
 */
static FUSESKUConfig::FsConfig makeUnrestrictiveFsSkuConfig(const const_flat_module_list_t& module_list){
    return {0, static_cast<uint32_t>(module_list.size()), UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, {}, {}};
}


/**
 * @brief Floorsweep all the module instances that don't meet sku requirements
 * For example, this would floorsweep a GPC which did not have enough TPCs
 * 
 * @param potentialSKU The SKU definition
 */
void FSChip_t::disableNonCompliantInstances(const FUSESKUConfig::SkuConfig& potentialSKU, std::string msg){
    std::stringstream ss;
    // Check all modules and make sure they have enough submodules of each type
    for(module_type_id submodule_type_id : findAllSubUnitTypes()){
        for(FSModule_t* module : getFlatModuleList(submodule_type_id)){
            if (!module->moduleCanFitSKU(potentialSKU, msg)){
                module->setEnabled(false);
                funcDownBin();
                ss << submodule_type_id << module->instance_number << ", ";
            }
        }
    }
    // write to the msg saying what had to be disabled
    if (ss.tellp() != 0){
        ss.seekp(-2, std::ios_base::end);
        ss << " had to be disabled because they didn't meet the sku";
        msg = ss.str();
    }
}

/**
 * @brief If this module is specified in the sku, then use whatever it says
 * If not, then assume no floorsweeping is required
 * 
 * @param module_id 
 * @param potentialSKU 
 * @return uint32_t 
 */
uint32_t FSChip_t::getMaxAllowed(module_type_id module_id, const FUSESKUConfig::SkuConfig& potentialSKU) const {
    const auto& sku_fs_config_it = potentialSKU.fsSettings.find(idToStr(module_id));
    if (sku_fs_config_it != potentialSKU.fsSettings.end()){
        return sku_fs_config_it->second.maxEnableCount;
    }
    return getFlatModuleList(module_id).size();
}

/**
 * @brief If this module is specified in the sku, then use whatever it says
 * If not, then assume any amount of floorsweeping is allowed
 * 
 * @param module_id 
 * @param potentialSKU 
 * @return uint32_t 
 */
uint32_t FSChip_t::getMinAllowed(module_type_id module_id, const FUSESKUConfig::SkuConfig& potentialSKU) const {
    const auto& sku_fs_config_it = potentialSKU.fsSettings.find(idToStr(module_id));
    if (sku_fs_config_it != potentialSKU.fsSettings.end()){
        return sku_fs_config_it->second.minEnableCount;
    }
    return 1;
}

/**
 * @brief A wrapper for canFitSKUSearch. It calls skuIsPossible first, then calls canFitSKUSearch
 * 
 * @param potentialSKU 
 * @param msg 
 * @return true 
 * @return false 
 */
bool FSChip_t::canFitSKU(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {

    if (!skuIsPossible(potentialSKU, msg)){
        return false;
    }

    std::unique_ptr<FSChip_t> disable_must_disables = clone();
    disable_must_disables->applyMustDisableLists(potentialSKU);
    disable_must_disables->applyDownBinHeuristic(potentialSKU);

    return disable_must_disables->canFitSKUSearchWrap(potentialSKU, msg) != nullptr;
}

/**
 * @brief To improve runtime, we can take advantage of the fact that gpc and fbp fs is independent in gh100
 * If a chip does have entanglements between GPCs and FBPs, then this function should be overridden
 *
 * @param potentialSKU
 * @param msg
 * @return std::unique_ptr<FSChip_t>
 */
std::unique_ptr<FSChip_t> FSChip_t::canFitSKUSearchWrap(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {

    // First check if the sku has GPC/TPC stuff, and split that into a new temporary sku
    FUSESKUConfig::SkuConfig gpcSKU = potentialSKU;
    gpcSKU.fsSettings.erase(idToStr(module_type_id::fbp));
    for (module_type_id fbp_submodule : FBPs[0]->findAllSubUnitTypes()) {
        gpcSKU.fsSettings.erase(idToStr(fbp_submodule));
    }

    // Then clone the chip and downbin with that sku
    std::string can_fit_gpc_msg;
    std::unique_ptr<FSChip_t> gpc_downbin_clone = canFitSKUSearch(gpcSKU, can_fit_gpc_msg);
    if (gpc_downbin_clone == nullptr) {
        msg = can_fit_gpc_msg;
        return nullptr;
    }

    // Then check if the sku has FBP/LTC stuff and split that into a new temporary sku
    FUSESKUConfig::SkuConfig fbpSKU = potentialSKU;
    fbpSKU.fsSettings.erase(idToStr(module_type_id::gpc));
    for (module_type_id gpc_submodule : GPCs[0]->findAllSubUnitTypes()) {
        fbpSKU.fsSettings.erase(idToStr(gpc_submodule));
    }

    // Then clone the chip and downbin with that sku
    std::string can_fit_fbp_msg;
    std::unique_ptr<FSChip_t> fbp_downbin_clone = canFitSKUSearch(fbpSKU, can_fit_fbp_msg);
    if (fbp_downbin_clone == nullptr) {
        msg = can_fit_fbp_msg;
        return nullptr;
    }

    // If both were successful, then merge the configs and return
    gpc_downbin_clone->merge(*fbp_downbin_clone);
    return gpc_downbin_clone;
}

/**
 * @brief Check if this chip instance can be further floorswept to fit into this SKU.
 * 
 * @param potentialSKU The SKU definition
 * @param msg The reason why it cannot be floorswept further to fit the SKU
 * @return true if it can fit the SKU
 * @return false if it cannot fit the SKU
 */
std::unique_ptr<FSChip_t> FSChip_t::canFitSKUSearch(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {

    std::unique_ptr<FSChip_t> result_ptr;

    // Just make sure we have more than the min enable counts
    for (const auto& fsconfig_it : potentialSKU.fsSettings) {
        module_type_id module_id = strToId(fsconfig_it.first);
        const FUSESKUConfig::FsConfig& skuconfig = fsconfig_it.second;

        const_flat_module_list_t module_vector = getFlatModuleList(module_id);
        if (!listCanFitSKU(skuconfig, module_vector, msg)){
            return nullptr;
        }
    }

    {
        std::unique_ptr<FSChip_t> func_downbinned_chip = clone();
        func_downbinned_chip->funcDownBin();

        // to avoid accidental relwrsion, make sure that func downbinning happened
        if (*func_downbinned_chip != *this){

            std::string funcDownBinIsValidMsg;
            if (!func_downbinned_chip->isValid(FSmode::FUNC_VALID, funcDownBinIsValidMsg)){
                std::stringstream ss;
                ss << "This config cannot be made valid: " << funcDownBinIsValidMsg;
                msg = ss.str();
                return nullptr;
            }
            // check that the fixed up chip can fit the SKU
            std::string canFitFuncDownBinMsg;
            result_ptr = func_downbinned_chip->canFitSKUSearch(potentialSKU, canFitFuncDownBinMsg);
            if (result_ptr == nullptr){
                std::stringstream ss;
                ss << "After fixing the config to make it valid, it cannot meet the SKU requirements: " << canFitFuncDownBinMsg;
                msg = ss.str();
                return nullptr;
            } 
            return result_ptr; // if the func downbinned chip is valid and can fit the sku, then return true
        }
    }

    {
        std::unique_ptr<FSChip_t> clone_chip = clone();
        std::string disable_non_comforming_units_msg;
        clone_chip->disableNonCompliantInstances(potentialSKU, disable_non_comforming_units_msg);

        // if all modules conformed to the sku, then proceed
        if (*clone_chip != *this){
            std::string sub_msg;
            result_ptr = clone_chip->canFitSKUSearch(potentialSKU, sub_msg);
            if (result_ptr == nullptr){
                std::stringstream ss;
                if (disable_non_comforming_units_msg.size() != 0){
                    ss << "after disabling some non-conforming units: ";
                    ss << disable_non_comforming_units_msg << ", ";
                }
                ss << sub_msg;
                msg = ss.str();
                return nullptr;
            } else {
                return result_ptr;
            }
        }
    }

    if (isInSKU(potentialSKU, msg)){
        if (isValid(fslib::FSmode::FUNC_VALID, msg)){
            return clone();
        }
    }
        
    for (module_type_id module_id : getSortedDownBinTypes()){
        const_flat_module_list_t module_vector = getFlatModuleList(module_id);
        // downbin based on maxEnableCount and see if it's still allowed
        if (getNumEnabled(module_vector) > getMaxAllowed(module_id, potentialSKU)){

            std::string can_fit_after_fs_msg;
            std::stringstream ss;
            ss << "Cannot fit the sku! ";
            ss << "There are " << getNumEnabled(module_vector) << " " << module_id << "s, and the sku requires max of " << getMaxAllowed(module_id, potentialSKU);

            std::stringstream disable_ss;
            for (uint32_t disable_candidate : getPreferredDisableListMustEnable(module_id, potentialSKU)) {
                std::unique_ptr<FSChip_t> sku_downbinned_chip = clone();
                sku_downbinned_chip->getModuleFlatIdx(module_id, disable_candidate)->setEnabled(false);

                result_ptr = sku_downbinned_chip->canFitSKUSearch(potentialSKU, can_fit_after_fs_msg);

                if (result_ptr != nullptr){
                    return result_ptr;
                }
                disable_ss.str("");
                disable_ss << ". Disable " << module_id << disable_candidate << " and try again: ";
            }

            ss << disable_ss.str();
            ss << can_fit_after_fs_msg;
            msg = ss.str();
            return nullptr;
        }
    }

    return nullptr;
}

/**
 * @brief Check if the sku is possible for this module type. This does not actually look at existing floorsweeping.
 * 
 * @param type_id The type of module being checked
 * @param skuconfig The SKU definition for this module
 * @param total_count total number of modules of this type
 * @param msg Reason why this isn't possible
 * @return true if the SKU is possible
 * @return false if the SKU is not possible
 */
static bool skuConfigIsPossible(module_type_id type_id, const FUSESKUConfig::FsConfig& skuconfig, uint32_t total_count, std::string& msg){

    // look at the mustDisableList and see if it conflicts with minEnableCount
    uint32_t disable_list_max_possible_left = total_count - skuconfig.mustDisableList.size();
    if (disable_list_max_possible_left < skuconfig.minEnableCount){
        std::stringstream ss;
        ss << "The SKU requires at least " << skuconfig.minEnableCount << " " << type_id << "s ";
        ss << " but the mustDisableList length is " << skuconfig.mustDisableList.size() << " and ";
        ss << "the most possible " << type_id << "s is " << total_count;
        msg = ss.str();
        return false;
    }

    // look at the mustEnableList and see if it conflicts with maxEnableCount
    if (skuconfig.mustEnableList.size() > skuconfig.maxEnableCount){
        std::stringstream ss;
        ss << "The SKU requires at most " << skuconfig.maxEnableCount << " " << type_id << "s ";
        ss << " but the mustEnableList length is " << skuconfig.mustEnableList.size();
        msg = ss.str();
        return false;
    }

    return true;
}

/**
 * @brief Check the group requirements and make sure they are valid.
 * 
 * @param type_id The type of the base module
 * @param skuconfig SkU definition for base module type
 * @param parent_id The type of the parent module
 * @param parent_skuconfig SKU definition for parent module types
 * @param msg Reason why this isn't possible
 * @return true if possible
 * @return false not possible
 */
static bool skuConfigIsPossibleGroup(module_type_id type_id,        const FUSESKUConfig::FsConfig& skuconfig,       
                                     module_type_id parent_id, const FUSESKUConfig::FsConfig& parent_skuconfig,
                                     uint32_t max_enable_count_per_group, uint32_t max_possible_group_count, std::string& msg){


    // if enableCountPerGroup is set, then enableCountPerGroup for child * min parent count must be >= min count and <= max count for the child
    if (skuconfig.enableCountPerGroup != UNKNOWN_ENABLE_COUNT){
        // group size * max num groups > max count
        if (skuconfig.enableCountPerGroup * parent_skuconfig.minEnableCount > skuconfig.maxEnableCount){
            std::stringstream ss;
            ss << "The SKU specifies " << skuconfig.enableCountPerGroup << " " << type_id << "s ";
            ss << " per " << parent_id << ", and a minimum of " << parent_skuconfig.minEnableCount << " " << parent_id << "s. ";
            ss << "but the max " << type_id << " count is " << skuconfig.maxEnableCount << ". " << skuconfig.enableCountPerGroup << " * ";
            ss << parent_skuconfig.minEnableCount << " > " << skuconfig.maxEnableCount;
            msg = ss.str();
            return false;
        }

        if (skuconfig.enableCountPerGroup * parent_skuconfig.maxEnableCount < skuconfig.minEnableCount){
            std::stringstream ss;
            ss << "The SKU specifies " << skuconfig.enableCountPerGroup << " " << type_id << "s ";
            ss << " per " << parent_id << ", and a maximum of " << parent_skuconfig.maxEnableCount << " " << parent_id << "s. ";
            ss << "but the min " << type_id << " count is " << skuconfig.minEnableCount << ". " << skuconfig.enableCountPerGroup << " * ";
            ss << parent_skuconfig.maxEnableCount << " < " << skuconfig.minEnableCount;
            msg = ss.str();
            return false;
        }
    }
    
    // if minEnablePerGroup is set, then use the same method as >= min above with enableCountPerGroup
    if (skuconfig.minEnablePerGroup != UNKNOWN_ENABLE_COUNT){
        // group size * max num groups > max count
        if (skuconfig.minEnablePerGroup * parent_skuconfig.minEnableCount > skuconfig.maxEnableCount){
            std::stringstream ss;
            ss << "The SKU specifies at least " << skuconfig.minEnablePerGroup << " " << type_id << "s ";
            ss << " per " << parent_id << ", and a minimum of " << parent_skuconfig.minEnableCount << " " << parent_id << "s. ";
            ss << "but the max " << type_id << " count is " << skuconfig.maxEnableCount << ". " << skuconfig.minEnablePerGroup << " * ";
            ss << parent_skuconfig.minEnableCount << " > " << skuconfig.maxEnableCount;
            msg = ss.str();
            return false;
        }
    }

    // if the max parent count * max enable per group is still less than the min possible count
    if (max_enable_count_per_group * parent_skuconfig.maxEnableCount < skuconfig.minEnableCount){
        std::stringstream ss;
        ss << "The SKU specifies " << max_enable_count_per_group << " " << type_id << "s ";
        ss << " per " << parent_id << ", and a maximum of " << parent_skuconfig.maxEnableCount << " " << parent_id << "s. ";
        ss << "but the min " << type_id << " count is " << skuconfig.minEnableCount << ". " << max_enable_count_per_group << " * ";
        ss << parent_skuconfig.maxEnableCount << " < " << skuconfig.minEnableCount;
        msg = ss.str();
        return false;
    }


    // now check mustDisable and mustEnable lists for each group
    module_index_list_t num_must_enable_per_group;
    num_must_enable_per_group.assign(max_possible_group_count, 0);
    for(auto mustEnableIdx : skuconfig.mustEnableList){
        uint32_t group_idx = mustEnableIdx / max_enable_count_per_group;
        num_must_enable_per_group[group_idx] += 1;
    }

    for(uint32_t group_idx = 0; group_idx < num_must_enable_per_group.size(); group_idx++){
        if (skuconfig.enableCountPerGroup != UNKNOWN_ENABLE_COUNT){
            if (num_must_enable_per_group[group_idx] > skuconfig.enableCountPerGroup){
                std::stringstream ss;
                ss << "The SKU specifies " << skuconfig.enableCountPerGroup << " " << type_id << "s per ";
                ss << parent_id << ", but the mustEnableList has " << num_must_enable_per_group[group_idx];
                ss << " entries that must be enabled in " << parent_id << " " << group_idx << "!";
                return false;
            }
        }
    }

    module_index_list_t num_must_disable_per_group;
    num_must_disable_per_group.assign(max_possible_group_count, 0);
    for(auto mustDisableIdx : skuconfig.mustDisableList){
        uint32_t group_idx = mustDisableIdx / max_enable_count_per_group;
        num_must_disable_per_group[group_idx] += 1;
    }

    for(uint32_t group_idx = 0; group_idx < num_must_disable_per_group.size(); group_idx++){
        uint32_t num_remaining_after_must_disable = max_enable_count_per_group - num_must_disable_per_group[group_idx];
        if (num_remaining_after_must_disable < skuconfig.getMinGroupCount()){
            std::stringstream ss;
            ss << "The SKU specifies at least " << skuconfig.getMinGroupCount() << " " << type_id << "s per ";
            ss << parent_id << ", but the mustDisableList has " << num_must_enable_per_group[group_idx];
            ss << " entries that must be disabled in " << parent_id << " " << group_idx << ", ";
            ss << " leaving only " << num_remaining_after_must_disable << " enabled after!";
            return false;
        }
    }

    return true;
}

/**
 * @brief This doesn't need to know anything about the chip's FS status, just uses the structure to make it easier to figure things out
 * 
 * @param potentialSKU The SKU definition to check
 * @param msg Reason why this isn't possible
 * @return true if possible
 * @return false if not possible
 */
bool FSChip_t::skuIsPossible(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {
    if (!potentialSKU.skuIsPossible(msg)){
        return false;
    }
    for (const auto& fsconfig_it : potentialSKU.fsSettings) {
        module_type_id module_id = strToId(fsconfig_it.first);
        const FUSESKUConfig::FsConfig& skuconfig = fsconfig_it.second;

        const_flat_module_list_t module_vector = getFlatModuleList(module_id);
        if (!skuConfigIsPossible(module_id, skuconfig, module_vector.size(), msg)){
            return false;
        }

        //check the group counts
        module_type_id parent_id = findParentModuleTypeId(module_id);
        if (parent_id != module_type_id::invalid && parent_id != module_type_id::chip) {
            const_flat_module_list_t group_module_vector = getFlatModuleList(parent_id);

            const_base_module_list_t temp_module_vector = group_module_vector[0]->returnModuleList(module_id);
            const auto& sku_fs_config_it = potentialSKU.fsSettings.find(idToStr(parent_id));
            const FUSESKUConfig::FsConfig& parent_skuconfig = (sku_fs_config_it != potentialSKU.fsSettings.end())
                                                              ? sku_fs_config_it->second
                                                              : makeUnrestrictiveFsSkuConfig(group_module_vector);
                                                                                   
            if (!skuConfigIsPossibleGroup(module_id, skuconfig, parent_id, parent_skuconfig, temp_module_vector.size(), group_module_vector.size(), msg)){
                return false;
            }
        }
    }

    // check to make sure that there are no mustDisables that are implicitly mustEnables
    std::unique_ptr<FSChip_t> fresh_chip = createChip(getChipEnum());
    fresh_chip->applyMustDisableLists(potentialSKU);
    fresh_chip->funcDownBin();
    
    for (module_type_id module_type : findAllSubUnitTypes()){
        const_flat_module_list_t module_list = static_cast<const FSChip_t*>(fresh_chip.get())->getFlatModuleList(module_type);
        for (uint32_t i = 0; i < module_list.size(); i++){
            if (!module_list[i]->getEnabled() && isInMustEnableList(potentialSKU, module_type, i)){
                std::stringstream ss;
                ss << "mustEnableList and mustDisableList for dependent module types conflict!";
                msg = ss.str();
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief Take a list of SKU definitions and return a list of the ones that this chip instance can fit into
 * 
 * @param allPossibleSKUs List of SKU definitions
 * @return std::vector<FUSESKUConfig::SkuConfig> List of SKU definitions which this chip can fit
 */
std::vector<FUSESKUConfig::SkuConfig> FSChip_t::getSKUs(const std::vector<FUSESKUConfig::SkuConfig>& allPossibleSKUs) const {
    std::vector<FUSESKUConfig::SkuConfig> list;
    //TODO. not implemented yet
    return list;
}

/**
 * @brief Apply extra floorsweeping to attempt to make the config valid
 * 
 */
void FSChip_t::funcDownBin() {
    for (auto& GPC : GPCs){
        GPC->funcDownBin();
    }

    for (auto& FBP : FBPs){
        FBP->funcDownBin();
    }
}


/**
 * @brief Apply extra floorsweeping to attempt to make this config fit the sku
 * 
 */
bool FSChip_t::downBin(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) {

    if (!skuIsPossible(potentialSKU, msg)){
        return false;
    }

    applyMustDisableLists(potentialSKU);

    funcDownBin();
    disableNonCompliantInstances(potentialSKU, msg);

    applyDownBinHeuristic(potentialSKU);
    
    std::unique_ptr<FSChip_t> downbinned_copy = canFitSKUSearchWrap(potentialSKU, msg);

    if (downbinned_copy == nullptr){
        return false;
    }

    *this = *downbinned_copy;
    msg = "";
    return true;
}

/**
 * @brief Apply any known heuristics in overriding functions.
 * The default is to do nothing.
 * 
 * @param potentialSKU 
 */
void FSChip_t::applyDownBinHeuristic(const FUSESKUConfig::SkuConfig& potentialSKU) {
    
}


/**
 * @brief compare this config to another config in the same sku. Return 1 if the candidate is better
 * Return 0 if the candidate is equal, and return -1 if the candidate is worse.
 * The candidate must already be in the same sku as this config
 * 
 * @param candidate 
 * @return int 
 */
int FSChip_t::compareConfig(const FSChip_t* candidate_chip) const {
    // the unspecialized case is that all configs are equal
    return 0;
}

/**
 * @brief Kill all the FBPs in this chip
 * 
 */
void FSChip_t::killFBPs(){
    for (auto& fbp : FBPs){
        fbp->setEnabled(false);
        fbp->funcDownBin();
    }
}

/**
 * @brief Pick GPCs and FBPs first when downbinning.
 * This isn't lwrrently being called, but this could be used in the future if we need to FS some module types
 * before others while downbinning.
 * 
 * @return std::vector<module_type_id> 
 */
std::vector<module_type_id> FSChip_t::getDownBinTypePriority() const {
    return {module_type_id::gpc, module_type_id::fbp, module_type_id::ltc, module_type_id::fbio};
}

//-----------------------------------------------------------------------------
// GPUWithuGPU_t
//-----------------------------------------------------------------------------
/**
 * @brief Initialize the microgpu map
 * 
 */
const std::vector<int>& GPUWithuGPU_t::getUGPUMap() const {
    const static std::vector<int> fbp_mapping = {3, 4, 5, 0, 1, 2, 11, 8, 7, 10, 9, 6};
    return fbp_mapping;
}

/**
 * @brief Get a reference to the LTC at this flat index
 * 
 * @param ltc_idx 
 * @return const LTC_t& 
 */
const LTC_t& GPUWithuGPU_t::getLTC(int ltc_idx) const {
    int ltcs_per_fbp = getChipPORSettings().num_ltcs / getChipPORSettings().num_fbps;
    int fbp_idx = ltc_idx / ltcs_per_fbp;
    int ltc_per_fbp_idx = ltc_idx % ltcs_per_fbp;
    return *FBPs[fbp_idx]->LTCs[ltc_per_fbp_idx];
}

/**
 * @brief Get a reference to the L2Slice at this flat index
 * 
 * @param l2slice_idx 
 * @return const L2Slice_t& 
 */
const L2Slice_t& GPUWithuGPU_t::getL2Slice(int l2slice_idx) const {
    int fbp_idx = l2slice_idx / FBP_t::slices_per_fbp;
    int l2slice_per_fbp_idx = l2slice_idx % FBP_t::slices_per_fbp;
    return *FBPs[fbp_idx]->L2Slices[l2slice_per_fbp_idx];
}

/**
 * @brief Get the index of the paired FBP of the FBP at this index
 * 
 * @param fbp_idx 
 * @return int 
 */
int GPUWithuGPU_t::getPairFBP(int fbp_idx) const {
    return getUGPUMap()[fbp_idx];
}

/**
 * @brief Get the flat index of the paired LTC of the LTC at this flat index
 * 
 * @param ltc_idx 
 * @return int 
 */
int GPUWithuGPU_t::getPairLTC(int ltc_idx) const {
    int ltcs_per_fbp = getChipPORSettings().num_ltcs / getChipPORSettings().num_fbps;
    int fbp_idx = ltc_idx / ltcs_per_fbp;
    int ltc_per_fbp_idx = ltc_idx % ltcs_per_fbp;
    int pair_ltc_per_fbp_idx = (ltc_per_fbp_idx + 1) % ltcs_per_fbp;
    int pair_fbp_idx = getPairFBP(fbp_idx);
    int pair_ltc_idx = pair_fbp_idx * ltcs_per_fbp + pair_ltc_per_fbp_idx;
    return pair_ltc_idx;
}

/**
 * @brief Get the flat index of the paired slice of the slice at this flat index
 * 
 * @param l2slice_idx 
 * @return int 
 */
int GPUWithuGPU_t::getPairL2Slice(int l2slice_idx) const {
    int ltcs_per_fbp = getChipPORSettings().num_ltcs / getChipPORSettings().num_fbps;
    int slices_per_ltc = FBP_t::slices_per_fbp / ltcs_per_fbp;
    int ltc_idx = l2slice_idx / slices_per_ltc;
    int slice_per_ltc_idx = l2slice_idx % slices_per_ltc;
    int pair_ltc_idx = getPairLTC(ltc_idx);
    int pair_l2slice_idx = (pair_ltc_idx * slices_per_ltc) + slice_per_ltc_idx;
    return pair_l2slice_idx;
}

/**
 * @brief Is this ugpu chip functionally valid?
 * 
 * @param fsmode degree of valid to check for 
 * @param msg if it's not valid. This will be set to a string that explains why
 * @return true if valid
 * @return false if not valid
 */
bool GPUWithuGPU_t::isValid(FSmode fsmode, std::string& msg) const {

    // check all the slices
    uint32_t total_ltcs = getChipPORSettings().num_fbps * FBP_t::slices_per_fbp;
    for (uint32_t l2slice_idx = 0; l2slice_idx < total_ltcs; l2slice_idx++){
        int pair_l2slice_idx = getPairL2Slice(l2slice_idx);
        const L2Slice_t& this_slice = getL2Slice(l2slice_idx);
        const L2Slice_t& pair_slice = getL2Slice(pair_l2slice_idx);
        if (this_slice.getEnabled() != pair_slice.getEnabled()){
            int enabled_slice_idx  = this_slice.getEnabled() ? l2slice_idx : pair_l2slice_idx;
            int disabled_slice_idx = pair_slice.getEnabled() ? l2slice_idx : pair_l2slice_idx;
            std::stringstream ss;
            ss << "Physical l2 slice" << enabled_slice_idx << " cannot be enabled when slice" << disabled_slice_idx << " is disabled!";
            msg = ss.str();
            return false;
        }
    }

    // check all the ltcs
    for (uint32_t ltc_idx = 0; ltc_idx < getChipPORSettings().num_ltcs; ltc_idx++){
        int pair_ltc_idx = getPairLTC(ltc_idx);
        const LTC_t& this_ltc = getLTC(ltc_idx);
        const LTC_t& pair_ltc = getLTC(pair_ltc_idx);
        if (this_ltc.getEnabled() ^ pair_ltc.getEnabled()){
            int enabled_ltc_idx =  this_ltc.getEnabled() ? ltc_idx : pair_ltc_idx;
            int disabled_ltc_idx = pair_ltc.getEnabled() ? ltc_idx : pair_ltc_idx;
            std::stringstream ss;
            ss << "Physical LTC" << enabled_ltc_idx << " cannot be enabled when LTC" << disabled_ltc_idx << " is disabled!";
            msg = ss.str();
            return false;
        }
    }

    // check all the fbps
    for (uint32_t fbp_idx = 0; fbp_idx < getUGPUMap().size(); fbp_idx++){
        int pair_fbp_idx = getPairFBP(fbp_idx);
        if (FBPs[fbp_idx]->getEnabled() ^ FBPs[pair_fbp_idx]->getEnabled()){
            int enabled_fbp_idx =  FBPs[fbp_idx]->getEnabled()      ? fbp_idx : pair_fbp_idx;
            int disabled_fbp_idx = FBPs[pair_fbp_idx]->getEnabled() ? fbp_idx : pair_fbp_idx;
            std::stringstream ss;
            ss << "Physical FBP" << enabled_fbp_idx << " cannot be enabled when FBP" << disabled_fbp_idx << " is disabled!";
            msg = ss.str();
            return false;
        }
    }

    return FSChip_t::isValid(fsmode, msg);
}

/**
 * @brief Get the number of unpaired FBPs
 * 
 * @return uint32_t 
 */
uint32_t GPUWithuGPU_t::getNumUnpairedFBPs() const {
    uint32_t num_unpaired_fbps = 0;
    for(uint32_t fbp_idx = 0; fbp_idx < FBPs.size(); fbp_idx++){
        if(FBPs[fbp_idx]->getEnabled() != FBPs[getPairFBP(fbp_idx)]->getEnabled()){
            num_unpaired_fbps += 1;
        }
    }
    return num_unpaired_fbps / 2; //checking all of them will count unpaired ones twice, so just divide by 2.
}

/**
 * @brief Get the number of unpaired LTCs
 * 
 * @return uint32_t 
 */
uint32_t GPUWithuGPU_t::getNumUnpairedLTCs() const {
    const_flat_module_list_t flat_ltcs = getFlatModuleList(module_type_id::ltc);

    uint32_t num_unpaired_ltcs = 0;
    for(uint32_t ltc_idx = 0; ltc_idx < flat_ltcs.size(); ltc_idx++){
        if(flat_ltcs[ltc_idx]->getEnabled() != flat_ltcs[getPairLTC(ltc_idx)]->getEnabled()){
            num_unpaired_ltcs += 1;
        }
    }
    return num_unpaired_ltcs / 2; //checking all of them will count unpaired ones twice, so just divide by 2.
}

/**
 * @brief Get the number of unpaired l2 slices
 * 
 * @return uint32_t 
 */
uint32_t GPUWithuGPU_t::getNumUnpairedL2Slices() const {
    const_flat_module_list_t flat_l2slices = getFlatModuleList(module_type_id::l2slice);

    uint32_t num_unpaired_l2slices = 0;
    for(uint32_t slice_idx = 0; slice_idx < flat_l2slices.size(); slice_idx++){
        if(flat_l2slices[slice_idx]->getEnabled() != flat_l2slices[getPairL2Slice(slice_idx)]->getEnabled()){
            num_unpaired_l2slices += 1;
        }
    }
    return num_unpaired_l2slices / 2; //checking all of them will count unpaired ones twice, so just divide by 2.
}

/**
 * @brief Apply extra floorsweeping to attempt to make the config valid
 * 
 */
void GPUWithuGPU_t::funcDownBin() {
    FSChip_t::funcDownBin();
    fixuGPU();
    fixSlicesPerChip();
    fixuGPU();
    FSChip_t::funcDownBin(); // call it again since fixuGPU may partially floorsweep stuff
}

/**
 * @brief Floorsweep away any LTCs with defective L2Slices if the slice count is not valid
 * 
 */
void GPUWithuGPU_t::fixSlicesPerChip() {
    // if it's more than 4 dead slices, we need to floorsweep LTCs instead
    std::string msg;
    if (!enableCountIsValid(module_type_id::l2slice, FSmode::FUNC_VALID, msg)) {
        for(uint32_t fbp_idx = 0; fbp_idx < FBPs.size(); fbp_idx++){
            for (uint32_t ltc_idx = 0; ltc_idx < FBPs[fbp_idx]->LTCs.size(); ltc_idx++){
                if (FBPs[fbp_idx]->getNumDisabledSlicesPerLTC(ltc_idx) != 0){
                    FBPs[fbp_idx]->LTCs[ltc_idx]->setEnabled(false);
                }
            }
        }
    }
}

/**
 * @brief floorsweep away all unpaired FBPs, LTCs, L2 Slices
 * 
 */
void GPUWithuGPU_t::fixuGPU() {

    // fix unpaired FBPs
    for(uint32_t fbp_idx = 0; fbp_idx < FBPs.size(); fbp_idx++){
        if(!FBPs[fbp_idx]->getEnabled()){
            FBPs[getPairFBP(fbp_idx)]->setEnabled(false);
        }
    }

    // fix unpaired LTCs
    flat_module_list_t flat_ltcs = getFlatModuleList(module_type_id::ltc);
    for(uint32_t ltc_idx = 0; ltc_idx < flat_ltcs.size(); ltc_idx++){
        if(!flat_ltcs[ltc_idx]->getEnabled()){
            flat_ltcs[getPairLTC(ltc_idx)]->setEnabled(false);
        }
    }

    // fix unpaired slices
    flat_module_list_t flat_l2slices = getFlatModuleList(module_type_id::l2slice);
    for(uint32_t slice_idx = 0; slice_idx < flat_l2slices.size(); slice_idx++){
        if(!flat_l2slices[slice_idx]->getEnabled()){
            flat_l2slices[getPairL2Slice(slice_idx)]->setEnabled(false);
        }
    }
}


/**
 * @brief Check if this chip instance can be further floorswept to fit into this SKU
 * This first checks if the uGPU rules can be followed, then calls the base class function
 * 
 * @param potentialSKU The SKU definition
 * @param msg The reason why it cannot be floorswept further to fit the SKU
 * @return true if it can fit the SKU
 * @return false if it cannot fit the SKU
 */
std::unique_ptr<FSChip_t> GPUWithuGPU_t::canFitSKUSearch(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {

    // handle unpaired FBP
    const auto& fbp_map_it = potentialSKU.fsSettings.find("fbp");
    if (fbp_map_it != potentialSKU.fsSettings.end()){
        const FUSESKUConfig::FsConfig& fbp_config = fbp_map_it->second;
        if (getNumEnabled(FBPs) - getNumUnpairedFBPs() < fbp_config.minEnableCount){
            std::stringstream ss;
            ss << "Not enough enabled FBPs to meet the SKU requirement of " << fbp_config.minEnableCount << " after disabling unpaired FBPs!";
            msg = ss.str();
            return nullptr;
        }
    }

    // handle unpaired LTC
    const auto& ltc_map_it = potentialSKU.fsSettings.find("ltc");
    if (ltc_map_it != potentialSKU.fsSettings.end()){
        const FUSESKUConfig::FsConfig& ltc_config = ltc_map_it->second;
        if (getNumEnabledLTCs() - getNumUnpairedLTCs() < ltc_config.minEnableCount){
            std::stringstream ss;
            ss << "Not enough enabled LTCs to meet the SKU requirement of " << ltc_config.minEnableCount << " after disabling unpaired LTCs!";
            ss << " there are only " << getNumEnabledLTCs() << " good LTCs, " << getNumUnpairedLTCs() << " of which are unpaired";
            msg = ss.str();
            return nullptr;
        }
    }


    // handle unpaired L2Slice
    const auto& l2slice_map_it = potentialSKU.fsSettings.find("l2slice");
    if (l2slice_map_it != potentialSKU.fsSettings.end()){
        const FUSESKUConfig::FsConfig& l2slice_config = l2slice_map_it->second;
        if (getNumEnabledSlices() - getNumUnpairedL2Slices() < l2slice_config.minEnableCount){
            std::stringstream ss;
            ss << "Not enough enabled L2Slices to meet the SKU requirement of " << l2slice_config.minEnableCount << " after disabling unpaired l2 slices!";
            ss << " there are only " << getNumEnabledSlices() << " good L2Slices, " << getNumUnpairedL2Slices() << " of which are unpaired";
            msg = ss.str();
            return nullptr;
        }
    }

    if (!uGPUImbalanceIsInSKU(potentialSKU, msg)){
        return nullptr;
    }

    return FSChip_t::canFitSKUSearch(potentialSKU, msg);
}

/**
 * @brief Specialization for ugpu chips
 * 
 * @param potentialSKU 
 * @param msg 
 * @return true 
 * @return false 
 */
bool GPUWithuGPU_t::skuIsPossible(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {

    // apply mustDisables and then check if the max imbalance is honored
    std::unique_ptr<FSChip_t> fresh_chip = createChip(getChipEnum());
    const GPUWithuGPU_t* ugpu_chip = dynamic_cast<const GPUWithuGPU_t*>(fresh_chip.get());
    assert(ugpu_chip != nullptr);

    fresh_chip->applyMustDisableLists(potentialSKU);
    fresh_chip->funcDownBin();

    const auto& fsSettings_it = potentialSKU.fsSettings.find(idToStr(module_type_id::tpc));
    if (fsSettings_it != potentialSKU.fsSettings.end()) {
        if (fsSettings_it->second.maxUGPUImbalance != UNKNOWN_ENABLE_COUNT) {
            uint32_t imbalance = std::abs(static_cast<int32_t>(ugpu_chip->getTPCCountPerUGPU(0)) - 
                static_cast<int32_t>(ugpu_chip->getTPCCountPerUGPU(1)));
            int32_t margin = static_cast<int32_t>(fsSettings_it->second.maxUGPUImbalance) - static_cast<int32_t>(imbalance);

            int32_t tpcs_available_to_fs = fresh_chip->getTotalEnableCount(module_type_id::tpc) - fsSettings_it->second.minEnableCount;

            if (margin + tpcs_available_to_fs < 0){
                std::stringstream ss;
                ss << "The tpc imbalance is too high!";
                msg = ss.str();
                return false;
            }
        }
    }

    return FSChip_t::skuIsPossible(potentialSKU, msg);
}

/**
 * @brief Return true if the TPC uGPU imbalance is within the SKU parameter
 * 
 * @param potentialSKU 
 * @param msg 
 * @return true 
 * @return false 
 */
bool GPUWithuGPU_t::uGPUImbalanceIsInSKU(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {
    const auto& fsSettings_it = potentialSKU.fsSettings.find(idToStr(module_type_id::tpc));
    if (fsSettings_it == potentialSKU.fsSettings.end()) {
        // if tpcs aren't specified in the sku config, then imbalance isn't a concern
        return true;
    }
    if (fsSettings_it->second.maxUGPUImbalance == UNKNOWN_ENABLE_COUNT) {
        // if maxUGPUImbalance wasn't specified
        return true;
    }
    uint32_t imbalance = std::abs(getuGPUImbalance());
    if (imbalance > fsSettings_it->second.maxUGPUImbalance) {
        std::stringstream ss;
        ss << "Cannot fit the sku because the TPC uGPU imbalance of " << imbalance << " exceeds the maximum allowed by the sku of ";
        ss << fsSettings_it->second.maxUGPUImbalance << ". (uGPU0: " << getTPCCountPerUGPU(0) << " tpcs, ";
        ss << "uGPU1: " << getTPCCountPerUGPU(1) << " tpcs, ";
        msg = ss.str();
        return false;
    }
    return true;
}

/**
 * @brief Get the TPC uGPU imbalance amount.
 * negative means that uGPU0 has fewer enabled TPCs than uGPU1
 * 
 * @return int32_t 
 */
int32_t GPUWithuGPU_t::getuGPUImbalance() const {
    return static_cast<int32_t>(getTPCCountPerUGPU(0)) - static_cast<int32_t>(getTPCCountPerUGPU(1));
}

/**
 * @brief Return the number of enabled TPCs in a uGPU
 * 
 * @param ugpu_idx 
 * @return uint32_t 
 */
uint32_t GPUWithuGPU_t::getTPCCountPerUGPU(uint32_t ugpu_idx) const {
    uint32_t count = 0;
    for (uint32_t i = 0; i < GPCs.size(); i++){
        if (getUGPUGPCMap()[i] == ugpu_idx) {
            count += GPCs[i]->getEnableCount(module_type_id::tpc);
        }
    }
    return count;
}

/**
 * @brief uGPU specializations for isInSKU. Checks for TPC uGPU imbalance before calling the base class function.
 * 
 * @param potentialSKU 
 * @param msg 
 * @return true 
 * @return false 
 */
bool GPUWithuGPU_t::isInSKU(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {
    if (!uGPUImbalanceIsInSKU(potentialSKU, msg)){
        return false;
    }

    return FSChip_t::isInSKU(potentialSKU, msg);
}

//-----------------------------------------------------------------------------
// DDRGPU_t
//-----------------------------------------------------------------------------

/**
 * @brief downbin this ddr chip to make it functionally valid
 * If there are multiple options, pick the choice with the most l2slices enabled
 * This function will only pick product valid slice configs.
 * There is not much value in slice configs that are not product valid
 * If there is a future desire for this, it could be added by adding an FSMode param
 * 
 */
void DDRGPU_t::funcDownBin() {
    FSChip_t::funcDownBin();
    
    std::string msg;
    // if it's already valid, don't do anything
    if (isValid(FSmode::PRODUCT, msg)){
        return;
    }

    // if something is dead in the memory system
    if (getNumEnabledSlices() != returnModuleList(module_type_id::l2slice).size()){

        // the logic here is somewhat subjective because there are so many choices for how to handle defects
        // 2 slice mode, 3 slice mode, half fbpa mode, full fbp FS, balanced ltc mode
        // the priority will be: 3 slice, half fbpa mode, full fbp fs, 2 slice, balanced mode


        std::unique_ptr<FSChip_t> best_memsys_fs_mode = clone();
        best_memsys_fs_mode->killFBPs(); // kill off the fbps so anything will be better than this


        {   // first check if 3 slice mode can work
            std::unique_ptr<FSChip_t> cloned_chip_holder = clone();
            DDRGPU_t* cloned_chip = static_cast<DDRGPU_t*>(cloned_chip_holder.get());
            cloned_chip->applySliceProduct(3);
            if (cloned_chip->isValid(FSmode::PRODUCT, msg)){
                if (cloned_chip->getNumEnabledSlices() > best_memsys_fs_mode->getNumEnabledSlices()){
                    best_memsys_fs_mode = std::move(cloned_chip_holder);
                }
            }
        }

        {   // then check if 32 bit mode can work
            std::unique_ptr<FSChip_t> cloned_chip_holder = clone();
            DDRGPU_t* cloned_chip = static_cast<DDRGPU_t*>(cloned_chip_holder.get());
            cloned_chip->applyHalfFBPA();
            if (cloned_chip->isValid(FSmode::PRODUCT, msg)){
                if (cloned_chip->getNumEnabledSlices() > best_memsys_fs_mode->getNumEnabledSlices()){
                    best_memsys_fs_mode = std::move(cloned_chip_holder);
                }
            }
        }

        {   // then check if 2 slice mode can work
            std::unique_ptr<FSChip_t> cloned_chip_holder = clone();
            DDRGPU_t* cloned_chip = static_cast<DDRGPU_t*>(cloned_chip_holder.get());
            cloned_chip->applySliceProduct(2);
            if (cloned_chip->isValid(FSmode::PRODUCT, msg)){
                if (cloned_chip->getNumEnabledSlices() > best_memsys_fs_mode->getNumEnabledSlices()){
                    best_memsys_fs_mode = std::move(cloned_chip_holder);
                }
            }
        }

        //pick whatever had the most slices enabled
        merge(*best_memsys_fs_mode);
    }
}

/**
 * @brief apply slice fs to the config, starting with start_mask
 * 
 * @param start_mask 
 * @param start_fbp the index of the fbp to start on
 */
void DDRGPU_t::applySliceProduct(mask32_t start_mask, uint32_t start_fbp) {
    std::string msg;
    mask32_t slice_mask = start_mask;

    for(uint32_t i = 0; i < FBPs.size(); i++){
        uint32_t fbp_idx = (i + start_fbp) % FBPs.size(); // wrap around
        if (FBPs[fbp_idx]->getEnabled()){
            FBPs[fbp_idx]->applyL2Mask(slice_mask);
            static_cast<DDRFBP_t*>(FBPs[fbp_idx].get())->nextL2ProdMask(msg, slice_mask, slice_mask);
        }
    }
}

/**
 * @brief Try applying n slice mode. If it was successful, return true, otherwise false
 * 
 * @param num_slices_per_ltc 
 * @return true 
 * @return false 
 */
bool DDRGPU_t::applySliceProduct(uint32_t num_slices_per_ltc) {
    mask32_t first_mask = 0;
    std::string msg;
    if (canDoSliceProduct(num_slices_per_ltc, msg, first_mask)){
        applySliceProduct(first_mask, 0);
        return true;
    }
    return false;
}

/**
 * @brief Check if it is possible to do slice FS on this config. If so, first_mask will be set to the chosen pattern's
 * first mask
 * 
 * @param num_slices_per_ltc 
 * @param msg 
 * @param first_mask 
 * @return true 
 * @return false 
 */
bool DDRGPU_t::canDoSliceProduct(uint32_t num_slices_per_ltc, std::string& msg, mask32_t& first_mask) const {
    
    // if it needs to be product valid, then just try all possibilities and if none are valid then fail
    for (const auto& pattern_kv : static_cast<const DDRFBP_t*>(FBPs[0].get())->getSlicePatternMap()){

        //uint32_t slices_per_ltc = FBP_t::slices_per_fbp / getChipPORSettings().num_ltcs;
        const mask32_t& slice_mask = pattern_kv.first;

        uint32_t requested_slice_en_count_per_fbp = num_slices_per_ltc * FBPs[0]->LTCs.size();
        uint32_t requested_slice_disable_count = FBP_t::slices_per_fbp - requested_slice_en_count_per_fbp;

        if (slice_mask.count() == requested_slice_disable_count){
            std::unique_ptr<FSChip_t> cloned_chip = clone();
            DDRGPU_t* ddr_gpu_clone = static_cast<DDRGPU_t*>(cloned_chip.get());
            ddr_gpu_clone->applySliceProduct(slice_mask, 0);
            if (ddr_gpu_clone->isValid(FSmode::PRODUCT, msg)){
                first_mask = slice_mask;
                return true;
            }
        }
    }
    std::stringstream ss;
    ss << "None of the slice masks would have been valid";
    msg = ss.str();
    return false;
}

/**
 * @brief wrapper around the other canDoSliceProduct function
 * 
 * @param num_slices_per_ltc 
 * @param msg 
 * @return true 
 * @return false 
 */
bool DDRGPU_t::canDoSliceProduct(uint32_t num_slices_per_ltc, std::string& msg) const {
    mask32_t throwaway;
    return canDoSliceProduct(num_slices_per_ltc, msg, throwaway);
}

/**
 * @brief apply half fbpa (32 bit mode) FS to this chip to make it functionally valid
 * 
 */
void DDRGPU_t::applyHalfFBPA(){
    for (auto& fbp : FBPs){
        fbp->funcDownBin();
        for(uint32_t i = 0; i < fbp->LTCs.size(); i++){
            std::string msg;
            // if the ltc has any slices disabled
            if (fbp->getNumEnabledSlicesPerLTC(i) < (fbp->L2Slices.size() / fbp->LTCs.size())){
                fbp->LTCs[i]->setEnabled(false);
                static_cast<DDRFBP_t*>(fbp.get())->HalfFBPAs[0]->setEnabled(false);
                fbp->funcDownBin();
            }
        }
    }
}

/**
 * @brief Return true if the sku is specifying a config with slice FS. This is based on the ratio between slices and ltcs
 * 
 * @param potentialSKU 
 * @return true 
 * @return false 
 */
bool DDRGPU_t::skuWantsSliceFS(const FUSESKUConfig::SkuConfig& potentialSKU) const {
    if (getMaxAllowed(module_type_id::l2slice, potentialSKU) < getModuleCount(module_type_id::l2slice)){
        if (getMinAllowed(module_type_id::ltc, potentialSKU) == getModuleCount(module_type_id::ltc)){
            return true;
        }
    }
    return false;
}

/**
 * @brief Return true if this sku is requesting 3 slice mode
 * 
 * @param potentialSKU 
 * @return true 
 * @return false 
 */
bool DDRGPU_t::skuWants3SliceMode(const FUSESKUConfig::SkuConfig& potentialSKU) const {
    if (skuWantsSliceFS(potentialSKU)){
        return getMaxAllowed(module_type_id::l2slice, potentialSKU) == getModuleCount(module_type_id::ltc) * 3;
    }
    return false;
}

/**
 * @brief Return true if this sku is requesting 2 slice mode
 * 
 * @param potentialSKU 
 * @return true 
 * @return false 
 */
bool DDRGPU_t::skuWants2SliceMode(const FUSESKUConfig::SkuConfig& potentialSKU) const {
    if (skuWantsSliceFS(potentialSKU)){
        return getMaxAllowed(module_type_id::l2slice, potentialSKU) == getModuleCount(module_type_id::ltc) * 2;
    }
    return false;
}

/**
 * @brief Special cases for ddr chips
 * 
 * @param potentialSKU 
 * @param msg 
 * @return true 
 * @return false 
 */
std::unique_ptr<FSChip_t> DDRGPU_t::canFitSKUSearch(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {

    //Special case for slices, then call the generic function after fixing up the slices

    std::unique_ptr<FSChip_t> downbinned_chip = clone();
    DDRGPU_t* ddr_clone = static_cast<DDRGPU_t*>(downbinned_chip.get());

    ddr_clone->applyMustDisableLists(potentialSKU);

    if (ddr_clone->skuWants3SliceMode(potentialSKU)){
        if (!ddr_clone->applySliceProduct(3)){
            return nullptr;
        }
    }

    if (ddr_clone->skuWants2SliceMode(potentialSKU)){
        if (!ddr_clone->applySliceProduct(2)){
            return nullptr;
        }
    }

    return ddr_clone->FSChip_t::canFitSKUSearch(potentialSKU, msg);
}


/**
 * @brief Apply extra floorsweeping to attempt to make this config fit the sku
 * This function calls the base class function, but first applies slice FS if it is requested
 * 
 */
bool DDRGPU_t::downBin(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) {

    // The strategy here will be to recognize that the 3 slice or 2 slice mode is being requested
    // Then call the base class function

    applyMustDisableLists(potentialSKU);

    if (skuWants3SliceMode(potentialSKU)){
        if (!applySliceProduct(3)){
            return false;
        }
    }

    if (skuWants2SliceMode(potentialSKU)){
        if (!applySliceProduct(2)){
            return false;
        }
    }

    return FSChip_t::downBin(potentialSKU, msg);
}

//-----------------------------------------------------------------------------
// Leaf Units
//-----------------------------------------------------------------------------

/**
 * @brief Return the module type
 * 
 * @return module_type_id 
 */
module_type_id PES_t::getTypeEnum() const {
    return module_type_id::pes;
}

/**
 * @brief Return the module type
 * 
 * @return module_type_id 
 */
module_type_id TPC_t::getTypeEnum() const {
    return module_type_id::tpc;
}

/**
 * @brief Return the module type
 * 
 * @return module_type_id 
 */
module_type_id ROP_t::getTypeEnum() const {
    return module_type_id::rop;
}

/**
 * @brief Return the module type
 * 
 * @return module_type_id 
 */
module_type_id CPC_t::getTypeEnum() const {
    return module_type_id::cpc;
}

/**
 * @brief Return the module type
 * 
 * @return module_type_id 
 */
module_type_id L2Slice_t::getTypeEnum() const {
    return module_type_id::l2slice;
}

/**
 * @brief Return the module type
 * 
 * @return module_type_id 
 */
module_type_id LTC_t::getTypeEnum() const {
    return module_type_id::ltc;
}

/**
 * @brief Return the module type
 * 
 * @return module_type_id 
 */
module_type_id FBIO_t::getTypeEnum() const {
    return module_type_id::fbio;
}

/**
 * @brief Return the module type
 * 
 * @return module_type_id 
 */
module_type_id HalfFBPA_t::getTypeEnum() const {
    return module_type_id::halffbpa;
}

/**
 * @brief Construct a new container object with this chip type instantiated inside
 * 
 * @param chip_enum 
 */
FSChipContainer_t::FSChipContainer_t(Chip chip_enum){
    createChipContainer(chip_enum, *this);
}

/**
 * @brief Make a clone of this chip inside the container
 * 
 * @param chip_to_clone 
 */
FSChipContainer_t::FSChipContainer_t(const FSChip_t* chip_to_clone){
    chip_to_clone->cloneContainer(*this);
}

//-----------------------------------------------------------------------------
// Mask Util Functions
//-----------------------------------------------------------------------------
// 
// Helper function, not for external use
template <class list_type>
static bool compareArrays(const list_type& list1, const list_type& list2) {
    for (uint32_t i = 0; i < sizeof(list1) / sizeof(list1[0]); i++) {
        if (list1[i] != list2[i]) {
            return false;
        }
    }
    return true;
}

/**
 * @brief compare two sets of fuses
 *
 * @param L left param
 * @param R right param
 * @return true if equal
 * @return false if not equal
 */
bool operator==(const FbpMasks& L, const FbpMasks& R) {
    return compareArrays(L.fbioPerFbpMasks, R.fbioPerFbpMasks) &&
        compareArrays(L.fbpaPerFbpMasks, R.fbpaPerFbpMasks) &&
        compareArrays(L.halfFbpaPerFbpMasks, R.halfFbpaPerFbpMasks) &&
        compareArrays(L.ltcPerFbpMasks, R.ltcPerFbpMasks) &&
        compareArrays(L.l2SlicePerFbpMasks, R.l2SlicePerFbpMasks) &&
        L.fbpMask == R.fbpMask;
}

/**
 * @brief compare two sets of fuses
 *
 * @param L left param
 * @param R right param
 * @return true if equal
 * @return false if not equal
 */
bool operator==(const GpcMasks& L, const GpcMasks& R) {
    return compareArrays(L.pesPerGpcMasks, R.pesPerGpcMasks) &&
        compareArrays(L.tpcPerGpcMasks, R.tpcPerGpcMasks) &&
        compareArrays(L.ropPerGpcMasks, R.ropPerGpcMasks) &&
        compareArrays(L.cpcPerGpcMasks, R.cpcPerGpcMasks) &&
        L.gpcMask == R.gpcMask;
}

template <class list_type>
static std::string maskPrinterHelper(const list_type& list1) {
    std::stringstream ss;
    ss << "{";
    ss << "0x" << std::hex << list1[0] << std::dec;
    //uint32_t len_to_print = sizeof(list1)/sizeof(list1[0]);
    uint32_t len_to_print = 16; // print fewer indexes to make it easier to read
    for (uint32_t i = 1; i < len_to_print; i++) {
        ss << ", 0x" << std::hex << list1[i] << std::dec;
    }
    ss << "}";
    return ss.str();
}

/**
 * @brief get a text representation of the fbp mask
 *
 * @param os
 * @param masks
 * @return std::ostream&
 */
std::ostream& operator<<(std::ostream& os, const FbpMasks& masks) {
    os << "fbpMask: 0x" << std::hex << masks.fbpMask << std::dec;
    os << ", fbioPerFbpMasks: " << maskPrinterHelper(masks.fbioPerFbpMasks);
    os << ", fbpaPerFbpMasks: " << maskPrinterHelper(masks.fbpaPerFbpMasks);
    os << ", halfFbpaPerFbpMasks: " << maskPrinterHelper(masks.halfFbpaPerFbpMasks);
    os << ", ltcPerFbpMasks: " << maskPrinterHelper(masks.ltcPerFbpMasks);
    os << ", l2SlicePerFbpMasks: " << maskPrinterHelper(masks.l2SlicePerFbpMasks);
    return os;
}

/**
 * @brief get a text representation of the gpc mask
 *
 * @param os
 * @param masks
 * @return std::ostream&
 */
std::ostream& operator<<(std::ostream& os, const GpcMasks& masks) {
    os << "gpcMask: 0x" << std::hex << masks.gpcMask << std::dec;
    os << ", pesPerGpcMasks: " << maskPrinterHelper(masks.pesPerGpcMasks);
    os << ", tpcPerGpcMasks: " << maskPrinterHelper(masks.tpcPerGpcMasks);
    os << ", ropPerGpcMasks: " << maskPrinterHelper(masks.ropPerGpcMasks);
    os << ", cpcPerGpcMasks: " << maskPrinterHelper(masks.cpcPerGpcMasks);
    return os;
}

}  // namespace fslib
