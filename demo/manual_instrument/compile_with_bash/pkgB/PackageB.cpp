#include "fastgrind.h"
#include "PackageB.h"
#include "PackageA.h"
#include <cstdlib>
#include <sstream>

PackageB::PackageB()
{
    __FASTGRIND__::FAST_GRIND;
    a_ = new PackageA();
}

PackageB::~PackageB()
{
    __FASTGRIND__::FAST_GRIND;
    delete a_;
}

void PackageB::allocTest() const
{
    __FASTGRIND__::FAST_GRIND;
    double *tmp = new double[128];
    delete[] tmp;

    double *tmp2 = (double *) malloc(sizeof(double) * 128);
    free(tmp2);
}