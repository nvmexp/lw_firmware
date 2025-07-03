#include "test.h"
#include <lwriscv/sbi.h>
#include <lwriscv/csr.h>
#include <liblwriscv/print.h>

char req_shutdown_1[] = "REQ: If functionId is non-zero, return SBI_ERR_ILWALID_PARAM";
char req_shutdown_2[] = "REQ: If functionId is zero, shutdown the core";

static SBI_RETURN_VALUE get_condition_shutdown(char** msg, int32_t funcid)
{
    SBI_RETURN_VALUE rc;
    rc.error = 0;
    rc.value = 0;
    *msg = req_shutdown_2;

    if (funcid != 0)
    {
        rc.error = SBI_ERR_ILWALID_PARAM;
        *msg = req_shutdown_1;
        TEST_EXPECT(PASS);
    }
    else
    {
        TEST_EXPECT(SHUT);
    }
    return rc;
}

int test_shutdown(int32_t funcid)
{
    char* req_msg;

    // Get expected conditions and corresponding requirements
    SBI_RETURN_VALUE expected_rc = get_condition_shutdown(&req_msg, funcid);
    SBI_RETURN_VALUE rc = sbicall0(SBI_EXTENSION_SHUTDOWN, funcid);

    // Make sure we get expected return values
    EXPECT_TRUE((expected_rc.error == rc.error) && (expected_rc.value == rc.value), "%s, exp_err: %ld, exp_value: 0x%lx, test_err: %ld, test_value: 0x%lx\n", req_msg, expected_rc.error, expected_rc.value, rc.error, rc.value);

    // notify pass
    TEST_PASSED;
    return 0;
}
