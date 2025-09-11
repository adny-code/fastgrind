#pragma once

#include <map>
#include <string>

class PackageB;

class PackageC
{
  public:
    PackageC();

    ~PackageC();

    void allocTest() const;

  protected:
    void allocBool() const;

    void allocChar() const;

    void allocInt() const;

    void allocFloat() const;

    void allocDouble() const;

    void allocLong() const;

    void allocLongLong() const;

    void allocPair() const;

    void allocTuple() const;

    void allocUniquePtr() const;

    void allocSharedPtr() const;

    void allocWeakPtr() const;

    void allocString() const;

    void allocList() const;

    void allocVector() const;

    void allocDeque() const;

    void allocSet() const;

    void allocUnorderedSet() const;

    void allocMap() const;

    void allocUnorderedMap() const;

    void allocMultiMap() const;

    void allocMultiUnorderedMap() const;

  private:
    PackageB *b_ = nullptr;
};
