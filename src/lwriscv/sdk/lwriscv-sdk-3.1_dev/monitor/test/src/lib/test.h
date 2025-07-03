#ifndef LIB_TEST_H
#define LIB_TEST_H

#include <stdint.h>
#include <lwriscv/sbi.h>

#define PASS      "PASS"
#define FAIL      "FAIL"
#define SHUT      "SHUT"
#define SWITCH    "SWITCH"
#define TIMER_INT "TIMER_INTERRUPT"

#define TEST_FAILED(fmt, ...) \
    do { \
        printf(FAIL ": " fmt, __VA_ARGS__); \
        sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST); \
    } while (0);

#define TEST_PASSED \
    do { \
        printf(PASS "\n"); \
    } while (0);

#define TIMER_INT_DELIVER \
    do { \
        printf(TIMER_INT "\n"); \
    } while (0);

#define TEST_EXPECT(s) \
    do { \
        printf("\n" "EXPECT_" s ": "); \
    } while (0);

#define EXPECT_TRUE(cond, fmt, ...) \
    do { \
        if (!(cond)) { \
            TEST_FAILED(fmt, __VA_ARGS__); \
            return -1;                \
        } \
    } while (0);

#define SWITCHED(partId) \
    do { \
        printf(SWITCH " to partition %d\n", partId); \
    } while (0);


void TEST(int32_t extension, int32_t funcid, uint64_t num_args, ...);
void TEST_INIT(const char* test);
void TEST_END(void);

int test_get_mcsr(uint32_t csr_addr, uint64_t* value);
int test_set_timer(int32_t funcid, uint64_t num_args, uint64_t* args);
int test_switch_to(int32_t funcid, uint64_t num_args, uint64_t* args);
int test_shutdown(int32_t funcid);
int test_release_priv_lockdown(int32_t funcid);
int test_update_tracectl(int32_t funcid, uint64_t num_args, uint64_t* args);
int test_fbif_transcfg(int32_t funcid, uint64_t num_args, uint64_t* args);
int test_fbif_regioncfg(int32_t funcid, uint64_t num_args, uint64_t* args);
int test_tfbif_transcfg(int32_t funcid, uint64_t num_args, uint64_t* args);
int test_tfbif_regioncfg(int32_t funcid, uint64_t num_args, uint64_t* args);
int test_ilwalid_extension(int32_t extension, int32_t funcid, uint64_t num_args, uint64_t* args);

#endif // LIB_TEST_H
