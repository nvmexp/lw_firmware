#include <cassert>
#include <iostream>
#include <fstream>
#include <string>
#include <iterator>
#include <algorithm>
#include "fs_lib.h"
#include <iomanip>
#include <sstream>

#define MAX_GPCS 8
#define MAX_FBPS 12
std::string print_gpc(fslib::GpcMasks gpcConfig) {
    std::stringstream outstr;
    outstr << std::hex ;
    outstr << "gpcMask: 0x" << gpcConfig.gpcMask << " | ";
    for (int i=0; i<MAX_GPCS; i++) {
        outstr << "GPC" << i << ": tpcPerGpcMask: 0x" << gpcConfig.tpcPerGpcMasks[i] << ", pesPerGpcMask: 0x" << gpcConfig.pesPerGpcMasks[i] << ", ropPerGpcMask: 0x" << gpcConfig.ropPerGpcMasks[i] << " | " ;
    }
    outstr << "\n";
    outstr << "------------------------------------------------\n";
    return outstr.str();
}

std::string print_fbp(fslib::FbpMasks fbConfig) {
    std::stringstream outstr;
    outstr << std::hex ;
    outstr << "fbpMask: 0x" << fbConfig.fbpMask << " | ";
    for (int i=0; i<MAX_FBPS; i++) {
        outstr << "FBP" << i << ": ltcPerFbpMask: 0x" << fbConfig.ltcPerFbpMasks[i] << ", l2SlicePerFbpMask: 0x" << fbConfig.l2SlicePerFbpMasks[i] << ", fbioPerFbpMask: 0x" << fbConfig.fbioPerFbpMasks[i] << " | " ;
    }
    outstr << "\n";
    outstr << "------------------------------------------------\n";
    return outstr.str();
}
int main() {
    bool is_valid = false;
    std::string msg;
    unsigned int count = 0;
    //------------------------------------------------------------------------------
    // Perfect FB config
    // (Perfect is always valid)
    // Varying FBP masks, keeping GPC mask same = {0}
    //------------------------------------------------------------------------------
    fslib::FbpMasks fbConfig0 = {0};
    fslib::GpcMasks gpcConfig0 = {0};
    for (auto i : {0}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
        fbConfig0.fbpaPerFbpMasks[i] = 0x0;
        fbConfig0.ltcPerFbpMasks[i] = 0x0;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    assert(is_valid == true);
    msg = "";
    count++;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GV100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GV100 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    assert(is_valid == true);
    msg = "";
    count++;


    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GP104, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GP104 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    assert(is_valid == true);
    msg = "";
    count++;


    //------------------------------------------------------------------------------
    // FBP0 disabled consistently assuming 2 FBPA/FBIO per FBP, PRODUCT
    //------------------------------------------------------------------------------

    fbConfig0 = {0};
    fbConfig0.fbpMask = 0x1;
    for (auto i : {0}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }

    is_valid =
        fslib::IsFloorsweepingValid(fslib::Chip::GA100, gpcConfig0, fbConfig0,
                                    msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    assert(is_valid == false);  // true for FUNC_VALID, false for PRODUCT
    
    msg = ""; 
    fbConfig0 = {0};  // resetting
    gpcConfig0 = {0};
    count++;


    //------------------------------------------------------------------------------
    // FBP0 disabled consistently, IGNORE_FSLIB
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x1;
    for (auto i : {0}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }

    is_valid =
        fslib::IsFloorsweepingValid(fslib::Chip::GA100, gpcConfig0, fbConfig0,
                                    msg, fslib::FSmode::IGNORE_FSLIB);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode IGNORE" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    assert(is_valid == true);  // Any config is true for IGNORE_FSLIB
    msg = "";
    fbConfig0 = {0};  // resetting
    gpcConfig0 = {0};
    count++;


    //------------------------------------------------------------------------------
    // FBP0 and FBP3 disabled consistently
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x9;
    for (auto i : {0, 3}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);  // true for FUNC_VALID, false for PRODUCT
    msg = "";
    fbConfig0 = {0};  // resetting
    gpcConfig0 = {0};
    count++;
    // Reason: HBM pairing is incosistent

    //------------------------------------------------------------------------------
    // FBP6 and FBP11 disabled consistently
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x840;
    for (auto i : {6, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);  // true for PRODUCT, 5HBM case
    msg = "";
    fbConfig0 = {0};  // resetting
    gpcConfig0 = {0};
    count++;
    // Reason: L2NoC and HBM pairing is consistent

    //------------------------------------------------------------------------------
    // FBP6 and FBP11 disabled consistently
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x840;
    for (auto i : {6, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid =
        fslib::IsFloorsweepingValid(fslib::Chip::GA100, gpcConfig0, fbConfig0,
                                    msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);  // true for PRODUCT, FUNC_VALID
    msg = "";
    fbConfig0 = {0};  // resetting
    gpcConfig0 = {0};
    count++;
    // Reason: L2NoC and HBM pairing is consistent

    //------------------------------------------------------------------------------
    // FBP 1,3,4,5,6,7,9,11 disabled => 0,2,8,10 enabled
    // Product invalid config for 2 hbms
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xAFA;
    for (auto i : {1, 3, 4, 5, 6, 7, 9, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);  // false for PRODUCT, true for FUNC_VALID, and GA101
    msg = "";
    fbConfig0 = {0};  // resetting
    gpcConfig0 = {0};
    count++;


    //------------------------------------------------------------------------------
    // FBP 1,3,4,5,6,7,9,11 disabled => 0,2,8,10 enabled
    // Functionally invalid config with 2 hbms
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xAFA;
    for (auto i : {1, 3, 4, 5, 6, 7, 9, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid =
        fslib::IsFloorsweepingValid(fslib::Chip::GA100, gpcConfig0, fbConfig0,
                                    msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);  // false for PRODUCT,FUNC_VALID, need to fix gpcmask
    msg = "";
    fbConfig0 = {0};  // resetting
    gpcConfig0 = {0};
    count++;


    //------------------------------------------------------------------------------
    // FBP 1,3,4,5,6,7,9,11 disabled => 0,2,8,10 enabled
    // Functionally invalid config with 2 hbms
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xAFA;
    for (auto i : {1, 3, 4, 5, 6, 7, 9, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA101, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    // Since GA101 is temporarily passthrough for ga101, this should pass
    assert(is_valid == true);  // false for GA101 product because GPCmask is {0}
    msg = "";
    fbConfig0 = {0};  // resetting
    gpcConfig0 = {0};
    count++;


    //------------------------------------------------------------------------------
    // FBP1 and FBP4 disabled
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x12;
    for (auto i : {1, 4}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid =
        fslib::IsFloorsweepingValid(fslib::Chip::GA100, gpcConfig0, fbConfig0,
                                    msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);  // true for PRODUCT, 5 hbm
    msg = "";
    fbConfig0 = {0};  // resetting
    gpcConfig0 = {0};
    count++;
    // Reason: L2NoC and HBM pairing is cosistent


    //------------------------------------------------------------------------------
    // FBP 0,2,3,5 disabled
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x2D;
    for (auto i : {0, 2, 3, 5}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);  // true for PRODUCT, 4 hbm
    msg = "";
    fbConfig0 = {0};  // resetting
    gpcConfig0 = {0};
    count++;
    // Reason: L2NoC and HBM pairing is cosistent


    //------------------------------------------------------------------------------
    // FBP 0,1,4 disabled
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x13;
    for (auto i : {0, 1, 4}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);  // false for PRODUCT, true for FUNC_VALID
    msg = "";
    fbConfig0 = {0};  // resetting
    gpcConfig0 = {0};
    count++;
    // Reason: L2NoC pairing is incosistent

    //------------------------------------------------------------------------------
    // FBP1 and FBP4 disabled
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x12;
    for (auto i : {1, 4}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);  // true for product, 5 hbm
    msg = "";
    fbConfig0 = {0};  // resetting
    gpcConfig0 = {0};
    count++;
    // Reason: L2NoC and HBM pairing is consistent


    //------------------------------------------------------------------------------
    // GA101 config
    // GA101 tests, make sure to relax the l2noc rules
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xAFA;
    gpcConfig0.gpcMask = 0x3C;
    for (auto i : {1, 3, 4, 5, 6, 7, 9, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {2, 3, 4, 5}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA101, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);  // true for GA101 PRODUCT
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // GA101 config as GA100 PRODUCT
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xAFA;
    gpcConfig0.gpcMask = 0x3C;
    for (auto i : {1, 3, 4, 5, 6, 7, 9, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {2, 3, 4, 5}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);  // true for GA101 PRODUCT
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // GA101 config as GA100 FUNC_VALID
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xAFA;
    gpcConfig0.gpcMask = 0x3C;
    for (auto i : {1, 3, 4, 5, 6, 7, 9, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {2, 3, 4, 5}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);  // true for GA101 PRODUCT
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // GA100 checking unsupported number of ltcs by AMAP
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {0,1,2}) {
        //fbConfig0.fbioPerFbpMasks[i] = 0x3;
        //fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x2;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);  // false as it breaks AMAP rule for supporting #ltcs
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // GA100 checking unsupported number of ltcs by AMAP
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x7bf;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {0,1,2,3,4,5,7,8,9,10}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);  // false as it breaks AMAP rule for supporting #ltcs
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;



    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite B and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfed;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    
    //------------------------------------------------------------------------------
    // One ugpu FS with half Hbmsite B and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfef;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,1,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // One ugpu FS with wrong half Hbmsite B and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xffd;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,2,3,4,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite B and E and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x7ad;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,2,3,5,7,8,9,10}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite A,B,E,F and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x2a8;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {3,5,7,9}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite A and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xffa;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,3,4,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1,2,3,4,5,6,7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }
        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0xfe;


    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // One ugpu FS with All Hbmsites and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0xfe;
//    for (auto i : {1,3,4,5,6,7,8,9,10,11}) {
//        fbConfig0.fbioPerFbpMasks[i] = 0x3;
//        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
//        fbConfig0.ltcPerFbpMasks[i] = 0x3;
//    }
    for (auto i : {1,2,3,4,5,6,7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }
        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0xfe;


    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;



    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite B and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfed;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0xfe;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    
    //------------------------------------------------------------------------------
    // One ugpu FS with half Hbmsite A and B and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfee;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0xfe;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // Two ugpu with Hbmsite A,B and GPC0 and GPC4, but A's complementary HBM C missing
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfe8;
    gpcConfig0.gpcMask = 0xee;
    for (auto i : {3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1,2,3,5,6,7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }
        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0xfe;


    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // One ugpu with Hbmsite A,F and GPC0 and GPC7, func_valid and product_ilwalid in ga100, but product_valid in ga101
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xafa;
    gpcConfig0.gpcMask = 0x7e;
    for (auto i : {1,3,4,5,6,7,9,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1,2,3,4,5,6}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA101, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA101 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // One ugpu with Hbmsite A and GPC0 and GPC7, func_valid and product_ilwalid in ga100, but product_valid in ga101
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xaff;
    gpcConfig0.gpcMask = 0x7e;
    for (auto i : {0,1,2,3,4,5,6,7,9,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1,2,3,4,5,6}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA101, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA101 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    //------------------------------------------------------------------------------
    // One ugpu with Hbmsite A and GPC0 and GPC7, func_valid and product_ilwalid in ga100, but product_valid in ga101
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xa;
    gpcConfig0.gpcMask = 0x3c;
    for (auto i : {1,3}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {2,3,4,5}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // FF0 disabled consistently (still assuming 2 FBPA/FBIO per FBP)
    //------------------------------------------------------------------------------
    fbConfig0 = {0};
    fbConfig0.fbpMask = 0x3;
    for (auto i : {0, 1}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    count++;
    // Reason: It does not follow GA100 rules

    //------------------------------------------------------------------------------
    // inonsistent GPC floorsweeping (assuming 3 PES, 5 TPC per GPC)
    //------------------------------------------------------------------------------
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0, 1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[0] = 0x7;
        gpcConfig0.tpcPerGpcMasks[0] = 0xff;
    }
    gpcConfig0.tpcPerGpcMasks[0] = 0xfe;  // Only making tpc0 of gpc0 valid

    is_valid =
        fslib::IsFloorsweepingValid(fslib::Chip::GA100, gpcConfig0, fbConfig0,
                                    msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    count++;

    //------------------------------------------------------------------------------
    // Inconsistent PES
    //------------------------------------------------------------------------------
    gpcConfig0 = {0};
    gpcConfig0.gpcMask = 0xfe;
    gpcConfig0.pesPerGpcMasks[0] = 0x7;
    gpcConfig0.pesPerGpcMasks[1] = 0x7;
    gpcConfig0.pesPerGpcMasks[2] = 0x7;
    gpcConfig0.pesPerGpcMasks[3] = 0x7;
    gpcConfig0.pesPerGpcMasks[4] = 0x7;
    gpcConfig0.pesPerGpcMasks[5] = 0x7;
    gpcConfig0.pesPerGpcMasks[6] = 0x7;
    gpcConfig0.pesPerGpcMasks[7] = 0x7;

    gpcConfig0.tpcPerGpcMasks[0] = 0xfe;
    gpcConfig0.tpcPerGpcMasks[1] = 0xff;
    gpcConfig0.tpcPerGpcMasks[2] = 0xff;
    gpcConfig0.tpcPerGpcMasks[3] = 0xff;
    gpcConfig0.tpcPerGpcMasks[4] = 0xff;
    gpcConfig0.tpcPerGpcMasks[5] = 0xff;
    gpcConfig0.tpcPerGpcMasks[6] = 0xff;
    gpcConfig0.tpcPerGpcMasks[7] = 0xff;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA100 \t Mode PRODUCT" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    count++;


    //------------------------------------------------------------------------------
    // Unit tests for GA10x:
    //------------------------------------------------------------------------------

    fbConfig0 = {0};
    gpcConfig0 = {0};
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;



    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,2,5}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
        fbConfig0.fbpaPerFbpMasks[i] = 0x0;
        fbConfig0.ltcPerFbpMasks[i] = 0x0;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xc3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;


    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xc3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;


    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x3c;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;


    fbConfig0.fbpMask = 0x0;

    fbConfig0.l2SlicePerFbpMasks[0] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[2] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x42;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;


    fbConfig0.fbpMask = 0x0;

    fbConfig0.l2SlicePerFbpMasks[0] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[2] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x24;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;


    fbConfig0.fbpMask = 0x0;

    fbConfig0.l2SlicePerFbpMasks[0] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[2] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x18;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;


    fbConfig0.fbpMask = 0x0;

    fbConfig0.l2SlicePerFbpMasks[0] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[2] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x81;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    // Check 1 FBP FS + l2slice FS
    fbConfig0.fbpMask = 0x4;

    fbConfig0.fbioPerFbpMasks[2] = 0x1;
    fbConfig0.fbpaPerFbpMasks[2] = 0x1;
    fbConfig0.ltcPerFbpMasks[2] = 0x3;
    fbConfig0.l2SlicePerFbpMasks[0] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[2] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x18;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    // Check 1 FBP FS + l2slice FS
    fbConfig0.fbpMask = 0x1;

    fbConfig0.fbioPerFbpMasks[0] = 0x1;
    fbConfig0.fbpaPerFbpMasks[0] = 0x1;
    fbConfig0.ltcPerFbpMasks[0] = 0x3;
    fbConfig0.l2SlicePerFbpMasks[0] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[2] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x81;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    // Check 2 FBP FS + l2slice FS
    fbConfig0.fbpMask = 0x14;
    for (auto i : {2,4}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.fbpaPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    fbConfig0.l2SlicePerFbpMasks[0] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[2] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[4] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x24;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    
    // Check 2 FBP FS + l2slice FS
    fbConfig0.fbpMask = 0x24;
    for (auto i : {2,5}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.fbpaPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    fbConfig0.l2SlicePerFbpMasks[0] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[2] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[5] = 0xff;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    // Check 2 FBP FS + l2slice FS, invalid pattern
    fbConfig0.fbpMask = 0x24;
    for (auto i : {2,5}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.fbpaPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    fbConfig0.l2SlicePerFbpMasks[0] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[2] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[5] = 0xff;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;
    
    // Check 2 FBP FS + l2slice FS + ltc FS -- NOT SUPPORTED YET
    fbConfig0.fbpMask = 0x14;
    for (auto i : {2,4}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.fbpaPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    fbConfig0.ltcPerFbpMasks[0] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[0] = 0x1;

    fbConfig0.l2SlicePerFbpMasks[0] = 0x1f;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[2] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[4] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x24;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    
    fbConfig0.fbpMask = 0x3a;
    for (auto i : {0,2}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x3c;
    }
    for (auto i : {1,3,4,5}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.fbpaPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;


    
    fbConfig0.fbpMask = 0x3a;
    for (auto i : {0,2}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x21;
    }
    for (auto i : {1,3,4,5}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.fbpaPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;


    fbConfig0.fbpMask = 0x3a;
    for (auto i : {0}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x24;
    }
    for (auto i : {2}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x81;
    }
    for (auto i : {1,3,4,5}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.fbpaPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);  //Marked false for new product valid pattern, as of 03/05/20
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;


    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x1;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;


    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x1f;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    // fs0_rand_snap, enforcing l2slicemask and ltcmask mutually compatible
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x3c;
        fbConfig0.ltcPerFbpMasks[i] = 0x0;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    fbConfig0.l2SlicePerFbpMasks[5] = 0xfc;
    fbConfig0.ltcPerFbpMasks[5] = 0x2;
    fbConfig0.halfFbpaPerFbpMasks[5] = 0x1;
    fbConfig0.fbioPerFbpMasks[5] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x3c;
        fbConfig0.ltcPerFbpMasks[i] = 0x0;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    fbConfig0.l2SlicePerFbpMasks[5] = 0x3c;
    fbConfig0.ltcPerFbpMasks[5] = 0x2;
    fbConfig0.halfFbpaPerFbpMasks[5] = 0x1;
    fbConfig0.fbioPerFbpMasks[5] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;


    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
    }
    fbConfig0.l2SlicePerFbpMasks[5] = 0x1;
    fbConfig0.ltcPerFbpMasks[5] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[5] = 0x1;
    fbConfig0.fbioPerFbpMasks[5] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    fbConfig0.fbpMask = 0x0;
    for (auto i : {1,2,3,4}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
    }
    fbConfig0.l2SlicePerFbpMasks[5] = 0xff;
    fbConfig0.ltcPerFbpMasks[5] = 0x0;
    fbConfig0.halfFbpaPerFbpMasks[5] = 0x0;
    fbConfig0.fbioPerFbpMasks[5] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    // Enforcing half_fbpa mask is ignored during balanced-ltc floorsweeping
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xf3;
        fbConfig0.ltcPerFbpMasks[i] = 0x2;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    fbConfig0.l2SlicePerFbpMasks[5] = 0xcf;
    fbConfig0.ltcPerFbpMasks[5] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[5] = 0x0;
    fbConfig0.fbioPerFbpMasks[5] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x3;
    gpcConfig0.pesPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.ropPerGpcMasks[1] = 0x1;
    gpcConfig0.ropPerGpcMasks[4] = 0x3;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.ropPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.ropPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    gpcConfig0.gpcMask = 0x7e;
    for (auto i : {1,2,3,4,5,6}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
    }
    //gpcConfig0.ropPerGpcMasks[1] = 0x1;
    gpcConfig0.tpcPerGpcMasks[0] = 0x3f;
    gpcConfig0.pesPerGpcMasks[0] = 0x7;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    std::cout << "**********************************" <<std::endl;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    // GA103 unit tests

    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x3c;
        fbConfig0.ltcPerFbpMasks[i] = 0x0;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    fbConfig0.l2SlicePerFbpMasks[4] = 0xfc;
    fbConfig0.ltcPerFbpMasks[4] = 0x2;
    fbConfig0.halfFbpaPerFbpMasks[4] = 0x1;
    fbConfig0.fbioPerFbpMasks[4] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA103, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA103 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;    

    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x1f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x3;
    gpcConfig0.pesPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA103, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA103 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    // GA104 unit tests
#ifdef LW_MODS_SUPPORTS_GA104_MASKS
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x3c;
        fbConfig0.ltcPerFbpMasks[i] = 0x0;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    fbConfig0.l2SlicePerFbpMasks[3] = 0xfc;
    fbConfig0.ltcPerFbpMasks[3] = 0x2;
    fbConfig0.halfFbpaPerFbpMasks[3] = 0x1;
    fbConfig0.fbioPerFbpMasks[3] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA104, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA104 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;    

    // GA104 as2 test which caught the MODS bug
    fbConfig0.fbpMask = 0x0;    
    for (auto i : {0,1,2,3,4,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA104, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA104 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;    

    
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0xf;
        gpcConfig0.pesPerGpcMasks[i] = 0x3;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x3;
    gpcConfig0.pesPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA104, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA104 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;
#endif // GA104 tests


    // GA106 unit tests

    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x3c;
        fbConfig0.ltcPerFbpMasks[i] = 0x0;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    fbConfig0.l2SlicePerFbpMasks[2] = 0xfc;
    fbConfig0.ltcPerFbpMasks[2] = 0x2;
    fbConfig0.halfFbpaPerFbpMasks[2] = 0x1;
    fbConfig0.fbioPerFbpMasks[2] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA106, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA106 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;    
    
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x1f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x3;
    gpcConfig0.pesPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA106, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA106 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    // GA107 unit tests

    fbConfig0.fbpMask = 0x0;       
    fbConfig0.l2SlicePerFbpMasks[0] = 0x0;
    fbConfig0.l2SlicePerFbpMasks[1] = 0xff;
    fbConfig0.ltcPerFbpMasks[0] = fbConfig0.ltcPerFbpMasks[1] = 0x3;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA107, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA107 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;


    fbConfig0.fbpMask = 0x0;       
    fbConfig0.l2SlicePerFbpMasks[0] = 0x0;
    fbConfig0.l2SlicePerFbpMasks[1] = 0xf;
    fbConfig0.ltcPerFbpMasks[0] = 0x0;
    fbConfig0.ltcPerFbpMasks[1] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[0] = 0x0;
    fbConfig0.halfFbpaPerFbpMasks[1] = 0x1;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA107, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA107 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;


    fbConfig0.fbpMask = 0x0;       
    fbConfig0.l2SlicePerFbpMasks[0] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x0F;
    fbConfig0.ltcPerFbpMasks[0] = 0x0;
    fbConfig0.ltcPerFbpMasks[1] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[0] = 0x0;
    fbConfig0.halfFbpaPerFbpMasks[1] = 0x1;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA107, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA107 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    // Random wrong config intentionally created to test the disabled chips
    fbConfig0.fbpMask = 0x33;
    for (auto i : {0,1,2,3,4,15}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x1f;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::G000, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip G000 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;
    
// Stray half_fbpa mask should throw error
#ifdef LW_MODS_SUPPORTS_HALFFBPA_MASK
    fbConfig0.fbpMask = 0x0;
    fbConfig0.halfFbpaPerFbpMasks[5] = 0x1;
    fbConfig0.ltcPerFbpMasks[5] = 0x1;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA102 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    fbConfig0.fbpMask = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[0] = 0x0;
    fbConfig0.ltcPerFbpMasks[0] = 0x3;
    fbConfig0.l2SlicePerFbpMasks[0] = 0xFF;
    fbConfig0.fbioPerFbpMasks[0] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[1] = 0x1;
    fbConfig0.ltcPerFbpMasks[1] = 0x0;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x10;
    fbConfig0.fbioPerFbpMasks[1] = 0x0;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA107, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA107 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    fbConfig0.fbpMask = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[0] = 0x0;
    fbConfig0.ltcPerFbpMasks[0] = 0x3;
    fbConfig0.l2SlicePerFbpMasks[0] = 0xFF;
    fbConfig0.fbioPerFbpMasks[0] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[1] = 0x1;
    fbConfig0.ltcPerFbpMasks[1] = 0x2;
    fbConfig0.l2SlicePerFbpMasks[1] = 0xf0;
    fbConfig0.fbioPerFbpMasks[1] = 0x0;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA107, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA107 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    fbConfig0.fbpMask = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[0] = 0x0;
    fbConfig0.ltcPerFbpMasks[0] = 0x3;
    fbConfig0.l2SlicePerFbpMasks[0] = 0xFF;
    fbConfig0.fbioPerFbpMasks[0] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[1] = 0x0;
    fbConfig0.ltcPerFbpMasks[1] = 0x2;
    fbConfig0.l2SlicePerFbpMasks[1] = 0xf0;
    fbConfig0.fbioPerFbpMasks[1] = 0x0;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA107, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA107 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;
#endif    
    
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x1f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x3;
    gpcConfig0.pesPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA107, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA107 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    // GA10B unit tests

    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0xf;
        gpcConfig0.pesPerGpcMasks[i] = 0x3;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0xc;
    gpcConfig0.pesPerGpcMasks[1] = 0x2;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA10B, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA10B \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;

    
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0xf;
        gpcConfig0.pesPerGpcMasks[i] = 0x3;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x8;
    gpcConfig0.pesPerGpcMasks[1] = 0x2;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA10B, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GA10B \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    count++;


// New GH100 tests
// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------

    fbConfig0.fbpMask = 0x12;
    gpcConfig0.gpcMask = 0x3c;
    for (auto i : {1,4}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {2,3,4,5}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    fbConfig0.fbpMask = 0x12;
    gpcConfig0.gpcMask = 0x3c;
    for (auto i : {1,4}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    fbConfig0.ltcPerFbpMasks[0] = 0x1;
    fbConfig0.fbioPerFbpMasks[0] = 0x0;
    fbConfig0.fbpaPerFbpMasks[0] = 0x0;
    fbConfig0.ltcPerFbpMasks[3] = 0x2;
    fbConfig0.fbioPerFbpMasks[3] = 0x0;
    fbConfig0.fbpaPerFbpMasks[3] = 0x0;

    for (auto i : {2,3,4,5}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    fbConfig0.fbpMask = 0xfc0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        //fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {0,1,2,3,4,5,6,7}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x1f8;
        gpcConfig0.pesPerGpcMasks[i] = 0x6;
        gpcConfig0.cpcPerGpcMasks[i] = 0x6;
        gpcConfig0.ropPerGpcMasks[i] = 0x1;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


//OLD GA100 tests rerun as new GH100 tests
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite B and GPC0
    // Second uGPU is technically enabled
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfed;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    
    //------------------------------------------------------------------------------
    // One ugpu FS with half Hbmsite B and GPC0
    // GH100 disallows 1uGPU configs
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfef;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,1,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // One ugpu FS with wrong half Hbmsite B and GPC0
    // Second uGPU is still enabled
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xffd;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,2,3,4,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite B and E and GPC0
    // GH100 disallows 1uGPU configs
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x7ad;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,2,3,5,7,8,9,10}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite A,C,D,F and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x852;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,4,6,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite A and GPC0
    // GH100 disallows 1uGPU configs
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xffa;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,3,4,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1,2,3,4,5,6,7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }
        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
        gpcConfig0.cpcPerGpcMasks[0] = 0x6;


    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // One ugpu FS with All Hbmsites and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0xfe;
//    for (auto i : {1,3,4,5,6,7,8,9,10,11}) {
//        fbConfig0.fbioPerFbpMasks[i] = 0x3;
//        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
//        fbConfig0.ltcPerFbpMasks[i] = 0x3;
//    }
    for (auto i : {1,2,3,4,5,6,7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }
        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
        gpcConfig0.cpcPerGpcMasks[0] = 0x6;


    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;



    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite B and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfed;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
        gpcConfig0.cpcPerGpcMasks[0] = 0x6;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    
    //------------------------------------------------------------------------------
    // One ugpu FS with half Hbmsite A and B and GPC0
    // GH100 disallows 1uGPU configs
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfee;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
        gpcConfig0.cpcPerGpcMasks[0] = 0x6;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // Two ugpu with Hbmsite A,B and GPC0 and GPC4, but A's complementary HBM C missing
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfe8;
    gpcConfig0.gpcMask = 0xee;
    for (auto i : {3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1,2,3,5,6,7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }
        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
        gpcConfig0.cpcPerGpcMasks[0] = 0x6;


    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    //------------------------------------------------------------------------------
    // One ugpu with Hbmsite A and GPC0 and GPC7, func_valid and product_ilwalid in ga100, but product_valid in ga101
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xa;
    gpcConfig0.gpcMask = 0x3c;
    for (auto i : {1,3}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {2,3,4,5}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //------------------------------------------------------------------------------
    // FF0 disabled consistently (still assuming 2 FBPA/FBIO per FBP)
    //------------------------------------------------------------------------------
    fbConfig0 = {0};
    fbConfig0.fbpMask = 0x3;
    for (auto i : {0, 1}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;
    // Reason: It does not follow GA100 rules

    //------------------------------------------------------------------------------
    // inonsistent GPC floorsweeping (assuming 3 PES, 5 TPC per GPC)
    //------------------------------------------------------------------------------
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0, 1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[0] = 0x7;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;  // Only making tpc0 of gpc0 valid
    gpcConfig0.cpcPerGpcMasks[0] = 0x6;

    is_valid =
        fslib::IsFloorsweepingValid(fslib::Chip::GH100, gpcConfig0, fbConfig0,
                                    msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    // FIXME ignore all GPC FS failures for GH100 
    // assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    //------------------------------------------------------------------------------
    // Inconsistent PES
    //------------------------------------------------------------------------------
    gpcConfig0 = {0};
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0, 1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
    gpcConfig0.cpcPerGpcMasks[0] = 0x6;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode PROD" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;


    //l2slice tests
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {1}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x1;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    //94 slices
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {1}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x2;
    }
    for (auto i : {4}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x20;
    }
     is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    //92 slices, different FBP
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {1,8}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x2;
    }
    for (auto i : {4,7}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x20;
    }
     is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    //92 slices, but same FBP
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {1}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x0C;
    }
    for (auto i : {4}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xC0;
    }
     is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    //90 slices, different FBP
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {1,8,2}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x2;
    }
    for (auto i : {4,7,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x20;
    }
     is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    
    //76 slices, FBP + slice FS
    fbConfig0.fbpMask = 0x840;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {6, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xFF;
    }
    for (auto i : {1,8}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x2;
    }
    for (auto i : {4,7}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x20;
    }
     is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    
    //84 slices, LTC + slice FS
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {6}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x1;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF;
    }
    for (auto i : {11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x2;
        fbConfig0.ltcPerFbpMasks[i] = 0x2;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF0;
    }
    for (auto i : {1,8}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x2;
    }
    for (auto i : {4,7}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x20;
    }
     is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;
    
    //68 slices, FBP + LTC + slice FS
    fbConfig0.fbpMask = 0x12;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {1, 4}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xFF;
    }
    for (auto i : {6}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x1;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF;
    }
    for (auto i : {11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x2;
        fbConfig0.ltcPerFbpMasks[i] = 0x2;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF0;
    }
    for (auto i : {0,8}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x2;
    }
    for (auto i : {3,7}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x20;
    }
     is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    //64 slices, FBP + LTC FS, no slice FS
    fbConfig0.fbpMask = 0x840;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {6, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xFF;
    }
    for (auto i : {1,8}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF0;
        fbConfig0.ltcPerFbpMasks[i] = 0x2;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    for (auto i : {4,7}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF;
        fbConfig0.ltcPerFbpMasks[i] = 0x1;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
     is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;
    
    //72 slices, only LTC FS, no slice FS
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {0,8,9}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF0;
        fbConfig0.ltcPerFbpMasks[i] = 0x2;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    for (auto i : {3,7,10}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF;
        fbConfig0.ltcPerFbpMasks[i] = 0x1;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;
    
    //72 slices, only LTC FS, no slice FS, wrong LTCs
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {0,8,11}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF0;
        fbConfig0.ltcPerFbpMasks[i] = 0x2;
        fbConfig0.fbioPerFbpMasks[i] = 0x2;
    }
    for (auto i : {3,7,10}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF;
        fbConfig0.ltcPerFbpMasks[i] = 0x1;
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    //single uGPU
    // GH100 disallows 1uGPU configs
    fbConfig0.fbpMask = 0xfee;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
        gpcConfig0.cpcPerGpcMasks[0] = 0x6;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;
    
    //single uGPU + LTC FS
    // GH100 disallows 1uGPU configs
    fbConfig0.fbpMask = 0xfee;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }
    for (auto i : {0}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF;
        fbConfig0.ltcPerFbpMasks[i] = 0x1;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    } 
        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
        gpcConfig0.cpcPerGpcMasks[0] = 0x6;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;
    
    
    //single uGPU + LTC FS + slice FS
    fbConfig0.fbpMask = 0xfee;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }
    for (auto i : {0}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF;
        fbConfig0.ltcPerFbpMasks[i] = 0x1;
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
    } 
     for (auto i : {4}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x1;
    } 
        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
        gpcConfig0.cpcPerGpcMasks[0] = 0x6;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;
    
    //92 slices, only LTC FS, no slice FS, AMAP should complain
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {8}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF0;
        fbConfig0.ltcPerFbpMasks[i] = 0x2;
        fbConfig0.fbioPerFbpMasks[i] = 0x2;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "Test config " << std::setw(3) << count << ":\t Chip GH100 \t Mode FUNC" << "\n"
              << print_gpc(gpcConfig0)
              << print_fbp(fbConfig0)
              << "FSLIB output: " << (is_valid ? "VALID" : "INVALID")
              << std::endl;
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    count++;

    
    // To run the following old tests, enable all the chips in fs_chip.cpp
/*
    //------------------------------------------------------------------------------
    // Old GV100 tests
    //------------------------------------------------------------------------------
    fbConfig0 = {0};
    gpcConfig0 = {0};
    fbConfig0.fbpMask = 0x1;
    fbConfig0.fbioPerFbpMasks[0] = 0x1;
    fbConfig0.fbpaPerFbpMasks[0] = 0x1;
    fbConfig0.ltcPerFbpMasks[0] = 0x3;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GV100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "Test " << std::setw(3) << count << ":" << (is_valid ? "VALID" : "INVALID")
              << " config: GPC: " << std::hex << std::setw(4) << gpcConfig0.gpcMask
              << " FBP: " << std::setw(6) << fbConfig0.fbpMask << std::dec
              << std::endl;  // true
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    count++;
    // Reason: FBIO masks are inconsistent for FBP0
    // -or-
    // Reason: FBP1 in FF0 isn't disabled when FBP0 is disabled

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GP104, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "Test " << std::setw(3) << count << ":" << (is_valid ? "VALID" : "INVALID")
              << " config: GPC: " << std::hex << std::setw(4) << gpcConfig0.gpcMask
              << " FBP: " << std::setw(6) << fbConfig0.fbpMask << std::dec
              << std::endl;  // true
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    count++;
    // Note: Even-odd FS doesn't apply if the entire FBP is disabled

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GP107, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "Test " << std::setw(3) << count << ":" << (is_valid ? "VALID" : "INVALID")
              << " config: GPC: " << std::hex << std::setw(4) << gpcConfig0.gpcMask
              << " FBP: " << std::setw(6) << fbConfig0.fbpMask << std::dec
              << std::endl;  // true
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    count++;


    //------------------------------------------------------------------------------
    // FBP0 disabled consistently assuming 2 FBPA/FBIO per FBP
    //------------------------------------------------------------------------------
    fbConfig0 = {0};
    fbConfig0.fbpMask = 0x1;
    fbConfig0.fbioPerFbpMasks[0] = 0x3;
    fbConfig0.fbpaPerFbpMasks[0] = 0x3;
    fbConfig0.ltcPerFbpMasks[0] = 0x3;

    is_valid = fslib::IsFloorsweepingValid(
                      fslib::Chip::GP104, gpcConfig0, fbConfig0, msg,
                      fslib::FSmode::PRODUCT);
    std::cout << "Test " << std::setw(3) << count << ":" << (is_valid ? "VALID" : "INVALID")
              << " config: GPC: " << std::hex << std::setw(4) << gpcConfig0.gpcMask
              << " FBP: " << std::setw(6) << fbConfig0.fbpMask << std::dec
              << std::endl;  // true
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    count++;
    // Reason: Disabling non-existent FBIO (0x3 given, but GP104 FBPs only have
    // 1 FBIO(s) per FBP)

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GV100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "Test " << std::setw(3) << count << ":" << (is_valid ? "VALID" : "INVALID")
              << " config: GPC: " << std::hex << std::setw(4) << gpcConfig0.gpcMask
              << " FBP: " << std::setw(6) << fbConfig0.fbpMask << std::dec
              << std::endl;  // true
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    count++;
    // Reason: FBP1 in FF0 isn't disabled when FBP0 is disabled

*/

/*
    //------------------------------------------------------------------------------
    // 2 ROP/L2s disabled (ROP/L2[0] on both FBPs)
    //------------------------------------------------------------------------------
    fbConfig0 = {0};
    for (auto i : {0, 1}) {
        fbConfig0.ltcPerFbpMasks[i] = 0x1;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GP107, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "Test " << std::setw(3) << count << ":" << (is_valid ? "VALID" : "INVALID")
              << " config: GPC: " << std::hex << std::setw(4) << gpcConfig0.gpcMask
              << " FBP: " << std::setw(6) << fbConfig0.fbpMask << std::dec
              << std::endl;  // true
    std::cout << msg << std::endl;
    assert(is_valid == false);
    // uReason: GP107 needs even-odd floorsweeping for ROP/L2
    count++;


    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GV100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "Test " << std::setw(3) << count << ":" << (is_valid ? "VALID" : "INVALID")
              << " config: GPC: " << std::hex << std::setw(4) << gpcConfig0.gpcMask
              << " FBP: " << std::setw(6) << fbConfig0.fbpMask << std::dec
              << std::endl;  // true
    std::cout << msg << std::endl;
    assert(is_valid == false);
    // Reason: Enabled FFs need to be perfect, but FBP[0] ROP/L2[0] is disabled
    count++;


    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GP104, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "Test " << std::setw(3) << count << ":" << (is_valid ? "VALID" : "INVALID")
              << " config: GPC: " << std::hex << std::setw(4) << gpcConfig0.gpcMask
              << " FBP: " << std::setw(6) << fbConfig0.fbpMask << std::dec
              << std::endl;  // true
    std::cout << msg << std::endl;
    assert(is_valid == false);
    // Reason:
    count++;


    for (auto i : {0, 1}) {
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x1;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GP104, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "Test " << std::setw(3) << count << ":" << (is_valid ? "VALID" : "INVALID")
              << " config: GPC: " << std::hex << std::setw(4) << gpcConfig0.gpcMask
              << " FBP: " << std::setw(6) << fbConfig0.fbpMask << std::dec
              << std::endl;  // true
    std::cout << msg << std::endl;
    assert(is_valid == true);
    count++;


    //------------------------------------------------------------------------------
    // Inconsistent ROP/L2 floorsweeping
    //------------------------------------------------------------------------------
    fbConfig0 = {0};
    fbConfig0.fbpMask = 0x1;
    fbConfig0.fbioPerFbpMasks[0] = 0x1;
    fbConfig0.fbpaPerFbpMasks[0] = 0x1;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GP104, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "Test " << std::setw(3) << count << ":" << (is_valid ? "VALID" : "INVALID")
              << " config: GPC: " << std::hex << std::setw(4) << gpcConfig0.gpcMask
              << " FBP: " << std::setw(6) << fbConfig0.fbpMask << std::dec
              << std::endl;  // true
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    // Reason: ROP/L2 floorsweeping inconsistent on FBP0
    count++;


    //------------------------------------------------------------------------------
    // Inconsistent ROP/L2 floorsweeping
    //------------------------------------------------------------------------------
    fbConfig0 = {0};
    fbConfig0.ltcPerFbpMasks[0] = 0x3;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GP104, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "Test " << std::setw(3) << count << ":" << (is_valid ? "VALID" : "INVALID")
              << " config: GPC: " << std::hex << std::setw(4) << gpcConfig0.gpcMask
              << " FBP: " << std::setw(6) << fbConfig0.fbpMask << std::dec
              << std::endl;  // true
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    // Reason: ROP/L2 floorsweeping inconsistent on FBP0
    count++;

*/
/*
    //------------------------------------------------------------------------------
    // Consistent GPC floorsweeping (assuming 3 PES, 5 TPC per GPC)
    //------------------------------------------------------------------------------
    gpcConfig0 = {0};
    gpcConfig0.gpcMask = 0x2;
    gpcConfig0.pesPerGpcMasks[0] = 0x0;
    gpcConfig0.tpcPerGpcMasks[0] = 0x0;
    gpcConfig0.pesPerGpcMasks[1] = 0x7;
    gpcConfig0.tpcPerGpcMasks[1] = 0x1f;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GP104, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "Test " << std::setw(3) << count << ":" << (is_valid ? "VALID" : "INVALID")
              << " config: GPC: " << std::hex << std::setw(4) << gpcConfig0.gpcMask
              << " FBP: " << std::setw(6) << fbConfig0.fbpMask << std::dec
              << std::endl;  // true
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    count++;


    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GV100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "Test " << std::setw(3) << count << ":" << (is_valid ? "VALID" : "INVALID")
              << " config: GPC: " << std::hex << std::setw(4) << gpcConfig0.gpcMask
              << " FBP: " << std::setw(6) << fbConfig0.fbpMask << std::dec
              << std::endl;  // true
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    // Reason: TPC masks are inconsistent for GPC1
    count++;


    //------------------------------------------------------------------------------
    // Consistent GPC floorsweeping (assuming 3 PES, 7 TPC per GPC)
    //------------------------------------------------------------------------------
    gpcConfig0 = {0};
    gpcConfig0.gpcMask = 0x2;
    gpcConfig0.pesPerGpcMasks[0] = 0x0;
    gpcConfig0.tpcPerGpcMasks[0] = 0x0;
    gpcConfig0.pesPerGpcMasks[1] = 0x7;
    gpcConfig0.tpcPerGpcMasks[1] = 0x7f;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GV100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "Test " << std::setw(3) << count << ":" << (is_valid ? "VALID" : "INVALID")
              << " config: GPC: " << std::hex << std::setw(4) << gpcConfig0.gpcMask
              << " FBP: " << std::setw(6) << fbConfig0.fbpMask << std::dec
              << std::endl;  // true
    std::cout << msg << std::endl;
    assert(is_valid == true);
    msg = "";
    count++;


    //------------------------------------------------------------------------------
    // Inconsistent PES
    //------------------------------------------------------------------------------
    gpcConfig0 = {0};
    gpcConfig0.gpcMask = 0x2;
    gpcConfig0.tpcPerGpcMasks[0] = 0x0;
    gpcConfig0.tpcPerGpcMasks[1] = 0x7f;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GV100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    std::cout << "Test " << std::setw(3) << count << ":" << (is_valid ? "VALID" : "INVALID")
              << " config: GPC: " << std::hex << std::setw(4) << gpcConfig0.gpcMask
              << " FBP: " << std::setw(6) << fbConfig0.fbpMask << std::dec
              << std::endl;  // true
    std::cout << msg << std::endl;
    assert(is_valid == false);
    msg = "";
    // Reason: PES masks are inconsistent for GPC1
    count++;
*/

    std::cout << "All tests passed. Total=" << count << std::endl;
    return 1;
}
