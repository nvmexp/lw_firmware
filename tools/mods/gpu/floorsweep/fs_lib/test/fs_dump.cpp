// Floorsweeping library: dump possible FS configs
// Copyright LWPU Corp. 2019
#include "fs_dump.h"
// GA100 test sweep +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
int main(int argc, char** argv) {

    InputParser input(argc, argv);
    if(input.cmdOptionExists("-h") || input.cmdOptionExists("-H") || input.cmdOptionExists("-help") || input.cmdOptionExists("-HELP") || 
            input.cmdOptionExists("--h") || input.cmdOptionExists("--H") || input.cmdOptionExists("--help") || input.cmdOptionExists("--HELP")){
        input.printErrorMsg(argv);
        return 1;
    }
    if (!((argc==1) || (argc==3) || (argc== 5) || (argc==7))) {
        input.printErrorMsg(argv);
        return 1;
    }
    const std::string &modename = input.getCmdOption("-mode");
    const std::string &filename = input.getCmdOption("-o");
    const std::string &chipname = input.getCmdOption("-chip");

    bool is_valid = false;
    std::string msg, newfilename;
    fslib::FbpMasks fbConfig0 = {0};
    fslib::GpcMasks gpcConfig0 = {0};

    newfilename = filename;
    std::ofstream myfile;
    if (chipname.empty()) {
        input.printErrorMsg(argv);
        std::cout << "Warning: -chip should be ga100 or ga102 or ga107, proceeding with default chip ga100" << std::endl;
    }
    if (filename.empty()) {
        input.printErrorMsg(argv);
        std::cout << "Warning: -o shouldn't be empty, proceeding with default_dump.csv" << std::endl;
        newfilename = "default_dump.csv";
    }
    std::cout << "Calling fslib, generating " << newfilename << " ..." << std::endl;

    myfile.open(newfilename);
    if (chipname == "ga102") {
        myfile << "F0:LTC0,F0:LTC1,F1:LTC0,F1:LTC1,F2:LTC0,F2:LTC1,F3:LTC0,F3:LTC1,F4:LTC0,F4:LTC1,F5:LTC0,F5:LTC1, F0,F1,F2,F3,F4,F5, FBP_MASK, #FBPs," << std::hex << std::endl;
    }
    // GA107 prints in reverse order
    else if (chipname == "ga107") {
        myfile << "F1:LTC1,F1:LTC0,F0:LTC1,F0:LTC0, F1,F0, HFBPA_F1,HFBPA_F0, FBP_MASK, #FBPs," << std::hex << std::endl;
    }
    else {
        myfile << "uGPUOn, A:F0,B:F1,A:F2,C:F3,B:F4,C:F5,E:F6,D:F7,F:F8,D:F9,F:F10,E:F11, FBP_MASK, #FBPs, GPC_MASK, #GPCs, G0,G1,G2,G3,G4,G5,G6,G7," << std::hex << std::endl;
    }
    if (chipname == "ga102") {
        unsigned int totall2slicemask = 0;
        unsigned int previousl2slicemask = 0;
        for (unsigned int fbpmask = 0; fbpmask <= 0x3f; fbpmask++) {
//            for (unsigned int ltcmask = 0; ltcmask <= 0x3; ltcmask++) {
                for (unsigned int l2sf024 = 0; l2sf024 <= 0xff; l2sf024++) {
                    for (unsigned int l2sf135 = 0; l2sf135 <= 0xff; l2sf135++) {
                        fbConfig0 = {0};    // resetting
                        fbConfig0.fbpMask = fbpmask;
                        totall2slicemask = 0;
                        bool anyBitSet = false;
                        int numFBPs = 6;
                        fbConfig0.l2SlicePerFbpMasks[0] = fbConfig0.l2SlicePerFbpMasks[2] = fbConfig0.l2SlicePerFbpMasks[4] = l2sf024;      
                        fbConfig0.l2SlicePerFbpMasks[1] = fbConfig0.l2SlicePerFbpMasks[3] = fbConfig0.l2SlicePerFbpMasks[5] = l2sf135;
                        if ((l2sf024 & 0xf) == 0xf) {
                            fbConfig0.ltcPerFbpMasks[0] |= 0x1;                                       
                            fbConfig0.ltcPerFbpMasks[2] |= 0x1;                                       
                            fbConfig0.ltcPerFbpMasks[4] |= 0x1;
                            anyBitSet = true;
                        }
                        if ((l2sf024 & 0xf0) == 0xf0) {
                            fbConfig0.ltcPerFbpMasks[0] |= 0x2;                                       
                            fbConfig0.ltcPerFbpMasks[2] |= 0x2;                                       
                            fbConfig0.ltcPerFbpMasks[4] |= 0x2;
                            anyBitSet = true;
                        }
                        if ((l2sf135 & 0xf) == 0xf) {
                            fbConfig0.ltcPerFbpMasks[1] |= 0x1;                                       
                            fbConfig0.ltcPerFbpMasks[3] |= 0x1;                                       
                            fbConfig0.ltcPerFbpMasks[5] |= 0x1;
                            anyBitSet = true;
                        }
                        if ((l2sf135 & 0xf0) == 0xf0) {
                            fbConfig0.ltcPerFbpMasks[1] |= 0x2;                                       
                            fbConfig0.ltcPerFbpMasks[3] |= 0x2;                                       
                            fbConfig0.ltcPerFbpMasks[5] |= 0x2;
                            anyBitSet = true;
                        }
                        
                        for (unsigned int i = 0; i < 6; i++) {
                            unsigned int maskbit = fbpmask & (0x1 << i);
                            if (maskbit) {
                                fbConfig0.ltcPerFbpMasks[i] = 0x3;
                                fbConfig0.fbioPerFbpMasks[i] = 0x1;
                                fbConfig0.fbpaPerFbpMasks[i] = 0x1;
                                fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
                                anyBitSet = true;
                                --numFBPs;
                            }
                            /*
                            else {
                                fbConfig0.ltcPerFbpMasks[i] |= ltcmask;
                                fbConfig0.fbioPerFbpMasks[i] = ltcmask;
                                fbConfig0.fbpaPerFbpMasks[i] = ltcmask;
                                anyBitSet = true;

                                for (unsigned int j=0; j < 4; j++) {
                                    unsigned int ltcmaskbit = ltcmask & (0x1 << j);
                                    if (ltcmaskbit) {
                                        fbConfig0.l2SlicePerFbpMasks[i] |= (0xf << ((j%2)*4));
                                        fbConfig0.ltcPerFbpMasks[i] |= (1 << (j%2));
                                        anyBitSet = true;
                                    }
                                }
                            }
                            */
                        }
                        for (int k=0; k<6; k++) {
                            totall2slicemask |= (fbConfig0.l2SlicePerFbpMasks[k] << 4*k);
                        }
                        if (totall2slicemask == previousl2slicemask) {  //preventing repetition
                            anyBitSet = false;
                        }
                        gpcConfig0 = {0};
                        if (anyBitSet && !(fbpmask == 0 && l2sf135 == 0 && l2sf024 == 0)) {    // fbpmask = 0 doesnt let any l2sf135 get written
                            if (modename == "FUNC_VALID") {
                                is_valid = fslib::IsFloorsweepingValid(fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
                            }
                            else {
                                is_valid = fslib::IsFloorsweepingValid(fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
                            }
                        }
                        else {
                            is_valid = 0; // force is_valid for all-0 configs
                        }
                        if (is_valid) {
                            myfile << std::hex;
                            for (int k=0; k < 6; k++) {
                                myfile << (fbConfig0.l2SlicePerFbpMasks[k] & 0xf) << "," << ((fbConfig0.l2SlicePerFbpMasks[k] >> 4) & 0xf) << ",";
                            }
                            for (int k=0; k < 6; k++) {
                                myfile << fbConfig0.ltcPerFbpMasks[k] << ",";
                            }                            
                            //std::copy ( fbConfig0.l2SlicePerFbpMasks, (fbConfig0.l2SlicePerFbpMasks + 6), std::ostream_iterator<unsigned int>(myfile, ",") );
                            myfile << "" << std::hex << fbConfig0.fbpMask;
                            myfile << "," << std::dec << (numFBPs);
                            myfile << "" << "\n";
                            previousl2slicemask = totall2slicemask;   
                        }
                        msg = "";            // resetting
                    }
                }
        //    }
        }
    }
    else if (chipname == "ga107") {
        int totalFBPs = 2;
        unsigned int totall2slicemask = 0;
        unsigned int previousl2slicemask = 0;
        for (unsigned int fbpmask = 0; fbpmask <= 0x3; fbpmask++) {
            for (unsigned int halffbpamask0 = 0; halffbpamask0 <= 0x1; halffbpamask0++) {
                for (unsigned int halffbpamask1 = 0; halffbpamask1 <= 0x1; halffbpamask1++) {
                    for (unsigned int ltcmask0 = 0; ltcmask0 <= 0x3; ltcmask0++) {
                        for (unsigned int ltcmask1 = 0; ltcmask1 <= 0x3; ltcmask1++) {
                            for (unsigned int l2sf0 = 0; l2sf0 <= 0xff; l2sf0++) {
                                for (unsigned int l2sf1 = 0; l2sf1 <= 0xff; l2sf1++) {
                                    fbConfig0 = {0};    // resetting
                                    fbConfig0.fbpMask = fbpmask;
                                    totall2slicemask = 0;
                                    bool anyBitSet = false;
                                    int numFBPs = totalFBPs;
                                    fbConfig0.l2SlicePerFbpMasks[0] = l2sf0;      
                                    fbConfig0.l2SlicePerFbpMasks[1] = l2sf1;
                                    fbConfig0.halfFbpaPerFbpMasks[0] = halffbpamask0;
                                    fbConfig0.halfFbpaPerFbpMasks[1] = halffbpamask1;
                                    fbConfig0.ltcPerFbpMasks[0] = ltcmask0;                                       
                                    fbConfig0.ltcPerFbpMasks[1] = ltcmask1;

                                    if ((l2sf0 & 0xf) == 0xf) {
                                        fbConfig0.ltcPerFbpMasks[0] |= 0x1;                                       
                                        anyBitSet = true;
                                    }
                                    if ((l2sf0 & 0xf0) == 0xf0) {
                                        fbConfig0.ltcPerFbpMasks[0] |= 0x2;                                       
                                        anyBitSet = true;
                                    }
                                    if ((l2sf1 & 0xf) == 0xf) {
                                        fbConfig0.ltcPerFbpMasks[1] |= 0x1;                                       
                                        anyBitSet = true;
                                    }
                                    if ((l2sf1 & 0xf0) == 0xf0) {
                                        fbConfig0.ltcPerFbpMasks[1] |= 0x2;                                       
                                        anyBitSet = true;
                                    }
                                                                    
                                    for (int i = 0; i < totalFBPs; i++) {
                                        int maskbit = fbpmask & (0x1 << i);
                                        if (maskbit) {
                                            fbConfig0.ltcPerFbpMasks[i] = 0x3;
                                            fbConfig0.fbioPerFbpMasks[i] = 0x1;
                                            //fbConfig0.fbpaPerFbpMasks[i] = 0x1;
                                            fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
                                            anyBitSet = true;
                                            --numFBPs;
                                        }
                                    }
                                    for (int k=0; k<totalFBPs; k++) {
                                        totall2slicemask |= (fbConfig0.l2SlicePerFbpMasks[k] << 4*k);
                                    }
                                    if (totall2slicemask == previousl2slicemask) {  //preventing repetition
                                        anyBitSet = false;
                                    }
                                    gpcConfig0 = {0};
                                    if (anyBitSet && !(fbpmask == 0 && l2sf1 == 0 && l2sf0 == 0)) {    // fbpmask = 0 doesnt let any l2sf135 get written
                                        if (modename == "FUNC_VALID") {
                                            is_valid = fslib::IsFloorsweepingValid(fslib::Chip::GA107, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
                                        }
                                        else {
                                            is_valid = fslib::IsFloorsweepingValid(fslib::Chip::GA107, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
                                        }
                                    }
                                    else {
                                        is_valid = 0; // force is_valid for all-0 configs
                                    }
                                    if (is_valid) {
                                        myfile << std::hex;
                                        for (int k=totalFBPs-1; k >=0 ; k--) {
                                            myfile << ((fbConfig0.l2SlicePerFbpMasks[k] & 0xf0) >> 4) << "," << (fbConfig0.l2SlicePerFbpMasks[k] & 0xf) << ",";
                                        }
                                        for (int k=totalFBPs-1; k >=0; k--) {
                                            myfile << fbConfig0.ltcPerFbpMasks[k] << ",";
                                        }                            
                                        for (int k=totalFBPs-1; k >=0; k--) {
                                            myfile << fbConfig0.halfFbpaPerFbpMasks[k] << ",";
                                        } 
                                        //std::copy ( fbConfig0.l2SlicePerFbpMasks, (fbConfig0.l2SlicePerFbpMasks + 6), std::ostream_iterator<unsigned int>(myfile, ",") );
                                        myfile << "" << std::hex << fbConfig0.fbpMask;
                                        myfile << "," << std::dec << (numFBPs);
                                        myfile << "" << "\n";
                                        previousl2slicemask = totall2slicemask;   
                                    }
                                    msg = "";            // resetting
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else {      // ga100
        for (unsigned int fbpmask = 0; fbpmask <= 0xfff; fbpmask++) {
            fbConfig0.fbpMask = fbpmask;
            for (unsigned int ltcmask = 0; ltcmask <= 3;ltcmask++) {
                bool anyBitSet = false;
                int numFBPs = 12;
                int numGPCs = 8;
                for (unsigned int i = 0; i < 12; i++) {
                    unsigned int maskbit = fbpmask & (0x1 << i);
                    if (maskbit) {
                        fbConfig0.ltcPerFbpMasks[i] = ltcmask;
                        fbConfig0.fbioPerFbpMasks[i] = ltcmask;
                        fbConfig0.fbpaPerFbpMasks[i] = ltcmask;
                        anyBitSet = true;
                        --numFBPs;
                    }
                }
                for (unsigned int gpcmask = 0; gpcmask <= 0xff; gpcmask++) {
                    gpcConfig0.gpcMask = gpcmask;
                    bool anyGbitSet = false;
                    numGPCs = 8;
                    for (unsigned int j=0; j<8; j++) {
                        unsigned int Gmaskbit = gpcmask & (0x1 << j);
                        if (Gmaskbit) {
                            gpcConfig0.pesPerGpcMasks[j] = 0x7;
                            gpcConfig0.tpcPerGpcMasks[j] = 0xff;
                            anyGbitSet = true;
                            --numGPCs;
                        }
                        else {
                            gpcConfig0.pesPerGpcMasks[j] = 0x0;
                            gpcConfig0.tpcPerGpcMasks[j] = 0x0;                  
                        }
                    }
                    if (anyBitSet || (anyGbitSet && !(fbpmask == 0 && ltcmask !=0))) {    // fbpmask = 0 doesnt let any ltcmask get written
                        if (modename == "FUNC_VALID") {
                            is_valid = fslib::IsFloorsweepingValid(fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID, true, myfile); //always print ugpu
                        }
                        else {
                            is_valid = fslib::IsFloorsweepingValid(fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT, true, myfile);
                        }
                    }
                    else {
                        is_valid = 0; // force is_valid for all-0 configs
                    }
                    if (is_valid) {
                        myfile << std::hex;
                        std::copy ( fbConfig0.ltcPerFbpMasks, (fbConfig0.ltcPerFbpMasks + 12), std::ostream_iterator<unsigned int>(myfile, ",") );
                        myfile << "" << std::hex << fbConfig0.fbpMask;
                        myfile << "," << std::dec << (numFBPs);
                        myfile << "," << std::hex << gpcConfig0.gpcMask;
                        myfile << "," << std::dec << (numGPCs) << ",";
                        myfile << std::hex;
                        std::copy ( gpcConfig0.tpcPerGpcMasks, (gpcConfig0.tpcPerGpcMasks + 8), std::ostream_iterator<unsigned int>(myfile, ",") );
                        myfile << "" << "\n";
                    }
                    msg = "";            // resetting
                    for (unsigned int j=0; j<12; j++) { // resetting
                        gpcConfig0.pesPerGpcMasks[j] = 0x0;
                        gpcConfig0.tpcPerGpcMasks[j] = 0x0;
                    }
                }
                for (unsigned int i = 0; i < 12; i++) { // resetting
                    fbConfig0.ltcPerFbpMasks[i] = 0x0;
                    fbConfig0.fbioPerFbpMasks[i] = 0x0;
                    fbConfig0.fbpaPerFbpMasks[i] = 0x0;
                }
            }
            fbConfig0 = {0};  // resetting
        }
    }
    myfile << std::dec;
    myfile.close();
    //Removing trailing , from every line
    std::cout << "Postprocessing " << newfilename << " ..." << std::endl;
    system((std::string("sed -i 's/,$//' ") + newfilename).c_str());

    return 0;
}
// END GA100 test sweep++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

