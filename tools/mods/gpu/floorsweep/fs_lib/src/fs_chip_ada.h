#pragma once

#include "fs_chip_core.h"


namespace fslib {

/**
 * @brief Configuration and rules for ADLit1 chips
 *
 * Contains custom GPC and FBP generators
 * This chip has DDR memory with no CPCs
 */
class ADLit1Chip_t : public DDRGPU_t {
   public:

    virtual void getGPCType(GPCHandle_t& handle) const;
    virtual void getFBPType(FBPHandle_t& handle) const;
    virtual bool isValid(FSmode fsmode, std::string& msg) const;
    virtual void funcDownBin();
protected:
    virtual bool checkBalancedLTCValid(FSmode fsmode, std::string& msg) const;
    virtual bool checkBalancedLTCAllowed() const;
};

/**
 * @brief Configuration and rules for AD102
 * 
 */
class AD102_t : public ADLit1Chip_t {
public:
    virtual const chip_settings_t& getChipSettings() const;
};

/**
 * @brief Configuration and rules for AD103
 * 
 */
class AD103_t : public ADLit1Chip_t {
public:
    virtual bool isValid(FSmode fsmode, std::string& msg) const;
    virtual const chip_settings_t& getChipSettings() const;
};

/**
 * @brief Configuration and rules for AD104
 * 
 */
class AD104_t : public ADLit1Chip_t {
public:
    virtual const chip_settings_t& getChipSettings() const;
};

/**
 * @brief Configuration and rules for AD106
 * 
 */
class AD106_t : public ADLit1Chip_t {
public:
    virtual const chip_settings_t& getChipSettings() const;
};

/**
 * @brief Configuration and rules for AD107
 * 
 */
class AD107_t : public ADLit1Chip_t {
public:
    virtual const chip_settings_t& getChipSettings() const;
protected:
    virtual bool checkBalancedLTCAllowed() const;
};

}  // namespace fslib



