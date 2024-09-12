cmake_minimum_required(VERSION 3.17)
project(laba1IIUVM)

set(CMAKE_CXX_STANDARD 14)

add_executable(laba1IIUVM main.cpp)

target_link_libraries(laba1IIUVM ${CMAKE_SOURCE_DIR} setupapi powrprof)