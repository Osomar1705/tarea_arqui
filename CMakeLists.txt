cmake_minimum_required(VERSION 3.11)
project(rv32-simulator CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(rv32sim src/main.cpp src/simulator.cpp src/disasm.cpp)
target_compile_options(rv32sim PRIVATE -Wall -Wextra)
