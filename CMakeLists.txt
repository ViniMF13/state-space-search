cmake_minimum_required(VERSION 3.14)
project(TP1_IA CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(tp1_app
  src/main.cpp
  src/instance.cpp
  src/state.cpp
  src/search.cpp)

target_include_directories(tp1_app PRIVATE include)
