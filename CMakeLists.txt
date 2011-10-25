# libtouchmouse

cmake_minimum_required(VERSION 2.6)

project(libtouchmouse)

# Set up output folder
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

# Add include path to hidapi.h
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/hidapi/hidapi)

if(WIN32)
	# Win32-specific options go here.
	message(STATUS "Detected Win32 system")
	list(APPEND SRC hidapi/windows/hid.c)
elseif(APPLE)
	# Apple-specific options go here.
	message(STATUS "Detected Apple system")
	list(APPEND SRC hidapi/mac/hid.c)
	set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
else()
	# Linux-specific options go here.
	message(STATUS "Detected non-windows, non-apple system; assuming some Linux variant")
	include_directories(/usr/include/libusb-1.0)
	list(APPEND SRC hidapi/linux/hid-libusb.c)
endif()

# Build

set(CMAKE_C_FLAGS "-Wall")

add_executable(touchmouse touchmouse.c ${SRC})
if(WIN32)
	target_link_libraries(touchmouse setupapi)
elseif(NOT APPLE)
	target_link_libraries(touchmouse usb-1.0 pthread rt)
endif()
