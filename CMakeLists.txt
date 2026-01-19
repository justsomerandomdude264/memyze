cmake_minimum_required(VERSION 3.16)

project(memyze VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

qt_add_executable(memyze
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    memoryanalyzer.cpp
    memoryanalyzer.h
    memorybar.cpp
    memorybar.h
    portmanager.cpp
    portmanager.h
    icon.qrc
)

target_link_libraries(memyze
    PRIVATE Qt6::Widgets Qt6::Concurrent
)

# ---------------------------
# Linux installation rules
# ---------------------------
include(GNUInstallDirs)

# Install binary
install(TARGETS memyze
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install desktop file
install(FILES packaging/memyze.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

# Install icon (must match Icon= in .desktop)
install(FILES assets/logo.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
    RENAME memyze.png
)
