#include "fastgrind.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <new>
#include <thread>
#include <unistd.h>
#include <vector>
#include <execinfo.h>

void print_stacktrace() {
    const int SIZE = 16;
    void *buffer[SIZE];
    int nptrs = backtrace(buffer, SIZE);
    char **symbols = backtrace_symbols(buffer, nptrs);

    printf("Stack trace:\n");
    for (int i = 0; i < nptrs; ++i) {
        printf("%s\n", symbols[i]);
    }
    free(symbols);
}

// Individual test case functions extracted from original testAllMalloc
void test_malloc_free()
{
    __FASTGRIND__::FAST_GRIND;
    void *p = std::malloc(128);
    if (p)
    {
        std::memset(p, 0xAB, 128);
        std::free(p);
    }
}

void test_calloc()
{
    __FASTGRIND__::FAST_GRIND;
    void *p = std::calloc(16, 8); // 128 bytes
    if (p)
        std::free(p);
}

void test_realloc_grow_shrink()
{
    __FASTGRIND__::FAST_GRIND;
    char *p = static_cast<char *>(std::malloc(32));
    p = static_cast<char *>(std::realloc(p, 256));
    p = static_cast<char *>(std::realloc(p, 64));
    std::free(p);
}

void test_new_delete_scalar()
{
    __FASTGRIND__::FAST_GRIND;
    int *pi = new int(42);
    if (pi)
        delete pi;
}

void test_new_delete_array()
{
    __FASTGRIND__::FAST_GRIND;
    int *arr = new int[50];
    arr[0] = 7;
    delete[] arr;
}

void test_nothrow_new_delete()
{
    __FASTGRIND__::FAST_GRIND;
    int *pi = new (std::nothrow) int(5);
    if (pi)
        delete pi;
}

void test_nothrow_new_delete_array()
{
    __FASTGRIND__::FAST_GRIND;
    int *arr = new (std::nothrow) int[10];
    arr[0] = 7;
    delete[] arr;
}

void test_valloc()
{
    __FASTGRIND__::FAST_GRIND;
    void *p = valloc(4096);
    if (p)
        free(p);
}

void test_pvalloc()
{
    __FASTGRIND__::FAST_GRIND;
    void *p = pvalloc(3000);
    if (p)
        free(p);
}

void test_memalign()
{
    __FASTGRIND__::FAST_GRIND;
    void *p = memalign(64, 256);
    if (p)
        free(p);
}

void test_posix_memalign()
{
    __FASTGRIND__::FAST_GRIND;
    void *p = nullptr;
    if (posix_memalign(&p, 128, 512) == 0)
    {
        free(p);
    }
}

void test_reallocarray_basic()
{
    __FASTGRIND__::FAST_GRIND;
#if defined(__GLIBC__) && (__GLIBC__ * 100 + __GLIBC_MINOR__) >= 230
    void *p = reallocarray(nullptr, 32, 16); // 512 bytes
    if (!p)
        p = std::malloc(32 * 16);
    free(p);
#endif
}

void test_aligned_alloc_cxx17()
{
    __FASTGRIND__::FAST_GRIND;
#if defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606
    void *p = aligned_alloc(64, 256); // size multiple of alignment
    if (p)
        free(p);
#endif
}

void test_aligned_new_delete()
{
    __FASTGRIND__::FAST_GRIND;
#if defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606
    struct alignas(64) Al64
    {
        int x;
    };
    auto *obj = new Al64();
    if (obj)
        delete obj;
    int *arr = new (std::align_val_t(32)) int[16];
    arr[0] = 7;
    ::operator delete[](arr, std::align_val_t(32));
#endif
}

void test_nothrow_aligned_new_delete()
{
    __FASTGRIND__::FAST_GRIND;
#if defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606
    int *p = new (std::align_val_t(64), std::nothrow) int(9);
    p[0] = 7;
    ::operator delete(p, std::align_val_t(64));
    int *arr = new (std::align_val_t(64), std::nothrow) int[4];
    arr[0] = 7;
    ::operator delete[](arr, std::align_val_t(64));
#endif
}

void test_sized_delete_path()
{
    __FASTGRIND__::FAST_GRIND;
#if defined(__cpp_sized_deallocation)
    struct Foo
    {
        int a[32];
    };
    auto *f = new Foo();
    if (f)
        delete f;
    auto *fa = new Foo[3];
    if (fa)
        delete[] fa;
#endif
}

void test_sized_aligned_delete()
{
    __FASTGRIND__::FAST_GRIND;
#if defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606 && defined(__cpp_sized_deallocation)
    struct alignas(64) BigAligned
    {
        char buf[2048];
        int x;
    };
    auto *p = new (std::align_val_t(64)) BigAligned();
    if (p)
        delete p;
    auto *pa = new (std::align_val_t(64)) BigAligned[4];
    if (pa)
        delete[] pa;
#endif
}

void test_sized_aligned_nothrow_delete()
{
    __FASTGRIND__::FAST_GRIND;
#if defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606 && defined(__cpp_sized_deallocation)
    struct alignas(128) BigAlignedNT
    {
        char buf[1024];
        long y;
    };
    auto *p2 = new (std::align_val_t(128), std::nothrow) BigAlignedNT();
    if (p2)
        delete p2;
    auto *p2a = new (std::align_val_t(128), std::nothrow) BigAlignedNT[3];
    if (p2a)
        delete[] p2a;
#endif
}

void test_throwing_constructor_deallocation()
{
    __FASTGRIND__::FAST_GRIND;
#if defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606 && defined(__cpp_sized_deallocation)
    struct alignas(256) ThrowAligned
    {
        char pad[4096];
        ThrowAligned()
        {
            throw 11;
        }
    };
    try
    {
        auto *tp = new (std::align_val_t(256), std::nothrow) ThrowAligned();
        (void) tp;
    }
    catch (...)
    {
    }
    try
    {
        auto *tpa = new (std::align_val_t(256), std::nothrow) ThrowAligned[2];
        (void) tpa;
    }
    catch (...)
    {
    }
#endif
}

void test_reallocarray_grow_shrink()
{
    __FASTGRIND__::FAST_GRIND;
#if defined(__GLIBC__) && (__GLIBC__ * 100 + __GLIBC_MINOR__) >= 230
    void *q = reallocarray(nullptr, 4, 128);
    if (q)
    {
        void *q2 = reallocarray(q, 8, 128); // grow
        if (!q2)
        {
            free(q);
        }
        else
        {
            void *q3 = reallocarray(q2, 2, 128); // shrink
            if (!q3)
                free(q2);
            else
                free(q3);
        }
    }
#endif
}

void add()
{
    __FASTGRIND__::FAST_GRIND;
    auto a = new int;
    delete a;
    return;
}

void add2()
{
    __FASTGRIND__::FAST_GRIND;
    std::vector<int *> pp;
    for (unsigned i = 0; i < 10000; ++i)
    {
        pp.push_back(new int);
    }

    for (auto &p : pp)
    {
        delete p;
    }
}

void testAllMalloc()
{
    __FASTGRIND__::FAST_GRIND;

    test_malloc_free();
    test_calloc();
    test_realloc_grow_shrink();
    test_new_delete_scalar();
    test_new_delete_array();
    test_nothrow_new_delete();
    test_nothrow_new_delete_array();
    test_valloc();
    test_pvalloc();
    test_memalign();
    test_posix_memalign();
    test_reallocarray_basic();
    test_aligned_alloc_cxx17();
    test_aligned_new_delete();
    test_nothrow_aligned_new_delete();
    test_sized_delete_path();
    test_sized_aligned_delete();
    test_sized_aligned_nothrow_delete();
    test_throwing_constructor_deallocation();
    test_reallocarray_grow_shrink();
}

int main()
{
    __FASTGRIND__::FAST_GRIND;
    usleep(2500 * 1000);
    printf("default malloc \n");

    testAllMalloc();

    add();
    usleep(2500 * 1000);

#if 1
    std::vector<std::thread *> threads;
    for (unsigned i = 0; i < 1; ++i)
    {
        threads.push_back(new std::thread(add2));
    }

    for (auto &t : threads)
    {
        t->join();
        delete t;
    }
#endif

    std::vector<unsigned> vv;
    for (unsigned i = 0; i < 100; ++i)
    {
        vv.push_back(i);
    }

    add();
    return 0;
}