#ifndef MEM_PROBE_H
#define MEM_PROBE_H

#define JE_MALLOC

#include <array>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

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

struct memFrame {
    memFrame() {}
    memFrame(size_t mallocBytes,
             size_t freeBytes,
             size_t funcId,
             size_t frameId)
        : mallocBytes(mallocBytes),
          freeBytes(freeBytes),
          funcId(funcId),
          frameId(frameId) {}

    memFrame& operator+=(const memFrame& other) {
        mallocBytes += other.mallocBytes;
        freeBytes += other.freeBytes;
        funcId += other.funcId;
        frameId += other.frameId;

        return *this;
    }

    size_t mallocBytes;
    size_t freeBytes;
    size_t funcId;
    size_t frameId;
};

class memTimer {
   public:
    memTimer() : _tick(0) {
        _thread = new std::thread(std::bind(&memTimer::fakeTimer, this));
    }
    ~memTimer() { stop(); };

    void stop() {
        _exit = true;
        if (_thread) {
            _thread->join();
            delete _thread;
            _thread = nullptr;
        }
    }

    size_t time() const { return _tick; }

   protected:
    void fakeTimer() {
        while (!_exit) {
            usleep(1000 * SMAMPLE_INTERVAL_MS);
            _tick += SMAMPLE_INTERVAL_MS;
        };
    }

   protected:
    bool _exit = false;
    std::thread* _thread = nullptr;
    std::atomic<size_t> _tick;
};

class memLocalInfo;
class memGlobalInfo {
    friend class memLocalInfo;

   public:
    ~memGlobalInfo() { dump(); }

    static memGlobalInfo& instance() {
        if (!_instance) {
            _instance = std::make_unique<memGlobalInfo>();
        }

        return *_instance.get();
    }

    size_t time() const { return _timer.time(); }

    void dump() const {
        printf("Func Memory Info\n");
        unsigned count = 0;
        for (auto& [tid, threadsInfo] : _frames) {
            for (auto& [frameId, tickInfo] : threadsInfo) {
                memFrame frame0(0, 0, tickInfo.begin()->second.funcId,
                                tickInfo.begin()->second.frameId);
                for (auto& [tick, frame] : tickInfo) {
                    frame0 += frame;
                }

                printf("  %u: threadId:%lu %s: alloc %lu free %lu\n", count++,
                       tid, getCallstack(frameId).c_str(), frame0.mallocBytes,
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

    std::string getCallstack(size_t frameId) const {
        if (_callstacks.find(frameId) == _callstacks.end())
            return "";

        std::string r;
        auto& callstack = _callstacks.at(frameId);
        for (unsigned c = 0; callstack[c] != nullptr; ++c)
            r += std::string(c > 0 ? "->" : "") + std::string(callstack[c]) +
                 "() ";

        return r;
    }

   protected:
    memTimer _timer;

    // key is threadId, second map key is frameId, second key is tick second is
    // frame info
    std::map<size_t,
             std::unordered_map<size_t, std::unordered_map<size_t, memFrame>>>
        _frames;

    std::map<size_t, std::array<const char*, MAX_STACK_DEPTH>> _callstacks;
    std::mutex _lk;

   private:
    static std::unique_ptr<memGlobalInfo> _instance;
};

inline std::unique_ptr<memGlobalInfo> memGlobalInfo::_instance = nullptr;

class memStack {
   public:
    void push(const char* v) {
        _stack[_offset++] = v;
        _stackId += size_t(v);
    }
    const char* pop() {
        auto v = _stack[--_offset];
        _stackId -= size_t(v);
        return v;
    }

    unsigned depth() const { return _offset; }
    const char* top() const { return _stack[_offset - 1]; }

    size_t frameId() const { return size_t(top()) + _stackId; }
    const std::array<const char*, MAX_STACK_DEPTH>& stack() const {
        return _stack;
    }

    static memStack& instance() {
        thread_local memStack __memStack__;
        return __memStack__;
    }

   protected:
    std::array<const char*, MAX_STACK_DEPTH> _stack;
    size_t _offset = 0;
    size_t _stackId = 0;
};

static thread_local size_t tid = syscall(SYS_gettid);

class memLocalInfo
    : public std::unordered_map<size_t /*frameId*/,
                                std::unordered_map<size_t /*tick*/, memFrame>> {
   public:
    ~memLocalInfo() { merge(); }

    static memLocalInfo& instance() {
        thread_local memLocalInfo __memThreadInfo__;
        return __memThreadInfo__;
    }

    void reset() {
        _frames.clear();
        _callstacks.clear();
    }

    void merge() {
        std::lock_guard<std::mutex> lg(memGlobalInfo::instance()._lk);
        memGlobalInfo::instance()._frames[tid].merge(_frames);

        for (auto& [k, v] : _callstacks)
            memGlobalInfo::instance()._callstacks[k] = v;

        reset();
    }

    void add(size_t sz) {
        if (_nested || memStack::instance().depth() == 0)
            return;

        ++_nested;
        const auto& stack = memStack::instance();
        getFrame(size_t(stack.top()), stack.frameId(),
                 memGlobalInfo::instance().time())
            .mallocBytes += sz;
        --_nested;
    }

    void sub(size_t sz) {
        if (_nested || memStack::instance().depth() == 0)
            return;

        ++_nested;
        const auto& stack = memStack::instance();
        getFrame(size_t(stack.top()), stack.frameId(),
                 memGlobalInfo::instance().time())
            .freeBytes += sz;
        --_nested;
    }

   protected:
    memFrame& getFrame(size_t funcId, size_t frameId, size_t time) {
        if (_callstacks.find(frameId) == _callstacks.end()) {
            _callstacks[frameId] = memStack::instance().stack();
            _callstacks[frameId][memStack::instance().depth()] = nullptr;
        }

        auto& frames = _frames[frameId];
        auto it = frames.find(time);
        if (it == frames.end()) {
            frames[time] = memFrame(0, 0, funcId, frameId);
            return frames[time];
        } else {
            return it->second;
        }
    }

   protected:
    // key is frameId, second map key is tick
    std::unordered_map<size_t, std::unordered_map<size_t, memFrame>> _frames;
    std::unordered_map<size_t, std::array<const char*, MAX_STACK_DEPTH>>
        _callstacks;
    int _nested = 0;
};

class memProbe {
   public:
    memProbe(const char* name) { memStack::instance().push(name); }
    ~memProbe() { memStack::instance().pop(); }
};

#define MEM_PROBE memProbe __probe__(__FUNCTION__);

#endif
