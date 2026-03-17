#ifndef CPP_FEATURE_COMPAT_H
#define CPP_FEATURE_COMPAT_H

#include <memory>
#include <utility>

#if __cplusplus >= 201402L
    #define FASTGRIND_TESTCASE_HAS_CXX14 1
#endif

#if __cplusplus >= 201703L
    #define FASTGRIND_TESTCASE_HAS_CXX17 1
#endif

namespace fastgrind_testcase_support
{

template <typename T, typename... Args> std::unique_ptr<T> makeUniqueCompat(Args &&...args)
{
#if defined(FASTGRIND_TESTCASE_HAS_CXX14)
    return std::make_unique<T>(std::forward<Args>(args)...);
#else
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
#endif
}

} // namespace fastgrind_testcase_support

#endif
