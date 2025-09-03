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
    std::string process(const std::string &in);

  private:
    std::vector<int> buffer_;
};
