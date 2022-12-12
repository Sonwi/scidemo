cmake_minimum_required(VERSION 3.0.0)
project(scidemo VERSION 0.1.0)

# set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "/home/sonwi/.local/lib/cmake/SCI")
set(SCI_DIR "/home/sonwi/.local/lib/cmake/SCI")

find_package(SCI CONFIG REQUIRED)

include(CTest)
enable_testing()

add_executable(scidemo main.cpp)
target_link_libraries(scidemo
    SCI::SCI-utils
    SCI::SCI-LinearOT
    SCI::SCI-NonLinear
    SCI::SCI-FloatingPoint
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
