cmake_minimum_required(VERSION 3.26)
project(bitvector)

set(CMAKE_CXX_STANDARD 20)

add_executable(cs-tulip main.cpp
        bitvector.h
        bitvector.cpp)
