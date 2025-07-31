#include "test.h"


extern "C" {

// void* __wrap_malloc(size_t sz) {
//     printf("malloc: %s \n", __FUNCTION__);
//     auto p = malloc(sz);
//     return p;
// }


// void __wrap_free(void* p) {
//     printf("free: %s \n", __FUNCTION__);
//     free(p);
// }

}

void test_func_print2()
{
    printf("test_func_print2: %s \n", __FUNCTION__);
    int* p = (int*)malloc(100);
    printf("malloc p: %p \n", p);
}
