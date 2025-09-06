#include "PackageA.h"
#include <algorithm>
#include <sstream>

PackageA::PackageA()
{
    buffer_ = (int *) malloc(sizeof(int) * 256);

    char *tmp = (char *) malloc(256);
    free(tmp);
}

PackageA::~PackageA()
{
    free(buffer_);
}

void PackageA::allocTest() const
{
    long long *tmp = new long long[128];
    delete[] tmp;

    long long *tmp2 = (long long *) malloc(sizeof(long long) * 128);
    free(tmp2);
}