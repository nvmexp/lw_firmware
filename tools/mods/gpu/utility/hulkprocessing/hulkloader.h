/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include <core/include/rc.h>
#include <memory>

class FalconImpl;

/**
 * An abstract class for loading hulk licences. Implements the template pattern. To implement a hulk 
 * loader for a specific device, create a subclass of HulkLoader and override the protected virtual
 * helper functions. Typically, you want to conditionally compile HulkLoader subclasses based 
 * on the TestDevice that the HulkLoader subclass loads hulk licences for. Refer to the MakeFile 
 * entries for GA100HulkLoader and AmpereHulkLoader for examples. 
 */
class HulkLoader
{
public:
    virtual ~HulkLoader() = default;
    /**
     * Loads the given hulkLicence.
     * 
     * \param hulkLicence The hulk licence binary this function loads
     * 
     * \param ignoreErrors If true, the function will return OK even if the falcon status registers 
     * indicate that the load wasn't successful. Any other errors will still return a not-OK error 
     * code
     */
    RC LoadHulkLicence(const vector<UINT32>& hulkLicence, bool ignoreErrors);
protected:
    /**
     * Exelwted by LoadHulkLicence before any other operation is exelwted. By default, this function 
     * does nothing.
     */
    virtual RC DoSetup() 
    { 
        return RC::OK; 
    };

    /**
     * \return The falcon implementation that LoadHulkLicence uses to load the hulk licence
     */
    virtual std::unique_ptr<FalconImpl> GetFalcon() noexcept  = 0;

    /**
     * \param array output parameter pointing to a pointer to the begining of the C-style array 
     * containing the binary of the falcon that will be used to load the hulk licence passed to the 
     * LoadHulkLicence function.
     * 
     * \param arrayLength output parameter pointing to the length of array
     */
    virtual void GetHulkFalconBinaryArray
    (
        unsigned char const **array, 
        size_t *arrayLength
    ) noexcept  = 0;

    /**
     * 
     * \param progress output parameter for the value of the "progress" register set by the hulk 
     * loading falcon. More information about progress codes can be found here:
     * https://rmopengrok.lwpu.com/source/xref/sw/main/bios/common/cert20/ucode_postcodes.h. 
     * Relevant defines have the prefix "LW_UCODE_POST_CODE_HULK_PROG_"
     * 
     * \param errorCode output parameter for the value of the "errorCode" register set by the hulk
     * loading falcon. More information about error codes can be found at the link above. Error 
     * codes can be found in the enum starting with "LW_UCODE_ERR_CODE_NOERROR"
     */
    virtual void GetLoadStatus(UINT32* progress, UINT32* errorCode) noexcept = 0;
};