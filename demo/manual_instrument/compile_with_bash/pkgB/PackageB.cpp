#include "memProbe.h"
#include "PackageB.h"
#include "PackageA.h"
#include <cstdlib>
#include <sstream>

PackageB::PackageB()
{
    __MERECORDER__::MEM_PROBE;
    a_ = new PackageA();
}

PackageB::~PackageB()
{
    __MERECORDER__::MEM_PROBE;
    delete a_;
}

void PackageB::allocTest() const
{
    __MERECORDER__::MEM_PROBE;
    double *tmp = new double[128];
    delete[] tmp;

    double *tmp2 = (double *) malloc(sizeof(double) * 128);
    free(tmp2);
}