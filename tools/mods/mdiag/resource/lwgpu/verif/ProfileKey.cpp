/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2014, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <list>
using namespace std;

#include "ProfileKey.h"

#include "core/include/argread.h"
#include "gpu/utility/surf2d.h"
#include "core/include/utility.h"
#include "mdiag/utils/surf_types.h"
#include "mdiag/utils/surfutil.h"
#include "mdiag/utils/IGpuSurfaceMgr.h"

#include "class/cl9097.h"

#include "mdiag/resource/lwgpu/crcchain.h"
#include "mdiag/resource/lwgpu/surfasm.h"
#include "mdiag/utils/lwgpu_classes.h"

#include "VerifUtils.h"
#include "SurfBufInfo.h"

bool ProfileKey::Generate
(
    GpuVerif*            verifier,
    const char*           suffix,      // crc suffix (eg., color or zeta)
    const CheckDmaBuffer* pCheck,      // check dma buffer
    int                   ColorBuffer, // is this for a color buffer?
    bool                  search,      // search the crc chain or grab first element
    UINT32                gpuSubdevice, // subdevice for generating the string
    std::string&          profileKey,  // crc profile key to be generated
    bool                  ignoreLocalLead // ignore local lead crc's
)
{
    CrcChain::const_iterator gpukey;
    UINT32 classnum = CrcChainManager::UNSPECIFIED_CLASSNUM;
    if (pCheck)
    {
        classnum = pCheck->CrcChainClass;
    }

    CrcChainManager* crcChainMgr = verifier->CrcChain_Manager();
    gpukey = crcChainMgr->GetCrcChain(classnum).begin();

    if (gpukey == crcChainMgr->GetCrcChain(classnum).end())
    {
        ErrPrintf("No crc chain defined for this class\n");
        return false;
    }

    //
    // In case of searching crc chain, find a loaded profile for crc chain search.
    // choose one between GpuVerif::profile and GpuVerif::gold_profile.
    // GpuVerif::profile has higher priority than GpuVerif::gold_profile.
    // If key is not found in lead profile, try gold profile then.
    ICrcProfile *profile = NULL, *leadProfile = NULL, *goldProfile = NULL;
    bool gold_profile_checkable = false;

    if (search)
    {
        leadProfile = verifier->Profile();
        goldProfile = verifier->GoldProfile();

        if (leadProfile && leadProfile->Loaded())
        {
            profile = leadProfile;
        }
        if (goldProfile && goldProfile->Loaded())
        {
            if (!profile)
                profile = goldProfile;
            else
                gold_profile_checkable = true;
        }
        if (!profile)
        {
            search = false;
            WarnPrintf("No valid crc profile file for crc chain search. "
                "Crc chain search is disabled\n");
        }
    }

    // init it to what we were asked to do, makes it one pass it we are not searching.
    bool searching = search;

    bool first_pass = true;

    char key[4096];
    string first_key;  // memorize the first key w/o searching

    do
    {
        string tempKey; // start-over

        // Some tests (i2m) do not have CRCs with prepended class names (e.g., gk110).
        // Thus, disable_crc_chain tells us to not include class name in the profile key.
        // See bug 789554.
        if (verifier->Params()->ParamPresent("-disable_crc_chain") == 0)
            tempKey = *gpukey;

        if (tempKey != "")
            tempKey += "_";

        if (pCheck)
        {
            tempKey += pCheck->Filename;

            if (verifier->Params()->ParamPresent("-append_crc_key"))
            {
                tempKey += "_";
                tempKey += verifier->Params()->ParamStr("-append_crc_key");
            }

            if (suffix && *suffix)
                tempKey += "_";
        }
        else
        {
            if (!BuildKeyColorZ(verifier, ColorBuffer, gpuSubdevice, tempKey))
                return false;
        }

        if (suffix != NULL)
            tempKey += suffix;

        strcpy(key, tempKey.c_str());

        if (search)
        {
            if (first_pass)
            {
                first_key = key;
                first_pass = false;
            }

            searching = ignoreLocalLead ?
                    profile->ReadStr("trace_dx5", key) == 0:
                    profile->ReadStrReadAll("trace_dx5", key) == 0;

            if (searching) // couldn't locate the key in the profile
            {
                ++gpukey;  // keep going

                if (gpukey == crcChainMgr->GetCrcChain(classnum).end())
                {
                    if (gold_profile_checkable) // key is not in lead profile, try gold profile
                    {
                        profile = goldProfile;
                        gold_profile_checkable = false;
                        gpukey = crcChainMgr->GetCrcChain(classnum).begin();
                    }
                    else
                    {
                        WarnPrintf("CRC profile doesn't contain a value for "
                                   "key name \"%s\"\n", first_key.c_str());

                        strcpy(key, first_key.c_str());
                        InfoPrintf("Profile is %s\n", key);

                        searching = false; // give up
                    }
                }
            }
            else
            {
                // Check for partial CRC chaining (bug 135760).
                // The gpu key of every profile key in this channel must match.
                // This used to be an error, but now is just a warning (bug 863835).
                string oldGpuKey = verifier->GetGpuKey();

                if (oldGpuKey.empty())
                {
                    verifier->SetGpuKey(*gpukey);
                }
                else if (oldGpuKey != *gpukey && ignoreLocalLead == true)
                {
                    WarnPrintf("Partial CRC chaining detected.  Add all CRC entries to the profile to avoid this warning.\n");
                }
            }
        }
    } while (searching);

    if (search)
        DebugPrintf("Profile key is %s (searching chip chain for crc match)\n", key);
    else
        DebugPrintf("Profile key is %s (grabbing top most dump chip)\n", key);

    profileKey = key;
    return true;
}

void ProfileKey::CleanUpProfile(GpuVerif* verifier, UINT32 gpuSubdevice)
{
    UINT32 i;
    const CrcChain* crcChain = verifier->GetCrcChain();
    CrcChain::const_iterator gpukey = crcChain->begin();

    if (gpukey == crcChain->end())
    {
        ErrPrintf("No dump crc chain defined for this class\n");
        return;
    }

    ICrcProfile* profile = verifier->Profile();
    IGpuSurfaceMgr* surfMgr = verifier->SurfaceManager();
    SurfInfo2* surfZ = verifier->SurfZ();

    UINT32 foundC[MAX_RENDER_TARGETS] = {0x0};
    UINT32 foundC2[MAX_RENDER_TARGETS] = {0x0};
    vector<UINT32> foundBuff;
    UINT32 foundZ = 0x0;
    UINT32 foundZ2 = 0x0;

    vector<SurfInfo2*>& surfaces = verifier->Surfaces();
    for(SurfIt2_t it = surfaces.begin(); it != surfaces.end(); ++it)
    {
        SurfInfo2* info = *it;
        foundC[info->index] = profile->ReadUInt("trace_dx5", info->GetProfKey().c_str(), 0);
    }
    if(verifier->ZSurfOK())
        foundZ = profile->ReadUInt("trace_dx5", verifier->SurfZ()->GetProfKey().c_str(), 0);

    bool buff_exist = true;
    vector<DmaBufferInfo2*>& dmaBufferInfos = verifier->DmaBufferInfos();
    foundBuff.resize(dmaBufferInfos.size(), 0);
    for (i = 0; i < dmaBufferInfos.size(); i++)
    {
        foundBuff[i] = profile->ReadUInt("trace_dx5", dmaBufferInfos[i]->GetProfKey().c_str(), 0);
        buff_exist = (buff_exist && (foundBuff[i] || !dmaBufferInfos[i]->DmaBuffers.pDmaBuf));
    }

    // if all existing buffer CRCs exist for this chip
    if ((foundC[0] || !surfMgr->IsSurfaceValid(SURFACE_TYPE_COLORA, gpuSubdevice)) &&
        (foundC[1] || !surfMgr->IsSurfaceValid(SURFACE_TYPE_COLORB, gpuSubdevice)) &&
        (foundC[2] || !surfMgr->IsSurfaceValid(SURFACE_TYPE_COLORC, gpuSubdevice)) &&
        (foundC[3] || !surfMgr->IsSurfaceValid(SURFACE_TYPE_COLORD, gpuSubdevice)) &&
        (foundC[4] || !surfMgr->IsSurfaceValid(SURFACE_TYPE_COLORE, gpuSubdevice)) &&
        (foundC[5] || !surfMgr->IsSurfaceValid(SURFACE_TYPE_COLORF, gpuSubdevice)) &&
        (foundC[6] || !surfMgr->IsSurfaceValid(SURFACE_TYPE_COLORG, gpuSubdevice)) &&
        (foundC[7] || !surfMgr->IsSurfaceValid(SURFACE_TYPE_COLORH, gpuSubdevice)) &&
        (foundZ    || !surfMgr->IsSurfaceValid(SURFACE_TYPE_Z,      gpuSubdevice)) &&
        buff_exist)
    {
        string key2;

        ++gpukey;
        while (gpukey != crcChain->end())
        {
            SurfIt2_t surfIt = surfaces.begin();
            for(i = 0; surfIt != surfaces.end(); ++i, ++surfIt)
            {
                if (!surfMgr->IsSurfaceValid(ColorSurfaceTypes[i], gpuSubdevice))
                    foundC2[i] = foundC[i];
                else
                {
                    SurfInfo2* info = *surfIt;
                    string::size_type sep = info->GetProfKey().find_first_of("_");
                    if (sep == string::npos)
                    {
                        ErrPrintf("%s is a wrong profile\n", info->GetProfKey().c_str());
                        sep = 0;
                    }
                    key2 = *gpukey + info->GetProfKey().substr(sep, info->GetProfKey().length() - sep);

                    if(key2 != info->GetProfKey())
                        foundC2[i] = profile->ReadUInt("trace_dx5", key2.c_str(), 0);
                }
            }

            if (!surfMgr->IsSurfaceValid(SURFACE_TYPE_Z, gpuSubdevice))
                foundZ2 = foundZ;
            else
            {
                string::size_type sep = surfZ->GetProfKey().find_first_of("_");
                if (sep == string::npos)
                {
                    ErrPrintf("%s is a wrong profile\n", surfZ->GetProfKey().c_str());
                    sep = 0;
                }
                key2 = *gpukey + surfZ->GetProfKey().substr(sep, surfZ->GetProfKey().length() - sep);

                if(key2 != surfZ->GetProfKey())
                    foundZ2 = profile->ReadUInt("trace_dx5", key2.c_str(), 0);
            }

            bool buff_matched = true;
            for (i = 0; i < dmaBufferInfos.size(); i++)
            {
                if (dmaBufferInfos[i]->DmaBuffers.pDmaBuf)
                {
                    string::size_type sep = dmaBufferInfos[i]->GetProfKey().find_first_of("_");
                    if (sep == string::npos)
                    {
                        ErrPrintf("%s is a wrong m_profile\n", dmaBufferInfos[i]->GetProfKey().c_str());
                        sep = 0;
                    }
                    key2 = *gpukey + dmaBufferInfos[i]->GetProfKey().substr(sep, dmaBufferInfos[i]->GetProfKey().length() - sep);
                    buff_matched &= foundBuff[i] == profile->ReadUInt("trace_dx5", key2.c_str(), 0);
                }
            }

            if ((foundC[0] == foundC2[0]) && (foundC[1] == foundC2[1]) &&
                (foundC[2] == foundC2[2]) && (foundC[3] == foundC2[3]) &&
                (foundC[4] == foundC2[4]) && (foundC[5] == foundC2[5]) &&
                (foundC[6] == foundC2[6]) && (foundC[7] == foundC2[7]) &&
                (foundZ == foundZ2) && buff_matched)
            {
                InfoPrintf("Removing crcs due to a match in the chain with gpu: %s\n", gpukey->c_str());
                for(SurfIt2_t it = surfaces.begin(); it != surfaces.end(); ++it)
                    profile->RemoveProfileData("trace_dx5", (*it)->GetProfKey().c_str());
                if(verifier->ZSurfOK())
                    profile->RemoveProfileData("trace_dx5", surfZ->GetProfKey().c_str());

                // reset m_gpukey to avoid partial crc chaining, bug# 321607
                verifier->SetGpuKey(*gpukey);
                return;
            }
            else if ((foundC2[0] || !surfMgr->IsSurfaceValid(SURFACE_TYPE_COLORA, gpuSubdevice)) &&
                     (foundC2[1] || !surfMgr->IsSurfaceValid(SURFACE_TYPE_COLORB, gpuSubdevice)) &&
                     (foundC2[2] || !surfMgr->IsSurfaceValid(SURFACE_TYPE_COLORC, gpuSubdevice)) &&
                     (foundC2[3] || !surfMgr->IsSurfaceValid(SURFACE_TYPE_COLORD, gpuSubdevice)) &&
                     (foundC2[4] || !surfMgr->IsSurfaceValid(SURFACE_TYPE_COLORE, gpuSubdevice)) &&
                     (foundC2[5] || !surfMgr->IsSurfaceValid(SURFACE_TYPE_COLORF, gpuSubdevice)) &&
                     (foundC2[6] || !surfMgr->IsSurfaceValid(SURFACE_TYPE_COLORG, gpuSubdevice)) &&
                     (foundC2[7] || !surfMgr->IsSurfaceValid(SURFACE_TYPE_COLORH, gpuSubdevice)) &&
                     (foundZ2    || !surfMgr->IsSurfaceValid(SURFACE_TYPE_Z, gpuSubdevice)))
            {
                // if we did find a crc value for a chip in the chain which did not match to
                // ours, stop looking, as we only want to remove down the chain as long as they all match
                break;
            }

            ++gpukey;
        }
    }
}

bool ProfileKey::BuildKeyColorZ(GpuVerif* verifier, int ColorBuffer, UINT32 subdev, string& tempKey)
{
    IGpuSurfaceMgr* surfMgr = verifier->SurfaceManager();

    UINT32 width, height, depthC, depthZ;
    MdiagSurf *surfCA = surfMgr->GetSurface(SURFACE_TYPE_COLORA, subdev);
    MdiagSurf *surfZ  = surfMgr->GetSurface(SURFACE_TYPE_Z, subdev);
    MdiagSurf *surfC;
    MdiagSurf *actualSurface;

    // This is a bit of a hack that allows us to figure out if this
    // profile key is really for the Z buffer.  Previously, the ColorBuffer
    // value would be zero whether the key was for ColorA or Z.
    if (ColorBuffer == -1)
    {
        surfC  = surfMgr->GetSurface(ColorSurfaceTypes[0], subdev);
        actualSurface = surfZ;
    }
    else
    {
        surfC  = surfMgr->GetSurface(ColorSurfaceTypes[ColorBuffer], subdev);
        actualSurface = surfC;
    }

    if (verifier->Params()->ParamPresent("-use_min_dim") > 0)
    {
        width  = surfMgr->GetWidth();
        height = surfMgr->GetHeight();
    }
    else
    {
        width  = actualSurface->GetWidth();
        height = actualSurface->GetHeight();
    }

    if (surfC && surfMgr->GetValid(surfC))
        depthC = max(surfC->GetDepth(), surfC->GetArraySize());
    else
        depthC = 1;

    if (surfZ && surfMgr->GetValid(surfZ))
        depthZ = max(surfZ->GetDepth(), surfZ->GetArraySize());
    else
        depthZ = 1;

    bool ProgZtAsCt0 = verifier->Params()->ParamDefined("-prog_zt_as_ct0") &&
                       verifier->Params()->ParamPresent("-prog_zt_as_ct0");

    if (ProgZtAsCt0 ||
        (surfC && surfMgr->GetValid(surfC) &&
         surfZ && surfMgr->GetValid(surfZ)))
    {
        UINT32 cformat = surfMgr->DeviceFormatFromFormat(
                surfC->GetColorFormat());
        UINT32 zformat = surfMgr->DeviceFormatFromFormat(
                surfZ->GetColorFormat());
        tempKey += surfMgr->ColorNameFromColorFormat(cformat);
        tempKey += "-"; // add dash before Z format
        tempKey += surfMgr->ZetaNameFromZetaFormat(zformat);
    }
    else if (surfC && surfMgr->GetValid(surfC))
    {
        UINT32 cformat = surfMgr->DeviceFormatFromFormat(
                surfC->GetColorFormat());
        tempKey += surfMgr->ColorNameFromColorFormat(cformat);
    }
    else if (surfZ && surfMgr->GetValid(surfZ))
    {
        UINT32 zformat = surfMgr->DeviceFormatFromFormat(
                surfZ->GetColorFormat());
        tempKey += surfMgr->ZetaNameFromZetaFormat(zformat);
    }
    tempKey += "_"; // add underscore to end

    if (ProgZtAsCt0)
        tempKey += "ZTCT0_";

    // Add the format of color buffer A if it differs from this color buffer,
    // since this affects alpha test.  See bug 150824 for dislwssion.
    if (surfC && surfCA &&
        ((surfC->GetColorFormat() != surfCA->GetColorFormat()) ||
         (surfMgr->GetForceNull(surfC) != surfMgr->GetForceNull(surfCA))))
    {
        UINT32 cformat = surfMgr->DeviceFormatFromFormat(
                surfCA->GetColorFormat());
        tempKey += "CA-";
        if (!surfMgr->GetForceNull(surfCA))
            tempKey += surfMgr->ColorNameFromColorFormat(cformat);
        else
            tempKey += "DISABLED";
        tempKey += "_";
    }

    bool forceDimInCrcKey = verifier->Params()->ParamPresent("-force_dim_in_crc_key") > 0;
    UINT32 traceArch = verifier->TraceArchClass();
    if (forceDimInCrcKey ||
        (width  != surfMgr->GetTargetDisplayWidth()) ||
        (height != surfMgr->GetTargetDisplayHeight()) ||
        (EngineClasses::IsGpuFamilyClassOrLater(
            traceArch, LWGpuClasses::GPU_CLASS_KEPLER) &&
         GetAAModeFromSurface(actualSurface) != AAMODE_1X1) ||
        (depthC != 1) || (depthZ != 1))
    {
        // Possibilities are:
        // AxB
        // AxBxC
        // AxBxC_Z_AxBxD
        tempKey += Utility::StrPrintf("%dx%d", width, height);
        if ((depthC != 1) || (depthZ != 1))
        {
            tempKey += Utility::StrPrintf("x%d", depthC);
            if (depthC != depthZ)
                tempKey += Utility::StrPrintf("_Z_%dx%dx%d", width, height, depthZ);
        }
        tempKey += "_";
    }

    // set dimension info
    SurfaceAssembler::Rectangle *rect = 0;

    SurfVec2_t& surfaces = verifier->Surfaces();
    SurfIt2_t surfIt = surfaces.begin();
    SurfIt2_t surfItEnd = surfaces.end();

    SurfInfo2* infoSurfZ = verifier->SurfZ();

    if (ColorBuffer == -1 && infoSurfZ)
    {
        rect = &infoSurfZ->rect;
    }
    else
    {
        for (; surfIt != surfItEnd; ++surfIt)
        {
            if ((*surfIt)->GetSurface() == actualSurface)
            {
                rect = &((*surfIt)->rect);
                break;
            }
        }
    }

    if (rect && rect->width > 0 && rect->height > 0 &&
        (verifier->Params()->ParamPresent("-use_min_dim") == 0))
    {
        // -crc_rectX/CRC_RECT is used, extra info in key is:
        // x-y-w-h
        tempKey += Utility::StrPrintf("rect%d-%d-%d-%d",
            rect->x, rect->y, rect->width, rect->height);
        tempKey += "_";
    }

    switch (GetAAModeFromSurface(actualSurface))
    {
        case AAMODE_1X1:       break;
        case AAMODE_2X1_D3D:
            if( verifier->Params()->ParamPresent("-samplemask_d3d_swizzle")>0 )
            {
                tempKey += "2X1_";
            }
            else
            {
                tempKey += "2X1_D3D_";
            }
            break;
        case AAMODE_4X2_D3D:
            if( verifier->Params()->ParamPresent("-samplemask_d3d_swizzle")>0 )
            {
                tempKey += "4X2_";
            }
            else
            {
                tempKey += "4X2_D3D_";
            }
            break;
        case AAMODE_2X1:       tempKey += "2X1_";       break;
        case AAMODE_2X2:       tempKey += "2X2_";       break;
        case AAMODE_4X2:       tempKey += "4X2_";       break;
        case AAMODE_4X4:       tempKey += "4X4_";       break;
        case AAMODE_2X2_VC_4:  tempKey += "2X2_VC_4_";  break;
        case AAMODE_2X2_VC_12: tempKey += "2X2_VC_12_"; break;
        case AAMODE_4X2_VC_8:
            if( verifier->Params()->ParamPresent("-samplemask_4x2_vc8_swizzle") > 0 )
            {
                tempKey += "4X2_VC_8_swizzle_";
            }
            else
            {
                tempKey += "4X2_VC_8_";
            }
            break;
        case AAMODE_4X2_VC_24: tempKey += "4X2_VC_24_"; break;
        default:
            ErrPrintf("Unknown AAmode %d\n",
                GetAAModeFromSurface(actualSurface));
            return false;
    }

    if (verifier->Params()->ParamDefined("-ForceVisible") &&
        verifier->Params()->ParamPresent("-ForceVisible"))
        tempKey += "FV_";

    if ((verifier->Params()->ParamDefined("-visible_early_z") &&
         verifier->Params()->ParamPresent("-visible_early_z") > 0) ||
        (verifier->Params()->ParamDefined("-visible_early_z_nobroadcast") &&
         verifier->Params()->ParamPresent("-visible_early_z_nobroadcast") > 0))
    {
        // Tesla and Fermi has different values defined for the latez etc
        UINT32 EarlyZHysteresis =
            verifier->Params()->ParamUnsigned("-early_z_hysteresis",
                LW9097_SET_OPPORTUNISTIC_EARLY_Z_HYSTERESIS_ACLWMULATED_PRIM_AREA_THRESHOLD_LATEZ_ALWAYS);
        switch (EarlyZHysteresis)
        {
            case LW9097_SET_OPPORTUNISTIC_EARLY_Z_HYSTERESIS_ACLWMULATED_PRIM_AREA_THRESHOLD_INSTANTANEOUS:
                tempKey += "VISEARLYZ-INST_";
                break;
            case LW9097_SET_OPPORTUNISTIC_EARLY_Z_HYSTERESIS_ACLWMULATED_PRIM_AREA_THRESHOLD_LATEZ_ALWAYS:
                tempKey += "VISEARLYZ-LATEZ_";
                break;
            default:
                tempKey += Utility::StrPrintf("VISEARLYZ-0x%X_",
                    DRF_VAL(9097, _SET_OPPORTUNISTIC_EARLY_Z_HYSTERESIS, _ACLWMULATED_PRIM_AREA_THRESHOLD,
                    EarlyZHysteresis));
                break;
        }
    }

    if (surfMgr->GetMultiSampleOverride())
    {
        if (surfMgr->GetMultiSampleOverrideValue())
        {
            // -MSenable is default for all but 1X1, no CRC key there
            if (!surfMgr->GetAAUsed())
                tempKey += "MSenable_";
        }
        else
        {
            if (surfMgr->GetNonMultisampledZOverride())
                tempKey += "MSdisableZ_";
            else
                tempKey += "MSdisable_";
        }
    }
    else
    {
        // -MSfreedom is default for 1X1, no CRC key there
        if (surfMgr->GetAAUsed())
            tempKey += "MSfreedom_";
    }

    _RAWMODE rawImageMode = verifier->RawImageMode();
    if (rawImageMode != RAWOFF)
    {
        assert(rawImageMode == RAWON);
        tempKey += "RawImages_";
    }

    if ((surfMgr->IsZlwllEnabled() || surfMgr->IsSlwllEnabled()) &&
        verifier->Params()->ParamPresent("-ZLwllMod"))
    {
        tempKey += "ZLWLLMOD_";
        if (surfMgr->IsZlwllEnabled() && surfMgr->IsSlwllEnabled())
            tempKey += "ZSO_";
        else if (surfMgr->IsZlwllEnabled())
            tempKey += "ZO_";
        else
            tempKey += "SO_";
    }

    if (verifier->Params()->ParamDefined("-blendreduce") &&
        verifier->Params()->ParamPresent("-blendreduce"))
    {
        bool vcaa_mode;

        switch (actualSurface->GetAAMode())
        {
            case Surface2D::AA_4_Virtual_8:
            case Surface2D::AA_4_Virtual_16:
            case Surface2D::AA_8_Virtual_16:
            case Surface2D::AA_8_Virtual_32:
                vcaa_mode = true;
                break;
            default:
                vcaa_mode = false;
        }

        if( !vcaa_mode ||
            !verifier->Params()->ParamPresent("-vcaa_override_blendreduce_crc") )
        {
            tempKey += "BLENDREDUCE_";
        }
    }

    if (verifier->Params()->ParamDefined("-blend_zero_times_x_is_zero") &&
        verifier->Params()->ParamPresent("-blend_zero_times_x_is_zero"))
        tempKey += "BLENDZEROTIMESX_";

    if (verifier->Params()->ParamDefined("-blend_allow_float_pixel_kills") &&
        verifier->Params()->ParamPresent("-blend_allow_float_pixel_kills"))
        tempKey += "BLENDALLOWFLOATPIXELKILLS_";

    if (verifier->Params()->ParamDefined("-srgb_write") &&
        verifier->Params()->ParamPresent("-srgb_write"))
    {
        // Only add this to the CRC key if the color format supports it
        if (surfMgr->GetRaster(surfC->GetColorFormat())->SupportsSrgbWrite())
            tempKey += "SRGBWRITE_";
    }

    // Only add woffset to the CRC key if one of them is nonzero
    if (surfMgr->GetUnscaledWindowOffsetX() ||
        surfMgr->GetUnscaledWindowOffsetY())
    {
        if (verifier->Params()->ParamPresent("-woffset_mod"))
        {
            tempKey += Utility::StrPrintf("WO%dx%d_",
                surfMgr->GetUnscaledWindowOffsetX(),
                surfMgr->GetUnscaledWindowOffsetY());
        }
    }

    if (verifier->Params()->ParamDefined("-yilw") &&
        verifier->Params()->ParamPresent("-yilw"))
    {
        if (verifier->Params()->ParamPresent("-yilw_mod"))
            tempKey += "YILW_";
    }

    if (verifier->Params()->ParamDefined("-append_crc_key") &&
        verifier->Params()->ParamPresent("-append_crc_key"))
    {
        tempKey+=verifier->Params()->ParamStr("-append_crc_key");
        tempKey+="_";
    }

    if (verifier->Params()->ParamDefined("-set_aa_alpha_ctrl_coverage") &&
        verifier->Params()->ParamPresent("-set_aa_alpha_ctrl_coverage"))
    {
        if (verifier->Params()->ParamUnsigned("-set_aa_alpha_ctrl_coverage"))
        {
            tempKey+= "AA-ALPHA-COVERAGE-1_";
        }
        else
        {
            tempKey+= "AA-ALPHA-COVERAGE-0_";
        }
    }

    // bug# 322213, -hybrid_centroid and -hybrid_passes args for per-sample shading
    if ((verifier->Params()->ParamPresent("-hybrid_passes") > 0) &&
        (verifier->Params()->ParamUnsigned("-hybrid_passes") > 1) &&
        !verifier->Params()->ParamPresent("-hybrid_skip_crc"))
    {
        tempKey += "HYBRID_PASSES_";

        char buf[1024];
        sprintf(buf, "%d", verifier->Params()->ParamUnsigned("-hybrid_passes"));
        tempKey += buf;

        if ((verifier->Params()->ParamPresent("-hybrid_centroid")) &&
            (verifier->Params()->ParamUnsigned("-hybrid_centroid") == 1))
        {
            tempKey += "_PER_PASS_";
        }
        else
        {
            tempKey += "_PER_FRAGMENT_";
        }
    }

    // bug# 334610, additional args for gt214_tesla class
    if ((verifier->Params()->ParamPresent("-alpha_coverage_aa") > 0) ||
        (verifier->Params()->ParamPresent("-alpha_coverage_psmask") > 0))
    {
        tempKey += "ALPHA_TO_COVERAGE_OVERRIDE_";
        if ((verifier->Params()->ParamPresent("-alpha_coverage_aa") > 0))
        {
            if (verifier->Params()->ParamUnsigned("-alpha_coverage_aa") == 0)
                tempKey += "AA_DISABLE_";
            else
                tempKey += "AA_ENABLE_";
        }

        if ((verifier->Params()->ParamPresent("-alpha_coverage_psmask") > 0))
        {
            if (verifier->Params()->ParamUnsigned("-alpha_coverage_psmask") == 0)
                tempKey += "PSMASK_DISABLE_";
            else
                tempKey += "PSMASK_ENABLE_";
        }
    }

    // bug# 334610, additional args for gt214_tesla class
    if (verifier->Params()->ParamPresent("-blend_s8u16s16") > 0)
    {
        if (verifier->Params()->ParamUnsigned("-blend_s8u16s16") == 1)
            tempKey += "BLEND_S8U16S16_TRUE_";
        else
            tempKey += "BLEND_S8U16S16_FALSE_";
    }

    // bug# 334610, additional args for gt214_tesla class
    if (verifier->Params()->ParamPresent("-fp32_blend") > 0)
    {
        if (verifier->Params()->ParamUnsigned("-fp32_blend") == 1)
            tempKey += "FP32_BLEND_RTNE_";
        else
            tempKey += "FP32_BLEND_TRUNCATION_";
    }

    // bug# 334610, additional args for gt214_tesla class
    if (verifier->Params()->ParamPresent("-vcaa_smask") > 0 &&
        !verifier->Params()->ParamPresent("-vcaa_smask_skip_crc"))
    {
        UINT32 argVal = verifier->Params()->ParamUnsigned("-vcaa_smask");
        switch (argVal)
        {
            case 0:
                tempKey += "VCAA_SMASK_AOETE_";
                break;
            case 1:
                tempKey += "VCAA_SMASK_CB_";
                break;
            case 2:
                tempKey += "VCAA_SMASK_FVCR_";
                break;
            default:
                ErrPrintf("Invalid argument for \"-vcaa_smask\"\n");
                return false;
        }
    }

    // bug# 334610, additional args for gt214_tesla class
    if (verifier->Params()->ParamPresent("-ps_smask_aa") > 0)
    {
        if (verifier->Params()->ParamUnsigned("-ps_smask_aa") == 1)
            tempKey += "PS_SMASK_AA_ENABLE_";
        else
            tempKey += "PS_SMASK_AA_DISABLE_";
    }

    // bug# 351846. class methods to control AlphaToCoverage dithering
    if (verifier->Params()->ParamPresent("-alpha_coverage_dither") > 0)
    {
        switch (verifier->Params()->ParamUnsigned("-alpha_coverage_dither"))
        {
            case 0:
                tempKey += "ALPHA_COVERAGE_DITHER_1X1_";
                break;
            case 1:
                tempKey += "ALPHA_COVERAGE_DITHER_2X2_";
                break;
            case 2:
                tempKey += "ALPHA_COVERAGE_DITHER_1X1_VIRTUAL_";
                break;
            default:
                MASSERT(!"Unsupported -alpha_coverage_dither value");
        }
    }

    // bug# 357796, reduce threshold options
    if (verifier->Params()->ParamPresent("-reduce_thresh_u8") > 0)
    {
        UINT32 aggrVal = verifier->Params()->ParamNUnsigned("-reduce_thresh_u8", 0);
        UINT32 consVal = verifier->Params()->ParamNUnsigned("-reduce_thresh_u8", 1);

        char buf[64];
        sprintf(buf, "RTHRESHU8_%d_%d_", aggrVal, consVal);
        tempKey += buf;
    }

    if (verifier->Params()->ParamPresent("-reduce_thresh_fp16") > 0)
    {
        UINT32 aggrVal = verifier->Params()->ParamNUnsigned("-reduce_thresh_fp16", 0);
        UINT32 consVal = verifier->Params()->ParamNUnsigned("-reduce_thresh_fp16", 1);

        char buf[64];
        sprintf(buf, "RTHRESHFP16_%d_%d_", aggrVal, consVal);
        tempKey += buf;
    }

    if (verifier->Params()->ParamPresent("-reduce_thresh_u10") > 0)
    {
        UINT32 aggrVal = verifier->Params()->ParamNUnsigned("-reduce_thresh_u10", 0);
        UINT32 consVal = verifier->Params()->ParamNUnsigned("-reduce_thresh_u10", 1);

        char buf[64];
        sprintf(buf, "RTHRESHU10_%d_%d_", aggrVal, consVal);
        tempKey += buf;
    }

    if (verifier->Params()->ParamPresent("-reduce_thresh_u16") > 0)
    {
        UINT32 aggrVal = verifier->Params()->ParamNUnsigned("-reduce_thresh_u16", 0);
        UINT32 consVal = verifier->Params()->ParamNUnsigned("-reduce_thresh_u16", 1);

        char buf[64];
        sprintf(buf, "RTHRESHU16_%d_%d_", aggrVal, consVal);
        tempKey += buf;
    }

    if (verifier->Params()->ParamPresent("-reduce_thresh_fp11") > 0)
    {
        UINT32 aggrVal = verifier->Params()->ParamNUnsigned("-reduce_thresh_fp11", 0);
        UINT32 consVal = verifier->Params()->ParamNUnsigned("-reduce_thresh_fp11", 1);

        char buf[64];
        sprintf(buf, "RTHRESHFP11_%d_%d_", aggrVal, consVal);
        tempKey += buf;
    }

    if (verifier->Params()->ParamPresent("-reduce_thresh_srgb8") > 0)
    {
        UINT32 aggrVal = verifier->Params()->ParamNUnsigned("-reduce_thresh_srgb8", 0);
        UINT32 consVal = verifier->Params()->ParamNUnsigned("-reduce_thresh_srgb8", 1);

        char buf[64];
        sprintf(buf, "RTHRESHSRGB8_%d_%d_", aggrVal, consVal);
        tempKey += buf;
    }

    // bug 357946, options to control SetViewportClipControl
    if ((verifier->Params()->ParamPresent("-geom_clip") > 0) ||
        (verifier->Params()->ParamPresent("-geometry_clip") > 0) ||
        (verifier->Params()->ParamPresent("-pixel_min_z") > 0) ||
        (verifier->Params()->ParamPresent("-pixel_max_z") > 0))
    {
        if (verifier->Params()->ParamPresent("-pixel_min_z") > 0)
        {
            if (verifier->Params()->ParamUnsigned("-pixel_min_z") == 0)
                tempKey += "PIXEL_MIN_Z_CLIP_";
            else
                tempKey += "PIXEL_MIN_Z_CLAMP_";
        }

        if (verifier->Params()->ParamPresent("-pixel_max_z") > 0)
        {
            if (verifier->Params()->ParamUnsigned("-pixel_max_z") == 0)
                tempKey += "PIXEL_MAX_Z_CLIP_";
            else
                tempKey += "PIXEL_MAX_Z_CLAMP_";
        }

        if (verifier->Params()->ParamPresent("-geom_clip") > 0)
        {
            switch(verifier->Params()->ParamUnsigned("-geom_clip"))
            {
            case 0:
                tempKey += "GEOM_CLIP_WZERO_CLIP_";
                break;
            case 1:
                tempKey += "GEOM_CLIP_PASSTHRU_";
                break;
            case 2:
                tempKey += "GEOM_CLIP_FRUSTUM_XY_CLIP_";
                break;
            case 3:
                tempKey += "GEOM_CLIP_FRUSTUM_XYZ_CLIP_";
                break;
            case 4:
                tempKey += "GEOM_CLIP_WZERO_CLIP_NO_ZLWLL_";
                break;
            case 5:
                tempKey += "GEOM_CLIP_FRUSTUM_Z_CLIP_";
                break;
            }
        }

        // legacy way to specify geometry clip
        if (verifier->Params()->ParamPresent("-geometry_clip") > 0)
        {
            switch(verifier->Params()->ParamUnsigned("-geometry_clip"))
            {
            case 0:
                tempKey += "GEOM_CLIP_WZERO_CLIP_";
                break;
            case 1:
                tempKey += "GEOM_CLIP_PASSTHRU_";
                break;
            case 2:
                tempKey += "GEOM_CLIP_FRUSTUM_XY_CLIP_";
                break;
            case 3:
                tempKey += "GEOM_CLIP_FRUSTUM_XYZ_CLIP_";
                break;
            case 4:
                tempKey += "GEOM_CLIP_WZERO_CLIP_NO_ZLWLL_";
                break;
            case 5:
                tempKey += "GEOM_CLIP_FRUSTUM_Z_CLIP_";
                break;
            }
        }
    }

    // If any -map_region command-line arguments were set for this
    // surface then extra information needs to be added to the
    // CRC profile key as the image drawn will be different due to
    // the partial mapping of the surface.
    if (surfMgr->IsPartiallyMapped(actualSurface) &&
        (verifier->Params()->ParamPresent("-inflate_rendertarget_and_offset_window") == 0 && verifier->Params()->ParamPresent("-inflate_rendertarget_and_offset_viewport") == 0))
    {
        tempKey += surfMgr->GetPartialMapCrcString(actualSurface);
    }

    return true;
}

