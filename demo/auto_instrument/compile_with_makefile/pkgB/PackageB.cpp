#include "PackageB.h"
#include "PackageA.h"
#include <cstdlib>
#include <sstream>

PackageB::PackageB()
{
    a_ = new PackageA();
}

PackageB::~PackageB()
{
    delete a_;
}

void PackageB::allocTest() const
{
    double *tmp = new double[128];
    delete[] tmp;

    double *tmp2 = (double *) malloc(sizeof(double) * 128);
    free(tmp2);
}