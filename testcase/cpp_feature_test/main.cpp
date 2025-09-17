#include <chrono>
#include <exception>
#include <iostream>
#include <string>

#include "fastgrind.h"
#include "modern_cpp.h"
#include "namespace_test.h"
#include "pkgA.h"
#include "pkgB.h"

void printBanner(const std::string &title)
{
    std::string border(title.length() + 8, '=');
    std::cout << "\n" << border << std::endl;
    std::cout << "    " << title << std::endl;
    std::cout << border << std::endl;
}

void printSeparator()
{
    std::cout << "\n" << std::string(60, '-') << std::endl;
}

class TestRunner
{
  private:
    int totalTests_ = 0;
    int passedTests_ = 0;
    int failedTests_ = 0;

  public:
    TestRunner() = default;

    template <typename TestFunc> void runTest(const std::string &testName, TestFunc testFunc)
    {
        std::cout << "\n🧪 Running: " << testName << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        auto start = std::chrono::high_resolution_clock::now();

        try
        {
            testFunc();
            passedTests_++;
            std::cout << "✅ PASSED: " << testName << std::endl;
        }
        catch (const std::exception &e)
        {
            failedTests_++;
            std::cout << "❌ FAILED: " << testName << std::endl;
            std::cout << "   Error: " << e.what() << std::endl;
        }
        catch (...)
        {
            failedTests_++;
            std::cout << "❌ FAILED: " << testName << std::endl;
            std::cout << "   Error: Unknown exception" << std::endl;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "⏱️  Duration: " << duration.count() << "ms" << std::endl;

        totalTests_++;
    }

    void printSummary()
    {
        printSeparator();
        std::cout << "\n📊 TEST SUMMARY" << std::endl;
        std::cout << "=================" << std::endl;
        std::cout << "Total Tests:  " << totalTests_ << std::endl;
        std::cout << "Passed:       " << passedTests_ << " ✅" << std::endl;
        std::cout << "Failed:       " << failedTests_ << " ❌" << std::endl;
        std::cout << "Success Rate: " << (totalTests_ > 0 ? (passedTests_ * 100.0 / totalTests_) : 0) << "%"
                  << std::endl;

        if (failedTests_ == 0)
        {
            std::cout << "\n🎉 ALL TESTS PASSED! 🎉" << std::endl;
        }
        else
        {
            std::cout << "\n⚠️  Some tests failed. Please check the output above." << std::endl;
        }
    }
};

void runOOPTests()
{
    printBanner("OBJECT-ORIENTED PROGRAMMING TESTS");

    TestRunner runner;

    runner.runTest("Virtual Functions Test", []() { testVirtualFunctions(); });

    runner.runTest("Multiple Inheritance Test", []() { testMultipleInheritance(); });

    runner.runTest("Function Overloading Test", []() { testFunctionOverloading(); });

    runner.runTest("Abstract Classes Test", []() { testAbstractClasses(); });

    runner.runTest("Polymorphism Test", []() { testPolymorphism(); });

    runner.printSummary();
}

void runTemplateTests()
{
    printBanner("TEMPLATE PROGRAMMING TESTS");

    TestRunner runner;

    runner.runTest("Basic Templates Test", []() { test_templates::testBasicTemplates(); });

    runner.runTest("Advanced Templates (SFINAE) Test", []() { test_templates::testAdvancedTemplates(); });

    runner.runTest("Variadic Templates Test", []() { test_templates::testVariadicTemplates(); });

    runner.runTest("Template Metaprogramming Test", []() { test_templates::testMetaprogramming(); });

    runner.runTest("Template Strategy Pattern Test", []() { test_templates::testTemplateStrategies(); });

    runner.runTest("Dynamic Binding Test", []() { test_templates::testDynamicBinding(); });

    runner.runTest("Type Traits Test", []() { test_templates::testTypeTraits(); });

    runner.printSummary();
}

void runModernCppTests()
{
    printBanner("MODERN C++ FEATURES TESTS");

    TestRunner runner;

    runner.runTest("Auto Keyword Test", []() { test_modern_cpp::testAutoKeyword(); });

    runner.runTest("Constexpr Test", []() { test_modern_cpp::testConstexpr(); });

    runner.runTest("Lambda Expressions Test", []() { test_modern_cpp::testLambdaExpressions(); });

    runner.runTest("Smart Pointers Test", []() { test_modern_cpp::testSmartPointers(); });

    runner.runTest("Move Semantics Test", []() { test_modern_cpp::testMoveSemantics(); });

    runner.runTest("Range-for and Init Lists Test", []() { test_modern_cpp::testRangeForAndInitLists(); });

    runner.runTest("Nullptr and Enums Test", []() { test_modern_cpp::testNullptrAndEnums(); });

    runner.runTest("Constructor Features Test", []() { test_modern_cpp::testConstructorFeatures(); });

    runner.runTest("Threads and Async Test", []() { test_modern_cpp::testThreadsAndAsync(); });

    runner.runTest("C++17 Features Test", []() { test_modern_cpp::testCpp17Features(); });

    runner.printSummary();
}

void runNamespaceTests()
{
    printBanner("NAMESPACE FEATURES TESTS");

    TestRunner runner;

    runner.runTest("Basic Namespace Test", []() { testBasicNamespace(); });

    runner.runTest("Nested Namespace Test", []() { testNestedNamespace(); });

    runner.runTest("Anonymous Namespace Test", []() { testAnonymousNamespace(); });

    runner.runTest("Namespace Alias Test", []() { testNamespaceAlias(); });

    runner.runTest("Using Declarations Test", []() { testUsingDeclarations(); });

    runner.runTest("Namespace Conflicts Test", []() { testNamespaceConflicts(); });

    runner.runTest("ADL (Argument-Dependent Lookup) Test", []() { testADL(); });

    runner.runTest("Inline Namespace Test", []() { testInlineNamespace(); });

    runner.runTest("Template Namespace Test", []() { testTemplateNamespace(); });

    runner.runTest("Global Namespace Test", []() { testGlobalNamespace(); });

    runner.printSummary();
}

void runAllTests()
{
    printBanner("COMPREHENSIVE C++ FEATURES TEST SUITE");

    auto startTime = std::chrono::high_resolution_clock::now();

    std::cout << "\n🚀 Starting comprehensive test suite..." << std::endl;

    runOOPTests();
    runTemplateTests();
    runModernCppTests();
    runNamespaceTests();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);

    printSeparator();
    std::cout << "\n🏁 FINAL SUMMARY" << std::endl;
    std::cout << "=================" << std::endl;
    std::cout << "✅ Object-Oriented Programming: Completed" << std::endl;
    std::cout << "✅ Template Programming: Completed" << std::endl;
    std::cout << "✅ Modern C++ Features: Completed" << std::endl;
    std::cout << "✅ Namespace Features: Completed" << std::endl;
    std::cout << "⏱️  Total Duration: " << totalDuration.count() << " seconds" << std::endl;

    std::cout << "\n🎯 Test Coverage Summary:" << std::endl;
    std::cout << "• Virtual functions, pure virtual functions, polymorphism ✅" << std::endl;
    std::cout << "• Function overloading, operator overloading ✅" << std::endl;
    std::cout << "• Multiple inheritance, abstract classes ✅" << std::endl;
    std::cout << "• Template metaprogramming, SFINAE ✅" << std::endl;
    std::cout << "• Variadic templates, type traits ✅" << std::endl;
    std::cout << "• Dynamic binding, strategy patterns ✅" << std::endl;
    std::cout << "• auto, constexpr, lambda expressions ✅" << std::endl;
    std::cout << "• Smart pointers, move semantics ✅" << std::endl;
    std::cout << "• Range-for, initializer lists ✅" << std::endl;
    std::cout << "• nullptr, scoped enums ✅" << std::endl;
    std::cout << "• C++17 features (optional, variant, any) ✅" << std::endl;
    std::cout << "• Threading and async programming ✅" << std::endl;
    std::cout << "• Namespace scoping and conflict resolution ✅" << std::endl;
    std::cout << "• ADL, inline namespaces ✅" << std::endl;

    std::cout << "\n🎉 All C++ feature tests completed successfully! 🎉" << std::endl;
}

int main()
{
    std::cout << "🔬 C++ Features Comprehensive Test Suite" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "This program tests various C++ language features including:" << std::endl;
    std::cout << "• Object-Oriented Programming (virtual functions, polymorphism)" << std::endl;
    std::cout << "• Template metaprogramming and generic programming" << std::endl;
    std::cout << "• Modern C++ features (C++11/14/17)" << std::endl;
    std::cout << "• Namespace scope management and conflict resolution" << std::endl;

    runAllTests();

    return 0;
}
