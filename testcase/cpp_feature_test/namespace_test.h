#ifndef NAMESPACE_TEST_H
#define NAMESPACE_TEST_H

#include "cpp_feature_compat.h"

#include <iostream>
#include <string>
#include <vector>

namespace basic_namespace
{
void function1();
extern int variable1;

class BasicClass
{
  public:
    void method();
};
} // namespace basic_namespace

namespace outer
{
extern int outerVar;
void outerFunction();

namespace inner
{
extern int innerVar;
void innerFunction();

namespace deep
{
extern int deepVar;
void deepFunction();
} // namespace deep
} // namespace inner
} // namespace outer

#if defined(FASTGRIND_TESTCASE_HAS_CXX17)
namespace company::product::version
{
std::string getVersion();
class ProductInfo
{
  public:
    void displayInfo();
};
} // namespace company::product::version
#else
namespace company
{
namespace product
{
namespace version
{
std::string getVersion();
class ProductInfo
{
  public:
    void displayInfo();
};
} // namespace version
} // namespace product
} // namespace company
#endif

namespace
{

int internalCounter = 0;
void internalFunction()
{
    std::cout << "Internal function called, counter: " << ++internalCounter << std::endl;
}
} // namespace

namespace very_long_company_name
{
namespace very_long_product_name
{
namespace very_long_version_name
{
void doSomething();
class LongNamedClass
{
  public:
    void process();
};
} // namespace very_long_version_name
} // namespace very_long_product_name
} // namespace very_long_company_name

namespace vlc = very_long_company_name;
namespace short_name = very_long_company_name::very_long_product_name::very_long_version_name;

namespace math_utils
{
const double PI = 3.14159265359;
const double E = 2.71828182846;

double sin(double x);
double cos(double x);
double log(double x);

namespace advanced
{
double gamma(double x);
double beta(double a, double b);
} // namespace advanced
} // namespace math_utils

namespace graphics
{
class Point
{
  private:
    double x_, y_;

  public:
    Point(double x, double y) : x_(x), y_(y)
    {
    }
    void draw() const;
    double getX() const
    {
        return x_;
    }
    double getY() const
    {
        return y_;
    }
};

void draw(const Point &p);
} // namespace graphics

namespace geometry
{
class Point
{
  private:
    int x_, y_;

  public:
    Point(int x, int y) : x_(x), y_(y)
    {
    }
    void print() const;
    int getX() const
    {
        return x_;
    }
    int getY() const
    {
        return y_;
    }
};

double distance(const Point &p1, const Point &p2);
} // namespace geometry

namespace adl_test
{
class MyClass
{
  private:
    int value_;

  public:
    explicit MyClass(int value) : value_(value)
    {
    }
    int getValue() const
    {
        return value_;
    }
};

void process(const MyClass &obj);
MyClass operator+(const MyClass &a, const MyClass &b);
std::ostream &operator<<(std::ostream &os, const MyClass &obj);
} // namespace adl_test

namespace library
{
inline namespace v2
{
void newFunction();
class NewClass
{
  public:
    void newMethod();
};
} // namespace v2

namespace v1
{
void oldFunction();
class OldClass
{
  public:
    void oldMethod();
};
} // namespace v1
} // namespace library

namespace template_namespace
{
template <typename T> class Container
{
  private:
    std::vector<T> data_;

  public:
    void add(const T &item)
    {
        data_.push_back(item);
    }
    T &get(size_t index)
    {
        return data_[index];
    }
    size_t size() const
    {
        return data_.size();
    }

    typename std::vector<T>::iterator begin()
    {
        return data_.begin();
    }
    typename std::vector<T>::iterator end()
    {
        return data_.end();
    }
};

template <typename T> void processContainer(Container<T> &container);
} // namespace template_namespace

void globalFunction();
extern int globalVariable;

void testBasicNamespace();
void testNestedNamespace();
void testAnonymousNamespace();
void testNamespaceAlias();
void testUsingDeclarations();
void testNamespaceConflicts();
void testADL();
void testInlineNamespace();
void testTemplateNamespace();
void testGlobalNamespace();
void runAllNamespaceTests();

#endif