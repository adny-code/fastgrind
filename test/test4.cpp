#include "test4.h"

void testFunc()
{
    MEM_PROBE;
    int *p1 = (int *)malloc(100);
    int *p2 = new int[100];

    delete[] p2;
    free(p1);
}