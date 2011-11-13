
find_package(Qt4)

if(QT4_FOUND)
	message(STATUS "Found Qt4, will build qtview example")
	include(${QT_USE_FILE})
	include_directories(${QT_INCLUDES})

	set (QTVIEW_HEADERS mousepoller.h mouseviewer.h)
	set (QTVIEW_SOURCES mousepoller.cpp mouseviewer.cpp main.cpp)

	QT4_WRAP_CPP(QTVIEW_MOC_OUTFILES ${QTVIEW_HEADERS} )

	add_executable(qtview ${QTVIEW_SOURCES} ${QTVIEW_MOC_OUTFILES})
	target_link_libraries(qtview touchmouse ${QT_LIBRARIES} ${PLATFORM_LIBS})

else()
	message(STATUS "Didn't find Qt, will not build qtview example")
endif()
