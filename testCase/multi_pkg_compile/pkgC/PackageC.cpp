#include "PackageC.h"
#include "pkgB/PackageB.h"
#include "pkgA/PackageA.h"
#include <sstream>
#include <thread>
#include <vector>
#include <cstdlib>

std::string PackageC::finalize(PackageB &b, const std::string &msg) {
    PackageA a; // create local A for processing
    std::string combined = b.combine(a, msg);
    // allocate memory in a loop
    for(int i=0;i<10;++i){
        char *buf = (char*)malloc(32);
        snprintf(buf,32,"%d", i);
        cache_[i] = buf;
        free(buf);
    }
    return combined + ":C";
}
