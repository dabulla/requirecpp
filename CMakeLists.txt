cmake_minimum_required(VERSION 3.5)

project(DependencyReactor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(DependencyReactor main.cpp DependencyReactor.hpp)

#target_include_directories(DependencyReactor .)
