#include "pkgA/PackageA.h"
#include "pkgB/PackageB.h"
#include "pkgC/PackageC.h"
#include <cstdlib>
#include <iostream>
#include <random>
#include <thread>
#include <vector>
#include <unistd.h>

void thread1()
{
    PackageA a;
    a.allocTest();
}

void thread2()
{
    PackageB b;
    b.allocTest();
}

void thread3()
{
    PackageC c;
    c.allocTest();
}

int main()
{
    std::thread t1(thread1);
    std::thread t2(thread2);
    std::thread t3(thread3);

    t1.join();
    t2.join();
    t3.join();

    return 0;
}
