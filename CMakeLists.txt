# libtouchmouse

cmake_minimum_required(VERSION 2.6)

project(libtouchmouse)

# Set up output folders
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

# Add include path to hidapi.h
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/hidapi/hidapi ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})

# Set library sources
if(WIN32)
	# Win32-specific options go here.
	message(STATUS "Detected Win32 system")
	list(APPEND LIBSRC hidapi/windows/hid.c)
elseif(APPLE)
	# Apple-specific options go here.
	message(STATUS "Detected Apple system")
	list(APPEND LIBSRC hidapi/mac/hid.c)
	set(CMAKE_SHARED_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
	set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
else()
	# Linux-specific options go here.
	message(STATUS "Detected non-windows, non-apple system; assuming some Linux variant")
	include_directories(/usr/include/libusb-1.0)
	list(APPEND LIBSRC hidapi/linux/hid-libusb.c)
endif()
list(APPEND LIBSRC src/touchmouse.c src/mono_timer.c)

set(CMAKE_C_FLAGS "-Wall -ggdb")

# Build library
add_library(touchmouse SHARED ${LIBSRC})

# Build demo
add_executable(consoledemo examples/consoledemo.c)
if(WIN32)
	target_link_libraries(touchmouse setupapi)
	target_link_libraries(consoledemo touchmouse setupapi)
elseif(NOT APPLE)
	target_link_libraries(consoledemo touchmouse usb-1.0 pthread rt)
else()
	target_link_libraries(consoledemo touchmouse)
endif()
