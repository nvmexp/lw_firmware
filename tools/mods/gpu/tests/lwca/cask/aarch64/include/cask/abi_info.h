#ifndef INCLUDE_GUARD_CASK_ABI_INFO_H 
#define INCLUDE_GUARD_CASK_ABI_INFO_H 
#include <string>
#include <vector>
// this strut describe the custom compiler Abi information that
// device function needed 
namespace cask {
struct AbiInfo{
    short start;
    short length;
    short ret_addr;
    std::string name;
    std::vector<int> scratchRegs;
	// default ctor for container initialize
	AbiInfo(){
	    start = 0;
	    length = 0;
	    ret_addr = 0;
	    name = "";
        scratchRegs = std::vector<int>(0);
	}
    AbiInfo(short start_, short length_, short ret_addr_, std::string name_,
            std::vector<int> scratchRegs_){
        start = start_;
        length = length_;
        ret_addr = ret_addr_;
        name = name_;
        scratchRegs = scratchRegs_;
    }
};
}
#endif
