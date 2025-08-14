#include "memProbe.h"

#include <gperftools/malloc_extension.h>
#include <vector>

void add()
{
    memProbe::MEM_PROBE;
    auto a = new int;
    delete a;
    return;
}

void add2()
{
    memProbe::MEM_PROBE;
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

bool sysCheckTcmalloc()
{
    size_t value = 0;
    return MallocExtension::instance()->GetNumericProperty(
               "tcmalloc.pageheap_free_bytes", &value);
}

int main()
{
    memProbe::MEM_PROBE;
    usleep(500 * 1000);

    printf("tid %lu tcmalloc status %u\n", syscall(SYS_gettid), sysCheckTcmalloc());

    add();
    usleep(500 * 1000);

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