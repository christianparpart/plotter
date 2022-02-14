cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(plotter VERSION "0.0.0" LANGUAGES CXX)

# setting defaults
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "-fdiagnostics-color=always")

add_executable(plotter main.cpp)
target_link_libraries(plotter sixel)
