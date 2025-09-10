#include "memBin.h"
#include "memGroup.h"
#include "memBoxGrouping.h"

int main()
{
    memBoxGrouping g;
    g.multiThreadGrouping(16);

    return 0;
}