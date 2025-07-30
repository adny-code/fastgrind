#include "memProbe.h"

#include <assert.h>

auto& __ = memTimer::instance();

#ifdef JE_MALLOC

#include <jemalloc/jemalloc.h>

extern "C" {
extern void* je_malloc_default(size_t);
extern void* je_free_default(void*);

void* __wrap_malloc(size_t sz) {
    auto p = je_malloc_default(sz);
    memLocalInfo::instance().add(malloc_usable_size(p));
    return p;
}

void __wrap_free(void* p) {
    if (p) {
        memLocalInfo::instance().sub(malloc_usable_size(p));
    }

    je_free_default(p);
}

void* __wrap__Znwm(size_t sz) {
    auto p = je_malloc_default(sz);
    memLocalInfo::instance().add(malloc_usable_size(p));
    return p;
}

void __wrap__ZdlPv(void* p) {
    if (p) {
        memLocalInfo::instance().sub(malloc_usable_size(p));
    }

    je_free_default(p);
}

void __wrap__ZdaPv(void* p) {
    if (p) {
        memLocalInfo::instance().sub(malloc_usable_size(p));
    }

    je_free_default(p);
}

void __wrap__ZdaPvm(void* p, size_t sz) {
    if (p) {
        memLocalInfo::instance().sub(malloc_usable_size(p));
    }

    je_free_default(p);
}

void __wrap__ZdlPvm(void* p, size_t sz) {
    if (p) {
        memLocalInfo::instance().sub(malloc_usable_size(p));
    }

    je_free_default(p);
}
};

#else

extern "C" {

extern void* __libc_malloc(size_t);
extern void __libc_free(void*);

void* malloc(size_t sz) {
    auto p = __libc_malloc(sz);
    memLocalInfo::instance().add(malloc_usable_size(p));
    return p;
}

void free(void* p) {
    if (p) {
        memLocalInfo::instance().sub(malloc_usable_size(p));
    }

    __libc_free(p);
}
};

#endif

memGlobalInfo::~memGlobalInfo() {
    memTimer::instance().stop();
    memLocalInfo::instance().merge();
    dump();
}

void memGlobalInfo::dump() const {
    printf("Func Memory Info\n");
    unsigned count = 0;
    for (auto& [tid, threadsInfo] : _frames) {
        for (auto& [frameId, tickInfo] : threadsInfo) {
            memFrame frame0(0, 0, tickInfo.begin()->second.funcId,
                            tickInfo.begin()->second.frameId);
            for (auto& [tick, frame] : tickInfo) {
                frame0 += frame;
            }

            printf("  %u: threadId:%lu %s: alloc %lu free %lu\n", count++, tid,
                   getCallstack(frameId).c_str(), frame0.mallocBytes,
                   frame0.freeBytes);
        }
    }

    printf("\n");
    printf("Callstack Info\n");
    count = 0;
    for (auto& [frameId, callstack] : _callstacks) {
        printf("  %u: frame:%lu %s\n", count++, frameId,
               getCallstack(frameId).c_str());
    }

    fflush(stdout);
}
