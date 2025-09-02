#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdlib>
#include "memProbe.h"

class PackageA {
public:
    PackageA();
    ~PackageA();
    std::string process(const std::string &in);
private:
    std::vector<int> buffer_;
};
