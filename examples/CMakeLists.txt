cmake_minimum_required(VERSION 2.8)
project(example)

add_executable(example example.cpp)
target_link_libraries(example webstreamer)

