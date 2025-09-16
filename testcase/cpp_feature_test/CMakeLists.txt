# Compile feature test programs.

file(GLOB FEATURE_TEST_SOURCES *.cpp)

add_executable(cpp_feature_test ${FEATURE_TEST_SOURCES})
target_link_libraries(cpp_feature_test PRIVATE pthread dl -lgcc_s)
target_link_libraries(cpp_feature_test PRIVATE fastgrind_includes fastgrind_wrap_flags fastgrind_instrument)