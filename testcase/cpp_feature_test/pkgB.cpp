#include "pkgB.h"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <chrono>

namespace test_templates {

void testBasicTemplates() {
    std::cout << "\n=== Testing Basic Templates ===" << std::endl;
    
    // 测试容器模板类
    std::cout << "Testing Container template class:" << std::endl;
    Container<int> intContainer;
    intContainer.add(10);
    intContainer.add(20);
    intContainer.add(30);
    
    std::cout << "Int container contents: ";
    for (const auto& item : intContainer) {
        std::cout << item << " ";
    }
    std::cout << "(size: " << intContainer.size() << ")" << std::endl;
    
    Container<std::string> stringContainer;
    stringContainer.add("Hello");
    stringContainer.add("Template");
    stringContainer.add("World");
    
    std::cout << "String container contents: ";
    for (const auto& item : stringContainer) {
        std::cout << item << " ";
    }
    std::cout << "(size: " << stringContainer.size() << ")" << std::endl;
    
    // 测试模板成员函数
    std::cout << "\nTesting template member function:" << std::endl;
    intContainer.addConverted(3.14);  // double -> int
    intContainer.addConverted('A');   // char -> int
    std::cout << "After adding converted values: ";
    for (const auto& item : intContainer) {
        std::cout << item << " ";
    }
    std::cout << std::endl;
    
    // 测试函数模板
    std::cout << "\nTesting function templates:" << std::endl;
    std::vector<int> intVec = {5, 2, 8, 1, 9, 3};
    std::cout << "Max int: " << findMax(intVec) << std::endl;
    
    std::vector<double> doubleVec = {3.14, 2.71, 1.41, 2.23};
    std::cout << "Max double: " << findMax(doubleVec) << std::endl;
    
    // 测试模板特化
    std::vector<std::string> stringVec = {"short", "medium", "very_long_string", "tiny"};
    std::cout << "Longest string: " << findMax(stringVec) << std::endl;
}

void testAdvancedTemplates() {
    std::cout << "\n=== Testing Advanced Templates (SFINAE & Type Traits) ===" << std::endl;
    
    // 测试类型萃取
    std::cout << "Testing is_container trait:" << std::endl;
    std::cout << "std::vector<int> is container: " << is_container<std::vector<int>>::value << std::endl;
    std::cout << "int is container: " << is_container<int>::value << std::endl;
    std::cout << "std::string is container: " << is_container<std::string>::value << std::endl;
    
    // 测试SFINAE
    std::cout << "\nTesting SFINAE with processValue:" << std::endl;
    
    auto result1 = processValue(42);
    std::cout << "Result for int: " << result1 << std::endl;
    
    auto result2 = processValue(3.14);
    std::cout << "Result for double: " << result2 << std::endl;
    
    auto result3 = processValue(std::string("Hello"));
    std::cout << "Result for string: " << result3 << std::endl;
    
    // 测试标准库类型萃取
    std::cout << "\nTesting standard type traits:" << std::endl;
    std::cout << "is_arithmetic<int>: " << std::is_arithmetic<int>::value << std::endl;
    std::cout << "is_arithmetic<std::string>: " << std::is_arithmetic<std::string>::value << std::endl;
    std::cout << "is_pointer<int*>: " << std::is_pointer<int*>::value << std::endl;
    std::cout << "is_reference<int&>: " << std::is_reference<int&>::value << std::endl;
}

void testVariadicTemplates() {
    std::cout << "\n=== Testing Variadic Templates ===" << std::endl;
    
    // 测试变长参数打印
    std::cout << "Testing variadic print functions:" << std::endl;
    
    std::cout << "printArgs: ";
    printArgs(1, 2.5, "hello", 'c', true);
    
    std::cout << "printArgsRecursive: ";
    printArgsRecursive(42, 3.14, "world", 'x');
    
    // 测试变长模板求和
    std::cout << "\nTesting variadic sum:" << std::endl;
    auto sum1 = sum(1, 2, 3, 4, 5);
    std::cout << "sum(1,2,3,4,5) = " << sum1 << std::endl;
    
    auto sum2 = sum(1.1, 2.2, 3.3);
    std::cout << "sum(1.1,2.2,3.3) = " << sum2 << std::endl;
    
    auto sum3 = sum(std::string("Hello"), std::string(" "), std::string("World"));
    std::cout << "sum(\"Hello\", \" \", \"World\") = " << sum3 << std::endl;
    
    // 测试TypeList
    std::cout << "\nTesting TypeList metaprogramming:" << std::endl;
    using MyTypes = TypeList<int, double, std::string, char>;
    std::cout << "TypeList size: " << MyTypes::size << std::endl;
    
    // 可以通过编译期检查类型
    static_assert(std::is_same<MyTypes::Head, int>::value, "Head should be int");
    std::cout << "TypeList Head is int: " << std::is_same<MyTypes::Head, int>::value << std::endl;
}

void testMetaprogramming() {
    std::cout << "\n=== Testing Template Metaprogramming ===" << std::endl;
    
    // 测试编译期计算
    std::cout << "Compile-time calculations:" << std::endl;
    
    constexpr int fact5 = Factorial<5>::value;
    constexpr int fact10 = Factorial<10>::value;
    std::cout << "Factorial<5> = " << fact5 << std::endl;
    std::cout << "Factorial<10> = " << fact10 << std::endl;
    
    constexpr int fib10 = Fibonacci<10>::value;
    constexpr int fib15 = Fibonacci<15>::value;
    std::cout << "Fibonacci<10> = " << fib10 << std::endl;
    std::cout << "Fibonacci<15> = " << fib15 << std::endl;
    
    // 展示编译期vs运行期的区别
    std::cout << "\nCompile-time vs Runtime comparison:" << std::endl;
    
    // 编译期计算（没有运行时开销）
    auto start = std::chrono::high_resolution_clock::now();
    constexpr int compile_time_result = Factorial<12>::value;
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Compile-time Factorial<12> = " << compile_time_result 
              << " (measurement overhead only)" << std::endl;
    
    // 运行时递归计算（有运行时开销）
    std::function<int(int)> runtime_factorial = [](int n) -> int {
        std::function<int(int)> factorial_impl;
        factorial_impl = [&factorial_impl](int x) -> int {
            return x <= 1 ? 1 : x * factorial_impl(x - 1);
        };
        return factorial_impl(n);
    };
    
    start = std::chrono::high_resolution_clock::now();
    int runtime_result = runtime_factorial(12);
    end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    std::cout << "Runtime factorial(12) = " << runtime_result 
              << " (took " << duration.count() << " ns)" << std::endl;
}

void testTemplateStrategies() {
    std::cout << "\n=== Testing Template Strategy Pattern ===" << std::endl;
    
    // 创建测试数据
    std::vector<int> data1 = {64, 34, 25, 12, 22, 11, 90};
    std::vector<int> data2 = data1;  // 复制用于第二种策略
    
    std::cout << "Original data: ";
    for (int val : data1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
    
    // 使用冒泡排序策略
    std::cout << "\nUsing Bubble Sort Strategy:" << std::endl;
    Sorter<BubbleSortStrategy> bubbleSorter;
    bubbleSorter.sort(data1.begin(), data1.end());
    
    std::cout << "Sorted data: ";
    for (int val : data1) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
    
    // 使用快速排序策略
    std::cout << "\nUsing Quick Sort Strategy:" << std::endl;
    Sorter<QuickSortStrategy> quickSorter;
    quickSorter.sort(data2.begin(), data2.end());
    
    std::cout << "Sorted data: ";
    for (int val : data2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
    
    // 展示策略模式的优势：编译期选择，零运行时开销
    std::cout << "\nStrategy pattern benefits:" << std::endl;
    std::cout << "- Compile-time strategy selection" << std::endl;
    std::cout << "- Zero runtime overhead for strategy dispatch" << std::endl;
    std::cout << "- Type safety and optimization opportunities" << std::endl;
}

void testDynamicBinding() {
    std::cout << "\n=== Testing Dynamic Binding ===" << std::endl;
    
    DynamicBinder binder;
    
    // 绑定各种类型的函数
    std::cout << "Binding various function types:" << std::endl;
    
    // Lambda表达式
    binder.bind("lambda", []() {
        std::cout << "Lambda function executed!" << std::endl;
    });
    
    // 普通函数
    auto regularFunc = []() {
        std::cout << "Regular function executed!" << std::endl;
    };
    binder.bind("regular", regularFunc);
    
    // 带捕获的Lambda
    int counter = 0;
    binder.bind("counter", [&counter]() {
        std::cout << "Counter function executed! Count: " << ++counter << std::endl;
    });
    
    // 绑定成员函数
    std::string message = "Hello from member function!";
    binder.bind("member", [&message]() {
        std::cout << message << std::endl;
    });
    
    // 复杂的函数对象
    binder.bind("complex", [&]() {
        std::cout << "Complex function with multiple captures: " 
                  << "counter=" << counter << ", message=" << message << std::endl;
    });
    
    // 列出所有绑定的函数
    std::cout << "\nListing all bound functions:" << std::endl;
    binder.listFunctions();
    
    // 动态调用函数
    std::cout << "\nDynamic function calls:" << std::endl;
    binder.call("lambda");
    binder.call("regular");
    binder.call("counter");
    binder.call("counter");  // 调用两次看计数器
    binder.call("member");
    binder.call("complex");
    
    // 尝试调用不存在的函数
    std::cout << "\nTrying to call non-existent function:" << std::endl;
    binder.call("nonexistent");
    
    std::cout << "\nDynamic binding advantages:" << std::endl;
    std::cout << "- Runtime function registration and dispatch" << std::endl;
    std::cout << "- Type erasure through std::function" << std::endl;
    std::cout << "- Support for closures and captures" << std::endl;
}

void testTypeTraits() {
    std::cout << "\n=== Testing Custom Type Traits ===" << std::endl;
    
    // 测试自定义类型萃取
    std::cout << "Testing custom TypeTraits:" << std::endl;
    
    using IntTraits = TypeTraits<int>;
    using IntPtrTraits = TypeTraits<int*>;
    using IntRefTraits = TypeTraits<int&>;
    using ConstIntTraits = TypeTraits<const int>;
    using ConstIntPtrTraits = TypeTraits<const int*>;
    
    std::cout << "int traits:" << std::endl;
    std::cout << "  is_pointer: " << IntTraits::is_pointer << std::endl;
    std::cout << "  is_reference: " << IntTraits::is_reference << std::endl;
    std::cout << "  is_const: " << IntTraits::is_const << std::endl;
    
    std::cout << "int* traits:" << std::endl;
    std::cout << "  is_pointer: " << IntPtrTraits::is_pointer << std::endl;
    std::cout << "  is_reference: " << IntPtrTraits::is_reference << std::endl;
    std::cout << "  is_const: " << IntPtrTraits::is_const << std::endl;
    
    std::cout << "int& traits:" << std::endl;
    std::cout << "  is_pointer: " << IntRefTraits::is_pointer << std::endl;
    std::cout << "  is_reference: " << IntRefTraits::is_reference << std::endl;
    std::cout << "  is_const: " << IntRefTraits::is_const << std::endl;
    
    std::cout << "const int traits:" << std::endl;
    std::cout << "  is_pointer: " << ConstIntTraits::is_pointer << std::endl;
    std::cout << "  is_reference: " << ConstIntTraits::is_reference << std::endl;
    std::cout << "  is_const: " << ConstIntTraits::is_const << std::endl;
    
    std::cout << "const int* traits:" << std::endl;
    std::cout << "  is_pointer: " << ConstIntPtrTraits::is_pointer << std::endl;
    std::cout << "  is_reference: " << ConstIntPtrTraits::is_reference << std::endl;
    std::cout << "  is_const: " << ConstIntPtrTraits::is_const << std::endl;
    
    // 展示类型萃取的实际应用
    std::cout << "\nType traits practical applications:" << std::endl;
    std::cout << "- Template specialization based on type properties" << std::endl;
    std::cout << "- SFINAE for conditional compilation" << std::endl;
    std::cout << "- Generic programming with type-dependent behavior" << std::endl;
}

void runAllTemplateTests() {
    std::cout << "\n########################################" << std::endl;
    std::cout << "#   C++ Template Features Test Suite  #" << std::endl;
    std::cout << "########################################" << std::endl;
    
    testBasicTemplates();
    testAdvancedTemplates();
    testVariadicTemplates();
    testMetaprogramming();
    testTemplateStrategies();
    testDynamicBinding();
    testTypeTraits();
    
    std::cout << "\n########################################" << std::endl;
    std::cout << "#   Template Tests Completed!         #" << std::endl;
    std::cout << "########################################" << std::endl;
}

} // namespace test_templates
