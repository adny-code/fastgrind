#include "PackageA.h"
#include <algorithm>
#include <sstream>

PackageA::PackageA() {
    // allocate some memory
    buffer_.resize(1024); // vector alloc
    int *raw = (int*)malloc(sizeof(int)*256); // raw alloc
    free(raw);
}

PackageA::~PackageA() {
    // implicit vector free
}

std::string PackageA::process(const std::string &in) {
    std::string out = in;
    std::reverse(out.begin(), out.end());
    // more allocations
    char *tmp = (char*)malloc(128);
    snprintf(tmp,128,"A:%s", out.c_str());
    std::string ret(tmp);
    free(tmp);
    return ret;
}
