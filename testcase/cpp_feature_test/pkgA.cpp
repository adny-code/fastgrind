#include "pkgA.h"
#include <iostream>
#include <memory>
#include <vector>

void testVirtualFunctions()
{
    std::cout << "\n=== Testing Virtual Functions ===" << std::endl;

    Circle circle(5.0);
    Rectangle rectangle(4.0, 6.0);

    std::cout << "Direct calls:" << std::endl;
    std::cout << "Circle area: " << circle.area() << std::endl;
    circle.draw();

    std::cout << "Rectangle area: " << rectangle.area() << std::endl;
    rectangle.draw();

    std::cout << "\nThrough base class pointers:" << std::endl;
    std::unique_ptr<Shape> shape1 = std::make_unique<Circle>(3.0);
    std::unique_ptr<Shape> shape2 = std::make_unique<Rectangle>(2.0, 8.0);

    std::cout << "Shape1 area: " << shape1->area() << std::endl;
    shape1->draw();

    std::cout << "Shape2 area: " << shape2->area() << std::endl;
    shape2->draw();

    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(std::make_unique<Circle>(2.0));
    shapes.push_back(std::make_unique<Rectangle>(3.0, 4.0));
    shapes.push_back(std::make_unique<Circle>(1.5));

    std::cout << "\nPolymorphic container iteration:" << std::endl;
    for (const auto &shape : shapes)
    {
        std::cout << "Area: " << shape->area() << " | ";
        shape->draw();
    }
}

void testMultipleInheritance()
{
    std::cout << "\n=== Testing Multiple Inheritance ===" << std::endl;

    MovableCircle movableCircle(4.0, 10.0, 20.0);

    std::cout << "Initial state:" << std::endl;
    std::cout << "Area: " << movableCircle.area() << std::endl;
    movableCircle.render();

    std::cout << "\nMoving the shape:" << std::endl;
    movableCircle.move(5.0, -3.0);
    movableCircle.render();

    std::cout << "\nTesting multiple inheritance casting:" << std::endl;

    Shape *shapePtr = &movableCircle;
    Drawable *drawablePtr = &movableCircle;
    Movable *movablePtr = &movableCircle;

    std::cout << "As Shape - Area: " << shapePtr->area() << std::endl;
    drawablePtr->render();
    movablePtr->move(1.0, 1.0);
}

void testFunctionOverloading()
{
    std::cout << "\n=== Testing Function Overloading ===" << std::endl;

    Calculator calc1;
    Calculator calc2("Scientific");

    std::cout << "\nFunction overloading examples:" << std::endl;

    int intResult = calc1.add(5, 3);
    std::cout << "Result: " << intResult << std::endl;

    double doubleResult = calc1.add(3.14, 2.86);
    std::cout << "Result: " << doubleResult << std::endl;

    std::string stringResult = calc1.add("Hello", " World!");
    std::cout << "Result: " << stringResult << std::endl;

    std::cout << "\nOperator overloading example:" << std::endl;
    Calculator calc3 = calc1 + calc2;

    std::cout << "\nDemonstrating overload resolution:" << std::endl;
    calc1.add(10, 20);
    calc1.add(10.0, 20.0);
    calc1.add(10.5f, 20.5f);
}

void testAbstractClasses()
{
    std::cout << "\n=== Testing Abstract Classes and Interfaces ===" << std::endl;

    std::unique_ptr<ILogger> consoleLogger = std::make_unique<ConsoleLogger>();
    std::unique_ptr<ILogger> fileLogger = std::make_unique<FileLogger>("app.log");

    std::vector<std::unique_ptr<ILogger>> loggers;
    loggers.push_back(std::make_unique<ConsoleLogger>());
    loggers.push_back(std::make_unique<FileLogger>("debug.log"));
    loggers.push_back(std::make_unique<FileLogger>("error.log"));

    std::cout << "Testing polymorphic logging:" << std::endl;
    for (auto &logger : loggers)
    {
        logger->setLevel(2);
        logger->log("This is a test message");
    }

    std::cout << "\nLogger interface demonstration complete" << std::endl;
}

void testPolymorphism()
{
    std::cout << "\n=== Testing Advanced Polymorphism ===" << std::endl;

    std::vector<std::unique_ptr<Animal>> zoo;
    zoo.push_back(std::make_unique<Dog>());
    zoo.push_back(std::make_unique<Bird>());
    zoo.push_back(std::make_unique<Dog>());
    zoo.push_back(std::make_unique<Bird>());

    std::cout << "Welcome to the polymorphic zoo!" << std::endl;

    int animalCount = 1;
    for (const auto &animal : zoo)
    {
        std::cout << "\nAnimal " << animalCount++ << ":" << std::endl;
        animal->performActions();
    }

    std::cout << "\n--- Runtime Type Identification Test ---" << std::endl;

    std::unique_ptr<Animal> mysteryAnimal1 = std::make_unique<Dog>();
    std::unique_ptr<Animal> mysteryAnimal2 = std::make_unique<Bird>();

    Dog *dogPtr = dynamic_cast<Dog *>(mysteryAnimal1.get());
    Bird *birdPtr = dynamic_cast<Bird *>(mysteryAnimal1.get());

    if (dogPtr)
    {
        std::cout << "mysteryAnimal1 is a Dog!" << std::endl;
        dogPtr->makeSound();
    }

    if (birdPtr)
    {
        std::cout << "mysteryAnimal1 is a Bird!" << std::endl;
    }
    else
    {
        std::cout << "mysteryAnimal1 is NOT a Bird!" << std::endl;
    }

    std::cout << "\n--- Virtual Function Behavior Test ---" << std::endl;
    {
        std::unique_ptr<Animal> animal = std::make_unique<Dog>();
        std::cout << "Animal in scope, calling virtual functions:" << std::endl;
        animal->makeSound();
        animal->move();
    }
    std::cout << "Animal destroyed (virtual destructor called)" << std::endl;
}

void runAllOOPTests()
{
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
