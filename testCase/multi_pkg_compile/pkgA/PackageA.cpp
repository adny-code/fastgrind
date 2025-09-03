#include "PackageA.h"
#include <algorithm>
#include <sstream>

PackageA::PackageA()
{
    buffer_.resize(1024);
    int *raw = (int *) malloc(sizeof(int) * 256);
    free(raw);
}

PackageA::~PackageA()
{
}

std::string PackageA::process(const std::string &in)
{
    std::string out = in;
    std::reverse(out.begin(), out.end());

    char *tmp = (char *) malloc(128);
    snprintf(tmp, 128, "A:%s", out.c_str());
    std::string ret(tmp);
    free(tmp);
    return ret;
}
