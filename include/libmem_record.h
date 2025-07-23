#ifndef MEM_RECORDER_H
#define MEM_RECORDER_H

#include <atomic>
#include <array>

struct memRecInfo {
public:
    size_t mallocSize;
    size_t freeSize;
};

class memRecorderMain {
public:
	memRecorderMain() {}

	~memRecorderMain() { dump(); }


	static memRecorderMain& instance() {
		static memRecorderMain recMain;
		return recMain;
	}

	void dump() {}

	size_t time() const { return _time; }

protected:
	std::atomic<size_t>	_time;
};

class memRecorderThread {
public:
	memRecorderThread() {}
	~memRecorderThread() { reportToMain(); }

	static memRecorderThread& instance() {
		thread_local memRecorderThread recThread;
		return recThread;
	}

protected:

	void reportToMain() {}

protected:
	unsigned _stackIdx = 0;
	std::array<memRecInfo, 1024> _recs;

    // key is name hash
    std::unordered_map<size_t, memRecInfo> _recs;
};

class memRecorder {
public:
	memRecorder(const char* funcName) {
		memRecorderThread::instance().push(funcName);
	}
	~memRecorder() {
		memRecorderThread::instance().pop();
	}

    void add(size_t sz) {}
    void sub(size_t sz) {}

protected:
};

#define MEM_RECORD memRecorder __rec__(__FUNCTION__);

void* malloc(size_t sz) {
	auto ptr = ::malloc(sz);
	memRecorderThread::instance().add(malloc_usable_size(ptr));
	return ptr;
}


void free(void* ptr) {
	memRecorderThread::instance().sub(malloc_usable_size(ptr));
	return ::free(ptr);
}

#endif
