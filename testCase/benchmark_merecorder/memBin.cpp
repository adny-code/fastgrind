#include "memBin.h"
#include <cmath>
#include <cstdio>

memBin::memBin(memBox binBorder, unsigned grindSise, unsigned dataSize)
    : _binBorder(binBorder), _gridSize(grindSise), _dataSize(dataSize)
{
    init(binBorder, grindSise, dataSize);
}

void memBin::init(memBox binBorder, unsigned grindSise, unsigned dataSize)
{
    _binBorder = binBorder;
    unsigned grindCnt = dataSize / grindSise;
    double rateXY = double(binBorder.width()) / double(binBorder.height());

    _gridXCnt = static_cast<unsigned>(sqrt(grindCnt * rateXY));
    _gridYCnt = grindCnt / _gridXCnt;

    _gridX = static_cast<unsigned>(ceil(static_cast<double>(binBorder.width()) / _gridXCnt));
    _gridY = static_cast<unsigned>(ceil(static_cast<double>(binBorder.height()) / _gridYCnt));

    _datas.resize(_gridXCnt);
    for (auto &item : _datas)
        item.resize(_gridYCnt);
}

bool memBin::getGridIndex(const memBox &box, unsigned &x1, unsigned &y1, unsigned &x2, unsigned &y2) const
{
    if (!box.valid() || !box.overlap(_binBorder, false))
        return false;

    int left = std::max(box.leftBottom()._x, _binBorder.leftBottom()._x);
    int bottom = std::max(box.leftBottom()._y, _binBorder.leftBottom()._y);
    int right = std::min(box.rightTop()._x, _binBorder.rightTop()._x);
    int top = std::min(box.rightTop()._y, _binBorder.rightTop()._y);

    x1 = (left - _binBorder.leftBottom()._x) / _gridX;
    y1 = (bottom - _binBorder.leftBottom()._y) / _gridY;
    x2 = (right - _binBorder.leftBottom()._x) / _gridX;
    y2 = (top - _binBorder.leftBottom()._y) / _gridY;

    if (x2 >= _gridXCnt)
        x2 = _gridXCnt - 1;
    if (y2 >= _gridYCnt)
        y2 = _gridYCnt - 1;

    return true;
}

void memBin::add(const memBox &box)
{
    unsigned x1, y1, x2, y2;
    if (!getGridIndex(box, x1, y1, x2, y2))
        return;

    for (unsigned ix = x1; ix <= x2; ++ix)
    {
        for (unsigned iy = y1; iy <= y2; ++iy)
        {
            _datas[ix][iy].emplace_back(box);
        }
    }
}

std::set<memBox> memBin::query(const memBox &box, bool proper) const
{
    std::set<memBox> results;
    unsigned x1, y1, x2, y2;
    if (!getGridIndex(box, x1, y1, x2, y2))
        return results;

    for (unsigned ix = x1; ix <= x2; ++ix)
    {
        for (unsigned iy = y1; iy <= y2; ++iy)
        {
            for (const auto &item : _datas[ix][iy])
            {
                if (item.overlap(box, proper))
                    results.emplace(item);
            }
        }
    }
    return results;
}

void memBin::dump() const
{
    printf("[Grouping] border: <%d, %d, %d, %d>, gridXCnt: %u, gridYCnt: %u, gridX: %u, gridY: %u \n",
           _binBorder.leftBottom().x(),
           _binBorder.leftBottom().y(),
           _binBorder.rightTop().x(),
           _binBorder.rightTop().y(),
           _gridXCnt,
           _gridYCnt,
           _gridX,
           _gridY);
}