#include "memBin.h"
#include "memBoxGrouping.h"
#include "memGroup.h"
#include <chrono>
#include <iostream>

class timer
{
  public:
    timer(std::string msg = "") : _msg(msg)
    {
        start = std::chrono::steady_clock::now();
    }

    ~timer()
    {
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "[Grouping] " << _msg << ": " << ms << " ms" << std::endl;
    }

  protected:
    std::chrono::steady_clock::time_point start;
    std::string _msg;
};

int main()
{
    memBoxGrouping g;
    {
        timer t("single thread test");
        std::deque<std::deque<memBox>> groups;
        for (unsigned i = 0; i < 512; ++i)
        {
            memBox testBox(-20000 + i * 100, -20000 + i * 100, -10000 + i * 100, -10000 + i * 100);
            g.getTouchedGroups(testBox, groups);
        }
    }

    {
        timer t("multi thread test");
        g.multiThreadGrouping(16);
    }

    return 0;
}