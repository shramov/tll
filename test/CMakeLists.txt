if (WITH_GTEST)
if (WITH_GTEST_SOURCE)
  set(INSTALL_GTEST OFF CACHE BOOL "Disable INSTALL_GTEST") # XXX: Need policy CMP0077 NEW
  add_subdirectory(/usr/src/googletest gtest)
  link_libraries(gtest_main)
else()
  pkg_check_modules(GTEST gtest_main REQUIRED)

  add_compile_options(${GTEST_CFLAGS})
  link_libraries(${GTEST_LDFLAGS})
  if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	  add_compile_options(-Wno-unused-command-line-argument) # cflags contains -lpthread
  endif()
endif()

add_executable(test-util test_conv.cc test_util.cc test_ring.cc ../src/ring.c)
target_link_libraries(test-util ${ZLIB_LIBRARIES})

add_test(NAME test-util COMMAND $<TARGET_FILE:test-util>)

set_property(SOURCE test_scheme.cc APPEND PROPERTY COMPILE_DEFINITIONS SCHEME_PATH="${CMAKE_CURRENT_SOURCE_DIR}/test_scheme.yaml")

add_executable(test-lib test_config.cc test_logger.cc test_scheme.cc test_stat.cc)
target_link_libraries(test-lib tll)
add_test(NAME test-lib COMMAND $<TARGET_FILE:test-lib>)
endif()
