cmake_minimum_required(VERSION 3.16)

project(BAM_IDE VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Network Widgets WebSockets)
#find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Network Widgets WebSockets)

#######################################
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebSockets Network)
#find_package(QTermWidget REQUIRED)
find_package(qtermwidget6 REQUIRED)
#######################################

set(PROJECT_SOURCES
        main.cpp
        mainwindowcodeeditor.cpp
        mainwindowcodeeditor.h
        mainwindowcodeeditor.ui
        cursorwidget.cpp
        cursorwidget.h
        resources.qrc
        customtooltip.h
        linehighlightwidget.cpp
        linehighlightwidget.h
        cpphighlighter.cpp
        cpphighlighter.h
        todolistwidget.cpp
        todolistwidget.h
        linenumberarea.cpp
        linenumberarea.h
        terminalwidget.cpp
        terminalwidget.h
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(BAM_IDE
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        todolistwidget.cpp todolistwidget.h
      )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET BAM_IDE APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(BAM_IDE SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(BAM_IDE
            ${PROJECT_SOURCES}
            resources.qrc
            terminalwidget.h terminalwidget.cpp)
    endif()
endif()

target_link_libraries(BAM_IDE PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(BAM_IDE PRIVATE Qt::WebSockets)

#######################################
#target_link_libraries(BAM_IDE PRIVATE QTermWidget::QTermWidget)
#target_link_libraries(BAM_IDE PRIVATE qtermwidget6::QTermWidget)
#target_link_libraries(BAM_IDE PRIVATE qtermwidget6)
target_link_libraries(BAM_IDE PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::WebSockets)
target_link_libraries(BAM_IDE PRIVATE qtermwidget6)
#######################################

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(QT_VERSION VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.BAM_IDE)
endif()
set_target_properties(BAM_IDE PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS BAM_IDE
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(BAM_IDE)
endif()
