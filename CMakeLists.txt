cmake_minimum_required(VERSION 3.21)
project(FractalFun C)

set(CMAKE_C_STANDARD 17)

add_executable(FractalFun main.c)
target_link_libraries(FractalFun m)