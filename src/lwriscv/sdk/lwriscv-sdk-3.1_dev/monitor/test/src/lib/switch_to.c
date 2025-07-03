#include "test.h"
#include <lwriscv/sbi.h>
#include <lwriscv/csr.h>
#include <liblwriscv/print.h>
#include "switch_to_param.h"

char req_switch_to_0[] = "Must provide 6 parameters, with last one being target partition ID";
char req_switch_to_1[] = "REQ: If functionId is invalid, shutdown the core";
char req_switch_to_2[] = "REQ: If target partition id is invalid, shutdown the core";
char req_switch_to_3[] = "REQ: If functionId and target partitionId is valid, switch to target partition";

static SBI_RETURN_VALUE get_condition_switch_to(char** msg, int32_t funcid, uint64_t target_id)
{
    SBI_RETURN_VALUE rc;
    rc.error = 0;
    rc.value = 0;
    *msg = req_switch_to_3;

    if (funcid != 0)
    {
        rc.error = SBI_ERR_ILWALID_PARAM;
        *msg = req_switch_to_1;
        TEST_EXPECT(SHUT);
        return rc;
    }

    if ((target_id != 0) && (target_id != 1))
    {
        *msg = req_switch_to_2;
        TEST_EXPECT(SHUT);
        return rc;
    }

    TEST_EXPECT(SWITCH);
    return rc;
}

int test_switch_to(int32_t funcid, uint64_t num_args, uint64_t* param)
{
    if (num_args != NUM_SBI_PARAMS)
    {
        TEST_FAILED("%s\n", req_switch_to_0);
        return -1;
    }
    uint64_t sbi_param[NUM_SBI_PARAMS];
    for (int i = 0; i < NUM_SBI_PARAMS; i++)
    {
        sbi_param[i] = *(param++);
    }

    char* req_msg;

    // Get expected conditions and corresponding requirements
    SBI_RETURN_VALUE expected_rc = get_condition_switch_to(&req_msg, funcid, sbi_param[5]);
    SBI_RETURN_VALUE rc = sbicall6(SBI_EXTENSION_LWIDIA, funcid,
                                   sbi_param[0], sbi_param[1], sbi_param[2], sbi_param[3], sbi_param[4], sbi_param[5]);

    // Make sure we get expected return values
    EXPECT_TRUE((expected_rc.error == rc.error) && (expected_rc.value == rc.value), "%s, exp_err: %ld, exp_value: 0x%lx, test_err: %ld, test_value: 0x%lx\n", req_msg, expected_rc.error, expected_rc.value, rc.error, rc.value);

    // notify pass
    TEST_PASSED;
    return 0;
}
