#include "PackageC.h"
#include "memProbe.h"
#include "pkgA/PackageA.h"
#include "pkgB/PackageB.h"
#include <cstdlib>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

PackageC::PackageC()
{
    b_ = new PackageB();
}

PackageC::~PackageC()
{
    delete b_;
}

void PackageC::allocTest() const
{
    allocBool();
    allocChar();
    allocInt();
    allocFloat();
    allocDouble();
    allocLong();
    allocLongLong();
    allocPair();
    allocTuple();
    allocUniquePtr();
    allocSharedPtr();
    allocWeakPtr();
    allocString();
    allocList();
    allocVector();
    allocDeque();
    allocSet();
    allocUnorderedSet();
    allocMap();
    allocUnorderedMap();
    allocMultiMap();
    allocMultiUnorderedMap();
}

void PackageC::allocBool() const
{
    bool *tmp = new bool;
    delete tmp;
}

void PackageC::allocChar() const
{
    char *tmp = new char;
    delete tmp;
}

void PackageC::allocInt() const
{
    int *tmp = new int;
    delete tmp;
}

void PackageC::allocFloat() const
{
    float *tmp = new float;
    delete tmp;
}

void PackageC::allocDouble() const
{
    double *tmp = new double;
    delete tmp;
}

void PackageC::allocLong() const
{
    long *tmp = new long;
    delete tmp;
}

void PackageC::allocLongLong() const
{
    long long *tmp = new long long;
    delete tmp;
}

void PackageC::allocPair() const
{
    auto *p = new std::pair<int, int>(1, 2);
    delete p;
}

void PackageC::allocTuple() const
{
    auto *t = new std::tuple<int, char, double>(1, 'x', 3.14);
    delete t;
}

void PackageC::allocUniquePtr() const
{
    std::unique_ptr<int> p(new int(0));
}

void PackageC::allocSharedPtr() const
{
    std::shared_ptr<int> p = std::make_shared<int>(0);
}

void PackageC::allocWeakPtr() const
{
    std::shared_ptr<int> sp = std::make_shared<int>(0);
    std::weak_ptr<int> wp(sp);
    (void)wp.lock();
}

void PackageC::allocString() const
{
    std::string tmp;
    for (int i = 0; i < 10000; ++i)
    {
        tmp += "x";
    }
}

void PackageC::allocList() const
{
    std::list<int> l;
    for (int i = 0; i < 10000; ++i)
    {
        l.push_back(i);
    }
}

void PackageC::allocVector() const
{
    std::vector<int> v;
    for (int i = 0; i < 10000; ++i)
    {
        v.push_back(i);
    }
}

void PackageC::allocDeque() const
{
    std::deque<int> d;
    for (int i = 0; i < 10000; ++i)
    {
        d.push_back(i);
    }
}

void PackageC::allocSet() const
{
    std::set<int> s;
    for (int i = 0; i < 10000; ++i)
    {
        s.insert(i);
    }
}

void PackageC::allocUnorderedSet() const
{
    std::unordered_set<int> s;
    for (int i = 0; i < 10000; ++i)
    {
        s.insert(i);
    }
}

void PackageC::allocMap() const
{
    std::map<int, int> m;
    for (int i = 0; i < 10000; ++i)
    {
        m[i] = i;
    }
}

void PackageC::allocUnorderedMap() const
{
    std::unordered_map<int, int> m;
    for (int i = 0; i < 10000; ++i)
    {
        m[i] = i;
    }
}

void PackageC::allocMultiMap() const
{
    std::multimap<int, int> mm;
    for (int i = 0; i < 10000; ++i)
    {
        mm.insert({i, i});
    }
}

void PackageC::allocMultiUnorderedMap() const
{
    std::unordered_multimap<int, int> mm;
    for (int i = 0; i < 10000; ++i)
    {
        mm.insert({i, i});
    }
}