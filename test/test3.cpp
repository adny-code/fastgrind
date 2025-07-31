#include "../include/memProbe1.h"
#include "test4.h"

int main()
{
    MEM_PROBE;
    int *p1 = (int *)malloc(1000);
    int *p2 = new int[1000];

    free(p1);

    testFunc();
    return 0;
}