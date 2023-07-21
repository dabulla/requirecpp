cmake_minimum_required(VERSION 3.5)

project(requirecpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(requirecpp main.cpp requirecpp.hpp requirecpp2.hpp requirecpp3.hpp requirecpp_dirty.hpp decorators.hpp closure_traits.hpp function.hpp
                                    testcases.h
                                    testcase1.cpp
                                    testcase2.cpp
                                    testcase3.cpp
                                    testcase4.cpp)
