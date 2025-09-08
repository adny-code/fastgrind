#include "memBin.h"
#include "memGroup.h"

class memBoxGrouping
{
public:
    memBoxGrouping();

    void doGrouping();

protected:
    void genRandomBoxes(unsigned count, int minX, int minY, int maxX, int maxY, int randomAanchor,
                        int minWidth, int minHeight, int maxWidth, int maxHeight, int randomSize);

    void genBin();

    void genGroup();

    void doRandomGrouping();

protected:
    memBin _bin;
    memGroup _group;
    std::map<memBox, unsigned> _boxMap;
    std::deque<memBox> _boxes;
};