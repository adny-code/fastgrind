#include "pkgA/PackageA.h"
#include "pkgB/PackageB.h"
#include "pkgC/PackageC.h"
#include <cstdlib>
#include <iostream>
#include <random>
#include <thread>
#include <vector>
#include <unistd.h>

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
    static thread_local std::mt19937 rng{std::random_device{}() ^
                                         (static_cast<std::mt19937::result_type>(reinterpret_cast<uintptr_t>(&rng)) +
                                          (static_cast<std::mt19937::result_type>(id) << 16))};
    std::uniform_int_distribution<int> dist(0, 2500);
    int random = dist(rng);

    usleep(random * 1000);
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
