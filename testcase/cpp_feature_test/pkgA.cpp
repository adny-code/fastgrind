#include "pkgA.h"
#include <iostream>
#include <memory>
#include <vector>


void testVirtualFunctions() {
    std::cout << "\n=== Testing Virtual Functions ===" << std::endl;
    
    // 创建不同形状的对象
    Circle circle(5.0);
    Rectangle rectangle(4.0, 6.0);
    
    // 直接调用
    std::cout << "Direct calls:" << std::endl;
    std::cout << "Circle area: " << circle.area() << std::endl;
    circle.draw();
    
    std::cout << "Rectangle area: " << rectangle.area() << std::endl;
    rectangle.draw();
    
    // 通过基类指针调用（展示动态绑定）
    std::cout << "\nThrough base class pointers:" << std::endl;
    std::unique_ptr<Shape> shape1 = std::make_unique<Circle>(3.0);
    std::unique_ptr<Shape> shape2 = std::make_unique<Rectangle>(2.0, 8.0);
    
    std::cout << "Shape1 area: " << shape1->area() << std::endl;
    shape1->draw();
    
    std::cout << "Shape2 area: " << shape2->area() << std::endl;
    shape2->draw();
    
    // 演示虚函数表的作用
    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(std::make_unique<Circle>(2.0));
    shapes.push_back(std::make_unique<Rectangle>(3.0, 4.0));
    shapes.push_back(std::make_unique<Circle>(1.5));
    
    std::cout << "\nPolymorphic container iteration:" << std::endl;
    for (const auto& shape : shapes) {
        std::cout << "Area: " << shape->area() << " | ";
        shape->draw();
    }
}

void testMultipleInheritance() {
    std::cout << "\n=== Testing Multiple Inheritance ===" << std::endl;
    
    MovableCircle movableCircle(4.0, 10.0, 20.0);
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "Area: " << movableCircle.area() << std::endl;
    movableCircle.render();
    
    std::cout << "\nMoving the shape:" << std::endl;
    movableCircle.move(5.0, -3.0);
    movableCircle.render();
    
    // 测试多重继承的类型转换
    std::cout << "\nTesting multiple inheritance casting:" << std::endl;
    
    // 可以转换为任何基类
    Shape* shapePtr = &movableCircle;
    Drawable* drawablePtr = &movableCircle;
    Movable* movablePtr = &movableCircle;
    
    std::cout << "As Shape - Area: " << shapePtr->area() << std::endl;
    drawablePtr->render();
    movablePtr->move(1.0, 1.0);
}

void testFunctionOverloading() {
    std::cout << "\n=== Testing Function Overloading ===" << std::endl;
    
    // 测试构造函数重载
    Calculator calc1;  // 默认构造函数
    Calculator calc2("Scientific");  // 参数构造函数
    
    // 测试函数重载
    std::cout << "\nFunction overloading examples:" << std::endl;
    
    int intResult = calc1.add(5, 3);
    std::cout << "Result: " << intResult << std::endl;
    
    double doubleResult = calc1.add(3.14, 2.86);
    std::cout << "Result: " << doubleResult << std::endl;
    
    std::string stringResult = calc1.add("Hello", " World!");
    std::cout << "Result: " << stringResult << std::endl;
    
    // 测试运算符重载
    std::cout << "\nOperator overloading example:" << std::endl;
    Calculator calc3 = calc1 + calc2;
    
    // 展示重载解析
    std::cout << "\nDemonstrating overload resolution:" << std::endl;
    calc1.add(10, 20);        // int版本
    calc1.add(10.0, 20.0);    // double版本
    calc1.add(10.5f, 20.5f);  // 会选择double版本（float提升为double）
}

void testAbstractClasses() {
    std::cout << "\n=== Testing Abstract Classes and Interfaces ===" << std::endl;
    
    // 创建不同类型的Logger
    std::unique_ptr<ILogger> consoleLogger = std::make_unique<ConsoleLogger>();
    std::unique_ptr<ILogger> fileLogger = std::make_unique<FileLogger>("app.log");
    
    // 测试接口的多态性
    std::vector<std::unique_ptr<ILogger>> loggers;
    loggers.push_back(std::make_unique<ConsoleLogger>());
    loggers.push_back(std::make_unique<FileLogger>("debug.log"));
    loggers.push_back(std::make_unique<FileLogger>("error.log"));
    
    std::cout << "Testing polymorphic logging:" << std::endl;
    for (auto& logger : loggers) {
        logger->setLevel(2);
        logger->log("This is a test message");
    }
    
    // 展示抽象类不能实例化
    // Shape shape;  // 编译错误！Shape是抽象类
    // ILogger logger;  // 编译错误！ILogger是纯虚类
    
    std::cout << "\nLogger interface demonstration complete" << std::endl;
}

void testPolymorphism() {
    std::cout << "\n=== Testing Advanced Polymorphism ===" << std::endl;
    
    // 创建动物园
    std::vector<std::unique_ptr<Animal>> zoo;
    zoo.push_back(std::make_unique<Dog>());
    zoo.push_back(std::make_unique<Bird>());
    zoo.push_back(std::make_unique<Dog>());
    zoo.push_back(std::make_unique<Bird>());
    
    std::cout << "Welcome to the polymorphic zoo!" << std::endl;
    
    int animalCount = 1;
    for (const auto& animal : zoo) {
        std::cout << "\nAnimal " << animalCount++ << ":" << std::endl;
        animal->performActions();  // 模板方法模式
    }
    
    // 测试运行时类型识别
    std::cout << "\n--- Runtime Type Identification Test ---" << std::endl;
    
    std::unique_ptr<Animal> mysteryAnimal1 = std::make_unique<Dog>();
    std::unique_ptr<Animal> mysteryAnimal2 = std::make_unique<Bird>();
    
    // 使用dynamic_cast进行安全的向下转型
    Dog* dogPtr = dynamic_cast<Dog*>(mysteryAnimal1.get());
    Bird* birdPtr = dynamic_cast<Bird*>(mysteryAnimal1.get());
    
    if (dogPtr) {
        std::cout << "mysteryAnimal1 is a Dog!" << std::endl;
        dogPtr->makeSound();
    }
    
    if (birdPtr) {
        std::cout << "mysteryAnimal1 is a Bird!" << std::endl;
    } else {
        std::cout << "mysteryAnimal1 is NOT a Bird!" << std::endl;
    }
    
    // 测试虚函数在构造和析构过程中的行为
    std::cout << "\n--- Virtual Function Behavior Test ---" << std::endl;
    {
        std::unique_ptr<Animal> animal = std::make_unique<Dog>();
        std::cout << "Animal in scope, calling virtual functions:" << std::endl;
        animal->makeSound();
        animal->move();
    } // 这里会调用虚析构函数
    std::cout << "Animal destroyed (virtual destructor called)" << std::endl;
}

void runAllOOPTests() {
    std::cout << "\n########################################" << std::endl;
    std::cout << "#     C++ OOP Features Test Suite     #" << std::endl;
    std::cout << "########################################" << std::endl;
    
    testVirtualFunctions();
    testMultipleInheritance();
    testFunctionOverloading();
    testAbstractClasses();
    testPolymorphism();
    
    std::cout << "\n########################################" << std::endl;
    std::cout << "#     OOP Tests Completed!            #" << std::endl;
    std::cout << "########################################" << std::endl;
}

