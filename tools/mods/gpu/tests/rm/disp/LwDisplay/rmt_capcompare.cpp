/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_CapabilityCompare.cpp
//! \brief the test inits LwDisplay HW and compares sw and hw display capabilities.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"

extern "C"
{
#include <drf.h>
#include <drf_index.h>
#include <drf_manual.h>
#include <drf_device.h>
#include <drf_register.h>
#include <drf_field.h>
#include <drf_define.h>
}

#include <cstring>
#include "lwmisc.h"

#include "lwdisp_cap_compare_regs.h"

#define DRF_FIELD_VALUE(value, msb, lsb)                        \
    (((value) >> (lsb)) & ((1ULL << ((msb) - (lsb) + 1)) - 1))

#define DRF_FIELD_MASK(msb, lsb)                \
    ((1ULL << ((msb) - (lsb) + 1)) - 1)

#define DRF_LINE_BUFFER_SIZE 1024

using namespace std;

class LwDispCapabilityCompareTest : public RmTest
{
    typedef struct _regcompare_filter
    {
        bool  regFieldValues;
        bool  regFieldDefines;
        bool  regFieldAllValues;
    } regcompare_filter;

public:
    LwDispCapabilityCompareTest();
    virtual ~LwDispCapabilityCompareTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    char* get_field_name( drf_register_t *_register, uint32_t fieldId );
    char* get_define_name( drf_field_t *_field, uint32_t field_value );
    int compare_register_fields(drf_register_t *register1,
                                                 drf_register_t *register2,
                                                 uint32_t          *val1,
                                                 uint32_t          *val2,
                                                 regcompare_filter *pFilter);
    bool skip_field_test(std::string field);
    int initialize_drf(const char **files, drf_state_t **state);

    SETGET_PROP(headerFile, std::string);
    SETGET_PROP(checkRegFieldValueMismatchOnly, bool);
    SETGET_PROP(checkRegFieldDefineMismatch, bool);

private:
    Display *m_pDisplay;
    drf_state_t *m_pDrfState;
    bool m_checkRegFieldValueMismatchOnly;
    bool m_checkRegFieldDefineMismatch;
    std::string m_headerFile;
};

bool LwDispCapabilityCompareTest :: skip_field_test(std::string field)
{
    //
    // Check for the fields that needs to be skipped, for now only LVDS
    // related caps need to be skipped
    //
    if (field.find("LVDS") != std::string::npos)
        return true;
    else
       return false;
}

// Given a register and field ID, return pointer to the field name with a special
// case for registers that contain() fields.
char* LwDispCapabilityCompareTest :: get_field_name( drf_register_t *_register, uint32_t fieldId )
{
    char* str;
    uint32_t n;
    str = strchr( _register->name, '(' );
    n = (uint32_t) (str ? (str - _register->name) : strlen(_register->name) );
    return( &(_register->fields[fieldId]->name[n+1]) );
}

// Given a field and value, return pointer to a symbolic name, or NULL if none found
char* LwDispCapabilityCompareTest :: get_define_name( drf_field_t *_field, uint32_t field_value )
{
    uint32_t define_num, n;
    char* define_name = NULL;
    char* str = NULL;
    drf_define_t *this_define;
    for( define_num = 0 ; define_num < _field->n_defines ; define_num++ ) {
        this_define = _field->defines[define_num];
        if( field_value == this_define->value ) {
            str = strchr( _field->name, '(' );
            n = (uint32_t) (str ? (str - _field->name) : strlen(_field->name) );
            define_name = &(this_define->name[n+1]);
            break;
        }
    }
    return( define_name );
}
//! \brief Compare the two registers
//!
//! Compare based on the register field names if the values match
//------------------------------------------------------------------------------------
int LwDispCapabilityCompareTest :: compare_register_fields
(
    drf_register_t         *register1,
    drf_register_t         *register2,
    uint32_t                 *val1,
    uint32_t                 *val2,
    regcompare_filter *pFilter
)
{
    uint32_t i, j, field_value1, field_value2;
    drf_field_t *field1, *field2;
    drf_register_t *pReg1, *pReg2 = NULL;
    int result = 0;
    LwBool bFieldFound = LW_FALSE;
    char* field_name1, *field_name2;

    pReg1 = register1;
    pReg2 = register2;
    if (pReg1->n_fields != pReg2->n_fields)
    {
        if (pReg1->n_fields < pReg2->n_fields)
        {
            pReg1 = register2;
            pReg2 = register1;
        }
    }

    if (pFilter->regFieldAllValues && val1 && val2)
    {
        Printf (Tee::PriHigh,"\nREG1 = %s (0x%08x)"
                          "\nREG2 = %s (0x%08x)",
                           pReg1->name, *val1, pReg2->name, *val2);
    }

    for (i = 0; i < pReg1->n_fields; i++)
    {
        bFieldFound = LW_FALSE;
        field1 = pReg1->fields[i];
        field_name1 = get_field_name(pReg1, i);
        for (j = 0; j < pReg2->n_fields; j++)
        {
            field2 = pReg2->fields[j];
            field_name2 = get_field_name(pReg2, j);
            if (0 == strcmp(field_name1, field_name2))
            {
                if (skip_field_test(field_name1))
                    continue;

                if ((field1->msb != field2->msb ||
                    field2->lsb != field2->lsb) && pFilter->regFieldDefines)
                {
                    Printf (Tee::PriHigh,"\nREGFIELD DEFINITION(MSB/LSB)"
                              "MISMATCH FOR FIELD %s: %s and %s",
                               field_name1, pReg1->name, pReg2->name);
                    Printf (Tee::PriHigh,"\n    %s %d:%d",
                                       field1->name, field1->msb, field1->lsb);
                    Printf (Tee::PriHigh,"\n    %s %d:%d",
                                       field2->name, field2->msb, field2->lsb);
                    result = -1;
                    continue;
                }
                bFieldFound = LW_TRUE;
                // Compare fields by msb and lsb, expect them to be same
                if (val1 && val2)
                {
                    field_value1 = DRF_FIELD_VALUE(*val1, field1->msb, field1->lsb);
                    field_value2 = DRF_FIELD_VALUE(*val2, field2->msb, field2->lsb);
                     if (pFilter->regFieldAllValues)
                    {
                        Printf (Tee::PriHigh,"\n\t%30s: %3x\t\t%3x",
                                                 field_name1, field_value1, field_value2);
                        if (field_value1 != field_value2)
                        {
                            Printf (Tee::PriHigh," (MISMATCH)");
                            result = -1;
                            break;
                        }
                    }
                    else
                    {
                        if (pFilter->regFieldValues)
                        {
                            if (field_value1 != field_value2)
                            {
                                Printf (Tee::PriHigh,"\nREGFIELD VALUE MISMATCH: ");
                                Printf (Tee::PriHigh,"\n    %s.%s = %x",
                                       pReg1->name, field_name1, field_value1);
                                Printf (Tee::PriHigh,"\n    %s.%s = %x",
                                       pReg2->name, field_name2, field_value2);
                                result = -1;
                            }
                        }
                    }
                    break;
                }
                break;

            } //strcmp
        }

        if (!bFieldFound && pFilter->regFieldDefines)
        {
            Printf (Tee::PriHigh,
                       "\nNO CORRESPONDING REGFIELD FOUND FOR: %s.%s",
                                        pReg1->name, get_field_name(pReg1, i));
            result = -1;
        }
    }

    return result;
}

//! \brief Initialize the manuals
//!
//! Initialize the manuals based on the file-path and return the state
//------------------------------------------------------------------------------
int LwDispCapabilityCompareTest::initialize_drf
(
    const char **files,
    drf_state_t    **state
)
{
    int ret;
    drf_device_t **devices;
    uint32_t n_devices;

    ret = drf_state_alloc(files, state);
    if (ret < 0) {
        switch (drf_errno) {
        case ENOENT:
            Printf (Tee::PriHigh,"No manuals found in the path\n");
            goto failed;
        default:
            Printf (Tee::PriHigh,"Failed to allocate state (%d)\n", drf_errno);
            goto failed;
        }
    }

    ret = drf_index_get_devices(*state, &devices, &n_devices);
    if (ret < 0) {
        drf_state_free(&state);
        Printf (Tee::PriHigh,"Failed to index files (%d)\n", drf_errno);
        goto failed;
    }
failed:
    return ret;
}

LwDispCapabilityCompareTest::LwDispCapabilityCompareTest()
{
    m_pDisplay = GetDisplay();
    SetName("LwDispCapabilityCompareTest");

    m_checkRegFieldValueMismatchOnly = false;
    m_checkRegFieldDefineMismatch = false;
}

//! \brief LwDispCapabilityCompareTest destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
LwDispCapabilityCompareTest::~LwDispCapabilityCompareTest()
{
}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string LwDispCapabilityCompareTest::IsTestSupported()
{
    if (m_pDisplay->GetDisplayClassFamily() != Display::LWDISPLAY)
    {
        return "The test is supported only on LwDisplay class!";
    }

    return RUN_RMTEST_TRUE;
}

//! \setup Initialise internal state from JS
//!
//! Initial state setup based on the JS setting
//------------------------------------------------------------------------------
RC LwDispCapabilityCompareTest::Setup()
{
    RC rc = OK;
    int ret;
    const char *headerfilelist[2] = {NULL,};
    char *headerfile;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    if (m_headerFile == "")
    {
        m_headerFile = std::string("dev_disp.h");
    }

    headerfile = (char*)calloc(m_headerFile.length()+2, sizeof(char));
    if (!headerfile)
        return RC::SOFTWARE_ERROR;
    std::strcpy(headerfile, m_headerFile.c_str());
    headerfilelist[0] = headerfile;

    ret = initialize_drf(headerfilelist, &m_pDrfState);
    if (0 != ret)
    {
        Printf (Tee::PriHigh,"\nFailed to initialize manuals, ");
        return RC::SOFTWARE_ERROR;
    }

    free(headerfile);

    return rc;
}

//! \brief Run the test
//!
//! It will Init LwDisplay HW and compare the HW/SW cap registers
//------------------------------------------------------------------------------
RC LwDispCapabilityCompareTest::Run()
{
    RC rc = OK;
    LwDisplay *pLwDisplay = m_pDisplay->GetLwDisplay();
    LwU32 numCapRegs;
    int ret;
    drf_register_t  **sw_cap_registers, **hw_cap_registers;
    LwU32 regidx, n_sw_registers, n_hw_registers;
    GpuSubdevice *pSubdev = m_pDisplay->GetOwningDisplaySubdevice();
    LwU32    sw_reg_value, hw_reg_value;
    // just compare register value matches and mis-matches
    regcompare_filter regFilter = { false, false, true};

    if (m_checkRegFieldValueMismatchOnly || m_checkRegFieldDefineMismatch)
    {
        regFilter.regFieldAllValues = false; // Force to not print all matches/mismatches
        regFilter.regFieldValues     = m_checkRegFieldValueMismatchOnly;
        regFilter.regFieldDefines   = m_checkRegFieldDefineMismatch;
    }

    // 1) Initialize display HW
    CHECK_RC(pLwDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    // 2) Loop over all the capability registers and compare the fields
    numCapRegs = ((sizeof(capability_reg_fields)/sizeof(capability_reg_fields[0])));
    for (regidx=0; regidx < numCapRegs; regidx++)
    {
        ret = drf_manual_lookup_by_name(m_pDrfState,
                                        capability_reg_fields[regidx].regs[SW_CAP_REGISTER_OFFSET],
                                        &sw_cap_registers,
                                        &n_sw_registers);
        if (ret < 0) {
            switch (drf_errno) {
            case ENOENT:
                //Printf (Tee::PriHigh,"No such register(s) \n");
                rc = RC::SOFTWARE_ERROR;
                goto failed;
            default:
                Printf (Tee::PriHigh,"Failed to look up register(s) \"%s\" (%x).\n",
                        capability_reg_fields[regidx].regs[SW_CAP_REGISTER_OFFSET], drf_errno);
                rc = RC::SOFTWARE_ERROR;
                continue;
            }
        }

        ret = drf_manual_lookup_by_name(m_pDrfState,
                                        capability_reg_fields[regidx].regs[HW_CAP_REGISTER_OFFSET],
                                        &hw_cap_registers,
                                        &n_hw_registers);
        if (ret < 0) {
            switch (drf_errno) {
            case ENOENT:
                //Printf (Tee::PriHigh,"No such register(s) \n");
                rc = RC::SOFTWARE_ERROR;
                goto failed;
            default:
                Printf (Tee::PriHigh,"Failed to look up register(s) \"%s\" (%x).\n",
                        capability_reg_fields[regidx].regs[SW_CAP_REGISTER_OFFSET], drf_errno);
                rc = RC::SOFTWARE_ERROR;
                continue;
            }
        }

        // Read the register values
        sw_reg_value = pSubdev->RegRd32((*sw_cap_registers)->address);
        hw_reg_value = pSubdev->RegRd32((*hw_cap_registers)->address);

        // Mark it as failure and continue for the complete list
        if (compare_register_fields(*sw_cap_registers, *hw_cap_registers,
                                                    &sw_reg_value, &hw_reg_value, &regFilter) != 0)
        {
            rc = RC::SOFTWARE_ERROR;
        }
    }

failed:
    return rc;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC LwDispCapabilityCompareTest::Cleanup()
{
    if (m_pDrfState)
        drf_state_free(m_pDrfState);
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(LwDispCapabilityCompareTest, RmTest,
    "Simple test to check verify Init of Display HW and channel allocation.");

CLASS_PROP_READWRITE(LwDispCapabilityCompareTest, headerFile, std::string,
                "dev_disp.h manual location");
CLASS_PROP_READWRITE(LwDispCapabilityCompareTest, checkRegFieldValueMismatchOnly,bool,
               "will check for register field value mismatches between SW/HW caps and not print matching values");
CLASS_PROP_READWRITE(LwDispCapabilityCompareTest, checkRegFieldDefineMismatch,bool,
              "will check for register field value mismatches between SW/HW caps");
