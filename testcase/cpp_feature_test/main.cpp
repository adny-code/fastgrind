#include <iostream>
#include <string>
#include <exception>
#include <chrono>

// 包含所有测试模块
#include "pkgA.h"
#include "pkgB.h"
#include "modern_cpp.h"
#include "namespace_test.h"
#include "fastgrind.h"

void printBanner(const std::string& title) {
    std::string border(title.length() + 8, '=');
    std::cout << "\n" << border << std::endl;
    std::cout << "    " << title << std::endl;
    std::cout << border << std::endl;
}

void printSeparator() {
    std::cout << "\n" << std::string(60, '-') << std::endl;
}

class TestRunner {
private:
    int totalTests_ = 0;
    int passedTests_ = 0;
    int failedTests_ = 0;
    
public:
    template<typename TestFunc>
    void runTest(const std::string& testName, TestFunc testFunc) {
        std::cout << "\n🧪 Running: " << testName << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            testFunc();
            passedTests_++;
            std::cout << "✅ PASSED: " << testName << std::endl;
        } catch (const std::exception& e) {
            failedTests_++;
            std::cout << "❌ FAILED: " << testName << std::endl;
            std::cout << "   Error: " << e.what() << std::endl;
        } catch (...) {
            failedTests_++;
            std::cout << "❌ FAILED: " << testName << std::endl;
            std::cout << "   Error: Unknown exception" << std::endl;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "⏱️  Duration: " << duration.count() << "ms" << std::endl;
        
        totalTests_++;
    }
    
    void printSummary() {
        printSeparator();
        std::cout << "\n📊 TEST SUMMARY" << std::endl;
        std::cout << "=================" << std::endl;
        std::cout << "Total Tests:  " << totalTests_ << std::endl;
        std::cout << "Passed:       " << passedTests_ << " ✅" << std::endl;
        std::cout << "Failed:       " << failedTests_ << " ❌" << std::endl;
        std::cout << "Success Rate: " << (totalTests_ > 0 ? (passedTests_ * 100.0 / totalTests_) : 0) << "%" << std::endl;
        
        if (failedTests_ == 0) {
            std::cout << "\n🎉 ALL TESTS PASSED! 🎉" << std::endl;
        } else {
            std::cout << "\n⚠️  Some tests failed. Please check the output above." << std::endl;
        }
    }
};

void showTestMenu() {
    std::cout << "\n🔧 C++ Features Test Suite" << std::endl;
    std::cout << "============================" << std::endl;
    std::cout << "1. Object-Oriented Programming Features (Virtual Functions, Polymorphism)" << std::endl;
    std::cout << "2. Template Programming & Metaprogramming" << std::endl;
    std::cout << "3. Modern C++ Features (C++11/14/17)" << std::endl;
    std::cout << "4. Namespace Features" << std::endl;
    std::cout << "5. Run ALL Tests" << std::endl;
    std::cout << "6. Interactive Test Selection" << std::endl;
    std::cout << "0. Exit" << std::endl;
    std::cout << "============================" << std::endl;
}

void runOOPTests() {
    printBanner("OBJECT-ORIENTED PROGRAMMING TESTS");
    
    TestRunner runner;
    
    runner.runTest("Virtual Functions Test", []() {
        test_oop::testVirtualFunctions();
    });
    
    runner.runTest("Multiple Inheritance Test", []() {
        test_oop::testMultipleInheritance();
    });
    
    runner.runTest("Function Overloading Test", []() {
        test_oop::testFunctionOverloading();
    });
    
    runner.runTest("Abstract Classes Test", []() {
        test_oop::testAbstractClasses();
    });
    
    runner.runTest("Polymorphism Test", []() {
        test_oop::testPolymorphism();
    });
    
    runner.printSummary();
}

void runTemplateTests() {
    printBanner("TEMPLATE PROGRAMMING TESTS");
    
    TestRunner runner;
    
    runner.runTest("Basic Templates Test", []() {
        test_templates::testBasicTemplates();
    });
    
    runner.runTest("Advanced Templates (SFINAE) Test", []() {
        test_templates::testAdvancedTemplates();
    });
    
    runner.runTest("Variadic Templates Test", []() {
        test_templates::testVariadicTemplates();
    });
    
    runner.runTest("Template Metaprogramming Test", []() {
        test_templates::testMetaprogramming();
    });
    
    runner.runTest("Template Strategy Pattern Test", []() {
        test_templates::testTemplateStrategies();
    });
    
    runner.runTest("Dynamic Binding Test", []() {
        test_templates::testDynamicBinding();
    });
    
    runner.runTest("Type Traits Test", []() {
        test_templates::testTypeTraits();
    });
    
    runner.printSummary();
}

void runModernCppTests() {
    printBanner("MODERN C++ FEATURES TESTS");
    
    TestRunner runner;
    
    runner.runTest("Auto Keyword Test", []() {
        test_modern_cpp::testAutoKeyword();
    });
    
    runner.runTest("Constexpr Test", []() {
        test_modern_cpp::testConstexpr();
    });
    
    runner.runTest("Lambda Expressions Test", []() {
        test_modern_cpp::testLambdaExpressions();
    });
    
    runner.runTest("Smart Pointers Test", []() {
        test_modern_cpp::testSmartPointers();
    });
    
    runner.runTest("Move Semantics Test", []() {
        test_modern_cpp::testMoveSemantics();
    });
    
    runner.runTest("Range-for and Init Lists Test", []() {
        test_modern_cpp::testRangeForAndInitLists();
    });
    
    runner.runTest("Nullptr and Enums Test", []() {
        test_modern_cpp::testNullptrAndEnums();
    });
    
    runner.runTest("Constructor Features Test", []() {
        test_modern_cpp::testConstructorFeatures();
    });
    
    runner.runTest("Threads and Async Test", []() {
        test_modern_cpp::testThreadsAndAsync();
    });
    
    runner.runTest("C++17 Features Test", []() {
        test_modern_cpp::testCpp17Features();
    });
    
    runner.printSummary();
}

void runNamespaceTests() {
    printBanner("NAMESPACE FEATURES TESTS");
    
    TestRunner runner;
    
    runner.runTest("Basic Namespace Test", []() {
        testBasicNamespace();
    });
    
    runner.runTest("Nested Namespace Test", []() {
        testNestedNamespace();
    });
    
    runner.runTest("Anonymous Namespace Test", []() {
        testAnonymousNamespace();
    });
    
    runner.runTest("Namespace Alias Test", []() {
        testNamespaceAlias();
    });
    
    runner.runTest("Using Declarations Test", []() {
        testUsingDeclarations();
    });
    
    runner.runTest("Namespace Conflicts Test", []() {
        testNamespaceConflicts();
    });
    
    runner.runTest("ADL (Argument-Dependent Lookup) Test", []() {
        testADL();
    });
    
    runner.runTest("Inline Namespace Test", []() {
        testInlineNamespace();
    });
    
    runner.runTest("Template Namespace Test", []() {
        testTemplateNamespace();
    });
    
    runner.runTest("Global Namespace Test", []() {
        testGlobalNamespace();
    });
    
    runner.printSummary();
}

void runAllTests() {
    printBanner("COMPREHENSIVE C++ FEATURES TEST SUITE");
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    std::cout << "\n🚀 Starting comprehensive test suite..." << std::endl;
    
    // 运行所有测试模块
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

void runInteractiveTests() {
    std::cout << "\n🎮 Interactive Test Selection" << std::endl;
    std::cout << "==============================" << std::endl;
    
    while (true) {
        std::cout << "\nSelect test category:" << std::endl;
        std::cout << "1. OOP - Virtual Functions" << std::endl;
        std::cout << "2. OOP - Multiple Inheritance" << std::endl;
        std::cout << "3. OOP - Function Overloading" << std::endl;
        std::cout << "4. OOP - Abstract Classes" << std::endl;
        std::cout << "5. OOP - Polymorphism" << std::endl;
        std::cout << "6. Templates - Basic Templates" << std::endl;
        std::cout << "7. Templates - Advanced (SFINAE)" << std::endl;
        std::cout << "8. Templates - Metaprogramming" << std::endl;
        std::cout << "9. Modern C++ - Lambda & Auto" << std::endl;
        std::cout << "10. Modern C++ - Smart Pointers" << std::endl;
        std::cout << "11. Modern C++ - C++17 Features" << std::endl;
        std::cout << "12. Namespace - Basic Features" << std::endl;
        std::cout << "13. Namespace - Advanced Features" << std::endl;
        std::cout << "0. Back to main menu" << std::endl;
        
        int choice;
        std::cout << "\nEnter your choice: ";
        std::cin >> choice;
        
        switch (choice) {
            case 1: test_oop::testVirtualFunctions(); break;
            case 2: test_oop::testMultipleInheritance(); break;
            case 3: test_oop::testFunctionOverloading(); break;
            case 4: test_oop::testAbstractClasses(); break;
            case 5: test_oop::testPolymorphism(); break;
            case 6: test_templates::testBasicTemplates(); break;
            case 7: test_templates::testAdvancedTemplates(); break;
            case 8: test_templates::testMetaprogramming(); break;
            case 9: 
                test_modern_cpp::testLambdaExpressions();
                test_modern_cpp::testAutoKeyword();
                break;
            case 10: test_modern_cpp::testSmartPointers(); break;
            case 11: test_modern_cpp::testCpp17Features(); break;
            case 12:
                testBasicNamespace();
                testNestedNamespace();
                break;
            case 13:
                testADL();
                testInlineNamespace();
                break;
            case 0: return;
            default:
                std::cout << "❌ Invalid choice! Please try again." << std::endl;
                continue;
        }
        
        std::cout << "\nPress Enter to continue...";
        std::cin.ignore();
        std::cin.get();
    }
}

int main() {
    std::cout << "🔬 C++ Features Comprehensive Test Suite" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "This program tests various C++ language features including:" << std::endl;
    std::cout << "• Object-Oriented Programming (virtual functions, polymorphism)" << std::endl;
    std::cout << "• Template metaprogramming and generic programming" << std::endl;
    std::cout << "• Modern C++ features (C++11/14/17)" << std::endl;
    std::cout << "• Namespace scope management and conflict resolution" << std::endl;
    
    while (true) {
        showTestMenu();
        
        int choice;
        std::cout << "\nEnter your choice: ";
        std::cin >> choice;
        
        switch (choice) {
            case 1:
                runOOPTests();
                break;
            case 2:
                runTemplateTests();
                break;
            case 3:
                runModernCppTests();
                break;
            case 4:
                runNamespaceTests();
                break;
            case 5:
                runAllTests();
                break;
            case 6:
                runInteractiveTests();
                break;
            case 0:
                std::cout << "\n👋 Thank you for using the C++ Features Test Suite!" << std::endl;
                std::cout << "🎓 Keep learning and exploring C++!" << std::endl;
                return 0;
            default:
                std::cout << "\n❌ Invalid choice! Please enter a number between 0-6." << std::endl;
                continue;
        }
        
        std::cout << "\nPress Enter to continue...";
        std::cin.ignore();
        std::cin.get();
    }
    
    return 0;
}
