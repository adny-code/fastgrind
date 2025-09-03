#pragma once

#include <memory>
#include <string>
#include <vector>

class PackageA;

class PackageB
{
  public:
    PackageB();
    std::string combine(PackageA &a, const std::string &msg);

  private:
    std::unique_ptr<int[]> data_;
};
