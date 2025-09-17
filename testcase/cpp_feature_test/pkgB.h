#ifndef PKGB_H
#define PKGB_H

#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>

namespace test_templates
{

template <typename T> class Container
{
  private:
    std::vector<T> data_;

  public:
    void add(const T &item)
    {
        data_.push_back(item);
    }

    T &get(size_t index)
    {
        if (index >= data_.size())
        {
            throw std::out_of_range("Index out of range");
        }
        return data_[index];
    }

    size_t size() const
    {
        return data_.size();
    }

    template <typename U> void addConverted(const U &item)
    {
        data_.push_back(static_cast<T>(item));
    }

    typename std::vector<T>::iterator begin()
    {
        return data_.begin();
    }
    typename std::vector<T>::iterator end()
    {
        return data_.end();
    }
    typename std::vector<T>::const_iterator begin() const
    {
        return data_.begin();
    }
    typename std::vector<T>::const_iterator end() const
    {
        return data_.end();
    }
};

template <typename T> T findMax(const std::vector<T> &vec)
{
    if (vec.empty())
    {
        throw std::invalid_argument("Empty vector");
    }

    T maxVal = vec[0];
    for (const auto &item : vec)
    {
        if (item > maxVal)
        {
            maxVal = item;
        }
    }
    return maxVal;
}

template <> inline std::string findMax<std::string>(const std::vector<std::string> &vec)
{
    if (vec.empty())
    {
        throw std::invalid_argument("Empty vector");
    }

    std::string maxVal = vec[0];
    for (const auto &item : vec)
    {
        if (item.length() > maxVal.length())
        {
            maxVal = item;
        }
    }
    return maxVal;
}

template <typename T> struct is_container
{
  private:
    template <typename C>
    static auto test(int) -> decltype(std::declval<C>().begin(), std::declval<C>().end(), std::true_type{});

    template <typename> static std::false_type test(...);

  public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T> typename std::enable_if<std::is_arithmetic<T>::value, T>::type processValue(T value)
{
    std::cout << "Processing arithmetic value: " << value << std::endl;
    return value * 2;
}

template <typename T> typename std::enable_if<!std::is_arithmetic<T>::value, T>::type processValue(T value)
{
    std::cout << "Processing non-arithmetic value: " << value << std::endl;
    return value;
}

template <typename... Args> void printArgs(Args... args)
{
    ((std::cout << args << " "), ...);
    std::cout << std::endl;
}

template <typename T> void printArgsRecursive(T &&t)
{
    std::cout << t << std::endl;
}

template <typename T, typename... Args> void printArgsRecursive(T &&t, Args &&...args)
{
    std::cout << t << " ";
    printArgsRecursive(args...);
}

template <typename T> T sum(T value)
{
    return value;
}

template <typename T, typename... Args> T sum(T first, Args... args)
{
    return first + sum(args...);
}

template <int N> struct Factorial
{
    static constexpr int value = N * Factorial<N - 1>::value;
};

template <> struct Factorial<0>
{
    static constexpr int value = 1;
};

template <int N> struct Fibonacci
{
    static constexpr int value = Fibonacci<N - 1>::value + Fibonacci<N - 2>::value;
};

template <> struct Fibonacci<0>
{
    static constexpr int value = 0;
};

template <> struct Fibonacci<1>
{
    static constexpr int value = 1;
};

template <typename... Types> struct TypeList
{
};

template <typename T, typename... Rest> struct TypeList<T, Rest...>
{
    using Head = T;
    using Tail = TypeList<Rest...>;
    static constexpr size_t size = 1 + sizeof...(Rest);
};

template <> struct TypeList<>
{
    static constexpr size_t size = 0;
};

template <typename SortStrategy> class Sorter
{
  private:
    SortStrategy strategy_;

  public:
    template <typename Iterator> void sort(Iterator begin, Iterator end)
    {
        strategy_.sort(begin, end);
    }
};

struct BubbleSortStrategy
{
    template <typename Iterator> void sort(Iterator begin, Iterator end)
    {
        std::cout << "Using Bubble Sort Strategy" << std::endl;

        for (auto i = begin; i != end; ++i)
        {
            for (auto j = begin; j != end - 1; ++j)
            {
                if (*j > *(j + 1))
                {
                    std::swap(*j, *(j + 1));
                }
            }
        }
    }
};

struct QuickSortStrategy
{
    template <typename Iterator> void sort(Iterator begin, Iterator end)
    {
        std::cout << "Using Quick Sort Strategy (std::sort)" << std::endl;
        std::sort(begin, end);
    }
};

class DynamicBinder
{
  private:
    std::map<std::string, std::function<void()>> functions_;

  public:
    template <typename Func> void bind(const std::string &name, Func &&func)
    {
        functions_[name] = std::forward<Func>(func);
    }

    void call(const std::string &name)
    {
        auto it = functions_.find(name);
        if (it != functions_.end())
        {
            it->second();
        }
        else
        {
            std::cout << "Function '" << name << "' not found!" << std::endl;
        }
    }

    void listFunctions() const
    {
        std::cout << "Available functions: ";
        for (const auto &pair : functions_)
        {
            std::cout << pair.first << " ";
        }
        std::cout << std::endl;
    }
};

template <typename T> struct TypeTraits
{
    static constexpr bool is_pointer = false;
    static constexpr bool is_reference = false;
    static constexpr bool is_const = false;
    using base_type = T;
};

template <typename T> struct TypeTraits<T *>
{
    static constexpr bool is_pointer = true;
    static constexpr bool is_reference = false;
    static constexpr bool is_const = false;
    using base_type = T;
};

template <typename T> struct TypeTraits<T &>
{
    static constexpr bool is_pointer = false;
    static constexpr bool is_reference = true;
    static constexpr bool is_const = false;
    using base_type = T;
};

template <typename T> struct TypeTraits<const T>
{
    static constexpr bool is_pointer = TypeTraits<T>::is_pointer;
    static constexpr bool is_reference = TypeTraits<T>::is_reference;
    static constexpr bool is_const = true;
    using base_type = typename TypeTraits<T>::base_type;
};

void testBasicTemplates();
void testAdvancedTemplates();
void testVariadicTemplates();
void testMetaprogramming();
void testTemplateStrategies();
void testDynamicBinding();
void testTypeTraits();
void runAllTemplateTests();

} // namespace test_templates

#endif
