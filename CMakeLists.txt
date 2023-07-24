cmake_minimum_required(VERSION 3.5)

project(requirecpp VERSION 0.1.0 LANGUAGES CXX
    DESCRIPTION "A header-only c++ for easy and maintainable dependency injection"
    HOMEPAGE_URL "https://github.com/dabulla/requirecpp.git")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(REQUIRECPP_TESTS "Build the library tests" ON)
option(REQUIRECPP_EXAMPLES "Build the library examples" ON)

add_library(requirecpp INTERFACE)

target_include_directories(requirecpp
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_sources(requirecpp INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>/requirecpp/requirecpp.hpp
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>/requirecpp/details/requirecpp.ipp
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>/requirecpp/decorators.hpp
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>/requirecpp/details/type_lookup.hpp
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>/requirecpp/details/closure_traits.hpp
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>/requirecpp/details/pretty_type.hpp
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>/requirecpp/details/callback.hpp
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>/requirecpp/details/trackable_object.hpp)

if(REQUIRECPP_TESTS)
    add_subdirectory(tests)
endif()
if(REQUIRECPP_EXAMPLES)
    add_subdirectory(examples)
endif()


install(TARGETS requirecpp
        EXPORT ${PROJECT_NAME}_targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "requirecpp_config_version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}_config.cmake.in"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}_config.cmake"
    INSTALL_DESTINATION
    ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)
