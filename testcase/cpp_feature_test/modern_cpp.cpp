#include "modern_cpp.h"
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <numeric>
#include <random>
#include <variant>
#include <optional>

namespace test_modern_cpp {

void testAutoKeyword() {
    std::cout << "\n=== Testing auto Keyword ===" << std::endl;
    
    // 基础auto使用
    auto intVar = 42;                    // int
    auto doubleVar = 3.14;              // double
    auto stringVar = std::string("Hello"); // std::string
    auto charPtr = "World";             // const char*
    
    std::cout << "Basic auto deduction:" << std::endl;
    std::cout << "intVar: " << intVar << " (type: int)" << std::endl;
    std::cout << "doubleVar: " << doubleVar << " (type: double)" << std::endl;
    std::cout << "stringVar: " << stringVar << " (type: std::string)" << std::endl;
    std::cout << "charPtr: " << charPtr << " (type: const char*)" << std::endl;
    
    // auto与容器
    std::vector<int> vec = {1, 2, 3, 4, 5};
    auto vecSize = vec.size();          // size_t
    auto vecIt = vec.begin();           // std::vector<int>::iterator
    
    std::cout << "\nauto with containers:" << std::endl;
    std::cout << "Vector size: " << vecSize << std::endl;
    std::cout << "First element: " << *vecIt << std::endl;
    
    // auto与lambda
    auto lambda = [](int x) { return x * x; };
    auto result = lambda(5);
    
    std::cout << "\nauto with lambda:" << std::endl;
    std::cout << "lambda(5) = " << result << std::endl;
    
    // auto与函数返回值
    auto findMax = [](const std::vector<int>& v) -> auto {
        return *std::max_element(v.begin(), v.end());
    };
    
    auto maxVal = findMax(vec);
    std::cout << "Max value: " << maxVal << std::endl;
    
    // decltype与auto (C++11)
    int x = 10;
    decltype(x) y = 20;              // int
    decltype(auto) z = x;            // C++14: int
    
    std::cout << "\ndecltype examples:" << std::endl;
    std::cout << "x: " << x << ", y: " << y << ", z: " << z << std::endl;
    
    // 尾置返回类型 (C++11)
    auto multiply = [](auto a, auto b) -> decltype(a * b) {
        return a * b;
    };
    
    auto product1 = multiply(3, 4);      // int * int
    auto product2 = multiply(2.5, 3);    // double * int
    
    std::cout << "multiply(3, 4) = " << product1 << std::endl;
    std::cout << "multiply(2.5, 3) = " << product2 << std::endl;
}

void testConstexpr() {
    std::cout << "\n=== Testing constexpr ===" << std::endl;
    
    // 编译期常量计算
    constexpr int fact5 = factorial(5);
    constexpr int fact10 = factorial(10);
    
    std::cout << "Compile-time factorials:" << std::endl;
    std::cout << "factorial(5) = " << fact5 << std::endl;
    std::cout << "factorial(10) = " << fact10 << std::endl;
    
    // 编译期素数检查
    constexpr bool is17Prime = isPrime(17);
    constexpr bool is18Prime = isPrime(18);
    
    std::cout << "\nCompile-time prime checks:" << std::endl;
    std::cout << "isPrime(17) = " << std::boolalpha << is17Prime << std::endl;
    std::cout << "isPrime(18) = " << std::boolalpha << is18Prime << std::endl;
    
    // constexpr对象
    constexpr ConstexprDemo demo(7);
    constexpr int demoValue = demo.getValue();
    constexpr int demoSquare = demo.square();
    
    std::cout << "\nConstexpr class:" << std::endl;
    std::cout << "demo.getValue() = " << demoValue << std::endl;
    std::cout << "demo.square() = " << demoSquare << std::endl;
    
    // 运行时vs编译时对比
    std::cout << "\nRuntime vs Compile-time comparison:" << std::endl;
    
    // 运行时计算
    int n = 8;
    int runtimeFact = 1;
    for (int i = 1; i <= n; ++i) {
        runtimeFact *= i;
    }
    std::cout << "Runtime factorial(8) = " << runtimeFact << std::endl;
    
    // 编译时计算
    constexpr int compiletimeFact = factorial(8);
    std::cout << "Compile-time factorial(8) = " << compiletimeFact << std::endl;
    
    std::cout << "Note: Compile-time version has zero runtime cost!" << std::endl;
}

void testLambdaExpressions() {
    std::cout << "\n=== Testing Lambda Expressions ===" << std::endl;
    
    // 基础lambda
    auto simpleLambda = []() {
        std::cout << "Simple lambda executed!" << std::endl;
    };
    simpleLambda();
    
    // 带参数的lambda
    auto paramLambda = [](int x, int y) {
        return x + y;
    };
    std::cout << "paramLambda(3, 4) = " << paramLambda(3, 4) << std::endl;
    
    // 捕获列表
    int multiplier = 10;
    std::string prefix = "Result: ";
    
    // 按值捕获
    auto valueLambda = [multiplier](int x) {
        return multiplier * x;
    };
    
    // 按引用捕获
    auto refLambda = [&prefix](int x) {
        return prefix + std::to_string(x);
    };
    
    // 混合捕获
    auto mixedLambda = [multiplier, &prefix](int x) {
        return prefix + std::to_string(multiplier * x);
    };
    
    std::cout << "\nCapture examples:" << std::endl;
    std::cout << "valueLambda(5) = " << valueLambda(5) << std::endl;
    std::cout << "refLambda(42) = " << refLambda(42) << std::endl;
    std::cout << "mixedLambda(7) = " << mixedLambda(7) << std::endl;
    
    // 修改prefix，看引用捕获的效果
    prefix = "New Result: ";
    std::cout << "After changing prefix:" << std::endl;
    std::cout << "refLambda(42) = " << refLambda(42) << std::endl;
    std::cout << "mixedLambda(7) = " << mixedLambda(7) << std::endl;
    
    // 泛型lambda (C++14)
    auto genericLambda = [](auto x, auto y) {
        return x + y;
    };
    
    std::cout << "\nGeneric lambda (C++14):" << std::endl;
    std::cout << "genericLambda(1, 2) = " << genericLambda(1, 2) << std::endl;
    std::cout << "genericLambda(1.5, 2.5) = " << genericLambda(1.5, 2.5) << std::endl;
    std::cout << "genericLambda(\"Hello\", \" World\") = " 
              << genericLambda(std::string("Hello"), std::string(" World")) << std::endl;
    
    // 与STL算法一起使用
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    std::cout << "\nLambda with STL algorithms:" << std::endl;
    std::cout << "Original numbers: ";
    for (int n : numbers) std::cout << n << " ";
    std::cout << std::endl;
    
    // 使用lambda进行过滤
    std::vector<int> evens;
    std::copy_if(numbers.begin(), numbers.end(), std::back_inserter(evens),
                 [](int n) { return n % 2 == 0; });
    
    std::cout << "Even numbers: ";
    for (int n : evens) std::cout << n << " ";
    std::cout << std::endl;
    
    // 使用lambda进行变换
    std::vector<int> squares;
    std::transform(numbers.begin(), numbers.end(), std::back_inserter(squares),
                   [](int n) { return n * n; });
    
    std::cout << "Squares: ";
    for (int n : squares) std::cout << n << " ";
    std::cout << std::endl;
    
    // 使用lambda进行累积
    int sum = std::accumulate(numbers.begin(), numbers.end(), 0,
                              [](int acc, int n) { return acc + n; });
    std::cout << "Sum: " << sum << std::endl;
}

void testSmartPointers() {
    std::cout << "\n=== Testing Smart Pointers ===" << std::endl;
    
    // unique_ptr测试
    std::cout << "Testing unique_ptr:" << std::endl;
    {
        auto resource1 = std::make_unique<Resource>("unique_resource1");
        resource1->processData();
        
        // 移动语义
        auto resource2 = std::move(resource1);
        if (!resource1) {
            std::cout << "resource1 is now null after move" << std::endl;
        }
        resource2->processData();
        
        // 自动销毁
        std::cout << "Leaving unique_ptr scope..." << std::endl;
    }
    std::cout << "unique_ptr resources automatically destroyed" << std::endl;
    
    // shared_ptr测试
    std::cout << "\nTesting shared_ptr:" << std::endl;
    {
        auto shared1 = std::make_shared<Resource>("shared_resource");
        std::cout << "shared1 use_count: " << shared1.use_count() << std::endl;
        
        {
            auto shared2 = shared1;  // 共享所有权
            std::cout << "After copying, use_count: " << shared1.use_count() << std::endl;
            shared2->processData();
            
            auto shared3 = shared1;
            std::cout << "After another copy, use_count: " << shared1.use_count() << std::endl;
            
            std::cout << "Leaving inner scope..." << std::endl;
        }
        
        std::cout << "After inner scope, use_count: " << shared1.use_count() << std::endl;
        shared1->processData();
        std::cout << "Leaving outer scope..." << std::endl;
    }
    std::cout << "shared_ptr resource automatically destroyed when count reached 0" << std::endl;
    
    // weak_ptr测试
    std::cout << "\nTesting weak_ptr:" << std::endl;
    std::weak_ptr<Resource> weak;
    
    {
        auto shared = std::make_shared<Resource>("weak_test_resource");
        weak = shared;
        
        std::cout << "weak_ptr expired: " << std::boolalpha << weak.expired() << std::endl;
        std::cout << "shared use_count: " << shared.use_count() << std::endl;
        std::cout << "weak use_count: " << weak.use_count() << std::endl;
        
        // 从weak_ptr获取shared_ptr
        if (auto locked = weak.lock()) {
            locked->processData();
            std::cout << "Successfully locked weak_ptr" << std::endl;
        }
        
        std::cout << "Leaving shared_ptr scope..." << std::endl;
    }
    
    std::cout << "After shared_ptr destroyed:" << std::endl;
    std::cout << "weak_ptr expired: " << std::boolalpha << weak.expired() << std::endl;
    
    if (auto locked = weak.lock()) {
        std::cout << "This should not print" << std::endl;
    } else {
        std::cout << "weak_ptr cannot be locked (resource destroyed)" << std::endl;
    }
}

void testMoveSemantics() {
    std::cout << "\n=== Testing Move Semantics ===" << std::endl;
    
    // 移动构造
    std::cout << "Creating original object:" << std::endl;
    MoveableClass original("original", {1, 2, 3, 4, 5});
    original.printData();
    
    std::cout << "\nMove construction:" << std::endl;
    MoveableClass moved = std::move(original);
    moved.printData();
    
    // 检查原对象状态
    std::cout << "Original after move: size = " << original.getSize() << std::endl;
    
    // 拷贝构造对比
    std::cout << "\nCopy construction:" << std::endl;
    MoveableClass copied = moved;  // 拷贝构造
    copied.printData();
    moved.printData();  // moved对象应该保持不变
    
    // 移动赋值
    std::cout << "\nMove assignment:" << std::endl;
    MoveableClass target("target", {10, 20});
    target.printData();
    
    target = std::move(copied);  // 移动赋值
    target.printData();
    std::cout << "copied after move: size = " << copied.getSize() << std::endl;
    
    // 完美转发示例
    std::cout << "\nPerfect forwarding example:" << std::endl;
    auto factory = [](auto&& arg) {
        return MoveableClass(std::forward<decltype(arg)>(arg), {100, 200, 300});
    };
    
    std::string name1 = "forwarded_copy";
    auto obj1 = factory(name1);          // 拷贝转发
    auto obj2 = factory(std::move(name1)); // 移动转发
    
    obj1.printData();
    obj2.printData();
    
    // 返回值优化（RVO）和命名返回值优化（NRVO）
    std::cout << "\nReturn Value Optimization:" << std::endl;
    auto createObject = [](const std::string& name) {
        return MoveableClass(name, {1000, 2000, 3000});  // RVO优化
    };
    
    auto rvoObj = createObject("RVO_object");
    rvoObj.printData();
}

void testRangeForAndInitLists() {
    std::cout << "\n=== Testing Range-for and Initializer Lists ===" << std::endl;
    
    // 初始化列表
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::map<std::string, int> map = {
        {"one", 1},
        {"two", 2},
        {"three", 3}
    };
    
    // 范围for循环
    std::cout << "Range-for with vector: ";
    for (const auto& elem : vec) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;
    
    std::cout << "Range-for with map:" << std::endl;
    for (const auto& [key, value] : map) {  // C++17结构化绑定
        std::cout << "  " << key << " = " << value << std::endl;
    }
    
    // 修改元素
    std::cout << "\nModifying elements:" << std::endl;
    for (auto& elem : vec) {
        elem *= 2;
    }
    
    std::cout << "After doubling: ";
    for (const auto& elem : vec) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;
    
    // 初始化列表作为函数参数
    auto processNumbers = [](std::initializer_list<int> numbers) {
        std::cout << "Processing numbers: ";
        for (const auto& num : numbers) {
            std::cout << num << " ";
        }
        std::cout << "(count: " << numbers.size() << ")" << std::endl;
    };
    
    processNumbers({10, 20, 30, 40});
    processNumbers({100, 200});
    
    // 统一初始化
    std::cout << "\nUniform initialization:" << std::endl;
    int x{42};
    double y{3.14};
    std::string s{"Hello"};
    std::vector<int> v{1, 2, 3, 4, 5};
    
    std::cout << "x: " << x << ", y: " << y << ", s: " << s << std::endl;
    std::cout << "v: ";
    for (const auto& elem : v) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;
}

void testNullptrAndEnums() {
    std::cout << "\n=== Testing nullptr and Scoped Enums ===" << std::endl;
    
    // nullptr测试
    std::cout << "Testing nullptr:" << std::endl;
    
    int* ptr1 = nullptr;
    std::unique_ptr<int> ptr2 = nullptr;
    
    if (ptr1 == nullptr) {
        std::cout << "ptr1 is null" << std::endl;
    }
    
    if (!ptr2) {
        std::cout << "ptr2 is null" << std::endl;
    }
    
    // 强类型枚举
    std::cout << "\nTesting scoped enums:" << std::endl;
    
    Color primaryColor = Color::RED;
    Status currentStatus = Status::RUNNING;
    
    // 不能隐式转换为int
    // int colorValue = primaryColor;  // 编译错误！
    int colorValue = static_cast<int>(primaryColor);  // 必须显式转换
    
    std::cout << "Primary color value: " << colorValue << std::endl;
    
    // 枚举比较
    if (primaryColor == Color::RED) {
        std::cout << "Color is RED" << std::endl;
    }
    
    if (currentStatus == Status::RUNNING) {
        std::cout << "Status is RUNNING" << std::endl;
    }
    
    // 枚举的作用域
    std::cout << "\nEnum scope test:" << std::endl;
    
    // 不同枚举类型不能比较
    // bool result = (primaryColor == currentStatus);  // 编译错误！
    
    auto processColor = [](Color c) {
        switch (c) {
            case Color::RED:
                std::cout << "Processing RED color" << std::endl;
                break;
            case Color::GREEN:
                std::cout << "Processing GREEN color" << std::endl;
                break;
            case Color::BLUE:
                std::cout << "Processing BLUE color" << std::endl;
                break;
            case Color::ALPHA:
                std::cout << "Processing ALPHA channel" << std::endl;
                break;
        }
    };
    
    processColor(Color::GREEN);
    processColor(Color::BLUE);
    
    // 枚举的底层类型
    std::cout << "\nEnum underlying types:" << std::endl;
    std::cout << "Color::RED underlying value: " << static_cast<int>(Color::RED) << std::endl;
    std::cout << "Status::PENDING underlying value: " << static_cast<int>(Status::PENDING) << std::endl;
}

void testConstructorFeatures() {
    std::cout << "\n=== Testing Constructor Features ===" << std::endl;
    
    // 委托构造函数
    std::cout << "Testing delegating constructors:" << std::endl;
    DerivedConstruct obj1;  // 调用委托构造函数
    obj1.display();
    
    std::cout << "\nTesting inherited constructors:" << std::endl;
    DerivedConstruct obj2("inherited", 42);  // 使用继承的构造函数
    obj2.display();
    
    std::cout << "\nTesting extended constructor:" << std::endl;
    DerivedConstruct obj3("extended", 100, 2.5);
    obj3.display();
}

void testThreadsAndAsync() {
    std::cout << "\n=== Testing Threads and Async ===" << std::endl;
    
    // 基础线程
    std::cout << "Testing basic threading:" << std::endl;
    
    auto worker = [](int id, int iterations) {
        for (int i = 0; i < iterations; ++i) {
            std::cout << "Thread " << id << " working... (" << i+1 << "/" << iterations << ")" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "Thread " << id << " finished!" << std::endl;
    };
    
    std::thread t1(worker, 1, 3);
    std::thread t2(worker, 2, 3);
    
    t1.join();
    t2.join();
    
    // 异步执行
    std::cout << "\nTesting async execution:" << std::endl;
    
    auto asyncTask = [](int x, int y) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return x * y + x + y;
    };
    
    // 异步启动任务
    auto future1 = std::async(std::launch::async, asyncTask, 10, 20);
    auto future2 = std::async(std::launch::async, asyncTask, 5, 8);
    
    std::cout << "Async tasks started, doing other work..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Getting async results:" << std::endl;
    
    std::cout << "Result 1: " << future1.get() << std::endl;
    std::cout << "Result 2: " << future2.get() << std::endl;
    
    // Promise/Future
    std::cout << "\nTesting promise/future:" << std::endl;
    
    std::promise<int> promise;
    std::future<int> future = promise.get_future();
    
    std::thread promiseThread([&promise]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        promise.set_value(42);
        std::cout << "Promise value set!" << std::endl;
    });
    
    std::cout << "Waiting for promise..." << std::endl;
    int promiseResult = future.get();
    std::cout << "Promise result: " << promiseResult << std::endl;
    
    promiseThread.join();
}

void testCpp17Features() {
    std::cout << "\n=== Testing C++17 Features ===" << std::endl;
    
    // 结构化绑定
    std::cout << "Testing structured bindings:" << std::endl;
    
    std::pair<int, std::string> pair = {42, "hello"};
    auto [number, text] = pair;
    std::cout << "Pair: " << number << ", " << text << std::endl;
    
    std::map<std::string, int> scores = {{"Alice", 85}, {"Bob", 92}, {"Charlie", 78}};
    for (const auto& [name, score] : scores) {
        std::cout << name << ": " << score << std::endl;
    }
    
    // std::optional
    std::cout << "\nTesting std::optional:" << std::endl;
    
    auto divide = [](double a, double b) -> std::optional<double> {
        if (b == 0.0) return std::nullopt;
        return a / b;
    };
    
    auto result1 = divide(10.0, 2.0);
    auto result2 = divide(10.0, 0.0);
    
    if (result1) {
        std::cout << "10 / 2 = " << *result1 << std::endl;
    }
    
    if (result2) {
        std::cout << "This shouldn't print" << std::endl;
    } else {
        std::cout << "Division by zero handled gracefully" << std::endl;
    }
    
    // std::variant
    std::cout << "\nTesting std::variant:" << std::endl;
    
    std::variant<int, double, std::string> var;
    
    var = 42;
    std::cout << "Variant holds int: " << std::get<int>(var) << std::endl;
    
    var = 3.14;
    std::cout << "Variant holds double: " << std::get<double>(var) << std::endl;
    
    var = std::string("hello");
    std::cout << "Variant holds string: " << std::get<std::string>(var) << std::endl;
    
    // 使用std::visit
    auto visitor = [](const auto& value) {
        std::cout << "Visiting: " << value << " (type: " << typeid(value).name() << ")" << std::endl;
    };
    
    var = 100;
    std::visit(visitor, var);
    
    var = 2.718;
    std::visit(visitor, var);
    
    // std::any
    std::cout << "\nTesting std::any:" << std::endl;
    
    std::any anything;
    
    anything = 42;
    std::cout << "any holds: " << std::any_cast<int>(anything) << std::endl;
    
    anything = std::string("world");
    std::cout << "any holds: " << std::any_cast<std::string>(anything) << std::endl;
    
    anything = 3.14159;
    try {
        // auto wrongType = std::any_cast<int>(anything);  // 抛出异常
        std::any_cast<int>(anything);  // 抛出异常
    } catch (const std::bad_any_cast& e) {
        std::cout << "Bad any_cast caught: " << e.what() << std::endl;
        std::cout << "Correct value: " << std::any_cast<double>(anything) << std::endl;
    }
}

void runAllModernCppTests() {
    std::cout << "\n########################################" << std::endl;
    std::cout << "#   Modern C++ Features Test Suite    #" << std::endl;
    std::cout << "########################################" << std::endl;
    
    testAutoKeyword();
    testConstexpr();
    testLambdaExpressions();
    testSmartPointers();
    testMoveSemantics();
    testRangeForAndInitLists();
    testNullptrAndEnums();
    testConstructorFeatures();
    testThreadsAndAsync();
    testCpp17Features();
    
    std::cout << "\n########################################" << std::endl;
    std::cout << "#   Modern C++ Tests Completed!       #" << std::endl;
    std::cout << "########################################" << std::endl;
}

} // namespace test_modern_cpp