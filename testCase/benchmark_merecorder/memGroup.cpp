#include "memGroup.h"
#include <climits>

memGroup::memGroup(unsigned size)
{
    _groups.resize(size, UINT_MAX);
}

unsigned memGroup::getHeader(unsigned index) const
{
    if (index >= _groups.size())
        return UINT_MAX;
    
    while (true) {
        unsigned tmpHeader = _groups[index];
        if (tmpHeader == UINT_MAX || tmpHeader == index)
            return index;
        
        index = tmpHeader;
    }
}

void memGroup::add(std::set<unsigned> groups)
{
    if (groups.empty())
        return;

    unsigned header = getHeader(*groups.begin());
    for (auto it = std::next(groups.begin()); it != groups.end(); ++it)
    {
        unsigned tmpHeader = getHeader(*it);
        if (tmpHeader < header)
            header = tmpHeader;
    }

    for (auto it = groups.begin(); it != groups.end(); ++it) {
        _groups[*it] = header;
        _cache[*it] = groups;
    }
}

void memGroup::getGroups(unsigned index, std::deque<unsigned> &groups) const
{
    if (index >= _groups.size())
        return;

    unsigned header = getHeader(index);
    for (auto& it : _cache) {
        if (getHeader(it.first) == header) {
            for (auto& item : it.second) {
                groups.push_back(item);
            }
        }
    }