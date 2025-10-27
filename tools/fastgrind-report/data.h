#ifndef DATA_H
#define DATA_H

#include <map>
#include <unordered_map>
#include <string>

struct memFrame
{
    /**
     * @brief Default constructor for memFrame.
     * @details Initializes all counters to zero.
     */
    memFrame() : mallocBytes(0), freeBytes(0), funcId(0), frameId(0)
    {
    }

    /**
     * @brief Construct a new memFrame object with specified values.
     * @param mallocBytes Total bytes allocated in this frame
     * @param freeBytes Total bytes freed in this frame
     * @param funcId Function identifier for this frame
     * @param frameId Stack frame identifier
     */
    memFrame(size_t mallocBytes, size_t freeBytes, size_t funcId, size_t frameId)
        : mallocBytes(mallocBytes), freeBytes(freeBytes), funcId(funcId), frameId(frameId)
    {
    }

    ~memFrame()
    {
    }

    memFrame(const memFrame &o)
        : mallocBytes(o.mallocBytes), freeBytes(o.freeBytes), funcId(o.funcId), frameId(o.frameId)
    {
    }

    memFrame(memFrame &&o) noexcept
        : mallocBytes(o.mallocBytes), freeBytes(o.freeBytes), funcId(o.funcId), frameId(o.frameId)
    {
    }

    memFrame &operator=(const memFrame &o)
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

    memFrame &operator=(memFrame &&o) noexcept
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

    size_t mallocBytes; ///< Total bytes allocated during this frame's lifetime
    size_t freeBytes;   ///< Total bytes freed during this frame's lifetime
    size_t funcId;      ///< Unique identifier for the function
    size_t frameId;     ///< Unique identifier for the call stack frame
};

bool loadData(const std::string& path, std::map<size_t, std::unordered_map<size_t, std::unordered_map<size_t, memFrame>>> &frames)
{
    return true;
}

#endif