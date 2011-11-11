
find_package(Qt4 REQUIRED QtCore QtGui)
include(${QT_USE_FILE})
include_directories(${QT_INCLUDES})

set (QTVIEW_HEADERS mousepoller.h mouseviewer.h)
set (QTVIEW_SOURCES mousepoller.cpp mouseviewer.cpp main.cpp)

QT4_WRAP_CPP(QTVIEW_MOC_OUTFILES ${QTVIEW_HEADERS} )

add_executable(qtview ${QTVIEW_SOURCES} ${QTVIEW_MOC_OUTFILES})
if(WIN32)
	target_link_libraries(qtview touchmouse ${QT_LIBRARIES} setupapi)
elseif(NOT APPLE)
	target_link_libraries(qtview touchmouse ${QT_LIBRARIES} usb-1.0 pthread rt)
else()
	target_link_libraries(qtview touchmouse ${QT_LIBRARIES})
endif()


