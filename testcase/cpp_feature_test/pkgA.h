#ifndef PKGA_H
#define PKGA_H

#include <iostream>
#include <memory>
#include <string>
#include <vector>

class Shape
{
  public:
    virtual double area() const
    {
        return 0.0;
    }

    virtual void draw() const = 0;

    virtual ~Shape() = default;

    std::string getName() const
    {
        return "Shape";
    }
};

class Circle : public Shape
{
  private:
    double radius_;

  public:
    explicit Circle(double r) : radius_(r)
    {
    }

    double area() const override
    {
        return 3.14159 * radius_ * radius_;
    }

    void draw() const override
    {
        std::cout << "Drawing Circle with radius: " << radius_ << std::endl;
    }

    std::string getName() const
    {
        return "Circle";
    }
};

class Rectangle : public Shape
{
  private:
    double width_, height_;

  public:
    Rectangle(double w, double h) : width_(w), height_(h)
    {
    }

    double area() const override
    {
        return width_ * height_;
    }

    void draw() const override
    {
        std::cout << "Drawing Rectangle: " << width_ << "x" << height_ << std::endl;
    }

    std::string getName() const
    {
        return "Rectangle";
    }
};

class Drawable
{
  public:
    virtual void render() const = 0;
    virtual ~Drawable() = default;
};

class Movable
{
  protected:
    double x_, y_;

  public:
    Movable(double x = 0, double y = 0) : x_(x), y_(y)
    {
    }
    virtual void move(double dx, double dy)
    {
        x_ += dx;
        y_ += dy;
    }
    virtual ~Movable() = default;
};

class MovableShape : public Shape, public Drawable, public Movable
{
  public:
    MovableShape(double x = 0, double y = 0) : Movable(x, y)
    {
    }

    void render() const override
    {
        std::cout << "Rendering movable shape at (" << x_ << ", " << y_ << ")" << std::endl;
        draw();
    }

    void move(double dx, double dy) override
    {
        Movable::move(dx, dy);
        std::cout << "Moved to (" << x_ << ", " << y_ << ")" << std::endl;
    }
};

class MovableCircle : public MovableShape
{
  private:
    double radius_;

  public:
    MovableCircle(double r, double x = 0, double y = 0) : MovableShape(x, y), radius_(r)
    {
    }

    double area() const override
    {
        return 3.14159 * radius_ * radius_;
    }

    void draw() const override
    {
        std::cout << "Drawing MovableCircle with radius: " << radius_ << " at (" << x_ << ", " << y_ << ")"
                  << std::endl;
    }
};

class Calculator
{
  public:
    int add(int a, int b) const
    {
        std::cout << "Adding integers: " << a << " + " << b << std::endl;
        return a + b;
    }

    double add(double a, double b) const
    {
        std::cout << "Adding doubles: " << a << " + " << b << std::endl;
        return a + b;
    }

    std::string add(const std::string &a, const std::string &b) const
    {
        std::cout << "Concatenating strings: \"" << a << "\" + \"" << b << "\"" << std::endl;
        return a + b;
    }

    Calculator()
    {
        std::cout << "Default Calculator created" << std::endl;
    }
    explicit Calculator(const std::string &name)
    {
        std::cout << "Named Calculator created: " << name << std::endl;
    }

    Calculator operator+(const Calculator &other) const
    {
        std::cout << "Calculator + Calculator called" << std::endl;
        return Calculator("Combined");
    }
};

class ILogger
{
  public:
    virtual void log(const std::string &message) = 0;
    virtual void setLevel(int level) = 0;
    virtual ~ILogger() = default;
};

class ConsoleLogger : public ILogger
{
  private:
    int level_;

  public:
    ConsoleLogger() : level_(1)
    {
    }

    void log(const std::string &message) override
    {
        std::cout << "[LOG:" << level_ << "] " << message << std::endl;
    }

    void setLevel(int level) override
    {
        level_ = level;
    }
};

class FileLogger : public ILogger
{
  private:
    std::string filename_;
    int level_;

  public:
    explicit FileLogger(const std::string &filename) : filename_(filename), level_(1)
    {
    }

    void log(const std::string &message) override
    {
        std::cout << "[FILE:" << filename_ << "::" << level_ << "] " << message << std::endl;
    }

    void setLevel(int level) override
    {
        level_ = level;
    }
};

class Animal
{
  public:
    virtual void makeSound() const = 0;
    virtual void move() const = 0;
    virtual ~Animal() = default;

    void performActions() const
    {
        std::cout << "Animal performing actions:" << std::endl;
        makeSound();
        move();
    }
};

class Dog : public Animal
{
  public:
    void makeSound() const override
    {
        std::cout << "Woof! Woof!" << std::endl;
    }

    void move() const override
    {
        std::cout << "Running on four legs" << std::endl;
    }
};

class Bird : public Animal
{
  public:
    void makeSound() const override
    {
        std::cout << "Tweet! Tweet!" << std::endl;
    }

    void move() const override
    {
        std::cout << "Flying with wings" << std::endl;
    }
};

void testVirtualFunctions();
void testMultipleInheritance();
void testFunctionOverloading();
void testAbstractClasses();
void testPolymorphism();
void runAllOOPTests();

#endif
