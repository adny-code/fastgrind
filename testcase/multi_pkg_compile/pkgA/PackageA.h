#pragma once

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

class PackageA
{
  public:
    PackageA();

    ~PackageA();

    void allocTest() const;

  private:
    int *buffer_ = nullptr;
};
