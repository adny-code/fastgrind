#include "memBoxGrouping.h"
#include <cstdlib>
#include <ctime>
#include <cmath>

void memBoxGrouping::genRandomBoxes(unsigned count, int minX, int minY, int maxX, int maxY, int randomAanchor,
                        int minWidth, int minHeight, int maxWidth, int maxHeight, int randomSize)
{
    _boxes.clear();
    _boxMap.clear();

    srand((unsigned)time(nullptr));
    int curX = minX;
    int curY = minY;

    double rateXY = double(maxX -minX) / double(maxY - minY);
    unsigned countX = static_cast<unsigned>(sqrt(count * rateXY));
    unsigned countY = count / countX;

    for (unsigned i = 0; i < countX; ++i) {
        for (unsigned j = 0; j < countY; ++j) {
            
        }
    }
}