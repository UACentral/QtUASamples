cmake_minimum_required(VERSION 3.16)

project(qtcon-ua-read LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS OpcUa)

add_executable(qtcon-ua-read
  main.cpp
)
target_link_libraries(qtcon-ua-read Qt${QT_VERSION_MAJOR}::Core Qt6::OpcUa)

include(GNUInstallDirs)
install(TARGETS qtcon-ua-read
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
