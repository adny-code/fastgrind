#pragma once
#include <string>
#include <memory>
#include <vector>
#include "memProbe.h"

class PackageA; // forward

class PackageB {
public:
    PackageB();
    std::string combine(PackageA &a, const std::string &msg);
private:
    std::unique_ptr<int[]> data_;
};
