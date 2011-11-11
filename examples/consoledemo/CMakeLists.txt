
add_executable(consoledemo consoledemo.c)
if(WIN32)
	target_link_libraries(consoledemo touchmouse setupapi)
elseif(NOT APPLE)
	target_link_libraries(consoledemo touchmouse usb-1.0 pthread rt)
else()
	target_link_libraries(consoledemo touchmouse)
endif()

