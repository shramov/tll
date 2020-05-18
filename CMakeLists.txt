cmake_minimum_required(VERSION 3.7)
project(TLL)

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
  cmake_policy(SET CMP0077 NEW) # For gtest
endif()

if (CMAKE_VERSION VERSION_LESS 3.16)
  include(GNUInstallDirs)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(WITH_GTEST "Build with googletest tests" ON)
option(WITH_GTEST_SOURCE "Build with googletest sources" ON)
option(WITH_LIBCPP "Build with LLVM C++ standard library (libc++)" OFF)

add_compile_options(-Wall -Werror -fPIC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
  # GCC 7.3 warns about unused var in auto [v, lock] = ... blocks
  add_compile_options(-Wno-unused-variable)
endif ()

if (${WITH_LIBCPP})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif ()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

include(FindPkgConfig)

pkg_check_modules(ZLIB zlib REQUIRED)
pkg_check_modules(YAML yaml-0.1 REQUIRED)

include_directories(${ZLIB_INCLUDE_DIRS} ${YAML_INCLUDE_DIRS})
link_directories(${ZLIB_LIBRARY_DIRS} ${YAML_LIBRARY_DIRS})

configure_file(tll.pc.in tll.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tll.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

link_libraries(fmt)

enable_testing()

add_subdirectory(src)
add_subdirectory(python)
add_subdirectory(test)
