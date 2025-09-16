#ifndef NAMESPACE_TEST_H
#define NAMESPACE_TEST_H

#include <iostream>
#include <string>
#include <vector>

// 测试1: 基础命名空间
namespace basic_namespace {
    void function1();
    extern int variable1;
    
    class BasicClass {
    public:
        void method();
    };
}

// 测试2: 嵌套命名空间
namespace outer {
    extern int outerVar;
    void outerFunction();
    
    namespace inner {
        extern int innerVar;
        void innerFunction();
        
        namespace deep {
            extern int deepVar;
            void deepFunction();
        }
    }
}

// C++17 嵌套命名空间简化语法
namespace company::product::version {
    std::string getVersion();
    class ProductInfo {
    public:
        void displayInfo();
    };
}

// 测试3: 匿名命名空间
namespace {
    // 文件作用域静态变量的替代方案
    int internalCounter = 0;
    void internalFunction() {
        std::cout << "Internal function called, counter: " << ++internalCounter << std::endl;
    }
}

// 测试4: 命名空间别名
namespace very_long_company_name {
    namespace very_long_product_name {
        namespace very_long_version_name {
            void doSomething();
            class LongNamedClass {
            public:
                void process();
            };
        }
    }
}

// 创建命名空间别名
namespace vlc = very_long_company_name;
namespace short_name = very_long_company_name::very_long_product_name::very_long_version_name;

// 测试5: 使用声明和using指令
namespace math_utils {
    const double PI = 3.14159265359;
    const double E = 2.71828182846;
    
    double sin(double x);
    double cos(double x);
    double log(double x);
    
    namespace advanced {
        double gamma(double x);
        double beta(double a, double b);
    }
}

// 测试6: 命名空间冲突解决
namespace graphics {
    class Point {
    private:
        double x_, y_;
    public:
        Point(double x, double y) : x_(x), y_(y) {}
        void draw() const;
        double getX() const { return x_; }
        double getY() const { return y_; }
    };
    
    void draw(const Point& p);
}

namespace geometry {
    class Point {
    private:
        int x_, y_;
    public:
        Point(int x, int y) : x_(x), y_(y) {}
        void print() const;
        int getX() const { return x_; }
        int getY() const { return y_; }
    };
    
    double distance(const Point& p1, const Point& p2);
}

// 测试7: ADL (Argument-Dependent Lookup) - Koenig查找
namespace adl_test {
    class MyClass {
    private:
        int value_;
    public:
        explicit MyClass(int value) : value_(value) {}
        int getValue() const { return value_; }
    };
    
    // 这个函数可以通过ADL找到
    void process(const MyClass& obj);
    MyClass operator+(const MyClass& a, const MyClass& b);
    std::ostream& operator<<(std::ostream& os, const MyClass& obj);
}

// 测试8: 内联命名空间 (C++11)
namespace library {
    inline namespace v2 {
        void newFunction();
        class NewClass {
        public:
            void newMethod();
        };
    }
    
    namespace v1 {
        void oldFunction();
        class OldClass {
        public:
            void oldMethod();
        };
    }
}

// 测试9: 命名空间中的模板
namespace template_namespace {
    template<typename T>
    class Container {
    private:
        std::vector<T> data_;
    public:
        void add(const T& item) { data_.push_back(item); }
        T& get(size_t index) { return data_[index]; }
        size_t size() const { return data_.size(); }
        
        // 迭代器
        typename std::vector<T>::iterator begin() { return data_.begin(); }
        typename std::vector<T>::iterator end() { return data_.end(); }
    };
    
    template<typename T>
    void processContainer(Container<T>& container);
}

// 测试10: 全局命名空间
void globalFunction();
extern int globalVariable;

// 测试函数声明
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

#endif // NAMESPACE_TEST_H