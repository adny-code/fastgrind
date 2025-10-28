#ifndef DATA_H
#define DATA_H

#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "fastgrind.h"

using namespace __FASTGRIND__;

struct memData
{
    memSerializerMap<const char *, memSerializeString> names;
    memSerializerMap<size_t, memSerializerMap<size_t, memNode>> datas;

    const char *getName(const char *nameId) const
    {
        if (names.find(nameId) != names.end())
        {
            return names.at(nameId).c_str();
        }
        else
        {
            return nameId;
        }
    }
};

std::vector<char> readFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        printf("[error] fail to open file '%s'\n", filename.c_str());
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size))
    {
        printf("[error] fail to read all data in file '%s'\n", filename.c_str());
    }

    return buffer;
}

bool loadData(const std::string &path, memData &data)
{
    std::vector<char> buffers = readFile(path);
    size_t pos = 0;
    if (!data.names.unserialize(buffers, pos) || !data.datas.unserialize(buffers, pos))
    {
        return false;
    }

    return true;
}

#endif