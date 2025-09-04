# Multi package compile example producing shared libraries then linking to an executable

add_library(pkgA_shared STATIC pkgA/PackageA.cpp)
set_target_properties(pkgA_shared PROPERTIES OUTPUT_NAME pkgA)
target_include_directories(pkgA_shared PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/pkgA
)
target_link_libraries(pkgA_shared PUBLIC merecorder_includes merecorder_instrument)

add_library(pkgB_shared STATIC pkgB/PackageB.cpp)
set_target_properties(pkgB_shared PROPERTIES OUTPUT_NAME pkgB)
target_include_directories(pkgB_shared PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/pkgB
)
target_link_libraries(pkgB_shared PUBLIC merecorder_includes merecorder_instrument pkgA_shared)

add_library(pkgC_shared STATIC pkgC/PackageC.cpp)
set_target_properties(pkgC_shared PROPERTIES OUTPUT_NAME pkgC)
target_include_directories(pkgC_shared PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/pkgC
)
target_link_libraries(pkgC_shared PUBLIC merecorder_includes merecorder_instrument pkgB_shared)

add_executable(multi_pkg_main main.cpp)
target_link_libraries(multi_pkg_main PRIVATE pkgA_shared pkgB_shared pkgC_shared pthread dl merecorder_wrap -Wl,--export-dynamic)
target_link_libraries(multi_pkg_main PRIVATE merecorder_instrument merecorder_includes)
