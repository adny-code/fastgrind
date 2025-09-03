#include "memProbe.h"
#include "pkgA/PackageA.h"
#include "pkgB/PackageB.h"
#include "pkgC/PackageC.h"
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

void worker(int id)
{
    PackageA a;
    PackageB b;
    PackageC c;
    for (int i = 0; i < 5; ++i)
    {
        std::string msg = "thread" + std::to_string(id) + "_iter" + std::to_string(i);
        std::string result = c.finalize(b, msg);
        char *dyn = (char *) malloc(100 + id);
        free(dyn);
        std::cout << "Result(" << id << "): " << result << std::endl;
    }
}

int main()
{
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
    {
        threads.emplace_back(worker, t);
    }
    for (auto &th : threads)
        th.join();
    return 0;
}
