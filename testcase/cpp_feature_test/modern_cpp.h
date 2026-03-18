#ifndef MODERN_CPP_H
#define MODERN_CPP_H

#include "cpp_feature_compat.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <future>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#if defined(FASTGRIND_TESTCASE_HAS_CXX17)
    #include <any>
    #include <optional>
    #include <variant>
#endif

namespace test_modern_cpp
{

void testAutoKeyword();

constexpr int factorial(int n)
{
    return (n <= 1) ? 1 : n * factorial(n - 1);
}

#if defined(FASTGRIND_TESTCASE_HAS_CXX14)
constexpr bool isPrime(int n)
#else
inline bool isPrime(int n)
#endif
{
    if (n <= 1)
        return false;
    if (n <= 3)
        return true;
    if (n % 2 == 0 || n % 3 == 0)
        return false;

    for (int i = 5; i * i <= n; i += 6)
    {
        if (n % i == 0 || n % (i + 2) == 0)
        {
            return false;
        }
    }
    return true;
}

class ConstexprDemo
{
  public:
    constexpr ConstexprDemo(int val) : value_(val)
    {
    }
    constexpr int getValue() const
    {
        return value_;
    }
    constexpr int square() const
    {
        return value_ * value_;
    }

  private:
    int value_;
};

void testConstexpr();

void testLambdaExpressions();

class Resource
{
  private:
    std::string name_;
    int *data_;

  public:
    explicit Resource(const std::string &name, int size = 10) : name_(name), data_(new int[size])
    {
        std::cout << "Resource '" << name_ << "' created" << std::endl;
        for (int i = 0; i < size; ++i)
        {
            data_[i] = i;
        }
    }

    ~Resource()
    {
        std::cout << "Resource '" << name_ << "' destroyed" << std::endl;
        delete[] data_;
    }

    Resource(const Resource &) = delete;
    Resource &operator=(const Resource &) = delete;

    Resource(Resource &&other) noexcept : name_(std::move(other.name_)), data_(other.data_)
    {
        other.data_ = nullptr;
        std::cout << "Resource '" << name_ << "' moved" << std::endl;
    }

    Resource &operator=(Resource &&other) noexcept
    {
        if (this != &other)
        {
            delete[] data_;
            name_ = std::move(other.name_);
            data_ = other.data_;
            other.data_ = nullptr;
        }
        return *this;
    }

    const std::string &getName() const
    {
        return name_;
    }
    void processData() const
    {
        std::cout << "Processing data in resource '" << name_ << "'" << std::endl;
    }
};

void testSmartPointers();

class MoveableClass
{
  private:
    std::string name_;
    std::vector<int> data_;

  public:
    MoveableClass(const std::string &name, std::initializer_list<int> values) : name_(name), data_(values)
    {
        std::cout << "MoveableClass '" << name_ << "' constructed" << std::endl;
    }

    MoveableClass(const MoveableClass &other) : name_(other.name_ + "_copy"), data_(other.data_)
    {
        std::cout << "MoveableClass '" << name_ << "' copy constructed" << std::endl;
    }

    MoveableClass(MoveableClass &&other) noexcept : name_(std::move(other.name_)), data_(std::move(other.data_))
    {
        std::cout << "MoveableClass '" << name_ << "' move constructed" << std::endl;
    }

    MoveableClass &operator=(const MoveableClass &other)
    {
        if (this != &other)
        {
            name_ = other.name_ + "_copy_assigned";
            data_ = other.data_;
            std::cout << "MoveableClass '" << name_ << "' copy assigned" << std::endl;
        }
        return *this;
    }

    MoveableClass &operator=(MoveableClass &&other) noexcept
    {
        if (this != &other)
        {
            name_ = std::move(other.name_);
            data_ = std::move(other.data_);
            std::cout << "MoveableClass '" << name_ << "' move assigned" << std::endl;
        }
        return *this;
    }

    ~MoveableClass()
    {
        std::cout << "MoveableClass '" << name_ << "' destroyed" << std::endl;
    }

    const std::string &getName() const
    {
        return name_;
    }
    size_t getSize() const
    {
        return data_.size();
    }

    void printData() const
    {
        std::cout << name_ << " data: ";
        for (int val : data_)
        {
            std::cout << val << " ";
        }
        std::cout << "(size: " << data_.size() << ")" << std::endl;
    }
};

void testMoveSemantics();

void testRangeForAndInitLists();

enum class Color : int
{
    RED = 1,
    GREEN = 2,
    BLUE = 3,
    ALPHA = 4
};

enum class Status
{
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED
};

void testNullptrAndEnums();

class BaseConstruct
{
  protected:
    std::string name_;
    int value_;

  public:
    BaseConstruct(const std::string &name, int value) : name_(name), value_(value)
    {
        std::cout << "BaseConstruct: " << name_ << ", " << value_ << std::endl;
    }

    virtual ~BaseConstruct() = default;

    virtual void display() const
    {
        std::cout << "Base: " << name_ << " = " << value_ << std::endl;
    }
};

class DerivedConstruct : public BaseConstruct
{
  public:
    using BaseConstruct::BaseConstruct;

    DerivedConstruct() : DerivedConstruct("default", 0)
    {
    }

    DerivedConstruct(const std::string &name, int value, double extra) : BaseConstruct(name, value), extra_(extra)
    {
        std::cout << "DerivedConstruct: extra = " << extra_ << std::endl;
    }

    void display() const override
    {
        BaseConstruct::display();
        std::cout << "  Extra: " << extra_ << std::endl;
    }

  private:
    double extra_ = 1.0;
};

void testConstructorFeatures();

void testThreadsAndAsync();

void testCpp17Features();

void runAllModernCppTests();

} // namespace test_modern_cpp

#endif