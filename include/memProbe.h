#ifndef MEM_PROBE_H
#define MEM_PROBE_H

#include <array>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <stdio.h>
#include <string.h>
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

namespace memProbe
{

#define __MEM_MAX_STACK_DEPTH     64
#define __MEM_SMAMPLE_INTERVAL_MS 100
#define __MEM_PROBE_STATUS        1

constexpr const char *__MEM_PATH_JSON_RESULT = "memProbe.data";

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
            usleep(1000 * __MEM_SMAMPLE_INTERVAL_MS);
            _tick += __MEM_SMAMPLE_INTERVAL_MS;
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
    memNode()
    {
    }
    memNode(const char *name) : _name(name)
    {
    }

    void add(const std::array<const char *, __MEM_MAX_STACK_DEPTH> &callstack, const memFrame &frame,
             unsigned depth = 0)
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

    std::string json() const
    {
        std::string s = "{";
        s += memFormat(
            "\"name\": \"%s\", \"malloc\": %lu, \"free\": %lu, \"children\": [", _name, _mallocBytes, _freeBytes);
        for (auto it = _childs.begin(); it != _childs.end(); ++it)
        {
            s += memFormat("%s%s", it != _childs.begin() ? ", " : "", it->second.json().c_str());
        }

        return s += "]}";
    }

    void dump() const
    {
        printf("%s\n", str().c_str());
    }

  protected:
    const char *_name = nullptr;

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

                printf("%u: threadId:%lu alloc %lu free %lu\n%s\n",
                       count++,
                       tid,
                       frame0.mallocBytes,
                       frame0.freeBytes,
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
        for (const auto &[threadId, frames] : _frames)
        {
            for (const auto &[frameId, tickFrames] : frames)
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

        exportJson();

        fflush(stdout);
    }

    void exportJson() const
    {
        FILE *file = fopen(__MEM_PATH_JSON_RESULT, "wb");
        if (file == NULL)
        {
            return;
        }

        // key is tick. second map first is threadId
        std::map<size_t, std::map<size_t, memNode>> datas;
        for (const auto &[tid, pp] : _frames)
        {
            for (const auto &[frameId, ppp] : pp)
            {
                const auto &callstack = _callstacks.at(frameId);
                for (const auto &[tick, frame] : ppp)
                {
                    datas[tick][tid].add(callstack, frame);
                }
            }
        }

        std::string s = "{";
        for (auto it = datas.begin(); it != datas.end(); ++it)
        {
            const auto &tick = it->first;
            s += memFormat("%s\"%lu\": {", it != datas.begin() ? ", " : "", tick);
            for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2)
            {
                const auto &tid = it2->first;
                s += memFormat("%s\"%lu\": %s", it2 != it->second.begin() ? ", " : "", tid, it2->second.json().c_str());
            }

            s += "}";
        }

        s += "}";

        // 写入字符串（包括结尾的 '\0' 可选）
        size_t len = s.size();
        size_t written = fwrite(s.c_str(), sizeof(char), len, file);
        if (written != len)
        {
            perror("Failed to write file");
            fclose(file);
            return;
        }

        // 关闭文件
        fclose(file);
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
    // key is threadId, second map key is frameId, second key is tick second is
    // frame info
    std::map<size_t, std::unordered_map<size_t, std::unordered_map<size_t, memFrame>>> _frames;

    std::map<size_t, std::array<const char *, __MEM_MAX_STACK_DEPTH>> _callstacks;
    std::mutex _lk;

  private:
    static std::unique_ptr<memGlobalInfo> _instance;

    // timer need to init after all other class members
    memTimer _timer;
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
    const std::array<const char *, __MEM_MAX_STACK_DEPTH> &stack() const
    {
        return _stack;
    }

    static memStack &instance()
    {
        thread_local memStack __memStack__;
        return __memStack__;
    }

  protected:
    std::array<const char *, __MEM_MAX_STACK_DEPTH> _stack;
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
    std::unordered_map<size_t, std::array<const char *, __MEM_MAX_STACK_DEPTH>> _callstacks;
    int _nested = 0;
};

class memProbe
{
  public:
    memProbe(const char *name)
    {
        if (__MEM_PROBE_STATUS)
            memStack::instance().push(name);
    }
    ~memProbe()
    {
        if (__MEM_PROBE_STATUS)
            memStack::instance().pop();
    }
};

#define MEM_PROBE memProbe __probe__(__PRETTY_FUNCTION__);

#if __cplusplus >= 202002L
    #define __CPP_STD_20 1
#endif

#if __cplusplus >= 201703L
    #define __CPP_STD_17 1
#endif

#if __cplusplus >= 201402L
    #define __CPP_STD_14 1
#endif

#if __cplusplus >= 201103L
    #define __CPP_STD_11 1
#endif

#if __cplusplus >= 199711L
    #define __CPP_STD_98 1
#endif

extern "C"
{
#if defined(TC_MALLOC)
    #include <gperftools/tcmalloc.h>
#elif defined(JE_MALLOC)
    #include <jemalloc/jemalloc.h>
    extern void *je_sdallocx_default(void *ptr, size_t size, int flags);
    extern void *je_malloc_default(size_t size);
    extern void je_free_default(void *ptr);
#else
    #include <malloc.h>
    extern void *__real_malloc(size_t);
    extern void *__real_calloc(size_t, size_t);
    extern void *__real_realloc(void *, size_t);
    extern void __real_free(void *);
    extern void *__real_aligned_alloc(size_t, size_t);
    extern void *__real_memalign(size_t, size_t);
    extern void *__real_valloc(size_t);
    extern void *__real_pvalloc(size_t);
    extern int __real_posix_memalign(void **, size_t, size_t);
#endif

    static thread_local bool __mem_in_probe_ = false;

#if defined(__CPP_STD_98)
    // override operator new
    inline void *__wrap__Znwm(size_t sz)
    {
        if (sz == 0)
            sz = 1;

        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            void *p = tc_malloc(sz);
    #elif defined(JE_MALLOC)
            void *p = je_malloc_default(sz);
    #else
            void *p = __real_malloc(sz);
    #endif
            if (!p)
                throw std::bad_alloc();
            return p;
        }

        __mem_in_probe_ = true;

        void *p = nullptr;
        for (;;)
        {
    #if defined(TC_MALLOC)
            p = tc_malloc(sz);
    #elif defined(JE_MALLOC)
            p = je_malloc_default(sz);
    #else
            p = __real_malloc(sz);
    #endif
            if (p)
                break;

            std::new_handler h = std::get_new_handler();
            if (!h)
            {
                __mem_in_probe_ = false;
                throw std::bad_alloc();
            }
            try
            {
                h();
            }
            catch (...)
            {
                __mem_in_probe_ = false;
                throw;
            }
        }
        size_t real_sz = malloc_usable_size(p);
        memLocalInfo::instance().add(real_sz);

        __mem_in_probe_ = false;
        return p;
    }

    // override operator new[]
    inline void *__wrap__Znam(size_t sz)
    {
        if (sz == 0)
            sz = 1;

        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            void *p = tc_malloc(sz);
    #elif defined(JE_MALLOC)
            void *p = je_malloc_default(sz);
    #else
            void *p = __real_malloc(sz);
    #endif
            if (!p)
                throw std::bad_alloc();
            return p;
        }

        __mem_in_probe_ = true;

        void *p = nullptr;
        for (;;)
        {
    #if defined(TC_MALLOC)
            p = tc_malloc(sz);
    #elif defined(JE_MALLOC)
            p = je_malloc_default(sz);
    #else
            p = __real_malloc(sz);
    #endif
            if (p)
                break;

            std::new_handler h = std::get_new_handler();
            if (!h)
            {
                __mem_in_probe_ = false;
                throw std::bad_alloc();
            }
            try
            {
                h();
            }
            catch (...)
            {
                __mem_in_probe_ = false;
                throw;
            }
        }

        size_t real_sz = malloc_usable_size(p);
        memLocalInfo::instance().add(real_sz);

        __mem_in_probe_ = false;
        return p;
    }

    // override operator delete
    inline void __wrap__ZdlPv(void *p)
    {
        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            tc_free(p);
    #elif defined(JE_MALLOC)
            je_free_default(p);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));
        __mem_in_probe_ = false;

    #if defined(TC_MALLOC)
        tc_free(p);
    #elif defined(JE_MALLOC)
        je_free_default(p);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete[]
    inline void __wrap__ZdaPv(void *p)
    {
        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            tc_free(p);
    #elif defined(JE_MALLOC)
            je_free_default(p);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));
        __mem_in_probe_ = false;

    #if defined(TC_MALLOC)
        tc_free(p);
    #elif defined(JE_MALLOC)
        je_free_default(p);
    #else
        __real_free(p);
    #endif
    }
#endif

#if defined(__CPP_STD_14)
    // override operator sized operator delete
    inline void __wrap__ZdlPvm(void *p, size_t sz)
    {
        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(JE_MALLOC)
            je_sdallocx_default(p, sz, 0);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));
        __mem_in_probe_ = false;

    #if defined(TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator sized operator delete[]
    inline void __wrap__ZdaPvm(void *p, size_t sz)
    {
        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(JE_MALLOC)
            je_sdallocx_default(p, sz, 0);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));
        __mem_in_probe_ = false;

    #if defined(TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
    }

    inline void *__wrap_malloc(size_t sz)
    {
        if (sz == 0)
            sz = 1;

        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            void *p = tc_malloc(sz);
    #elif defined(JE_MALLOC)
            void *p = je_malloc_default(sz);
    #else
            void *p = __real_malloc(sz);
    #endif
            if (!p)
                throw std::bad_alloc();
            return p;
        }

        __mem_in_probe_ = true;

        void *p = nullptr;
        for (;;)
        {
    #if defined(TC_MALLOC)
            p = tc_malloc(sz);
    #elif defined(JE_MALLOC)
            p = je_malloc_default(sz);
    #else
            p = __real_malloc(sz);
    #endif
            if (p)
                break;

            std::new_handler h = std::get_new_handler();
            if (!h)
            {
                __mem_in_probe_ = false;
                throw std::bad_alloc();
            }
            try
            {
                h();
            }
            catch (...)
            {
                __mem_in_probe_ = false;
                throw;
            }
        }

        size_t real_sz = malloc_usable_size(p);
        memLocalInfo::instance().add(real_sz);

        __mem_in_probe_ = false;
        return p;
    }

    inline void *__wrap_calloc(size_t nmemb, size_t size)
    {
        if (nmemb == 0 || size == 0)
        {
            nmemb = 1;
            size = 1;
        }

        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            void *p = tc_calloc(nmemb, size);
    #elif defined(JE_MALLOC)
            void *p = calloc(nmemb, size);
    #else
            void *p = __real_calloc(nmemb, size);
    #endif
            if (!p)
                throw std::bad_alloc();
            return p;
        }

        __mem_in_probe_ = true;

        void *p = nullptr;
        for (;;)
        {
    #if defined(TC_MALLOC)
            p = tc_calloc(nmemb, size);
    #elif defined(JE_MALLOC)
            p = calloc(nmemb, size);
    #else
            p = __real_calloc(nmemb, size);
    #endif
            if (p)
                break;
            std::new_handler h = std::get_new_handler();
            if (!h)
            {
                __mem_in_probe_ = false;
                throw std::bad_alloc();
            }
            try
            {
                h();
            }
            catch (...)
            {
                __mem_in_probe_ = false;
                throw;
            }
        }

        size_t real_sz = malloc_usable_size(p);
        memLocalInfo::instance().add(real_sz);

        __mem_in_probe_ = false;
        return p;
    }

    inline void *__wrap_realloc(void *ptr, size_t size)
    {
        if (size == 0)
            size = 1;

        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            void *p = tc_realloc(ptr, size);
    #elif defined(JE_MALLOC)
            void *p = realloc(ptr, size);
    #else
            void *p = __real_realloc(ptr, size);
    #endif
            if (!p)
                throw std::bad_alloc();
            return p;
        }

        __mem_in_probe_ = true;

        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;

    #if defined(TC_MALLOC)
        void *p = tc_realloc(ptr, size);
    #elif defined(JE_MALLOC)
        void *p = realloc(ptr, size);
    #else
        void *p = __real_realloc(ptr, size);
    #endif

        if (p)
        {
            memLocalInfo::instance().sub(old_size);
            memLocalInfo::instance().add(malloc_usable_size(p));
        }

        __mem_in_probe_ = false;
        return p;
    }

    inline void __wrap_free(void *p)
    {
        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            tc_free(p);
    #elif defined(JE_MALLOC)
            je_free_default(p);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));
        __mem_in_probe_ = false;

    #if defined(TC_MALLOC)
        tc_free(p);
    #elif defined(JE_MALLOC)
        je_free_default(p);
    #else
        __real_free(p);
    #endif
    }
#endif

#if defined(__CPP_STD_17)
    inline void *__wrap_aligned_alloc(size_t alignment, size_t size)
    {
        if (alignment == 0 || (alignment & (alignment - 1)) || (alignment % sizeof(void *) != 0))
        {
            errno = EINVAL;
            return nullptr;
        }
        if (size % alignment != 0)
        {
            errno = EINVAL;
            return nullptr;
        }

        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            return tc_memalign(alignment, size);
    #elif defined(JE_MALLOC)
            return aligned_alloc(alignment, size);
    #else
            return __real_aligned_alloc(alignment, size);
    #endif
        }

        __mem_in_probe_ = true;

        void *p = nullptr;
    #if defined(TC_MALLOC)
        p = tc_memalign(alignment, size);
    #elif defined(JE_MALLOC)
        p = aligned_alloc(alignment, size);
    #else
        p = __real_aligned_alloc(alignment, size);
    #endif

        if (p)
            memLocalInfo::instance().add(malloc_usable_size(p));

        __mem_in_probe_ = false;
        return p;
    }

    // override operator new(std::size_t, std::align_val_t).
    inline void *__wrap__ZnwmSt11align_val_t(size_t size, std::align_val_t al)
    {
        if (size == 0)
            size = 1;
        size_t alignment = static_cast<size_t>(al);

        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            void *p = tc_memalign(alignment, size);
    #elif defined(JE_MALLOC)
            void *p = mallocx(size, MALLOCX_ALIGN(alignment));
    #else
            size_t sz = (size + alignment - 1) & ~(alignment - 1);
            void *p = __real_aligned_alloc(alignment, sz);
    #endif
            if (!p)
                throw std::bad_alloc();
            return p;
        }

        __mem_in_probe_ = true;

        void *p = nullptr;
        for (;;)
        {
    #if defined(TC_MALLOC)
            p = tc_memalign(alignment, size);
    #elif defined(JE_MALLOC)
            p = mallocx(size, MALLOCX_ALIGN(alignment));
    #else
            size_t sz = (size + alignment - 1) & ~(alignment - 1);
            p = __real_aligned_alloc(alignment, sz);
    #endif
            if (p)
                break;

            std::new_handler h = std::get_new_handler();
            if (!h)
            {
                __mem_in_probe_ = false;
                throw std::bad_alloc();
            }
            try
            {
                h();
            }
            catch (...)
            {
                __mem_in_probe_ = false;
                throw;
            }
        }

    #if defined(JE_MALLOC)
        size_t real_sz = sallocx(p, 0);
    #else
        size_t real_sz = malloc_usable_size(p);
    #endif
        memLocalInfo::instance().add(real_sz);

        __mem_in_probe_ = false;
        return p;
    }

    // override operator new[](std::size_t size, std::align_val_t alignment).
    inline void *__wrap__ZnamSt11align_val_t(size_t size, std::align_val_t al)
    {
        if (size == 0)
            size = 1;
        size_t alignment = static_cast<size_t>(al);

        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            void *p = tc_memalign(alignment, size);
    #elif defined(JE_MALLOC)
            void *p = mallocx(size, MALLOCX_ALIGN(alignment));
    #else
            size_t sz = (size + alignment - 1) & ~(alignment - 1);
            void *p = __real_aligned_alloc(alignment, sz);
    #endif
            if (!p)
                throw std::bad_alloc();
            return p;
        }

        __mem_in_probe_ = true;

        void *p = nullptr;
        for (;;)
        {
    #if defined(TC_MALLOC)
            p = tc_memalign(alignment, size);
    #elif defined(JE_MALLOC)
            p = mallocx(size, MALLOCX_ALIGN(alignment));
    #else
            size_t sz = (size + alignment - 1) & ~(alignment - 1);
            p = __real_aligned_alloc(alignment, sz);
    #endif
            if (p)
                break;

            std::new_handler h = std::get_new_handler();
            if (!h)
            {
                __mem_in_probe_ = false;
                throw std::bad_alloc();
            }
            try
            {
                h();
            }
            catch (...)
            {
                __mem_in_probe_ = false;
                throw;
            }
        }

    #if defined(JE_MALLOC)
        size_t real_sz = sallocx(p, 0);
    #else
        size_t real_sz = malloc_usable_size(p);
    #endif
        memLocalInfo::instance().add(real_sz);

        __mem_in_probe_ = false;
        return p;
    }

    // override operator delete(void *p, std::align_val_t al)
    inline void __wrap__ZdlPvSt11align_val_t(void *p, std::align_val_t al)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            tc_free(p);
    #elif defined(JE_MALLOC)
            dallocx(p, 0);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
        {
    #if defined(JE_MALLOC)
            size_t real_sz = sallocx(p, 0);
    #else
            size_t real_sz = malloc_usable_size(p);
    #endif
            memLocalInfo::instance().sub(real_sz);
        }
        __mem_in_probe_ = false;

    #if defined(TC_MALLOC)
        tc_free(p);
    #elif defined(JE_MALLOC)
        dallocx(p, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete[](void *p, std::align_val_t al)
    inline void __wrap__ZdaPvSt11align_val_t(void *p, std::align_val_t al)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            tc_free(p);
    #elif defined(JE_MALLOC)
            dallocx(p, 0);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
        {
    #if defined(JE_MALLOC)
            size_t real_sz = sallocx(p, 0);
    #else
            size_t real_sz = malloc_usable_size(p);
    #endif
            memLocalInfo::instance().sub(real_sz);
        }
        __mem_in_probe_ = false;

    #if defined(TC_MALLOC)
        tc_free(p);
    #elif defined(JE_MALLOC)
        dallocx(p, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete(void *p, size_t sz, std::align_val_t al)
    inline void __wrap__ZdlPvmSt11align_val_t(void *p, size_t sz, std::align_val_t al)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(JE_MALLOC)
            je_sdallocx_default(p, sz, 0);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
        {
    #if defined(JE_MALLOC)
            size_t real_sz = sallocx(p, 0);
    #else
            size_t real_sz = malloc_usable_size(p);
    #endif
            memLocalInfo::instance().sub(real_sz);
        }
        __mem_in_probe_ = false;

    #if defined(TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete[](void *p, size_t sz, std::align_val_t al)
    inline void __wrap__ZdaPvmSt11align_val_t(void *p, size_t sz, std::align_val_t al)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(JE_MALLOC)
            je_sdallocx_default(p, sz, 0);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
        {
    #if defined(JE_MALLOC)
            size_t real_sz = sallocx(p, 0);
    #else
            size_t real_sz = malloc_usable_size(p);
    #endif
            memLocalInfo::instance().sub(real_sz);
        }
        __mem_in_probe_ = false;

    #if defined(TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
    }
#endif

    /*
    inline void *__wrap_memalign(size_t alignment, size_t size)
    {
        if (__mem_in_probe_)
            return tc_memalign(alignment, size);

        __mem_in_probe_ = true;
        auto p = tc_memalign(alignment, size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __mem_in_probe_ = false;
        return p;
    }

    inline void *__wrap_valloc(size_t size)
    {
        if (__mem_in_probe_)
            return tc_valloc(size);

        __mem_in_probe_ = true;
        auto p = tc_valloc(size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __mem_in_probe_ = false;
        return p;
    }

    inline void *__wrap_pvalloc(size_t size)
    {
        if (__mem_in_probe_)
            return tc_pvalloc(size);

        __mem_in_probe_ = true;
        auto p = tc_pvalloc(size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __mem_in_probe_ = false;
        return p;
    }

    inline int __wrap_posix_memalign(void **memptr, size_t alignment, size_t size)
    {
        if (__mem_in_probe_)
            return tc_posix_memalign(memptr, alignment, size);

        __mem_in_probe_ = true;
        void *ptr = nullptr;
        int err = tc_posix_memalign(&ptr, alignment, size);
        if (err == 0)
        {
            size_t actual = malloc_usable_size(ptr);
            memLocalInfo::instance().add(actual);
            *memptr = ptr;
        }
        __mem_in_probe_ = false;

        return err;
    }

    inline void *__wrap_reallocf(void *ptr, size_t size)
    {
        if (__mem_in_probe_)
            return tc_realloc(ptr, size);

        __mem_in_probe_ = true;
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
        __mem_in_probe_ = false;
        return p;
    }

    inline void *__wrap_recalloc(void *ptr, size_t nmemb, size_t size)
    {
        if (__mem_in_probe_)
            return tc_realloc(ptr, nmemb * size);

        __mem_in_probe_ = true;
        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
        auto p = tc_realloc(ptr, nmemb * size);
        size_t new_size = malloc_usable_size(p);
        if (new_size > old_size)
            memset((char *)p + old_size, 0, new_size - old_size);

        memLocalInfo::instance().sub(old_size);
        memLocalInfo::instance().add(new_size);
        __mem_in_probe_ = false;
        return p;
    }
    */

    /*
    inline void *__wrap_aligned_calloc(size_t alignment, size_t nmemb, size_t size)
    {
        if (__mem_in_probe_)
            return tc_memalign(alignment, nmemb * size);

        __mem_in_probe_ = true;
        auto p = tc_memalign(alignment, nmemb * size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __mem_in_probe_ = false;
        return p;
    }

    inline void *__wrap_aligned_realloc(void *ptr, size_t size, size_t alignment)
    {
        if (__mem_in_probe_)
            return tc_memalign(alignment, size);

        __mem_in_probe_ = true;
        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
        auto p = tc_memalign(alignment, size);
        if (!p)
        {
            __mem_in_probe_ = false;
            return nullptr;
        }
        tc_free(ptr);
        memLocalInfo::instance().sub(old_size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __mem_in_probe_ = false;
        return p;
    }

    inline void *__wrap_aligned_recalloc(void *ptr, size_t nmemb, size_t size, size_t alignment)
    {
        if (__mem_in_probe_)
            return tc_memalign(alignment, nmemb * size);

        __mem_in_probe_ = true;
        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
        auto p = tc_memalign(alignment, nmemb * size);
        memLocalInfo::instance().sub(old_size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __mem_in_probe_ = false;
        return p;
    }

    inline void *__wrap_aligned_reallocf(void *ptr, size_t size, size_t alignment)
    {
        if (__mem_in_probe_)
            return tc_memalign(alignment, size);

        __mem_in_probe_ = true;
        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
        auto p = tc_memalign(alignment, size);
        memLocalInfo::instance().sub(old_size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __mem_in_probe_ = false;
        return p;
    }

    inline void *__wrap_aligned_recallocf(void *ptr, size_t nmemb, size_t size, size_t alignment)
    {
        if (__mem_in_probe_)
            return tc_memalign(alignment, nmemb * size);

        __mem_in_probe_ = true;
        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
        auto p = tc_memalign(alignment, nmemb * size);
        memLocalInfo::instance().sub(old_size);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __mem_in_probe_ = false;
        return p;
    }

    // nothrow
    inline void *__wrap__ZnwmRKSt9nothrow_t(size_t sz, const std::nothrow_t &)
    {
        if (__mem_in_probe_)
            return tc_malloc(sz);

        __mem_in_probe_ = true;
        auto p = tc_malloc(sz);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __mem_in_probe_ = false;
        return p;
    }

    inline void *__wrap__ZnamRKSt9nothrow_t(size_t sz, const std::nothrow_t &)
    {
        if (__mem_in_probe_)
            return tc_malloc(sz);

        __mem_in_probe_ = true;
        auto p = tc_malloc(sz);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __mem_in_probe_ = false;
        return p;
    }

    inline void __wrap__ZdlPvRKSt9nothrow_t(void *p, const std::nothrow_t &)
    {
        if (__mem_in_probe_)
        {
            tc_free(p);
            return;
        }

        __mem_in_probe_ = true;
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));
        __mem_in_probe_ = false;
        tc_free(p);
    }

    inline void __wrap__ZdaPvRKSt9nothrow_t(void *p, const std::nothrow_t &)
    {
        if (__mem_in_probe_)
        {
            tc_free(p);
            return;
        }

        __mem_in_probe_ = true;
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));
        __mem_in_probe_ = false;
        tc_free(p);
    }

    inline void __wrap__ZdlPvmRKSt9nothrow_t(void *p, size_t sz, const std::nothrow_t &)
    {
        if (__mem_in_probe_)
        {
            tc_free_sized(p, sz);
            return;
        }

        __mem_in_probe_ = true;
        if (p)
        {
            size_t realsz = malloc_usable_size(p);
            if (realsz > sz)
                memLocalInfo::instance().sub(sz);
            else
                memLocalInfo::instance().sub(realsz);
        }
        __mem_in_probe_ = false;
        tc_free_sized(p, sz);
    }

    inline void __wrap__ZdaPvmRKSt9nothrow_t(void *p, size_t sz, const std::nothrow_t &)
    {
        if (__mem_in_probe_)
        {
            tc_free_sized(p, sz);
            return;
        }

        __mem_in_probe_ = true;
        if (p)
        {
            size_t realsz = malloc_usable_size(p);
            if (realsz > sz)
                memLocalInfo::instance().sub(sz);
            else
                memLocalInfo::instance().sub(realsz);
        }
        __mem_in_probe_ = false;
        tc_free_sized(p, sz);
    }

    inline void __wrap__ZdlPvSt11align_val_tRKSt9nothrow_t(void *p, std::align_val_t al, const std::nothrow_t &)
    {
        if (__mem_in_probe_)
        {
            tc_delete_aligned(p, al);
            return;
        }

        __mem_in_probe_ = true;
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));
        __mem_in_probe_ = false;
        tc_delete_aligned(p, al);
    }

    inline void __wrap__ZdaPvSt11align_val_tRKSt9nothrow_t(void *p, std::align_val_t al, const std::nothrow_t &)
    {
        if (__mem_in_probe_)
        {
            tc_deletearray_aligned(p, al);
            return;
        }

        __mem_in_probe_ = true;
        if (p)
            memLocalInfo::instance().sub(malloc_usable_size(p));
        __mem_in_probe_ = false;
        tc_deletearray_aligned(p, al);
    }

    inline void __wrap__ZdlPvmSt11align_val_tRKSt9nothrow_t(void *p, size_t sz, std::align_val_t al,
                                                            const std::nothrow_t &)
    {
        if (__mem_in_probe_)
        {
            tc_deletearray_sized_aligned(p, sz, al);
            return;
        }

        __mem_in_probe_ = true;
        if (p)
        {
            size_t realsz = malloc_usable_size(p);
            if (realsz > sz)
                memLocalInfo::instance().sub(sz);
            else
                memLocalInfo::instance().sub(realsz);
        }
        __mem_in_probe_ = false;
        tc_deletearray_sized_aligned(p, sz, al);
    }

    inline void __wrap__ZdaPvmSt11align_val_tRKSt9nothrow_t(void *p, size_t sz, std::align_val_t al,
                                                            const std::nothrow_t &)
    {
        if (__mem_in_probe_)
        {
            tc_delete_sized_aligned(p, sz, al);
            return;
        }

        __mem_in_probe_ = true;
        if (p)
        {
            size_t realsz = malloc_usable_size(p);
            if (realsz > sz)
                memLocalInfo::instance().sub(sz);
            else
                memLocalInfo::instance().sub(realsz);
        }
        __mem_in_probe_ = false;
        tc_delete_sized_aligned(p, sz, al);
    }

    inline void *__wrap__ZnwmSt11align_val_tRKSt9nothrow_t(size_t size, std::align_val_t al, const std::nothrow_t &)
    {
        if (__mem_in_probe_)
            return tc_new_aligned(size, al);

        __mem_in_probe_ = true;
        auto p = tc_new_aligned(size, al);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __mem_in_probe_ = false;
        return p;
    }

    inline void *__wrap__ZnamSt11align_val_tRKSt9nothrow_t(size_t size, std::align_val_t al, const std::nothrow_t &)
    {
        if (__mem_in_probe_)
            return tc_new_aligned(size, al);

        __mem_in_probe_ = true;
        auto p = tc_new_aligned(size, al);
        memLocalInfo::instance().add(malloc_usable_size(p));
        __mem_in_probe_ = false;
        return p;
    }

    // sys calls
    inline void *__wrap_sbrk(intptr_t increment)
    {
        void *ret = nullptr;
        return ret;
    }

    inline int __wrap_brk(void *addr)
    {
        int rc = 0;
        return rc;
    }

    inline void *__wrap_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
    {
        void *p = nullptr;
        return p;
    }

    inline int __wrap_munmap(void *addr, size_t length)
    {
        return 0;
    }

    inline void *__wrap_mremap(void *old_address, size_t old_size, size_t new_size, int flags, ...)
    {
        void *new_address = nullptr;
        return new_address;
    }
    */

    static std::vector<void *> mmProbeOverrideFunc = {
        (void *) &__wrap__Znwm,
        (void *) &__wrap__Znam,
        (void *) &__wrap__ZdlPv,
        (void *) &__wrap__ZdaPv,
        (void *) &__wrap__ZdlPvm,
        (void *) &__wrap__ZdaPvm,
        (void *) &__wrap_malloc,
        (void *) &__wrap_calloc,
        (void *) &__wrap_realloc,
        (void *) &__wrap_free,
        (void *) &__wrap_aligned_alloc,
        (void *) &__wrap__ZdlPvSt11align_val_t,
        (void *) &__wrap__ZdaPvSt11align_val_t,
        (void *) &__wrap__ZdlPvmSt11align_val_t,
        (void *) &__wrap__ZdaPvmSt11align_val_t,
        (void *) &__wrap__ZnwmSt11align_val_t,
        (void *) &__wrap__ZnamSt11align_val_t,
        //   (void *)&__wrap_memalign,
        //   (void *)&__wrap_valloc,
        //   (void *)&__wrap_pvalloc,
        //   (void *)&__wrap_posix_memalign,
        //   (void *)&__wrap_reallocf,
        //   (void *)&__wrap_recalloc,
        //   (void *)&__wrap_aligned_calloc,
        //   (void *)&__wrap_aligned_realloc,
        //   (void *)&__wrap_aligned_recalloc,
        //   (void *)&__wrap_aligned_reallocf,
        //   (void *)&__wrap_aligned_recallocf,
        //   (void *)&__wrap__ZnwmRKSt9nothrow_t,
        //   (void *)&__wrap__ZnamRKSt9nothrow_t,
        //   (void *)&__wrap__ZdlPvRKSt9nothrow_t,
        //   (void *)&__wrap__ZdaPvRKSt9nothrow_t,
        //   (void *)&__wrap__ZdlPvmRKSt9nothrow_t,
        //   (void *)&__wrap__ZdaPvmRKSt9nothrow_t,
        //   (void *)&__wrap__ZdlPvSt11align_val_tRKSt9nothrow_t,
        //   (void *)&__wrap__ZdaPvSt11align_val_tRKSt9nothrow_t,
        //   (void *)&__wrap__ZdlPvmSt11align_val_tRKSt9nothrow_t,
        //   (void *)&__wrap__ZdaPvmSt11align_val_tRKSt9nothrow_t,
        //   (void *)&__wrap__ZnwmSt11align_val_tRKSt9nothrow_t,
        //   (void *)&__wrap__ZnamSt11align_val_tRKSt9nothrow_t,
        //   (void *)&__wrap_sbrk,
        //   (void *)&__wrap_brk,
        //   (void *)&__wrap_mmap,
        //   (void *)&__wrap_munmap,
        //   (void *)&__wrap_mremap
    };
};
} // namespace memProbe

#endif
