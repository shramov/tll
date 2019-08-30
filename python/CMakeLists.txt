if(${CMAKE_VERSION} VERSION_LESS "3.12.0")
	find_package(PythonInterp 3 REQUIRED)
else()
	find_package(Python3 COMPONENTS Interpreter)
	set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
endif()

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pytll.stamp
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   COMMAND ${PYTHON_EXECUTABLE} setup.py build_ext --inplace
                            --library-dirs=$<TARGET_LINKER_FILE_DIR:tll>
                            --rpath=$<TARGET_LINKER_FILE_DIR:tll>
                            --include-dirs=${CMAKE_CURRENT_SOURCE_DIR}/../src/
		   COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/pytll.stamp
                   DEPENDS tll tll/logger.pyx)
add_custom_target(pytll ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/pytll.stamp)
