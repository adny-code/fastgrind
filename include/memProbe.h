/**
 * @file memProbe.h
 * @brief Lightweight in-process memory allocation probe and callstack aggregator.
 *
 * This header provides a set of instrumentation utilities that intercept dynamic
 * memory allocation/deallocation (malloc/new family, and optionally low level
 * syscalls) to build per-thread, per-time-slice statistics. Captured data is
 * organized by synthetic frames (derived from the active function call stack)
 * and can be exported as a hierarchical JSON tree suitable for visualization.
 *
 * Usage pattern:
 *   1. Include this header in one translation unit (typically a .cpp).
 *   2. Link with --wrap symbols (GNU ld) or provide alternative malloc impls as
 *      expected (tcmalloc/jemalloc) so the __wrap_* hooks are used.
 *   3. Annotate functions of interest with MEM_PROBE macro (or rely on global
 *      interception) to push/pop symbolic stack entries.
 *   4. At process end (static destruction) memGlobalInfo automatically dumps
 *      human readable and JSON formatted results (merecorder.json).
 *
 * Thread safety: Per-thread accumulation is stored in thread local structures
 * and periodically merged into a global, mutex-protected container on thread
 * teardown (TLS dtor) or on demand. Allocation hooks avoid recursion using a
 * thread local guard flag.
 *
 * Limitations:
 *  - When a block of memory is allocated and released in different function stack
 *    frames, it will be recorded truthfully, resulting in the memory allocated and
 *    released in those function stack frames being mismatched.
 *  - Export happens at destruction of memGlobalInfo singleton; call dump()
 *    earlier if needed.
 *  - Export JSON file overwrites previous content.
 *
 * @copyright
 * See LICENSE for details.
 */
#ifndef MEM_PROBE_H
#define MEM_PROBE_H

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <assert.h>
#include <malloc.h>
#include <sys/syscall.h>
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

/** @def __MEM_MAX_STACK_DEPTH
 *  @brief Maximum depth of the logical probe call stack that will be captured.
 */
#define __MEM_MAX_STACK_DEPTH 64

/** @def __MEM_SAMPLE_INTERVAL_MS
 *  @brief Sampling granularity (milliseconds) for the internal timer tick.
 */
#define __MEM_SAMPLE_INTERVAL_MS 100

/** @def __MEM_PROBE_STATUS
 *  @brief Global enable switch (set to 0 at compile time to disable probing at runtime with minimal overhead).
 */
#define __MEM_PROBE_STATUS 1

// #define __MEM_DEBUG_INFO 1

/** @brief Output filename for exported JSON statistics. */
constexpr const char *__MEM_PATH_JSON_RESULT = "merecorder.json";

#define MEM_NO_INSTRUMENT __attribute__((no_instrument_function))

#if defined(MERECORDER_INSTRUMENT)
    #include <cxxabi.h>
    #include <dlfcn.h>
    #include <string.h>

MEM_NO_INSTRUMENT static const char *demangleFunc(const char *mangled)
{
    if (!mangled)
        return mangled;
    static std::mutex lk;
    static std::unordered_map<const char *, const char *> cache;
    {
        std::lock_guard<std::mutex> g(lk);
        auto it = cache.find(mangled);
        if (it != cache.end())
            return it->second;
    }
    int status = 0;
    char *tmp = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
    const char *ret = (status == 0 && tmp) ? strdup(tmp) : mangled;
    free(tmp);
    {
        std::lock_guard<std::mutex> g(lk);
        cache[mangled] = ret;
    }
    return ret;
}
#endif

template <typename... Args> static std::string memFormat(const char *fstr, Args... args) MEM_NO_INSTRUMENT;
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
 * @struct memFrame
 * @brief Aggregated allocation statistics for a (threadId, frameId, tick) tuple.
 *
 * A memFrame collects the total allocated and freed bytes observed while the
 * logical frame (identified by its synthetic frameId derived from the call
 * stack) was active during a specific timer tick.
 */
struct memFrame
{
    /**
     * @brief Empty constructor for memFrame.
     *
     */
    MEM_NO_INSTRUMENT memFrame() : mallocBytes(0), freeBytes(0), funcId(0), frameId(0)
    {
    }

    /**
     * @brief Construct a new mem Frame object
     */
    MEM_NO_INSTRUMENT memFrame(size_t mallocBytes, size_t freeBytes, size_t funcId, size_t frameId)
        : mallocBytes(mallocBytes), freeBytes(freeBytes), funcId(funcId), frameId(frameId)
    {
    }

    MEM_NO_INSTRUMENT ~memFrame()
    {
    }

    MEM_NO_INSTRUMENT memFrame(const memFrame &o)
        : mallocBytes(o.mallocBytes), freeBytes(o.freeBytes), funcId(o.funcId), frameId(o.frameId)
    {
    }

    MEM_NO_INSTRUMENT memFrame(memFrame &&o) noexcept
        : mallocBytes(o.mallocBytes), freeBytes(o.freeBytes), funcId(o.funcId), frameId(o.frameId)
    {
    }

    MEM_NO_INSTRUMENT memFrame &operator=(const memFrame &o)
    {
        if (this != &o)
        {
            mallocBytes = o.mallocBytes;
            freeBytes = o.freeBytes;
            funcId = o.funcId;
            frameId = o.frameId;
        }
        return *this;
    }

    MEM_NO_INSTRUMENT memFrame &operator=(memFrame &&o) noexcept
    {
        if (this != &o)
        {
            mallocBytes = o.mallocBytes;
            freeBytes = o.freeBytes;
            funcId = o.funcId;
            frameId = o.frameId;
        }
        return *this;
    }

    /**
     * @brief Accumulate another frame's counters into this one.
     * @param other Source counters to add.
     * @return *this
     */
    MEM_NO_INSTRUMENT memFrame &operator+=(const memFrame &other)
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

/**
 * @class memTimer
 * @brief Simple background monotonic tick generator.
 *
 * Spawns a thread that sleeps for __MEM_SAMPLE_INTERVAL_MS each loop and
 * increments an atomic tick counter. Used to discretize time for memory
 * sampling windows without relying on system signals.
 */
class memTimer
{
  public:
    /** @brief Start timer thread immediately upon construction. */
    MEM_NO_INSTRUMENT memTimer() : _tick(0)
    {
        _thread = new std::thread(std::bind(&memTimer::fakeTimer, this));
    }

    /** @brief Stops timer thread on destruction. */
    MEM_NO_INSTRUMENT ~memTimer()
    {
        stop();
    };

    /** @brief Explicitly stop the timer worker thread (idempotent). */
    MEM_NO_INSTRUMENT void stop()
    {
        _exit = true;
        if (_thread)
        {
            _thread->join();
            delete _thread;
            _thread = nullptr;
        }
    }

    /** @return Elapsed time in milliseconds in discrete ticks. */
    MEM_NO_INSTRUMENT size_t time() const
    {
        return _tick;
    }

  protected:
    /** @brief Worker loop incrementing the tick counter until shutdown. */
    MEM_NO_INSTRUMENT void fakeTimer()
    {
        while (!_exit)
        {
            usleep(1000 * __MEM_SAMPLE_INTERVAL_MS);
            _tick += __MEM_SAMPLE_INTERVAL_MS;
        };
    }

  protected:
    bool _exit = false;
    std::thread *_thread = nullptr;
    std::atomic<size_t> _tick;
};

/**
 * @class memNode
 * @brief Node in a hierarchical call tree accumulating memory statistics.
 *
 * Each node corresponds to a single function symbol (name pointer is used as key)
 * and contains aggregated malloc / free byte counts plus child nodes for deeper
 * call stack levels.
 */
class memNode
{
  public:
    MEM_NO_INSTRUMENT memNode()
    {
    }

    /** @brief Construct named node. */
    MEM_NO_INSTRUMENT memNode(const char *name) : _name(name)
    {
    }

    MEM_NO_INSTRUMENT ~memNode()
    {
    }

    MEM_NO_INSTRUMENT memNode(const memNode &o)
        : _name(o._name), _childs(o._childs), _mallocBytes(o._mallocBytes), _freeBytes(o._freeBytes)
    {
    }

    MEM_NO_INSTRUMENT memNode(memNode &&o) noexcept
        : _name(o._name), _childs(std::move(o._childs)), _mallocBytes(o._mallocBytes), _freeBytes(o._freeBytes)
    {
    }

    MEM_NO_INSTRUMENT memNode &operator=(const memNode &o)
    {
        if (this != &o)
        {
            _name = o._name;
            _childs = o._childs;
            _mallocBytes = o._mallocBytes;
            _freeBytes = o._freeBytes;
        }
        return *this;
    }

    MEM_NO_INSTRUMENT memNode &operator=(memNode &&o) noexcept
    {
        if (this != &o)
        {
            _name = o._name;
            _childs = std::move(o._childs);
            _mallocBytes = o._mallocBytes;
            _freeBytes = o._freeBytes;
        }
        return *this;
    }

    /**
     * @brief Insert a memFrame into the tree along the provided call stack.
     * @param callstack Null-terminated array of function name pointers.
     * @param frame Frame data to merge.
     * @param depth Current depth while recursing (internal use).
     */
    MEM_NO_INSTRUMENT void add(const std::array<const char *, __MEM_MAX_STACK_DEPTH> &callstack, const memFrame &frame,
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

    /** @return Mutable reference to child node map keyed by function name pointer. */
    MEM_NO_INSTRUMENT std::map<const char *, memNode> &childs()
    {
        return _childs;
    }

    /**
     * @brief Produce a human-readable multi-line string of the subtree.
     * @param indent Current indentation level (spaces are 4 * indent).
     */
    MEM_NO_INSTRUMENT std::string str(unsigned indent = 0) const
    {
        std::string s =
            std::string(indent * 4, ' ') + memFormat("%s malloc %'ld free %'ld", _name, _mallocBytes, _freeBytes);

        for (auto it = _childs.begin(); it != _childs.end(); ++it)
        {
            s += std::string("\n") + it->second.str(indent + 1);
        }

        return s;
    }

    /** @brief Serialize subtree to compact JSON (no pretty formatting). */
    MEM_NO_INSTRUMENT std::string json() const
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

    /** @brief Print the formatted tree to stdout. */
    MEM_NO_INSTRUMENT void dump() const
    {
        printf("%s\n", str().c_str());
    }

  protected:
    const char *_name = nullptr;

    //! @note Child map key is raw function name pointer (assumed stable during process lifetime).
    std::map<const char *, memNode> _childs;

    size_t _mallocBytes = 0;
    size_t _freeBytes = 0;
};

class memLocalInfo;
/**
 * @class memGlobalInfo
 * @brief Singleton aggregating memory statistics across all threads.
 *
 * Maintains thread->frameId->tick->memFrame structures and a mapping from
 * frameId to captured call stacks (array of function name pointers). Responsible
 * for dumping textual and JSON reports. Thread-local data merges into this
 * structure under mutex protection.
 */
class memGlobalInfo
{
    friend class memLocalInfo;

  public:
    MEM_NO_INSTRUMENT memGlobalInfo()
    {
        setlocale(LC_ALL, "");
    }

    MEM_NO_INSTRUMENT ~memGlobalInfo()
    {
        callStackTrans();
        dump();
    }

    /** @brief Access singleton instance (lazy constructed). */
    MEM_NO_INSTRUMENT static memGlobalInfo &instance()
    {
        if (!_instance)
        {
            _instance = std::unique_ptr<memGlobalInfo>(new memGlobalInfo);
        }

        return *_instance.get();
    }

    /** @return Current global timer tick (ms). */
    MEM_NO_INSTRUMENT size_t time() const
    {
        return _timer.time();
    }

    /**
     * @brief Dump aggregated statistics and call tree to stdout and export JSON.
     * @note Safe to call multiple times; JSON file is overwritten.
     */
    MEM_NO_INSTRUMENT void dump() const
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

    /** @brief Export hierarchical statistics to JSON file (pretty-printed). */
    MEM_NO_INSTRUMENT void exportJson() const
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

        auto appendIndent = [&]() MEM_NO_INSTRUMENT { pretty.append(indent, ' '); };

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

    /**
     * @brief Reconstruct printable call stack for a frameId (top frame first).
     * @param frameId Synthetic frame identifier.
     * @return Multi-line string (empty if unknown id).
     */
    MEM_NO_INSTRUMENT std::string getCallstack(size_t frameId) const
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
    MEM_NO_INSTRUMENT void callStackTrans()
    {
#if defined(MERECORDER_INSTRUMENT)
        for (auto &kv : _callstacks)
        {
            auto &arr = kv.second;
            for (size_t i = 0; i < __MEM_MAX_STACK_DEPTH; ++i)
            {
                const char *entry = arr[i];
                if (!entry)
                    break;

                Dl_info info;
                if (dladdr((void *) entry, &info) && info.dli_sname && info.dli_saddr == (void *) entry)
                {
                    const char *pretty = demangleFunc(info.dli_sname);
                    arr[i] = pretty;
                }
                else
                {
                    std::string fallback = memFormat("<unresolved@%p>", entry);
                    arr[i] = strdup(fallback.c_str());
                }
            }
        }
#endif
    }

  protected:
    // first map key is threadId, second map key is frameId, third map key is tick.
    // value is memFrame.
    std::map<size_t, std::unordered_map<size_t, std::unordered_map<size_t, memFrame>>> _frames;

    std::map<size_t, std::array<const char *, __MEM_MAX_STACK_DEPTH>> _callstacks;
    std::mutex _lk;

  private:
    static std::unique_ptr<memGlobalInfo> _instance;

    // timer need to init after all other class members.
    memTimer _timer;
};

#if defined(__CPP_STD_17)
inline std::unique_ptr<memGlobalInfo> memGlobalInfo::_instance = nullptr;
#else
std::unique_ptr<memGlobalInfo> memGlobalInfo::_instance = nullptr;
#endif

/**
 * @class memStack
 * @brief Thread-local logical call stack used to synthesize frame identifiers.
 *
 * The stack is explicitly manipulated via memProbe RAII objects (MEM_PROBE macro)
 * rather than relying on platform unwinding. Each push/pop updates a rolling id
 * so that the same textual sequence of function names maps to a deterministic
 * frameId across time slices.
 */
class memStack
{
  public:
    MEM_NO_INSTRUMENT memStack()
    {
        _offset = 0;
        _stackId = 0;
    }

    MEM_NO_INSTRUMENT ~memStack()
    {
    }

    /** @brief Push function name onto stack and update frame id hash. */
    MEM_NO_INSTRUMENT void push(const char *v)
    {
        _stack[_offset++] = v;
        _stackId += size_t(v);
    }

    /** @brief Pop top of stack (must not be empty). */
    MEM_NO_INSTRUMENT const char *pop()
    {
        auto v = _stack[--_offset];
        _stackId -= size_t(v);
        return v;
    }

    /** @return Current stack depth. */
    MEM_NO_INSTRUMENT unsigned depth() const
    {
        return _offset;
    }

    /** @return Top function name (undefined if empty). */
    MEM_NO_INSTRUMENT const char *top() const
    {
        return _stack[_offset - 1];
    }

    /** @return Synthetic frame id composed from top symbol pointer plus cumulative hash. */
    MEM_NO_INSTRUMENT size_t frameId() const
    {
        return size_t(top()) + _stackId;
    }

    /** @return Reference to underlying fixed-size stack array. */
    MEM_NO_INSTRUMENT const std::array<const char *, __MEM_MAX_STACK_DEPTH> &stack() const
    {
        return _stack;
    }

    /** @brief Access thread-local singleton instance. */
    MEM_NO_INSTRUMENT static memStack &instance()
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

/**
 * @class memLocalInfo
 * @brief Per-thread pending statistics prior to merge into global state.
 *
 * Accumulates memFrame objects keyed by frameId and timer tick, along with a
 * copy of each unique call stack encountered. On thread exit (destructor) or
 * explicit merge(), content is transferred to memGlobalInfo.
 */
class memLocalInfo : public std::unordered_map<size_t /*frameId*/, std::unordered_map<size_t /*tick*/, memFrame>>
{
  public:
    MEM_NO_INSTRUMENT memLocalInfo() : std::unordered_map<size_t, std::unordered_map<size_t, memFrame>>()
    {
    }

    MEM_NO_INSTRUMENT ~memLocalInfo()
    {
        merge();
    }

    /** @brief Access thread-local singleton. */
    MEM_NO_INSTRUMENT static memLocalInfo &instance()
    {
        thread_local memLocalInfo __memThreadInfo__;
        return __memThreadInfo__;
    }

    /** @brief Clear local accumulated frames and call stacks. */
    MEM_NO_INSTRUMENT void reset()
    {
        _frames.clear();
        _callstacks.clear();
    }

    /** @brief Merge local thread data into global aggregator then reset. */
    MEM_NO_INSTRUMENT void merge()
    {
        std::lock_guard<std::mutex> lg(memGlobalInfo::instance()._lk);

        memGlobalInfo::instance()._frames[tid] = _frames;

        for (auto it = _callstacks.begin(); it != _callstacks.end(); ++it)
            memGlobalInfo::instance()._callstacks[it->first] = it->second;

        reset();
    }

    /** @brief Record an allocation of sz bytes (real usable size). */
    MEM_NO_INSTRUMENT void add(size_t sz)
    {
        if (_nested || memStack::instance().depth() == 0)
            return;

        ++_nested;
        const auto &stack = memStack::instance();
        getFrame(size_t(stack.top()), stack.frameId(), memGlobalInfo::instance().time()).mallocBytes += sz;
        --_nested;
    }

    /** @brief Record a deallocation of sz bytes (real usable size). */
    MEM_NO_INSTRUMENT void sub(size_t sz)
    {
        if (_nested || memStack::instance().depth() == 0)
            return;

        ++_nested;
        const auto &stack = memStack::instance();
        getFrame(size_t(stack.top()), stack.frameId(), memGlobalInfo::instance().time()).freeBytes += sz;
        --_nested;
    }

  protected:
    /** @brief Retrieve (or create) the memFrame for (frameId,time) and ensure call stack snapshot stored. */
    MEM_NO_INSTRUMENT memFrame &getFrame(size_t funcId, size_t frameId, size_t time)
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

/**
 * @class memProbe
 * @brief RAII helper pushing current function onto logical stack.
 *
 * Construct an instance at function scope to automatically attribute all
 * allocations to that function until destruction (end of scope). Macro
 * MEM_PROBE wraps construction with __PRETTY_FUNCTION__ providing decorated name.
 */
class memProbe
{
  public:
    MEM_NO_INSTRUMENT memProbe(const char *name)
    {
        if (__MEM_PROBE_STATUS)
            memStack::instance().push(name);
    }
    MEM_NO_INSTRUMENT ~memProbe()
    {
        if (__MEM_PROBE_STATUS)
            memStack::instance().pop();
    }
};

/**
 * @def MEM_PROBE
 * @brief Annotate a scope to participate in logical memory call stack.
 * @details Expands to creation of a memProbe with the current pretty function symbol.
 */
#define MEM_PROBE memProbe __probe__(__PRETTY_FUNCTION__);

extern "C"
{
#if defined(MERECORDER_INSTRUMENT)
    MEM_NO_INSTRUMENT void __cyg_profile_func_enter(void *this_fn, void *call_site)
    {
        (void) call_site;
        static thread_local bool __mem_in_enter = false;
        if (__mem_in_enter)
            return;
        __mem_in_enter = true;

    #if defined(__MEM_DEBUG_INFO)
        Dl_info info;
        if (dladdr(this_fn, &info) && info.dli_sname)
        {
            const char *pretty = demangleFunc(info.dli_sname);
            printf("Entered function: %s\n", pretty);
        }
        else
        {
            printf("Entered unknown function at %p\n", this_fn);
        }
    #endif // __MEM_DEBUG_INFO
        memStack::instance().push((const char *) this_fn);

        __mem_in_enter = false;
    }

    MEM_NO_INSTRUMENT void __cyg_profile_func_exit(void *this_fn, void *call_site)
    {
        (void) call_site;
        (void) this_fn;
        static thread_local bool __mem_in_exit = false;
        if (__mem_in_exit)
            return;
        __mem_in_exit = true;

    #if defined(__MEM_DEBUG_INFO)
        Dl_info info;
        if (dladdr(this_fn, &info) && info.dli_sname)
        {
            const char *pretty = demangleFunc(info.dli_sname);
            printf("Exited function: %s\n", pretty);
        }
        else
        {
            printf("Exited unknown function at %p\n", this_fn);
        }
    #endif // __MEM_DEBUG_INFO
        memStack::instance().pop();

        __mem_in_exit = false;
    }
#endif // MERECORDER_INSTRUMENT

#if defined(MERECORDER_TC_MALLOC)
    #include <gperftools/tcmalloc.h>
#elif defined(MERECORDER_JE_MALLOC)
    #include <jemalloc/jemalloc.h>
    extern void *je_sdallocx_default(void *ptr, size_t size, int flags);
    extern void *je_malloc_default(size_t size);
    extern void je_free_default(void *ptr);
#else
    #define DEFAULT_MALLOC 1
    // #define __USE_SYS_WRAP 1
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
    MEM_NO_INSTRUMENT inline void *__wrap_malloc(size_t sz)
    {
        if (sz == 0)
            sz = 1;

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            return tc_malloc(sz);
    #elif defined(MERECORDER_JE_MALLOC)
            return je_malloc_default(sz);
    #else
            return __real_malloc(sz);
    #endif
        }

        __mem_in_probe_ = true;

        void *p =
    #if defined(MERECORDER_TC_MALLOC)
            tc_malloc(sz);
    #elif defined(MERECORDER_JE_MALLOC)
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

    MEM_NO_INSTRUMENT inline void *__wrap_calloc(size_t nmemb, size_t size)
    {
        if (nmemb == 0 || size == 0)
        {
            nmemb = 1;
            size = 1;
        }

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            return tc_calloc(nmemb, size);
    #elif defined(MERECORDER_JE_MALLOC)
            return calloc(nmemb, size);
    #else
            return __real_calloc(nmemb, size);
    #endif
        }

        __mem_in_probe_ = true;

        void *p =
    #if defined(MERECORDER_TC_MALLOC)
            tc_calloc(nmemb, size);
    #elif defined(MERECORDER_JE_MALLOC)
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

    MEM_NO_INSTRUMENT inline void *__wrap_realloc(void *ptr, size_t size)
    {
        if (size == 0)
            size = 1;

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            return tc_realloc(ptr, size);
    #elif defined(MERECORDER_JE_MALLOC)
            return realloc(ptr, size);
    #else
            return __real_realloc(ptr, size);
    #endif
        }

        __mem_in_probe_ = true;

        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;

        void *p =
    #if defined(MERECORDER_TC_MALLOC)
            tc_realloc(ptr, size);
    #elif defined(MERECORDER_JE_MALLOC)
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

    MEM_NO_INSTRUMENT inline void __wrap_free(void *p)
    {
        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
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

    #if defined(MERECORDER_TC_MALLOC)
        tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
        je_free_default(p);
    #else
        __real_free(p);
    #endif
    }

    // override operator new
    MEM_NO_INSTRUMENT inline void *__wrap__Znwm(size_t sz)
    {
        if (sz == 0)
            sz = 1;

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            void *p = tc_malloc(sz);
    #elif defined(MERECORDER_JE_MALLOC)
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
    #if defined(MERECORDER_TC_MALLOC)
            p = tc_malloc(sz);
    #elif defined(MERECORDER_JE_MALLOC)
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
        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().add(real_sz);
        }

        __mem_in_probe_ = false;
        return p;
    }

    // override operator new[]
    MEM_NO_INSTRUMENT inline void *__wrap__Znam(size_t sz)
    {
        if (sz == 0)
            sz = 1;

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            void *p = tc_malloc(sz);
    #elif defined(MERECORDER_JE_MALLOC)
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
    #if defined(MERECORDER_TC_MALLOC)
            p = tc_malloc(sz);
    #elif defined(MERECORDER_JE_MALLOC)
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
        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().add(real_sz);
        }

        __mem_in_probe_ = false;
        return p;
    }

    // override operator delete
    MEM_NO_INSTRUMENT inline void __wrap__ZdlPv(void *p)
    {
        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
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

    #if defined(MERECORDER_TC_MALLOC)
        tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
        je_free_default(p);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete[]
    MEM_NO_INSTRUMENT inline void __wrap__ZdaPv(void *p)
    {
        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
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

    #if defined(MERECORDER_TC_MALLOC)
        tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
        je_free_default(p);
    #else
        __real_free(p);
    #endif
    }

    // override operator new with nothrow
    MEM_NO_INSTRUMENT inline void *__wrap__ZnwmRKSt9nothrow_t(size_t sz, const std::nothrow_t &)
    {
        if (sz == 0)
            sz = 1;

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            void *p = tc_malloc(sz);
    #elif defined(MERECORDER_JE_MALLOC)
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
    #if defined(MERECORDER_TC_MALLOC)
            p = tc_malloc(sz);
    #elif defined(MERECORDER_JE_MALLOC)
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
    MEM_NO_INSTRUMENT inline void *__wrap__ZnamRKSt9nothrow_t(size_t sz, const std::nothrow_t &)
    {
        if (sz == 0)
            sz = 1;

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            void *p = tc_malloc(sz);
    #elif defined(MERECORDER_JE_MALLOC)
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
    #if defined(MERECORDER_TC_MALLOC)
            p = tc_malloc(sz);
    #elif defined(MERECORDER_JE_MALLOC)
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
    MEM_NO_INSTRUMENT inline void __wrap__ZdlPvRKSt9nothrow_t(void *p, const std::nothrow_t &)
    {
        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
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

    #if defined(MERECORDER_TC_MALLOC)
        tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
        je_free_default(p);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete[] with nothrow
    MEM_NO_INSTRUMENT inline void __wrap__ZdaPvRKSt9nothrow_t(void *p, const std::nothrow_t &)
    {
        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
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

    #if defined(MERECORDER_TC_MALLOC)
        tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
        je_free_default(p);
    #else
        __real_free(p);
    #endif
    }

    #if defined(DEFAULT_MALLOC) && defined(__USE_SYS_WRAP)
    // sys call
    MEM_NO_INSTRUMENT inline void *__wrap_sbrk(intptr_t increment)
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
    MEM_NO_INSTRUMENT inline int __wrap_brk(void *addr)
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
    MEM_NO_INSTRUMENT inline void *__wrap_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
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
    MEM_NO_INSTRUMENT inline int __wrap_munmap(void *addr, size_t length)
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
    MEM_NO_INSTRUMENT inline void *__wrap_mremap(void *old_address, size_t old_size, size_t new_size, int flags, ...)
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
    MEM_NO_INSTRUMENT inline void *__wrap_valloc(size_t size)
    {
        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            return tc_valloc(size);
    #elif defined(MERECORDER_JE_MALLOC)
            return valloc(size);
    #else
            return __real_valloc(size);
    #endif
        }

        __mem_in_probe_ = true;

        void *p =
    #if defined(MERECORDER_TC_MALLOC)
            tc_valloc(size);
    #elif defined(MERECORDER_JE_MALLOC)
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
    MEM_NO_INSTRUMENT inline void *__wrap_pvalloc(size_t size)
    {
        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            return tc_pvalloc(size);
    #elif defined(MERECORDER_JE_MALLOC)
            return pvalloc(size);
    #else
            return __real_pvalloc(size);
    #endif
        }

        __mem_in_probe_ = true;

        void *p =
    #if defined(MERECORDER_TC_MALLOC)
            tc_pvalloc(size);
    #elif defined(MERECORDER_JE_MALLOC)
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
    MEM_NO_INSTRUMENT inline void *__wrap_memalign(size_t alignment, size_t size)
    {
        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            return tc_memalign(alignment, size);
    #elif defined(MERECORDER_JE_MALLOC)
            return memalign(alignment, size);
    #else
            return __real_memalign(alignment, size);
    #endif
        }

        __mem_in_probe_ = true;

        void *p =
    #if defined(MERECORDER_TC_MALLOC)
            tc_memalign(alignment, size);
    #elif defined(MERECORDER_JE_MALLOC)
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
    MEM_NO_INSTRUMENT inline int __wrap_posix_memalign(void **memptr, size_t alignment, size_t size)
    {
        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            return tc_posix_memalign(memptr, alignment, size);
    #elif defined(MERECORDER_JE_MALLOC)
            return posix_memalign(memptr, alignment, size);
    #else
            return __real_posix_memalign(memptr, alignment, size);
    #endif
        }

        __mem_in_probe_ = true;

        int ret =
    #if defined(MERECORDER_TC_MALLOC)
            tc_posix_memalign(memptr, alignment, size);
    #elif defined(MERECORDER_JE_MALLOC)
            posix_memalign(memptr, alignment, size);
    #else
            __real_posix_memalign(memptr, alignment, size);
    #endif

        if (ret == 0 && memptr && *memptr)
        {
            size_t real_sz = malloc_usable_size(*memptr);
            memLocalInfo::instance().add(real_sz);
        }

        __mem_in_probe_ = false;
        return ret;
    }

    // glibc function
    MEM_NO_INSTRUMENT inline void *__wrap_reallocarray(void *ptr, size_t nmemb, size_t size)
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
    #if defined(MERECORDER_TC_MALLOC)
            return tc_realloc(ptr, total);
    #elif defined(MERECORDER_JE_MALLOC)
            return realloc(ptr, total);
    #else
            return __real_realloc(ptr, total);
    #endif
        }

        __mem_in_probe_ = true;

        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;

        void *p =
    #if defined(MERECORDER_TC_MALLOC)
            tc_realloc(ptr, total);
    #elif defined(MERECORDER_JE_MALLOC)
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
    MEM_NO_INSTRUMENT inline void __wrap__ZdlPvm(void *p, size_t sz)
    {
        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(MERECORDER_JE_MALLOC)
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

    #if defined(MERECORDER_TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(MERECORDER_JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete[](void*, std::size_t)
    MEM_NO_INSTRUMENT inline void __wrap__ZdaPvm(void *p, size_t sz)
    {
        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(MERECORDER_JE_MALLOC)
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

    #if defined(MERECORDER_TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(MERECORDER_JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
    }
#endif

#if defined(__CPP_STD_17)
    MEM_NO_INSTRUMENT inline void *__wrap_aligned_alloc(size_t alignment, size_t size)
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
    #if defined(MERECORDER_TC_MALLOC)
            return tc_memalign(alignment, size);
    #elif defined(MERECORDER_JE_MALLOC)
            return aligned_alloc(alignment, size);
    #else
            return __real_aligned_alloc(alignment, size);
    #endif
        }

        __mem_in_probe_ = true;

        void *p = nullptr;
    #if defined(MERECORDER_TC_MALLOC)
        p = tc_memalign(alignment, size);
    #elif defined(MERECORDER_JE_MALLOC)
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
    MEM_NO_INSTRUMENT inline void *__wrap__ZnwmSt11align_val_t(size_t size, std::align_val_t al)
    {
        if (size == 0)
            size = 1;
        size_t alignment = static_cast<size_t>(al);

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            void *p = tc_memalign(alignment, size);
    #elif defined(MERECORDER_JE_MALLOC)
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
    #if defined(MERECORDER_TC_MALLOC)
            p = tc_memalign(alignment, size);
    #elif defined(MERECORDER_JE_MALLOC)
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
        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().add(real_sz);
        }

        __mem_in_probe_ = false;
        return p;
    }

    // override operator new[](std::size_t size, std::align_val_t alignment)
    MEM_NO_INSTRUMENT inline void *__wrap__ZnamSt11align_val_t(size_t size, std::align_val_t al)
    {
        if (size == 0)
            size = 1;
        size_t alignment = static_cast<size_t>(al);

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            void *p = tc_memalign(alignment, size);
    #elif defined(MERECORDER_JE_MALLOC)
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
    #if defined(MERECORDER_TC_MALLOC)
            p = tc_memalign(alignment, size);
    #elif defined(MERECORDER_JE_MALLOC)
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
        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().add(real_sz);
        }

        __mem_in_probe_ = false;
        return p;
    }

    // override operator delete(void *p, std::align_val_t al)
    MEM_NO_INSTRUMENT inline void __wrap__ZdlPvSt11align_val_t(void *p, std::align_val_t al)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
            dallocx(p, 0);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().sub(real_sz);
        }
        __mem_in_probe_ = false;

    #if defined(MERECORDER_TC_MALLOC)
        tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
        dallocx(p, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete[](void *p, std::align_val_t al)
    MEM_NO_INSTRUMENT inline void __wrap__ZdaPvSt11align_val_t(void *p, std::align_val_t al)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
            dallocx(p, 0);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().sub(real_sz);
        }
        __mem_in_probe_ = false;

    #if defined(MERECORDER_TC_MALLOC)
        tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
        dallocx(p, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete(void *p, size_t sz, std::align_val_t al)
    MEM_NO_INSTRUMENT inline void __wrap__ZdlPvmSt11align_val_t(void *p, size_t sz, std::align_val_t al)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(MERECORDER_JE_MALLOC)
            je_sdallocx_default(p, sz, 0);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().sub(real_sz);
        }
        __mem_in_probe_ = false;

    #if defined(MERECORDER_TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(MERECORDER_JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete[](void *p, size_t sz, std::align_val_t al)
    MEM_NO_INSTRUMENT inline void __wrap__ZdaPvmSt11align_val_t(void *p, size_t sz, std::align_val_t al)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(MERECORDER_JE_MALLOC)
            je_sdallocx_default(p, sz, 0);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().sub(real_sz);
        }
        __mem_in_probe_ = false;

    #if defined(MERECORDER_TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(MERECORDER_JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete(void *p, size_t sz, const std::nothrow_t &)
    MEM_NO_INSTRUMENT inline void __wrap__ZdlPvmRKSt9nothrow_t(void *p, size_t sz, const std::nothrow_t &)
    {
        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(MERECORDER_JE_MALLOC)
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

    #if defined(MERECORDER_TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(MERECORDER_JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete[](void *p, size_t sz, const std::nothrow_t &)
    MEM_NO_INSTRUMENT inline void __wrap__ZdaPvmRKSt9nothrow_t(void *p, size_t sz, const std::nothrow_t &)
    {
        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(MERECORDER_JE_MALLOC)
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

    #if defined(MERECORDER_TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(MERECORDER_JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator new(std::size_t size, std::align_val_t al, const std::nothrow_t &)
    MEM_NO_INSTRUMENT inline void *__wrap__ZnwmSt11align_val_tRKSt9nothrow_t(size_t size, std::align_val_t al,
                                                                             const std::nothrow_t &)
    {
        if (size == 0)
            size = 1;
        size_t alignment = static_cast<size_t>(al);

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            void *p = tc_memalign(alignment, size);
    #elif defined(MERECORDER_JE_MALLOC)
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
    #if defined(MERECORDER_TC_MALLOC)
            p = tc_memalign(alignment, size);
    #elif defined(MERECORDER_JE_MALLOC)
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
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().add(real_sz);
        }

        __mem_in_probe_ = false;
        return p;
    }

    // override operator new[](std::size_t size, std::align_val_t al, const std::nothrow_t &)
    MEM_NO_INSTRUMENT inline void *__wrap__ZnamSt11align_val_tRKSt9nothrow_t(size_t size, std::align_val_t al,
                                                                             const std::nothrow_t &)
    {
        if (size == 0)
            size = 1;
        size_t alignment = static_cast<size_t>(al);

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            void *p = tc_memalign(alignment, size);
    #elif defined(MERECORDER_JE_MALLOC)
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
    #if defined(MERECORDER_TC_MALLOC)
            p = tc_memalign(alignment, size);
    #elif defined(MERECORDER_JE_MALLOC)
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
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().add(real_sz);
        }

        __mem_in_probe_ = false;
        return p;
    }

    // override operator delete(void *p, std::align_val_t al, const std::nothrow_t &)
    MEM_NO_INSTRUMENT inline void __wrap__ZdlPvSt11align_val_tRKSt9nothrow_t(void *p, std::align_val_t al,
                                                                             const std::nothrow_t &)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
            dallocx(p, 0);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().sub(real_sz);
        }
        __mem_in_probe_ = false;

    #if defined(MERECORDER_TC_MALLOC)
        tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
        dallocx(p, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete[](void *p, std::align_val_t al, const std::nothrow_t &)
    MEM_NO_INSTRUMENT inline void __wrap__ZdaPvSt11align_val_tRKSt9nothrow_t(void *p, std::align_val_t al,
                                                                             const std::nothrow_t &)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
            dallocx(p, 0);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().sub(real_sz);
        }
        __mem_in_probe_ = false;

    #if defined(MERECORDER_TC_MALLOC)
        tc_free(p);
    #elif defined(MERECORDER_JE_MALLOC)
        dallocx(p, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete(void *p, size_t sz, std::align_val_t al, const std::nothrow_t &)
    MEM_NO_INSTRUMENT inline void __wrap__ZdlPvmSt11align_val_tRKSt9nothrow_t(void *p, size_t sz, std::align_val_t al,
                                                                              const std::nothrow_t &)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(MERECORDER_JE_MALLOC)
            je_sdallocx_default(p, sz, 0);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().sub(real_sz);
        }
        __mem_in_probe_ = false;

    #if defined(MERECORDER_TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(MERECORDER_JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete[](void *p, size_t sz, std::align_val_t al, const std::nothrow_t &)
    MEM_NO_INSTRUMENT inline void __wrap__ZdaPvmSt11align_val_tRKSt9nothrow_t(void *p, size_t sz, std::align_val_t al,
                                                                              const std::nothrow_t &)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(MERECORDER_TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(MERECORDER_JE_MALLOC)
            je_sdallocx_default(p, sz, 0);
    #else
            __real_free(p);
    #endif
            return;
        }

        __mem_in_probe_ = true;
        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().sub(real_sz);
        }
        __mem_in_probe_ = false;

    #if defined(MERECORDER_TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(MERECORDER_JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
    }

#endif

    /**
     * @brief Collection of function pointers to all wrapper hooks provided.
     * @details Useful for tools wanting to verify symbol interposition or to iterate over wrappers.
     */
    static const void *mmProbeOverrideFunc[] = {
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
