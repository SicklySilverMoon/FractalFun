cmake_minimum_required(VERSION 3.21)
project(FractalFun C)

set(CMAKE_C_STANDARD 17)
FIND_PACKAGE(ImageMagick REQUIRED COMPONENTS MagickWand)

add_executable(FractalFun main.c)
target_link_libraries(FractalFun m atomic ${ImageMagick_LIBRARIES})
target_include_directories(FractalFun PUBLIC ${ImageMagick_INCLUDE_DIRS})
message(NOTICE, ${ImageMagick_MagickWand_INCLUDE_DIRS})