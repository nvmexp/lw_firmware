// Floorsweeping library: dump possible FS configs
// Copyright LWPU Corp. 2019
#ifndef FS_DUMP_H
#define FS_DUMP_H

#include <cassert>
#include <iostream>
#include <fstream>
#include <string>
#include <iterator>
#include <algorithm>
#include "fs_lib.h"
#include <cstdlib>
#include <vector>
//Parser to read through command line options
class InputParser{
    public:
        InputParser (int &argc, char **argv){
            for (int i=1; i < argc; ++i)
                this->tokens.push_back(std::string(argv[i]));
        }
        const std::string& getCmdOption(const std::string &option) const{
            std::vector<std::string>::const_iterator itr;
            itr =  std::find(this->tokens.begin(), this->tokens.end(), option);
            if (itr != this->tokens.end() && ++itr != this->tokens.end()){
                return *itr;
            }
            static const std::string empty_string("");
            return empty_string;
        }
        bool cmdOptionExists(const std::string &option) const{
            return std::find(this->tokens.begin(), this->tokens.end(), option)
                   != this->tokens.end();
        }
        void printErrorMsg(char **argv) {
            std::cout <<"Usage: " << argv[0] << " -chip <ga100/ga102> -mode <PRODUCT/FUNC_VALID/IGNORE_FSLIB> -o <yourfilename.csv> " << std::endl;            
        }
    private:
        std::vector<std::string> tokens;
};

#endif
