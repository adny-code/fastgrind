#include "test.h"

int main() {
    test_thread_local::instance().a = 10;
    testFunc();
    return 0;
}