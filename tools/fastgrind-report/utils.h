#ifndef UTILS_H
#define UTILS_H

#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <locale>
#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
template <typename... Args> static std::string strFormat(const char *format, const Args &...args)
{
    size_t size = 1 + snprintf(nullptr, 0, format, std::forward<const Args &>(args)...);
    char *bytes = new char[size];
    snprintf(bytes, size, format, args...);
    std::string out = std::string(bytes);
    delete[] bytes;
    return out;
}
#pragma GCC diagnostic pop

std::string comma(const std::string &s)
{
    if (s == "" || ((s.front() == '+' || s.front() == '-') && s.size() <= 4) || (s.size() <= 3))
    {
        return s;
    }

    int start = s.size() % 3;
    std::string r = s.substr(0, start);
    for (int i = 0; start + i < s.size(); ++i)
    {
        if (i % 3 == 0)
        {
            r += ",";
        }

        r.push_back(s.at(i));
    }

    return r;
}

#endif