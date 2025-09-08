#include "memProbe.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <new>
#include <thread>
#include <unistd.h>
#include <vector>

void add()
{
    __MERECORDER__::MEM_PROBE;
    auto a = new int;
    delete a;
    return;
}

void add2()
{
    __MERECORDER__::MEM_PROBE;
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
    __MERECORDER__::MEM_PROBE;
    printf("===== testAllMalloc begin =====\n");

    // 1. malloc / free
    {
        void *p = std::malloc(128);
        if (p)
        {
            std::memset(p, 0xAB, 128);
            std::free(p);
        }
    }

    // 2. calloc
    {
        void *p = std::calloc(16, 8); // 128 bytes
        std::free(p);
    }

    // 3. realloc grow + shrink
    {
        char *p = static_cast<char *>(std::malloc(32));
        p = static_cast<char *>(std::realloc(p, 256));
        p = static_cast<char *>(std::realloc(p, 64));
        std::free(p);
    }

    // 4. new / delete (scalar)
    {
        int *pi = new int(42);
        delete pi;
    }

    // 5. new[] / delete[] (array)
    {
        int *arr = new int[50];
        arr[0] = 7;
        delete[] arr;
    }

    // 6. nothrow new / delete
    {
        int *pi = new (std::nothrow) int(5);
        delete pi;
    }

    // 7. nothrow new[] / delete[]
    {
        int *arr = new (std::nothrow) int[10];
        delete[] arr;
    }

    // 8. valloc
    {
        void *p = valloc(4096);
        if (p)
            free(p);
    }

    // 9. pvalloc
    {
        void *p = pvalloc(3000);
        if (p)
            free(p);
    }

    // 10. memalign
    {
        void *p = memalign(64, 256);
        if (p)
            free(p);
    }

    // 11. posix_memalign
    {
        void *p = nullptr;
        if (posix_memalign(&p, 128, 512) == 0)
        {
            free(p);
        }
    }

    // 12. reallocarray basic
    {
#if defined(__GLIBC__) && (__GLIBC__ * 100 + __GLIBC_MINOR__) >= 230
        void *p = reallocarray(nullptr, 32, 16); // 512 bytes
        if (!p)
            p = std::malloc(32 * 16);
        free(p);
#endif
    }

    // 13. C++17 aligned_alloc
    {
#if defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606
        void *p = aligned_alloc(64, 256); // size multiple of alignment
        if (p)
            free(p);
#endif
    }

    // 14. C++17 aligned new / delete
    {
#if defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606
        struct alignas(64) Al64
        {
            int x;
        };
        auto *obj = new Al64();
        delete obj;
        int *arr = new (std::align_val_t(32)) int[16];
        ::operator delete[](arr, std::align_val_t(32));
#endif
    }

    // 15. nothrow aligned new / delete
    {
#if defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606
        int *p = new (std::align_val_t(64), std::nothrow) int(9);
        ::operator delete(p, std::align_val_t(64));
        int *arr = new (std::align_val_t(64), std::nothrow) int[4];
        ::operator delete[](arr, std::align_val_t(64));
#endif
    }

    // 16. Sized delete path
    {
#if defined(__cpp_sized_deallocation)
        struct Foo
        {
            int a[32];
        };
        auto *f = new Foo();
        delete f;
        auto *fa = new Foo[3];
        delete[] fa;
#endif
    }

    // 17. Sized + aligned delete
    {
#if defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606 && defined(__cpp_sized_deallocation)
        struct alignas(64) BigAligned
        {
            char buf[2048];
            int x;
        };
        auto *p = new (std::align_val_t(64)) BigAligned();
        delete p;
        auto *pa = new (std::align_val_t(64)) BigAligned[4];
        delete[] pa;
#endif
    }

    // 18. Sized + aligned + nothrow delete
    {
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

    // 19. Throwing constructor (ensure deallocation path)
    {
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

    // 20. reallocarray grow/shrink
    {
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

    printf("===== testAllMalloc end =====\n");
}

int main()
{
    __MERECORDER__::MEM_PROBE;
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