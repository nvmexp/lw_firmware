#include "test.h"
#include <lwriscv/sbi.h>
#include <lwriscv/csr.h>
#include <liblwriscv/print.h>

char req_set_timer_0[] = "Must provide time parameter";
char req_set_timer_1[] = "REQ: If functionId is non-zero, return SBI_ERR_ILWALID_PARAM";
char req_set_timer_2[] = "REQ: If functionId is zero, return SBI_SUCCESS";
char req_set_timer_3[] = "REQ: Mtimecmp must match specified input";

static SBI_RETURN_VALUE get_condition_set_timer(char** msg, int32_t funcid)
{
    SBI_RETURN_VALUE rc;
    rc.error = 0;
    rc.value = 0;
    *msg = req_set_timer_2;

    if (funcid != 0)
    {
        rc.error = SBI_ERR_ILWALID_PARAM;
        *msg = req_set_timer_1;
    }

    return rc;
}

int test_set_timer(int32_t funcid, uint64_t num_args, uint64_t* param)
{
    TEST_EXPECT(PASS);

    if (num_args < 1)
    {
        TEST_FAILED("%s\n", req_set_timer_0);
        return -1;
    }

    char* req_msg;
    uint64_t time = *param;

    // Get expected conditions and corresponding requirements
    SBI_RETURN_VALUE expected_rc = get_condition_set_timer(&req_msg, funcid);
    SBI_RETURN_VALUE rc = sbicall1(SBI_EXTENSION_SET_TIMER, funcid, time);

    // Make sure we get expected return values
    EXPECT_TRUE((expected_rc.error == rc.error) && (expected_rc.value == rc.value), "%s, exp_err: %ld, exp_value: 0x%lx, test_err: %ld, test_value: 0x%lx\n", req_msg, expected_rc.error, expected_rc.value, rc.error, rc.value);

    // For normal set-up, also read mtimecmp and make sure it matches the specified value
    if (expected_rc.error == SBI_SUCCESS)
    {
        uint64_t mtimecmp_value;
        EXPECT_TRUE(test_get_mcsr(LW_RISCV_CSR_MTIMECMP, &mtimecmp_value) == 0, "read mtimecmp failed addr: 0x%x\n", LW_RISCV_CSR_MTIMECMP);
        EXPECT_TRUE(mtimecmp_value == time, "%s, time: 0x%lx, value in mtimecmp: 0x%lx\n", req_set_timer_3, time, mtimecmp_value);
    }

    // notify pass
    TEST_PASSED;
    return 0;
}
