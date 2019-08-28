add_library(logger OBJECT logger/logger.cc)

add_library(tll SHARED
	$<TARGET_OBJECTS:logger>
)

target_link_libraries(tll PRIVATE pthread)
target_link_libraries(tll PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/ld.script")
set_target_properties(tll PROPERTIES INTERFACE_LINK_LIBRARIES "")
