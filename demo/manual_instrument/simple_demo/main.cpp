#include "fastgrind.h"

#include <cstdlib>
#include <iostream>
#include <random>
#include <thread>
#include <unistd.h>
#include <vector>

struct mallocTest
{
    char *a = nullptr;
    int *b = nullptr;
    double *c = nullptr;
    std::vector<int> vec;

    mallocTest();
    ~mallocTest();

    void alloc();
};

mallocTest::mallocTest()
{
    __FASTGRIND__::FAST_GRIND;
}

mallocTest::~mallocTest()
{
    __FASTGRIND__::FAST_GRIND;
    delete a;
    delete b;
    delete c;
}

void mallocTest::alloc()
{
    __FASTGRIND__::FAST_GRIND;
    a = new char;
    b = new int;
    c = new double;
    vec.resize(400);
}

void threadFunc1()
{
    __FASTGRIND__::FAST_GRIND;
    for (unsigned i = 0; i < 5; ++i)
    {
        int *a = new int[100];
        sleep(1);
        delete[] a;
    }
}

void threadFunc2()
{
    __FASTGRIND__::FAST_GRIND;
    mallocTest t;
    for (unsigned i = 0; i < 5; ++i)
    {
        t.alloc();
        sleep(1);
    }
}

int main()
{
    __FASTGRIND__::FAST_GRIND;

    std::thread t1(threadFunc1);
    std::thread t2(threadFunc2);

    t1.join();
    t2.join();
    return 0;
}