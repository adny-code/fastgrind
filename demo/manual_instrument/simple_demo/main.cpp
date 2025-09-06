#include "memProbe.h"

#include <vector>
#include <thread>
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <random>

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
    __MERECORDER__::MEM_PROBE;
}

mallocTest::~mallocTest()
{
    __MERECORDER__::MEM_PROBE;
    delete a;
    delete b;
    delete c;
}

void mallocTest::alloc()
{
    __MERECORDER__::MEM_PROBE;
    a = new char;
    b = new int;
    c = new double;
    vec.resize(400);
}

void threadFunc1()
{
    __MERECORDER__::MEM_PROBE;
    int *a = new int[100];
    sleep(1);
    delete[] a;
}

void threadFunc2()
{
    __MERECORDER__::MEM_PROBE;
    mallocTest t;
    t.alloc();
    sleep(1);
}


int main()
{
    __MERECORDER__::MEM_PROBE;

    std::thread t1(threadFunc1);
    std::thread t2(threadFunc2);

    t1.join();
    t2.join();
    return 0;
}