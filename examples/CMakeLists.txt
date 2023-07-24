
add_executable(chicken-egg-aware chicken-egg-aware.cpp)
target_link_libraries(chicken-egg-aware PRIVATE requirecpp)

add_executable(chicken-egg-unaware chicken-egg-unaware.cpp)
target_link_libraries(chicken-egg-unaware PRIVATE requirecpp)

add_executable(car-assembly-states car-assembly-states.cpp)
target_link_libraries(car-assembly-states PRIVATE requirecpp)
