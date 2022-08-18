cmake_minimum_required(VERSION 3.0)

project(MEMORYMANAGER)

set (SRCS ./src/mm.cpp
          ./src/testapp.cpp
          ./src/gluethread/glthread.cpp)

include_directories(./src ./src/gluethread)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -g -Wall")

add_executable(main ${SRCS})