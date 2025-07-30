#include "memProbe.h"

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
