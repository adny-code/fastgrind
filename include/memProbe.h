#ifndef MEM_PROBE_H
#define MEM_PROBE_H

#include <array>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include <assert.h>
#include <locale.h>
#include <malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_STACK_DEPTH 64
#define SMAMPLE_INTERVAL_MS 100

#define MEM_PROBE_STATUS 1

template <typename... Args> static std::string memFormat(const char *fstr, Args... args)
{
    size_t size = 1 + snprintf(nullptr, 0, fstr, args...);
    char *bytes = new char[size];
    snprintf(bytes, size, fstr, args...);
    std::string out(bytes);
    delete[] bytes;
    return out;
}

/**
 * @brief Memory allocation and deallocation information.
 * @details: MemFrame holds one thread's and one tick's information.
 * @param mallocBytes: Total bytes allocated in this frame.
 * @param freeBytes: Total bytes freed in this frame.
 * @param funcId: Identifier for the function where this frame was created.
 * @param frameId: Identifier for the frame, used to track the call stack.
 */
struct memFrame
{
    /**
     * @brief Empty constructor for memFrame.
     *
     */
    memFrame() : mallocBytes(0), freeBytes(0), funcId(0), frameId(0)
    {
    }

    /**
     * @brief Construct a new mem Frame object
     */
    memFrame(size_t mallocBytes, size_t freeBytes, size_t funcId, size_t frameId)
        : mallocBytes(mallocBytes), freeBytes(freeBytes), funcId(funcId), frameId(frameId)
    {
    }

    memFrame &operator+=(const memFrame &other)
    {
        mallocBytes += other.mallocBytes;
        freeBytes += other.freeBytes;
        if (!funcId)
            funcId = other.funcId;
        if (!frameId)
            frameId = other.frameId;

        return *this;
    }

    size_t mallocBytes;
    size_t freeBytes;
    size_t funcId;
    size_t frameId;
};

class memTimer
{
  public:
    memTimer() : _tick(0)
    {
        _thread = new std::thread(std::bind(&memTimer::fakeTimer, this));
    }
    ~memTimer()
    {
        stop();
    };

    void stop()
    {
        _exit = true;
        if (_thread)
        {
            _thread->join();
            delete _thread;
            _thread = nullptr;
        }
    }

    size_t time() const
    {
        return _tick;
    }

  protected:
    void fakeTimer()
    {
        while (!_exit)
        {
            usleep(1000 * SMAMPLE_INTERVAL_MS);
            _tick += SMAMPLE_INTERVAL_MS;
        };
    }

  protected:
    bool _exit = false;
    std::thread *_thread = nullptr;
    std::atomic<size_t> _tick;
};

/*!@note data structure:
 *
 */
class memNode
{
  public:
    memNode(const char *name) : _name(name)
    {
    }

    void add(const std::array<const char *, MAX_STACK_DEPTH> &callstack, const memFrame &frame, unsigned depth = 0)
    {
        _mallocBytes += frame.mallocBytes;
        _freeBytes += frame.freeBytes;

        auto &curFunc = callstack[depth];
        if (curFunc)
        {

            if (_childs.find(curFunc) == _childs.end())
            {
                _childs.emplace(curFunc, memNode(curFunc));
            }

            _childs.at(curFunc).add(callstack, frame, depth + 1);
        }
    }

    std::map<const char *, memNode> &childs()
    {
        return _childs;
    }

    std::string str(unsigned indent = 0) const
    {
        std::string s =
            std::string(indent * 4, ' ') + memFormat("%s malloc %'ld free %'ld", _name, _mallocBytes, _freeBytes);

        for (auto &[name, child] : _childs)
        {
            s += std::string("\n") + child.str(indent + 1);
        }

        return s;
    }

    void dump() const
    {
        printf("%s\n", str().c_str());
    }

  protected:
    const char *_name;

    //!@note key is func name
    std::map<const char *, memNode> _childs;

    size_t _mallocBytes = 0;
    size_t _freeBytes = 0;
};

class memLocalInfo;
class memGlobalInfo
{
    friend class memLocalInfo;

  public:
    memGlobalInfo()
    {
        setlocale(LC_ALL, "");
    }

    ~memGlobalInfo()
    {
        dump();
    }

    static memGlobalInfo &instance()
    {
        if (!_instance)
        {
            _instance = std::make_unique<memGlobalInfo>();
        }

        return *_instance.get();
    }

    size_t time() const
    {
        return _timer.time();
    }

    void dump() const
    {
        printf("[Func Memory Info]\n");
        unsigned count = 0;
        for (auto &[tid, threadsInfo] : _frames)
        {
            for (auto &[frameId, tickInfo] : threadsInfo)
            {
                memFrame frame0(0, 0, tickInfo.begin()->second.funcId, tickInfo.begin()->second.frameId);
                for (auto &[tick, frame] : tickInfo)
                {
                    frame0 += frame;
                }

                printf("%u: threadId:%lu alloc %lu free %lu\n%s\n", count++, tid, frame0.mallocBytes, frame0.freeBytes,
                       getCallstack(frameId).c_str());
            }
        }

        printf("\n");
        printf("[Callstack Info]\n");
        count = 0;
        for (auto &[frameId, callstack] : _callstacks)
        {
            printf("%u: frame:%lu\n%s\n", count++, frameId, getCallstack(frameId).c_str());
        }

        printf("\n");
        printf("[Dump By Callstack]\n");
        memNode info("this");
        for (auto &[threadId, frames] : _frames)
        {
            for (auto &[frameId, tickFrames] : frames)
            {
                assert(_callstacks.find(frameId) != _callstacks.end());
                const auto &callstack = _callstacks.at(frameId);
                for (auto &[tick, frame] : tickFrames)
                {
                    info.add(callstack, frame);
                }
            }
        }

        info.dump();

        fflush(stdout);
    }

    std::string getCallstack(size_t frameId) const
    {
        if (_callstacks.find(frameId) == _callstacks.end())
            return "";

        std::string r;
        auto &callstack = _callstacks.at(frameId);
        unsigned depth = 0;
        for (depth = 0; callstack[depth] != nullptr; ++depth)
        {
        }

        for (unsigned i = 0; i < depth; ++i)
        {
            r += memFormat("%s%u: %s", (i ? "\n  " : "  "), i, callstack[depth - 1 - i]);
        }
        return r;
    }

  protected:
    memTimer _timer;

    // key is threadId, second map key is frameId, second key is tick second is
    // frame info
    std::map<size_t, std::unordered_map<size_t, std::unordered_map<size_t, memFrame>>> _frames;

    std::map<size_t, std::array<const char *, MAX_STACK_DEPTH>> _callstacks;
    std::mutex _lk;

  private:
    static std::unique_ptr<memGlobalInfo> _instance;
};

inline std::unique_ptr<memGlobalInfo> memGlobalInfo::_instance = nullptr;

class memStack
{
  public:
    void push(const char *v)
    {
        _stack[_offset++] = v;
        _stackId += size_t(v);
    }
    const char *pop()
    {
        auto v = _stack[--_offset];
        _stackId -= size_t(v);
        return v;
    }

    unsigned depth() const
    {
        return _offset;
    }
    const char *top() const
    {
        return _stack[_offset - 1];
    }

    size_t frameId() const
    {
        return size_t(top()) + _stackId;
    }
    const std::array<const char *, MAX_STACK_DEPTH> &stack() const
    {
        return _stack;
    }

    static memStack &instance()
    {
        thread_local memStack __memStack__;
        return __memStack__;
    }

  protected:
    std::array<const char *, MAX_STACK_DEPTH> _stack;
    size_t _offset = 0;
    size_t _stackId = 0;
};

static thread_local size_t tid = syscall(SYS_gettid);

class memLocalInfo : public std::unordered_map<size_t /*frameId*/, std::unordered_map<size_t /*tick*/, memFrame>>
{
  public:
    ~memLocalInfo()
    {
        merge();
    }

    static memLocalInfo &instance()
    {
        thread_local memLocalInfo __memThreadInfo__;
        return __memThreadInfo__;
    }

    void reset()
    {
        _frames.clear();
        _callstacks.clear();
    }

    void merge()
    {
        std::lock_guard<std::mutex> lg(memGlobalInfo::instance()._lk);
        memGlobalInfo::instance()._frames[tid].merge(_frames);

        for (auto &[k, v] : _callstacks)
            memGlobalInfo::instance()._callstacks[k] = v;

        reset();
    }

    void add(size_t sz)
    {
        if (_nested || memStack::instance().depth() == 0)
            return;

        ++_nested;
        const auto &stack = memStack::instance();
        getFrame(size_t(stack.top()), stack.frameId(), memGlobalInfo::instance().time()).mallocBytes += sz;
        --_nested;
    }

    void sub(size_t sz)
    {
        if (_nested || memStack::instance().depth() == 0)
            return;

        ++_nested;
        const auto &stack = memStack::instance();
        getFrame(size_t(stack.top()), stack.frameId(), memGlobalInfo::instance().time()).freeBytes += sz;
        --_nested;
    }

  protected:
    memFrame &getFrame(size_t funcId, size_t frameId, size_t time)
    {
        if (_callstacks.find(frameId) == _callstacks.end())
        {
            _callstacks[frameId] = memStack::instance().stack();
            _callstacks[frameId][memStack::instance().depth()] = nullptr;
        }

        auto &frames = _frames[frameId];
        auto it = frames.find(time);
        if (it == frames.end())
        {
            frames[time] = memFrame(0, 0, funcId, frameId);
            return frames[time];
        }
        else
        {
            return it->second;
        }
    }

  protected:
    // key is frameId, second map key is tick
    std::unordered_map<size_t, std::unordered_map<size_t, memFrame>> _frames;
    std::unordered_map<size_t, std::array<const char *, MAX_STACK_DEPTH>> _callstacks;
    int _nested = 0;
};

class memProbe
{
  public:
    memProbe(const char *name)
    {
        if (MEM_PROBE_STATUS)
            memStack::instance().push(name);
    }
    ~memProbe()
    {
        if (MEM_PROBE_STATUS)
            memStack::instance().pop();
    }
};

#define MEM_PROBE memProbe __probe__(__PRETTY_FUNCTION__);

#ifdef JE_MALLOC
#include <jemalloc/jemalloc.h>

extern "C"
{
    extern void *je_malloc_default(size_t);
    extern void *je_free_default(void *);
    extern void *__real_aligned_alloc(size_t alignment, size_t size);

    inline void *__wrap_malloc(size_t sz)
    {
        auto p = je_malloc_default(sz);
        memLocalInfo::instance().add(malloc_usable_size(p));
        return p;
    }

    inline void __wrap_free(void *p)
    {
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));

        je_free_default(p);
    }

    // override operator new
    inline void *__wrap__Znwm(size_t sz)
    {
        auto p = je_malloc_default(sz);
        memLocalInfo::instance().add(malloc_usable_size(p));
        return p;
    }

    // override operator new[]
    inline void *__wrap__Znam(size_t sz)
    {
        auto p = je_malloc_default(sz);
        memLocalInfo::instance().add(malloc_usable_size(p));
        return p;
    }

    // override operator delete
    inline void __wrap__ZdlPv(void *p)
    {
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));

        je_free_default(p);
    }

    // override operator delete[]
    inline void __wrap__ZdaPv(void *p)
    {
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));

        je_free_default(p);
    }

    // override operator sized operator delete
    inline void __wrap__ZdaPvm(void *p, size_t sz)
    {
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));

        je_free_default(p);
    }

    // override operator sized operator delete[]
    inline void __wrap__ZdlPvm(void *p, size_t sz)
    {
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));

        je_free_default(p);
    }

    inline void *__wrap_calloc(size_t nmemb, size_t size)
    {
        auto p = je_malloc_default(nmemb * size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        return p;
    }

    inline void *__wrap_realloc(void *ptr, size_t size)
    {
        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
        auto p = je_malloc_default(size);
        memLocalInfo::instance().sub(old_size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        return p;
    }

    inline void *__wrap_memalign(size_t alignment, size_t size)
    {
        auto p = je_malloc_default(size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        return p;
    }

    inline void *__wrap_valloc(size_t size)
    {
        auto p = je_malloc_default(size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        return p;
    }

    inline void *__wrap_pvalloc(size_t size)
    {
        auto p = je_malloc_default(size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        return p;
    }

    inline int __wrap_posix_memalign(void **memptr, size_t alignment, size_t size)
    {
        auto p = __real_aligned_alloc(alignment, size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        *memptr = p;
        return 0;
    }

    inline void *__wrap_aligned_alloc(size_t alignment, size_t sz)
    {

        auto p = __real_aligned_alloc(alignment, sz);
        memLocalInfo::instance().add(malloc_usable_size(p));
        return p;
    }

    static std::vector<void *> mmProbeOverrideFunc = {
        (void *)&__wrap_malloc,       (void *)&__wrap_free,          (void *)&__wrap__Znwm,
        (void *)&__wrap__Znam,        (void *)&__wrap__ZdlPv,        (void *)&__wrap__ZdaPv,
        (void *)&__wrap__ZdaPvm,      (void *)&__wrap__ZdlPvm,       (void *)&__wrap_calloc,
        (void *)&__wrap_realloc,      (void *)&__wrap_memalign,      (void *)&__wrap_valloc,
        (void *)&__wrap_pvalloc,      (void *)&__wrap_aligned_alloc, (void *)&__wrap_posix_memalign,
        (void *)&__wrap_aligned_alloc};
};

#elif defined(TCMALLOC)
#include <cstring>
#include <gperftools/tcmalloc.h>

thread_local bool __tcmalloc_in_probe__ = false;
extern "C"
{
    inline void *__wrap_malloc(size_t sz)
    {
        if (__tcmalloc_in_probe__)
            return tc_malloc(sz);

        __tcmalloc_in_probe__ = true;
        auto p = tc_malloc(sz);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        return p;
    }

    inline void __wrap_free(void *p)
    {
        if (__tcmalloc_in_probe__)
        {
            tc_free(p);
            return;
        }

        __tcmalloc_in_probe__ = true;
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        tc_free(p);
    }

    // override operator new
    inline void *__wrap__Znwm(size_t sz)
    {
        if (__tcmalloc_in_probe__)
            return tc_malloc(sz);

        __tcmalloc_in_probe__ = true;
        auto p = tc_malloc(sz);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        return p;
    }

    // override operator new[]
    inline void *__wrap__Znam(size_t sz)
    {
        if (__tcmalloc_in_probe__)
            return tc_malloc(sz);

        __tcmalloc_in_probe__ = true;
        auto p = tc_malloc(sz);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        return p;
    }

    // override operator delete
    inline void __wrap__ZdlPv(void *p)
    {
        if (__tcmalloc_in_probe__)
        {
            tc_free(p);
            return;
        }

        __tcmalloc_in_probe__ = true;
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        tc_free(p);
    }

    // override operator delete[]
    inline void __wrap__ZdaPv(void *p)
    {
        if (__tcmalloc_in_probe__)
        {
            tc_free(p);
            return;
        }

        __tcmalloc_in_probe__ = true;
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        tc_free(p);
    }

    // override operator sized operator delete
    inline void __wrap__ZdaPvm(void *p, size_t sz)
    {
        if (__tcmalloc_in_probe__)
        {
            tc_free_sized(p, sz);
            return;
        }

        __tcmalloc_in_probe__ = true;
        if (p)
        {
            size_t realsz = malloc_usable_size(p);
            if (realsz > sz)
                memLocalInfo::instance().sub(sz);
            else
                memLocalInfo::instance().sub(realsz);
        }
        __tcmalloc_in_probe__ = false;
        tc_free_sized(p, sz);
    }

    // override operator sized operator delete[]
    inline void __wrap__ZdlPvm(void *p, size_t sz)
    {
        if (__tcmalloc_in_probe__)
        {
            tc_free_sized(p, sz);
            return;
        }

        __tcmalloc_in_probe__ = true;
        if (p)
        {
            size_t realsz = malloc_usable_size(p);
            if (realsz > sz)
                memLocalInfo::instance().sub(sz);
            else
                memLocalInfo::instance().sub(realsz);
        }
        __tcmalloc_in_probe__ = false;
        tc_free_sized(p, sz);
    }

    inline void *__wrap_calloc(size_t nmemb, size_t size)
    {
        if (__tcmalloc_in_probe__)
            return tc_calloc(nmemb, size);

        __tcmalloc_in_probe__ = true;
        auto p = tc_calloc(nmemb, size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        return p;
    }

    inline void *__wrap_realloc(void *ptr, size_t size)
    {
        if (__tcmalloc_in_probe__)
            return tc_realloc(ptr, size);

        __tcmalloc_in_probe__ = true;
        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
        auto p = tc_realloc(ptr, size);
        memLocalInfo::instance().sub(old_size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        return p;
    }

    inline void *__wrap_memalign(size_t alignment, size_t size)
    {
        if (__tcmalloc_in_probe__)
            return tc_memalign(alignment, size);

        __tcmalloc_in_probe__ = true;
        auto p = tc_memalign(alignment, size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        return p;
    }

    inline void *__wrap_valloc(size_t size)
    {
        if (__tcmalloc_in_probe__)
            return tc_valloc(size);

        __tcmalloc_in_probe__ = true;
        auto p = tc_valloc(size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        return p;
    }

    inline void *__wrap_pvalloc(size_t size)
    {
        if (__tcmalloc_in_probe__)
            return tc_pvalloc(size);

        __tcmalloc_in_probe__ = true;
        auto p = tc_pvalloc(size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        return p;
    }

    inline int __wrap_posix_memalign(void **memptr, size_t alignment, size_t size)
    {
        if (__tcmalloc_in_probe__)
            return tc_posix_memalign(memptr, alignment, size);

        __tcmalloc_in_probe__ = true;
        void *ptr = nullptr;
        int err = tc_posix_memalign(&ptr, alignment, size);
        if (err == 0)
        {
            size_t actual = malloc_usable_size(ptr);
            memLocalInfo::instance().add(actual);
            *memptr = ptr;
        }
        __tcmalloc_in_probe__ = false;

        return err;
    }

    inline void *__wrap_aligned_alloc(size_t alignment, size_t size)
    {
        if (__tcmalloc_in_probe__)
            return tc_memalign(alignment, size);

        __tcmalloc_in_probe__ = true;
        assert((alignment & (alignment - 1)) == 0);
        assert(size % alignment == 0);

        auto p = tc_memalign(alignment, size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        return p;
    }

    inline void *__wrap_reallocf(void *ptr, size_t size)
    {
        if (__tcmalloc_in_probe__)
            return tc_realloc(ptr, size);

        __tcmalloc_in_probe__ = true;
        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
        auto p = tc_realloc(ptr, size);
        if (!p)
        {
            memLocalInfo::instance().sub(old_size);
            tc_free(ptr);
        }
        else
        {
            memLocalInfo::instance().sub(old_size);
            memLocalInfo::instance().add(malloc_usable_size(p));
        }
        __tcmalloc_in_probe__ = false;
        return p;
    }

    inline void *__wrap_recalloc(void *ptr, size_t nmemb, size_t size)
    {
        if (__tcmalloc_in_probe__)
            return tc_realloc(ptr, nmemb * size);

        __tcmalloc_in_probe__ = true;
        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
        auto p = tc_realloc(ptr, nmemb * size);
        size_t new_size = malloc_usable_size(p);
        if (new_size > old_size)
            memset((char *)p + old_size, 0, new_size - old_size);

        memLocalInfo::instance().sub(old_size);
        memLocalInfo::instance().add(new_size);
        __tcmalloc_in_probe__ = false;
        return p;
    }

    inline void *__wrap_aligned_realloc(void *ptr, size_t size, size_t alignment)
    {
        if (__tcmalloc_in_probe__)
            return tc_memalign(alignment, size);

        __tcmalloc_in_probe__ = true;
        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
        auto p = tc_memalign(alignment, size);
        if (!p)
        {
            __tcmalloc_in_probe__ = false;
            return nullptr;
        }
        tc_free(ptr);
        memLocalInfo::instance().sub(old_size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        return p;
    }

    inline void *__wrap_aligned_recalloc(void *ptr, size_t nmemb, size_t size, size_t alignment)
    {
        if (__tcmalloc_in_probe__)
            return tc_memalign(alignment, nmemb * size);

        __tcmalloc_in_probe__ = true;
        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
        auto p = tc_memalign(alignment, nmemb * size);
        memLocalInfo::instance().sub(old_size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        return p;
    }

    inline void *__wrap_aligned_calloc(size_t alignment, size_t nmemb, size_t size)
    {
        if (__tcmalloc_in_probe__)
            return tc_memalign(alignment, nmemb * size);

        __tcmalloc_in_probe__ = true;
        auto p = tc_memalign(alignment, nmemb * size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        return p;
    }

    inline void *__wrap_aligned_reallocf(void *ptr, size_t size, size_t alignment)
    {
        if (__tcmalloc_in_probe__)
            return tc_memalign(alignment, size);

        __tcmalloc_in_probe__ = true;
        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
        auto p = tc_memalign(alignment, size);
        memLocalInfo::instance().sub(old_size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        return p;
    }

    inline void *__wrap_aligned_recallocf(void *ptr, size_t nmemb, size_t size, size_t alignment)
    {
        if (__tcmalloc_in_probe__)
            return tc_memalign(alignment, nmemb * size);

        __tcmalloc_in_probe__ = true;
        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
        auto p = tc_memalign(alignment, nmemb * size);
        memLocalInfo::instance().sub(old_size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __tcmalloc_in_probe__ = false;
        return p;
    }

    static std::vector<void *> mmProbeOverrideFunc = {
        (void *)&__wrap_malloc, (void *)&__wrap_free,    (void *)&__wrap__Znwm,   (void *)&__wrap__Znam,
        (void *)&__wrap__ZdlPv, (void *)&__wrap__ZdaPv,  (void *)&__wrap__ZdaPvm, (void *)&__wrap__ZdlPvm,
        (void *)&__wrap_calloc, (void *)&__wrap_realloc, (void *)&__wrap_memalign};
};

#else
extern "C"
{
    extern void *__libc_malloc(size_t);
    extern void __libc_free(void *);

    inline void *malloc(size_t sz)
    {
        auto p = __libc_malloc(sz);
        memLocalInfo::instance().add(malloc_usable_size(p));
        return p;
    }

    inline void free(void *p)
    {
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));

        __libc_free(p);
    }

    static std::vector<void *> mmProbeOverrideFunc = {(void *)&malloc, (void *)&free};
};

inline void *operator new(size_t size)
{
    auto p = malloc(size);
    if (!p)
        throw std::bad_alloc();
    return p;
}

inline void *operator new[](size_t size)
{
    auto p = malloc(size);
    if (!p)
        throw std::bad_alloc();
    return p;
}

inline void operator delete(void *p) noexcept
{
    free(p);
}

inline void operator delete[](void *p) noexcept
{
    free(p);
}

namespace __default_malloc_in_probe
{
static void *(*_force_new)(std::size_t) __attribute__((used)) = &operator new;
static void *(*_force_new_array)(std::size_t) __attribute__((used)) = &operator new[];
static void (*_force_delete)(void *) noexcept __attribute__((used)) = &operator delete;
static void (*_force_delete_array)(void *) noexcept __attribute__((used)) = &operator delete[];
} // namespace __default_malloc_in_probe

#endif

#endif
