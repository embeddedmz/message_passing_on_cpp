project(MessagePassingModernCppExample)

cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 11)

find_package (Threads)

add_executable(MessagePassingModernCppExample MessagePassingModernCppExample.cpp observable/obs/connection.cpp)

target_include_directories(MessagePassingModernCppExample PRIVATE ./ observable/)
target_link_libraries (MessagePassingModernCppExample Threads::Threads)
