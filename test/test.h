#pragma once

#include <memory>
#include <iostream>
#include <stdio.h>

struct test_thread_local
{
    int a = 0;
    test_thread_local() {
        printf("constructor: %s \n", __FUNCTION__);
    }

    ~test_thread_local() {
        printf("destructor: %s \n", __FUNCTION__);
    }

    static test_thread_local& instance() {
        thread_local test_thread_local g_test_thread_local;
        return g_test_thread_local;
    }
};

class test1
{
public:
    test1() {
        printf("constructor: %s \n", __FUNCTION__);
    }

    ~test1() {
        printf("destructor: %s \n", __FUNCTION__);
    }

    static std::unique_ptr<test1>&  getInstance() {
        if (!_ptr) {
            _ptr = std::make_unique<test1>();
        }
        return _ptr;
    }

private:
    static std::unique_ptr<test1>   _ptr;
};

inline std::unique_ptr<test1> test1::_ptr = nullptr;


void testFunc() {
    static std::unique_ptr<test1>& instance1 = test1::getInstance();
}