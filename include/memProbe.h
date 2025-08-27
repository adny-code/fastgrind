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
#include <vector>

#include <assert.h>
#include <cstdlib>
#include <locale.h>
#include <malloc.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

namespace __MERECORDER__
{

#if __cplusplus >= 202002L
    // #pragma message("C++20")
    #define __CPP_STD_20 1
#endif

#if __cplusplus >= 201703L
    // #pragma message("C++17")
    #define __CPP_STD_17 1
#endif

#if __cplusplus >= 201402L
    // #pragma message("C++14")
    #define __CPP_STD_14 1
#endif

#if __cplusplus >= 201103L
    // #pragma message("C++11")
    #define __CPP_STD_11 1
#endif

#if __cplusplus >= 199711L
    // #pragma message("C++98")
    #define __CPP_STD_98 1
#endif

#define __MEM_MAX_STACK_DEPTH     64
#define __MEM_SMAMPLE_INTERVAL_MS 100
#define __MEM_PROBE_STATUS        1

constexpr const char *__MEM_PATH_JSON_RESULT = "merecorder.json";

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

        for (auto it = _childs.begin(); it != _childs.end(); ++it)
        {
            s += std::string("\n") + it->second.str(indent + 1);
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
            _instance = std::unique_ptr<memGlobalInfo>(new memGlobalInfo);
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
        for (auto it = _frames.begin(); it != _frames.end(); ++it)
        {
            const auto &tid = it->first;
            const auto &threadsInfo = it->second;
            for (auto it2 = threadsInfo.begin(); it2 != threadsInfo.end(); ++it2)
            {
                const auto &frameId = it2->first;
                const auto &tickInfo = it2->second;
                memFrame frame0(0, 0, tickInfo.begin()->second.funcId, tickInfo.begin()->second.frameId);
                for (auto it3 = tickInfo.begin(); it3 != tickInfo.end(); ++it3)
                {
                    frame0 += it3->second;
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
        for (auto it = _callstacks.begin(); it != _callstacks.end(); ++it)
        {
            printf("%u: frame:%lu\n%s\n", count++, it->first, getCallstack(it->first).c_str());
        }

        printf("\n");
        printf("[Dump By Callstack]\n");
        memNode info("this");
        for (auto it = _frames.begin(); it != _frames.end(); ++it)
        {
            const auto &frames = it->second;
            for (auto it2 = frames.begin(); it2 != frames.end(); ++it2)
            {
                const auto &frameId = it2->first;
                const auto &tickFrames = it2->second;
                assert(_callstacks.find(frameId) != _callstacks.end());
                const auto &callstack = _callstacks.at(frameId);
                for (auto it3 = tickFrames.begin(); it3 != tickFrames.end(); ++it3)
                {
                    info.add(callstack, it3->second);
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
        if (!file)
            return;

        std::map<size_t, std::map<size_t, memNode>> datas;
        for (auto it = _frames.begin(); it != _frames.end(); ++it)
        {
            const auto &tid = it->first;
            for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2)
            {
                const auto &frameId = it2->first;
                const auto &callstack = _callstacks.at(frameId);
                for (auto it3 = it2->second.begin(); it3 != it2->second.end(); ++it3)
                {
                    const auto &tick = it3->first;
                    datas[tick][tid].add(callstack, it3->second);
                }
            }
        }

        std::string compact = "{";
        for (auto it = datas.begin(); it != datas.end(); ++it)
        {
            const auto &tick = it->first;
            compact += memFormat("%s\"%lu\": {", it != datas.begin() ? ", " : "", tick);
            for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2)
            {
                const auto &tid = it2->first;
                compact +=
                    memFormat("%s\"%lu\": %s", it2 != it->second.begin() ? ", " : "", tid, it2->second.json().c_str());
            }
            compact += "}";
        }
        compact += "}";

        std::string pretty;
        pretty.reserve(compact.size() * 2);
        int indent = 0;
        bool inString = false;
        char prev = 0;

        auto appendIndent = [&]() { pretty.append(indent, ' '); };

        for (size_t i = 0; i < compact.size(); ++i)
        {
            char c = compact[i];

            if (c == '"' && prev != '\\')
                inString = !inString;

            if (!inString)
            {
                switch (c)
                {
                case '{':
                case '[':
                    if (i + 1 < compact.size() &&
                        ((c == '{' && compact[i + 1] == '}') || (c == '[' && compact[i + 1] == ']')))
                    {
                        pretty += c;
                        pretty += compact[++i];
                    }
                    else
                    {
                        pretty += c;
                        pretty += '\n';
                        indent += 4;
                        appendIndent();
                    }
                    break;
                case '}':
                case ']':
                    pretty += '\n';
                    indent -= 4;
                    if (indent < 0)
                        indent = 0;
                    appendIndent();
                    pretty += c;
                    break;
                case ',':
                    pretty += c;
                    pretty += '\n';
                    appendIndent();
                    while (i + 1 < compact.size() && compact[i + 1] == ' ')
                        ++i;
                    break;
                case ':':
                    pretty += ": ";
                    while (i + 1 < compact.size() && compact[i + 1] == ' ')
                        ++i;
                    break;
                default:
                    if (c == ' ' && !pretty.empty() && pretty.back() == '\n')
                    {
                    }
                    else
                    {
                        pretty += c;
                    }
                    break;
                }
            }
            else
            {
                pretty += c;
            }
            prev = c;
        }

        fwrite(pretty.c_str(), 1, pretty.size(), file);
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

#if defined(__CPP_STD_17)
inline std::unique_ptr<memGlobalInfo> memGlobalInfo::_instance = nullptr;
#else
std::unique_ptr<memGlobalInfo> memGlobalInfo::_instance = nullptr;
#endif

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

        memGlobalInfo::instance()._frames[tid] = _frames;

        for (auto it = _callstacks.begin(); it != _callstacks.end(); ++it)
            memGlobalInfo::instance()._callstacks[it->first] = it->second;

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
    #define DEFAULT_MALLOC 1
    // #define __USE_SYS_WRAP 1
    #include <malloc.h>
    #include <stdlib.h>
    extern void *__real_malloc(size_t);
    extern void *__real_calloc(size_t, size_t);
    extern void *__real_realloc(void *, size_t);
    extern void __real_free(void *);
    extern void *__real_aligned_alloc(size_t, size_t);
    extern void *__real_memalign(size_t, size_t);
    extern void *__real_valloc(size_t);
    extern void *__real_pvalloc(size_t);
    extern int __real_posix_memalign(void **, size_t, size_t);
    extern void *__real_sbrk(intptr_t);
    extern int __real_brk(void *);
    extern void *__real_mmap(void *, size_t, int, int, int, off_t);
    extern int __real_munmap(void *, size_t);
    extern void *__real_mremap(void *, size_t, size_t, int, ...);
#endif

    static thread_local bool __mem_in_probe_ = false;

#if defined(__CPP_STD_98)
    inline void *__wrap_malloc(size_t sz)
    {
        if (sz == 0)
            sz = 1;

        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            return tc_malloc(sz);
    #elif defined(JE_MALLOC)
            return je_malloc_default(sz);
    #else
            return __real_malloc(sz);
    #endif
        }

        __mem_in_probe_ = true;

        void *p =
    #if defined(TC_MALLOC)
            tc_malloc(sz);
    #elif defined(JE_MALLOC)
            je_malloc_default(sz);
    #else
            __real_malloc(sz);
    #endif

        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().add(real_sz);
        }

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
            return tc_calloc(nmemb, size);
    #elif defined(JE_MALLOC)
            return calloc(nmemb, size);
    #else
            return __real_calloc(nmemb, size);
    #endif
        }

        __mem_in_probe_ = true;

        void *p =
    #if defined(TC_MALLOC)
            tc_calloc(nmemb, size);
    #elif defined(JE_MALLOC)
            calloc(nmemb, size);
    #else
            __real_calloc(nmemb, size);
    #endif

        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().add(real_sz);
        }

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
            return tc_realloc(ptr, size);
    #elif defined(JE_MALLOC)
            return realloc(ptr, size);
    #else
            return __real_realloc(ptr, size);
    #endif
        }

        __mem_in_probe_ = true;

        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;

        void *p =
    #if defined(TC_MALLOC)
            tc_realloc(ptr, size);
    #elif defined(JE_MALLOC)
            realloc(ptr, size);
    #else
            __real_realloc(ptr, size);
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

    // override operator new with nothrow
    inline void *__wrap__ZnwmRKSt9nothrow_t(size_t sz, const std::nothrow_t &)
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
                break;
            }
            try
            {
                h();
            }
            catch (...)
            {
                __mem_in_probe_ = false;
                return nullptr;
            }
        }
        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().add(real_sz);
        }

        __mem_in_probe_ = false;
        return p;
    }

    // override operator new[] with nothrow
    inline void *__wrap__ZnamRKSt9nothrow_t(size_t sz, const std::nothrow_t &)
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
                break;
            }
            try
            {
                h();
            }
            catch (...)
            {
                __mem_in_probe_ = false;
                return nullptr;
            }
        }

        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().add(real_sz);
        }

        __mem_in_probe_ = false;
        return p;
    }

    // override operator delete with nothrow
    inline void __wrap__ZdlPvRKSt9nothrow_t(void *p, const std::nothrow_t &)
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

    // override operator delete[] with nothrow
    inline void __wrap__ZdaPvRKSt9nothrow_t(void *p, const std::nothrow_t &)
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

    #if defined(DEFAULT_MALLOC) && defined(__USE_SYS_WRAP)
    // sys call
    inline void *__wrap_sbrk(intptr_t increment)
    {
        if (__mem_in_probe_)
            return __real_sbrk(increment);

        __mem_in_probe_ = true;

        void *ret = __real_sbrk(increment);
        if (ret != (void *) -1)
        {
            if (increment > 0)
                memLocalInfo::instance().add(static_cast<size_t>(increment));
            else if (increment < 0)
                memLocalInfo::instance().sub(static_cast<size_t>(-increment));
        }

        __mem_in_probe_ = false;
        return ret;
    }

    // sys call
    inline int __wrap_brk(void *addr)
    {
        if (__mem_in_probe_)
            return __real_brk(addr);

        __mem_in_probe_ = true;

        void *old_brk = __real_sbrk(0);
        int ret = __real_brk(addr);
        if (ret == 0)
        {
            void *new_brk = __real_sbrk(0);
            intptr_t diff = static_cast<char *>(new_brk) - static_cast<char *>(old_brk);
            if (diff > 0)
                memLocalInfo::instance().add(static_cast<size_t>(diff));
            else if (diff < 0)
                memLocalInfo::instance().sub(static_cast<size_t>(-diff));
        }

        __mem_in_probe_ = false;
        return ret;
    }

    // sys call
    inline void *__wrap_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
    {
        if (__mem_in_probe_)
            return __real_mmap(addr, length, prot, flags, fd, offset);

        __mem_in_probe_ = true;

        void *p = __real_mmap(addr, length, prot, flags, fd, offset);
        if (p != (void *) -1)
            memLocalInfo::instance().add(length);

        __mem_in_probe_ = false;
        return p;
    }

    // sys call
    inline int __wrap_munmap(void *addr, size_t length)
    {
        if (__mem_in_probe_)
            return __real_munmap(addr, length);

        __mem_in_probe_ = true;

        int ret = __real_munmap(addr, length);
        if (ret == 0)
            memLocalInfo::instance().sub(length);

        __mem_in_probe_ = false;
        return ret;
    }

    // sys call
    inline void *__wrap_mremap(void *old_address, size_t old_size, size_t new_size, int flags, ...)
    {
        void *new_addr_opt = nullptr;
        #ifdef MREMAP_FIXED
        if (flags & MREMAP_FIXED)
        {
            va_list ap;
            va_start(ap, flags);
            new_addr_opt = va_arg(ap, void *);
            va_end(ap);
        }
        #endif

        if (__mem_in_probe_)
        {
        #ifdef MREMAP_FIXED
            if (flags & MREMAP_FIXED)
                return __real_mremap(old_address, old_size, new_size, flags, new_addr_opt);
        #endif
            return __real_mremap(old_address, old_size, new_size, flags);
        }

        __mem_in_probe_ = true;

        void *ret =
        #ifdef MREMAP_FIXED
            (flags & MREMAP_FIXED) ? __real_mremap(old_address, old_size, new_size, flags, new_addr_opt)
                                   : __real_mremap(old_address, old_size, new_size, flags);
        #else
            __real_mremap(old_address, old_size, new_size, flags);
        #endif

        if (ret != (void *) -1)
        {
            if (new_size > old_size)
                memLocalInfo::instance().add(new_size - old_size);
            else if (new_size < old_size)
                memLocalInfo::instance().sub(old_size - new_size);
        }

        __mem_in_probe_ = false;
        return ret;
    }
    #endif

    // glibc function, glibc 2.12 abort this function
    inline void *__wrap_valloc(size_t size)
    {
        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            return tc_valloc(size);
    #elif defined(JE_MALLOC)
            return valloc(size);
    #else
            return __real_valloc(size);
    #endif
        }

        __mem_in_probe_ = true;

        void *p =
    #if defined(TC_MALLOC)
            tc_valloc(size);
    #elif defined(JE_MALLOC)
            valloc(size);
    #else
            __real_valloc(size);
    #endif

        if (p)
        {
            memLocalInfo::instance().add(malloc_usable_size(p));
        }

        __mem_in_probe_ = false;
        return p;
    }

    // glibc function, glibc 2.12 abort this function
    inline void *__wrap_pvalloc(size_t size)
    {
        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            return tc_pvalloc(size);
    #elif defined(JE_MALLOC)
            return pvalloc(size);
    #else
            return __real_pvalloc(size);
    #endif
        }

        __mem_in_probe_ = true;

        void *p =
    #if defined(TC_MALLOC)
            tc_pvalloc(size);
    #elif defined(JE_MALLOC)
            pvalloc(size);
    #else
            __real_pvalloc(size);
    #endif

        if (p)
        {
            memLocalInfo::instance().add(malloc_usable_size(p));
        }

        __mem_in_probe_ = false;
        return p;
    }

    // glibc function
    inline void *__wrap_memalign(size_t alignment, size_t size)
    {
        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            return tc_memalign(alignment, size);
    #elif defined(JE_MALLOC)
            return memalign(alignment, size);
    #else
            return __real_memalign(alignment, size);
    #endif
        }

        __mem_in_probe_ = true;

        void *p =
    #if defined(TC_MALLOC)
            tc_memalign(alignment, size);
    #elif defined(JE_MALLOC)
            memalign(alignment, size);
    #else
            __real_memalign(alignment, size);
    #endif

        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().add(real_sz);
        }

        __mem_in_probe_ = false;
        return p;
    }

    // glibc function
    inline int __wrap_posix_memalign(void **memptr, size_t alignment, size_t size)
    {
        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            return tc_posix_memalign(memptr, alignment, size);
    #elif defined(JE_MALLOC)
            return posix_memalign(memptr, alignment, size);
    #else
            return __real_posix_memalign(memptr, alignment, size);
    #endif
        }

        __mem_in_probe_ = true;

        int ret =
    #if defined(TC_MALLOC)
            tc_posix_memalign(memptr, alignment, size);
    #elif defined(JE_MALLOC)
            posix_memalign(memptr, alignment, size);
    #else
            __real_posix_memalign(memptr, alignment, size);
    #endif

        if (ret == 0 && memptr && *memptr)
        {
    #if defined(JE_MALLOC)
            size_t real_sz = sallocx(*memptr, 0);
    #else
            size_t real_sz = malloc_usable_size(*memptr);
    #endif
            memLocalInfo::instance().add(real_sz);
        }

        __mem_in_probe_ = false;
        return ret;
    }

    // glibc function
    inline void *__wrap_reallocarray(void *ptr, size_t nmemb, size_t size)
    {
        if (nmemb == 0 || size == 0)
        {
            nmemb = 1;
            size = 1;
        }

        size_t total = 0;
    #if defined(__GNUC__)
        if (__builtin_mul_overflow(nmemb, size, &total))
        {
            errno = ENOMEM;
            return NULL;
        }
    #else
        if (size != 0 && nmemb > static_cast<size_t>(-1) / size)
        {
            errno = ENOMEM;
            return NULL;
        }
        total = nmemb * size;
    #endif

        if (__mem_in_probe_)
        {
    #if defined(TC_MALLOC)
            return tc_realloc(ptr, total);
    #elif defined(JE_MALLOC)
            return realloc(ptr, total);
    #else
            return __real_realloc(ptr, total);
    #endif
        }

        __mem_in_probe_ = true;

        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;

        void *p =
    #if defined(TC_MALLOC)
            tc_realloc(ptr, total);
    #elif defined(JE_MALLOC)
            realloc(ptr, total);
    #else
            __real_realloc(ptr, total);
    #endif

        if (p)
        {
            memLocalInfo::instance().sub(old_size);
            memLocalInfo::instance().add(malloc_usable_size(p));
        }

        __mem_in_probe_ = false;
        return p;
    }
    // #endif

    // #if defined(__CPP_STD_14)
    // override operator delete(void*, std::size_t)
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

    // override operator delete[](void*, std::size_t)
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

    // override operator new(std::size_t, std::align_val_t)
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

    // override operator new[](std::size_t size, std::align_val_t alignment)
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

    // override operator delete(void *p, size_t sz, const std::nothrow_t &)
    inline void __wrap__ZdlPvmRKSt9nothrow_t(void *p, size_t sz, const std::nothrow_t &)
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

    // override operator delete[](void *p, size_t sz, const std::nothrow_t &)
    inline void __wrap__ZdaPvmRKSt9nothrow_t(void *p, size_t sz, const std::nothrow_t &)
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

    // override operator new(std::size_t size, std::align_val_t al, const std::nothrow_t &)
    inline void *__wrap__ZnwmSt11align_val_tRKSt9nothrow_t(size_t size, std::align_val_t al, const std::nothrow_t &)
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
                break;
            }
            try
            {
                h();
            }
            catch (...)
            {
                __mem_in_probe_ = false;
                return nullptr;
            }
        }

        if (p)
        {
    #if defined(JE_MALLOC)
            size_t real_sz = sallocx(p, 0);
    #else
            size_t real_sz = malloc_usable_size(p);
    #endif
            memLocalInfo::instance().add(real_sz);
        }

        __mem_in_probe_ = false;
        return p;
    }

    // override operator new[](std::size_t size, std::align_val_t al, const std::nothrow_t &)
    inline void *__wrap__ZnamSt11align_val_tRKSt9nothrow_t(size_t size, std::align_val_t al, const std::nothrow_t &)
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
                break;
            }
            try
            {
                h();
            }
            catch (...)
            {
                __mem_in_probe_ = false;
                return nullptr;
            }
        }

        if (p)
        {
    #if defined(JE_MALLOC)
            size_t real_sz = sallocx(p, 0);
    #else
            size_t real_sz = malloc_usable_size(p);
    #endif
            memLocalInfo::instance().add(real_sz);
        }

        __mem_in_probe_ = false;
        return p;
    }

    // override operator delete(void *p, std::align_val_t al, const std::nothrow_t &)
    inline void __wrap__ZdlPvSt11align_val_tRKSt9nothrow_t(void *p, std::align_val_t al, const std::nothrow_t &)
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

    // override operator delete[](void *p, std::align_val_t al, const std::nothrow_t &)
    inline void __wrap__ZdaPvSt11align_val_tRKSt9nothrow_t(void *p, std::align_val_t al, const std::nothrow_t &)
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

    // override operator delete(void *p, size_t sz, std::align_val_t al, const std::nothrow_t &)
    inline void __wrap__ZdlPvmSt11align_val_tRKSt9nothrow_t(void *p, size_t sz, std::align_val_t al,
                                                            const std::nothrow_t &)
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

    // override operator delete[](void *p, size_t sz, std::align_val_t al, const std::nothrow_t &)
    inline void __wrap__ZdaPvmSt11align_val_tRKSt9nothrow_t(void *p, size_t sz, std::align_val_t al,
                                                            const std::nothrow_t &)
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

    static std::vector<void *> mmProbeOverrideFunc = {
#if defined(__CPP_STD_98)
        (void *) &__wrap_malloc,
        (void *) &__wrap_calloc,
        (void *) &__wrap_realloc,
        (void *) &__wrap_free,
        (void *) &__wrap__Znwm,
        (void *) &__wrap__Znam,
        (void *) &__wrap__ZdlPv,
        (void *) &__wrap__ZdaPv,
        (void *) &__wrap__ZnwmRKSt9nothrow_t,
        (void *) &__wrap__ZnamRKSt9nothrow_t,
        (void *) &__wrap__ZdlPvRKSt9nothrow_t,
        (void *) &__wrap__ZdaPvRKSt9nothrow_t,
    #if defined(DEFAULT_MALLOC) && defined(__USE_SYS_WRAP)
        // sys call
        (void *) &__wrap_sbrk,
        (void *) &__wrap_brk,
        (void *) &__wrap_mmap,
        (void *) &__wrap_munmap,
        (void *) &__wrap_mremap,
    #endif
        // glibc functions
        (void *) &__wrap_valloc,
        (void *) &__wrap_pvalloc,
        (void *) &__wrap_memalign,
        (void *) &__wrap_posix_memalign,
        (void *) &__wrap_reallocarray,
        // #endif
        // #if defined(__CPP_STD_14)
        (void *) &__wrap__ZdlPvm,
        (void *) &__wrap__ZdaPvm,
#endif
#if defined(__CPP_STD_17)
        (void *) &__wrap_aligned_alloc,
        (void *) &__wrap__ZnwmSt11align_val_t,
        (void *) &__wrap__ZnamSt11align_val_t,
        (void *) &__wrap__ZdlPvSt11align_val_t,
        (void *) &__wrap__ZdaPvSt11align_val_t,
        (void *) &__wrap__ZdlPvmSt11align_val_t,
        (void *) &__wrap__ZdaPvmSt11align_val_t,
        (void *) &__wrap__ZdlPvmRKSt9nothrow_t,
        (void *) &__wrap__ZdaPvmRKSt9nothrow_t,
        (void *) &__wrap__ZnwmSt11align_val_tRKSt9nothrow_t,
        (void *) &__wrap__ZnamSt11align_val_tRKSt9nothrow_t,
        (void *) &__wrap__ZdlPvSt11align_val_tRKSt9nothrow_t,
        (void *) &__wrap__ZdaPvSt11align_val_tRKSt9nothrow_t,
        (void *) &__wrap__ZdlPvmSt11align_val_tRKSt9nothrow_t,
        (void *) &__wrap__ZdaPvmSt11align_val_tRKSt9nothrow_t,
#endif
    };
};
} // namespace __MERECORDER__

#endif
