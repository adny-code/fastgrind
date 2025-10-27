#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <cstdio>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
template <typename... Args>
static std::string strFormat(const char* format, const Args&... args) {
    size_t size =
        1 + snprintf(nullptr, 0, format, std::forward<const Args&>(args)...);
    char* bytes = new char[size];
    snprintf(bytes, size, format, args...);
    std::string out = std::string(bytes);
    delete[] bytes;
    return out;
}
#pragma GCC diagnostic pop


#endif