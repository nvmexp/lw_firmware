#include "test.h"
#include <lwriscv/sbi.h>
#include <lwriscv/csr.h>
#include <liblwriscv/print.h>

char req_ilwalid_extension_0[] = "REQ: If extension is invalid, return SBI_ERR_NOT_SUPPORTED";

static SBI_RETURN_VALUE get_condition_ilwalid_extension(char** msg, int32_t extension)
{
    SBI_RETURN_VALUE rc;
    rc.error = SBI_ERR_NOT_SUPPORTED;
    rc.value = extension;
    *msg = req_ilwalid_extension_0;

    return rc;
}

int test_ilwalid_extension(int32_t extension, int32_t funcid, uint64_t num_args, uint64_t* param)
{
    TEST_EXPECT(PASS);

    char* req_msg;

    // Get expected conditions and corresponding requirements
    SBI_RETURN_VALUE expected_rc = get_condition_ilwalid_extension(&req_msg, extension);
    SBI_RETURN_VALUE rc;

    switch (num_args)
    {
        case 0:
            rc = sbicall0(extension, funcid);
            break;
        case 1:
            rc = sbicall1(extension, funcid, *param);
            break;
        case 2:
            rc = sbicall2(extension, funcid, *param, *(param+1));
            break;
        case 3:
            rc = sbicall3(extension, funcid, *param, *(param+1), *(param+2));
            break;
        case 4:
            rc = sbicall4(extension, funcid, *param, *(param+1), *(param+2), *(param+3));
            break;
        case 5:
            rc = sbicall5(extension, funcid, *param, *(param+1), *(param+2), *(param+3), *(param+4));
            break;
        case 6:
            rc = sbicall6(extension, funcid, *param, *(param+1), *(param+2), *(param+3), *(param+4), *(param+5));
            break;
    };

    // Make sure we get expected return values
    EXPECT_TRUE((expected_rc.error == rc.error) && (expected_rc.value == rc.value), "%s, exp_err: %ld, exp_value: 0x%lx, test_err: %ld, test_value: 0x%lx\n", req_msg, expected_rc.error, expected_rc.value, rc.error, rc.value);

    // notify pass
    TEST_PASSED;
    return 0;
}
