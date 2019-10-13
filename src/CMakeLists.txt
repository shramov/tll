add_library(config OBJECT config/config.cc config/yaml.cc)
add_library(logger OBJECT logger/logger.cc)

add_subdirectory(channel)

add_library(tll SHARED
	$<TARGET_OBJECTS:config>
	$<TARGET_OBJECTS:logger>
	$<TARGET_OBJECTS:channel>
	ring.c
	scheme.cc
	stat.cc
)

target_link_libraries(tll PRIVATE dl ${YAML_LIBRARIES} ${ZLIB_LIBRARIES})
target_link_libraries(tll PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/ld.script")
set_target_properties(tll PROPERTIES INTERFACE_LINK_LIBRARIES "")
set_target_properties(tll PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ld.script)
set_target_properties(tll PROPERTIES VERSION 0 SOVERSION 0)

install(TARGETS tll
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	)
install(DIRECTORY tll DESTINATION include)
