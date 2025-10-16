cmake_minimum_required(VERSION 3.16)

project(pocsub LANGUAGES C)

# Allow user to override where open62541 was installed.
set(open62541_DIR "C:/open62541-install/lib/cmake/open62541" CACHE PATH "Path to open62541Config.cmake")
find_package(open62541 CONFIG REQUIRED)

add_executable(pocsub main.c)
target_link_libraries(pocsub PRIVATE open62541::open62541 ws2_32)

include(GNUInstallDirs)
install(TARGETS pocsub
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
