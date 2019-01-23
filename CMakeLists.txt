cmake_minimum_required(VERSION 2.8)
project(webstreamer)

set (CMAKE_CXX_STANDARD 11)
set (CMAKE_VERBOSE_MAKEFILE OFF)
set (EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/examples)
set (LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib)

#------ OPTIONS -------
option(BUILD_EXP "Build examples along with lib" OFF)
#----------------------

#------ PACKAGES ------
find_package(OpenCV 3 REQUIRED)
find_package(Threads  REQUIRED)
#----------------------

file(GLOB SE_SRCS ${CMAKE_SOURCE_DIR}/src/streameye/*.cpp
                  ${CMAKE_SOURCE_DIR}/src/webstreamer.cpp)

message("[DEBUG] [PSD] ${PROJECT_SOURCE_DIR}")
message("[DEBUG] [CSD] ${CMAKE_SOURCE_DIR}")
message("[DEBUG] [SES] ${SE_SRCS}")
add_library(webstreamer STATIC ${SE_SRCS})

target_include_directories(webstreamer PUBLIC ${PROJECT_SOURCE_DIR}/include
                                       PUBLIC ${PROJECT_SOURCE_DIR}/src/streameye
                                       PUBLIC ${OpenCV_INCLUDE_DIRS})

target_link_libraries(webstreamer ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT})

if(BUILD_EXP)
  add_subdirectory(${CMAKE_SOURCE_DIR}/examples)
endif(BUILD_EXP)
