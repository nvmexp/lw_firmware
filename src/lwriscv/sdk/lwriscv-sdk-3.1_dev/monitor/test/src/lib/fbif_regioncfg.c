#include "test.h"
#include <lwmisc.h>
#include <lwriscv/sbi.h>
#include <lwriscv/csr.h>
#include <liblwriscv/print.h>
#include <liblwriscv/io.h>

char req_fbif_regioncfg_0[] = "Must provide 2 parameter for region & value";
char req_fbif_regioncfg_1[] = "REQ: Return SBI_SUCCESS upon success";
char req_fbif_regioncfg_2[] = "REQ: T(region) filed shall be updated upon success";
char req_fbif_regioncfg_3[] = "REQ: Return SBI_ERR_ILWALID_PARAM and value 1 if region is not 0-7";
char req_fbif_regioncfg_4[] = "REQ: Return SBI_ERR_ILWALID_PARAM and value 2 if regioncfg is not a valid 4bit value";

#define SET_VALUE 0xFFFFFFFFUL

static SBI_RETURN_VALUE get_condition_fbif_regioncfg(char** msg, uint64_t region, uint64_t regioncfg)
{
    SBI_RETURN_VALUE rc;
    rc.error = SBI_SUCCESS;
    rc.value = 0;
    *msg = req_fbif_regioncfg_1;

    if (region > 7)
    {
        rc.error = SBI_ERR_ILWALID_PARAM;
        rc.value = 1;
        *msg = req_fbif_regioncfg_3;
        return rc;
    }

    if (regioncfg > 0xFUL)
    {
        rc.error = SBI_ERR_ILWALID_PARAM;
        rc.value = 2;
        *msg = req_fbif_regioncfg_4;
        return rc;
    }

    return rc;
}

static uint32_t get_exp_value(uint64_t region, uint32_t orig, uint32_t tx)
{
    uint32_t mask = 0xFU << (region * 4);
    return (orig & ~mask) | ((tx << (region * 4)) & mask);
}

static void clear_set_regioncfg(void)
{
    localWrite(LW_PRGNLCL_FBIF_REGIONCFG, SET_VALUE);
    
}

int test_fbif_regioncfg(int32_t funcid, uint64_t num_args, uint64_t* param)
{
    TEST_EXPECT(PASS);

    if (num_args < 2)
    {
        TEST_FAILED("%s\n", req_fbif_regioncfg_0);
        return -1;
    }

    char* req_msg;
    uint64_t region = *param;
    uint64_t regioncfg = *(param + 1);

    // Get expected conditions and corresponding requirements
    SBI_RETURN_VALUE expected_rc = get_condition_fbif_regioncfg(&req_msg, region, regioncfg);

    // set regioncfg to all 1's. and test only the field per region is updated
    clear_set_regioncfg();

    SBI_RETURN_VALUE rc = sbicall2(SBI_EXTENSION_LWIDIA, funcid, region, regioncfg);

    // Make sure we get expected return values
    EXPECT_TRUE((expected_rc.error == rc.error) && (expected_rc.value == rc.value), "%s, exp_err: %ld, exp_value: 0x%lx, test_err: %ld, test_value: 0x%lx\n", req_msg, expected_rc.error, expected_rc.value, rc.error, rc.value);

    // Make sure the tracectl is successfully updated
    if (expected_rc.error == SBI_SUCCESS)
    {
        uint32_t regioncfg_r = localRead(LW_PRGNLCL_FBIF_REGIONCFG);
        uint32_t exp = get_exp_value(region, SET_VALUE, (uint32_t)regioncfg); 
        EXPECT_TRUE(regioncfg_r == exp, "%s, expected: 0x%08x, real: 0x%08x\n", req_fbif_regioncfg_2, exp, regioncfg_r);
    }

    // notify pass
    TEST_PASSED;
    return 0;
}
