#include "fastgrind.h"

#include <iostream>
#include <string>
#include <thread>
#include <vector>

void allocate_payload(std::size_t count)
{
    __FASTGRIND__::FAST_GRIND;

    std::vector<std::string> payload;
    payload.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        payload.emplace_back(128, 'x');
    }
}

void worker_task()
{
    __FASTGRIND__::FAST_GRIND;
    allocate_payload(256);
}

int main()
{
    __FASTGRIND__::FAST_GRIND;

    std::thread worker(worker_task);
    allocate_payload(128);
    worker.join();

    std::cout << "fastgrind demo finished" << std::endl;
    return 0;
}