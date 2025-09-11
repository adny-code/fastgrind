#pragma once

#include "memBin.h"
#include "memGroup.h"
#include <mutex>

class memBoxGrouping
{
  public:
    memBoxGrouping();

    void getTouchedGroups(const memBox& q, std::deque<std::deque<memBox>>& groups);

    void multiThreadGrouping(unsigned threadCnt, unsigned testBoxCnt = 1024);

  protected:
    void genRandomBoxes(unsigned count, int minX, int minY, int maxX, int maxY);

    void genBin();

    void genGroup();

    void genTestBoxes(unsigned cnt, std::deque<memBox>& testBoxes) const;

    void threadJobs(const std::deque<memBox>& jobs, std::map<memBox, std::deque<std::deque<memBox>>>& results);

  protected:
    memBox _border;
    memBin _bin;
    memGroup _group;
    std::map<memBox, unsigned> _boxMap;
    std::deque<memBox> _boxes;

    std::mutex _mx;
};