#include "memBoxGrouping.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <random>
#include <thread>
#include <vector>

#define M1N (1 << 20)
memBoxGrouping::memBoxGrouping()
{
    genRandomBoxes(M1N, -M1N * 5, -M1N * 5, M1N * 5, M1N * 5);
    genBin();
    genGroup();
}

void memBoxGrouping::getTouchedGroups(const memBox &q, std::deque<std::deque<memBox>> &groups) const
{
    std::deque<memBox> touchList = _bin.query(q, true);
    std::set<unsigned> headers;
    for (auto &it : touchList)
    {
        unsigned header = _group.getHeader(_boxMap.at(it));
        headers.emplace(header);
    }

    for (auto it : headers)
    {
        std::deque<memBox> oneGroup;
        std::deque<unsigned> group;
        _group.getGroups(it, group);
        for (auto ii : group)
            oneGroup.emplace_back(_boxes[ii]);
        groups.emplace_back(std::move(oneGroup));
    }
}

void memBoxGrouping::multiThreadGrouping(unsigned threadCnt) const
{
    std::deque<memBox> testBoxes;
    genTestBoxes(1024, testBoxes);

    std::map<memBox, std::deque<std::deque<memBox>>> results;

    threadCnt = std::max(1u, threadCnt);
    const unsigned total = 1024u;
    const unsigned step = std::max(1u, (total + threadCnt - 1) / threadCnt);

    std::vector<std::thread> threads;
    threads.reserve((total + step - 1) / step);

    for (unsigned i = 0; i < total; i += step)
    {
        std::deque<memBox> spliceBoxes(
            testBoxes.begin() + i,
            (i + step >= total) ? testBoxes.end() : testBoxes.begin() + i + step);

        threads.emplace_back(&memBoxGrouping::threadJobs, this, spliceBoxes, std::ref(results));
    }

    for (auto &t : threads)
        t.join();
}

void memBoxGrouping::genTestBoxes(unsigned cnt, std::deque<memBox> &testBoxes) const
{
    auto getRandom = [](int l, int r) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(l, r);
        return dis(gen);
    };

    for (unsigned i = 0; i < cnt; ++i)
    {
        int ax = getRandom(-M1N * 4, M1N * 4);
        int ay = getRandom(-M1N * 4, M1N * 4);
        int aw = getRandom(M1N / 100, M1N / 10);
        int ah = getRandom(M1N / 100, M1N / 10);
        testBoxes.emplace_back(memBox(ax, ay, ax + aw, ay + ah));
    }
}

void memBoxGrouping::threadJobs(const std::deque<memBox>& jobs, std::map<memBox, std::deque<std::deque<memBox>>>& results)
{
    std::map<memBox, std::deque<std::deque<memBox>>> tmpResults;
    for (auto &it : jobs)
    {
        std::deque<std::deque<memBox>> groups;
        getTouchedGroups(it, groups);
        tmpResults[it] = std::move(groups);
    }

    std::unique_lock<std::mutex> lock(_mx);
    for (auto& it : tmpResults) {
        results[it.first] = std::move(it.second);
    }
}

void memBoxGrouping::genRandomBoxes(unsigned count, int minX, int minY, int maxX, int maxY)
{
    assert(maxX > minX && maxY > minY);

    int curX = minX;
    int curY = minY;

    int borderWidth = maxX - minX;
    int borderHeigth = maxY - minY;
    double rateXY = double(borderWidth) / double(borderHeigth);
    unsigned countX = static_cast<unsigned>(sqrt(count * rateXY));
    unsigned countY = count / countX;

    unsigned dx = borderWidth / countX;
    unsigned dy = borderHeigth / countY;

    auto getRandom = [](int l, int r) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(l, r);
        return dis(gen);
    };

    for (unsigned i = 0; i < countX; ++i)
    {
        for (unsigned j = 0; j < countY; ++j)
        {
            curX += i * dx;
            curY += j * dy;

            int w = getRandom(dx * 0.5, dx * 1.5);
            int h = getRandom(dy * 0.5, dy * 1.5);

            memBox b = memBox(curX, curY, curX + w, curY + h);
            _boxMap[b] = _boxes.size();
            _boxes.emplace_back(b);
        }
    }

    _border = memBox(minX - (dx * 2), minY - (dy * 2), maxX + (dx * 2), maxY + (dy * 2));
}

void memBoxGrouping::genBin()
{
    _bin.init(_border, 16, _boxes.size());
}

void memBoxGrouping::genGroup()
{
    for (auto &b : _boxes)
    {
        std::deque<memBox> touchList = _bin.query(b, false);
        _bin.add(b);

        std::set<unsigned> thisGroup;
        for (auto &tb : touchList)
            thisGroup.emplace(_boxMap[tb]);

        _group.add(thisGroup);
    }
}