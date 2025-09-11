#pragma once

#include <memory>
#include <string>
#include <vector>

class PackageA;

class PackageB
{
  public:
    PackageB();
  
    ~PackageB();

    void allocTest() const;

  private:
    PackageA *a_ = nullptr;
};
