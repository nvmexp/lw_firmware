/**************************************************************************************************
 * File     : regmap_helper.cpp
 * Purpose  : To provide generalised functions to aid reading/writing/polling registers as part 
 *            of various tests.
 *
 *    Last edited on September 20, 2018 by Vibhor Dodeja
 *************************************************************************************************/

/**************************************************************************************************\
 * README                                                                                         *
 *     Colwentions for parameters                                                                 *
 *         unit, reg_n, field_n, val_n/value_n                                                    *
 *             These are string parameters which together are used to identify registers,         *
 *             fields and value defines (as defined in the manuals).                              *
 *             They are used as follows to match with the manuals:                                *
 *                 Registers are matched with LW_<unit>_<reg_n>    [Referred as REG henceforth]   *
 *                 Fields are matched with REG_<field_n>           [Referred as FLD henceforth]   *
 *                 Values are matched with FLD_<val_n>             [Referred as VAL henceforth]   *
 *                                                                                                *
 *         val/numVal                                                                             *
 *             val is different from val_n. val is a numeric value that is written without        *
 *             matching with the manuals, while val_n is a string which is used to match the      *
 *             manual defines.                                                                    *
 *                                                                                                *
 *         baseAddr                                                                               *
 *             Some registers like LW_RUNLIST_* (Ampere onwards) are defined from 0 offset.       *
 *             They require adding the offset to point to the correct register. This offset is    *
 *             specified in the baseAddr parameter. This parameter is always the last parameter   *
 *             and is always optional. It takes the default value of 0 if not provided.           *
 *                                                                                                *
 *         field_val_pairs                                                                        *
 *             Some functions provide the ability to read/write/poll multiple fields of the       *
 *             same register. All these functions accept field_n and val_n as pairs and can be    *
 *             passed to the function in the following format                                     *
 *                 { { <field_1>, <val_1> }, { <field_2>, <val_2> } ... }                         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *     Register Write Function - regWr                                                            *
 *         Note: regWr is actually a wrapper over drf_* functions. Against each use case below    *
 *         the corresponding drf_ function is specified. You call that drf function with the      *
 *         same parameters for the same effect.                                                   *
 *                                                                                                *
 *         regWr is a polymorphic function which can write to registers in the following ways:    *
 *             regWr(unit, reg_n, field_n, val_n, baseAddr)  [wrapper over drf_def]               *
 *                 Writes single field FLD of register REG with value VAL.                        *
 *                                                                                                *
 *             regWr(unit, reg_n, field_val_pairs, baseAddr) [wrapper over drf_der]               *
 *                 Writes multiple fields FLDs of register REG with corresponding VALs            *
 *                                                                                                *
 *             regWr(unit, reg_n, field_n, numVal, baseAddr) [wrapper over drf_num]               *
 *                 Writes field FLD of register REG with value numVal                             *
 *                                                                                                *
 *             regWr(unit, reg_n, numVal, baseAddr)          [wrapper over drf_num]               *
 *                 Writes register REG with value numVal.                                         *
 *                                                                                                *
 *         It returns 0 if the write passed. And 1 if the write failed.                           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *     Register Read Function - regRd                                                             *
 *         regRd reads a register or a field of a register and returns its numeric value. To      *
 *         indicate whether a read was successful or not, it requires a boolean variable          *
 *         (referred as "valid" henceforth) to be passed as an argument. The function then writes * 
 *         to this boolean variable to true if the read succeeded and false if the read failed.   *
 *                                                                                                *
 *              regRd(unit, reg_n, valid, baseAddr)                                               *
 *                  Reads register REG.                                                           *
 *              regRd(unit, reg_n, field_n, valid, baseAddr)                                      *
 *                  Reads field FLD of register REG                                               *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *      Check register value function - regChk/regMaskedChk                                       *
 *          regChk reads a register or field(s) of a register and compares it to a reference      *
 *          value passed as argument.                                                             *
 *                                                                                                *
 *              regChk(unit, reg_n, numVal, baseAddr)                                             *
 *                  Reads register REG and compares its numeric value to numVal                   *
 *              regChk(unit, reg_n, field_n, val_n, baseAddr)                                     *
 *                  Reads field FLD of register REG and compares it with value VAL                *
 *              regChk(unit, reg_n, field_val_pairs, baseAddr)                                    *
 *                  Reads multiple fields FLDs of register REG and compares each with their       *
 *                  corresponding VALs.                                                           *
 *              regChk(unit, reg_n, field_n, numVal, baseAddr)                                    *
 *                  Reads field FLD of register REG and compares its numeric value with numVal    *
 *                                                                                                *
 *         regMaskedChk(unit, reg_n, numVal, mask, baseAddr) reads a register REG, masks the      *
 *         result with the mask provided as argument and compares it with reference value numVal  *
 *                                                                                                *
 *         Both these functions return 0 if all values match, 1 if any values is different        *
 *         and -1 if there is an error while reading any field/value.                             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *      Register poll function - regPoll                                                          *
 *          Extra parameters for this function:                                                   *
 *             interPollWait                                                                      *
 *                  It defines the time for which the code waits between 2 reads of the register. *
 *                  It is defined in us.                                                          *
 *             maxPoll                                                                            *
 *                  The maximum number of times the function will read the register before        *
 *                  timing out.                                                                   *
 *             equal                                                                              *  
 *                  It is a boolean parameter. If it is true, the function will poll until the    *      
 *                  value of the register/field(s) is equal the reference value. If it is false,  *          
 *                  it will poll until the value of the register/field(s) is different from the   *      
 *                  reference value.                                                              *  
 *                  It is an optional parameter with the default value of "true". However, it is  *      
 *                  mandatory to pass this argument if baseAddr is to be specified.               *      
 *                                                                                                *                      
 *         regPoll polls the value of a register or field(s) of a register until either it is     *      
 *         equal to/different from (specified by equal arg) the refernece value or or until it    *          
 *         times out. It comes in the following flavors:                                          *                  
 *                                                                                                *              
 *              regPoll(unit, reg_n, numVal, interPollWait, maxPoll, equal, baseAddr)             *                      
 *                  Polls register REG until its numerical value is (not) equal to numVal         *                  
 *              regPoll(unit, reg_n, numVal, mask, interPollWait, maxPoll, equal, baseAddr)       *                          
 *                  Polls register REG until its numerical value masked by mask argument is (not) *                              
 *                  equal to numVal.                                                              *                      
 *              regPoll(unit, reg_n, field_n, val_n, interPollWait, maxPoll, equal, baseAddr)     *      
 *                  Polls field FLD of register REG until it is (not) equal to VAL                *                      
 *              regPoll(unit, reg_n, field_val_pairs, interPollWait, maxPoll, equal, baseAddr)    *              
 *                  Polls fields FLDs of register REG until each is (not) equal to their          *                      
 *                  corresponding VAL                                                             *                      
 *              regPoll(unit, reg_n, field_n, numVal, interPollWait, maxPoll, equal, baseAddr)    *                  
 *                  Polls field FLD of register REG until its numerical value is (not) equal to   *                  
 *                  numVal                                                                        *          
 *                                                                                                *                  
 *         It returns 0 if the poll completes, 1 if it timeouts and -1 if there is a read error.  *                          
 *                                                                                                *              
\**************************************************************************************************/

#include "regmap_helper.h"

// Constructor
regmap_helper::regmap_helper(LWGpuResource *gpu, IRegisterMap *rm):
    lwgpu(gpu), m_regMap(rm){
}

// Program field LW_{unit}_{reg_n}_{field_n} with value LW_{unit}_{reg_n}_{field_n}_{def}
//  Note that 'baseAddr' is an optional parameter and takes default value 0, if not specified.
int regmap_helper::drf_def(string unit, 
                            string reg_n, 
                            string field_n, 
                            string def, 
                            UINT32 baseAddr) {
    // Local variable declaration.
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data, setting_new;

    string reg_name = "LW_" + unit + "_" + reg_n;
    string field_name = reg_name.substr(0,reg_name.find('(')) + "_" + field_n;
    string value_name = field_name + "_" + def;

    // Look for register define
    if ((reg = m_regMap->FindRegister(reg_name.c_str()))){
        if ( reg_name.find('(') != string::npos) {
            reg_addr = baseAddr + reg->GetAddress(stoi(reg_name.substr(reg_name.find('(')+1,reg_name.find(')')-reg_name.find('('))));
        } else {
            reg_addr = baseAddr + reg->GetAddress();
        }
        reg_data = lwgpu->RegRd32(reg_addr);
        DebugPrintf("reg_name %s addr=0x%x\n reg_data=0x%x\n",
                    reg_name.c_str(), reg_addr, reg_data);
        
        // Register Found! Look for field define
        if ((field = reg->FindField(field_name.c_str()))){
            DebugPrintf(" field_name %s, start_bit: 0x%x, bit_mask=0x%x\n",
                        field_name.c_str(), field->GetStartBit(), field->GetMask());

            // Field found! Look for value define
            if((value  = field->FindValue(value_name.c_str()))){
                setting_new = value->GetValue();
                DebugPrintf("value_names=%s, value=0x%x\n", value_name.c_str(), setting_new);

                // Write value to field of register.
                reg_data = ((reg_data & ~field->GetMask()) |
                            setting_new << field->GetStartBit());
                DebugPrintf("reg_data 0x%x\n", reg_data);
                lwgpu->RegWr32(reg_addr, reg_data);

                InfoPrintf("drf_helper: Writing %s = 0x%x\n", field_name.c_str(), reg_data);
                return 0;
            }
            else {
                ErrPrintf("Value %s was not found in register map.\n", value_name.c_str());
                return 1;
            }
        }
        else{
            ErrPrintf("Field %s was not found in register map.\n", field_name.c_str());
            return 1;
        }
    }
    else{
        ErrPrintf("Register %s was not found in register map.\n", reg_name.c_str());
        return 1;
    }
}


// Program field LW_{unit}_{reg_n}_{field_n} with number
//  Note that 'baseAddr' is an optional parameter and takes default value 0, if not specified.
int regmap_helper::drf_num(string unit, 
                            string reg_n, 
                            string field_n, 
                            int number, 
                            UINT32 baseAddr) {
    // Local variable declaration.
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 reg_addr, reg_data;

    string reg_name = "LW_" + unit + "_" + reg_n;
    string field_name = reg_name.substr(0,reg_name.find('(')) + "_" + field_n;

    // Look for register define
    if ((reg = m_regMap->FindRegister(reg_name.c_str()))){
        if ( reg_name.find('(') != string::npos) {
            reg_addr = baseAddr + reg->GetAddress(stoi(reg_name.substr(reg_name.find('(')+1,reg_name.find(')')-reg_name.find('('))));
        } else {
            reg_addr = baseAddr + reg->GetAddress();
        }
        reg_data = lwgpu->RegRd32(reg_addr);

        // Register found! Look for field define.
        if ((field = reg->FindField(field_name.c_str()))){
            DebugPrintf(" field_name %s, start_bit: 0x%x, bit_mask=0x%x\n",
                        field_name.c_str(), field->GetStartBit(), field->GetMask());
            
            // Field found! Write data.
            reg_data = (reg_data & ~field->GetMask()) |
                ((number << field->GetStartBit()) & field->GetMask());
            lwgpu->RegWr32(reg_addr, reg_data);

            InfoPrintf("drf_helper: Writing %s = 0x%x\n", field_name.c_str(), reg_data);
            return 0;
        }
        else{
            ErrPrintf("Field %s was not found in register map.\n", field_name.c_str());
            return 1;
        }
    }
    else{
        ErrPrintf("Register %s was not found in register map.\n", reg_name.c_str());
        return 1;
    }
}

// Program multiple fields in a register.
// Usage:
//  To program LW_{unit}_{reg_n} registers'
//  fields _{field_1}, _{field_2}... with values _{val_1}, _{val_2}...
//  Call as:
//      drf_def(unit, reg_n, {{field_1,val_1}, {field_2,val_2}...}, baseAddr);
//
//  If same field is supplied multiple times with different values, the last one will be taken.
//
//  Note that 'baseAddr' is an optional parameter and takes default value 0, if not specified.
int regmap_helper::drf_def ( string unit, 
                                string reg_n, 
                                vector<pair<string, string>> field_value_pairs, 
                                UINT32 baseAddr) {
    // Local variable declaration.
    unique_ptr<IRegisterClass> reg;
    UINT32 regAddr, regData;

    string regName = "LW_" + unit + "_" + reg_n;
    
    // Look for register define.
    if ( (reg = m_regMap->FindRegister(regName.c_str())) ) {
        if ( regName.find('(') != string::npos) {
            regAddr = baseAddr + reg->GetAddress(stoi(regName.substr(regName.find('(')+1,regName.find(')')-regName.find('('))));
        } else {
            regAddr = baseAddr + reg->GetAddress();
        }
        regData = lwgpu->RegRd32(regAddr);

        DebugPrintf("%s: Register name: %s; Addr: 0x%x; Read data: 0x%x.\n",
                    __FUNCTION__, regName.c_str(), regAddr, regData );

        UINT32 netVal = regData;
        // Register found! Look for field-value defines.
        for (pair<string,string> fvpair : field_value_pairs ) {
            unique_ptr<IRegisterField> field;
            unique_ptr<IRegisterValue> value;

            string fieldName = regName.substr(0,regName.find('(')) + "_" + fvpair.first;
            if ( (field = reg->FindField(fieldName.c_str())) ) {
                DebugPrintf("%s: Field name: %s; Start bit: 0x%x; Bit mask: 0x%x.\n",
                            __FUNCTION__, fieldName.c_str(), field->GetStartBit(), field->GetMask() );

                string valueName = fieldName + "_" + fvpair.second;

                if ( (value = field->FindValue(valueName.c_str())) ) {
                    DebugPrintf("%s: Value name: %s\n",
                                __FUNCTION__, valueName.c_str());
                    
                    netVal = ( netVal & ~field->GetMask() ) | 
                            (( value->GetValue() << field->GetStartBit() ) & field->GetMask() );

                    DebugPrintf("%s: Net Reg Data: 0x%x.\n",
                                __FUNCTION__, netVal );
                }
                else {
                    ErrPrintf("%s: Value name %s not found in register map.\n", __FUNCTION__, valueName.c_str());
                    return 1;
                }
            }
            else {
                ErrPrintf("%s: Field name %s not found in register map.\n", __FUNCTION__, fieldName.c_str());
                return 1;
            }
        }

        // Write to register
        lwgpu->RegWr32(regAddr, netVal);
        InfoPrintf("%s: Writing register %s (Addr: 0x%x) with value 0x%x.\n", 
                    __FUNCTION__, regName.c_str(), regAddr, netVal);
        return 0;
    }
    else {
        ErrPrintf("%s: Register %s not found in register map.\n", __FUNCTION__, regName.c_str());
        return 1;
    }
}

// Program register LW_{unit}_{reg_n} with value.
// Note that 'baseAddr' is an optional parameter and takes default value 0, if not specified.
int regmap_helper::drf_num(string unit, 
                            string reg_n, 
                            UINT32 val, 
                            UINT32 baseAddr) {
    // Local variable declaration.
    unique_ptr<IRegisterClass> reg;
    UINT32 regAddr, regData;

    string regName = "LW_" + unit + "_" + reg_n;
    // Look for register
    if ( (reg = m_regMap->FindRegister(regName.c_str())) ) {
        if ( regName.find('(') != string::npos) {
            regAddr = baseAddr + reg->GetAddress(stoi(regName.substr(regName.find('(')+1,regName.find(')')-regName.find('('))));
        } else {
            regAddr = baseAddr + reg->GetAddress();
        }   
        regData = lwgpu->RegRd32(regAddr);

        DebugPrintf("%s: Register name: %s; Addr: 0x%x; Read data: 0x%x.\n",
                    __FUNCTION__, regName.c_str(), regAddr, regData);

        // Write to register
        lwgpu->RegWr32(regAddr, val);
        InfoPrintf("%s: Writing register %s (Addr: 0x%x) with value 0x%x.\n", 
                    __FUNCTION__, regName.c_str(), regAddr, val);
        return 0;
    }
    else {
        ErrPrintf("%s: Register %s not found in register map.\n", __FUNCTION__, regName.c_str());
        return 1;
    }
}

// Read register LW_{unit}_{reg_n}
// Send any boolean variable to the function and read its value after completion
// to check if the value returned by the function is valid.
UINT32 regmap_helper::regRd(string unit, 
                                string reg_n, 
                                bool &valid, 
                                UINT32 baseAddr) {
    // Local variable declaration.
    unique_ptr<IRegisterClass> reg;
    UINT32 regAddr, regData;

    string regName = "LW_" + unit + "_" + reg_n;

    // Look for register
    if ( (reg = m_regMap->FindRegister(regName.c_str())) ) {
        if ( regName.find('(') != string::npos) {
        regAddr = baseAddr + reg->GetAddress(stoi(regName.substr(regName.find('(')+1,regName.find(')')-regName.find('('))));
        } else {
            regAddr = baseAddr + reg->GetAddress();
        } 
        regData = lwgpu->RegRd32(regAddr);

        InfoPrintf("%s: Register name: %s; Addr: 0x%x; Read data: 0x%x.\n",
                    __FUNCTION__, regName.c_str(), regAddr, regData);

        if ( (regData & 0xffff0000) == 0xbadf0000 ) {
            ErrPrintf("%s: 0xbadf found while reading register %s.\n", __FUNCTION__, regName.c_str());
            valid = false;
        }
        else valid = true;
        return regData;
    }
    else {
        ErrPrintf("%s: Register %s not found in register map.\n", __FUNCTION__, regName.c_str());
        valid = false;
        return 0;
    }
}

// Read register field LW_{unit}_{reg_n}_{field_n}
// Send any boolean variable to the function and read its value after completion
// to check if the value returned by the function is valid.
UINT32 regmap_helper::regRd(string unit, 
                                string reg_n, 
                                string field_n, 
                                bool &valid, 
                                UINT32 baseAddr) {
    // Local variable declaration.
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;

    UINT32 regAddr, regData;

    string regName = "LW_" + unit + "_" + reg_n;
    string fieldName = regName.substr(0,regName.find('(')) + "_" + field_n;

    // Look for register
    if ( (reg = m_regMap->FindRegister(regName.c_str())) ) {
        if ( regName.find('(') != string::npos) {
            regAddr = baseAddr + reg->GetAddress(stoi(regName.substr(regName.find('(')+1,regName.find(')')-regName.find('('))));
        } else {
            regAddr = baseAddr + reg->GetAddress();
        }       
        regData = lwgpu->RegRd32(regAddr);

        DebugPrintf("%s: Register name: %s; Addr: 0x%x; Read data: 0x%x.\n",
                    __FUNCTION__, regName.c_str(), regAddr, regData);

        if ( (regData & 0xffff0000) == 0xbadf0000 ) {
            ErrPrintf("%s: 0xbadf found while reading register %s (Addr: 0x%x; Read data: 0x%x).\n",
                        __FUNCTION__, regName.c_str(), regAddr, regData);
            valid = false;
            return regData;
        }
        
        // Register found! Look for field.
        if ( (field = reg->FindField(fieldName.c_str())) ) {
            DebugPrintf("%s: Field name: %s; Start bit: 0x%x; Bit mask: 0x%x.\n",       
                        __FUNCTION__, fieldName.c_str(), field->GetStartBit(), field->GetMask());

            regData = ( regData & field->GetMask() ) >> field->GetStartBit();
            
            InfoPrintf("%s: Register name: %s; Addr: 0x%x; Field name: %s; Read value: 0x%x.\n",
                        __FUNCTION__, regName.c_str(), regAddr, fieldName.c_str(), regData);

            valid = true;
            return regData;
        }
        else {
            ErrPrintf("%s: Field name %s not found in register map.\n", __FUNCTION__, fieldName.c_str());
            valid = false;
            return 0;
        }
    }
    else {
        ErrPrintf("%s: Register %s not found in register map.\n", __FUNCTION__, regName.c_str());
        valid = false;
        return 0;
    }
}

// Check if LW_{unit}_{reg_n} register's read value is equal to a specified value.
// Returns:
//  0 : If register value equal
//  1 : If register value not equal
//  -1: If error while reading.
int regmap_helper::regChk(string unit, 
                            string reg_n, 
                            UINT32 val, 
                            UINT32 baseAddr) {
    string regName = "LW_" + unit + "_" + reg_n;

    bool valid;
    UINT32 read = regRd(unit, reg_n, valid, baseAddr);
    
    if ( !valid ) {
        ErrPrintf("%s: Error reading register %s.\n", __FUNCTION__, regName.c_str());
        return -1;
    }

    DebugPrintf("%s: Register Name: %s; Read value: 0x%x; Reference value: 0x%x.\n",
                __FUNCTION__, regName.c_str(), read, val);

    return read != val;
}

// Check if LW_{unit}_{reg_n}_{field_n} field's read value is equal to LW_{unit}_{reg_n}_{field_n}_{val_n}.
// Returns:
//  0 : If field value equal
//  1 : If field value different
//  -1: If error while reading.
int regmap_helper::regChk(string unit, 
                            string reg_n, 
                            string field_n, 
                            string val_n, 
                            UINT32 baseAddr) {
    string regName = "LW_" + unit + "_" + reg_n;
    string fieldName = regName.substr(0,regName.find('(')) + "_" + field_n;
    string valName = fieldName + "_" + val_n;

    bool valid;
    UINT32 readVal = regRd(unit, reg_n, field_n, valid, baseAddr);

    if ( !valid ) {
        ErrPrintf("%s: Error reading field %s.\n", __FUNCTION__, fieldName.c_str());
        return -1;
    }

    // Look for value define.
    unique_ptr<IRegisterValue> val;
    if ( (val = m_regMap->FindRegister(regName.c_str())->FindField(fieldName.c_str())->FindValue(valName.c_str())) ) {
        UINT32 refVal = val->GetValue();
        DebugPrintf("%s: Field Name: %s; Read Value: 0x%x; Reference Value name: %s; Reference value: 0x%x.\n",
                    __FUNCTION__, fieldName.c_str(), readVal, valName.c_str(), refVal);

        return readVal != refVal;
    }
    else {
        ErrPrintf("%s: Value name %s not found in register map.\n", __FUNCTION__, valName.c_str());
        return -1;
    }
}

// Check if LW_{unit}_{reg_n}_{field_1}, LW_{unit}_{reg_n}_{field_2}...
// fields' read value is equal to 
// LW_{unit}_{reg_n}_{field_1}_{val_1}, LW_{unit}_{reg_n}_{field_2}_{val_2}...
//
// Returns:
//  0 : If all field values equal
//  1 : If any field value different
//  -1: If error while reading.
int regmap_helper::regChk(string unit, 
                            string reg_n, 
                            vector<pair<string,string>> field_val_pairs, 
                            UINT32 baseAddr) {
    string regName = "LW_" + unit + "_" + reg_n;

    bool valid;

    UINT32 readVal = regRd(unit, reg_n, valid, baseAddr);
    if ( !valid ) {
        ErrPrintf("%s: Error reading register %s.\n", __FUNCTION__, regName.c_str());
        return -1;
    }

    // Look for field value defines.
    for ( pair<string,string> fvpair : field_val_pairs ) {
        unique_ptr<IRegisterField> field;
        unique_ptr<IRegisterValue> val;

        string fieldName = regName.substr(0,regName.find('(')) + "_" + fvpair.first;
        if ( (field = m_regMap->FindRegister(regName.c_str())->FindField(fieldName.c_str())) ) {
            DebugPrintf("%s: Field name: %s; Start bit: 0x%x; Bit mask: 0x%x.\n",
                        __FUNCTION__, fieldName.c_str(), field->GetStartBit(), field->GetMask() );

            string valName = fieldName + "_" + fvpair.second;
            if ( (val = field->FindValue(valName.c_str())) ) {
                UINT32 refVal = val->GetValue();
                DebugPrintf("%s: Field Name: %s; Read Value: 0x%x; Reference Value name: %s; Reference value: 0x%x.\n",
                        __FUNCTION__, fieldName.c_str(), readVal, valName.c_str(), refVal);

                UINT32 fldVal = (readVal & field->GetMask()) >> field->GetStartBit(); 
                if ( fldVal != refVal ) return 1;
            }
            else {
                ErrPrintf("%s: Value name %s not found in register map.\n", __FUNCTION__, valName.c_str());
                return -1;
            }
        }
        else {
            ErrPrintf("%s: Field name %s not found in register map.\n", __FUNCTION__, fieldName.c_str());
            return -1;
        }
    }
    return 0;
}

// Check if LW_{unit}_{reg_n}_{field_n} field's read value is equal to the given value.
// Returns:
//  0 : If field value equal
//  1 : If field value different
//  -1: If error while reading.
int regmap_helper::regChk(string unit, 
                            string reg_n, 
                            string field_n, 
                            UINT32 val, 
                            UINT32 baseAddr) {
    string regName = "LW_" + unit + "_" + reg_n;
    string fieldName = regName.substr(0,regName.find('(')) + "_" + field_n;

    bool valid;
    UINT32 readVal = regRd(unit, reg_n, field_n, valid, baseAddr);

    if ( !valid ) {
        ErrPrintf("%s: Error reading field %s.\n", __FUNCTION__, fieldName.c_str());
        return -1;
    }

    DebugPrintf("%s: Field Name: %s; Read Value: 0x%x; Reference value: 0x%x.\n",
                __FUNCTION__, fieldName.c_str(), readVal, val);

    return readVal != val;
}

// Check if LW_{unit}_{reg_n} register's read value masked with {mask} is equal to a specified value.
// Returns:
//  0 : If register value equal
//  1 : If register value not equal
//  -1: If error while reading.
int regmap_helper::regMaskedChk(string unit, 
                                    string reg_n, 
                                    UINT32 val, 
                                    UINT32 mask, 
                                    UINT32 baseAddr) {
    string regName = "LW_" + unit + "_" + reg_n;

    bool valid;
    UINT32 read = regRd(unit, reg_n, valid, baseAddr);
    
    if ( !valid ) {
        ErrPrintf("%s: Error reading register %s.\n", __FUNCTION__, regName.c_str());
        return -1;
    }

    DebugPrintf("%s: Register Name: %s; Read value: 0x%x; Reference value: 0x%x.\n",
                __FUNCTION__, regName.c_str(), read, val);

    return (read & mask) != val;
}

// Poll LW_{unit}_{reg_n} register until it is equal/not equal to a given value.
// It will poll at most {maxPoll} times after which it will timeout.
// Returns:
//  0 : If done.
//  1 : If timeout.
//  -1: If error while reading.
int regmap_helper::regPoll(string unit, 
                            string reg_n, 
                            UINT32 val, 
                            UINT32 interPollWait, 
                            int maxPoll, 
                            bool equal, 
                            UINT32 baseAddr) {
    string regName = "LW_" + unit + "_" + reg_n;
    
    bool done = false;
    int count = 0;
    
    while ( !done && count < maxPoll ) {
        int res = regChk(unit, reg_n, val, baseAddr);
        if (res == -1) {
            ErrPrintf("%s: Error reading register %s.\n", __FUNCTION__, regName.c_str());
            return -1;
        }
        if ( (equal && !res) || (!equal && res) ) done = true;
        else Platform::Delay(interPollWait);
        count++;
    }

    if ( !done ) {
        ErrPrintf("%s: Timeout while polling for register %s to be %s reference value of 0x%x.\n", 
                    __FUNCTION__, regName.c_str(), equal?"equal to":"different from", val);
        return 1;
    }
    
    InfoPrintf("%s: Done polling for register %s to be %s reference value of 0x%x.\n",
                __FUNCTION__, regName.c_str(), equal?"equal to":"different from", val);
    return 0;
}

// Poll LW_{unit}_{reg_n} register until it's masked value equal/not equal to a given value.
// It will poll at most {maxPoll} times after which it will timeout.
// Returns:
//  0 : If done.
//  1 : If timeout.
//  -1: If error while reading.
int regmap_helper::regPoll(string unit, 
                                    string reg_n, 
                                    UINT32 mask, 
                                    UINT32 val, 
                                    UINT32 interPollWait, 
                                    int maxPoll, 
                                    bool equal, 
                                    UINT32 baseAddr) {
    string regName = "LW_" + unit + "_" + reg_n;
    
    bool done = false;
    int count = 0;
    
    while ( !done && count < maxPoll ) {
        int res = regMaskedChk(unit, reg_n, mask, val, baseAddr);
        if (res == -1) {
            ErrPrintf("%s: Error reading register %s.\n", __FUNCTION__, regName.c_str());
            return -1;
        }
        if ( (equal && !res) || (!equal && res) ) done = true;
        else Platform::Delay(interPollWait);
        count++;
    }

    if ( !done ) {
        ErrPrintf("%s: Timeout while polling for register %s masked with 0x%x to be %s reference value of 0x%x.\n", 
                    __FUNCTION__, regName.c_str(), mask, equal?"equal to":"different from", val);
        return 1;
    }
    
    InfoPrintf("%s: Done polling for register %s masked with 0x%x to be %s reference value of 0x%x.\n",
                __FUNCTION__, regName.c_str(), mask, equal?"equal to":"different from", val);
    return 0;
}

// Poll LW_{unit}_{reg_n}_{field_n} register field until it is 
// equal/not equal to LW_{unit}_{reg_n}_{field_n}_{val_n} value.
// It will poll at most {maxPoll} times after which it will timeout.
// Returns:
//  0 : If done.
//  1 : If timeout.
//  -1: If error while reading.
int regmap_helper::regPoll(string unit, 
                            string reg_n, 
                            string field_n, 
                            string val_n,
                            UINT32 interPollWait, 
                            int maxPoll, 
                            bool equal, 
                            UINT32 baseAddr) {
    string fieldName = "LW_" + unit + "_" + reg_n.substr(0,reg_n.find('(')) + "_" + field_n;
    string valName = fieldName + "_" + val_n;
    
    bool done = false;
    int count = 0;
    
    while ( !done && count < maxPoll ) {
        int res = regChk(unit, reg_n, field_n, val_n, baseAddr);
        if (res == -1) {
            ErrPrintf("%s: Error checking value %s.\n", __FUNCTION__, valName.c_str());
            return -1;
        }
        if ( (equal && !res) || (!equal && res) ) done = true;
        else Platform::Delay(interPollWait);
        count++;
    }

    if ( !done ) {
        ErrPrintf("%s: Timeout while polling for register field %s to be %s reference value %s.\n", 
                    __FUNCTION__, fieldName.c_str(), equal?"equal to":"different from", valName.c_str());
        return 1;
    }
    
    InfoPrintf("%s: Done polling for register %s to be %s reference value %s.\n",
                __FUNCTION__, fieldName.c_str(), equal?"equal to":"different from", valName.c_str());
    return 0;
}

// Poll LW_{unit}_{reg_n}_{field_1}, LW_{unit}_{reg_n}_{field_2}...
// register fields until they are equal/not equal to 
// LW_{unit}_{reg_n}_{field_1}_{val_1}, LW_{unit}_{reg_n}_{field_2}_{val_2} values respectively.
// It will poll at most {maxPoll} times after which it will timeout.
// Returns:
//  0 : If all done.
//  1 : If atleast 1 timeout.
//  -1: If error while reading.
int regmap_helper::regPoll(string unit, 
                            string reg_n, 
                            vector<pair<string,string>> field_val_pairs, 
                            UINT32 interPollWait, 
                            int maxPoll, 
                            bool equal, 
                            UINT32 baseAddr) {
    string regName = "LW_" + unit + "_" + reg_n;

    bool done = false;
    int count = 0;
    
    while ( !done && count < maxPoll ) {
        int res = regChk(unit, reg_n, field_val_pairs, baseAddr);
        if (res == -1) {
            ErrPrintf("%s: Error reading register %s.\n", __FUNCTION__, regName.c_str());
            return -1;
        }
        if ( (equal && !res) || (!equal && res) ) done = true;
        else Platform::Delay(interPollWait);
        count++;
    }

    if ( !done ) {
        ErrPrintf("%s: Timeout while polling for various fields of register %s.\n", 
                    __FUNCTION__, regName.c_str());
        return 1;
    }
    
    InfoPrintf("%s: Done polling for various fields of register %s.\n",
                __FUNCTION__, regName.c_str());
    return 0;
}
// Poll LW_{unit}_{reg_n}_{field_n} register until it is equal/not equal to a given value.
// It will poll at most {maxPoll} times after which it will timeout.
// Returns:
//  0 : If done.
//  1 : If timeout.
//  -1: If error while reading.
int regmap_helper::regPoll(string unit, 
                            string reg_n, 
                            string field_n, 
                            UINT32 val, 
                            UINT32 interPollWait, 
                            int maxPoll, 
                            bool equal, 
                            UINT32 baseAddr) {
    string fieldName = "LW_" + unit + "_" + reg_n + "_" + field_n;
    
    bool done = false;
    int count = 0;
    
    while ( !done && count < maxPoll ) {
        int res = regChk(unit, reg_n, field_n, val, baseAddr);
        if (res == -1) {
            ErrPrintf("%s: Error reading register field %s.\n", __FUNCTION__, fieldName.c_str());
            return -1;
        }
        if ( (equal && !res) || (!equal && res) ) done = true;
        else Platform::Delay(interPollWait);
        count++;
    }

    if ( !done ) {
        ErrPrintf("%s: Timeout while polling for register field %s to be %s reference value of 0x%x.\n", 
                    __FUNCTION__, fieldName.c_str(), equal?"equal to":"different from", val);
        return 1;
    }
    
    InfoPrintf("%s: Done polling for register field %s to be %s reference value of 0x%x.\n",
                __FUNCTION__, fieldName.c_str(), equal?"equal to":"different from", val);
    return 0;
}
