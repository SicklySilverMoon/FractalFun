cmake_minimum_required(VERSION 3.22)
project(FractalFun CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(FractalFun main.cpp colours.h complex_t.h lodepng/lodepng.cpp lodepng/lodepng.h)