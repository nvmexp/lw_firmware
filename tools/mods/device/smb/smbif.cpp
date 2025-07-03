/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2007,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "smbdev.h"
#include "smbmgr.h"
#include "smbport.h"
#include "core/include/tasker.h"

MCPDEV_EXPOSE_TO_JS(Smb,"Smbus device");
MCP_SUBDEV_EXPOSE_TO_JS(Smb, Port);

MCP_JSCLASS_TEST(Smb,Reset, 0, "Reset Smbus Controller");

//TODO:: Need to be fixed after Search function gets implemented as per Bug ID: 410442
//MCP_JSCLASS_TEST(Smb,Search,1,"Search for Smbus slave devices on the system. Prints out addresses. If array argument is passed, fills it with slave addresses.");
MCP_JSCLASS_TEST(Smb,Populate,1,"Search for Smbus slave devices on the system. Prints out addresses. If array argument is passed, fills it with slave addresses.");
//------------------------------------- Commands -----------------------------
MCP_JSCLASS_TEST(Smb, WrQuickCmd, 1, "Write Quick Command")
MCP_JSCLASS_TEST(Smb, RdQuickCmd, 1, "Read Quick Command")
MCP_JSCLASS_TEST(Smb, SendByteCmd, 2, "Send Byte Command")
MCP_JSCLASS_METHOD(Smb, RecvByteCmd, 2, "Recive Byte Command")
MCP_JSCLASS_TEST(Smb, WriteByteCmd, 3, "Write Byte Command")
MCP_JSCLASS_METHOD(Smb, ReadByteCmd, 2, "Read Byte Command")
MCP_JSCLASS_TEST(Smb, WriteWordCmd, 3, "Write Word Command")
MCP_JSCLASS_METHOD(Smb, ReadWordCmd, 2, "Read Word Command")
MCP_JSCLASS_TEST(Smb, WriteBlkCmd, 3, "Write Block Command")
MCP_JSCLASS_TEST(Smb, ReadBlkCmd, 3, "Read Block Command")
MCP_JSCLASS_TEST(Smb, WriteBlk, 3, "Write Block via multiple byte Write")
MCP_JSCLASS_TEST(Smb, ReadBlk, 4, "Read Block via multiple byte Read")
MCP_JSCLASS_METHOD(Smb, ProcessCallCmd, 3, "ProcessCall Command")
MCP_JSCLASS_TEST(Smb, HostNotify, 2, "Host Notification")
MCP_JSCLASS_TEST(Smb, IsSlavePresent, 1, "Presence of slave address")

//!This method is used inside, not exposed.
RC GetLwrrentPort(SmbPort** ppPort)
{
    RC rc;
    SmbDevice *pDev = NULL;
    CHECK_RC(SmbMgr::Mgr()->GetLwrrent((McpDev**)&pDev));
    CHECK_RC(pDev->GetLwrrentSubDev(ppPort));
    return OK;
}

C_(Smb_Reset)
{
    RC rc;
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Smb.Reset()");
        return JS_FALSE;
    }

    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));

    RETURN_RC ( pPort->ResetCtrlReg());
}

// -------------------------------- Commands ------------------------------//

C_(Smb_WrQuickCmd)
{
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, "Usage: Smbus.WrQuickCmd(SlvAdrs)");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 SlvAdrs;

    if (OK != pJavaScript->FromJsval(pArguments[0], &SlvAdrs))
    {
        JS_ReportError(pContext, "SlvAdrs bad value.");
        return JS_FALSE;
    }

    RC rc;
    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));
    RETURN_RC(pPort->WrQuickCmd((UINT16)SlvAdrs));
}

C_(Smb_RdQuickCmd)
{
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, "Usage: Smb.RdQuickCmd(SlvAdrs)");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 SlvAdrs;

    if (OK != pJavaScript->FromJsval(pArguments[0], &SlvAdrs))
    {
        JS_ReportError(pContext, "SlvAdrs bad value.");
        return JS_FALSE;
    }

    RC rc;
    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));
    RETURN_RC(pPort->RdQuickCmd((UINT16)SlvAdrs));
}

C_(Smb_SendByteCmd)
{
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if (NumArguments != 2)
    {
        JS_ReportError(pContext, "Usage: Smb.SendByteCmd(SlvAdrs, Data)");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 SlvAdrs, Data;

    if (OK != pJavaScript->FromJsval(pArguments[0], &SlvAdrs))
    {
        JS_ReportError(pContext, "SlavAdrs bad value.");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[1], &Data))
    {
        JS_ReportError(pContext, "Data bad value.");
        return JS_FALSE;
    }

    RC rc;
    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));
    RETURN_RC(pPort->SendByteCmd((UINT16)SlvAdrs, (UINT08)Data));
}

C_(Smb_RecvByteCmd)
{
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, "Usage: Smb.RecvByteCmd(SlvAdrs)");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 SlvAdrs;

    if (OK != pJavaScript->FromJsval(pArguments[0], &SlvAdrs))
    {
        JS_ReportError(pContext, "SlavAdrs bad value.");
        return JS_FALSE;
    }

    RC rc;
    UINT08 Data = 0;
    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));

    if ( OK != (rc = pPort->RecvByteCmd((UINT16)SlvAdrs, &Data)))
    {
        pJavaScript->ToJsval(0xdeadbeef, pReturlwalue);
        return JS_FALSE;
    }
    else
    {
        if (OK != pJavaScript->ToJsval((UINT32)Data, pReturlwalue))
        {
            JS_ReportError(pContext, "Error oclwrred in Smb.RecvByteCmd()");
            *pReturlwalue = JSVAL_NULL;
            return JS_FALSE;
        }
    }

    return JS_TRUE;
}

C_(Smb_WriteByteCmd)
{
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if (NumArguments != 3)
    {
        JS_ReportError(pContext, "Usage: Smb.WriteByteCmd(SlvAdrs, CmdCode, Data)");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 SlvAdrs, CmdCode, Data;

    if (OK != pJavaScript->FromJsval(pArguments[0], &SlvAdrs))
    {
        JS_ReportError(pContext, "SlavAdrs bad value.");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[1], &CmdCode))
    {
        JS_ReportError(pContext, "CmdCode bad value.");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[2], &Data))
    {
        JS_ReportError(pContext, "Data bad value.");
        return JS_FALSE;
    }

    RC rc;
    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));
    RETURN_RC(pPort->WrByteCmd((UINT16)SlvAdrs,(UINT08)CmdCode, (UINT08)Data));
}

C_(Smb_ReadByteCmd)
{
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if (NumArguments != 2)
    {
        JS_ReportError(pContext, "Usage: Smb.ReadByteCmd(SlvAdrs, CmdCode)");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 SlvAdrs, CmdCode;

    if (OK != pJavaScript->FromJsval(pArguments[0], &SlvAdrs))
    {
        JS_ReportError(pContext, "SlavAdrs bad value.");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[1], &CmdCode))
    {
        JS_ReportError(pContext, "CmdCode bad value.");
        return JS_FALSE;
    }

    RC rc;
    UINT08 Data = 0;
    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));

    if ( OK != (rc = pPort->RdByteCmd((UINT16)SlvAdrs,(UINT08)CmdCode, &Data )))
    {
        Printf(Tee::PriNormal, "0xdeadbeef\n");
        pJavaScript->ToJsval(0xdeadbeef, pReturlwalue);
        return JS_FALSE;
    }
    else
    {
        Printf(Tee::PriDebug, "Read: 0x%02x\n", Data);

        if (OK != pJavaScript->ToJsval((UINT32)Data, pReturlwalue))
        {
            JS_ReportError(pContext, "Error oclwrred in Smbus.ReadByteCmd()");
            *pReturlwalue = JSVAL_NULL;
            return JS_FALSE;
        }
    }

    return JS_TRUE;
}

C_(Smb_WriteWordCmd)
{
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if (NumArguments != 3)
    {
        JS_ReportError(pContext, "Usage: Smb.WriteWordCmd(SlvAdrs, CmdCode, Data)");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 SlvAdrs, CmdCode, Data;

    if (OK != pJavaScript->FromJsval(pArguments[0], &SlvAdrs))
    {
        JS_ReportError(pContext, "SlavAdrs bad value.");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[1], &CmdCode))
    {
        JS_ReportError(pContext, "CmdCode bad value.");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[2], &Data))
    {
        JS_ReportError(pContext, "Data bad value.");
        return JS_FALSE;
    }

    RC rc;
    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));
    RETURN_RC( pPort->WrWordCmd((UINT16)SlvAdrs,(UINT08)CmdCode,(UINT16)Data));
}

C_(Smb_ReadWordCmd)
{
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if (NumArguments != 2)
    {
        JS_ReportError(pContext, "Usage: Smb.ReadWordCmd(SlvAdrs, CmdCode)");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 SlvAdrs, CmdCode;

    if (OK != pJavaScript->FromJsval(pArguments[0], &SlvAdrs))
    {
        JS_ReportError(pContext, "SlavAdrs bad value.");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[1], &CmdCode))
    {
        JS_ReportError(pContext, "CmdCode bad value.");
        return JS_FALSE;
    }

    RC rc;
    UINT16 Data = 0;
    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));

    if ( OK != (rc = pPort->RdWordCmd((UINT16)SlvAdrs,(UINT08)CmdCode, &Data)))
    {
        Printf(Tee::PriNormal, "0xdeadbeef\n");
        pJavaScript->ToJsval(0xdeadbeef, pReturlwalue);
        return JS_FALSE;
    }
    else
    {
        Printf(Tee::PriDebug, "Read: 0x%04x\n", Data);
        if (OK != pJavaScript->ToJsval((UINT32)Data, pReturlwalue))
        {
            JS_ReportError(pContext, "Error oclwrred in Smb.ReadWordCmd()");
            *pReturlwalue = JSVAL_NULL;
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

C_(Smb_WriteBlkCmd)
{
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if (NumArguments != 3)
    {
        JS_ReportError(pContext, "Usage: Smb.WriteBlkCmd(SlvAdrs, CmdCode, [Data])");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 SlvAdrs, CmdCode;
    JsArray  jsData;

    if (OK != pJavaScript->FromJsval(pArguments[0], &SlvAdrs))
    {
        JS_ReportError(pContext, "SlavAdrs bad value.");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[1], &CmdCode))
    {
        JS_ReportError(pContext, "CmdCode bad value.");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[2], &jsData))
    {
        JS_ReportError(pContext, "Data bad value.");
        return JS_FALSE;
    }

    // copy the data from array to vector
    vector<UINT08> vData(jsData.size());

    UINT32 dataToTruncate;
    for (UINT32 i = 0; i< jsData.size(); i++)
    {
        if (OK != pJavaScript->FromJsval((jsData[i]), &dataToTruncate))
        {
            JS_ReportError(pContext, "data element bad value.");
            return JS_FALSE;
        }

        vData[i] = (UINT08)dataToTruncate;
    }

    RC rc;
    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));
    RETURN_RC(pPort->WrBlkCmd((UINT16)SlvAdrs,(UINT08)CmdCode, &vData));
}

C_(Smb_ReadBlk)
{
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if (NumArguments != 4)
    {
        JS_ReportError(pContext, "Usage: Smb.ReadBlk(SlvAdrs, CmdCode, Bytes, [data])");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 SlvAdrs, CmdCode, Bytes;
    JSObject *jsData;

    if (OK != pJavaScript->FromJsval(pArguments[0], &SlvAdrs))
    {
        JS_ReportError(pContext, "SlavAdrs bad value.");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[1], &CmdCode))
    {
        JS_ReportError(pContext, "CmdCode bad value.");
        return JS_FALSE;
    }

    if (OK != pJavaScript->FromJsval(pArguments[2], &Bytes))
    {
        JS_ReportError(pContext, "Bytes bad value.");
        return JS_FALSE;
    }

    if (OK != pJavaScript->FromJsval(pArguments[3], &jsData))
    {
        JS_ReportError(pContext, "data array bad value.");
        return JS_FALSE;
    }

    vector<UINT08>  vData08;
    RC rc;
    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));

    for (UINT32 i = 0; i<Bytes; ++i)
    {
        if ( OK != (rc = pPort->RdByteCmd((UINT16)SlvAdrs,(UINT08)CmdCode+i,&(vData08[i]))))
        {
            break;
        }
    }

    for (UINT32 i = 0; i < Bytes; i++)
    {
        if (OK != (rc = pJavaScript->SetElement(jsData, i, vData08[i])))
        {
            JS_ReportError(pContext, "ReadBlkCmd: Set Element Error.");
            return JS_FALSE;
        }
    }

    for (UINT32 i = 0; i < vData08.size(); i++)
        Printf(Tee::PriDebug, "0x%02x ", vData08[i]);

    return JS_TRUE;
}

C_(Smb_WriteBlk)
{
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if (NumArguments != 3)
    {
        JS_ReportError(pContext, "Usage: Smb.WriteBlk(SlvAdrs, CmdCode, [Data])");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 SlvAdrs, CmdCode;
    JsArray  jsData;

    if (OK != pJavaScript->FromJsval(pArguments[0], &SlvAdrs))
    {
        JS_ReportError(pContext, "SlavAdrs bad value.");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[1], &CmdCode))
    {
        JS_ReportError(pContext, "CmdCode bad value.");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[2], &jsData))
    {
        JS_ReportError(pContext, "Data bad value.");
        return JS_FALSE;
    }

    // copy the data from array to vector
    RC rc;
    UINT32 dataToTruncate;
    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));

    for (UINT32 i = 0; i< jsData.size(); i++)
    {
        if (OK != pJavaScript->FromJsval((jsData[i]), &dataToTruncate))
        {
            JS_ReportError(pContext, "data element bad value.");
            return JS_FALSE;
        }

        if ( OK != ( rc = pPort->WrByteCmd((UINT16)SlvAdrs,(UINT08)CmdCode+i, (UINT08)(dataToTruncate&0xff))))

        RETURN_RC(rc);
        Tasker::Sleep(10);
    }

    RETURN_RC(OK);
}

C_(Smb_ReadBlkCmd)
{
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if (NumArguments != 3)
    {
        JS_ReportError(pContext, "Usage: Smb.ReadBlkCmd(SlvAdrs, CmdCode, [data])");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 SlvAdrs, CmdCode;
    JSObject *jsData;

    if (OK != pJavaScript->FromJsval(pArguments[0], &SlvAdrs))
    {
        JS_ReportError(pContext, "SlavAdrs bad value.");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[1], &CmdCode))
    {
        JS_ReportError(pContext, "CmdCode bad value.");
        return JS_FALSE;
    }

    if (OK != pJavaScript->FromJsval(pArguments[2], &jsData))
    {
        JS_ReportError(pContext, "data array bad value.");
        return JS_FALSE;
    }

    vector<UINT08> vData;

    RC rc;
    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));

    if ( OK != (rc = pPort->RdBlkCmd((UINT16)SlvAdrs,(UINT08)CmdCode,&vData)))
        RETURN_RC(rc);

    UINT32 Size = vData.size();
    if (Size == 0)
        Printf(Tee::PriNormal, "No Data read back.\n");

    for (UINT32 i = 0; i < Size; i++)
    {
        if (OK != (rc = pJavaScript->SetElement(jsData, i, vData[i])))
        {
            JS_ReportError(pContext, "ReadBlkCmd: Set Element Error.");
            return JS_FALSE;
        }
    }

    for (UINT32 i = 0; i < vData.size(); i++)
        Printf(Tee::PriDebug, "0x%02x ", vData[i]);

    return JS_TRUE;
}

C_(Smb_ProcessCallCmd)
{
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if (NumArguments != 3)
    {
        JS_ReportError(pContext, "Usage: Smb.ProcesssCallCmd(SlvAdrs, CmdCode, Data)");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 SlvAdrs, CmdCode, Data;

    if (OK != pJavaScript->FromJsval(pArguments[0], &SlvAdrs))
    {
        JS_ReportError(pContext, "SlavAdrs bad value.");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[1], &Data))
    {
        JS_ReportError(pContext, "Data bad value.");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[2], &CmdCode))
    {
        JS_ReportError(pContext, "CmdCode bad value.");
        return JS_FALSE;
    }

    RC rc;
    UINT16 DataRd = 0;
    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));

    if ( OK != (rc = pPort->ProcCallCmd((UINT16)SlvAdrs,(UINT08)CmdCode,(UINT16)Data,&DataRd)))
    {
        Printf(Tee::PriNormal, "0xdeadbeef\n");
        pJavaScript->ToJsval(0xdeadbeef, pReturlwalue);
        return JS_FALSE;
    }
    else
    {
        Printf(Tee::PriDebug, "0x%04x\n", DataRd);
        if (OK != pJavaScript->ToJsval((UINT32)DataRd, pReturlwalue))
        {
            JS_ReportError(pContext, "Error oclwrred in Smbus.ProcessCallCmd()");
            *pReturlwalue = JSVAL_NULL;
            return JS_FALSE;
        }
    }

    return JS_TRUE;
}

C_(Smb_HostNotify)
{
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if (NumArguments != 2)
    {
        JS_ReportError(pContext, "Usage: Smb.HostNotify(DvcAdrs, Data)");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 DvcAdrs, Data;

    if (OK != pJavaScript->FromJsval(pArguments[0], &DvcAdrs))
    {
        JS_ReportError(pContext, "SlavAdrs bad value.");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[1], &Data))
    {
        JS_ReportError(pContext, "Data bad value.");
        return JS_FALSE;
    }

    RC rc;
    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));
    RETURN_RC(pPort->HostNotify((UINT08)DvcAdrs,(UINT16)Data));
}

C_(Smb_IsSlavePresent)
{
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, "Usage: Smb.IsSlavePresent(SlvAddr)");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 SlaveAddr;

    if (OK != pJavaScript->FromJsval(pArguments[0], &SlaveAddr))
    {
        JS_ReportError(pContext, "SlavAdrs bad value.");
        return JS_FALSE;
    }

    RC rc;
    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));
    RETURN_RC(pPort->IsSlavePresent((UINT16)SlaveAddr));
}

C_(Smb_Populate)
{
    RC rc;
    MASSERT( (pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));

    if(NumArguments != 0 && NumArguments != 1)
    {
        JS_ReportError(pContext, "Usage: Smb.Populate() or Smb.Populate([slvAddr])");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    JSObject *pSlvAddr;
    vector<UINT08> address;
    address.clear();

    if( (NumArguments==1)
        && (OK != pJavaScript->FromJsval(pArguments[0], &pSlvAddr)) )
        {
            JS_ReportError(pContext, "slvAddr bad value.");
            return JS_FALSE;
        }

    SmbPort* pPort = NULL;
    C_CHECK_RC(GetLwrrentPort(&pPort));

    // Search the current port for all slave addresses
    C_CHECK_RC(pPort->Search());
    C_CHECK_RC(pPort->GetSlaveAddrs(&address));

    if(NumArguments==1)
    {
        //colwert vector to jsarray
        for(UINT32 i=0;i<address.size();i++)
        {
            if(OK!=(rc=pJavaScript->SetElement(pSlvAddr,i,address[i])))
                RETURN_RC(rc);
        }
    }
    RETURN_RC(OK);
}

