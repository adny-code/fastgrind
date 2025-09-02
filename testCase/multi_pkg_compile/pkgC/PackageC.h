#pragma once
#include <string>
#include <map>
#include "memProbe.h"

class PackageB;

class PackageC {
public:
    std::string finalize(PackageB &b, const std::string &msg);
private:
    std::map<int,std::string> cache_;
};
