# Enable CTest for testing
enable_testing()

add_executable(test_basics basics.cpp)
target_link_libraries(test_basics PRIVATE requirecpp)
add_executable(test_qualifiers qualifiers.cpp)
target_link_libraries(test_qualifiers PRIVATE requirecpp)
add_executable(test_destruction destruction.cpp)
target_link_libraries(test_destruction PRIVATE requirecpp)
add_executable(test_states states.cpp)
target_link_libraries(test_states PRIVATE requirecpp)
add_executable(test_thread-blocking-require thread-blocking-require.cpp)
target_link_libraries(test_thread-blocking-require PRIVATE requirecpp)

add_test(NAME test_basics COMMAND $<TARGET_FILE:test_basics>)
add_test(NAME test_qualifiers COMMAND $<TARGET_FILE:test_qualifiers>)
add_test(NAME test_destruction COMMAND $<TARGET_FILE:test_destruction>)
add_test(NAME test_states COMMAND $<TARGET_FILE:test_states>)
add_test(NAME test_thread-blocking-require COMMAND $<TARGET_FILE:test_thread-blocking-require>)
