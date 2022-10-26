cmake_minimum_required(VERSION 3.21)
project(FractalFun CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(FractalFun main.cpp colours.h complex_t.h lodepng/lodepng.cpp lodepng/lodepng.h)
target_link_libraries(FractalFun m atomic ${ImageMagick_LIBRARIES})
target_include_directories(FractalFun PUBLIC ${ImageMagick_INCLUDE_DIRS})
message(NOTICE, ${ImageMagick_MagickWand_INCLUDE_DIRS})