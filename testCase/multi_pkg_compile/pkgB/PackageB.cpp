#include "PackageB.h"
#include "PackageA.h"
#include <cstdlib>
#include <sstream>

PackageB::PackageB()
{
    data_ = std::unique_ptr<int[]>(new int[512]);
}

std::string PackageB::combine(PackageA &a, const std::string &msg)
{
    std::string processed = a.process(msg);
    std::stringstream ss;
    ss << processed << ":B";
    void *tmp = malloc(64);
    free(tmp);
    return ss.str();
}
