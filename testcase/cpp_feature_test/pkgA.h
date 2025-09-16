#ifndef PKGA_H
#define PKGA_H

#include <iostream>
#include <memory>
#include <vector>
#include <string>

// 测试1: 基础虚函数和继承
class Shape {
public:
    // 虚函数
    virtual double area() const { return 0.0; }
    
    // 纯虚函数
    virtual void draw() const = 0;
    
    // 虚析构函数
    virtual ~Shape() = default;
    
    // 普通成员函数
    std::string getName() const { return "Shape"; }
};

class Circle : public Shape {
private:
    double radius_;
    
public:
    explicit Circle(double r) : radius_(r) {}
    
    // 重写虚函数
    double area() const override { 
        return 3.14159 * radius_ * radius_; 
    }
    
    // 实现纯虚函数
    void draw() const override {
        std::cout << "Drawing Circle with radius: " << radius_ << std::endl;
    }
    
    std::string getName() const { return "Circle"; }
};

class Rectangle : public Shape {
private:
    double width_, height_;
    
public:
    Rectangle(double w, double h) : width_(w), height_(h) {}
    
    double area() const override {
        return width_ * height_;
    }
    
    void draw() const override {
        std::cout << "Drawing Rectangle: " << width_ << "x" << height_ << std::endl;
    }
    
    std::string getName() const { return "Rectangle"; }
};

// 测试2: 多重继承和虚继承
class Drawable {
public:
    virtual void render() const = 0;
    virtual ~Drawable() = default;
};

class Movable {
protected:
    double x_, y_;
    
public:
    Movable(double x = 0, double y = 0) : x_(x), y_(y) {}
    virtual void move(double dx, double dy) {
        x_ += dx;
        y_ += dy;
    }
    virtual ~Movable() = default;
};

// 多重继承
class MovableShape : public Shape, public Drawable, public Movable {
public:
    MovableShape(double x = 0, double y = 0) : Movable(x, y) {}
    
    void render() const override {
        std::cout << "Rendering movable shape at (" << x_ << ", " << y_ << ")" << std::endl;
        draw();
    }
    
    void move(double dx, double dy) override {
        Movable::move(dx, dy);
        std::cout << "Moved to (" << x_ << ", " << y_ << ")" << std::endl;
    }
};

class MovableCircle : public MovableShape {
private:
    double radius_;
    
public:
    MovableCircle(double r, double x = 0, double y = 0) 
        : MovableShape(x, y), radius_(r) {}
    
    double area() const override {
        return 3.14159 * radius_ * radius_;
    }
    
    void draw() const override {
        std::cout << "Drawing MovableCircle with radius: " << radius_ 
                  << " at (" << x_ << ", " << y_ << ")" << std::endl;
    }
};

// 测试3: 函数重载
class Calculator {
public:
    // 重载 + 操作符
    int add(int a, int b) const {
        std::cout << "Adding integers: " << a << " + " << b << std::endl;
        return a + b;
    }
    
    double add(double a, double b) const {
        std::cout << "Adding doubles: " << a << " + " << b << std::endl;
        return a + b;
    }
    
    std::string add(const std::string& a, const std::string& b) const {
        std::cout << "Concatenating strings: \"" << a << "\" + \"" << b << "\"" << std::endl;
        return a + b;
    }
    
    // 重载构造函数
    Calculator() { std::cout << "Default Calculator created" << std::endl; }
    explicit Calculator(const std::string& name) { 
        std::cout << "Named Calculator created: " << name << std::endl; 
    }
    
    // 运算符重载
    Calculator operator+(const Calculator& other) const {
        std::cout << "Calculator + Calculator called" << std::endl;
        return Calculator("Combined");
    }
};

// 测试4: 抽象类和接口模拟
class ILogger {
public:
    virtual void log(const std::string& message) = 0;
    virtual void setLevel(int level) = 0;
    virtual ~ILogger() = default;
};

class ConsoleLogger : public ILogger {
private:
    int level_;
    
public:
    ConsoleLogger() : level_(1) {}
    
    void log(const std::string& message) override {
        std::cout << "[LOG:" << level_ << "] " << message << std::endl;
    }
    
    void setLevel(int level) override {
        level_ = level;
    }
};

class FileLogger : public ILogger {
private:
    std::string filename_;
    int level_;
    
public:
    explicit FileLogger(const std::string& filename) : filename_(filename), level_(1) {}
    
    void log(const std::string& message) override {
        std::cout << "[FILE:" << filename_ << "::" << level_ << "] " << message << std::endl;
    }
    
    void setLevel(int level) override {
        level_ = level;
    }
};

// 测试5: 动态多态演示
class Animal {
public:
    virtual void makeSound() const = 0;
    virtual void move() const = 0;
    virtual ~Animal() = default;
    
    // 模板方法模式
    void performActions() const {
        std::cout << "Animal performing actions:" << std::endl;
        makeSound();
        move();
    }
};

class Dog : public Animal {
public:
    void makeSound() const override {
        std::cout << "Woof! Woof!" << std::endl;
    }
    
    void move() const override {
        std::cout << "Running on four legs" << std::endl;
    }
};

class Bird : public Animal {
public:
    void makeSound() const override {
        std::cout << "Tweet! Tweet!" << std::endl;
    }
    
    void move() const override {
        std::cout << "Flying with wings" << std::endl;
    }
};

// 测试函数声明
void testVirtualFunctions();
void testMultipleInheritance();
void testFunctionOverloading();
void testAbstractClasses();
void testPolymorphism();
void runAllOOPTests();


#endif // PKGA_H
