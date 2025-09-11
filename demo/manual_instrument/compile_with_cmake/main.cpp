#include "fastGrind.h"
#include "pkgA/PackageA.h"
#include "pkgB/PackageB.h"
#include "pkgC/PackageC.h"
#include <cstdlib>
#include <iostream>
#include <random>
#include <thread>
#include <unistd.h>
#include <vector>

void thread1()
{
    __FASTGRIND__::FAST_GRIND;
    for (unsigned i = 0; i < 5; ++i)
    {
        PackageA a;
        a.allocTest();
        sleep(1);
    }
}

void thread2()
{
    __FASTGRIND__::FAST_GRIND;
    for (unsigned i = 0; i < 5; ++i)
    {
        PackageB b;
        b.allocTest();
        sleep(1);
    }
}

void thread3()
{
    __FASTGRIND__::FAST_GRIND;
    for (unsigned i = 0; i < 5; ++i)
    {
        PackageC c;
        c.allocTest();
        sleep(1);
    }
}

int main()
{
    __FASTGRIND__::FAST_GRIND;
    std::thread t1(thread1);
    std::thread t2(thread2);
    std::thread t3(thread3);

    t1.join();
    t2.join();
    t3.join();

    return 0;
}
