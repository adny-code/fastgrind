#include "PackageC.h"
#include "pkgA/PackageA.h"
#include "pkgB/PackageB.h"
#include <cstdlib>
#include <sstream>
#include <thread>
#include <vector>

std::string PackageC::finalize(PackageB &b, const std::string &msg)
{
    PackageA a;
    std::string combined = b.combine(a, msg);

    for (int i = 0; i < 10; ++i)
    {
        char *buf = (char *) malloc(32);
        snprintf(buf, 32, "%d", i);
        cache_[i] = buf;
        free(buf);
    }
    return combined + ":C";
}
