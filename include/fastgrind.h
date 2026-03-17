/**
 * @file fastgrind.h
 * @brief Lightweight in-process memory allocation probe and callstack aggregator.
 *
 * This header provides a set of instrumentation utilities that intercept dynamic
 * memory allocation/deallocation (malloc/new family, and optionally low level
 * syscalls) to build per-thread, per-time-slice statistics. Captured data is
 * organized by synthetic frames (derived from the active function call stack)
 * and can be exported two files (fastgrind.fgb/fastgrind.text) for analysis.
 * @details
 * Usage pattern:
 *   1. Include this header in one translation unit (typically a .cpp).
 *   2. Link with --wrap symbols (GNU ld) or provide alternative malloc impls as
 *      expected (tcmalloc/jemalloc) so the __wrap_* hooks are used.
 *   3. Annotate functions of interest with FAST_GRIND macro (or rely on global
 *      interception) to push/pop symbolic stack entries.
 *   4. At process end (static destruction) memGlobalInfo automatically dumps
 *      human readable results (fastgrind.fgb/fastgrind.text).
 *
 * @note Thread safety: Per-thread accumulation is stored in thread local structures
 * and periodically merged into a global, mutex-protected container on thread
 * teardown (TLS dtor) or on demand. Allocation hooks avoid recursion using a
 * thread local guard flag.
 *
 * @warning Notes:
 *  - When a block of memory is allocated and released in different function stack
 *    frames, it will be recorded truthfully, resulting in the memory allocated and
 *    released in those function stack frames being mismatched.
 *  - Weak support for template metaprogramming and anonymous functions in summary report.
 *  - Export files overwrite previous content.
 *
 * @author
 * For any advice or questions, please contact us:
 *  - email: zfzmalloc@gmail.com
 *  - github: https://github.com/adny-code/fastgrind
 *
 * @copyright
 * See LICENSE for details.
 */
#ifndef FAST_GRIND_H
#define FAST_GRIND_H

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <assert.h>
#include <errno.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(FASTGRIND_INSTRUMENT)
    #include <cxxabi.h>
    #include <dlfcn.h>
#endif

#if defined(FASTGRIND_TC_MALLOC)
    #include <gperftools/tcmalloc.h>
#elif defined(FASTGRIND_JE_MALLOC)
    #include <jemalloc/jemalloc.h>
#endif

namespace __FASTGRIND__
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
#ifndef __MEM_MAX_STACK_DEPTH
#define __MEM_MAX_STACK_DEPTH 64
#else
#endif

/** @def __MEM_SAMPLE_INTERVAL_MS
 *  @brief Sampling granularity (milliseconds) for the internal timer tick.
 */
#define __MEM_SAMPLE_INTERVAL_MS 500

/** @def __FAST_GRIND_STATUS
 *  @brief Global enable switch (set to 0 at compile time to disable probing at runtime with minimal overhead).
 */
#define __FAST_GRIND_STATUS 1

// #define __MEM_DEBUG_INFO 1

/** @brief Output filename for exported binary statistics. */
constexpr const char *__MEM_PATH_BINARY_RESULT = "fastgrind.fgb";
constexpr const char *__MEM_PATH_TEXT_RESULT = "fastgrind.text";

#define MEM_NO_INSTRUMENT __attribute__((no_instrument_function))

#define MEM_INLINE_USED __attribute__((used))

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

enum memFgbSectionType : uint32_t
{
    MEM_FGB_SECTION_FUNCTION_TABLE = 1,
    MEM_FGB_SECTION_STACK_TABLE = 2,
    MEM_FGB_SECTION_THREAD_TABLE = 3,
    MEM_FGB_SECTION_THREAD_FUNCTION_DIRECTORY = 4,
    MEM_FGB_SECTION_FUNCTION_STACK_POSTINGS = 5,
    MEM_FGB_SECTION_TICK_DATA = 6,
    MEM_FGB_SECTION_TICK_DIRECTORY = 7,
};

enum memFgbSectionFlags : uint32_t
{
    MEM_FGB_SECTION_FLAG_NONE = 0,
    MEM_FGB_SECTION_FLAG_CRC32 = 1u << 0,
};

enum memFgbStackFlags : uint16_t
{
    MEM_FGB_STACK_FLAG_NONE = 0,
    MEM_FGB_STACK_FLAG_LEAF = 1u << 0,
};

struct memFgbSectionInfo
{
    uint32_t type = 0;
    uint32_t flags = MEM_FGB_SECTION_FLAG_NONE;
    uint64_t offset = 0;
    uint64_t length = 0;
    uint32_t crc32 = 0;
};

MEM_NO_INSTRUMENT static void memAppendBytes(std::vector<unsigned char> &out, const void *data, size_t size)
{
    if (!data || size == 0)
        return;

    const auto *bytes = static_cast<const unsigned char *>(data);
    out.insert(out.end(), bytes, bytes + size);
}

MEM_NO_INSTRUMENT static void memAppendU16LE(std::vector<unsigned char> &out, uint16_t value)
{
    out.push_back(static_cast<unsigned char>(value & 0xffu));
    out.push_back(static_cast<unsigned char>((value >> 8) & 0xffu));
}

MEM_NO_INSTRUMENT static void memAppendU32LE(std::vector<unsigned char> &out, uint32_t value)
{
    out.push_back(static_cast<unsigned char>(value & 0xffu));
    out.push_back(static_cast<unsigned char>((value >> 8) & 0xffu));
    out.push_back(static_cast<unsigned char>((value >> 16) & 0xffu));
    out.push_back(static_cast<unsigned char>((value >> 24) & 0xffu));
}

MEM_NO_INSTRUMENT static void memAppendU64LE(std::vector<unsigned char> &out, uint64_t value)
{
    for (unsigned shift = 0; shift < 64; shift += 8)
        out.push_back(static_cast<unsigned char>((value >> shift) & 0xffu));
}

MEM_NO_INSTRUMENT static uint32_t memCrc32Update(uint32_t crc, const unsigned char *data, size_t size)
{
    static uint32_t table[256] = {0};
    static bool initialized = false;

    if (!initialized)
    {
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t value = i;
            for (unsigned bit = 0; bit < 8; ++bit)
                value = (value & 1u) ? (0xedb88320u ^ (value >> 1)) : (value >> 1);

            table[i] = value;
        }
        initialized = true;
    }

    crc = ~crc;
    for (size_t i = 0; i < size; ++i)
        crc = table[(crc ^ data[i]) & 0xffu] ^ (crc >> 8);

    return ~crc;
}

MEM_NO_INSTRUMENT static uint32_t memCrc32(const std::vector<unsigned char> &data)
{
    if (data.empty())
        return 0;

    return memCrc32Update(0, data.data(), data.size());
}

MEM_NO_INSTRUMENT static bool memWriteRaw(FILE *file, const void *data, size_t size, uint32_t *crc32 = nullptr)
{
    if (!file)
        return false;

    if (size > 0 && fwrite(data, 1, size, file) != size)
        return false;

    if (crc32 && data && size > 0)
        *crc32 = memCrc32Update(*crc32, static_cast<const unsigned char *>(data), size);

    return true;
}

MEM_NO_INSTRUMENT static bool memWriteU16LE(FILE *file, uint16_t value, uint32_t *crc32 = nullptr)
{
    unsigned char bytes[2] = {static_cast<unsigned char>(value & 0xffu),
                              static_cast<unsigned char>((value >> 8) & 0xffu)};
    return memWriteRaw(file, bytes, sizeof(bytes), crc32);
}

MEM_NO_INSTRUMENT static bool memWriteU32LE(FILE *file, uint32_t value, uint32_t *crc32 = nullptr)
{
    unsigned char bytes[4] = {static_cast<unsigned char>(value & 0xffu),
                              static_cast<unsigned char>((value >> 8) & 0xffu),
                              static_cast<unsigned char>((value >> 16) & 0xffu),
                              static_cast<unsigned char>((value >> 24) & 0xffu)};
    return memWriteRaw(file, bytes, sizeof(bytes), crc32);
}

MEM_NO_INSTRUMENT static bool memWriteU64LE(FILE *file, uint64_t value, uint32_t *crc32 = nullptr)
{
    unsigned char bytes[8] = {0};
    for (unsigned i = 0; i < 8; ++i)
        bytes[i] = static_cast<unsigned char>((value >> (i * 8)) & 0xffu);

    return memWriteRaw(file, bytes, sizeof(bytes), crc32);
}

#if defined(FASTGRIND_INSTRUMENT)
MEM_NO_INSTRUMENT static const char *demangleFunc(const char *mangled);
MEM_NO_INSTRUMENT static const char *enhancedSymbolResolve(void *addr);
#endif

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
     * @brief Default constructor for memFrame.
     * @details Initializes all counters to zero.
     */
    MEM_NO_INSTRUMENT memFrame() : mallocBytes(0), freeBytes(0), funcId(0), frameId(0)
    {
    }

    /**
     * @brief Construct a new memFrame object with specified values.
     * @param mallocBytes Total bytes allocated in this frame
     * @param freeBytes Total bytes freed in this frame
     * @param funcId Function identifier for this frame
     * @param frameId Stack frame identifier
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

    size_t mallocBytes; ///< Total bytes allocated during this frame's lifetime
    size_t freeBytes;   ///< Total bytes freed during this frame's lifetime
    size_t funcId;      ///< Unique identifier for the function
    size_t frameId;     ///< Unique identifier for the call stack frame
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
    bool _exit = false;             ///< Flag to signal timer thread to exit
    std::thread *_thread = nullptr; ///< Background timer thread
    std::atomic<size_t> _tick;      ///< Current tick count in milliseconds
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
    /** @brief Default constructor creating an unnamed node. */
    MEM_NO_INSTRUMENT memNode()
    {
    }

    /**
     * @brief Construct named node.
     * @param name Function name associated with this node (pointer assumed stable)
     */
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
     * @param tm Total malloc bytes across all nodes (for percentage calculation)
     * @param tf Total free bytes across all nodes (for percentage calculation)
     * @param indent Current indentation level (spaces are 4 * indent).
     * @return Formatted string representation of this node and its children
     */
    MEM_NO_INSTRUMENT std::string str(unsigned long tm, unsigned long tf, unsigned indent = 0) const
    {
        double rm = tm > 0 ? double(_mallocBytes * 100) / double(tm) : 0.0;
        double rf = tf > 0 ? double(_freeBytes * 100) / double(tf) : 0.0;
        rm = trunc(rm * 100) / 100;
        rf = trunc(rf * 100) / 100;
        std::string s = std::string(indent * 4, ' ') +
                        memFormat("- %.2f/%.2f%%  %s malloc %'ld free %'ld", rm, rf, _name, _mallocBytes, _freeBytes);

        for (auto it = _childs.begin(); it != _childs.end(); ++it)
        {
            s += std::string("\n") + it->second.str(tm, tf, indent + 1);
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
        std::string content = "[Dump By Callstack]\n" + str(_mallocBytes, _freeBytes);
        // printf("%s\n", content.c_str());

        FILE *file = fopen(__MEM_PATH_TEXT_RESULT, "wb");
        if (file)
        {
            fwrite(content.c_str(), 1, content.size(), file);
            fclose(file);
            printf("[FASTGRIND] saved: %s (size=%zu bytes)\n", __MEM_PATH_TEXT_RESULT, content.size());
            fflush(stdout);
        }
    }

  protected:
    const char *_name = nullptr; ///< Function name pointer (stable during process lifetime)

    //! @note Child map key is raw function name pointer (assumed stable during process lifetime).
    std::map<const char *, memNode> _childs; ///< Child nodes representing deeper call stack levels

    size_t _mallocBytes = 0; ///< Total bytes allocated in this subtree
    size_t _freeBytes = 0;   ///< Total bytes freed in this subtree
};

class memLocalInfo;
/**
 * @class memGlobalInfo
 * @brief Singleton aggregating memory statistics across all threads.
 *
 * Maintains thread->frameId->tick->memFrame structures and a mapping from
 * frameId to captured call stacks (array of function name pointers). Responsible
 * for dumping textual and binary reports. Thread-local data merges into this
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
    MEM_NO_INSTRUMENT static memGlobalInfo *instance()
    {
        return _instance;
    }

    MEM_NO_INSTRUMENT static void setInst(memGlobalInfo *inst)
    {
        _instance = inst;
    }

    static std::atomic<int> &refs()
    {
        return _refs;
    }

    /** @return Current global timer tick (ms). */
    MEM_NO_INSTRUMENT size_t time() const
    {
        return _timer.time();
    }

    /**
     * @brief Dump aggregated statistics and call tree to stdout and export binary trace.
     * @note Safe to call multiple times; the binary file is overwritten.
     */
    MEM_NO_INSTRUMENT void dump() const
    {
        printf("[FASTGRIND] Start summary memory info\n");
        fflush(stdout);

        memNode info("Total");
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

        exportBinary();

        fflush(stdout);
    }

  protected:
    /** @brief Export sparse profiling data to a binary trace file. */
    MEM_NO_INSTRUMENT void exportBinary() const
    {
        struct memFgbStackNode
        {
            uint32_t nodeId = 0;
            uint32_t parentNodeId = 0;
            uint32_t functionId = 0;
            uint16_t depth = 0;
            uint16_t flags = MEM_FGB_STACK_FLAG_NONE;
        };

        struct memFgbThreadInfo
        {
            uint64_t threadId = 0;
            uint64_t firstTickMs = 0;
            uint64_t lastTickMs = 0;
            uint64_t totalMallocBytes = 0;
            uint64_t totalFreeBytes = 0;
            bool hasData = false;
            std::set<uint32_t> functionIds;
        };

        struct memFgbFrameRecord
        {
            uint32_t leafNodeId = 0;
            uint64_t mallocBytes = 0;
            uint64_t freeBytes = 0;
        };

        struct memFgbTickDirectoryEntry
        {
            uint64_t tickMs = 0;
            uint64_t fileOffset = 0;
            uint32_t threadBlockCount = 0;
        };

        constexpr uint16_t headerBytes = 96;
        constexpr uint32_t sectionCount = 7;
        constexpr uint32_t sectionEntryBytes = 32;

        std::set<std::string> uniqueFunctions;
        for (const auto &entry : _callstacks)
        {
            const auto &callstack = entry.second;
            for (size_t i = 0; i < __MEM_MAX_STACK_DEPTH; ++i)
            {
                const char *name = callstack[i];
                if (!name)
                    break;

                uniqueFunctions.emplace(name);
            }
        }

        std::vector<std::string> functionNames(uniqueFunctions.begin(), uniqueFunctions.end());
        std::map<std::string, uint32_t> functionIds;
        for (size_t i = 0; i < functionNames.size(); ++i)
            functionIds[functionNames[i]] = static_cast<uint32_t>(i + 1);

        std::map<size_t, std::vector<uint32_t>> frameStacks;
        for (const auto &entry : _callstacks)
        {
            std::vector<uint32_t> stackIds;
            const auto &callstack = entry.second;
            for (size_t i = 0; i < __MEM_MAX_STACK_DEPTH; ++i)
            {
                const char *name = callstack[i];
                if (!name)
                    break;

                stackIds.push_back(functionIds.at(std::string(name)));
            }
            frameStacks.emplace(entry.first, std::move(stackIds));
        }

        std::vector<std::pair<size_t, std::vector<uint32_t>>> orderedFrameStacks(frameStacks.begin(), frameStacks.end());
        std::sort(
            orderedFrameStacks.begin(), orderedFrameStacks.end(),
            [](const std::pair<size_t, std::vector<uint32_t>> &lhs,
               const std::pair<size_t, std::vector<uint32_t>> &rhs) {
                if (lhs.second != rhs.second)
                    return lhs.second < rhs.second;

                return lhs.first < rhs.first;
            });

        std::vector<memFgbStackNode> stackNodes;
        stackNodes.push_back(memFgbStackNode());

        std::map<uint32_t, std::map<uint32_t, uint32_t>> trieChildren;
        std::map<size_t, uint32_t> frameLeafNodes;
        std::map<uint32_t, std::set<uint32_t>> functionLeafSets;

        for (const auto &entry : orderedFrameStacks)
        {
            uint32_t parentNodeId = 0;
            uint16_t depth = 0;
            std::set<uint32_t> ancestryFunctions;

            for (uint32_t functionId : entry.second)
            {
                ++depth;
                ancestryFunctions.emplace(functionId);

                auto &childMap = trieChildren[parentNodeId];
                auto found = childMap.find(functionId);
                if (found == childMap.end())
                {
                    uint32_t nodeId = static_cast<uint32_t>(stackNodes.size());
                    stackNodes.push_back(memFgbStackNode{nodeId, parentNodeId, functionId, depth, MEM_FGB_STACK_FLAG_NONE});
                    childMap[functionId] = nodeId;
                    parentNodeId = nodeId;
                }
                else
                {
                    parentNodeId = found->second;
                }
            }

            stackNodes[parentNodeId].flags = static_cast<uint16_t>(stackNodes[parentNodeId].flags | MEM_FGB_STACK_FLAG_LEAF);
            frameLeafNodes[entry.first] = parentNodeId;
            for (uint32_t functionId : ancestryFunctions)
                functionLeafSets[functionId].insert(parentNodeId);
        }

        std::map<uint64_t, memFgbThreadInfo> threads;
        std::map<uint64_t, std::map<uint64_t, std::vector<memFgbFrameRecord>>> tickData;
        uint64_t maxTickMs = 0;

        for (const auto &threadEntry : _frames)
        {
            uint64_t threadId = static_cast<uint64_t>(threadEntry.first);
            auto &threadInfo = threads[threadId];
            threadInfo.threadId = threadId;

            for (const auto &frameEntry : threadEntry.second)
            {
                size_t frameId = frameEntry.first;
                if (frameLeafNodes.find(frameId) == frameLeafNodes.end() || frameStacks.find(frameId) == frameStacks.end())
                    continue;

                const uint32_t leafNodeId = frameLeafNodes.at(frameId);
                const auto &stackIds = frameStacks.at(frameId);
                threadInfo.functionIds.insert(stackIds.begin(), stackIds.end());

                for (const auto &tickEntry : frameEntry.second)
                {
                    uint64_t tickMs = static_cast<uint64_t>(tickEntry.first);
                    const auto &frame = tickEntry.second;

                    tickData[tickMs][threadId].push_back(memFgbFrameRecord{leafNodeId, static_cast<uint64_t>(frame.mallocBytes),
                                                                           static_cast<uint64_t>(frame.freeBytes)});

                    threadInfo.totalMallocBytes += static_cast<uint64_t>(frame.mallocBytes);
                    threadInfo.totalFreeBytes += static_cast<uint64_t>(frame.freeBytes);

                    if (!threadInfo.hasData)
                    {
                        threadInfo.firstTickMs = tickMs;
                        threadInfo.lastTickMs = tickMs;
                        threadInfo.hasData = true;
                    }
                    else
                    {
                        if (tickMs < threadInfo.firstTickMs)
                            threadInfo.firstTickMs = tickMs;
                        if (tickMs > threadInfo.lastTickMs)
                            threadInfo.lastTickMs = tickMs;
                    }

                    if (tickMs > maxTickMs)
                        maxTickMs = tickMs;
                }
            }
        }

        std::vector<unsigned char> functionSection;
        for (const auto &functionName : functionNames)
        {
            memAppendU32LE(functionSection, static_cast<uint32_t>(functionName.size()));
            memAppendBytes(functionSection, functionName.data(), functionName.size());
        }

        std::vector<unsigned char> stackSection;
        for (const auto &node : stackNodes)
        {
            memAppendU32LE(stackSection, node.nodeId);
            memAppendU32LE(stackSection, node.parentNodeId);
            memAppendU32LE(stackSection, node.functionId);
            memAppendU16LE(stackSection, node.depth);
            memAppendU16LE(stackSection, node.flags);
        }

        uint32_t activeThreadCount = 0;
        for (const auto &threadEntry : threads)
        {
            if (threadEntry.second.hasData)
                ++activeThreadCount;
        }

        std::vector<unsigned char> threadSection;
        std::vector<unsigned char> threadFunctionSection;
        memAppendU32LE(threadFunctionSection, activeThreadCount);
        for (const auto &threadEntry : threads)
        {
            const auto &threadInfo = threadEntry.second;
            if (!threadInfo.hasData)
                continue;

            memAppendU64LE(threadSection, threadInfo.threadId);
            memAppendU64LE(threadSection, threadInfo.firstTickMs);
            memAppendU64LE(threadSection, threadInfo.lastTickMs);
            memAppendU64LE(threadSection, threadInfo.totalMallocBytes);
            memAppendU64LE(threadSection, threadInfo.totalFreeBytes);

            memAppendU64LE(threadFunctionSection, threadInfo.threadId);
            memAppendU32LE(threadFunctionSection, static_cast<uint32_t>(threadInfo.functionIds.size()));
            memAppendU32LE(threadFunctionSection, 0);
            for (uint32_t functionId : threadInfo.functionIds)
                memAppendU32LE(threadFunctionSection, functionId);
        }

        std::vector<unsigned char> functionPostingSection;
        memAppendU32LE(functionPostingSection, static_cast<uint32_t>(functionNames.size()));
        for (uint32_t functionId = 1; functionId <= functionNames.size(); ++functionId)
        {
            const auto &leafSet = functionLeafSets[functionId];
            memAppendU32LE(functionPostingSection, functionId);
            memAppendU32LE(functionPostingSection, static_cast<uint32_t>(leafSet.size()));
            for (uint32_t leafNodeId : leafSet)
                memAppendU32LE(functionPostingSection, leafNodeId);
        }

        const std::string tmpPath = memFormat("%s.tmp", __MEM_PATH_BINARY_RESULT);
        FILE *file = fopen(tmpPath.c_str(), "wb+");
        if (!file)
        {
            fprintf(stderr, "[FASTGRIND] failed to open %s: %s\n", tmpPath.c_str(), strerror(errno));
            return;
        }

        auto failExport = [&](const char *message) MEM_NO_INSTRUMENT {
            int savedErrno = errno;
            fclose(file);
            remove(tmpPath.c_str());
            fprintf(stderr, "[FASTGRIND] %s: %s\n", message, strerror(savedErrno));
        };

        std::vector<unsigned char> headerPadding(headerBytes + sectionCount * sectionEntryBytes, 0);
        if (!memWriteRaw(file, headerPadding.data(), headerPadding.size()))
        {
            failExport("failed to reserve binary header space");
            return;
        }

        uint64_t fileOffset = static_cast<uint64_t>(headerPadding.size());
        std::vector<memFgbSectionInfo> sections(sectionCount);

        auto writeSectionBuffer = [&](size_t index, uint32_t type, const std::vector<unsigned char> &buffer) MEM_NO_INSTRUMENT {
            sections[index].type = type;
            sections[index].flags = MEM_FGB_SECTION_FLAG_CRC32;
            sections[index].offset = fileOffset;
            sections[index].length = static_cast<uint64_t>(buffer.size());
            sections[index].crc32 = memCrc32(buffer);
            if (!buffer.empty() && !memWriteRaw(file, buffer.data(), buffer.size()))
                return false;

            fileOffset += buffer.size();
            return true;
        };

        if (!writeSectionBuffer(0, MEM_FGB_SECTION_FUNCTION_TABLE, functionSection) ||
            !writeSectionBuffer(1, MEM_FGB_SECTION_STACK_TABLE, stackSection) ||
            !writeSectionBuffer(2, MEM_FGB_SECTION_THREAD_TABLE, threadSection) ||
            !writeSectionBuffer(3, MEM_FGB_SECTION_THREAD_FUNCTION_DIRECTORY, threadFunctionSection) ||
            !writeSectionBuffer(4, MEM_FGB_SECTION_FUNCTION_STACK_POSTINGS, functionPostingSection))
        {
            failExport("failed to write binary section");
            return;
        }

        sections[5].type = MEM_FGB_SECTION_TICK_DATA;
        sections[5].flags = MEM_FGB_SECTION_FLAG_CRC32;
        sections[5].offset = fileOffset;
        uint32_t tickDataCrc = 0;
        std::vector<memFgbTickDirectoryEntry> tickDirectoryEntries;

        for (const auto &tickEntry : tickData)
        {
            tickDirectoryEntries.push_back(
                memFgbTickDirectoryEntry{tickEntry.first, fileOffset, static_cast<uint32_t>(tickEntry.second.size())});

            for (const auto &threadEntry : tickEntry.second)
            {
                if (!memWriteU64LE(file, threadEntry.first, &tickDataCrc) ||
                    !memWriteU32LE(file, static_cast<uint32_t>(threadEntry.second.size()), &tickDataCrc) ||
                    !memWriteU32LE(file, 0, &tickDataCrc))
                {
                    failExport("failed to write tick data header");
                    return;
                }
                fileOffset += 16;

                for (const auto &record : threadEntry.second)
                {
                    if (!memWriteU32LE(file, record.leafNodeId, &tickDataCrc) || !memWriteU32LE(file, 0, &tickDataCrc) ||
                        !memWriteU64LE(file, record.mallocBytes, &tickDataCrc) ||
                        !memWriteU64LE(file, record.freeBytes, &tickDataCrc))
                    {
                        failExport("failed to write frame record");
                        return;
                    }
                    fileOffset += 24;
                }
            }
        }

        sections[5].length = fileOffset - sections[5].offset;
        sections[5].crc32 = tickDataCrc;

        std::vector<unsigned char> tickDirectorySection;
        for (const auto &entry : tickDirectoryEntries)
        {
            memAppendU64LE(tickDirectorySection, entry.tickMs);
            memAppendU64LE(tickDirectorySection, entry.fileOffset);
            memAppendU32LE(tickDirectorySection, entry.threadBlockCount);
            memAppendU32LE(tickDirectorySection, 0);
        }

        if (!writeSectionBuffer(6, MEM_FGB_SECTION_TICK_DIRECTORY, tickDirectorySection))
        {
            failExport("failed to write tick directory section");
            return;
        }

        if (fseek(file, 0, SEEK_SET) != 0)
        {
            failExport("failed to rewrite binary header");
            return;
        }

        if (!memWriteRaw(file, "FGB1", 4) || !memWriteU16LE(file, 1) || !memWriteU16LE(file, 0) ||
            !memWriteRaw(file, "\x01", 1) || !memWriteRaw(file, "\x00", 1) || !memWriteU16LE(file, headerBytes) ||
            !memWriteU32LE(file, __MEM_SAMPLE_INTERVAL_MS) || !memWriteU64LE(file, maxTickMs) ||
            !memWriteU32LE(file, static_cast<uint32_t>(functionNames.size())) ||
            !memWriteU32LE(file, static_cast<uint32_t>(stackNodes.size())) ||
            !memWriteU32LE(file, static_cast<uint32_t>(tickDirectoryEntries.size())) ||
            !memWriteU32LE(file, activeThreadCount) || !memWriteU32LE(file, sectionCount) ||
            !memWriteU32LE(file, 0) || !memWriteU64LE(file, headerBytes) || !memWriteU64LE(file, sections[0].offset) ||
            !memWriteU64LE(file, sections[1].offset) || !memWriteU64LE(file, sections[6].offset) ||
            !memWriteU64LE(file, sections[5].offset) || !memWriteU64LE(file, 0))
        {
            failExport("failed to serialize binary header");
            return;
        }

        for (const auto &section : sections)
        {
            if (!memWriteU32LE(file, section.type) || !memWriteU32LE(file, section.flags) ||
                !memWriteU64LE(file, section.offset) || !memWriteU64LE(file, section.length) ||
                !memWriteU32LE(file, section.crc32) || !memWriteU32LE(file, 0))
            {
                failExport("failed to serialize section directory");
                return;
            }
        }

        if (fflush(file) != 0)
        {
            failExport("failed to flush binary trace file");
            return;
        }

        fclose(file);

        if (rename(tmpPath.c_str(), __MEM_PATH_BINARY_RESULT) != 0)
        {
            remove(tmpPath.c_str());
            fprintf(stderr, "[FASTGRIND] failed to publish %s: %s\n", __MEM_PATH_BINARY_RESULT, strerror(errno));
            return;
        }

        printf("[FASTGRIND] saved: %s (size=%zu bytes)\n", __MEM_PATH_BINARY_RESULT, static_cast<size_t>(fileOffset));
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

    MEM_NO_INSTRUMENT void callStackTrans()
    {
#if defined(FASTGRIND_INSTRUMENT)
        for (auto &kv : _callstacks)
        {
            auto &arr = kv.second;
            for (size_t i = 0; i < __MEM_MAX_STACK_DEPTH; ++i)
            {
                const char *entry = arr[i];
                if (!entry)
                    break;

                void *func_addr = reinterpret_cast<void *>(const_cast<char *>(entry));
                const char *resolved = enhancedSymbolResolve(func_addr);
                arr[i] = resolved;
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
    static memGlobalInfo *_instance;
    static std::atomic<int> _refs;

    // timer need to init after all other class members.
    memTimer _timer;
};

#if defined(__CPP_STD_17)
__attribute__((weak)) MEM_INLINE_USED inline memGlobalInfo *memGlobalInfo::_instance = nullptr;
__attribute__((weak)) MEM_INLINE_USED inline std::atomic<int> memGlobalInfo::_refs{0};
#else
__attribute__((weak)) MEM_INLINE_USED memGlobalInfo *memGlobalInfo::_instance = nullptr;
__attribute__((weak)) MEM_INLINE_USED std::atomic<int> memGlobalInfo::_refs{0};
#endif

/**
 * @class memStack
 * @brief Thread-local logical call stack used to synthesize frame identifiers.
 *
 * The stack is explicitly manipulated via fastgrind RAII objects (FAST_GRIND macro)
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
        if (_offset >= __MEM_MAX_STACK_DEPTH) {
            static bool warned_stack = false;
            if (!warned_stack) {
                fprintf(stderr, "[FASTGRIND] WARNING: Call stack depth exceeded %d\n", __MEM_MAX_STACK_DEPTH);
                warned_stack = true;
            }

            return;
        }

        assert(_offset < __MEM_MAX_STACK_DEPTH);

        _stack[_offset++] = v;
        _stackId += size_t(v);
    }

    /** @brief Pop top of stack (must not be empty). */
    MEM_NO_INSTRUMENT const char *pop()
    {
        if (_offset == 0)
            return nullptr;

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

static thread_local long tid = syscall(SYS_gettid);

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
        if (!memGlobalInfo::instance())
            return;

        if (tid == -1) {
            static bool warned_tid = false;
            if (!warned_tid) {
                fprintf(stderr, "[FASTGRIND] WARNING: Thread ID error: %ld\n", tid);
                warned_tid = true;
            }
        }

        size_t stid = size_t(tid);
        std::lock_guard<std::mutex> lg(memGlobalInfo::instance()->_lk);

        memGlobalInfo::instance()->_frames[stid] = _frames;

        for (auto it = _callstacks.begin(); it != _callstacks.end(); ++it)
            memGlobalInfo::instance()->_callstacks[it->first] = it->second;

        reset();
    }

    /** @brief Record an allocation of sz bytes (real usable size). */
    MEM_NO_INSTRUMENT void add(size_t sz)
    {
        if (_nested || memStack::instance().depth() == 0 || !memGlobalInfo::instance())
            return;

        ++_nested;
        const auto &stack = memStack::instance();
        getFrame(size_t(stack.top()), stack.frameId(), memGlobalInfo::instance()->time()).mallocBytes += sz;
        --_nested;
    }

    /** @brief Record a deallocation of sz bytes (real usable size). */
    MEM_NO_INSTRUMENT void sub(size_t sz)
    {
        if (_nested || memStack::instance().depth() == 0 || !memGlobalInfo::instance())
            return;

        ++_nested;
        const auto &stack = memStack::instance();
        getFrame(size_t(stack.top()), stack.frameId(), memGlobalInfo::instance()->time()).freeBytes += sz;
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
 * @class fastgrind
 * @brief RAII helper pushing current function onto logical stack.
 *
 * Construct an instance at function scope to automatically attribute all
 * allocations to that function until destruction (end of scope). Macro
 * FAST_GRIND wraps construction with __PRETTY_FUNCTION__ providing decorated name.
 */
class fastgrind
{
  public:
    /**
     * @brief Construct a fastgrind probe and push function onto call stack.
     * @param name Function name to push onto the logical call stack
     */
    MEM_NO_INSTRUMENT fastgrind(const char *name)
    {
        if (__FAST_GRIND_STATUS)
            memStack::instance().push(name);
    }

    /**
     * @brief Destructor automatically pops function from call stack.
     */
    MEM_NO_INSTRUMENT ~fastgrind()
    {
        if (__FAST_GRIND_STATUS)
            memStack::instance().pop();
    }
};

/**
 * @def FAST_GRIND
 * @brief Annotate a scope to participate in logical memory call stack.
 * @details Expands to creation of a fastgrind with the current pretty function symbol.
 */
#define FAST_GRIND fastgrind __probe__(__PRETTY_FUNCTION__);

extern "C"
{
    MEM_NO_INSTRUMENT __attribute__((weak, constructor)) void before_main()
    {
        if (memGlobalInfo::refs().fetch_add(1) == 0)
        {
            memGlobalInfo::setInst(new memGlobalInfo);
        }
    }

    MEM_NO_INSTRUMENT __attribute__((weak, destructor)) void after_main()
    {
        if (memGlobalInfo::refs().fetch_sub(1) == 1)
        {
            delete memGlobalInfo::instance();
            memGlobalInfo::setInst(nullptr);
        }
    }

#if defined(FASTGRIND_INSTRUMENT)
    MEM_NO_INSTRUMENT __attribute__((weak)) void __cyg_profile_func_enter(void *this_fn, void *call_site)
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

    MEM_NO_INSTRUMENT __attribute__((weak)) void __cyg_profile_func_exit(void *this_fn, void *call_site)
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
#endif // FASTGRIND_INSTRUMENT

#if defined(FASTGRIND_TC_MALLOC)
    #define TC_MALLOC 1
#elif defined(FASTGRIND_JE_MALLOC)
    #define JE_MALLOC 1
    extern void *je_sdallocx_default(void *ptr, size_t size, int flags);
    extern void *je_malloc_default(size_t size);
    extern void je_free_default(void *ptr);

    MEM_NO_INSTRUMENT MEM_INLINE_USED static inline void *je_calloc_emulate(size_t nmemb, size_t size)
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

        void *p = je_malloc_default(total);
        if (p)
        {
            memset(p, 0, total);
        }
        return p;
    }

    MEM_NO_INSTRUMENT MEM_INLINE_USED static inline void *je_realloc_emulate(void *ptr, size_t size)
    {
        if (size == 0)
        {
            if (ptr)
            {
                je_free_default(ptr);
            }
            return nullptr;
        }
        if (!ptr)
        {
            return je_malloc_default(size);
        }
        void *newp = je_malloc_default(size);
        if (!newp)
        {
            return nullptr;
        }
        size_t old_sz = malloc_usable_size(ptr);
        size_t copy_sz = old_sz < size ? old_sz : size;
        if (copy_sz)
            memcpy(newp, ptr, copy_sz);
        je_free_default(ptr);
        return newp;
    }

    MEM_NO_INSTRUMENT MEM_INLINE_USED static inline void *je_valloc_emulate(size_t size)
    {
        if (size == 0)
            size = 1;

        long pg = sysconf(_SC_PAGESIZE);
        if (pg <= 0)
            pg = 4096;
        size_t align = static_cast<size_t>(pg);
        size_t rounded = (size + align - 1) & ~(align - 1);
        return mallocx(rounded, MALLOCX_ALIGN(align));
    }

    MEM_NO_INSTRUMENT MEM_INLINE_USED static inline void *je_pvalloc_emulate(size_t size)
    {
        if (size == 0)
            size = 1;

        long pg = sysconf(_SC_PAGESIZE);
        if (pg <= 0)
            pg = 4096;
        size_t align = static_cast<size_t>(pg);
        size_t rounded = (size + align - 1) & ~(align - 1);
        return mallocx(rounded, MALLOCX_ALIGN(align));
    }

    MEM_NO_INSTRUMENT MEM_INLINE_USED static inline void *je_memalign_emulate(size_t alignment, size_t size)
    {
        if (size == 0)
            size = 1;

        if (alignment == 0 || (alignment & (alignment - 1)) != 0 || (alignment % sizeof(void *) != 0))
        {
            errno = EINVAL;
            return nullptr;
        }

        return mallocx(size, MALLOCX_ALIGN(alignment));
    }

    MEM_NO_INSTRUMENT MEM_INLINE_USED static inline int je_posix_memalign_emulate(void **memptr, size_t alignment,
                                                                                  size_t size)
    {
        if (memptr == nullptr)
        {
            return EINVAL;
        }

        if (size == 0)
            size = 1;

        if (alignment == 0 || (alignment & (alignment - 1)) != 0 || (alignment % sizeof(void *) != 0))
        {
            return EINVAL;
        }

        void *p = mallocx(size, MALLOCX_ALIGN(alignment));
        if (!p)
        {
            return ENOMEM;
        }
        *memptr = p;
        return 0;
    }

    MEM_NO_INSTRUMENT MEM_INLINE_USED static inline void *je_aligned_alloc_emulate(size_t alignment, size_t size)
    {
        if (alignment == 0 || (alignment & (alignment - 1)) != 0 || (alignment % sizeof(void *) != 0))
        {
            errno = EINVAL;
            return nullptr;
        }
        if (size % alignment != 0)
        {
            errno = EINVAL;
            return nullptr;
        }

        return mallocx(size, MALLOCX_ALIGN(alignment));
    }

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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap_malloc(size_t sz)
    {
        if (sz == 0)
            sz = 1;

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            return tc_malloc(sz);
    #elif defined(FASTGRIND_JE_MALLOC)
            return je_malloc_default(sz);
    #else
            return __real_malloc(sz);
    #endif
        }

        __mem_in_probe_ = true;

        void *p =
    #if defined(FASTGRIND_TC_MALLOC)
            tc_malloc(sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap_calloc(size_t nmemb, size_t size)
    {
        if (nmemb == 0 || size == 0)
        {
            nmemb = 1;
            size = 1;
        }

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            return tc_calloc(nmemb, size);
    #elif defined(FASTGRIND_JE_MALLOC)
            return je_calloc_emulate(nmemb, size);
    #else
            return __real_calloc(nmemb, size);
    #endif
        }

        __mem_in_probe_ = true;

        void *p = nullptr;
    #if defined(FASTGRIND_TC_MALLOC)
        p = tc_calloc(nmemb, size);
    #elif defined(FASTGRIND_JE_MALLOC)
        p = je_calloc_emulate(nmemb, size);
    #else
        p = __real_calloc(nmemb, size);
    #endif

        if (p)
        {
            size_t real_sz = malloc_usable_size(p);
            memLocalInfo::instance().add(real_sz);
        }

        __mem_in_probe_ = false;
        return p;
    }

    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap_realloc(void *ptr, size_t size)
    {
        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            return tc_realloc(ptr, size);
    #elif defined(FASTGRIND_JE_MALLOC)
            return je_realloc_emulate(ptr, size);
    #else
            return __real_realloc(ptr, size);
    #endif
        }

        __mem_in_probe_ = true;

        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
        if (size == 0 && ptr)
        {
            if (old_size)
                memLocalInfo::instance().sub(old_size);

    #if defined(FASTGRIND_TC_MALLOC)
            tc_free(ptr);
    #elif defined(FASTGRIND_JE_MALLOC)
            je_free_default(ptr);
    #else
            __real_free(ptr);
    #endif

            __mem_in_probe_ = false;
            return nullptr;
        }

        void *p = nullptr;
    #if defined(FASTGRIND_TC_MALLOC)
        p = tc_realloc(ptr, size);
    #elif defined(FASTGRIND_JE_MALLOC)
        p = je_realloc_emulate(ptr, size);
    #else
        p = __real_realloc(ptr, size);
    #endif

        if (p)
        {
            if (ptr && old_size)
                memLocalInfo::instance().sub(old_size);
            memLocalInfo::instance().add(malloc_usable_size(p));
        }

        __mem_in_probe_ = false;
        return p;
    }

    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap_free(void *p)
    {
        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
        je_free_default(p);
    #else
        __real_free(p);
    #endif
    }

    // override operator new
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap__Znwm(size_t sz)
    {
        if (sz == 0)
            sz = 1;

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            void *p = tc_malloc(sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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
    #if defined(FASTGRIND_TC_MALLOC)
            p = tc_malloc(sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap__Znam(size_t sz)
    {
        if (sz == 0)
            sz = 1;

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            void *p = tc_malloc(sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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
    #if defined(FASTGRIND_TC_MALLOC)
            p = tc_malloc(sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap__ZdlPv(void *p)
    {
        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
        je_free_default(p);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete[]
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap__ZdaPv(void *p)
    {
        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
        je_free_default(p);
    #else
        __real_free(p);
    #endif
    }

    // override operator new with nothrow
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap__ZnwmRKSt9nothrow_t(size_t sz, const std::nothrow_t &)
    {
        if (sz == 0)
            sz = 1;

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            void *p = tc_malloc(sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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
    #if defined(FASTGRIND_TC_MALLOC)
            p = tc_malloc(sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap__ZnamRKSt9nothrow_t(size_t sz, const std::nothrow_t &)
    {
        if (sz == 0)
            sz = 1;

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            void *p = tc_malloc(sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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
    #if defined(FASTGRIND_TC_MALLOC)
            p = tc_malloc(sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap__ZdlPvRKSt9nothrow_t(void *p, const std::nothrow_t &)
    {
        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
        je_free_default(p);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete[] with nothrow
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap__ZdaPvRKSt9nothrow_t(void *p, const std::nothrow_t &)
    {
        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
        je_free_default(p);
    #else
        __real_free(p);
    #endif
    }

    #if defined(DEFAULT_MALLOC) && defined(__USE_SYS_WRAP)
    // sys call
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap_sbrk(intptr_t increment)
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline int __wrap_brk(void *addr)
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap_mmap(void *addr, size_t length, int prot, int flags, int fd,
                                                               off_t offset)
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline int __wrap_munmap(void *addr, size_t length)
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap_mremap(void *old_address, size_t old_size, size_t new_size,
                                                                 int flags, ...)
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap_valloc(size_t size)
    {
        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            return tc_valloc(size);
    #elif defined(FASTGRIND_JE_MALLOC)
            return je_valloc_emulate(size);
    #else
            return __real_valloc(size);
    #endif
        }

        __mem_in_probe_ = true;

        void *p =
    #if defined(FASTGRIND_TC_MALLOC)
            tc_valloc(size);
    #elif defined(FASTGRIND_JE_MALLOC)
            je_valloc_emulate(size);
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap_pvalloc(size_t size)
    {
        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            return tc_pvalloc(size);
    #elif defined(FASTGRIND_JE_MALLOC)
            return je_pvalloc_emulate(size);
    #else
            return __real_pvalloc(size);
    #endif
        }

        __mem_in_probe_ = true;

        void *p =
    #if defined(FASTGRIND_TC_MALLOC)
            tc_pvalloc(size);
    #elif defined(FASTGRIND_JE_MALLOC)
            je_pvalloc_emulate(size);
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap_memalign(size_t alignment, size_t size)
    {
        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            return tc_memalign(alignment, size);
    #elif defined(FASTGRIND_JE_MALLOC)
            return je_memalign_emulate(alignment, size);
    #else
            return __real_memalign(alignment, size);
    #endif
        }

        __mem_in_probe_ = true;

        void *p =
    #if defined(FASTGRIND_TC_MALLOC)
            tc_memalign(alignment, size);
    #elif defined(FASTGRIND_JE_MALLOC)
            je_memalign_emulate(alignment, size);
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline int __wrap_posix_memalign(void **memptr, size_t alignment, size_t size)
    {
        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            return tc_posix_memalign(memptr, alignment, size);
    #elif defined(FASTGRIND_JE_MALLOC)
            return je_posix_memalign_emulate(memptr, alignment, size);
    #else
            return __real_posix_memalign(memptr, alignment, size);
    #endif
        }

        __mem_in_probe_ = true;

        int ret =
    #if defined(FASTGRIND_TC_MALLOC)
            tc_posix_memalign(memptr, alignment, size);
    #elif defined(FASTGRIND_JE_MALLOC)
            je_posix_memalign_emulate(memptr, alignment, size);
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap_reallocarray(void *ptr, size_t nmemb, size_t size)
    {
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
    #if defined(FASTGRIND_TC_MALLOC)
            return tc_realloc(ptr, total);
    #elif defined(FASTGRIND_JE_MALLOC)
            return realloc(ptr, total);
    #else
            return __real_realloc(ptr, total);
    #endif
        }

        __mem_in_probe_ = true;

        size_t old_size = ptr ? malloc_usable_size(ptr) : 0;

        if (total == 0 && ptr)
        {
            if (old_size)
                memLocalInfo::instance().sub(old_size);

    #if defined(FASTGRIND_TC_MALLOC)
            tc_free(ptr);
    #elif defined(FASTGRIND_JE_MALLOC)
            je_free_default(ptr);
    #else
            __real_free(ptr);
    #endif
            __mem_in_probe_ = false;
            return nullptr;
        }

        void *p = nullptr;
    #if defined(FASTGRIND_TC_MALLOC)
        p = tc_realloc(ptr, total);
    #elif defined(FASTGRIND_JE_MALLOC)
        p = realloc(ptr, total);
    #else
        p = __real_realloc(ptr, total);
    #endif

        if (p)
        {
            if (ptr && old_size)
                memLocalInfo::instance().sub(old_size);
            memLocalInfo::instance().add(malloc_usable_size(p));
        }

        __mem_in_probe_ = false;
        return p;
    }
    // #endif

    // #if defined(__CPP_STD_14)
    // override operator delete(void*, std::size_t)
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap__ZdlPvm(void *p, size_t sz)
    {
        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(FASTGRIND_JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
        (void) sz;
    }

    // override operator delete[](void*, std::size_t)
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap__ZdaPvm(void *p, size_t sz)
    {
        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(FASTGRIND_JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
        (void) sz;
    }
#endif

#if defined(__CPP_STD_17)
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap_aligned_alloc(size_t alignment, size_t size)
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
    #if defined(FASTGRIND_TC_MALLOC)
            return tc_memalign(alignment, size);
    #elif defined(FASTGRIND_JE_MALLOC)
            return je_aligned_alloc_emulate(alignment, size);
    #else
            return __real_aligned_alloc(alignment, size);
    #endif
        }

        __mem_in_probe_ = true;

        void *p = nullptr;
    #if defined(FASTGRIND_TC_MALLOC)
        p = tc_memalign(alignment, size);
    #elif defined(FASTGRIND_JE_MALLOC)
        p = je_aligned_alloc_emulate(alignment, size);
    #else
        p = __real_aligned_alloc(alignment, size);
    #endif

        if (p)
            memLocalInfo::instance().add(malloc_usable_size(p));

        __mem_in_probe_ = false;
        return p;
    }

    // override operator new(std::size_t, std::align_val_t)
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap__ZnwmSt11align_val_t(size_t size, std::align_val_t al)
    {
        if (size == 0)
            size = 1;
        size_t alignment = static_cast<size_t>(al);

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            void *p = tc_memalign(alignment, size);
    #elif defined(FASTGRIND_JE_MALLOC)
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
    #if defined(FASTGRIND_TC_MALLOC)
            p = tc_memalign(alignment, size);
    #elif defined(FASTGRIND_JE_MALLOC)
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap__ZnamSt11align_val_t(size_t size, std::align_val_t al)
    {
        if (size == 0)
            size = 1;
        size_t alignment = static_cast<size_t>(al);

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            void *p = tc_memalign(alignment, size);
    #elif defined(FASTGRIND_JE_MALLOC)
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
    #if defined(FASTGRIND_TC_MALLOC)
            p = tc_memalign(alignment, size);
    #elif defined(FASTGRIND_JE_MALLOC)
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap__ZdlPvSt11align_val_t(void *p, std::align_val_t al)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
        dallocx(p, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete[](void *p, std::align_val_t al)
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap__ZdaPvSt11align_val_t(void *p, std::align_val_t al)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
        dallocx(p, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete(void *p, size_t sz, std::align_val_t al)
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap__ZdlPvmSt11align_val_t(void *p, size_t sz, std::align_val_t al)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(FASTGRIND_JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
        (void) sz;
    }

    // override operator delete[](void *p, size_t sz, std::align_val_t al)
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap__ZdaPvmSt11align_val_t(void *p, size_t sz, std::align_val_t al)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(FASTGRIND_JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
        (void) sz;
    }

    // override operator delete(void *p, size_t sz, const std::nothrow_t &)
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap__ZdlPvmRKSt9nothrow_t(void *p, size_t sz,
                                                                               const std::nothrow_t &)
    {
        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(FASTGRIND_JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
        (void) sz;
    }

    // override operator delete[](void *p, size_t sz, const std::nothrow_t &)
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap__ZdaPvmRKSt9nothrow_t(void *p, size_t sz,
                                                                               const std::nothrow_t &)
    {
        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(FASTGRIND_JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
        (void) sz;
    }

    // override operator new(std::size_t size, std::align_val_t al, const std::nothrow_t &)
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap__ZnwmSt11align_val_tRKSt9nothrow_t(size_t size,
                                                                                             std::align_val_t al,
                                                                                             const std::nothrow_t &)
    {
        if (size == 0)
            size = 1;
        size_t alignment = static_cast<size_t>(al);

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            void *p = tc_memalign(alignment, size);
    #elif defined(FASTGRIND_JE_MALLOC)
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
    #if defined(FASTGRIND_TC_MALLOC)
            p = tc_memalign(alignment, size);
    #elif defined(FASTGRIND_JE_MALLOC)
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void *__wrap__ZnamSt11align_val_tRKSt9nothrow_t(size_t size,
                                                                                             std::align_val_t al,
                                                                                             const std::nothrow_t &)
    {
        if (size == 0)
            size = 1;
        size_t alignment = static_cast<size_t>(al);

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            void *p = tc_memalign(alignment, size);
    #elif defined(FASTGRIND_JE_MALLOC)
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
    #if defined(FASTGRIND_TC_MALLOC)
            p = tc_memalign(alignment, size);
    #elif defined(FASTGRIND_JE_MALLOC)
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
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap__ZdlPvSt11align_val_tRKSt9nothrow_t(void *p,
                                                                                             std::align_val_t al,
                                                                                             const std::nothrow_t &)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
        dallocx(p, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete[](void *p, std::align_val_t al, const std::nothrow_t &)
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap__ZdaPvSt11align_val_tRKSt9nothrow_t(void *p,
                                                                                             std::align_val_t al,
                                                                                             const std::nothrow_t &)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free(p);
    #elif defined(FASTGRIND_JE_MALLOC)
        dallocx(p, 0);
    #else
        __real_free(p);
    #endif
    }

    // override operator delete(void *p, size_t sz, std::align_val_t al, const std::nothrow_t &)
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap__ZdlPvmSt11align_val_tRKSt9nothrow_t(void *p, size_t sz,
                                                                                              std::align_val_t al,
                                                                                              const std::nothrow_t &)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(FASTGRIND_JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
        (void) sz;
    }

    // override operator delete[](void *p, size_t sz, std::align_val_t al, const std::nothrow_t &)
    MEM_NO_INSTRUMENT MEM_INLINE_USED inline void __wrap__ZdaPvmSt11align_val_tRKSt9nothrow_t(void *p, size_t sz,
                                                                                              std::align_val_t al,
                                                                                              const std::nothrow_t &)
    {
        (void) al;

        if (__mem_in_probe_)
        {
    #if defined(FASTGRIND_TC_MALLOC)
            tc_free_sized(p, sz);
    #elif defined(FASTGRIND_JE_MALLOC)
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

    #if defined(FASTGRIND_TC_MALLOC)
        tc_free_sized(p, sz);
    #elif defined(FASTGRIND_JE_MALLOC)
        je_sdallocx_default(p, sz, 0);
    #else
        __real_free(p);
    #endif
        (void) sz;
    }

#endif

    /**
     * @brief Collection of function pointers to all wrapper hooks provided.
     * @details Useful for tools wanting to verify symbol interposition or to iterate over wrappers.
     */
    __attribute__((unused)) static const void *mmProbeOverrideFunc[] = {
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

#if defined(FASTGRIND_INSTRUMENT)
MEM_NO_INSTRUMENT static const char *demangleFunc(const char *mangled)
{
    if (!mangled)
        return mangled;

    struct protect
    {
        ~protect()
        {
            cache.clear();
        }
        std::unordered_map<const char *, const char *> cache;
        std::mutex lk;
    };

    static protect p;
    {
        std::lock_guard<std::mutex> g(p.lk);
        auto it = p.cache.find(mangled);
        if (it != p.cache.end())
            return it->second;
    }

    int status = 0;
    char *tmp = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
    const char *ret = nullptr;

    if (status == 0 && tmp)
    {
        ret = strdup(tmp);
        free(tmp);
    }
    else
    {
        ret = mangled;
    }

    {
        std::lock_guard<std::mutex> g(p.lk);
        p.cache[mangled] = ret;
    }
    return ret;
}

MEM_NO_INSTRUMENT static const char *tryAddr2lineResolve(void *addr)
{
    struct protect
    {
        ~protect()
        {
            exe_path_initialized = false;
            memset(exe_path, 0, sizeof(exe_path));
        }
        char exe_path[1024] = {0};
        bool exe_path_initialized = false;
    };

    static protect p;
    char *exe_path = p.exe_path;
    bool &exe_path_initialized = p.exe_path_initialized;

    if (!exe_path_initialized)
    {
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len > 0)
        {
            exe_path[len] = '\0';
            exe_path_initialized = true;
        }
        else
        {
            return nullptr;
        }
    }

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "addr2line -f -C -e %s %p 2>/dev/null", exe_path, addr);

    FILE *fp = popen(cmd, "r");
    if (!fp)
        return nullptr;

    char function_name[512] = {0};
    char file_location[512] = {0};

    if (fgets(function_name, sizeof(function_name), fp))
    {
        char *newline = strchr(function_name, '\n');
        if (newline)
            *newline = '\0';

        if (fgets(file_location, sizeof(file_location), fp))
        {
            newline = strchr(file_location, '\n');
            if (newline)
                *newline = '\0';

            if (strcmp(function_name, "??") != 0)
            {
                const char *filename = strrchr(file_location, '/');
                filename = filename ? filename + 1 : file_location;

                std::string result;
                if (strcmp(file_location, "??:0") != 0 && strlen(filename) > 0)
                {
                    result = memFormat("%s [%s]", function_name, filename);
                }
                else
                {
                    result = std::string(function_name);
                }

                pclose(fp);
                return strdup(result.c_str());
            }
        }
    }

    pclose(fp);
    return nullptr;
}

MEM_NO_INSTRUMENT static std::string beautifySymbolName(const char *symbol)
{
    if (!symbol)
        return "<null>";

    std::string name(symbol);

    if (name.find("lambda") != std::string::npos)
    {
        size_t pos = name.find("::");
        if (pos != std::string::npos)
        {
            std::string container = name.substr(0, pos);
            return memFormat("<lambda in %s>", container.c_str());
        }
        return "<lambda>";
    }

    if (name.find('<') != std::string::npos)
    {
        size_t template_start = name.find('<');
        size_t template_end = name.rfind('>');

        if (template_end != std::string::npos && template_end > template_start)
        {
            std::string base_name = name.substr(0, template_start);
            std::string template_params = name.substr(template_start + 1, template_end - template_start - 1);

            if (template_params.length() > 128)
            {
                template_params = template_params.substr(0, 128) + "...";
            }

            return memFormat("%s<%s>", base_name.c_str(), template_params.c_str());
        }
    }

    return name;
}

MEM_NO_INSTRUMENT static const char *enhancedSymbolResolve(void *addr)
{
    struct protect
    {
        ~protect()
        {
            resolve_cache.clear();
        }
        std::mutex resolve_mutex;
        std::unordered_map<void *, const char *> resolve_cache;
    };

    static protect p;
    std::mutex &resolve_mutex = p.resolve_mutex;
    std::unordered_map<void *, const char *> &resolve_cache = p.resolve_cache;
    {
        std::lock_guard<std::mutex> g(resolve_mutex);
        auto it = resolve_cache.find(addr);
        if (it != resolve_cache.end())
            return it->second;
    }

    const char *result = nullptr;

    Dl_info info;
    if (dladdr(addr, &info))
    {
        if (info.dli_sname && info.dli_saddr == addr)
        {
            const char *demangled = demangleFunc(info.dli_sname);
            std::string beautified = beautifySymbolName(demangled);
            result = strdup(beautified.c_str());
        }
        else if (info.dli_sname)
        {
            ptrdiff_t offset = (char *) addr - (char *) info.dli_saddr;
            if (offset >= 0 && offset < 0x10000)
            {
                const char *demangled = demangleFunc(info.dli_sname);
                std::string beautified = beautifySymbolName(demangled);
                if (offset == 0)
                {
                    result = strdup(beautified.c_str());
                }
                else
                {
                    std::string with_offset = memFormat("%s+0x%lx", beautified.c_str(), offset);
                    result = strdup(with_offset.c_str());
                }
            }
        }

        if (!result && info.dli_fname)
        {
            const char *basename = strrchr(info.dli_fname, '/');
            basename = basename ? basename + 1 : info.dli_fname;

            ptrdiff_t module_offset = (char *) addr - (char *) info.dli_fbase;
            std::string module_info = memFormat("%s+0x%lx", basename, module_offset);
            result = strdup(module_info.c_str());
        }
    }

    if (!result)
    {
        result = tryAddr2lineResolve(addr);
        if (result)
        {
            std::string beautified = beautifySymbolName(result);
            free((void *) result);
            result = strdup(beautified.c_str());
        }
    }

    if (!result)
    {
        try
        {
            if (addr && ((uintptr_t) addr % sizeof(void *)) == 0)
            {
                void **potential_func_ptr = static_cast<void **>(addr);
                void *func_addr = *potential_func_ptr;

                Dl_info ptr_info;
                if (dladdr(func_addr, &ptr_info) && ptr_info.dli_sname)
                {
                    const char *demangled = demangleFunc(ptr_info.dli_sname);
                    std::string beautified = beautifySymbolName(demangled);
                    std::string indirect_info = memFormat("*(%s)", beautified.c_str());
                    result = strdup(indirect_info.c_str());
                }
            }
        }
        catch (...)
        {
        }
    }

    if (!result)
    {
        std::string fallback = memFormat("<unresolved@%p>", addr);
        result = strdup(fallback.c_str());
    }

    {
        std::lock_guard<std::mutex> g(resolve_mutex);
        resolve_cache[addr] = result;
    }

    return result;
}
#endif
} // namespace __FASTGRIND__

#endif
