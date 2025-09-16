#ifndef MODERN_CPP_H
#define MODERN_CPP_H

#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <thread>
#include <future>
#include <chrono>
#include <optional>
#include <variant>
#include <any>
#include <tuple>
#include <initializer_list>
#include <utility>

namespace test_modern_cpp {

// 测试1: auto关键字和类型推导 (C++11/14/17)
void testAutoKeyword();

// 测试2: constexpr关键字 (C++11/14/17)
constexpr int factorial(int n) {
    return (n <= 1) ? 1 : n * factorial(n - 1);
}

constexpr bool isPrime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) {
            return false;
        }
    }
    return true;
}

class ConstexprDemo {
public:
    constexpr ConstexprDemo(int val) : value_(val) {}
    constexpr int getValue() const { return value_; }
    constexpr int square() const { return value_ * value_; }
    
private:
    int value_;
};

void testConstexpr();

// 测试3: Lambda表达式 (C++11/14/17)
void testLambdaExpressions();

// 测试4: 智能指针 (C++11/14)
class Resource {
private:
    std::string name_;
    int* data_;
    
public:
    explicit Resource(const std::string& name, int size = 10) 
        : name_(name), data_(new int[size]) {
        std::cout << "Resource '" << name_ << "' created" << std::endl;
        for (int i = 0; i < size; ++i) {
            data_[i] = i;
        }
    }
    
    ~Resource() {
        std::cout << "Resource '" << name_ << "' destroyed" << std::endl;
        delete[] data_;
    }
    
    // 禁用拷贝，演示智能指针的重要性
    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;
    
    // 允许移动 (C++11)
    Resource(Resource&& other) noexcept 
        : name_(std::move(other.name_)), data_(other.data_) {
        other.data_ = nullptr;
        std::cout << "Resource '" << name_ << "' moved" << std::endl;
    }
    
    Resource& operator=(Resource&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            name_ = std::move(other.name_);
            data_ = other.data_;
            other.data_ = nullptr;
        }
        return *this;
    }
    
    const std::string& getName() const { return name_; }
    void processData() const {
        std::cout << "Processing data in resource '" << name_ << "'" << std::endl;
    }
};

void testSmartPointers();

// 测试5: 右值引用和移动语义 (C++11)
class MoveableClass {
private:
    std::string name_;
    std::vector<int> data_;
    
public:
    // 构造函数
    MoveableClass(const std::string& name, std::initializer_list<int> values)
        : name_(name), data_(values) {
        std::cout << "MoveableClass '" << name_ << "' constructed" << std::endl;
    }
    
    // 拷贝构造函数
    MoveableClass(const MoveableClass& other) 
        : name_(other.name_ + "_copy"), data_(other.data_) {
        std::cout << "MoveableClass '" << name_ << "' copy constructed" << std::endl;
    }
    
    // 移动构造函数 (C++11)
    MoveableClass(MoveableClass&& other) noexcept
        : name_(std::move(other.name_)), data_(std::move(other.data_)) {
        std::cout << "MoveableClass '" << name_ << "' move constructed" << std::endl;
    }
    
    // 拷贝赋值运算符
    MoveableClass& operator=(const MoveableClass& other) {
        if (this != &other) {
            name_ = other.name_ + "_copy_assigned";
            data_ = other.data_;
            std::cout << "MoveableClass '" << name_ << "' copy assigned" << std::endl;
        }
        return *this;
    }
    
    // 移动赋值运算符 (C++11)
    MoveableClass& operator=(MoveableClass&& other) noexcept {
        if (this != &other) {
            name_ = std::move(other.name_);
            data_ = std::move(other.data_);
            std::cout << "MoveableClass '" << name_ << "' move assigned" << std::endl;
        }
        return *this;
    }
    
    ~MoveableClass() {
        std::cout << "MoveableClass '" << name_ << "' destroyed" << std::endl;
    }
    
    const std::string& getName() const { return name_; }
    size_t getSize() const { return data_.size(); }
    
    void printData() const {
        std::cout << name_ << " data: ";
        for (int val : data_) {
            std::cout << val << " ";
        }
        std::cout << "(size: " << data_.size() << ")" << std::endl;
    }
};

void testMoveSemantics();

// 测试6: 范围for循环和初始化列表 (C++11)
void testRangeForAndInitLists();

// 测试7: nullptr和强类型枚举 (C++11)
enum class Color : int {
    RED = 1,
    GREEN = 2,
    BLUE = 3,
    ALPHA = 4
};

enum class Status {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED
};

void testNullptrAndEnums();

// 测试8: 委托构造函数和继承构造函数 (C++11)
class BaseConstruct {
protected:
    std::string name_;
    int value_;
    
public:
    BaseConstruct(const std::string& name, int value) 
        : name_(name), value_(value) {
        std::cout << "BaseConstruct: " << name_ << ", " << value_ << std::endl;
    }
    
    virtual ~BaseConstruct() = default;
    
    virtual void display() const {
        std::cout << "Base: " << name_ << " = " << value_ << std::endl;
    }
};

class DerivedConstruct : public BaseConstruct {
public:
    // 继承构造函数 (C++11)
    using BaseConstruct::BaseConstruct;
    
    // 委托构造函数 (C++11)
    DerivedConstruct() : DerivedConstruct("default", 0) {}
    
    DerivedConstruct(const std::string& name, int value, double extra)
        : BaseConstruct(name, value), extra_(extra) {
        std::cout << "DerivedConstruct: extra = " << extra_ << std::endl;
    }
    
    void display() const override {
        BaseConstruct::display();
        std::cout << "  Extra: " << extra_ << std::endl;
    }
    
private:
    double extra_ = 1.0;  // 成员初始化 (C++11)
};

void testConstructorFeatures();

// 测试9: 线程和异步编程 (C++11)
void testThreadsAndAsync();

// 测试10: C++17特性
void testCpp17Features();

// 运行所有现代C++特性测试
void runAllModernCppTests();

} // namespace test_modern_cpp

#endif // MODERN_CPP_H