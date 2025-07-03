#ifndef TEST_PARTITION_SWITCH_POLICY_CHECK_H
#define TEST_PARTITION_SWITCH_POLICY_CHECK_H

#include <stdint.h>
#include <lwmisc.h>

#define COREPMP_START_INDEX 8
#define COREPMP_ENTRY_COUNT 16

#define IOPMP_START_INDEX 8
#define IOPMP_ENTRY_COUNT 8

typedef enum {
    MODE_OFF   = 0,
    MODE_TOR   = 1,
    MODE_NA4   = 2,
    MODE_NAPOT = 3,
} pmp_mode_enum;

typedef struct {
    uint8_t read;
    uint8_t write;
    uint8_t execute;
    pmp_mode_enum mode;
    uint64_t address;
} corepmp_cfg_struct;

typedef struct {
    pmp_mode_enum mode;
    uint32_t cfg;
    uint32_t address_lo;
    uint32_t address_hi;
} iopmp_cfg_struct;

typedef struct {
    uint32_t s_uid;
    uint32_t secret_mask0;
    uint32_t secret_mask1;
    uint32_t secret_mask_lock0;
    uint32_t secret_mask_lock1;
    uint32_t debug_ctl;
    uint32_t debug_ctl_lock;
    uint32_t hash_mpu;
    uint32_t mpu_start_index;
    uint32_t mpu_entry_count;
    uint32_t device_map_group_0;
    uint32_t device_map_group_1;
    uint32_t device_map_group_2;
    uint32_t device_map_group_3;
    corepmp_cfg_struct corepmp_cfgs[COREPMP_ENTRY_COUNT];
    iopmp_cfg_struct iopmp_cfgs[IOPMP_ENTRY_COUNT];
} policy_struct;

#endif
