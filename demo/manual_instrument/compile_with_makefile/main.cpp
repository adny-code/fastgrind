#include "memProbe.h"
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
    __MERECORDER__::MEM_PROBE;
    for (unsigned i = 0; i < 5; ++i)
    {
        PackageA a;
        a.allocTest();
    }
}

void thread2()
{
    __MERECORDER__::MEM_PROBE;
    for (unsigned i = 0; i < 5; ++i)
    {
        PackageB b;
        b.allocTest();
    }
}

void thread3()
{
    __MERECORDER__::MEM_PROBE;
    for (unsigned i = 0; i < 5; ++i)
    {
        PackageC c;
        c.allocTest();
    }
}

int main()
{
    __MERECORDER__::MEM_PROBE;
    std::thread t1(thread1);
    std::thread t2(thread2);
    std::thread t3(thread3);

    t1.join();
    t2.join();
    t3.join();

    return 0;
}
