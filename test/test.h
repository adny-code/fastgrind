#pragma once

#include <iostream>
#include <memory>
#include <stdio.h>

static void test_func_print()
{
    printf("test_func_print: %s \n", __FUNCTION__);
}

void test_func_print2();

// #include <jemalloc/jemalloc.h>
// extern "C" {
// extern void* je_malloc_default(size_t);
// extern void* je_free_default(void*);

// inline void* __wrap_malloc(size_t sz) {
//     printf("malloc: %s \n", __FUNCTION__);
//     void* p =  je_malloc_default(sz);
//     return p;
// }

// inline void __wrap_free(void* p) {
//     printf("free: %s \n", __FUNCTION__);
//     je_free_default(p);
// }
// // override operator new
// inline void* __wrap__Znwm(size_t sz) {
//     auto p = je_malloc_default(sz);
//     return p;
// }

// }

// static void* memFuncTable[] = {(void*)&__wrap_malloc, (void*)&__wrap_free, (void*)&__wrap__Znwm, nullptr};

struct test_thread_local
{
    int a = 0;
    test_thread_local()
    {
        printf("constructor: %s \n", __FUNCTION__);
    }

    ~test_thread_local()
    {
        printf("destructor: %s \n", __FUNCTION__);
    }

    static test_thread_local &instance()
    {
        thread_local test_thread_local g_test_thread_local;
        return g_test_thread_local;
    }
};

class test1
{
  public:
    test1()
    {
        printf("constructor: %s \n", __FUNCTION__);
    }

    ~test1()
    {
        printf("destructor: %s \n", __FUNCTION__);
    }

    static std::unique_ptr<test1> &getInstance()
    {
        if (!_ptr)
        {
            _ptr = std::make_unique<test1>();
        }
        return _ptr;
    }

  private:
    static std::unique_ptr<test1> _ptr;
};

inline std::unique_ptr<test1> test1::_ptr = nullptr;

// void testFunc() {
//     static std::unique_ptr<test1>& instance1 = test1::getInstance();
// }