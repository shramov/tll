if (WITH_GTEST)
enable_testing()
add_subdirectory(/usr/src/googletest gtest)

add_executable(test-util test_conv.cc test_util.cc)
target_link_libraries(test-util gtest gtest_main z)

add_test(test-util test-util)

add_executable(test-lib test_logger.cc)
target_link_libraries(test-lib gtest gtest_main tll)
add_test(test-lib test-lib)
endif()
