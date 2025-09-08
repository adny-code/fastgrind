#include "memBox.h"
#include <deque>

class memBin
{
    typedef std::deque<memBox> memBoxList;

  public:
    memBin(memBox binBorder, unsigned grindSise, unsigned dataSize);

    void add(const memBox &box);

    std::deque<memBox> query(const memBox &box, bool proper) const;

  protected:
    bool getGridIndex(const memBox &box, unsigned &x1, unsigned &y1, unsigned &x2, unsigned &y2) const;

  protected:
    memBox _binBorder;

    unsigned _gridXCnt;
    unsigned _gridYCnt;
    unsigned _gridX;
    unsigned _gridY;

    unsigned _gridSize;
    unsigned _dataSize;
    std::deque<std::deque<memBoxList>> _datas;
};