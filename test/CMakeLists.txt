if (WITH_GTEST)
add_subdirectory(/usr/src/googletest gtest)

add_executable(test-util test_conv.cc test_util.cc)
target_link_libraries(test-util gtest gtest_main ${ZLIB_LIBRARIES})

add_test(NAME test-util COMMAND $<TARGET_FILE:test-util>)

add_executable(test-lib test_stat.cc test_logger.cc)
target_link_libraries(test-lib gtest gtest_main tll)
add_test(NAME test-lib COMMAND $<TARGET_FILE:test-lib>)
endif()
