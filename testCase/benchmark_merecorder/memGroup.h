#include <deque>
#include <vector>
#include <set>
#include <map>

class memGroup
{
  public:
    memGroup(unsigned size);

    void add(std::set<unsigned> groups);

    unsigned getHeader(unsigned index) const;

    void getGroups(unsigned index, std::deque<unsigned>& groups) const;

  protected:
    std::vector<unsigned> _groups;
    std::map<unsigned, std::set<unsigned>> _cache;
};