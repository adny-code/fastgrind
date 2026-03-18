#include "namespace_test.h"
#include <cmath>
#include <iostream>

namespace basic_namespace
{
int variable1 = 10;
}

namespace outer
{
int outerVar = 100;

namespace inner
{
int innerVar = 200;

namespace deep
{
int deepVar = 300;
}
} // namespace inner
} // namespace outer

int globalVariable = 999;

namespace basic_namespace
{
void function1()
{
    std::cout << "basic_namespace::function1() called" << std::endl;
    std::cout << "Accessing variable1: " << variable1 << std::endl;
}

void BasicClass::method()
{
    std::cout << "BasicClass::method() called" << std::endl;
}
} // namespace basic_namespace

namespace outer
{
void outerFunction()
{
    std::cout << "outer::outerFunction() called, outerVar = " << outerVar << std::endl;
}

namespace inner
{
void innerFunction()
{
    std::cout << "outer::inner::innerFunction() called" << std::endl;
    std::cout << "innerVar = " << innerVar << ", outerVar = " << outerVar << std::endl;
}

namespace deep
{
void deepFunction()
{
    std::cout << "outer::inner::deep::deepFunction() called" << std::endl;
    std::cout << "deepVar = " << deepVar << std::endl;
    std::cout << "innerVar = " << innerVar << std::endl;
    std::cout << "outerVar = " << outerVar << std::endl;
}
} // namespace deep
} // namespace inner
} // namespace outer

#if defined(FASTGRIND_TESTCASE_HAS_CXX17)
namespace company::product::version
{
std::string getVersion()
{
    return "1.0.0";
}

void ProductInfo::displayInfo()
{
    std::cout << "Product: CompanyProduct, Version: " << getVersion() << std::endl;
}
} // namespace company::product::version

namespace very_long_company_name::very_long_product_name::very_long_version_name
{
void doSomething()
{
    std::cout << "Long namespace function executed" << std::endl;
}

void LongNamedClass::process()
{
    std::cout << "LongNamedClass::process() called" << std::endl;
}
} // namespace very_long_company_name::very_long_product_name::very_long_version_name
#else
namespace company
{
namespace product
{
namespace version
{
std::string getVersion()
{
    return "1.0.0";
}

void ProductInfo::displayInfo()
{
    std::cout << "Product: CompanyProduct, Version: " << getVersion() << std::endl;
}
} // namespace version
} // namespace product
} // namespace company

namespace very_long_company_name
{
namespace very_long_product_name
{
namespace very_long_version_name
{
void doSomething()
{
    std::cout << "Long namespace function executed" << std::endl;
}

void LongNamedClass::process()
{
    std::cout << "LongNamedClass::process() called" << std::endl;
}
} // namespace very_long_version_name
} // namespace very_long_product_name
} // namespace very_long_company_name
#endif

namespace math_utils
{
double sin(double x)
{
    return std::sin(x);
}

double cos(double x)
{
    return std::cos(x);
}

double log(double x)
{
    return std::log(x);
}

namespace advanced
{
double gamma(double x)
{

    return std::exp(std::lgamma(x));
}

double beta(double a, double b)
{

    return std::exp(std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b));
}
} // namespace advanced
} // namespace math_utils

namespace graphics
{
void Point::draw() const
{
    std::cout << "Drawing graphics::Point at (" << x_ << ", " << y_ << ")" << std::endl;
}

void draw(const Point &p)
{
    std::cout << "Graphics draw function for point at (" << p.getX() << ", " << p.getY() << ")" << std::endl;
}
} // namespace graphics

namespace geometry
{
void Point::print() const
{
    std::cout << "geometry::Point(" << x_ << ", " << y_ << ")" << std::endl;
}

double distance(const Point &p1, const Point &p2)
{
    int dx = p1.getX() - p2.getX();
    int dy = p1.getY() - p2.getY();
    return std::sqrt(dx * dx + dy * dy);
}
} // namespace geometry

namespace adl_test
{
void process(const MyClass &obj)
{
    std::cout << "ADL process function called with value: " << obj.getValue() << std::endl;
}

MyClass operator+(const MyClass &a, const MyClass &b)
{
    return MyClass(a.getValue() + b.getValue());
}

std::ostream &operator<<(std::ostream &os, const MyClass &obj)
{
    os << "MyClass(" << obj.getValue() << ")";
    return os;
}
} // namespace adl_test

namespace library
{
inline namespace v2
{
void newFunction()
{
    std::cout << "New function from v2 (inline namespace)" << std::endl;
}

void NewClass::newMethod()
{
    std::cout << "NewClass::newMethod() from v2" << std::endl;
}
} // namespace v2

namespace v1
{
void oldFunction()
{
    std::cout << "Old function from v1" << std::endl;
}

void OldClass::oldMethod()
{
    std::cout << "OldClass::oldMethod() from v1" << std::endl;
}
} // namespace v1
} // namespace library

namespace template_namespace
{
template <typename T> void processContainer(Container<T> &container)
{
    std::cout << "Processing container with " << container.size() << " elements:" << std::endl;
    for (auto &item : container)
    {
        std::cout << "  " << item << std::endl;
    }
}

template void processContainer<int>(Container<int> &);
template void processContainer<std::string>(Container<std::string> &);
} // namespace template_namespace

void globalFunction()
{
    std::cout << "Global function called, globalVariable = " << globalVariable << std::endl;
}

void testBasicNamespace()
{
    std::cout << "\n=== Testing Basic Namespace ===" << std::endl;

    basic_namespace::function1();

    std::cout << "basic_namespace::variable1 = " << basic_namespace::variable1 << std::endl;

    basic_namespace::BasicClass obj;
    obj.method();

    basic_namespace::variable1 = 20;
    std::cout << "After modification: " << basic_namespace::variable1 << std::endl;
}

void testNestedNamespace()
{
    std::cout << "\n=== Testing Nested Namespace ===" << std::endl;

    outer::outerFunction();

    outer::inner::innerFunction();

    outer::inner::deep::deepFunction();

    std::cout << "\nTesting nested namespace access:" << std::endl;
    std::cout << "Version: " << company::product::version::getVersion() << std::endl;

    company::product::version::ProductInfo info;
    info.displayInfo();

#if !defined(FASTGRIND_TESTCASE_HAS_CXX17)
    std::cout << "C++17 nested namespace syntax is unavailable in this build; using equivalent C++11 namespace blocks." << std::endl;
#endif
}

void testAnonymousNamespace()
{
    std::cout << "\n=== Testing Anonymous Namespace ===" << std::endl;

    internalFunction();
    internalFunction();
    internalFunction();

    std::cout << "Internal counter value: " << internalCounter << std::endl;
}

void testNamespaceAlias()
{
    std::cout << "\n=== Testing Namespace Alias ===" << std::endl;

    very_long_company_name::very_long_product_name::very_long_version_name::doSomething();

    std::cout << "\nUsing namespace aliases:" << std::endl;
    short_name::doSomething();

    short_name::LongNamedClass obj;
    obj.process();

    vlc::very_long_product_name::very_long_version_name::doSomething();
}

void testUsingDeclarations()
{
    std::cout << "\n=== Testing Using Declarations ===" << std::endl;

    using math_utils::cos;
    using math_utils::PI;
    using math_utils::sin;

    std::cout << "PI = " << PI << std::endl;
    std::cout << "sin(PI/2) = " << sin(PI / 2) << std::endl;
    std::cout << "cos(PI) = " << cos(PI) << std::endl;

    std::cout << "log(E) = " << math_utils::log(math_utils::E) << std::endl;

    {
        using namespace math_utils::advanced;
        std::cout << "\nUsing advanced math functions:" << std::endl;
        std::cout << "gamma(5) = " << math_utils::advanced::gamma(5) << std::endl;
        std::cout << "beta(2, 3) = " << beta(2, 3) << std::endl;
    }
}

void testNamespaceConflicts()
{
    std::cout << "\n=== Testing Namespace Conflicts ===" << std::endl;

    graphics::Point graphicsPoint(10.5, 20.7);
    geometry::Point geometryPoint(10, 20);

    std::cout << "Graphics point:" << std::endl;
    graphicsPoint.draw();
    graphics::draw(graphicsPoint);

    std::cout << "\nGeometry point:" << std::endl;
    geometryPoint.print();

    geometry::Point point1(0, 0);
    geometry::Point point2(3, 4);
    double dist = geometry::distance(point1, point2);
    std::cout << "Distance between points: " << dist << std::endl;

    std::cout << "\nDemonstrating type differences:" << std::endl;
    std::cout << "Graphics Point coordinates: (" << graphicsPoint.getX() << ", " << graphicsPoint.getY() << ")"
              << std::endl;
    std::cout << "Geometry Point coordinates: (" << geometryPoint.getX() << ", " << geometryPoint.getY() << ")"
              << std::endl;
}

void testADL()
{
    std::cout << "\n=== Testing ADL (Argument-Dependent Lookup) ===" << std::endl;

    adl_test::MyClass obj1(10);
    adl_test::MyClass obj2(20);

    process(obj1);

    adl_test::MyClass result = obj1 + obj2;
    std::cout << "obj1 + obj2 = " << result << std::endl;

    adl_test::process(result);

    std::cout << "\nADL explanation:" << std::endl;
    std::cout << "- Functions in the same namespace as the argument types" << std::endl;
    std::cout << "- Can be found without namespace qualification" << std::endl;
    std::cout << "- This is how std::cout << works with user-defined types" << std::endl;
}

void testInlineNamespace()
{
    std::cout << "\n=== Testing Inline Namespace ===" << std::endl;

    library::newFunction();
    library::NewClass newObj;
    newObj.newMethod();

    library::v2::newFunction();

    library::v1::oldFunction();
    library::v1::OldClass oldObj;
    oldObj.oldMethod();

    std::cout << "\nInline namespace benefits:" << std::endl;
    std::cout << "- Provides versioning mechanism" << std::endl;
    std::cout << "- Allows backward compatibility" << std::endl;
    std::cout << "- Default version is accessible without qualification" << std::endl;
}

void testTemplateNamespace()
{
    std::cout << "\n=== Testing Template Namespace ===" << std::endl;

    template_namespace::Container<int> intContainer;
    intContainer.add(1);
    intContainer.add(2);
    intContainer.add(3);

    template_namespace::Container<std::string> stringContainer;
    stringContainer.add("Hello");
    stringContainer.add("Template");
    stringContainer.add("Namespace");

    std::cout << "Processing int container:" << std::endl;
    template_namespace::processContainer(intContainer);

    std::cout << "\nProcessing string container:" << std::endl;
    template_namespace::processContainer(stringContainer);

    using IntContainer = template_namespace::Container<int>;
    IntContainer anotherIntContainer;
    anotherIntContainer.add(10);
    anotherIntContainer.add(20);

    std::cout << "\nUsing type alias:" << std::endl;
    template_namespace::processContainer(anotherIntContainer);
}

void testGlobalNamespace()
{
    std::cout << "\n=== Testing Global Namespace ===" << std::endl;

    globalFunction();

    std::cout << "Global variable: " << globalVariable << std::endl;

    ::globalFunction();
    std::cout << "::globalVariable = " << ::globalVariable << std::endl;

    {
        int globalVariable = 123;
        std::cout << "Local globalVariable: " << globalVariable << std::endl;
        std::cout << "Global globalVariable: " << ::globalVariable << std::endl;
    }

    std::cout << "\nGlobal namespace operator (::) uses:" << std::endl;
    std::cout << "- Explicitly access global scope" << std::endl;
    std::cout << "- Resolve naming conflicts" << std::endl;
    std::cout << "- Access global functions/variables when shadowed" << std::endl;
}

void runAllNamespaceTests()
{
    std::cout << "\n########################################" << std::endl;
    std::cout << "#   C++ Namespace Features Test Suite #" << std::endl;
    std::cout << "########################################" << std::endl;

    testBasicNamespace();
    testNestedNamespace();
    testAnonymousNamespace();
    testNamespaceAlias();
    testUsingDeclarations();
    testNamespaceConflicts();
    testADL();
    testInlineNamespace();
    testTemplateNamespace();
    testGlobalNamespace();

    std::cout << "\n########################################" << std::endl;
    std::cout << "#   Namespace Tests Completed!        #" << std::endl;
    std::cout << "########################################" << std::endl;
}