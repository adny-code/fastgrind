#include "namespace_test.h"
#include <iostream>
#include <cmath>

// 定义命名空间中的变量
namespace basic_namespace {
    int variable1 = 10;
}

namespace outer {
    int outerVar = 100;
    
    namespace inner {
        int innerVar = 200;
        
        namespace deep {
            int deepVar = 300;
        }
    }
}

// 定义全局变量
int globalVariable = 999;

// 实现基础命名空间中的函数
namespace basic_namespace {
    void function1() {
        std::cout << "basic_namespace::function1() called" << std::endl;
        std::cout << "Accessing variable1: " << variable1 << std::endl;
    }
    
    void BasicClass::method() {
        std::cout << "BasicClass::method() called" << std::endl;
    }
}

// 实现嵌套命名空间中的函数
namespace outer {
    void outerFunction() {
        std::cout << "outer::outerFunction() called, outerVar = " << outerVar << std::endl;
    }
    
    namespace inner {
        void innerFunction() {
            std::cout << "outer::inner::innerFunction() called" << std::endl;
            std::cout << "innerVar = " << innerVar << ", outerVar = " << outerVar << std::endl;
        }
        
        namespace deep {
            void deepFunction() {
                std::cout << "outer::inner::deep::deepFunction() called" << std::endl;
                std::cout << "deepVar = " << deepVar << std::endl;
                std::cout << "innerVar = " << innerVar << std::endl;
                std::cout << "outerVar = " << outerVar << std::endl;
            }
        }
    }
}

// 实现C++17嵌套命名空间
namespace company::product::version {
    std::string getVersion() {
        return "1.0.0";
    }
    
    void ProductInfo::displayInfo() {
        std::cout << "Product: CompanyProduct, Version: " << getVersion() << std::endl;
    }
}

// 实现长命名空间
namespace very_long_company_name::very_long_product_name::very_long_version_name {
    void doSomething() {
        std::cout << "Long namespace function executed" << std::endl;
    }
    
    void LongNamedClass::process() {
        std::cout << "LongNamedClass::process() called" << std::endl;
    }
}

// 实现数学工具命名空间
namespace math_utils {
    double sin(double x) {
        return std::sin(x);
    }
    
    double cos(double x) {
        return std::cos(x);
    }
    
    double log(double x) {
        return std::log(x);
    }
    
    namespace advanced {
        double gamma(double x) {
            // 简化的伽马函数近似
            return std::exp(std::lgamma(x));
        }
        
        double beta(double a, double b) {
            // 简化的贝塔函数近似
            return std::exp(std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b));
        }
    }
}

// 实现图形命名空间
namespace graphics {
    void Point::draw() const {
        std::cout << "Drawing graphics::Point at (" << x_ << ", " << y_ << ")" << std::endl;
    }
    
    void draw(const Point& p) {
        std::cout << "Graphics draw function for point at (" 
                  << p.getX() << ", " << p.getY() << ")" << std::endl;
    }
}

// 实现几何命名空间
namespace geometry {
    void Point::print() const {
        std::cout << "geometry::Point(" << x_ << ", " << y_ << ")" << std::endl;
    }
    
    double distance(const Point& p1, const Point& p2) {
        int dx = p1.getX() - p2.getX();
        int dy = p1.getY() - p2.getY();
        return std::sqrt(dx*dx + dy*dy);
    }
}

// 实现ADL测试命名空间
namespace adl_test {
    void process(const MyClass& obj) {
        std::cout << "ADL process function called with value: " << obj.getValue() << std::endl;
    }
    
    MyClass operator+(const MyClass& a, const MyClass& b) {
        return MyClass(a.getValue() + b.getValue());
    }
    
    std::ostream& operator<<(std::ostream& os, const MyClass& obj) {
        os << "MyClass(" << obj.getValue() << ")";
        return os;
    }
}

// 实现内联命名空间
namespace library {
    inline namespace v2 {
        void newFunction() {
            std::cout << "New function from v2 (inline namespace)" << std::endl;
        }
        
        void NewClass::newMethod() {
            std::cout << "NewClass::newMethod() from v2" << std::endl;
        }
    }
    
    namespace v1 {
        void oldFunction() {
            std::cout << "Old function from v1" << std::endl;
        }
        
        void OldClass::oldMethod() {
            std::cout << "OldClass::oldMethod() from v1" << std::endl;
        }
    }
}

// 实现模板命名空间
namespace template_namespace {
    template<typename T>
    void processContainer(Container<T>& container) {
        std::cout << "Processing container with " << container.size() << " elements:" << std::endl;
        for (auto& item : container) {
            std::cout << "  " << item << std::endl;
        }
    }
    
    // 显式实例化常用类型
    template void processContainer<int>(Container<int>&);
    template void processContainer<std::string>(Container<std::string>&);
}

// 实现全局函数
void globalFunction() {
    std::cout << "Global function called, globalVariable = " << globalVariable << std::endl;
}

// 测试函数实现
void testBasicNamespace() {
    std::cout << "\n=== Testing Basic Namespace ===" << std::endl;
    
    // 使用完全限定名称
    basic_namespace::function1();
    
    // 访问命名空间中的变量
    std::cout << "basic_namespace::variable1 = " << basic_namespace::variable1 << std::endl;
    
    // 创建命名空间中的类对象
    basic_namespace::BasicClass obj;
    obj.method();
    
    // 修改命名空间变量
    basic_namespace::variable1 = 20;
    std::cout << "After modification: " << basic_namespace::variable1 << std::endl;
}

void testNestedNamespace() {
    std::cout << "\n=== Testing Nested Namespace ===" << std::endl;
    
    // 访问外层命名空间
    outer::outerFunction();
    
    // 访问内层命名空间
    outer::inner::innerFunction();
    
    // 访问深层命名空间
    outer::inner::deep::deepFunction();
    
    // C++17嵌套命名空间语法
    std::cout << "\nTesting C++17 nested namespace syntax:" << std::endl;
    std::cout << "Version: " << company::product::version::getVersion() << std::endl;
    
    company::product::version::ProductInfo info;
    info.displayInfo();
}

void testAnonymousNamespace() {
    std::cout << "\n=== Testing Anonymous Namespace ===" << std::endl;
    
    // 调用匿名命名空间中的函数
    internalFunction();
    internalFunction();
    internalFunction();
    
    // 匿名命名空间变量只在当前编译单元可见
    std::cout << "Internal counter value: " << internalCounter << std::endl;
}

void testNamespaceAlias() {
    std::cout << "\n=== Testing Namespace Alias ===" << std::endl;
    
    // 使用原始长命名空间
    very_long_company_name::very_long_product_name::very_long_version_name::doSomething();
    
    // 使用别名
    std::cout << "\nUsing namespace aliases:" << std::endl;
    short_name::doSomething();
    
    short_name::LongNamedClass obj;
    obj.process();
    
    // 另一个别名示例
    vlc::very_long_product_name::very_long_version_name::doSomething();
}

void testUsingDeclarations() {
    std::cout << "\n=== Testing Using Declarations ===" << std::endl;
    
    // 使用声明引入特定名称
    using math_utils::PI;
    using math_utils::sin;
    using math_utils::cos;
    
    std::cout << "PI = " << PI << std::endl;
    std::cout << "sin(PI/2) = " << sin(PI/2) << std::endl;
    std::cout << "cos(PI) = " << cos(PI) << std::endl;
    
    // 完全限定名称调用
    std::cout << "log(E) = " << math_utils::log(math_utils::E) << std::endl;
    
    // using指令引入整个命名空间
    {
        using namespace math_utils::advanced;
        std::cout << "\nUsing advanced math functions:" << std::endl;
        std::cout << "gamma(5) = " << math_utils::advanced::gamma(5) << std::endl;
        std::cout << "beta(2, 3) = " << beta(2, 3) << std::endl;
    }
    
    // 离开作用域后，using指令失效
    // std::cout << gamma(5);  // 编译错误
}

void testNamespaceConflicts() {
    std::cout << "\n=== Testing Namespace Conflicts ===" << std::endl;
    
    // 创建同名但不同命名空间的类
    graphics::Point graphicsPoint(10.5, 20.7);
    geometry::Point geometryPoint(10, 20);
    
    std::cout << "Graphics point:" << std::endl;
    graphicsPoint.draw();
    graphics::draw(graphicsPoint);
    
    std::cout << "\nGeometry point:" << std::endl;
    geometryPoint.print();
    
    // 计算几何点之间的距离
    geometry::Point point1(0, 0);
    geometry::Point point2(3, 4);
    double dist = geometry::distance(point1, point2);
    std::cout << "Distance between points: " << dist << std::endl;
    
    // 演示不同命名空间中同名类的区别
    std::cout << "\nDemonstrating type differences:" << std::endl;
    std::cout << "Graphics Point coordinates: (" 
              << graphicsPoint.getX() << ", " << graphicsPoint.getY() << ")" << std::endl;
    std::cout << "Geometry Point coordinates: (" 
              << geometryPoint.getX() << ", " << geometryPoint.getY() << ")" << std::endl;
}

void testADL() {
    std::cout << "\n=== Testing ADL (Argument-Dependent Lookup) ===" << std::endl;
    
    adl_test::MyClass obj1(10);
    adl_test::MyClass obj2(20);
    
    // ADL允许不使用命名空间前缀调用
    process(obj1);  // 相当于 adl_test::process(obj1)
    
    // 运算符重载也通过ADL找到
    adl_test::MyClass result = obj1 + obj2;  // 使用重载的operator+
    std::cout << "obj1 + obj2 = " << result << std::endl;  // 使用重载的operator<<
    
    // 显式调用也可以
    adl_test::process(result);
    
    std::cout << "\nADL explanation:" << std::endl;
    std::cout << "- Functions in the same namespace as the argument types" << std::endl;
    std::cout << "- Can be found without namespace qualification" << std::endl;
    std::cout << "- This is how std::cout << works with user-defined types" << std::endl;
}

void testInlineNamespace() {
    std::cout << "\n=== Testing Inline Namespace ===" << std::endl;
    
    // 内联命名空间的内容可以直接访问
    library::newFunction();    // 相当于 library::v2::newFunction()
    library::NewClass newObj;
    newObj.newMethod();
    
    // 也可以显式访问
    library::v2::newFunction();
    
    // 非内联命名空间需要显式指定
    library::v1::oldFunction();
    library::v1::OldClass oldObj;
    oldObj.oldMethod();
    
    std::cout << "\nInline namespace benefits:" << std::endl;
    std::cout << "- Provides versioning mechanism" << std::endl;
    std::cout << "- Allows backward compatibility" << std::endl;
    std::cout << "- Default version is accessible without qualification" << std::endl;
}

void testTemplateNamespace() {
    std::cout << "\n=== Testing Template Namespace ===" << std::endl;
    
    // 创建不同类型的容器
    template_namespace::Container<int> intContainer;
    intContainer.add(1);
    intContainer.add(2);
    intContainer.add(3);
    
    template_namespace::Container<std::string> stringContainer;
    stringContainer.add("Hello");
    stringContainer.add("Template");
    stringContainer.add("Namespace");
    
    // 处理容器
    std::cout << "Processing int container:" << std::endl;
    template_namespace::processContainer(intContainer);
    
    std::cout << "\nProcessing string container:" << std::endl;
    template_namespace::processContainer(stringContainer);
    
    // 使用using声明简化模板使用
    using IntContainer = template_namespace::Container<int>;
    IntContainer anotherIntContainer;
    anotherIntContainer.add(10);
    anotherIntContainer.add(20);
    
    std::cout << "\nUsing type alias:" << std::endl;
    template_namespace::processContainer(anotherIntContainer);
}

void testGlobalNamespace() {
    std::cout << "\n=== Testing Global Namespace ===" << std::endl;
    
    // 调用全局函数
    globalFunction();
    
    // 访问全局变量
    std::cout << "Global variable: " << globalVariable << std::endl;
    
    // 显式使用全局命名空间运算符
    ::globalFunction();  // 等同于 globalFunction()
    std::cout << "::globalVariable = " << ::globalVariable << std::endl;
    
    // 在存在命名空间冲突时，使用::访问全局版本
    {
        int globalVariable = 123;  // 局部变量隐藏全局变量
        std::cout << "Local globalVariable: " << globalVariable << std::endl;
        std::cout << "Global globalVariable: " << ::globalVariable << std::endl;
    }
    
    std::cout << "\nGlobal namespace operator (::) uses:" << std::endl;
    std::cout << "- Explicitly access global scope" << std::endl;
    std::cout << "- Resolve naming conflicts" << std::endl;
    std::cout << "- Access global functions/variables when shadowed" << std::endl;
}

void runAllNamespaceTests() {
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