add_library(config OBJECT config/config.cc config/yaml.cc)
add_library(logger OBJECT logger/logger.cc)

add_library(tll SHARED
	$<TARGET_OBJECTS:config>
	$<TARGET_OBJECTS:logger>
	ring.c
	scheme.cc
	stat.cc
)

target_link_libraries(tll PRIVATE pthread ${YAML_LIBRARIES} ${ZLIB_LIBRARIES})
target_link_libraries(tll PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/ld.script")
set_target_properties(tll PROPERTIES INTERFACE_LINK_LIBRARIES "")
set_target_properties(tll PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ld.script)
